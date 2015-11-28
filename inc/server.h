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

typedef enum {
    UNSET, SET, DONE
} RetireStatus;

void* ReceiveFromAll(void* _S);
void* AcceptConnections(void* _S);
void* AntiEntropyP1(void* _S);
class Server {
public:
    Server(char**);
    void SendOrAskName(int fd);
    string CreateName();
    void CreatePortToClockMap(unordered_map<string, int>& port_to_clock);
    void CommitTentativeWrites();

    bool ConnectToServer(const int port);
    void ConnectToAllServers(char** argv);
    bool ConnectToMaster();
    // int CompleteV(string);
    void ReceiveFromAllMode();
    void RemoveServer(int port);

    void SendRetiringMsgToServer(int);
    void SendMessageToServer(const string&, const string &);
    void SendMessageToServerByFd(int , const string &);
    void SendMessageToMaster(const string & message);
    void SendMessageToClient(int fd, const string& message);
    void SendDoneToClient(int fd);
    void SendDoneToMaster();
    void SendWriteIdToClient(int);
    void SendAEP1Response(int fd);
    void SendAntiEntropyP2(vector<string>& token, int fd);

    void AddWrite(string type, string song, string url);
    string GetWriteLogAsString();
    string GetNextServer(string);
    string GetServerForRetireMessage();
    string GetRelevantWrites(string song);


    void InitializeLocks();
    void EstablishMasterCommunication();
    void InitialSetup(pthread_t&, pthread_t&, pthread_t&);
    void HandleInitialServerHandshake(int port, int fd, const std::vector<string>& token);

    void RemoveFromMiscFd(int fd);
    std::map<string, int> GetServerFdCopy();
    std::unordered_set<int> GetMiscFdCopy();
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
    void CloseClientConnections();
    void CloseServerAndMiscConnections();
    void AddRetireWrite();
    void WaitForAck(int);


    int get_pid();
    string get_name();
    bool get_pause();
    int get_my_listen_port();
    int get_master_listen_port();
    int get_master_fd();
    int get_server_fd(const string&);
    string get_server_name(int);
    void set_retiring();
    int get_misc_fd(int port);
    RetireStatus get_retiring();
    int get_server_fd_peerport_map(int fd);

    void set_retiring(RetireStatus);
    void set_pause(bool);
    void set_name(const string & my_name);
    void set_server_fd(const string&, int);
    void set_client_fd(int port, int fd);
    void set_server_name(int, const string&);
    void set_master_fd(int);
    void set_my_listen_port(int port);
    void set_misc_fd(int fd);
    void set_server_fd_peerport_map(int fd, int peer_port);

private:
    int pid_;// server's ID. DONT use this for server_listen_port/server_fd
    string name_;
    bool am_primary_;
    RetireStatus retiring_;
    bool pause_;
    int master_listen_port_;
    int my_listen_port_;

    int master_fd_;

    std::unordered_map<int, string> server_name_;    //maps port to name
    std::map<string, int> server_fd_;
    std::unordered_map<int, int> server_fd_peerport_map_; //hash of fd to peer port
    std::unordered_map<int, int> client_fd_; // hash of port to fd
    std::unordered_set<int> misc_fd_; // set of fds whose origin is not known yet

    std::unordered_map<string, int> vclock_;
    int max_csn_;

    std::unordered_map<string, string> database_;
    std::map<IdTuple, Command> write_log_;
    std::map<IdTuple, Command> undo_log_;


};

#endif //SERVER_H_