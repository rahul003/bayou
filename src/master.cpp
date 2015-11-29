#include "../inc/master.h"
#include "../inc/constants.h"
#include "../inc/utils.h"

#include "iostream"
#include "vector"
#include "string"
#include "fstream"
#include "sstream"
#include "cstring"
#include "unistd.h"
#include "fcntl.h"
#include "spawn.h"
#include "sys/types.h"
#include "signal.h"
#include "errno.h"
#include "limits.h"
#include "sys/socket.h"
#include "assert.h"
#include "pthread.h"
using namespace std;

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

extern char **environ;

Master::Master() {
    primary_id_ = -1;
    master_port_ = kMasterPort;
    file_num_ = 0;
}

int Master::get_master_port() {
    return master_port_;
}

int Master::get_server_listen_port(const int server_id) {
    if (server_listen_port_.find(server_id) != server_listen_port_.end())
        return server_listen_port_[server_id];
    else
        return -1;
}

int Master::get_server_fd(const int server_id) {
    if (server_fd_.find(server_id) != server_fd_.end())
        return server_fd_[server_id];
    else
        return -1;
}

int Master::get_client_fd(const int client_id) {
    if (client_fd_.find(client_id) != client_fd_.end())
        return client_fd_[client_id];
    else
        return -1;
}

int Master::get_primary_id() {
    return primary_id_;
}

int Master::get_num_servers() {
    return server_listen_port_.size();
}

string Master::get_server_name(int server_id) {
    if(server_name_.find(server_id) != server_name_.end())
        return server_name_[server_id];
    else
        return "";
}

void Master::set_primary_id(const int id) {
    primary_id_ = id;
}

void Master::set_server_fd(const int server_id, const int fd) {
    server_fd_[server_id] = fd;
    SetCloseExecFlag(fd);
}

void Master::set_client_fd(const int client_id, const int fd) {
    client_fd_[client_id] = fd;
    SetCloseExecFlag(fd);
}

void Master::set_server_listen_port(const int server_id, const int port_num) {
    server_listen_port_[server_id] = port_num;
}

void Master::set_server_name(int server_id, const string& name) {
    server_name_[server_id] = name;
}

void Master::SetCloseExecFlag(const int fd) {
    if (fd == -1)
        return;

    int flags;

    flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        D(cout << "M  : ERROR in setting CloseExec for " << fd << endl;)
    }
    flags |= FD_CLOEXEC;
    if (fcntl(fd, F_SETFD, flags) == -1) {
        D(cout << "M  : ERROR in setting CloseExec for " << fd << endl;)
    }
}

/**
 * spawns 1 server and connects to them
 * @param server_id id of server to be spawned
 * @param isPrimary true if the server with id=server_id is primary
 * @return true if server wes spawned successfully
 */
bool Master::SpawnServer(const int server_id, bool isPrimary) {
    pid_t pid;
    int status;

    int num_basic_arg = 5;
    int num_arg = num_basic_arg + get_num_servers();

    /*
    0 - server executable
    1 - numargs
    2 - serverid
    3- whether it is priamry or not
    4 - master port
    5 onwards - other servers ports
    */

    char num_args[10];
    sprintf(num_args, "%d", num_arg);

    char server_id_arg[10];
    sprintf(server_id_arg, "%d", server_id);

    char is_primary_arg[10];
    sprintf(is_primary_arg, "%d", isPrimary);

    char master_port_arg[10];
    sprintf(master_port_arg, "%d", master_port_);

    char ** other_servers_port_arg;
    other_servers_port_arg = new char *[get_num_servers()];
    //confirm that no other thread writes to server_listen_ports
    int i = 0;
    for (auto &s_p : server_listen_port_) {
        other_servers_port_arg[i] = new char[10];
        sprintf(other_servers_port_arg[i], "%d", s_p.second);
        i++;
    }

    char **argv;
    argv = new char*[num_arg + 1];
    argv[0] = (char*)kServerExecutable.c_str();
    argv[1] = num_args;
    argv[2] = server_id_arg;
    argv[3] = is_primary_arg;
    argv[4] = master_port_arg;
    for (int i = num_basic_arg; i < num_arg; i++)
    {
        argv[i] = other_servers_port_arg[i - num_basic_arg];
    }
    argv[num_arg] = NULL;

    status = posix_spawn(&pid,
                         (char*)kServerExecutable.c_str(),
                         NULL,
                         NULL,
                         argv,
                         environ);
    if (status == 0) {
        D(cout << "M  : Spawned S" << server_id << endl;)
        all_pids_[server_id] = pid;
    } else {
        D(cout << "M  : ERROR: Cannot spawn S"
          << server_id << " - " << strerror(status) << endl);
        return false;
    }

    while (get_server_fd(server_id) == -1 || get_server_name(server_id) == "") {
        usleep(kBusyWaitSleep);
    }
    // don't wait for done. uses port message as ACK.
    // DONE might cause issue with two threads receiving at same fd
    // WaitForDone(get_server_fd(server_id));
    return true;
}

