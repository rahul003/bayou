#include "../inc/server.h"
#include "../inc/constants.h"
#include "../inc/utils.h"
#include "../inc/data_classes.h"

#include "iostream"
#include "vector"
#include "string"
#include "fstream"
#include "sstream"
#include "assert.h"
#include "unistd.h"
#include "signal.h"
#include "errno.h"
#include "sys/socket.h"
#include "limits.h"

using namespace std;

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

pthread_mutex_t server_lock;
pthread_mutex_t misc_fd_lock;
pthread_mutex_t pause_lock;
pthread_mutex_t retiring_lock;

Server::Server(char** argv)
{
    pid_ = atoi(argv[2]);
    am_primary_ = (atoi(argv[3]) != 0);
    master_listen_port_ = atoi(argv[4]);
    my_listen_port_ = -1;

    if (am_primary_) {
        set_name(kFirst);
        vclock_[get_name()] = 0;
    } else {
        set_name("");
    }
    pause_ = false;
    max_csn_ = 0;
    retiring_ = UNSET;
    master_fd_ = -1;

}

int Server::get_pid() {
    return pid_;
}

bool Server::get_pause() {
    bool copy;
    pthread_mutex_lock(&pause_lock);
    copy = pause_;
    pthread_mutex_unlock(&pause_lock);
    return copy;
}

string Server::get_name() {
    return name_;
}

int Server::get_my_listen_port() {
    return my_listen_port_;
}

int Server::get_master_listen_port() {
    return master_listen_port_;
}

int Server::get_server_fd(const string& name) {
    int fd = -1;
    pthread_mutex_lock(&server_lock);
    if (server_fd_.find(name) != server_fd_.end())
        fd = server_fd_[name];
    pthread_mutex_unlock(&server_lock);
    return fd;
}

string Server::get_server_name(int port) {
    string name = "";
    pthread_mutex_lock(&server_lock);
    if (server_name_.find(port) != server_name_.end())
        name = server_name_[port];
    pthread_mutex_unlock(&server_lock);
    return name;
}

int Server::get_master_fd() {
    return master_fd_;
}

RetireStatus Server::get_retiring() {
    RetireStatus copy;
    pthread_mutex_lock(&retiring_lock);
    copy = retiring_;
    pthread_mutex_unlock(&retiring_lock);
    return copy;
}

int Server::get_server_fd_peerport_map(int fd) {
    int port;

    pthread_mutex_lock(&server_lock);
    if(server_fd_peerport_map_.find(fd)!=server_fd_peerport_map_.end())
        port = server_fd_peerport_map_[fd];
    else
        port = -1;
    pthread_mutex_unlock(&server_lock);

    return port;
}

void Server::set_retiring(RetireStatus s) {
    pthread_mutex_lock(&retiring_lock);
    retiring_ = s;
    pthread_mutex_unlock(&retiring_lock);
}

void Server::set_pause(bool t)
{
    pthread_mutex_lock(&pause_lock);
    pause_ = t;
    pthread_mutex_unlock(&pause_lock);
}
void Server::set_name(const string& my_name) {
    name_ = my_name;
}

void Server::set_misc_fd(int fd) {
    pthread_mutex_lock(&misc_fd_lock);
    misc_fd_.insert(fd);
    pthread_mutex_unlock(&misc_fd_lock);
}

void Server::set_master_fd(int fd) {
    master_fd_ = fd;
}

void Server::set_my_listen_port(int port) {
    my_listen_port_ = port;
}

void Server::set_client_fd(int port, int fd) {
    client_fd_[port] = fd;
}

// dont' call this explicitly. It is automatically called when set_server_fd is called
void Server::set_server_fd_peerport_map(int fd, int peer_port) {
    pthread_mutex_lock(&server_lock);
    server_fd_peerport_map_[fd] = peer_port;
    pthread_mutex_unlock(&server_lock);
}

void Server::set_server_fd(const string& name, int fd) {
    pthread_mutex_lock(&server_lock);
    server_fd_[name] = fd;
    pthread_mutex_unlock(&server_lock);

    // don't move inside lock since it already has lock
    set_server_fd_peerport_map(fd, GetPeerPortFromFd(fd));
}

void Server::set_server_name(int port, const string& name) {
    pthread_mutex_lock(&server_lock);
    server_name_[port] = name;
    pthread_mutex_unlock(&server_lock);
}

std::map<string, int> Server::GetServerFdCopy() {
    pthread_mutex_lock(&server_lock);
    std::map<string, int> server_fd_copy(server_fd_);
    pthread_mutex_unlock(&server_lock);
    return server_fd_copy;
}

std::unordered_set<int> Server::GetMiscFdCopy() {
    pthread_mutex_lock(&misc_fd_lock);
    std::unordered_set<int> misc_fd_copy(misc_fd_);
    pthread_mutex_unlock(&misc_fd_lock);
    return misc_fd_copy;
}

void Server::RemoveFromMiscFd(int fd) {
    pthread_mutex_lock(&misc_fd_lock);
    misc_fd_.erase(fd);
    pthread_mutex_unlock(&misc_fd_lock);
}

