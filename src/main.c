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

#include "../include/dynarray.h"
#include "../include/cstring.h"
#include "../include/fmt.h"
#include "../include/slice.h"
#include "../include/panic.h"
#include "../include/jtable.h"

#define EPOLL_TIMEOUT 10000
#define MAX_EVENTS 100

/**
 * Get a line from `stdin`, or just `len` `char`s of it if it's too long.
 */
struct cstring readline(FILE *f)
{
        struct cstring str = cstring_new();
        while (true) {
                char buf[32];
                memset(buf, 0, sizeof buf);
                fgets(buf, sizeof buf, f);
                size_t len = sizeof buf - 1;
                while (buf[len] == '\0') {
                        if (len == 0) {
                                break;
                        }
                        len--;
                }
                cstring_extend(&str, (uint8_t const *)&buf[0], (uint8_t const *)&buf[len]);
                if (buf[len] == '\n') {
                        break;
                }
        }
        return str;
}

int inet_ntop_auto(struct sockaddr *sa, char *str)
{
        void const *cp;
        switch (sa->sa_family) {
        case AF_INET:
                cp = &((struct sockaddr_in *)sa)->sin_addr;
                break;
        case AF_INET6:
                cp = &((struct sockaddr_in6 *)sa)->sin6_addr;
                break;
        default:
                printf("sa->family = %d\n", sa->sa_family);
                return -1;
        }
        inet_ntop(sa->sa_family, cp, str, INET6_ADDRSTRLEN);
        return 0;
}

bool is_ascii_control(char ch)
{
        return ch == '\n' || ch == '\r';
}

/** 
 * Allow the user to select the specific address to connect to, returns the 
 * same error types as `getaddrinfo()`. Returns 0..N to indicate the index of 
 * the selected `addrinfo`.
 */
int select_addrinfo(char const *name, char const *service, struct addrinfo **addrinfos_out)
{
        struct addrinfo hints = (struct addrinfo){
                .ai_socktype = SOCK_STREAM,
                .ai_family = AF_UNSPEC,
                .ai_flags = name ? 0 : AI_PASSIVE,
        };

        struct addrinfo *addrinfos;
        int err;
        if ((err = getaddrinfo(name, service, &hints, &addrinfos))) {
                return err;
        }

        // NOTE: This is actually not quite right, since the user can
        // technically select something that we skipped... but whatever for now
        int maxsel = 0;
        for (struct addrinfo *addrinfo = addrinfos; addrinfo != NULL;
             addrinfo = addrinfo->ai_next, maxsel++) {
                char str[INET6_ADDRSTRLEN];
                if (inet_ntop_auto(addrinfo->ai_addr, str) == -1) {
                        continue;
                }
                printf("[%d] %s\n", maxsel, str);
        }

        int sel = -1;
        while (sel == -1 || sel >= maxsel) {
                sel = -1;
                printf("\nSelect (default = 0): ");
                struct cstring selstr = readline(stdin);
                if (selstr.buf.len == 1) {
                        sel = 0;
                } else {
                        sscanf(cstring_as_cstr(&selstr), "%d\n", &sel);
                }
                cstring_free(&selstr);
        }
        puts("");

        *addrinfos_out = addrinfos;
        return sel;
}

void select_socket(char const *name, char const *service, struct addrinfo **addrinfos,
                   struct addrinfo *addrinfo, int *sockfd)
{
        int err;
        if ((err = select_addrinfo(name, service, addrinfos)) < 0) {
                fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(err));
                exit(1);
        }
        int sel = err;

        struct addrinfo *ai;
        for (ai = *addrinfos; sel > 0; ai = ai->ai_next) {
                --sel;
        }
        *addrinfo = *ai;

        *sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (*sockfd == -1) {
                fprintf(stderr, "socket(): %s\n", strerror(errno));
                exit(1);
        }
}

int cmdsend(int const argc, char const *argv[])
{
        PANIC("not yet implemented!");
        if (argc < 4) {
                return 1;
        }
        char const *name = argv[2];
        char const *service = argv[3];
        printf("name = '%s'\n", name);
        printf("service = '%s'\n\n", service);

        struct addrinfo *addrinfos, addrinfo;
        int sockfd;
        select_socket(name, service, &addrinfos, &addrinfo, &sockfd);

        if (connect(sockfd, addrinfo.ai_addr, addrinfo.ai_addrlen) == -1) {
                fprintf(stderr, "connect(): %s\n", strerror(errno));
                return 1;
        }

        freeaddrinfo(addrinfos);
        return 0;
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

struct accept_clients_args {
        int sockfd;
        struct dynarray *clientaddrs;
        pthread_mutex_t *clientaddrs_lock;
};

/*
// contains `struct sockaddr_storage`
struct dynarray clientaddrs = dynarray_new();
pthread_mutex_t clientaddrs_lock;
pthread_mutex_init(&clientaddrs_lock, NULL);

struct accept_clients_args ac_args = { sockfd, &clientaddrs, &clientaddrs_lock };
pthread_t ac_thread;
pthread_create(&ac_thread, NULL, (void *(*)(void *))accept_clients, &ac_args);
*/

/** Continuously accept new clients and add them to `clientaddrs` */
void *accept_clients(struct accept_clients_args *args)
{
        puts("accept_clients()");
        int sockfd = args->sockfd;
        struct dynarray *clientaddrs = args->clientaddrs;
        pthread_mutex_t *clientaddrs_lock = args->clientaddrs_lock;
        while (true) {
                struct sockaddr_storage clientaddr;
                socklen_t clientaddr_sz = sizeof clientaddr;
                int clientsockfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientaddr_sz);
                if (clientsockfd == -1) {
                        return NULL; // TODO: decide how we should handle this
                }

                char str[INET6_ADDRSTRLEN];
                if (inet_ntop_auto((struct sockaddr *)&clientaddr, str) == -1) {
                        puts("Bad client ignored");
                        continue;
                }

                pthread_mutex_lock(clientaddrs_lock);
                DYNARRAY_PUSH(clientaddrs, struct sockaddr_storage, clientaddr);
                pthread_mutex_unlock(clientaddrs_lock);

                printf("Client \"%s\" connected\n", str);
        }
        return NULL;
}