bool Master::SpawnClient(const int client_id, const int server_id) {
    pid_t pid;
    int status;

    int num_basic_arg = 5;
    int num_arg = num_basic_arg + get_num_servers();

    /*
    0 - client executable
    1 - numargs
    2 - client id
    3 - port of server to whom to connect
    4 - master port
    5 onwards - every server's port
    */

    char num_args[10];
    sprintf(num_args, "%d", num_arg);

    char client_id_arg[10];
    sprintf(client_id_arg, "%d", client_id);

    char server_port_arg[10];

    char master_port_arg[10];
    sprintf(master_port_arg, "%d", master_port_);

    char ** all_servers_port_arg;
    all_servers_port_arg = new char *[get_num_servers()];
    //TODO: confirm that no other thread writes to server_listen_port
    int i = 0;
    for (auto &s_p : server_listen_port_) {
        all_servers_port_arg[i] = new char[10];
        sprintf(all_servers_port_arg[i], "%d", s_p.second);

        if (server_id == s_p.first) {   // server to whom to connect
            sprintf(server_port_arg, "%d", s_p.second);
        }

        i++;
    }

    char **argv;
    argv = new char*[num_arg + 1];
    argv[0] = (char*)kClientExecutable.c_str();
    argv[1] = num_args;
    argv[2] = client_id_arg;
    argv[3] = server_port_arg;
    argv[4] = master_port_arg;
    for (int i = num_basic_arg; i < num_arg; i++) {
        argv[i] = all_servers_port_arg[i - num_basic_arg];
    }
    argv[num_arg] = NULL;

    status = posix_spawn(&pid,
                         (char*)kClientExecutable.c_str(),
                         NULL,
                         NULL,
                         argv,
                         environ);
    if (status == 0) {
        D(cout << "M  : Spawned C" << client_id << endl;)
        all_pids_[client_id] = pid;
    } else {
        D(cout << "M  : ERROR: Cannot spawn C"
          << client_id << " - " << strerror(status) << endl);
        return false;
    }

    while (get_client_fd(client_id) == -1) {
        usleep(kBusyWaitSleep);
    }

    // don't wait for done. use IamClient as ACK.
    // DONE might cause issue with two threads receiving at same fd
    // WaitForDone(get_client_fd(client_id));
    return true;
}

void Master::SendRetireMessage(int id)
{
    string msg = kRetireServer + kInternalDelim+ kMessageDelim;
    SendMessageToServer(id, msg);
}

int Master::SendLogRequest(int id)
{
    string msg = kPrintLog + kInternalDelim + kMessageDelim;
    return SendMessageToServer(id, msg);
}

int Master::SendToAllServers(const string& msg)
{
    int c=0;
    for(auto &s : server_fd_)
    {
        c = c+SendMessageToServer(s.first, msg);
    }
    return c;
}
/**
 * receives DONE message from someone
 * @param fd fd on which DONE is expected
 */
void Master::WaitForDone(const int fd) {
    char buf[kMaxDataSize];
    int num_bytes;

    bool done = false;
    while (!done) {
        done = true;
        if ((num_bytes = recv(fd, buf, kMaxDataSize - 1, 0)) == -1) {
            D(cout << "M  : ERROR in receiving DONE from someone, fd=" << fd << endl;)
        }
        else if (num_bytes == 0) {   //connection closed
            D(cout << "M  : ERROR Connection closed by someone, fd=" << fd << endl;)
        }
        else {
            buf[num_bytes] = '\0';
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kDone) {
                    D(cout << "M  : DONE received" << endl;)
                } else {
                    done = false;
                    D(cout << "M  : Unexpected message received at fd="
                      << fd << ": " << msg << endl;)
                }
            }
        }
    }
}

void Master::WaitForAll(const int num) {
    int c=0;
    for(auto &s: server_fd_)
    {
        WaitForDone(s.second);
        c++;
    }
    D(assert(c==num);)
}

