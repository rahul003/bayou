#ifndef MASTER_H_
#define MASTER_H_

#include "vector"
#include "string"
#include "fstream"
#include "iostream"
#include "set"
#include "map"
#include "unordered_map"
#include "data_classes.h"
using namespace std;

void* AcceptConnections(void* _M);

class Master {
public:
    Master();

    void ReadTest();
    bool SpawnServer(const int server_id, bool isPrimary = false);
    void CrashServer(const int);
    bool SpawnClient(const int c_id, const int s_id);
    void KillAllProcesses();
    int SendToAllServers(const string& msg);
    void WaitForAll(const int num);
    int SendLogRequest(int id);

    int SendMessageToServer(const int server_id, const string & message);
    void SendMessageToClient(const int client_id, const string & message);
    void SendRetireMessage(int id);
    void ConstructMessage(const string& type, const string &body, string &message);
    void WaitForPortMessage(const int fd);
    void WaitForDone(const int fd);
    void WaitForLogResponse(const int);
    void ProcessAndPrintLog(int id, const string& log);
    string WaitForVC(int sid);

    void SendChangeConnectionServer(const string& type, int id, const string& name, int port=-1);
    void SendChangeConnectionClient(string type, int id, int port);
    void StabilizeMode();

    void SendPutToClient(int client_id,
                         const string& song_name,
                         const string& url);
    void GetUrlFromClient(const int fd);
    void SendGetToClient(int client_id,
                         const string& song_name);
    void SendDeleteToClient(int client_id,
                            const string& song_name);

    void SetCloseExecFlag(const int fd);

    bool is_client_id(int id);
    bool is_server_id(int id);

    int get_master_port();
    int get_master_fd();
    int get_server_listen_port(const int server_id);
    int get_num_servers();
    int get_server_fd(const int server_id);
    int get_client_fd(const int client_id);
    int get_primary_id();
    string get_server_name(int server_id);

    void set_server_fd(const int server_id, const int fd);
    void set_client_fd(const int client_id, const int fd);
    void set_primary_id(const int primary_id);
    void set_server_listen_port(const int server_id, const int port_num);
    void set_server_name(int server_id, const string& name);

private:
    //to kill when exiting
    std::unordered_map<int, pid_t> all_pids_;

    //id to fd
    std::unordered_map<int, int> server_fd_;
    std::unordered_map<int, int> client_fd_;
    int primary_id_;

    // id to name
    std::unordered_map<int, string> server_name_;

    int master_port_;   // port used by master for communication
    int master_fd_;

    Graph servers_;

    int file_num_;
    //map from id given in test to port
    std::unordered_map<int, int> server_listen_port_;

};


#endif //MASTER_H_