struct sockdata {
        int sockfd;
        struct cstring name;
};

ssize_t accept_client(int pubsockfd, struct dynarray *sockdata, int epollfd)
{
        struct sockaddr_storage clientaddr;
        socklen_t clientaddr_sz = sizeof clientaddr;
        int clientsockfd = accept(pubsockfd, (struct sockaddr *)&clientaddr, &clientaddr_sz);
        if (clientsockfd == -1) {
                return -1;
        }

        char str[INET6_ADDRSTRLEN];
        if (inet_ntop_auto((struct sockaddr *)&clientaddr, str) == -1) {
                puts("Bad client");
        }

        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.u64 = dynarray_length(sockdata, TYPEINFO(struct cstring));
        epoll_ctl(epollfd, EPOLL_CTL_ADD, clientsockfd, &event);

        struct sockdata data;
        data.sockfd = clientsockfd;
        data.name = cstring_is("anon");
        fmt_int(&data.name, &clientsockfd);
        DYNARRAY_PUSH(sockdata, struct sockdata, data);

        return event.data.u64;
}

// void terminate_client(int )

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

#define BLUE(S) "\033[0;34m" S "\033[0m"

int cmdlisten(int const argc, char const *argv[])
{
        int exitstatus = 0;
        if (argc < 3) {
                return 1;
        }
        char const *service = argv[2];
        printf("service = '%s'\n\n", service);

        struct addrinfo *addrinfos, addrinfo;
        int pubsockfd; // People connect with this
        select_socket(NULL, service, &addrinfos, &addrinfo, &pubsockfd);

        if (bind(pubsockfd, addrinfo.ai_addr, addrinfo.ai_addrlen) == -1) {
                fprintf(stderr, "bind(): %s\n", strerror(errno));
                exitstatus = 1;
                goto cmdlisten_cleanup;
        }

        if (listen(pubsockfd, 10) == -1) {
                fprintf(stderr, "listen(): %s\n", strerror(errno));
                exitstatus = 1;
                goto cmdlisten_cleanup;
        }

        int yes = 1;
        if (setsockopt(pubsockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
                perror("setsockopt");
                exitstatus = 1;
                goto cmdlisten_cleanup;
        }

        int epollfd = epoll_create1(0);
        struct epoll_event event = { EPOLLIN, { .u64 = 0 } };
        epoll_ctl(epollfd, EPOLL_CTL_ADD, pubsockfd, &event);

        struct dynarray sockdata = dynarray_new(); // struct sockdata
        DYNARRAY_PUSH(&sockdata, struct sockdata,
                      ((struct sockdata){
                              .sockfd = pubsockfd,
                              .name = cstring_is("pub"),
                      }));

        bool dorun = true;
        struct epoll_event events[MAX_EVENTS];
        while (dorun) {
                size_t nr_events = epoll_wait(epollfd, events, MAX_EVENTS, EPOLL_TIMEOUT);
                struct dynarray msg = dynarray_new();
                for (size_t i = 0; i < nr_events; ++i) {
                        struct sockdata client = *(struct sockdata *)dynarray_get(
                                &sockdata, TYPEINFO(struct sockdata), events[i].data.u64);
                        if (client.sockfd == pubsockfd) {
                                // A new client connected
                                ssize_t index = accept_client(pubsockfd, &sockdata, epollfd);
                                struct sockdata newclient = *(struct sockdata *)dynarray_get(
                                        &sockdata, TYPEINFO(struct sockdata), index);
                                if (index == -1) {
                                        puts("Client attempted to connect, but was invalid");
                                }
                                printf(BLUE("%s connected\n"), cstring_as_cstr(&newclient.name));
                        } else {
                                // An existing client sent us something
                                msg.len = 0;
                                ssize_t recvd = dynarray_recv(&msg, client.sockfd, terminate_msg);
                                if (recvd == -1) {
                                        fprintf(stderr, "recv(): %s\n", strerror(errno));
                                        exitstatus = 1;
                                        goto cmdlisten_cleanup;
                                }

                                trim_msg(&msg);
                                if (recvd == 0 || strcmp((char *)msg.data, "exit") == 0) {
                                        printf(BLUE("%s disconnected\n"),
                                               cstring_as_cstr(&client.name));
                                        epoll_ctl(epollfd, EPOLL_CTL_DEL, client.sockfd, NULL);
                                        continue;
                                }
                                printf("%s: %s\n", cstring_as_cstr(&client.name),
                                       (char const *)msg.data);
                        }
                }
                dynarray_free(&msg);
        }

cmdlisten_cleanup:
        for (struct sockdata *client = (struct sockdata *)dynarray_begin(&sockdata);
             (void *)client != dynarray_end(&sockdata); ++client) {
                cstring_free(&client->name);
        }
        dynarray_free(&sockdata);
        freeaddrinfo(addrinfos);
        close(pubsockfd);
        close(epollfd);
        return exitstatus;
}

int main(int const argc, char const *argv[])
{
        if (argc < 2) {
                exit(1);
        }
        char const *cmd = argv[1];
        if (strcmp(cmd, "listen") == 0) {
                return cmdlisten(argc, argv);
        } else if (strcmp(cmd, "send") == 0) {
                return cmdsend(argc, argv);
        }
        return 0;
}
