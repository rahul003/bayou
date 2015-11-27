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
pthread_mutex_t retiring_lock;

Server::Server(char** argv)
{
    pid_ = atoi(argv[2]);
    am_primary_ = (atoi(argv[3]) != 0);
    master_listen_port_ = atoi(argv[4]);
    my_listen_port_ = -1;

    if (am_primary_) {
        set_name(kPrimary);
        vclock_[name_] = 0;
    }

    max_csn_=0;

}

int Server::get_pid() {
    return pid_;
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

int Server::get_misc_fd(int port) {
    int fd = -1;
    pthread_mutex_lock(&misc_fd_lock);
    if (misc_fd_.find(port) != misc_fd_.end())
        fd = misc_fd_[port];
    pthread_mutex_unlock(&misc_fd_lock);
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

bool Server::get_retiring() {
    bool copy;
    pthread_mutex_lock(&retiring_lock);
    copy = retiring_;
    pthread_mutex_unlock(&retiring_lock);
    return copy;
}

void Server::set_retiring() {
    pthread_mutex_lock(&retiring_lock);
    retiring_ = true;
    pthread_mutex_unlock(&retiring_lock);
}

void Server::set_name(const string& my_name) {
    name_ = my_name;
}

void Server::set_misc_fd(int port, int fd) {
    pthread_mutex_lock(&misc_fd_lock);
    misc_fd_[port] = fd;
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

void Server::set_server_fd(const string& name, int fd) {
    pthread_mutex_lock(&server_lock);
    server_fd_[name] = fd;
    pthread_mutex_unlock(&server_lock);
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

std::unordered_map<int, int> Server::GetMiscFdCopy() {
    pthread_mutex_lock(&misc_fd_lock);
    std::unordered_map<int, int> misc_fd_copy(misc_fd_);
    pthread_mutex_unlock(&misc_fd_lock);
    return misc_fd_copy;
}

void Server::RemoveFromMiscFd(int port) {
    pthread_mutex_lock(&misc_fd_lock);
    misc_fd_.erase(port);
    pthread_mutex_unlock(&misc_fd_lock);
}

void* ReceiveFromMaster(void* _S) {
    Server* S = (Server*)_S;
    char buf[kMaxDataSize];
    int num_bytes;
    // D(cout << "S" << S->get_pid() << " : Receiving from M" << endl;)
    while (true) {  // always listen to messages from the master
        num_bytes = recv(S->get_master_fd(), buf, kMaxDataSize - 1, 0);
        if (num_bytes == -1) {
            D(cout << "S" << S->get_pid() << " : ERROR in receiving message from M" << endl;)
        } else if (num_bytes == 0) {    // connection closed by master
            D(cout << "S" << S->get_pid() << " : Connection closed by M" << endl;)
        } else {
            buf[num_bytes] = '\0';

            // extract multiple messages from the received buf
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kRetireServer) {
                    S->set_retiring();
                    string send_to = S->GetServerForRetireMessage();
                    string msg = kAntiEntropyP1 + kMessageDelim;
                    S->SendMessageToServer(send_to,msg);
                } else {    //other messages
                    D(cout << "S" << S->get_pid()
                      << " : ERROR Unexpected message received: " << msg << endl;)
                }
            }
        }
    }
    return NULL;
}


void* AntiEntropyP1(void* _S) {
    Server* S = (Server*)_S;
    string msg = kAntiEntropyP1 + kMessageDelim;
    string cur_server;
    while(true)
    {
        usleep(kAntiEntropyInterval);
        cur_server = S->GetNextServer(cur_server);
        S->SendMessageToServer(cur_server,msg);
    }
    return NULL;
}
string Server::GetServerForRetireMessage(){
    string rval;

    pthread_mutex_lock(&server_lock);
    auto it = server_fd_.begin();
    rval = it->first;
    pthread_mutex_unlock(&server_lock);
    return rval;
}

string Server::GetNextServer(string last){
    string rval;
    pthread_mutex_lock(&server_lock);
    auto it = server_fd_.upper_bound(last);
    if(it==server_fd_.end())
        it = server_fd_.begin();
    rval = it->first;
    pthread_mutex_unlock(&server_lock);
    return rval;
}

void Server::CreateFdSet(fd_set& fromset,
                         vector<int>& fds,
                         int& fd_max) {

    fd_max = INT_MIN;
    char buf;
    int fd_temp;
    FD_ZERO(&fromset);
    fds.clear();

    // client fds
    // for (auto it = client_fd_.begin(); it != client_fd_.end(); ) {
    //     fd_temp = it->first;
    //     if (fd_temp == -1) {
    //         continue;
    //     }
    //     int rv = recv(fd_temp, &buf, 1, MSG_DONTWAIT | MSG_PEEK);
    //     if (rv == 0) {
    //         close(fd_temp);
    //         it = client_fd_.erase(it);
    //     } else {
    //         FD_SET(fd_temp, &fromset);
    //         fd_max = max(fd_max, fd_temp);
    //         fds.push_back(fd_temp);
    //         it++;
    //     }
    // }

    // server fds
    map<string, int> server_fd_copy = GetServerFdCopy();
    for (auto it = server_fd_copy.begin(); it != server_fd_copy.end(); ) {
        fd_temp = it->second;
        if (fd_temp == -1) {
            continue;
        }
        int rv = recv(fd_temp, &buf, 1, MSG_DONTWAIT | MSG_PEEK);
        if (rv == 0) {
            D(cout << "S" << get_pid()
              << " : ERROR Unexpected peek error in server fd for server "
              << it->first << endl;)
            close(fd_temp);
            it++;
            // TODO: shoudl I erase it?
            // if yes, see if locking causes issue
            // ideally, retirement should take care of removing
            // this case should never happen
        } else {
            FD_SET(fd_temp, &fromset);
            fd_max = max(fd_max, fd_temp);
            fds.push_back(fd_temp);
            it++;
        }
    }

    // misc fds
    unordered_map<int, int> misc_fd_copy = GetMiscFdCopy();
    for (auto it = misc_fd_copy.begin(); it != misc_fd_copy.end(); ) {
        fd_temp = it->second;
        if (fd_temp == -1) {
            continue;
        }
        int rv = recv(fd_temp, &buf, 1, MSG_DONTWAIT | MSG_PEEK);
        if (rv == 0) {
            D(cout << "S" << get_pid()
              << " : ERROR Unexpected peek error in misc fd for tuple <"
              << it->first << "," << it->second << ">" << endl;)
            close(fd_temp);
            it++;
            // TODO: shoudl I erase it?
            // if yes, see if locking causes issue
            // ideally, this case should never happen
        } else {
            FD_SET(fd_temp, &fromset);
            fd_max = max(fd_max, fd_temp);
            fds.push_back(fd_temp);
            it++;
        }
    }
}

void Server::ReceiveFromServersAndMiscMode()
{
    char buf[kMaxDataSize];
    int num_bytes;

    fd_set fromset;
    std::vector<int> fds;
    int fd_max;

    while (true) {
        CreateFdSet(fromset, fds, fd_max);

        if (fd_max == INT_MIN) {
            usleep(kBusyWaitSleep);
            continue;
        }

        struct timeval timeout = kSelectTimeoutTimeval;
        int rv = select(fd_max + 1, &fromset, NULL, NULL, &timeout);
        if (rv == -1) { //error in select
            D(cout << "S" << get_pid()
              << ": ERROR in select() errno=" << errno
              << " fdmax=" << fd_max << endl;)
        } else if (rv == 0) {
            // D(cout << "S" << get_pid() << ": Select timeout" << endl;)
        } else {
            for (int i = 0; i < fds.size(); i++) {
                if (FD_ISSET(fds[i], &fromset)) { // we got one!!
                    if ((num_bytes = recv(fds[i], buf, kMaxDataSize - 1, 0)) == -1) {
                        D(cout << "S" << get_pid()
                          << ": ERROR in receiving" << endl;)

                        //TODO: What to do here?
                        // ResetFD(fds[i], primary_id);

                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "S" << get_pid() << ": Connection closed" << endl;)

                        //TODO: What to do here?
                        // ResetFD(fds[i], primary_id);

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
                                
                                int port = GetPortFromFd(fds[i]);
                                if (token[1] == kClient) {
                                    set_client_fd(port, fds[i]);
                                    RemoveFromMiscFd(port);
                                    SendDoneToClient(fds[i]);
                                } else if (token[1] == kServer || token[1] == kNewServer) {
                                    HandleInitialServerHandshake(port, fds[i], token);
                                    RemoveFromMiscFd(port);
                                }
                            } else if (token[0] == kYouAre) {
                                //YOUARE-MY_NAME-IAM-PEERNAME
                                D(assert(token.size() == 4);)

                                int port = GetPeerPortFromFd(fds[i]);
                                set_name(token[1]);
                                vclock_[name_] = 0;
                                set_server_name(port, token[3]);
                                set_server_fd(token[3], fds[i]);
                                vclock_[token[3]] = 0;
                                RemoveFromMiscFd(port);
                            } 
                            else if(token[0] == kAntiEntropyP1)
                            {
                                SendAEP1Response(fds[i]);
                            }
                            else if(token[0] == kAntiEntropyP1Resp)
                            {
                                D(assert(token.size()==3);)
                                int recvd_csn = stoi(token[1]);
                                unordered_map<string, int> recvd_clock;
                                StringToClock(token[2], recvd_clock);
                                string msg;
                                ConstructAEP2Message(msg, recvd_csn, recvd_clock);
                                SendMessageToServerByFd(fds[i], msg);

                                if(get_retiring())
                                {
                                    SendRetiringMsgToServer(fds[i]);                                    
                                    SendDoneToMaster();
                                }
                            }
                            else if (token[0]==kRetiring)
                            { //kretiring-kwasprim-$
                                if(token[1]==kWasPrim)
                                {
                                    am_primary_ = true;
                                    CommitTentativeWrites();
                                }
                                vclock_.erase(get_server_name(fds[i]));
                            }
                            else if(token[0]==kAntiEntropyP2)
                            {
                                D(assert(token.size()==4);)
                                if((!token[2].empty()) || (!token[3].empty()))
                                    ExtractAEP2Message(token[2], token[3]);
                            }
                            else if(token[0] == kServerVC)
                            {
                                unordered_map<int, int> port_to_clock;
                                CreatePortToClockMap(port_to_clock);
                                string msg=kServerVC+kInternalDelim;
                                msg+=UnorderedMapToString(port_to_clock)+kMessageDelim;
                                SendMessageToClient(fds[i],msg);
                            }
                            else if(token[0]==kGet)
                            {
                                string msg = kUrl+kInternalDelim;
                                msg += database_[token[1]]+kMessageDelim;
                                SendMessageToClient(fds[i], msg);

                                msg = kRelWrites+kInternalDelim;
                                msg += GetRelevantWrites(token[1]) + kMessageDelim;
                                SendMessageToClient(fds[i], msg);
                            }
                            else if(token[0] == kPut)
                            {
                                D(assert(token.size()==3);)
                                AddWrite(token[0], token[1], token[2]);
                                SendWriteIdToClient(fds[i]);
                            }
                            else if(token[0] == kDelete)
                            {
                                D(assert(token.size()==2);)
                                AddWrite(token[0], token[1], "");
                                SendWriteIdToClient(fds[i]);
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
string Server::GetRelevantWrites(string song)
{
    unordered_map<int, int> port_to_ts;
    for(auto &w: write_log_)
    {
        if(w.second.get_song()==song)
        {
            int p;
            if(w.first.get_sname()==get_name())
                p = my_listen_port_;
            else
                p = GetPeerPortFromFd(get_server_fd(w.first.get_sname()));
            port_to_ts[p] = w.first.get_accept_ts();
        }
    }
    
    return UnorderedMapToString(port_to_ts);
}
                                
void Server::CreatePortToClockMap(unordered_map<int, int>& port_to_clock)
{
    for(auto &c: vclock_)
    {
        if(c.first==get_name())
            port_to_clock[my_listen_port_] = c.second;
        else
            port_to_clock[GetPeerPortFromFd(get_server_fd(c.first))] = c.second;
    }
}
void Server::SendWriteIdToClient(int fd)
{
    string msg = kWriteID+kInternalDelim;
    msg += to_string(my_listen_port_)+kInternalDelim;
    msg += to_string(vclock_[get_name()])+kMessageDelim;
    SendMessageToClient(fd, msg);
}

void Server::AddWrite(string type, string song, string url)
{
    //new writes will have clock from 1
    vclock_[get_name()]++;
    IdTuple w;
    if(am_primary_)
    {
        max_csn_++;
        w = IdTuple(max_csn_, get_name(), vclock_[get_name()]);
    }
    else
        w = IdTuple(INT_MAX, get_name(), vclock_[get_name()]);

    Command c(type,song, url);
    write_log_[w] = c;
    ExecuteCommandsOnDatabase(w);
}

void Server::CommitTentativeWrites(){
    auto it = write_log_.lower_bound(IdTuple(max_csn_+1,"", 0));
    IdTuple first;
    bool first_set = false;
    while(it!=write_log_.end())
    {
        Command c = it->second;
        IdTuple w = it->first;
        write_log_.erase(it->first);
        max_csn_++;
        w = IdTuple(max_csn_, w.get_sname(), w.get_accept_ts());
        if (!first_set)
        {
            first = w;
            first_set = true;
        }
        write_log_[w] = c;
    }
    ExecuteCommandsOnDatabase(first);
}

void* ReceiveFromServersAndMisc(void* _S) {
    Server* S = (Server*)_S;
    S->ReceiveFromServersAndMiscMode();
    return NULL;
}

void Server::HandleInitialServerHandshake(int port, int fd, const std::vector<string>& token) {
    // token[0] is kIAm
    if (token[1] == kServer) {  // sending server already has name
        //IAM-SERVER-SERVER_NAME
        D(assert(token.size() == 3);)
        set_server_name(port, token[2]);
        set_server_fd(token[2], fd);
        vclock_[token[2]] = 0;

        string message;
        ConstructMessage(kIAm, get_name(), message);
        SendMessageToServer(token[2], message);

    } else if (token[1] == kNewServer) {    // sending server is new. Does not have a name
        string name = CreateName();
        set_server_name(port, name);
        set_server_fd(name, fd);
        vclock_[name] = 0;

        // send his and my name to peer
        string you_are_msg;
        string i_am_msg;
        ConstructMessage(kYouAre, name, you_are_msg);
        ConstructMessage(kIAm, get_name(), i_am_msg);

        string message = you_are_msg;
        message.pop_back();
        message = message + kInternalDelim + i_am_msg;
        SendMessageToServer(name, message);
    }
}

string Server::CreateName() {
    return (get_name() + kComma + to_string(vclock_[name_]));
}

void Server::SendOrAskName(int fd) {
    if (!get_name().empty())
    {
        string msg;
        ConstructIAmMessage(kIAm, kServer, get_name(), msg);
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
    msg+=to_string(max_csn_) + kInternalDelim;
    msg += ClockToString(vclock_) + kMessageDelim;
    SendMessageToServerByFd(fd, msg);
}

void Server::ConstructAEP2Message(string& msg, const int& r_csn, unordered_map<string, int>& r_clock)
{
    //AEP2-c1.n1.w1.m1a.m1b.m1c-n1.w1,m1
    //AEP2-commitedwrites-tentativewrites$
    //only this thread will use max_csn no need of lock
    msg += kAntiEntropyP2+kInternalDelim;
    string committed,new_tent;
    if(r_csn<max_csn_)
    {
        //gives first element higher than this
        auto it = write_log_.lower_bound(IdTuple(r_csn+1,"",0));
        while(it->first.get_csn()<=max_csn_)
        {  
            committed+=WriteToString(it->first, it->second)+kComma;
            it++;
        }
    }
    msg+=committed+kInternalDelim;
    auto it = write_log_.lower_bound(IdTuple(max_csn_+1,"", 0));
    while(it!=write_log_.end())
    {
        if((it->first.get_accept_ts())>(r_clock[it->first.get_sname()]))
        {
            new_tent+=WriteToString(it->first, it->second)+kComma;
        }
        it++;
    }
    msg+=new_tent+kMessageDelim;
}
IdTuple Server::RollBack(const string& committed_writes, const string& tent_writes)
{
    IdTuple earliest;
    bool earliest_found = false;
    vector<string> writes, parts;
    if(!committed_writes.empty()){
        writes = split(committed_writes, kComma[0]);
        parts = split(writes[0], kInternalWriteDelim[0]);
        IdTuple cur_first(stoi(parts[0]), parts[1], stoi(parts[2]));
        earliest = cur_first;
        earliest_found = true;
    }
    if(!tent_writes.empty() && !earliest_found)
    {
        writes = split(tent_writes, kComma[0]);
        parts = split(writes[0], kInternalWriteDelim[0]);
        IdTuple cur_first(INT_MAX, parts[0], stoi(parts[1]));
        
        earliest = cur_first;
        earliest_found = true;        
    }
    D(assert(earliest_found==true);)

    auto it=write_log_.rbegin();
    while(it!=write_log_.rend() && it->first>earliest)
    {
        Command undo_c = undo_log_[it->first];
        if (undo_c.get_type() == kDelete)
        {
            database_.erase(undo_c.get_song());
        }
        else if(undo_c.get_type()==kUndo)
        {
            database_[undo_c.get_song()] = undo_c.get_url();
        }
        undo_log_.erase(it->first);
        it++;
    }
    return earliest;
}

void Server::ExtractAEP2Message(const string& committed_writes, const string& tent_writes){
    //AEP2-c1.n1.w1.m1-n1.w1,m1
    //AEP2-commitedwrites-tentativewrites$
    IdTuple from = RollBack(committed_writes, tent_writes);

    vector<string> writes = split(committed_writes, kComma[0]);
    for(auto &p: writes)
    {
        vector<string> parts = split(p, kInternalWriteDelim[0]);
        D(assert(parts.size()==6);)
        IdTuple w(INT_MAX, parts[1], stoi(parts[2]));
        if(write_log_.find(w)!=write_log_.end())
        {
            write_log_.erase(w);
        }
        Command c(parts[3], parts[4], parts[5]);
        IdTuple w_new(stoi(parts[0]), parts[1], stoi(parts[2]));
        write_log_[w_new] = c;
    }
    writes = split(tent_writes, kComma[0]);
    for(auto &p: writes)
    {
        vector<string> parts = split(p, kInternalWriteDelim[0]);
        D(assert(parts.size()==5);)
        IdTuple w(INT_MAX, parts[0], stoi(parts[1]));
        Command c(parts[2], parts[3], parts[4]);
        write_log_[w] = c;
    }

    ExecuteCommandsOnDatabase(from);

}
// ExecCommandOnDatabase(w_new, parts[3], parts[4], parts[5]);

void Server::ExecuteCommandsOnDatabase(IdTuple from)
{
    auto it=write_log_.find(from);

    while(it!=write_log_.end())
    {
        IdTuple w = it->first;
        string song = it->second.get_song();
        string url = it->second.get_url();
        string type = it->second.get_type();

        if (type==kPut)
        {
            if(w.get_csn()==INT_MAX)
            {
                //undo command for a write needed only if tentative
                if(database_.find(song)==database_.end())
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
        else if (type==kDelete)
        {
            if(w.get_csn()==INT_MAX)
            {
                undo_log_[w] = Command(kUndo, song, database_[song]);
            }
            database_.erase(song);
        }

        //updating vector clock if behind. it will be behind for new writes
        if(vclock_[w.get_sname()]<w.get_accept_ts())
            vclock_[w.get_sname()] = w.get_accept_ts();

        it++;
    }
    
}

void Server::SendRetiringMsgToServer(int fd)
{
    string msg = kRetiring + kInternalDelim;
    if(am_primary_)
        msg+=kWasPrim;
    msg += kInternalDelim + kMessageDelim;

    SendMessageToServerByFd(fd, msg);
    char buf[kMaxDataSize];
    int num_bytes;

    bool ack = false;
    while (!ack) {     // connection with server has timeout
        if ((num_bytes = recv(fd, buf, kMaxDataSize - 1, 0)) == -1) {
            // D(cout << "M  : ERROR in receiving ack from someone, fd=" << fd << endl;)
        }
        else if (num_bytes == 0) {   //connection closed
            D(cout << "M  : ERROR Connection closed by someone, fd=" << fd << endl;)
        }
        else {
            buf[num_bytes] = '\0';
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kAck) {
                    ack = true;
                    D(cout << "S  "<<get_name()<<": ACK received" << endl;)
                } else {
                    D(cout << "S  "<<get_name()<<": Unexpected message received at fd="
                      << fd << ": " << msg << endl;)
                }
            }
        }
    }
}

void Server::SendMessageToServerByFd(int fd, const string & message) {
    if (send(fd, message.c_str(), message.size(), 0) == -1) {
        D(cout << "S" << get_pid() << " : ERROR: Cannot send message to S" << endl;)
    } else {
        D(cout << "S" << get_pid() << " : Message sent to S: " << message << endl;)
    }
}

void Server::SendMessageToServer(const string& name, const string & message) {
    if (send(get_server_fd(name), message.c_str(), message.size(), 0) == -1) {
        D(cout << "S" << get_pid() << " : ERROR: Cannot send message to server " << name << endl;)
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
              to_string(get_my_listen_port()) + kMessageDelim;
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
                          pthread_t& receive_from_master_thread,
                          pthread_t& receive_from_servers_thread,
                          pthread_t& anti_entropy_p1_thread) {
    InitializeLocks();
    CreateThread(AcceptConnections, (void*)this, accept_connections_thread);

    while (get_my_listen_port() == -1) {
        usleep(kBusyWaitSleep);
    }
    EstablishMasterCommunication();

    CreateThread(ReceiveFromMaster, (void*)this, receive_from_master_thread);
    CreateThread(ReceiveFromServersAndMisc, (void*)this, receive_from_servers_thread);
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
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    pthread_t accept_connections_thread;
    pthread_t receive_from_master_thread;
    pthread_t receive_from_servers_thread;
    pthread_t anti_entropy_p1_thread;

    Server S(argv);
    S.InitialSetup(accept_connections_thread,
                   receive_from_master_thread,
                   receive_from_servers_thread,
                   anti_entropy_p1_thread);

    S.ConnectToAllServers(argv);
    S.SendDoneToMaster();

    void* status;
    pthread_join(anti_entropy_p1_thread, &status);   
    pthread_join(accept_connections_thread, &status);
    pthread_join(receive_from_master_thread, &status);
    pthread_join(receive_from_servers_thread, &status);
    return 0;
}