void Server::RemoveServer(int port) {
    if(port == -1)
        return;
    string name = get_server_name(port);
    if(name == "")
        return;
    int fd = get_server_fd(name);
    if(fd == -1)
        return;

    pthread_mutex_lock(&server_lock);
    // server_name_.erase(port);
    server_fd_.erase(name);
    server_fd_peerport_map_.erase(fd);
    pthread_mutex_unlock(&server_lock);
}

void Server::CloseClientConnections()
{
    for (auto &c : client_fd_)
    {
        close(c.second);
    }
    client_fd_.clear();
}

void Server::CloseServerAndMiscConnections()
{
    for (auto &s : server_fd_)
    {
        close(s.second);
    }
    // no need to clear server_fd_ as server is going to be killed soon
    for (auto &m : misc_fd_)
    {
        close(m);
    }
    // no need to clear misc_fd_ as server is going to be killed soon
}

void Server::AddRetireWrite()
{
    vclock_[get_name()]++;
    IdTuple w;
    if (am_primary_)
    {
        max_csn_++;
        w = IdTuple(max_csn_, get_name(), vclock_[get_name()]);
    }
    else
        w = IdTuple(INT_MAX, get_name(), vclock_[get_name()]);
    Command c(kRetire);
    write_log_[w] = c;
    // ExecuteCommandsOnDatabase(w);
    //dont execute as will change vector clock which casues issues with antientropy
}

void* AntiEntropyP1(void* _S) {
    Server* S = (Server*)_S;
    string msg = kAntiEntropyP1 + kInternalDelim + kMessageDelim;
    string cur_server = "";
    while (true)
    {
        if(S->get_retiring()) {
            return NULL;
        }
        usleep(kAntiEntropyInterval);
        if (!S->get_pause())
        {
            cur_server = S->GetNextServer(cur_server);
            if (cur_server != "")
                S->SendMessageToServer(cur_server, msg);
        }
    }
    return NULL;
}

string Server::GetWriteLogAsString() {
    string rval;
    for (auto &entry : write_log_)
    {
        IdTuple w = entry.first;
        Command c = entry.second;

        if (c.get_type() == kPut)
        {
            rval += "PUT:(";
            rval += c.get_song() + "," + c.get_url() + "):";
            if (w.get_csn() == INT_MAX)
                rval += "FALSE";
            else
                rval += "TRUE";
            rval += kInternalListDelim;
        }
        else if (c.get_type() == kDelete)
        {
            rval += "DELETE:(";
            rval += c.get_song() + "):";
            if (w.get_csn() == INT_MAX)
                rval += "FALSE";
            else
                rval += "TRUE";
            rval += kInternalListDelim;
        }
    }
    return rval;
}

string Server::GetServerForRetireMessage() {
    string rval = "";

    pthread_mutex_lock(&server_lock);
    auto it = server_fd_.begin();
    if (it != server_fd_.end())
        rval = it->first;
    pthread_mutex_unlock(&server_lock);

    return rval;
}

string Server::GetNextServer(string last) {
    string rval = "";
    pthread_mutex_lock(&server_lock);
    if (!server_fd_.empty()) {
        auto it = server_fd_.upper_bound(last);
        if (it == server_fd_.end())
            it = server_fd_.begin();
        rval = it->first;
    }
    pthread_mutex_unlock(&server_lock);
    return rval;
}

void Server::CreateFdSet(fd_set& fromset,
                         vector<int>& fds,
                         int& fd_max) {

    fd_max = INT_MIN;
    char buf[kMaxDataSize];
    int fd_temp;
    FD_ZERO(&fromset);
    fds.clear();

    // client fds
    for (auto it = client_fd_.begin(); it != client_fd_.end(); ) {
        fd_temp = it->second;
        if (fd_temp == -1) {
            continue;
        }
        errno = 0;
        int rv = recv(fd_temp, &buf, kMaxDataSize-1, MSG_DONTWAIT | MSG_PEEK);
        if (rv == 0 || errno == EBADF || errno == ENOTCONN) {
            close(fd_temp);
            D(cout << "S" << get_pid()
              << " : ERROR Unexpected peek error in client fd for client "
              << it->first << endl;)
            it = client_fd_.erase(it);
            //ideally this should never happen
        } else {
            FD_SET(fd_temp, &fromset);
            fd_max = max(fd_max, fd_temp);
            fds.push_back(fd_temp);
            it++;
        }
    }

    // server fds
    map<string, int> server_fd_copy = GetServerFdCopy();
    for (auto it = server_fd_copy.begin(); it != server_fd_copy.end(); ) {
        fd_temp = it->second;
        if (fd_temp == -1) {
            continue;
        }
        errno = 0;
        int rv = recv(fd_temp, &buf, kMaxDataSize-1, MSG_DONTWAIT | MSG_PEEK);
        if (rv == 0 || errno == EBADF || errno == ENOTCONN) {
            D(cout << "S" << get_pid()
              << " : peek error in server fd for server "
              << it->first << endl;)
            RemoveServer(get_server_fd_peerport_map(fd_temp));    // takes port as arg
            close(fd_temp);
            it++;
        } else {
            FD_SET(fd_temp, &fromset);
            fd_max = max(fd_max, fd_temp);
            fds.push_back(fd_temp);
            it++;
        }
    }

    // misc fds
    unordered_set<int> misc_fd_copy = GetMiscFdCopy();
    for (auto it = misc_fd_copy.begin(); it != misc_fd_copy.end(); ) {
        fd_temp = *(it);
        if (fd_temp == -1) {
            continue;
        }
        errno = 0;
        int rv = recv(fd_temp, &buf, kMaxDataSize-1, MSG_DONTWAIT | MSG_PEEK);
        if (rv == 0 || errno == EBADF || errno == ENOTCONN) {
            D(cout << "S" << get_pid()
              << " : ERROR Unexpected peek error in misc fd="
              << *it << endl;)
            close(fd_temp);
            RemoveFromMiscFd(fd_temp);
            it++;
            // ideally, this case should never happen
        } else {
            FD_SET(fd_temp, &fromset);
            fd_max = max(fd_max, fd_temp);
            fds.push_back(fd_temp);
            it++;
        }
    }

    //master_fd
    fd_temp = get_master_fd();
    if (fd_temp != -1) {
        errno = 0;
        int rv = recv(fd_temp, &buf, kMaxDataSize-1, MSG_DONTWAIT | MSG_PEEK);
        if (rv == 0 || errno == EBADF || errno == ENOTCONN) {
            D(cout << "S" << get_pid()
              << " : ERROR Unexpected peek error in master fd=" << endl;)
            close(fd_temp);
            set_master_fd(-1);
            // ideally, this case should never happen
        } else {
            FD_SET(fd_temp, &fromset);
            fd_max = max(fd_max, fd_temp);
            fds.push_back(fd_temp);
        }
    }
}

