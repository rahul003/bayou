#include "../inc/server.h"
#include "../inc/constants.h"
#include "../inc/utils.h"

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

Server::Server(char** argv)
{
    pid_ = atoi(argv[2]);
    am_primary_ = (atoi(argv[3]) != 0);
    master_listen_port_ = atoi(argv[4]);
    my_listen_port_ = -1;
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
    int fd;
    pthread_mutex_lock(&server_lock);
    fd = server_fd_[name];
    pthread_mutex_unlock(&server_lock);
    return fd;
}

int Server::get_misc_fd(int port) {
    int fd;
    pthread_mutex_lock(&misc_fd_lock);
    fd = misc_fd_[port];
    pthread_mutex_unlock(&misc_fd_lock);
    return fd;
}

string Server::get_server_name(int port) {
    string name;
    pthread_mutex_lock(&server_lock);
    name = server_name_[port];
    pthread_mutex_unlock(&server_lock);
    return name;
}

int Server::get_master_fd() {
    return master_fd_;
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

void Server::set_server_fd(const string& name, int fd) {
    //we should never remove serverfd/port because can reconnect
    //TODO: why?
    pthread_mutex_lock(&server_lock);
    server_fd_[name] = fd;
    pthread_mutex_unlock(&server_lock);
}

void Server::set_server_name(int port, const string& name) {
    pthread_mutex_lock(&server_lock);
    server_name_[port] = name;
    pthread_mutex_unlock(&server_lock);
}

std::unordered_map<string, int> Server::GetServerFdCopy() {
    pthread_mutex_lock(&server_lock);
    std::unordered_map<string, int> server_fd_copy(server_fd_);
    pthread_mutex_unlock(&server_lock);
    return server_fd_copy;
}

std::unordered_map<int, int> Server::GetMiscFdCopy() {
    pthread_mutex_lock(&misc_fd_lock);
    std::unordered_map<int, int> misc_fd_copy(misc_fd_);
    pthread_mutex_unlock(&misc_fd_lock);
    return misc_fd_copy;
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

                // if (token[0] == ) {

                // } else {    //other messages
                //     D(cout << "S" << S->get_pid()
                //       << " : ERROR Unexpected message received: " << msg << endl;)
                // }
            }
        }
    }
    return NULL;
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
    unordered_map<string, int> server_fd_copy = GetServerFdCopy();
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

void* ReceiveFromServersAndMisc(void* _S) {
    Server* S = (Server*)_S;
    char buf[kMaxDataSize];
    int num_bytes;

    fd_set fromset;
    std::vector<int> fds;
    int fd_max;

    while (true) {
        S->CreateFdSet(fromset, fds, fd_max);

        if (fd_max == INT_MIN) {
            usleep(kBusyWaitSleep);
            continue;
        }

        struct timeval timeout = kSelectTimeoutTimeval;
        int rv = select(fd_max + 1, &fromset, NULL, NULL, &timeout);
        if (rv == -1) { //error in select
            D(cout << "S" << S->get_pid()
              << ": ERROR in select() errno=" << errno
              << " fdmax=" << fd_max << endl;)
        } else if (rv == 0) {
            // D(cout << "S" << S->get_pid() << ": Select timeout" << endl;)
        } else {
            for (int i = 0; i < fds.size(); i++) {
                if (FD_ISSET(fds[i], &fromset)) { // we got one!!
                    if ((num_bytes = recv(fds[i], buf, kMaxDataSize - 1, 0)) == -1) {
                        D(cout << "S" << S->get_pid()
                          << ": ERROR in receiving" << endl;)

                        //TODO: What to do here?
                        // ResetFD(fds[i], primary_id);

                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "S" << S->get_pid() << ": Connection closed" << endl;)

                        //TODO: What to do here?
                        // ResetFD(fds[i], primary_id);

                    } else {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message) {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);

                            // if (token[0] == ) {

                            // } else {    //other messages
                            //     D(cout << "S" << S->get_pid()
                            //       << " : ERROR Unexpected message received: " << msg << endl;)
                            // }
                        }
                    }
                }
            }
        }
    }
    return NULL;
}

