#include "../inc/utils.h"
#include "../inc/constants.h"

#include "string"
#include "vector"
#include "iostream"
#include "sstream"
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>

using namespace std;
#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    if (s == "")
        return elems;

    split(s, delim, elems);

    return elems;
}

/**
 * creates a new thread
 * @param  f function pointer to be passed to the new thread
 * @param  arg arguments for the function f passed to the thread
 * @param  thread thread identifier for new thread
 */
void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread) {
    if (pthread_create(&thread, NULL, f, arg)) {
        D(cout << "U " << ": ERROR: Unable to create thread" << endl;)
        pthread_exit(NULL);
    }
}

int GetPortFromFd(int fd) {

    socklen_t len;
    struct sockaddr_storage addr;
    int port;

    len = sizeof addr;
    if (getsockname(fd, (struct sockaddr*)&addr, &len) == -1) {
        perror("getsockname");
        return -1;
    } else {
        // deal with both IPv4 and IPv6:
        if (addr.ss_family == AF_INET) {
            struct sockaddr_in *s = (struct sockaddr_in *)&addr;
            port = ntohs(s->sin_port);
        } else { // AF_INET6
            struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
            port = ntohs(s->sin6_port);
        }
        return port;
    }
}

int GetPeerPortFromFd(int fd) {
    socklen_t len;
    struct sockaddr_storage addr;
    // char ipstr[INET6_ADDRSTRLEN];
    int port;

    len = sizeof addr;
    if (getpeername(fd, (struct sockaddr*)&addr, &len) == -1) {
        perror("getsockname");
        return -1;
    } else {
        // deal with both IPv4 and IPv6:
        if (addr.ss_family == AF_INET) {
            struct sockaddr_in *s = (struct sockaddr_in *)&addr;
            port = ntohs(s->sin_port);
            // inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
        } else { // AF_INET6
            struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
            port = ntohs(s->sin6_port);
            // inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
        }
        return port;
    }
}

unordered_map<int, int> StringToUnorderedMap(string str) {
    // Port,Ts;Port,Ts;Port,ts
    unordered_map<int, int> ans;
    std::vector<string> tuple = split(str, kSemiColon[0]);
    for (const auto t : tuple) {
        std::vector<string> entry = split(t, kComma[0]);
        assert(entry.size() == 2);
        ans[stoi(entry[0])] = stoi(entry[1]);
    }
    return ans;
}