#include "../inc/client.h"
#include "../inc/constants.h"

#include "iostream"
#include "unistd.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
using namespace std;

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

/**
 * Connects to master
 * @return  true if connection was successfull
 */
bool Client::ConnectToMaster() {
    int numbytes, sockfd;
    struct addrinfo hints, *l, *servinfo;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, std::to_string(get_master_listen_port()).c_str(),
                          &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return false;
    }
    // loop through all the results and connect to the first we can
    for (l = servinfo; l != NULL; l = l->ai_next)
    {
        if ((sockfd = socket(l->ai_family, l->ai_socktype,
                             l->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        errno = 0;
        if (connect(sockfd, l->ai_addr, l->ai_addrlen) == -1) {
            close(sockfd);
            // cout << strerror(errno) << endl;
            continue;
        }

        break;
    }
    if (l == NULL) {
        return false;
    }
    freeaddrinfo(servinfo); // all done with this structure
    set_master_fd(sockfd);

    return true;
}

/**
 * Connects to a server port
 * @param port port of server whose server to connect to
 * @return  true if connection was successfull
 */
bool Client::ConnectToServer(const int port) {

    int numbytes, sockfd;
    struct addrinfo hints, *l, *servinfo;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, std::to_string(port).c_str(),
                          &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return false;
    }
    // loop through all the results and connect to the first we can
    for (l = servinfo; l != NULL; l = l->ai_next)
    {
        if ((sockfd = socket(l->ai_family, l->ai_socktype,
                             l->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        // if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&kReceiveTimeoutTimeval,
        //                sizeof(struct timeval)) == -1) {
        //     perror("setsockopt ERROR");
        //     exit(1);
        // }

        errno = 0;
        if (connect(sockfd, l->ai_addr, l->ai_addrlen) == -1) {
            close(sockfd);
            // cout << strerror(errno) << endl;
            continue;
        }

        break;
    }
    if (l == NULL) {
        return false;
    }
    freeaddrinfo(servinfo); // all done with this structure
    set_server_fd(port, sockfd);
    return true;
}