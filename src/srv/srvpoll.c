#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <poll.h>

#include "srvpoll.h"

void _close_all_fds(sp_pfds_t *pfds) {
        for (int i = 0; i < pfds->count; i += 1) {
                close(pfds->fds[i].fd);
        }
}

int _sp_new_listener_socket(char port[PORTSTRLEN]) {
        int listener;
        int opt_error;
        int bind_error;
        int opt_true = 1;
        struct addrinfo hints;
        struct addrinfo *ai;
        struct addrinfo *p;

        memset(&hints, '\0', sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

        int ai_error = getaddrinfo( NULL, port, &hints, &ai);
        if (ai_error != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ai_error));
                return -1;
        }

        for (p = ai; p != NULL; p = p->ai_next) {
                listener = socket(p->ai_family, SOCK_STREAM, 0);
                if ( listener == -1) {
                        perror("socket");
                        continue;
                }

                opt_error = setsockopt(listener, SOL_SOCKET,
                                SO_REUSEADDR, &opt_true, sizeof(int));
                if (opt_error == -1) {
                        perror("setsockopt");
                        close(listener);
                        continue;
                }

                bind_error = bind(listener, p->ai_addr, p->ai_addrlen);
                if (opt_error == -1) {
                        perror("bind");
                        close(listener);
                        continue;
                }

                break;
        }

        freeaddrinfo(ai);

        if (p == NULL) return -1;

        if (listen(listener, BACKLOG) == -1) return -1;

        return listener;
}

ssize_t sp_pfds_get_size(uint16_t length) {
        if (PFDS_MAX < length) length = PFDS_MAX;
        if (length < PFDS_MIN) length = PFDS_MIN;

        return sizeof(int16_t) + sizeof(int16_t) + sizeof(clientstate_e *)
                + (sizeof(struct pollfd) * length);
}

void sp_pfds_print(sp_pfds_t *pfds, char *location) {
        printf("%s\tCount: %d\tLength: %d\n",
                        location, pfds->count, pfds->len);
        printf("id:\tfd\tst\n");
        for (uint16_t i = 0; i < pfds->len; i += 1) {
                printf("%d:\t%d\t%d\n", i, pfds->fds[i].fd, pfds->states[i]);
        }
}

int sp_pfds_init(sp_pfds_t *pfds, char port[PORTSTRLEN]) {
        if (pfds == NULL) return SPE_PFDS_NULL;
        if (pfds->states == NULL) return SPE_STATES_NULL;
        if (pfds->fds == NULL) return SPE_FDS_NULL;

        pfds->count = 1;
        pfds->len = PFDS_MIN;
        pfds->states[0] = STATE_HELLO;
        for (int i = 0; i < PFDS_MIN; i += 1) {
                pfds->states[i] = STATE_NEW;
        } 

        pfds->fds[0].fd = _sp_new_listener_socket(port);
        pfds->fds[0].events = POLLIN | POLLPRI | POLLERR
                | POLLHUP | POLLNVAL;

        return SPE_SUCCESS;
}

int sp_new_connection(sp_pfds_t *pfds, int *events, int *new_fd) {
        if (*events == 0) return 0;

        if (pfds->fds[0].revents & (POLLPRI | POLLERR | POLLHUP | POLLNVAL)) {
                /* if we get error/hangup events just abort everything */
                errno = ECONNABORTED;
                _close_all_fds(pfds);
                *events -= 1;
                return -1;
        }
        
        if (!(pfds->fds[0].revents & POLLIN)) {
                return 0;
        }

        *events -= 1;

        static struct sockaddr_storage addr;
        static socklen_t addr_len = sizeof(addr);

        *new_fd = accept(pfds->fds[0].fd, (struct sockaddr *)&addr, &addr_len);

        if (*new_fd == -1) {
                return -1;
        }

        return sp_add_pfd(pfds, *new_fd);
}

bool sp_next_event(sp_pfds_t *pfds, uint16_t *i, int *events) {
        for ( ;*i < pfds->count && *events > 0; *i += 1) {
                if (pfds->fds[*i].revents
                                & (POLLIN | POLLPRI | POLLERR
                                        | POLLHUP | POLLNVAL)) {
                        *events -= 1;
                        return true;
                }
        }
        return false;
}

int sp_is_event_readable(sp_pfds_t *pfds, int read_id) {
        if (pfds->fds[read_id].revents & POLLNVAL) {
                errno = EINVAL;
                return -1;
        }

        if (pfds->fds[read_id].revents & (POLLHUP | POLLERR)) {
                return 0;
        }

        return 1;
}

sp_error_e sp_add_pfd(sp_pfds_t *pfds, int newfd) {
        if (pfds->count >= PFDS_MAX) {
                return SPE_PFDS_MAX;
        }

        if (pfds->count > pfds->len) {
                return SPE_RANGE;
        }

        if (pfds->count == pfds->len) {
                return SPE_RESIZE;
        }

        pfds->states[pfds->count] = STATE_HELLO;
        pfds->fds[pfds->count].fd = newfd;
        pfds->fds[pfds->count].events = POLLIN;
        pfds->count += 1;

        return SPE_SUCCESS;
}

sp_error_e sp_rm_pfd(sp_pfds_t *pfds, int rm_id, bool *resize) {
        if (rm_id >= pfds->count || rm_id <= 0) {
                return SPE_RANGE;
        }

        if (pfds->count * 3 < pfds->len) {
                *resize = true;
        } else {
                *resize = false;
        }

        pfds->count -= 1;
        pfds->fds[rm_id] = pfds->fds[pfds->count];
        pfds->states[rm_id] = pfds->states[pfds->count];

        return SPE_SUCCESS;
}

int sp_get_addr(int socket, char addr_str[INET6_ADDRSTRLEN]) {
        struct sockaddr_storage addr = {0};
        socklen_t addr_len = sizeof(struct sockaddr_storage);

        if (getpeername(socket, (struct sockaddr *)&addr, &addr_len) != 0)
                return -1;

        if (addr.ss_family == AF_INET) {

                inet_ntop(addr.ss_family, (struct sockaddr_in*) &addr,
                                addr_str, INET6_ADDRSTRLEN);
        } else {

                inet_ntop(addr.ss_family, (struct sockaddr_in6*) &addr,
                                addr_str, INET6_ADDRSTRLEN);
        }

        return 0;
}
