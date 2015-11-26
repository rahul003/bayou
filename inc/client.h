#ifndef CLIENT_H_
#define CLIENT_H_

#include "vector"
#include "string"
#include "map"
#include "unordered_set"
#include "unordered_map"
using namespace std;

void* ReceiveMessagesFromMaster(void* _C);

class Client {
public:
    Client(char**);

    void ConstructMessage(const string& type, const string &body, string &message);
    void SendMessageToMaster(const string & message);
    void SendDoneToMaster();
    void ConnectToMultipleServers();
    void EstablishMasterCommunication();
    void ConstructIAmMessage(const string& type,
                             const string &process_type,
                             string &message);


    bool ConnectToMaster();
    bool ConnectToServer(const int port);

    int get_pid();
    int get_master_fd();
    int get_master_listen_port();
    int get_server_fd(const int port);


    void set_pid(const int pid);
    void set_master_fd(const int fd);
    void set_server_fd(const int server_port, const int fd);

private:
    int pid_;   // client's ID

    int master_fd_;

    int master_listen_port_;

    std::unordered_map<int, int> server_fd_; // hash of port to fd
    std::unordered_set<int> connected_servers_; // set of ports of connected servers
};

#endif //CLIENT_H_