void Server::ReceiveFromAllMode()
{
    char buf[kMaxDataSize];
    int num_bytes;

    fd_set fromset;
    std::vector<int> fds;
    int fd_max;

    while (true) {
        CreateFdSet(fromset, fds, fd_max);

        if (fd_max == INT_MIN) {
            D(cout << "S" << get_pid() << ": ERROR Unexpected fd_set empty" << endl;)
            usleep(kBusyWaitSleep);
            continue;
        }


        struct timeval timeout = kSelectTimeoutTimeval;
        int rv = select(fd_max + 1, &fromset, NULL, NULL, &timeout);
        if (rv == -1) { //error in select
            D(cout << "S" << get_pid()
              << ": ERROR in select() errno=" << errno
              << " fdmax=" << fd_max << endl;)
        } else 
        if (rv == 0) {
            // D(cout << "S" << get_pid() << ": Select timeout" << endl;)
        } else {
            for (int i = 0; i < fds.size(); i++) {
                if (FD_ISSET(fds[i], &fromset)) { // we got one!!
                    if ((num_bytes = recv(fds[i], buf, kMaxDataSize - 1, 0)) == -1) {
                        D(cout << "S" << get_pid() << ": ERROR in receiving" << endl;)
                        // can only be a server fd
                        RemoveServer(get_server_fd_peerport_map(fds[i]));
                        close(fds[i]);
                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "S" << get_pid() << ": Connection closed" << endl;)
                        // can only be a server fd
                        RemoveServer(get_server_fd_peerport_map(fds[i]));
                        close(fds[i]);
                    } else {

                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message) {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);
                            if (token[0] == kIAm) {
                                //IAM-CLIENT
                                //IAM-SERVER-SERVER_NAME
                                //IAM-NEWSERVER
                                D(assert(token.size() >= 2);)

                                int port = GetPeerPortFromFd(fds[i]);
                                if (token[1] == kClient) {
                                    set_client_fd(port, fds[i]);
                                    RemoveFromMiscFd(fds[i]);
                                    SendDoneToClient(fds[i]);
                                } else if (token[1] == kServerP1 || token[1] == kNewServer) {
                                    HandleInitialServerHandshake(port, fds[i], token);
                                    RemoveFromMiscFd(fds[i]);
                                } else if(token[1] == kServerP2) {
                                    D(assert(token.size() == 3);)
                                    set_server_name(port, token[2]);
                                    set_server_fd(token[2], fds[i]);
                                    
                                    // I may already have entry for this server in my VC
                                    // see comment in ExecuteCommandOnDatabase function at the end
                                    // near VC update line
                                    if (vclock_.find(token[2]) == vclock_.end())
                                        vclock_[token[2]] = 0;
                                }
                            } else if (token[0] == kYouAre) {
                                //YOUARE-MY_NAME-IAM-SERVER-PEERNAME
                                D(assert(token.size() == 5);)

                                int port = GetPeerPortFromFd(fds[i]);
                                set_name(token[1]);
                                std::vector<string> v = split(token[1], kName[0]);
                                vclock_[get_name()] = stoi(v.back());
                                //first write this server accepts will be with tk+1
                                set_server_name(port, token[4]);
                                set_server_fd(token[4], fds[i]);
                                vclock_[token[4]] = 0;
                                RemoveFromMiscFd(fds[i]);
                            }
                            else if (token[0] == kAntiEntropyP1)
                            {
                                SendAEP1Response(fds[i]);
                            }
                            else if (token[0] == kAntiEntropyP1Resp)
                            {
                                SendAntiEntropyP2(token, fds[i]);
                            }
                            else if (token[0] == kRetiring)
                            {
                                //kretiring-kwasprim-$
                                if (token[1] == kWasPrim)
                                {
                                    am_primary_ = true;
                                    CommitTentativeWrites();
                                }
                                // vclock_.erase(get_server_name(fds[i]));
                                string msg = kAck + kInternalDelim + kMessageDelim;
                                SendMessageToServerByFd(fds[i], msg);
                            }
                            else if (token[0] == kAntiEntropyP2)
                            {
                                D(assert(token.size() == 3);)
                                if (!(token[1].empty() && token[2].empty()))
                                    ExtractAEP2Message(token[1], token[2]);
                            }
                            else if (token[0] == kServerVC)
                            {
                                //SERVERVC-CLIENT
                                //SERVERVC-MASTER
                                D(assert(token.size() == 2);)
                                unordered_map<string, int> port_to_clock;
                                CreatePortToClockMap(port_to_clock);
                                string msg = kServerVC + kInternalDelim;
                                msg += UnorderedMapToString(port_to_clock);
                                if(token[1] == kClient) {
                                    msg += kMessageDelim;
                                    SendMessageToClient(fds[i], msg);
                                }
                                else if(token[1] == kMaster) {
                                    msg += to_string(max_csn_) + kInternalDelim + kMessageDelim;
                                    SendMessageToMaster(msg);
                                }
                            }
                            else if (token[0] == kGet)
                            {
                                string msg = kUrl + kInternalDelim;
                                string url;
                                if (database_.find(token[1]) == database_.end())
                                    url = kErrKey;
                                else
                                    url = database_[token[1]];
                                msg += url + kInternalDelim + kMessageDelim;
                                SendMessageToClient(fds[i], msg);

                                msg = kRelWrites + kInternalDelim;
                                msg += GetRelevantWrites(token[1]) + kMessageDelim;
                                SendMessageToClient(fds[i], msg);
                            }
                            else if (token[0] == kPut)
                            {
                                D(assert(token.size() == 3);)
                                AddWrite(token[0], token[1], token[2]);
                                SendWriteIdToClient(fds[i]);
                            }
                            else if (token[0] == kDelete)
                            {
                                D(assert(token.size() == 2);)
                                AddWrite(token[0], token[1], "");
                                SendWriteIdToClient(fds[i]);
                            }
                            else if (token[0] == kRetireServer) {
                                set_retiring(SET);
                                CloseClientConnections();
                                AddRetireWrite();
                                string send_to = GetServerForRetireMessage();
                                if (send_to != "") {
                                    string msg = kAntiEntropyP1 + kInternalDelim + kMessageDelim;
                                    SendMessageToServer(send_to, msg);
                                }
                            }
                            else if (token[0] == kPrintLog) {
                                string msg = kMyLog + kInternalDelim;
                                msg += GetWriteLogAsString() + kInternalDelim + kMessageDelim;

                                SendMessageToMaster(msg);
                            }
                            else if (token[0] == kPause)
                            {
                                D(cout << "S" << get_pid()
                                  << " : PAUSE Anti-Entropy" << endl;)
                                set_pause(true);
                                SendDoneToMaster();
                            }
                            else if (token[0] == kStart)
                            {
                                D(cout << "S" << get_pid()
                                  << " : START Anti-Entropy" << endl;)
                                set_pause(false);
                                SendDoneToMaster();
                            }
                            else {    //other messages
                                D(cout << "S" << get_pid()
                                  << " : ERROR Unexpected message received: " << msg << endl;)
                            }
                        }
                    }
                }
            }
        }
    }
}

