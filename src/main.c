#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>

#include "../include/dynarray.h"
#include "../include/cstring.h"
#include "../include/fmt.h"
#include "../include/slice.h"

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

bool terminate_msg(struct dynarray *msg)
{
        if (msg->len == 0) {
                return false;
        }
        return ((char *)msg->data)[msg->len - 1] == '\n';
}

int cmdlisten(int const argc, char const *argv[])
{
        int exitstatus = 0;
        if (argc < 3) {
                return 1;
        }
        char const *service = argv[2];
        printf("service = '%s'\n\n", service);

        struct addrinfo *addrinfos, addrinfo;
        int sockfd;
        select_socket(NULL, service, &addrinfos, &addrinfo, &sockfd);

        if (bind(sockfd, addrinfo.ai_addr, addrinfo.ai_addrlen) == -1) {
                fprintf(stderr, "bind(): %s\n", strerror(errno));
                exitstatus = 1;
                goto cmdlisten_cleanup;
        }

        if (listen(sockfd, 1) == -1) {
                fprintf(stderr, "listen(): %s\n", strerror(errno));
                exitstatus = 1;
                goto cmdlisten_cleanup;
        }

        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
                perror("setsockopt");
                exitstatus = 1;
                goto cmdlisten_cleanup;
        }

        struct sockaddr_storage clientaddr;
        socklen_t clientaddr_sz = sizeof clientaddr;
        int clientsockfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientaddr_sz);
        if (clientsockfd == -1) {
                fprintf(stderr, "accept(): %s\n", strerror(errno));
                exitstatus = 1;
                goto cmdlisten_cleanup;
        }

        char str[INET6_ADDRSTRLEN];
        if (inet_ntop_auto((struct sockaddr *)&clientaddr, str) == -1) {
                exitstatus = 1;
                perror("Client had a weird ip address, I'm too scared to continue.");
                goto cmdlisten_cleanup;
        }
        printf("%s connected.\n", str);

        struct dynarray msg = dynarray_new();
        while (true) {
                msg.len = 0;
                ssize_t recvd = dynarray_recv(&msg, clientsockfd, terminate_msg);
                if (recvd == -1) {
                        fprintf(stderr, "recv(): %s\n", strerror(errno));
                        exitstatus = 1;
                        goto cmdlisten_cleanup;
                }
                if (recvd == 0) {
                        printf("Client terminated connection.\n");
                        exitstatus = 0;
                        goto cmdlisten_cleanup;
                }

                while (msg.len != 0 && is_ascii_control(*(char *)dynarray_get(
                                               &msg, TYPEINFO(uint8_t), msg.len - 1))) {
                        msg.len--;
                }
                DYNARRAY_PUSH(&msg, uint8_t, '\0');

                if (strcmp((char *)msg.data, "exit") == 0) {
                        break;
                }
                printf("client: %s\n", (char const *)msg.data);
        }

cmdlisten_cleanup:
        dynarray_free(&msg);
        freeaddrinfo(addrinfos);
        close(sockfd);
        close(clientsockfd);
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
