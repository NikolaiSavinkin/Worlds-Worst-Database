#ifndef SRVPOLL_H
#define SRVPOLL_H

#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <poll.h>

#include "dbproto.h"

#define BACKLOG 16
#define PFDS_MAX 256
#define PFDS_MIN 2

typedef enum {
        SPE_SUCCESS = 1,
        SPE_PFDS_MAX,
        SPE_RANGE,
        SPE_RESIZE,
        SPE_PFDS_NULL,
        SPE_FDS_NULL,
        SPE_STATES_NULL,
} sp_error_e;

typedef enum {
        STATE_NEW,
        STATE_CONNECTED,
        STATE_DISCONNECTED,
        STATE_HELLO,
        STATE_MSG,
        STATE_GOODBYE,
} clientstate_e;

typedef struct {
        uint16_t count;         /* number of fds in fds[] */
        uint16_t len;           /* length of fds[] array */
        clientstate_e *states;  /* array of states,
                                 * index aligned with fds[] */
        struct pollfd fds[];
} sp_pfds_t;

ssize_t sp_pfds_get_size(uint16_t length);

void sp_pfds_print(sp_pfds_t *pfds, char *location);

int sp_pfds_init(sp_pfds_t *pfds, char port[PORTSTRLEN]);

int sp_new_connection(sp_pfds_t *pfds, int *events, int *new_fd);

bool sp_next_event(sp_pfds_t *pfds, uint16_t *i, int *events);

int sp_is_event_readable(sp_pfds_t *pfds, int read_id);

sp_error_e sp_add_pfd(sp_pfds_t *pfds, int newfd);

sp_error_e sp_rm_pfd(sp_pfds_t *pfds, int rm_id, bool *resize);

int sp_get_addr(int socket, char addr_str[INET6_ADDRSTRLEN]);

#endif