void Server::SendAntiEntropyP2(vector<string>& token, int fd)
{
    bool was_set = false;
    if (get_retiring() == SET)
        was_set = true;
    D(assert(token.size() == 3);)
    int recvd_csn = stoi(token[1]);
    unordered_map<string, int> recvd_clock;
    StringToClock(token[2], recvd_clock);
    string msg;
    ConstructAEP2Message(msg, recvd_csn, recvd_clock);
    SendMessageToServerByFd(fd, msg);

    if (was_set)
    {
        string msg = kRetiring + kInternalDelim;
        if (am_primary_)
            msg += kWasPrim;
        msg += kInternalDelim + kMessageDelim;
        SendMessageToServerByFd(fd, msg);
        WaitForAck(fd);
        set_retiring(DONE);
        CloseServerAndMiscConnections();
        SendDoneToMaster();

        // dont' go back to receive thread. Die in sleep
        while(true) {
            sleep(kGeneralSleep);
        }
    }
}
string Server::GetRelevantWrites(string song)
{
    unordered_map<string, int> name_to_ts;
    for (auto &w : write_log_)
    {
        if (w.second.get_song() == song)
        {
            name_to_ts[w.first.get_sname()] = w.first.get_accept_ts();
        }
    }

    return UnorderedMapToString(name_to_ts);
}

