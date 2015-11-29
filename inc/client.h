#ifndef CLIENT_H_
#define CLIENT_H_

#include "vector"
#include "string"
#include "map"
#include "unordered_set"
#include "unordered_map"
using namespace std;

void* ReceiveFromMaster(void* _C);

class Client {
public:
    Client(char**);

    void SendMessageToMaster(const string & message);
    void SendMessageToServer(const string & message, int server_port);
    void SendDoneToMaster();

    void ConstructMessage(const string& type, const string &body, string &message);
    void ConstructIAmMessage(const string& type,
                             const string &process_type,
                             string &message);

    void EstablishMasterCommunication();
    void BreakConnectionWithServer(int port);
    void RestoreConnectionWithServer(int port);
    void WaitForDone(const int server_port);

    void HandleWriteRequest(string type, string song_name, string url = "");
    string HandleReadRequest(string song_name);
    void UpdateReadVector(unordered_map<string, int>& rel_writes);
    bool CheckSessionGuaranteesWrites(unordered_map<string, int>& server_vc);
    bool CheckSessionGuaranteesReads(unordered_map<string, int>& server_vc);
    unordered_map<string, int> GetServerVectorClock(int server_port);
    void GetWriteID(int server_port);
    string GetResultAndRelWrites(int server_port);

    bool ConnectToMaster();
    void ConnectToMultipleServers();
    bool ConnectToServer(const int port);
    void RemoveServerFromSets(int server_port);

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

    std::unordered_map<string, int> read_vector_;  // hash of servername to clockvalue
    std::unordered_map<string, int> write_vector_;
};

#endif //CLIENT_H_