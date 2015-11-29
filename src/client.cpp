#include "../inc/client.h"
#include "../inc/utils.h"
#include "../inc/constants.h"

#include "iostream"
#include "vector"
#include "string"
#include "cstring"
#include "fstream"
#include "sstream"
#include "unistd.h"
#include "signal.h"
#include "errno.h"
#include "assert.h"
#include "sys/socket.h"

using namespace std;

// #define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

Client::Client(char** argv) {
    int num_args = atoi(argv[1]);
    pid_ = atoi(argv[2]);
    connected_servers_.insert(atoi(argv[3]));
    master_listen_port_ = atoi(argv[4]);

    for (int i = 5; i < num_args; ++i) {
        server_fd_[atoi(argv[i])] = -1;
    }

    read_vector_.clear();
    write_vector_.clear();
}

int Client::get_pid() {
    return pid_;
}

int Client::get_master_fd() {
    return master_fd_;
}

int Client::get_master_listen_port() {
    return master_listen_port_;
}

int Client::get_server_fd(const int port) {
    if (server_fd_.find(port) != server_fd_.end())
        return server_fd_[port];
    else
        return -1;
}

void Client::set_pid(const int pid) {
    pid_ = pid;
}

void Client::set_master_fd(const int fd) {
    master_fd_ = fd;
}

void Client::set_server_fd(const int server_port, const int fd) {
    server_fd_[server_port] = fd;
}

void Client::RemoveServerFromSets(int server_port) {
    server_fd_.erase(server_port);
    connected_servers_.erase(server_port);
}

void Client::ConstructMessage(const string& type, const string &body, string &message) {
    message = type + kInternalDelim + body + kMessageDelim;
}

void Client::SendMessageToMaster(const string & message) {
    if (send(get_master_fd(), message.c_str(), message.size(), 0) == -1) {
        D(cout << "C" << get_pid() << " : ERROR: Cannot send message to M" << endl;)
    } else {
        D(cout << "C" << get_pid() << " : Message sent to M" << ": " << message << endl;)
    }
}

void Client::SendMessageToServer(const string & message, int server_port) {
    int fd = get_server_fd(server_port);
    if (send(fd, message.c_str(), message.size(), 0) == -1) {
        D(cout << "C" << get_pid() << " : ERROR: Cannot send message to S" << endl;)
        RemoveServerFromSets(server_port);
        close(fd);
    } else {
        D(cout << "C" << get_pid() << " : Message sent to S" << ": " << message << endl;)
    }
}

void Client::SendDoneToMaster() {
    string message;
    ConstructMessage(kDone, "", message);
    SendMessageToMaster(message);
}

/**
 * receives DONE message from server
 * @param server_port port on which DONE is expected
 */
void Client::WaitForDone(const int server_port) {
    char buf[kMaxDataSize];
    int num_bytes;
    int fd = get_server_fd(server_port);

    bool done = false;
    while (!done) {
        done = true;
        if ((num_bytes = recv(fd, buf, kMaxDataSize - 1, 0)) == -1) {
            D(cout << "C" << get_pid() << " : ERROR in receiving DONE from S, fd=" << fd << endl;)
            RemoveServerFromSets(server_port);
            close(fd);
        }
        else if (num_bytes == 0) {   //connection closed
            D(cout << "C" << get_pid() << " : ERROR Connection closed by S, fd=" << fd << endl;)
            RemoveServerFromSets(server_port);
            close(fd);
        }
        else {
            buf[num_bytes] = '\0';
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kDone) {
                    D(cout << "C" << get_pid() << " : DONE received" << endl;)
                } else {
                    done = false;
                    D(cout << "C" << get_pid() << " : ERROR Unexpected message received at fd="
                      << fd << ": " << msg << endl;)
                }
            }
        }
    }
}

/**
 * connects to all servers whose port numbers were given by the master
 * @param argv command line arguments passed by master
 */