void Server::CreatePortToClockMap(unordered_map<string, int>& port_to_clock)
{
    for (auto &c : vclock_)
    {
        port_to_clock[c.first] = c.second;
    }
}
void Server::SendWriteIdToClient(int fd)
{
    string msg = kWriteID + kInternalDelim;
    msg += get_name() + kComma;
    msg += to_string(vclock_[get_name()]) + kInternalDelim + kMessageDelim;
    SendMessageToClient(fd, msg);
}

void Server::AddWrite(string type, string song, string url)
{
    //new writes will have clock from 1
    vclock_[get_name()]++;
    IdTuple w;
    if (am_primary_)
    {
        max_csn_++;
        w = IdTuple(max_csn_, get_name(), vclock_[get_name()]);
    }
    else
        w = IdTuple(INT_MAX, get_name(), vclock_[get_name()]);

    Command c(type, song, url);
    write_log_[w] = c;
    ExecuteCommandsOnDatabase(w);
}

void Server::CommitTentativeWrites() {
    auto it = write_log_.lower_bound(IdTuple(max_csn_ + 1, "", 0));
    IdTuple first;
    bool first_set = false;
    std::map<IdTuple, Command> temp;
    while (it != write_log_.end())
    {
        Command c = it->second;
        IdTuple w = it->first;
        it = write_log_.erase(it);
        max_csn_++;
        w = IdTuple(max_csn_, w.get_sname(), w.get_accept_ts());
        if (!first_set)
        {
            first = w;
            first_set = true;
        }
        temp[w] = c;
    }

    for(auto &e: temp) {
        write_log_[e.first] = e.second;
    }

    ExecuteCommandsOnDatabase(first);
}

void* ReceiveFromAll(void* _S) {
    Server* S = (Server*)_S;
    S->ReceiveFromAllMode();
    return NULL;
}

void Server::HandleInitialServerHandshake(int port, int fd, const std::vector<string>& token) {
    // token[0] is kIAm
    if (token[1] == kServerP1) {  // sending server already has name
        //IAM-SERVER-SERVER_NAME
        D(assert(token.size() == 3);)
        set_server_name(port, token[2]);
        set_server_fd(token[2], fd);

        // I may already have entry for this server in my VC
        // see comment in ExecuteCommandOnDatabase function at the end
        // near VC update line
        if(vclock_.find(token[2]) == vclock_.end())
            vclock_[token[2]] = 0;

        string message;
        ConstructIAmMessage(kIAm, kServerP2, get_name() + kInternalDelim, message);
        SendMessageToServer(token[2], message);

    } else if (token[1] == kNewServer) {    // sending server is new. Does not have a name
        vclock_[get_name()]++;
        string name = CreateName();
        set_server_name(port, name);
        set_server_fd(name, fd);

        //vc for the server I created
        vclock_[name] = vclock_[get_name()];

        int temp_csn;
        if (am_primary_) {
            max_csn_++;
            temp_csn = max_csn_;
        } else {
            temp_csn = INT_MAX;
        }

        IdTuple w(temp_csn, get_name(), vclock_[get_name()]);
        write_log_[w] = Command(kCreate);
        ExecuteCommandsOnDatabase(w);

        //can also set to vclock[name] = vclock[name_]

        // send his and my name to peer
        string you_are_msg;
        string i_am_msg;
        ConstructMessage(kYouAre, name + kInternalDelim, you_are_msg);
        ConstructIAmMessage(kIAm, kServerP2, get_name() + kInternalDelim, i_am_msg);

        string message = you_are_msg;
        message.pop_back();
        message = message + i_am_msg;
        SendMessageToServer(name, message);
    }
}

string Server::CreateName() {
    return (get_name() + kName + to_string(vclock_[get_name()]));
}

void Server::SendOrAskName(int fd) {
    if (!get_name().empty())
    {
        string msg;
        ConstructIAmMessage(kIAm, kServerP1, get_name() + kInternalDelim, msg);
        SendMessageToServerByFd(fd, msg);
    }
    else {
        string msg;
        ConstructIAmMessage(kIAm, kNewServer, "", msg);
        SendMessageToServerByFd(fd, msg);
    }
}

void Server::SendAEP1Response(int fd)
{
    string msg;
    msg += kAntiEntropyP1Resp + kInternalDelim;
    msg += to_string(max_csn_) + kInternalDelim;
    msg += ClockToString(vclock_) + kInternalDelim + kMessageDelim;
    SendMessageToServerByFd(fd, msg);
}

void Server::ConstructAEP2Message(string& msg, const int& r_csn, unordered_map<string, int>& r_clock)
{
    //AEP2-c1.n1.w1.m1a.m1b.m1c-n1.w1,m1
    //AEP2-commitedwrites-tentativewrites$
    //only this thread will use max_csn no need of lock
    msg += kAntiEntropyP2 + kInternalDelim;
    string committed, new_tent;
    if (r_csn < max_csn_)
    {
        //gives first element higher than this
        auto it = write_log_.lower_bound(IdTuple(r_csn + 1, "", 0));
        while (it != write_log_.end() && it->first.get_csn() <= max_csn_)
        {
            committed += WriteToString(it->first, it->second) + kComma;
            it++;
        }
    }
    msg += committed + kInternalDelim;
    auto it = write_log_.lower_bound(IdTuple(max_csn_ + 1, "", 0));
    while (it != write_log_.end())
    {
        if(r_clock.find(it->first.get_sname()) == r_clock.end() || 
            it->first.get_accept_ts() > r_clock[it->first.get_sname()])
        {
            new_tent += WriteToString(it->first, it->second) + kComma;
        }
        it++;
    }
    msg += new_tent + kInternalDelim + kMessageDelim;
}

