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
#include "sys/socket.h"

using namespace std;

#define DEBUG

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

void Client::SendMessageToServer(const string & message, int fd) {
    if (send(fd, message.c_str(), message.size(), 0) == -1) {
        D(cout << "C" << get_pid() << " : ERROR: Cannot send message to S" << endl;)
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
 * receives DONE message from someone
 * @param fd fd on which DONE is expected
 */
void Client::WaitForDone(const int fd) {
    char buf[kMaxDataSize];
    int num_bytes;
    errno = 0;
    if ((num_bytes = recv(fd, buf, kMaxDataSize - 1, 0)) == -1) {
        cout << errno << strerror(errno) << endl;
        D(cout << "C" << get_pid() << " : ERROR in receiving DONE from someone, fd=" << fd << endl;)
    }
    else if (num_bytes == 0) {   //connection closed
        D(cout << "C" << get_pid() << " : ERROR Connection closed by someone, fd=" << fd << endl;)
    }
    else {
        buf[num_bytes] = '\0';
        std::vector<string> message = split(string(buf), kMessageDelim[0]);
        for (const auto &msg : message) {
            std::vector<string> token = split(string(msg), kInternalDelim[0]);
            if (token[0] == kDone) {
                D(cout << "M  : DONE received" << endl;)
            } else {
                D(cout << "M  : Unexpected message received at fd="
                  << fd << ": " << msg << endl;)
            }
        }
    }
}

/**
 * connects to all servers whose port numbers were given by the master
 * @param argv command line arguments passed by master
 */
void Client::ConnectToMultipleServers() {
    for (auto& server_port : connected_servers_) {
        if (ConnectToServer(server_port)) {
            D(cout << "C" << get_pid() << " : Connected to S." << endl;)
        }
        else {
            D(cout << "C" << get_pid() << " : ERROR in connecting to S." << endl;)
        }

        // send Iam Client message to server
        string message;
        ConstructIAmMessage(kIAm, kClient, message);
        SendMessageToServer(message, get_server_fd(server_port));
        WaitForDone(get_server_fd(server_port));
    }
}

void* ReceiveFromMaster(void* _C) {
    Client* C = (Client*)_C;
    char buf[kMaxDataSize];
    int num_bytes;
    // D(cout << "C" << C->get_pid() << " : Receiving from M" << endl;)
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

                // if (token[0] == ) {

                // } else {    //other messages
                //     D(cout << "C" << C->get_pid()
                //       << " : ERROR Unexpected message received from M: "
                //       << message << endl;)
                // }
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
              to_string(get_pid()) + kMessageDelim;
}

/**
 * connects to master
 */
void Client::EstablishMasterCommunication() {
    if (!ConnectToMaster()) {
        D(cout << "C" << get_pid() << " : ERROR in connecting to M" << endl;)
    }
    // send Iam Client message to master
    string message;
    ConstructIAmMessage(kIAm, kClient, message);
    SendMessageToMaster(message);
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    Client C(argv);
    C.EstablishMasterCommunication();

    pthread_t receive_from_master_thread;
    CreateThread(ReceiveFromMaster, (void*)&C, receive_from_master_thread);

    C.ConnectToMultipleServers();
    C.SendDoneToMaster();


    void *status;
    pthread_join(receive_from_master_thread, &status);
    return 0;
}