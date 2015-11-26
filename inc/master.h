#ifndef MASTER_H_
#define MASTER_H_

#include "vector"
#include "string"
#include "fstream"
#include "iostream"
#include "unordered_map"
using namespace std;

void* AcceptConnections(void* _M);

class Master {
public:
    Master();

    void ReadTest();
    bool SpawnServer(const int server_id, bool isPrimary = false);
    bool SpawnClient(const int c_id, const int s_id);
    void KillAllProcesses();
    void SendMessageToServer(const int server_id, const string & message);
    void ConstructMessage(const string& type, const string &body, string &message);
    void WaitForPortMessage(const int fd);
    void WaitForDone(const int fd);

    void SetCloseExecFlag(const int fd);

    int get_master_port();
    int get_master_fd();
    int get_server_listen_port(const int server_id);
    int get_num_servers();
    int get_server_fd(const int server_id);
    int get_client_fd(const int client_id);
    int get_primary_id();

    void set_server_fd(const int server_id, const int fd);
    void set_client_fd(const int client_id, const int fd);
    void set_primary_id(const int primary_id);
    void set_server_listen_port(const int server_id, const int port_num);

private:
    //to kill when exiting
    std::vector<pid_t> all_pids_;

    //coz dynamic we could just use below vector size to know how many
    std::unordered_map<int, int> server_fd_;
    std::unordered_map<int, int> client_fd_;
    int primary_id_;

    int current_server_port_;
    int master_port_;   // port used by master for communication
    int master_fd_;

    //map from id given in test to port
    std::unordered_map<int, int> server_listen_port_;

};
#endif //MASTER_H_