IdTuple Server::RollBack(const string& committed_writes, const string& tent_writes)
{
    IdTuple earliest;
    bool earliest_found = false;
    vector<string> writes, parts;
    if (!committed_writes.empty()) {
        writes = split(committed_writes, kComma[0]);
        parts = split(writes[0], kInternalWriteDelim[0]);
        D(assert(parts.size() == 6);) // 5 for delete, 6 for put
        IdTuple cur_first(stoi(parts[0]), parts[1], stoi(parts[2]));
        earliest = cur_first;
        earliest_found = true;
    }
    if (!tent_writes.empty() && !earliest_found)
    {
        writes = split(tent_writes, kComma[0]);
        parts = split(writes[0], kInternalWriteDelim[0]);
        D(assert(parts.size() == 6);) // 5 for delete, 6 for put
        IdTuple cur_first(INT_MAX, parts[1], stoi(parts[2]));

        earliest = cur_first;
        earliest_found = true;
    }
    D(assert(earliest_found == true);)

    auto it = write_log_.rbegin();
    while (it != write_log_.rend() && it->first > earliest)
    {
        string c_type = it->second.get_type();
        if (c_type == kCreate || c_type == kRetire)
        {
            //creation or retire write. has no undo
        }
        else
        {
            Command undo_c = undo_log_[it->first];
            if (undo_c.get_type() == kDelete)
            {
                database_.erase(undo_c.get_song());
            }
            else if (undo_c.get_type() == kUndo)
            {
                database_[undo_c.get_song()] = undo_c.get_url();
            }
            else if (undo_c.get_type() == kNoOp)
            {
                
            }
            undo_log_.erase(it->first);
        }
        it++;
    }
    return earliest;
}

void Server::ExtractAEP2Message(const string& committed_writes, const string& tent_writes) {
    //AEP2-c1.n1.w1.m1-c1,n1.w1,m1
    //AEP2-commitedwrites-tentativewrites$
    IdTuple from = RollBack(committed_writes, tent_writes);

    vector<string> writes = split(committed_writes, kComma[0]);
    for (auto &p : writes)
    {
        vector<string> parts = split(p, kInternalWriteDelim[0]);
        D(assert(parts.size() == 6);) // 5 for delete, 6 for put
        IdTuple w(INT_MAX, parts[1], stoi(parts[2]));

        // check if this write already present in committed
        IdTuple w_new(stoi(parts[0]), parts[1], stoi(parts[2]));
        if (write_log_.find(w_new) != write_log_.end())
            continue;

        if (write_log_.find(w) != write_log_.end()) {
            write_log_.erase(w);
        }
        Command c(parts[3], parts[4], parts[5]);
        write_log_[w_new] = c;
    }
    writes = split(tent_writes, kComma[0]);

    if(am_primary_ && writes.size())
        from.set_csn(max_csn_+1);
    for (auto &p : writes)
    {
        vector<string> parts = split(p, kInternalWriteDelim[0]);
        D(assert(parts.size() == 6);) // 5 for delete, 6 for put
        int temp_csn;

        // check if this write already present in committed or tentative
        if(vclock_.find(parts[1]) != vclock_.end()) {
            if (vclock_[parts[1]] >=  stoi(parts[2]) ) {
                continue;
            }
        }

        if (am_primary_) {
            max_csn_++;
            temp_csn = max_csn_;
        } else {
            temp_csn = INT_MAX;
        }
        IdTuple w(temp_csn, parts[1], stoi(parts[2]));
        Command c(parts[3], parts[4], parts[5]);
        write_log_[w] = c;
    }

    ExecuteCommandsOnDatabase(from);

}