void Master::WaitForLogResponse(const int server_id) {
    char buf[kMaxDataSize];
    int num_bytes;

    bool done = false;
    while (!done) {
        done = true;
        if ((num_bytes = recv(get_server_fd(server_id), buf, kMaxDataSize - 1, 0)) == -1) {
            D(cout << "M  : ERROR in receiving Log from S" << server_id << endl;)
        }
        else if (num_bytes == 0) {   //connection closed
            D(cout << "M  : ERROR Connection closed by server S" << server_id << endl;)
        }
        else {
            buf[num_bytes] = '\0';
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kMyLog) {
                    D(assert(token.size() == 2);)
                    D(cout << "M  : WriteLog received" << endl;)
                    ProcessAndPrintLog(server_id, token[1]);
                } else {
                    done = false;
                    D(cout << "M  : Unexpected message received by server S:"
                      << server_id << ": " << msg << endl;)
                }
            }
        }
    }
}
void Master::ProcessAndPrintLog(int id, const string& log)
{
    vector<string> writes = split(log,kInternalListDelim[0]);
    fstream f((kLogFileName + to_string(file_num_)+","+to_string(id)), fstream::out);
    for(auto&w: writes)
        f << w << endl;
    f << "----------------"<<endl;
    f.close();
    file_num_++;
}
/**
 * receives port num from someone. Sets appropriate listen_port variable
 * @param fd fd on which port num is expected
 */
void Master::WaitForPortMessage(const int fd) {
    char buf[kMaxDataSize];
    int num_bytes;

    bool done = false;
    while (!done) {
        done = true;
        if ((num_bytes = recv(fd, buf, kMaxDataSize - 1, 0)) == -1) {
            D(cout << "M  : ERROR in receiving PORT from someone, fd=" << fd << endl;)
        }
        else if (num_bytes == 0) {   //connection closed
            D(cout << "M  : ERROR Connection closed by someone, fd=" << fd << endl;)
        }
        else {
            buf[num_bytes] = '\0';
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                // PORT-SERVER-ID-PORT_NUM-Servername
                // or IAM-CLIENT-ID
                if (token[0] == kPort) {
                    D(assert(token.size() == 5);)
                    if (token[1] == kServer) {
                        int server_id = stoi(token[2]);
                        int port_num = stoi(token[3]);
                        string server_name = token[4];
                        D(cout << "M  : PORT received from S" << server_id << endl;)
                        set_server_listen_port(server_id, port_num);
                        set_server_fd(server_id, fd);
                        set_server_name(server_id, server_name);
                    } else {
                        D(cout << "M  : Unexpected message received at fd="
                          << fd << ": " << msg << endl;)
                    }
                } else if (token[0] == kIAm) {
                    D(assert(token.size() == 3);)
                    if (token[1] == kClient) {
                        int client_id = stoi(token[2]);
                        D(cout << "M  : IAM-CLIENT received from C" << client_id << endl;)
                        set_client_fd(client_id, fd);
                    } else {
                        D(cout << "M  : Unexpected message received at fd="
                          << fd << ": " << msg << endl;)
                    }
                } else {
                    done = false;
                    D(cout << "M  : Unexpected message received at fd="
                      << fd << ": " << msg << endl;)
                }
            }
        }
    }
}

/**
 * receives URL from client
 * @param fd fd of client
 */
void Master::GetUrlFromClient(const int fd) {
    char buf[kMaxDataSize];
    int num_bytes;

    if ((num_bytes = recv(fd, buf, kMaxDataSize - 1, 0)) == -1) {
        D(cout << "M  : ERROR in receiving url from C" << endl;)
    }
    else if (num_bytes == 0) {   //connection closed
        D(cout << "M  : ERROR Connection closed by C" << endl;)
    }
    else {
        buf[num_bytes] = '\0';
        std::vector<string> message = split(string(buf), kMessageDelim[0]);
        for (const auto &msg : message) {
            std::vector<string> token = split(string(msg), kInternalDelim[0]);
            if (token[0] == kUrl) {
                D(assert(token.size() == 2);)
                D(cout << "M  : URL received from C"
                  << " : " << token[1] << endl;)
            } else {
                D(cout << "M  : Unexpected message received from C"
                  << ": " << msg << endl;)
            }
        }
    }
}

/**
 * sends put command to client
 * @param client_id id of client to which command needs to be sent
 * @param song_name song name to be put
 * @param url       url associated with song_name
 */
void Master::SendPutToClient(int client_id,
                             const string& song_name,
                             const string& url) {
    string message = kPut + kInternalDelim +
                     song_name + kInternalDelim +
                     url + kInternalDelim + kMessageDelim;
    SendMessageToClient(client_id, message);
    WaitForDone(get_client_fd(client_id));
}