void Client::ConnectToMultipleServers() {
    for (auto it = connected_servers_.begin(); it != connected_servers_.end();) {
        int server_port = *it;
        if (ConnectToServer(server_port)) {
            D(cout << "C" << get_pid() << " : Connected to S." << endl;)

            // send Iam Client message to server
            string message;
            ConstructIAmMessage(kIAm, kClient, message);
            SendMessageToServer(message, server_port);
            WaitForDone(server_port);
            it++;
        }
        else {
            D(cout << "C" << get_pid() << " : ERROR in connecting to S." << endl;)
            it = connected_servers_.erase(it);
        }
    }
}

void Client::UpdateReadVector(unordered_map<string, int>& rel_writes) {
    for (auto& e : rel_writes) { // e = <name,clockvalue>
        auto it = read_vector_.find(e.first);
        if (it == read_vector_.end()) {
            read_vector_[e.first] = e.second;
        }
        else {
            read_vector_[e.first] = max(read_vector_[e.first], e.second);
        }
    }
}

bool Client::CheckSessionGuaranteesWrites(unordered_map<string, int>& server_vc) {
    // cout<<"READ SET"<<endl;
    // for (auto& e : read_vector_) { // e = <name,clockvalue>
    //     cout<<e.first<<","<<e.second;
    // }

    // cout<<"----------"<<endl;
    // cout<<"WRITE SET"<<endl;
    // for (auto& e : write_vector_) { // e = <name,clockvalue>
    //     cout<<e.first<<","<<e.second;
    // }

    // Writes-Follow-Reads
    for (auto& e : read_vector_) { // e = <name,clockvalue>
        auto it = server_vc.find(e.first);
        if (it == server_vc.end()) {
            return false;
        }
        else {
            if (it->second < e.second)
                return false;
        }
    }

    // Monotonic-Writes
    for (auto& e : write_vector_) { // e = <name,clockvalue>
        auto it = server_vc.find(e.first);
        if (it == server_vc.end()) {
            return false;
        }
        else {
            if (it->second < e.second)
                return false;
        }
    }

    return true;
}

bool Client::CheckSessionGuaranteesReads(unordered_map<string, int>& server_vc) {
    // Monotonic-Writes
    for (auto& e : read_vector_) { // e = <name,clockvalue>
        auto it = server_vc.find(e.first);
        if (it == server_vc.end()) {
            return false;
        }
        else {
            if (it->second < e.second)
                return false;
        }
    }

    // Read-Your-Writes
    for (auto& e : write_vector_) { // e = <name,clockvalue>
        auto it = server_vc.find(e.first);
        if (it == server_vc.end()) {
            return false;
        }
        else {
            if (it->second < e.second)
                return false;
        }
    }

    return true;
}

/**
 * sends vector clock request to server
 * receives server's' vector clock
 * @param  server_port server's port
 * @return    server's vector clock
 */
unordered_map<string, int> Client::GetServerVectorClock(int server_port) {
    int fd = get_server_fd(server_port);
    string message = kServerVC + kInternalDelim + kClient + kInternalDelim + kMessageDelim;
    SendMessageToServer(message, server_port);

    char buf[kMaxDataSize];
    int num_bytes;
    unordered_map<string, int> ret;

    bool done = false;
    while (!done) {
        done = true;
        if ((num_bytes = recv(fd, buf, kMaxDataSize - 1, 0)) == -1) {
            D(cout << "C" << get_pid() << " : ERROR in receiving VC from S, fd=" << fd << endl;)
            RemoveServerFromSets(server_port);
            close(fd);
        }
        else if (num_bytes == 0) {   //connection closed
            D(cout << "C" << get_pid() << " : ERROR Connection closed by S, fd=" << fd << endl;)
            RemoveServerFromSets(server_port);
            close(fd);
        }
        else {
            buf[num_bytes] = '\0';
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kServerVC) {
                    //SERVERVC-Port,Ts;Port,Ts;Port,Ts
                    D(assert(token.size() == 2);)
                    D(cout << "C" << get_pid() << " : VC received: " << token[1] << endl;)
                    ret = StringToUnorderedMap(token[1]);
                } else {
                    done = false;
                    D(cout << "C" << get_pid() << " : ERROR Unexpected message received at fd="
                      << fd << ": " << msg << endl;)
                }
            }
        }
    }
    return ret;
}