void Server::ExecuteCommandsOnDatabase(IdTuple from)
{
    auto it = write_log_.find(from);


    while (it != write_log_.end())
    {
        IdTuple w = it->first;
        string song = it->second.get_song();
        string url = it->second.get_url();
        string type = it->second.get_type();
        if (type == kPut)
        {
            if (w.get_csn() == INT_MAX)
            {
                //undo command for a write needed only if tentative
                if (database_.find(song) == database_.end())
                {
                    //wasnt in database
                    undo_log_[w] = Command(kDelete, song, "");
                }
                else
                {
                    //overwritten
                    undo_log_[w] = Command(kUndo, song, database_[song]);
                }
            }
            else
                max_csn_ = w.get_csn();
            database_[song] = url;
        }
        else if (type == kDelete)
        {
            if (w.get_csn() == INT_MAX)
            {
                if(database_.find(song) != database_.end())
                    undo_log_[w] = Command(kUndo, song, database_[song]);
                else 
                {
                    undo_log_[w] = Command(kNoOp);
                }
            }
            else
                max_csn_ = w.get_csn();
            database_.erase(song);
        }
        else if (type == kCreate)
        {
            string name = w.get_sname() + kName + to_string(w.get_accept_ts());
            // vclock_[name] = 0;
            
            
            // incorrect assert because this might be a commit notification in which case
            // your vc for w.get_sname() can be greater than the commit notification's ts
            // because you have some tentative writes with the same w.get_sname()
            
            // if(vclock_.find(name) != vclock_.end())
                // D(assert(vclock_[name] <= w.get_accept_ts());)

            // updating vc of the server who has been created using this create write
            vclock_[name] = w.get_accept_ts();

            if (w.get_csn() != INT_MAX)
                max_csn_ = w.get_csn();
        }
        else if (type == kRetire)
        {
            // vclock_.erase(w.get_sname());
            if (w.get_csn() != INT_MAX)
                max_csn_ = w.get_csn();
        }

        // cout << "S" << get_pid() << "-------------" 
        // <<  w.get_sname() <<"," << vclock_[w.get_sname()]
        // << "," << w.get_accept_ts() << "," << type << "," 
        // << song << "," << url << endl;

        // incorrect assert because this might be a commit notification in which case
        // your vc for w.get_sname() can be greater than the commit notification's ts
        // because you have some tentative writes with the same w.get_sname()

        // D(assert(vclock_[w.get_sname()] <= w.get_accept_ts());)

        // required for writes sent by others
        if (vclock_.find(w.get_sname()) == vclock_.end() || vclock_[w.get_sname()] < w.get_accept_ts())
            vclock_[w.get_sname()] = w.get_accept_ts();

        // IMPORTANT NOTE:
        // Suppose I am S2. Above update can create a new entry in vc for 
        // a server S0 whom I haven't talked to yet, but I received a create write from S1 with
        // get_sname() = S0.
        // 
        // Make sure that later when I do I-AM-SERVER(p1)(p2) handshake with S0, I DO NOT reset
        // S0's entry in my VC to 0

        //updating vector clock if behind. it will be behind for new writes
        // int s_clock = CompleteV(w.get_sname());
        // if (s_clock == INT_MIN)
        // {   //not seen creation write
        //     //below is effectively seeing creation write.
        //     //actually i think it will first see the creation write definitely
        //     vclock_[w.get_sname()] = w.get_accept_ts();
        // }
        // else if (s_clock < INT_MAX)
        // {
        //     if (vclock_[w.get_sname()] < w.get_accept_ts())
        //         vclock_[w.get_sname()] = w.get_accept_ts();
        // }
        // else
        // {
        //     //retired. and i know it.
        // }

        it++;
    }

}

// int Server::CompleteV(string s)
// {
//     if (vclock_.find(s) != vclock_.end())
//     {
//         return vclock_[s];
//     }
//     if (s == kFirst)
//     {
//         return INT_MAX;
//     }
//     vector<string> sub_name = split(s, kName[0]);
//     D(assert(sub_name.size() >= 2);)
//     string last = sub_name[sub_name.size() - 1];
//     string second_last = sub_name[sub_name.size() - 2];
//     if (CompleteV(second_last) >= stoi(last))
//         return INT_MAX;
//     else
//         return INT_MIN;
// }

void Server::WaitForAck(int fd)
{
    char buf[kMaxDataSize];
    int num_bytes;

    bool ack = false;
    while (!ack) {
        if ((num_bytes = recv(fd, buf, kMaxDataSize - 1, 0)) == -1) {
            D(cout << "S" << get_pid()
                << " : ERROR in receiving ACK, fd=" << fd << endl;)
        }
        else if (num_bytes == 0) {   //connection closed
            D(cout << "S" << get_pid()
              << " : ERROR Connection closed by server while waiting for ACK, fd=" << fd << endl;)
        }
        else {
            buf[num_bytes] = '\0';
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kAck) {
                    ack = true;
                    D(cout << "S" << get_pid() << " : ACK received" << endl;)
                } else {
                    // D(cout << "S" << get_pid() << " : ERROR Unexpected message received at fd="
                    //   << fd << ": " << msg << endl;)
                }
            }
        }
    }
}

void Server::SendMessageToServerByFd(int fd, const string & message) {
    if (send(fd, message.c_str(), message.size(), 0) == -1) {
        D(cout << "S" << get_pid() << " : ERROR: Cannot send message to S" << endl;)
        RemoveServer(get_server_fd_peerport_map(fd));
        close(fd);
    } else {
        D(cout << "S" << get_pid() << " : Message sent to S: " << message << endl;)
    }
}

void Server::SendMessageToServer(const string& name, const string & message) {
    int fd = get_server_fd(name);
    if (send(fd, message.c_str(), message.size(), 0) == -1) {
        D(cout << "S" << get_pid() << " : ERROR: Cannot send message to server " << name << endl;)
        RemoveServer(get_server_fd_peerport_map(fd));
        close(fd);
    } else {
        D(cout << "S" << get_pid() << " : Message sent to server " << name << ": " << message << endl;)
    }
}