/**
 * sends delete command to client
 * @param client_id id of client to which command needs to be sent
 * @param song_name song name to be deleted
 */
void Master::SendDeleteToClient(int client_id,
                                const string& song_name) {
    string message = kDelete + kInternalDelim +
                     song_name + kInternalDelim + kMessageDelim;
    SendMessageToClient(client_id, message);
    WaitForDone(get_client_fd(client_id));
}

/**
 * sends get command to client
 * @param client_id id of client to which command needs to be sent
 * @param song_name song name whose url needs to be retrieved
 */
void Master::SendGetToClient(int client_id,
                             const string& song_name) {
    string message = kGet + kInternalDelim +
                     song_name + kInternalDelim + kMessageDelim;
    SendMessageToClient(client_id, message);
    GetUrlFromClient(get_client_fd(client_id));
}

/**
 * sends message to a client
 * @param client_id id of client to which message needs to be sent
 * @param message   message to be sent
 */
void Master::SendMessageToClient(const int client_id, const string & message) {
    if (send(get_client_fd(client_id), message.c_str(), message.size(), 0) == -1) {
        D(cout << "M  : ERROR: Cannot send message to C" << client_id << endl;)
    } else {
        D(cout << "M  : Message sent to C" << client_id << ": " << message << endl;)
    }
}

/**
 * sends message to a server
 * @param server_id id of server to which message needs to be sent
 * @param message   message to be sent
 */
int Master::SendMessageToServer(const int server_id, const string & message) {
    if (send(get_server_fd(server_id), message.c_str(), message.size(), 0) == -1) {
        D(cout << "M  : ERROR: Cannot send message to S" << server_id << endl;)
        return 0;
    } else {
        D(cout << "M  : Message sent to S" << server_id << ": " << message << endl;)
        return 1;
    }
}

void Master::ConstructMessage(const string & type, const string & body, string & message) {
    message = type + kInternalDelim + body + kMessageDelim;
}

/**
 * kills all processes. Only used at end.
 */
void Master::KillAllProcesses() {
    for (auto &p : all_pids_) {
        if (p.second != -1) {
            kill(p.second, SIGKILL);
        }
    }
}

void Master::SendChangeConnectionServer(const string& type, int id, const string& name, int port){

    string msg = type + kInternalDelim;
    if(port == -1) { //breakconnection
        msg += name + kInternalDelim + kMessageDelim;
    } else { // restoreConnection
        msg += name + kInternalDelim + to_string(port) + kInternalDelim+ kMessageDelim;
    }
    SendMessageToServer(id, msg);
    WaitForDone(get_server_fd(id));

}

void Master::SendChangeConnectionClient(string type, int id, int port){

    string msg = type + kInternalDelim;
    msg += to_string(port) + kInternalDelim + kMessageDelim;
    SendMessageToClient(id, msg);
    WaitForDone(get_client_fd(id));
}

/**
 * kills the specified server. Set its pid and fd to -1
 * @param server_id id of server to be killed
 */
void Master::CrashServer(const int server_id)
{
    int pid = all_pids_[server_id];
    if (pid != -1) {
        kill(pid, SIGKILL);
        all_pids_.erase(server_id);
        server_fd_.erase(server_id);
        server_listen_port_.erase(server_id);
        server_name_.erase(server_id);
        D(cout << "M  : Server S" << server_id << " killed" << endl;)
    }
}

/**
 * reads test commands from stdin
 */
