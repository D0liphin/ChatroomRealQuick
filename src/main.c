#include "main.h"
#include "command.h"
#include "../include/dynarray.h"
#include "../include/cstring.h"
#include "../include/fmt.h"
#include "../include/slice.h"
#include "../include/panic.h"
#include "../include/jtable.h"

#define EPOLL_TIMEOUT 10000
#define MAX_EVENTS 100
#define MAX_QUEUED_CONNECTIONS 10

bool is_ascii_control(char ch)
{
        return ch == '\n' || ch == '\r';
}

/**
 * Receive an arbitrary amount of data, extending `arr`. Uses kilobyte chunks.
 * Terminate based on `terminate()` callback.
 *
 * # Returns
 * - `-1` for failure and set `errno`
 * - `0` for client terminated connected
 * - `n` for number of bytes received
 */
ssize_t dynarray_recv(struct dynarray *restrict msg, int sockfd,
                      bool(terminate(struct dynarray *msg)))
{
        ssize_t total_recvd = 0;
        while (true) {
                uint8_t msgpart[1024];
                memset(msgpart, 0, sizeof msgpart);
                ssize_t recvd = recv(sockfd, msgpart, sizeof msgpart, 0);
                if (recvd <= 0) {
                        return recvd;
                }
                total_recvd += recvd;
                dynarray_extend(msg, &msgpart[0], &msgpart[0] + recvd);
                if (terminate(msg)) {
                        return total_recvd;
                }
        }
}

/** Test if a message ends with a newline */
bool terminate_msg(struct dynarray *msg)
{
        if (msg->len == 0) {
                return false;
        }
        return ((char *)msg->data)[msg->len - 1] == '\n';
}

/** Trim any control characters off the end of a `dynarray[char]` */
void trim_msg(struct dynarray *restrict msg)
{
        while (msg->len != 0 &&
               is_ascii_control(*(char *)dynarray_get(msg, TYPEINFO(uint8_t), msg->len - 1))) {
                msg->len--;
        }
        DYNARRAY_PUSH(msg, uint8_t, '\0');
}

/** Create a new server (listener) socket */
int add_server_socket(int epollfd, char const *name, char const *service)
{
        // Get addr
        struct addrinfo req = {
                .ai_socktype = SOCK_STREAM,
                .ai_family = AF_UNSPEC,
                .ai_flags = name ? 0 : AI_PASSIVE,
        };
        struct addrinfo *ais;
        int err;
        if ((err = getaddrinfo(name, service, &req, &ais))) return err;
        if (ais == NULL) return -1;
        struct addrinfo *ai;
        for (ai = ais; ai != NULL; ai = ai->ai_next) {
                struct sockaddr *sa = ai->ai_addr;
                if (sa->sa_family == AF_INET || sa->sa_family == AF_INET6) {
                        break;
                }
        }

        // Setup socket
        int sockfd;
        if ((sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1) return -1;
        if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) == -1) return -1;
        if (listen(sockfd, MAX_QUEUED_CONNECTIONS) == -1) return -1;
        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) return -1;

        // Store metadata
        struct sockserver *server = malloc(sizeof(struct sockserver));
        server->sockaddr = *ai->ai_addr;
        server->flags = SOCKSERVER;
        server->sockfd = sockfd;

        // Register fd with epoll
        struct epoll_event event = { EPOLLIN, { .ptr = (void *)server } };
        epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event);

        freeaddrinfo(ais);
        return 0;
}

int add_client(int epollfd, int serversockfd)
{
        struct sockaddr clientaddr;
        socklen_t clientaddrsz = sizeof clientaddr;
        int clientsockfd = accept(serversockfd, (struct sockaddr *)&clientaddr, &clientaddrsz);
        if (clientsockfd == -1) return -1;

        struct sockclient *client = malloc(sizeof(struct sockclient));
        client->sockaddr = clientaddr;
        client->flags = SOCKCLIENT;
        client->sockfd = clientsockfd;
        client->name = NULL;

        struct epoll_event event = { EPOLLIN, { .ptr = (void *)client } };
        epoll_ctl(epollfd, EPOLL_CTL_ADD, clientsockfd, &event);

        return 0;
}

void del_client(int epollfd, struct sockclient *client)
{
        epoll_ctl(epollfd, EPOLL_CTL_DEL, client->sockfd, NULL);
        free(client->name);
        free(client);
}

/** Get the type of this socket, either `SOCKSERVER` or `SOCKCLIENT` */
int socktype(void *sockinfo)
{
        uint32_t flags = *(uint32_t *)sockinfo;
        if (flags & SOCKCLIENT) {
                return SOCKCLIENT;
        }
        if (flags & SOCKSERVER) {
                return SOCKSERVER;
        }
        return 0;
}

void handle_event(int epollfd, struct epoll_event ev)
{
        if (socktype(ev.data.ptr) == SOCKSERVER) {
                struct sockserver *server = ev.data.ptr;
                add_client(epollfd, server->sockfd);

        } else if (socktype(ev.data.ptr) == SOCKCLIENT) {
                struct sockclient *client = ev.data.ptr;
                struct dynarray msg = dynarray_new();
                ssize_t recvd = dynarray_recv(&msg, client->sockfd, terminate_msg);
                if (recvd == -1) return;
                if (recvd == 0) {
                        del_client(epollfd, client);
                        return;
                }
                trim_msg(&msg);
                char const *args;
                switch (select_command(msg.data, &args)) {
                case COMMAND_SAY:
                        command_say(client, args);
                        break;
                case COMMAND_SETUSER:
                        command_setuser(client, args);
                        break;
                }
        }
}

int cmdlisten(int const argc, char const *argv[])
{
        if (argc < 4) return -1;
        char const *name = argv[2];
        char const *service = argv[3];

        int epollfd = epoll_create1(0);
        if (add_server_socket(epollfd, name, service) == -1) {
                printf("error: %s\n", strerror(errno));
        }
        struct epoll_event events[MAX_EVENTS];
        while (true) {
                size_t nr_events = epoll_wait(epollfd, events, MAX_EVENTS, EPOLL_TIMEOUT);
                for (size_t i = 0; i < nr_events; ++i) {
                        handle_event(epollfd, events[i]);
                }
        }

        PANIC("Clean up not yet implemented");
}

int main(int const argc, char const *argv[])
{
        if (argc < 2) {
                exit(1);
        }
        char const *cmd = argv[1];
        if (strcmp(cmd, "listen") == 0) {
                return cmdlisten(argc, argv);
        }
        return 0;
}