void Server::SendMessageToMaster(const string & message) {
    if (send(get_master_fd(), message.c_str(), message.size(), 0) == -1) {
        D(cout << "S" << get_pid() << " : ERROR: Cannot send message to M" << endl;)
    } else {
        D(cout << "S" << get_pid() << " : Message sent to M" << ": " << message << endl;)
    }
}

void Server::SendMessageToClient(int fd, const string& message) {
    if (send(fd, message.c_str(), message.size(), 0) == -1) {
        D(cout << "S" << get_pid() << " : ERROR: Cannot message to C" << endl;)
    } else {
        D(cout << "S" << get_pid() << " : Message sent to C" << ": " << message << endl;)
    }
}

void Server::SendDoneToClient(int fd) {
    string message;
    ConstructMessage(kDone, "", message);
    SendMessageToClient(fd, message);
}

void Server::SendDoneToMaster() {
    string message;
    ConstructMessage(kDone, "", message);
    SendMessageToMaster(message);
}

void Server::ConstructPortMessage(string & message) {
    message = kPort + kInternalDelim +
              kServer + kInternalDelim +
              to_string(get_pid()) + kInternalDelim +
              to_string(get_my_listen_port()) + kInternalDelim + kMessageDelim;
}

void Server::ConstructMessage(const string & type, const string & body, string & message) {
    message = type + kInternalDelim + body + kMessageDelim;
}

void Server::ConstructIAmMessage(const string & type, const string & process_type, const string & name, string & message) {
    message = type + kInternalDelim + process_type + kInternalDelim + name + kMessageDelim;
}

/**
 * connects to all servers whose port numbers were given by the master
 * @param argv command line arguments passed by master
 */
void Server::ConnectToAllServers(char** argv) {
    int num_args = atoi(argv[1]);
    int i = 5;
    while (i < num_args)
    {
        if (ConnectToServer(atoi(argv[i]))) {
            // D(cout << "S" << get_pid()
            //   << " : Connected to server "
            //   << get_server_name(atoi(argv[i])) << endl;)
        }
        else {
            D(cout << "S" << get_pid()
              << " : ERROR in connecting to server "
              << get_server_name(atoi(argv[i])) << endl;)
        }

        while (get_server_name(atoi(argv[i])) == "") {
            usleep(kBusyWaitSleep);
        }

        i++;
    }

    // sleeps till it gets a name from some other server.
    while(get_name() == "") {
        usleep(kBusyWaitSleep);
    }
}

/**
 * connects to master and sends self's listen port number
 */
void Server::EstablishMasterCommunication() {
    if (!ConnectToMaster()) {
        D(cout << "S" << get_pid() << " : ERROR in connecting to M" << endl;)
    }
    // send port to master
    string message;
    ConstructPortMessage(message);
    SendMessageToMaster(message);
}

/**
 * Does initial setup like creating accept thread, connecting to master
 * creating receive threads for master.
 * @param accept_connections_thread  thread for accepting incoming connections
 * @param receive_from_master_thread thread for receiving from master
 * @param receive_from_servers_thread thread for receiving from servers
 */
void Server::InitialSetup(pthread_t& accept_connections_thread,
                          pthread_t& receive_from_all_thread,
                          pthread_t& anti_entropy_p1_thread) {
    InitializeLocks();
    CreateThread(AcceptConnections, (void*)this, accept_connections_thread);

    while (get_my_listen_port() == -1) {
        usleep(kBusyWaitSleep);
    }
    EstablishMasterCommunication();

    CreateThread(ReceiveFromAll, (void*)this, receive_from_all_thread);
    CreateThread(AntiEntropyP1, (void*)this, anti_entropy_p1_thread);
}

/**
 * Initialize all mutex locks
 */
void Server::InitializeLocks() {
    if (pthread_mutex_init(&server_lock, NULL) != 0) {
        D(cout << "S" << get_pid() << " : Mutex init failed" << endl;)
        pthread_exit(NULL);
    }
    if (pthread_mutex_init(&misc_fd_lock, NULL) != 0) {
        D(cout << "S" << get_pid() << " : Mutex init failed" << endl;)
        pthread_exit(NULL);
    }
    if (pthread_mutex_init(&retiring_lock, NULL) != 0) {
        D(cout << "S" << get_pid() << " : Mutex init failed" << endl;)
        pthread_exit(NULL);
    }
    if (pthread_mutex_init(&pause_lock, NULL) != 0) {
        D(cout << "S" << get_pid() << " : Mutex init failed" << endl;)
        pthread_exit(NULL);
    }

}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    pthread_t accept_connections_thread;
    pthread_t receive_from_all_thread;
    pthread_t anti_entropy_p1_thread;

    Server S(argv);
    S.InitialSetup(accept_connections_thread,
                   receive_from_all_thread,
                   anti_entropy_p1_thread);

    S.ConnectToAllServers(argv);
    S.SendDoneToMaster();

    void* status;
    pthread_join(anti_entropy_p1_thread, &status);
    pthread_join(accept_connections_thread, &status);
    pthread_join(receive_from_all_thread, &status);
    return 0;
}