void Client::GetWriteID(int server_port) {
    int fd = get_server_fd(server_port);
    char buf[kMaxDataSize];
    int num_bytes;

    bool done = false;
    while (!done) {
        done = true;
        if ((num_bytes = recv(fd, buf, kMaxDataSize - 1, 0)) == -1) {
            D(cout << "C" << get_pid() << " : ERROR in receiving WRITEID from S, fd=" << fd << endl;)
            RemoveServerFromSets(server_port);
            close(fd);
        }
        else if (num_bytes == 0) {   //connection closed
            D(cout << "C" << get_pid() << " : ERROR Connection closed by S, fd=" << fd << endl;)
            RemoveServerFromSets(server_port);
            close(fd);
        }
        else {
            buf[num_bytes] = '\0';
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kWriteID) {
                    //WRITEID-name,Ts (delim=kName=!)
                    D(assert(token.size() == 2);)

                    std::vector<string> entry = split(string(token[1]), kComma[0]);

                    D(assert(entry.size() == 2));
                    string name = entry[0];
                    int timestamp = stoi(entry[1]);
                    D(cout << "C" << get_pid() << " : WRITEID received: " << token[1] << endl;)
                    write_vector_[name] = timestamp;
                } else {
                    done = false;
                    D(cout << "C" << get_pid() << " : ERROR Unexpected message received at fd="
                      << fd << ": " << msg << endl;)
                }
            }
        }
    }
}

string Client::GetResultAndRelWrites(int server_port) {
    int fd = get_server_fd(server_port);
    char buf[kMaxDataSize];
    int num_bytes;
    string url;

    int count = 0;
    while (count != 2) {
        if ((num_bytes = recv(fd, buf, kMaxDataSize - 1, 0)) == -1) {
            // cout << errno << strerror(errno) << endl;
            D(cout << "C" << get_pid() 
                << " : ERROR in receiving Result/Rel Writes from S, fd=" << fd << endl;)
            count = 2;
            RemoveServerFromSets(server_port);
            close(fd);
        }
        else if (num_bytes == 0) {   //connection closed
            D(cout << "C" << get_pid() << " : ERROR Connection closed by S, fd=" << fd << endl;)
            count = 2;
            RemoveServerFromSets(server_port);
            close(fd);
        }
        else {
            buf[num_bytes] = '\0';
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kUrl) {
                    //URL-url
                    D(assert(token.size() == 2));
                    D(cout << "C" << get_pid() << " : URL received: " << token[1] << endl;)
                    url = token[1];
                    count++;
                } else if (token[0] == kRelWrites) {
                    //RELWRITES-name,Ts;name,Ts;name,ts
                    D(assert(token.size() == 2));
                    D(cout << "C" << get_pid() << " : RELWRITES received: " << token[1] << endl;)
                    unordered_map<string, int> rel_writes = StringToUnorderedMap(token[1]);
                    UpdateReadVector(rel_writes);
                    count++;
                } else {
                    D(cout << "C" << get_pid() << " : ERROR Unexpected message received at fd="
                      << fd << ": " << msg << endl;)
                }
            }
        }
    }
    return url;
}

void Client::HandleWriteRequest(string type, string song_name, string url) {
    string message;

    if (type == kPut)
        message = kPut + kInternalDelim +
                  song_name + kInternalDelim +
                  url + kInternalDelim + kMessageDelim;
    else if (type == kDelete)
        message = kDelete + kInternalDelim +
                  song_name + kInternalDelim + kMessageDelim;

    for (auto server_port : connected_servers_) {
        unordered_map<string, int> server_vc =
            GetServerVectorClock(server_port);
        if (CheckSessionGuaranteesWrites(server_vc)) {
            SendMessageToServer(message, server_port);
            GetWriteID(server_port);
            break;
        } else {
            // do nothing. Just drop the write
        }
    }
}

/**
 * handles get request
 * @param  song_name name of song
 * @return           url of requested song
 */
string Client::HandleReadRequest(string song_name) {
    string message = kGet + kInternalDelim +
                     song_name + kInternalDelim + kMessageDelim;

    for (auto server_port : connected_servers_) {
        unordered_map<string, int> server_vc =
            GetServerVectorClock(server_port);
        if (CheckSessionGuaranteesReads(server_vc)) {
            SendMessageToServer(message, server_port);
            return kUrl + kInternalDelim +
                   GetResultAndRelWrites(server_port) + kInternalDelim + kMessageDelim;
        } else {
            return kUrl + kInternalDelim + kErrDep + kInternalDelim + kMessageDelim;
        }
    }
    return "";
}

