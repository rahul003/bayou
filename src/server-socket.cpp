#include "../inc/server.h"
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
 * returns port number
 * @param  sa sockaddr structure
 * @return    port number contained in sa
 */
int return_port_no(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}

void sigchld_handler(int s) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

/**
 * Connects to master
 * @return  true if connection was successfull
 */
bool Server::ConnectToMaster() {
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
 * function for server's accept connections thread
 * @param _S Pointer to server class object
 */
void* AcceptConnections(void* _S) {
    Server *S = (Server *)_S;

    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    const char* po = "0";


    struct sockaddr_in addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (listen(sockfd, kBacklog) == -1) {
        perror("listen ERROR");
        exit(1);
    }

    // retrieves the randomly assigned port
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1)
        perror("getsockname");
    else
        S->set_my_listen_port(ntohs(sin.sin_port));

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    while (1) {
        // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept ERROR");
            continue;
        }

        if (setsockopt(new_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&kReceiveTimeoutTimeval,
                       sizeof(struct timeval)) == -1) {
            perror("setsockopt ERROR");
            exit(1);
        }

        int incoming_port = ntohs(return_port_no((struct sockaddr *)&their_addr));
        S->set_misc_fd(new_fd);
    }
    pthread_exit(NULL);
}

/**
 * Connects to a server port
 * @param port port of server whose server to connect to
 * @return  true if connection was successfull
 */
bool Server::ConnectToServer(const int port) {

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

        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&kReceiveTimeoutTimeval,
                       sizeof(struct timeval)) == -1) {
            perror("setsockopt ERROR");
            exit(1);
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
    set_misc_fd(sockfd);
    SendOrAskName(sockfd);
    return true;
}