void Server::WaitForNameAndSetFd(int port, int fd)
{
    SendOrAskName(fd);

    char buf[kMaxDataSize];

    int num_wait;
    //if i dont have name, i wait for other party name as well as mine
    if (name_.empty())
        num_wait = 2;
    else
        num_wait = 1;
    while (num_wait) {
        int num_bytes = recv(fd, buf, kMaxDataSize, MSG_PEEK);
        if (num_bytes  == -1) {
            D(cout << "S" << get_pid() << " : ERROR in receiving name from someone" << endl;)
        }
        else if (num_bytes == 0)
        {   //connection closed
            D(cout << "S" << get_pid() << " : ERROR Connection closed by someone while waiting for name" << endl;)
        }
        else
        {
            buf[num_bytes] = '\0';
            std::vector<string> messages = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : messages)
            {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kIAm)
                {
                    num_wait--;
                    //not new process sends message of foll type
                    assert(token.size() > 2);

                    if (token[1] == kServer)
                    {
                        //kName kServer (name)
                        set_server_fd(token[2], fd);
                        set_server_name(port, token[2]);
                    }
                    else if (token[1] == kClient)
                    {
                        //kName kClient (name)
                        // set_client_fd(token[2], fd);
                        // set_client_name(port, token[2]);
                    }
                }
                else if (token[0] == kNewServer)
                {
                    num_wait--;
                    string name = CreateName();
                    set_server_fd(name, fd);
                    set_server_name(port, name);
                    string msg;
                    ConstructMessage(kYouAre, name, msg);
                    SendMessageToServer(name, msg);
                }
                else if (token[0] == kYouAre)
                {
                    num_wait--;
                    name_ = token[1];
                }

            }
        }
    }
}
string Server::CreateName() {


}

void Server::SendOrAskName(int fd) {
    if (!get_name().empty())
    {
        string msg;
        ConstructIAmMessage(kIAm, kServer, get_name(), msg);
        SendInitialMessageToServer(fd, msg);
    }
    else {
        //we can remove this if we dont want to send my name.
        //then our unique identifier for a server has to be the port
        string msg;
        ConstructMessage(kNewServer, "", msg);
        SendInitialMessageToServer(fd, msg);
    }
}

void Server::SendInitialMessageToServer(int fd, const string & message) {
    if (send(fd, message.c_str(), message.size(), 0) == -1) {
        D(cout << "S" << get_pid() << " : ERROR: Cannot send message to S" << endl;)
    } else {
        D(cout << "S" << get_pid() << " : Message sent to S: " << message << endl;)
    }
}

void Server::SendMessageToServer(const string name, const string & message) {
    if (send(get_server_fd(name), message.c_str(), message.size(), 0) == -1) {
        D(cout << "S" << get_pid() << " : ERROR: Cannot send message to S" << name << endl;)
    } else {
        D(cout << "S" << get_pid() << " : Message sent to S" << name << ": " << message << endl;)
    }
}

void Server::SendMessageToMaster(const string & message) {
    if (send(get_master_fd(), message.c_str(), message.size(), 0) == -1) {
        D(cout << "S" << get_pid() << " : ERROR: Cannot send message to M" << endl;)
    } else {
        D(cout << "S" << get_pid() << " : Message sent to M" << ": " << message << endl;)
    }
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
            D(cout << "S" << get_pid()
              << " : Connected to server "
              << get_server_name(atoi(argv[i])) << endl;)
        }
        else {
            D(cout << "S" << get_pid()
              << " : ERROR in connecting to server "
              << get_server_name(atoi(argv[i])) << endl;)
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
                          pthread_t& receive_from_servers_thread) {
    InitializeLocks();
    CreateThread(AcceptConnections, (void*)this, accept_connections_thread);

    while (get_my_listen_port() == -1) {
        usleep(kBusyWaitSleep);
    }
    EstablishMasterCommunication();

    CreateThread(ReceiveFromMaster, (void*)this, receive_from_master_thread);
    CreateThread(ReceiveFromServersAndMisc, (void*)this, receive_from_servers_thread);
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

    Server S(argv);
    S.InitialSetup(accept_connections_thread,
                   receive_from_master_thread,
                   receive_from_servers_thread);

    S.ConnectToAllServers(argv);
    S.SendDoneToMaster();

    void* status;
    pthread_join(accept_connections_thread, &status);
    pthread_join(receive_from_master_thread, &status);
    pthread_join(receive_from_servers_thread, &status);
    return 0;
}