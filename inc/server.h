#ifndef SERVER_H_
#define SERVER_H_

#include "vector"
#include "string"
#include "set"
#include "unordered_set"
#include "map"
#include "unordered_map"
#include "utils.h"
#include "data_classes.h"
using namespace std;

void* ReceiveMessagesFromClient(void* _rcv_thread_arg);
void* ReceiveFromMaster(void* _S);
void* ReceiveFromServersAndMisc(void* _S);
void* AcceptConnections(void* _S);
void* AntiEntropyP1(void* _S);
class Server {
public:
    Server(char**);
    void SendOrAskName(int fd);
    string CreateName();

    bool ConnectToServer(const int port);
    void ConnectToAllServers(char** argv);
    bool ConnectToMaster();

    void ReceiveFromServersAndMiscMode();

    void SendMessageToServer(const string&, const string &);
    void SendMessageToServerByFd(int , const string &);
    void SendMessageToMaster(const string & message);
    void SendMessageToClient(int fd, const string& message);
    void SendDoneToClient(int fd);
    void SendDoneToMaster();

    string GetNextServer(string);
    void SendAEP1Response(int fd);

    void InitializeLocks();
    void EstablishMasterCommunication();
    void InitialSetup(pthread_t&, pthread_t&, pthread_t&, pthread_t&);
    void HandleInitialServerHandshake(int port, int fd, const std::vector<string>& token);

    void RemoveFromMiscFd(int port);
    std::map<string, int> GetServerFdCopy();
    std::unordered_map<int, int> GetMiscFdCopy();
    void CreateFdSet(fd_set & fromset,
                     vector<int>& fds,
                     int& fd_max);

    void ConstructIAmMessage(const string&, const string &, const string& , string &);
    void ConstructMessage(const string&, const string& , string &);
    void ConstructPortMessage(string & message);
    void ConstructAEP2Message(string& msg, const int& r_csn, unordered_map<string, int>& r_clock);
    void ExtractAEP2Message(const string&, const string&);
    void ExecuteCommandsOnDatabase(IdTuple from);
    IdTuple RollBack(const string& committed_writes, const string& tent_writes);


    int get_pid();
    string get_name();
    int get_my_listen_port();
    int get_master_listen_port();
    int get_master_fd();
    int get_server_fd(const string&);
    string get_server_name(int);
    int get_misc_fd(int port);

    void set_name(const string & my_name);
    void set_server_fd(const string&, int);
    void set_client_fd(int port, int fd);
    void set_server_name(int, const string&);
    void set_master_fd(int);
    void set_my_listen_port(int port);
    void set_misc_fd(int port, int fd);

private:
    int pid_;// server's ID. DONT use this for server_listen_port/server_fd
    string name_;
    bool am_primary_;

    int master_listen_port_;
    int my_listen_port_;

    int master_fd_;

    //maps port to name
    std::unordered_map<int, string> server_name_;
    std::map<string, int> server_fd_;
    std::unordered_map<int, int> client_fd_; // hash of fd to port
    std::unordered_map<int, int> misc_fd_; // hash of port to fd for <port, fd> tuples whose origin is not known yet

    std::unordered_map<string, int> vclock_;
    int max_csn_;
    
    std::unordered_map<string, string> database_;
    std::map<IdTuple, Command> write_log_;
    std::map<IdTuple, Command> undo_log_;


};

#endif //SERVER_H_