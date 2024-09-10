#pragma once

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <stdlib.h>

#define BLUE(STR) "\x1b[34m" STR "\x1b[0m"

#define SOCKSERVER 1
#define SOCKCLIENT (1 << 1)
#define CLIENTREG (1 << 2)

struct sockserver {
        uint32_t flags; // Structural prefixing, be careful
        int sockfd;
        struct sockaddr sockaddr;
};

struct sockclient {
        uint32_t flags; // Structural prefixing, be careful
        int sockfd;
        struct sockaddr sockaddr;
        char *name; // nul terminated
};