void Client::BreakConnectionWithServer(int port) {
    if(connected_servers_.find(port) != connected_servers_.end() &&
        server_fd_.find(port) != server_fd_.end()) {
        
        connected_servers_.erase(port);
        close(server_fd_[port]);
        server_fd_.erase(port);
    } else {
        D(cout << "C" << get_pid() 
            << " : ERROR Invalid port for BreakConnection" << port << endl;)
    }
}

void Client::RestoreConnectionWithServer(int port) {
    if (ConnectToServer(port)) {
        D(cout << "C" << get_pid() << " : Connected to S." << endl;)

        // send Iam Client message to server
        string message;
        ConstructIAmMessage(kIAm, kClient, message);
        SendMessageToServer(message, port);
        WaitForDone(port);
        connected_servers_.insert(port);
    } else {
        D(cout << "C" << get_pid() << " : ERROR in connecting to S." << endl;)
    }
}

void* ReceiveFromMaster(void* _C) {
    Client* C = (Client*)_C;
    char buf[kMaxDataSize];
    int num_bytes;

    while (true) {  // always listen to messages from the master
        num_bytes = recv(C->get_master_fd(), buf, kMaxDataSize - 1, 0);
        if (num_bytes == -1) {
            D(cout << "C" << C->get_pid() << " : ERROR in receiving message from M" << endl;)
        } else if (num_bytes == 0) {    // connection closed by master
            D(cout << "C" << C->get_pid() << " : Connection closed by M" << endl;)
        } else {
            buf[num_bytes] = '\0';
            // extract multiple messages from the received buf
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kPut) {
                    //PUT-SONG_NAME-URL
                    D(assert(token.size() == 3));
                    C->HandleWriteRequest(kPut, token[1], token[2]);
                    C->SendDoneToMaster();
                } else if (token[0] == kDelete) {
                    //DELETE-SONG_NAME
                    D(assert(token.size() == 2));
                    C->HandleWriteRequest(kDelete, token[1]);
                    C->SendDoneToMaster();
                } else if (token[0] == kGet) {
                    //GET-SONG_NAME
                    D(assert(token.size() == 2));
                    string url = C->HandleReadRequest(token[1]);
                    C->SendMessageToMaster(url);
                } else if(token[0] == kBreakConnection) {
                    //BREAK-port
                    D(assert(token.size() == 2));
                    C->BreakConnectionWithServer(stoi(token[1]));
                    C->SendDoneToMaster();
                } else if(token[0] == kRestoreConnection) {
                    //RESTORE-port
                    D(assert(token.size() == 2));
                    C->RestoreConnectionWithServer(stoi(token[1]));
                    C->SendDoneToMaster();
                }else {    //other messages
                    D(cout << "C" << C->get_pid()
                      << " : ERROR Unexpected message received from M: "
                      << msg << endl;)
                }
            }
        }
    }
    return NULL;
}

void Client::ConstructIAmMessage(const string & type,
                                 const string & process_type,
                                 string & message) {
    message = type + kInternalDelim +
              process_type + kInternalDelim +
              to_string(get_pid()) + kInternalDelim + kMessageDelim;
}

/**
 * connects to master
 */
void Client::EstablishMasterCommunication() {
    if (!ConnectToMaster()) {
        D(cout << "C" << get_pid() << " : ERROR in connecting to M" << endl;)
    }

}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    Client C(argv);
    C.EstablishMasterCommunication();

    pthread_t receive_from_master_thread;
    CreateThread(ReceiveFromMaster, (void*)&C, receive_from_master_thread);

    C.ConnectToMultipleServers();

    // send Iam Client message to master
    string message;
    C.ConstructIAmMessage(kIAm, kClient, message);
    C.SendMessageToMaster(message);
    // don't send done. Master uses IamClient as ACK.
    // DONE might cause issue with two threads receiving at same fd
    // C.SendDoneToMaster();


    void *status;
    pthread_join(receive_from_master_thread, &status);
    return 0;
}