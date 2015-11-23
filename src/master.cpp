#include "../inc/master.h"
#include "../inc/constants.h"

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
#include "pthread.h"
using namespace std;

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

extern char **environ;

Master::Master()
{
    primary_id_ = -1;
    master_port_ = 20000;
    current_server_port_ = 20001;
    current_client_port_ = 30001;
}

int Master::get_master_port() {
    return master_port_;
}

int Master::get_server_listen_port(const int server_id) {
    return server_listen_ports_[server_id];
}

int Master::get_client_listen_port(const int client_id) {
    return client_listen_ports_[client_id];
}

int Master::get_server_fd(const int server_id) {
    return server_fd_[server_id];
}

int Master::get_client_fd(const int client_id) {
    return client_fd_[client_id];
}

int Master::get_primary_id(){
    return primary_id_;
}

int Master::get_num_servers(){
    return server_listen_ports_.size();

}
void Master::set_primary_id(const int id){
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
 * @return true if server wes spawned successfully
 */
 bool Master::SpawnServer(const int server_id, bool isPrimary) {
    pid_t pid;
    int status;
    
    int num_basic_arg = 6;
    int num_arg = num_basic_arg+get_num_servers();
    char num_args[10];
    sprintf(num_args, "%d", num_arg);
    /*
    0 - server executable
    1 - numargs
    2 - serverid
    3- whether it is priamry or not
    4 - master port
    5 - servers port
    6 onwards - other servers ports
    */

    char server_id_arg[10];
    sprintf(server_id_arg, "%d", server_id);

    char is_primary_arg[10];
    sprintf(is_primary_arg, "%d", isPrimary);

    char master_port_arg[10];
    sprintf(master_port_arg, "%d", master_port_);

    char port_arg[10];
    sprintf(port_arg, "%d", current_server_port_);

    char ** other_servers_port_arg;
    other_servers_port_arg = new char *[get_num_servers()]; 
    //confirm that no other thread writes to server_listen_ports
    int i=0;
    for(auto &s_p: server_listen_ports_){
        other_servers_port_arg[i] = new char[10];
        sprintf(other_servers_port_arg[i], "%d", get_server_listen_port(s_p.second));
        i++;
    }
    
    char **argv;
    argv = new char*[num_arg];
    argv[0] = (char*)kServerExecutable.c_str();
    argv[1] = num_args;
    argv[2] = server_id_arg;
    argv[3] = is_primary_arg;
    argv[4] = master_port_arg;
    argv[5] = port_arg;
    for(int i=num_basic_arg; i<num_arg; i++)
    {
        argv[i] = other_servers_port_arg[i-num_basic_arg];
    }
    
    // char *argv[] = {,
    //     server_id_arg,
    //     is_primary_arg,
    //     port_arg,
    //     NULL
    // };
    status = posix_spawn(&pid,
       (char*)kServerExecutable.c_str(),
       NULL,
       NULL,
       argv,
       environ);
    if (status == 0) {
        D(cout << "M  : Spawned server S" << server_id << endl;)
        all_pids_.push_back(pid);
    } else {
        D(cout << "M  : ERROR: Cannot spawn server S"
          << server_id << " - " << strerror(status) << endl);
        return false;
    }

    server_listen_ports_[server_id]=current_server_port_;
    current_server_port_++;

    // sleep for some time to make sure accept threads of server are running
    usleep(kGeneralSleep);
    usleep(kGeneralSleep);
    if (ConnectToServer(server_id)) {
        D(cout << "M  : Connected to server S" << server_id << endl;)
    } else {
        D(cout << "M  : ERROR: Cannot connect to server S" << server_id << endl;)
        return false;
    }

    return true;
}

bool Master::SpawnClient(const int client_id, const int server_id) {
    pid_t pid;
    int status;
    char client_id_arg[10];
    char server_id_arg[10];
    char port_arg[10];
    char server_port_arg[10];
    sprintf(port_arg, "%d", current_client_port_);
    sprintf(server_port_arg, "%d", get_server_listen_port(server_id));

    sprintf(client_id_arg, "%d", client_id);
    sprintf(server_id_arg, "%d", server_id);

    char *argv[] = {(char*)kClientExecutable.c_str(),
        client_id_arg,
        server_id_arg,
        port_arg,
        server_port_arg,
        NULL
    };
    status = posix_spawn(&pid,
       (char*)kClientExecutable.c_str(),
       NULL,
       NULL,
       argv,
       environ);
    if (status == 0) {
        D(cout << "M  : Spawned client C" << client_id << " for server S"<<server_id<<endl;)
        all_pids_.push_back(pid);
    } else {
        D(cout << "M  : ERROR: Cannot spawn client C"
          << client_id << " - " << strerror(status) << endl);
        return false;
    }

    client_listen_ports_[client_id]=current_client_port_;
    current_client_port_++;

    // sleep for some time to make sure accept threads of client are running
    usleep(kGeneralSleep);
    usleep(kGeneralSleep);
    if (ConnectToClient(client_id)) {
        D(cout << "M  : Connected to client S" << client_id << endl;)
    } else {
        D(cout << "M  : ERROR: Cannot connect to client S" << client_id << endl;)
        return false;
    }
    return true;
}

/**
 * sends message to a server
 * @param server_id id of server to which message needs to be sent
 * @param message   message to be sent
 */
 void Master::SendMessageToServer(const int server_id, const string & message) {
    if (send(get_server_fd(server_id), message.c_str(), message.size(), 0) == -1) {
        D(cout << "M  : ERROR: Cannot send message to server S" << server_id << endl;)
    } else {
        D(cout << "M  : Message sent to server S" << server_id << ": " << message << endl;)
    }
}

void Master::ConstructMessage(const string& type, const string &body, string &message) {
    message = type + kInternalDelim + body + kMessageDelim;
}

/**
 * kills all processes. Only used at end.
 */
 void Master::KillAllProcesses() {
    for(auto &p: all_pids_){
        if (p != -1) {
            kill(p, SIGKILL);
        }
    }
}

/**
 * reads test commands from stdin
 */
 void Master::ReadTest() {
    string line;
    while (getline(std::cin, line)) {
        if(line[0]=='#')
            continue;

        std::istringstream iss(line);
        string keyword;
        iss >> keyword;
        if (keyword == kJoinServer) {
            int id;
            iss >> id;
            if(primary_id_==-1)
                SpawnServer(id);
            else
            {
                SpawnServer(id, true);
                set_primary_id(id);
            }
        }
        else if(keyword == kJoinClient){
            int cid, sid;
            iss>>cid>>sid;
            SpawnClient(cid, sid);
        }
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN);

    Master M;
    M.ReadTest();
    
    usleep(2000 * 1000);
    D(cout << "M  : GOODBYE. Killing everyone..." << endl;)
    M.KillAllProcesses();
    return 0;
}