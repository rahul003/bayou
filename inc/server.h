#ifndef SERVER_H_
#define SERVER_H_

#include "vector"
#include "string"
#include "set"
#include "unordered_set"
#include "map"
#include "unordered_map"
#include "utils.h"
using namespace std;

void* ReceiveMessagesFromClient(void* _rcv_thread_arg);
void* ReceiveMessagesFromMaster(void* _S );
void* AcceptConnections(void* _S);

class Server {
public:
    Server(char**);
    void WaitForNameAndSetFd(int port, int fd);
    void SendOrAskName(int fd);
    string CreateName();

    bool ConnectToServer(const int port);
    void ConnectToAllServers(char** argv);
    bool ConnectToMaster();

    void SendMessageToServer(const string, const string &);
    void SendInitialMessageToServer(int , const string &);
    void SendMessageToMaster(const string & message);
    void SendDoneToMaster();
    void EstablishMasterCommunication();
    void InitialSetup(pthread_t&, pthread_t&);

    void ConstructIAmMessage(const string&, const string &, const string& , string &);
    void ConstructMessage(const string&, const string& , string &);
    void ConstructPortMessage(string& message);

    int get_pid();
    string get_name();
    int get_my_listen_port();
    int get_master_listen_port();
    int get_master_fd();
    int get_server_fd(string);
    int get_client_fd(string);
    string get_server_name(int);
    string get_client_name(int);

    void set_server_fd(string, int);
    void set_client_fd(string, int);
    void set_server_name(int, string);
    void set_client_name(int, string);
    void set_master_fd(int);
    void set_my_listen_port(int port);

private:
    int pid_;// server's ID. DONT use this for server_listen_port/server_fd
    string name_;
    bool am_primary_;

    int master_listen_port_;
    int my_listen_port_;

    int master_fd_;

    //maps port to name
    std::unordered_map<int, string> server_name_;
    std::unordered_map<int, string> client_name_;
    std::unordered_map<string, int> server_fd_;
    std::unordered_map<string, int> client_fd_;

};

#endif //SERVER_H_