void Master::ReadTest() {
    string line;
    while (getline(std::cin, line)) {
        if (line[0] == '#')
            continue;

        std::istringstream iss(line);
        string keyword;
        iss >> keyword;
        if (keyword == kJoinServer) {
            int id;
            iss >> id;
            if (primary_id_ != -1)
                SpawnServer(id);
            else
            {
                SpawnServer(id, true);
                set_primary_id(id);
            }
            servers_.AddNode(id);

        } else if (keyword == kRetireServer){
            int id;
            iss >> id;
            SendRetireMessage(id);
            WaitForDone(get_server_fd(id));
            CrashServer(id);

            servers_.RemoveNode(id);

        } else if(keyword == kBreakConnection){
            int id1, id2;
            iss>> id1 >> id2;

            if(is_client_id(id1) && is_server_id(id2))
            {
                SendChangeConnectionClient(kBreakConnection, id1, server_listen_port_[id2]);
            }
            if(is_server_id(id1) && is_client_id(id2))
            {
                SendChangeConnectionClient(kBreakConnection, id2, server_listen_port_[id1]);
            }
            if(is_server_id(id1) && is_server_id(id2))
            {
                SendChangeConnectionServer(kBreakConnection, id1, server_name_[id2]);
                servers_.RemoveEdge(id1, id2);
            }  
        }else if(keyword == kRestoreConnection){
            int id1, id2;
            iss>> id1 >> id2;
            //to make things simple, restore is only sent to client if a client is part of the pair
            if(is_client_id(id1) && is_server_id(id2))
            {
                SendChangeConnectionClient(kRestoreConnection, id1, server_listen_port_[id2]);
            }
            if(is_server_id(id1) && is_client_id(id2))
            {
                SendChangeConnectionClient(kRestoreConnection, id2, server_listen_port_[id1]);
            }
            if(is_server_id(id1) && is_server_id(id2))
            {
                //anyone is good enough
                SendChangeConnectionServer(kRestoreConnection, id1, server_name_[id2], server_listen_port_[id2]);
                servers_.AddEdge(id1, id2);
            }  
        }else if (keyword == kStabilize){
            StabilizeMode();
        }else if(keyword == kPrintLog){
            int id;
            iss>>id;
            if(SendLogRequest(id) !=0 )
                WaitForLogResponse(id);
        }
        else if(keyword == kPause)
        {
            string msg = kPause + kInternalDelim + kMessageDelim;
            int num = SendToAllServers(msg);
            WaitForAll(num);
        } else if(keyword == kStart){
            string msg = kStart + kInternalDelim + kMessageDelim;
            int num = SendToAllServers(msg);
            WaitForAll(num);
        } else if (keyword == kJoinClient){
            int cid, sid;
            iss >> cid >> sid;
            SpawnClient(cid, sid);
        } else if (keyword == kPut){
            int cid;
            string song_name, url;
            iss >> cid >> song_name >> url;
            SendPutToClient(cid, song_name, url);
        } else if (keyword == kGet) {
            int cid;
            string song_name;
            iss >> cid >> song_name;
            SendGetToClient(cid, song_name);
        } else if (keyword == kDelete) {
            int cid;
            string song_name;
            iss >> cid >> song_name;
            SendDeleteToClient(cid, song_name);
        }
    }
}

void Master::StabilizeMode(){
    set<set<int> > components = servers_.GetConnectedComponents();

    string msg = kServerVC + kInternalDelim + kMaster + kInternalDelim + kMessageDelim;
    for(auto &comp : components)
    {
        auto it = comp.begin();
        unordered_map<string, int> prev_vclock;
        int prev_csn, csn;
        while(it!=comp.end())
        {
            SendMessageToServer((*it), msg);
            string message = WaitForVC(*it);
            unordered_map<string, int> vclock;

            std::vector<string> token = split(message, kInternalDelim[0]);
            vclock = StringToUnorderedMap(token[1]);
            csn = stoi(token[2]);

            if(it==comp.begin()) {
                prev_vclock = vclock;
                prev_csn = csn;
                it++;
            }
            else if(vclock != prev_vclock || csn != prev_csn)
            {
                it = comp.begin();
                usleep(kAntiEntropyInterval);
                usleep(kAntiEntropyInterval);
            }
            else
                it++;
        }
        //this comes out when all VCs in a connected component are equal
    }
}

string Master::WaitForVC(int sid){
    char buf[kMaxDataSize];
    int num_bytes;
    string rval;
    bool done = false;
    while (!done) {
        done = true;
        if ((num_bytes = recv(get_server_fd(sid), buf, kMaxDataSize - 1, 0)) == -1) {
            D(cout << "M  : ERROR in receiving VC from S" << sid << endl;)
        }
        else if (num_bytes == 0) {   //connection closed
            D(cout << "M  : ERROR Connection closed by server S" << sid << endl;)
        }
        else {
            buf[num_bytes] = '\0';
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kServerVC) {
                    D(assert(token.size()==3);)
                    rval = msg;
                } else {
                    done = false;
                    D(cout<<"M  : ERROR Unexpected message received in WaitForVC from "<<sid<<": "<<msg<<endl;)
                }
            }
        }
    }
    return rval;
}

bool Master::is_client_id(int id){
    return !(is_server_id(id));
}
bool Master::is_server_id(int id){
    if(server_listen_port_.find(id)!=server_listen_port_.end())
        return true;
    else
        return false;
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    Master M;

    pthread_t accept_connections_thread;
    CreateThread(AcceptConnections, (void*)&M, accept_connections_thread);

    M.ReadTest();

    usleep(3000 * 1000);
    D(cout << "M  : GOODBYE. Killing everyone..." << endl;)
    M.KillAllProcesses();
    return 0;
}