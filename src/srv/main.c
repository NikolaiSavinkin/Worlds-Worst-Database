#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

#include "dbproto.h"
#include "file.h"
#include "parse.h"
#include "srvpoll.h"

void print_usage(char *argv[]) {
        printf("Usage %s [-n] -f <file path> -p <port number>\n", argv[0]);

        printf("\t-f <path>\n");
        printf("\t\t(required) path to database file\n");

        printf("\t-n\n");
        printf("\t\tcreate new database file\n");

        printf("\t-p <port>\n");
        printf("\t\t(required) port number to listen on\n");

        return;
}

void send_err_res(int fd, const char *msg) {

        static dbproto_buf_t res;
        static dbproto_err_res_t err_res;

        res = dbproto_new_buf(MSG_ERROR_RES, 1);
        err_res.len = htons(strlen(msg));
        strncpy(err_res.msg, msg, MSG_SIZE);
        dbproto_pack(&res, &err_res, sizeof(err_res));

        dbproto_send(fd, &res);
}

int handle_message(int dbfd, dbheader_t **header, employee_t **employees,
                sp_pfds_t *pfds, uint16_t i, dbproto_buf_t *buf,
                char addr_str[INET6_ADDRSTRLEN]) {

        static dbproto_buf_t res;
        static dbproto_hdr_t hdr = {0};

        dbproto_unpack(buf, &hdr, sizeof(hdr));
        hdr.type = ntohl(hdr.type);
        hdr.len = ntohs(hdr.len);

        switch (hdr.type) {
        case MSG_EMPLOYEE_ADD_REQ:
                dbproto_employee_add_req_t employee_add = {0};
                dbproto_unpack(buf, &employee_add, sizeof(employee_add));

                if (add_employee(*header, employees, employee_add.add_str)
                                == STATUS_ERROR) {
                        perror("add employee");
                        send_err_res(pfds->fds[i].fd,
                                        "failed to add employee");
                        return -1;
                }

                if (output_file(dbfd, *header, *employees)
                                == STATUS_ERROR) {
                        perror("output file");
                        send_err_res(pfds->fds[i].fd,
                                        "failed to commit changes");
                        return -1;
                }

                res = dbproto_new_buf(MSG_EMPLOYEE_ADD_RES, 0);
                dbproto_send(pfds->fds[i].fd, &res);
                printf("%s added %s\n", addr_str, employee_add.add_str);
                break;

        case MSG_EMPLOYEE_DEL_REQ:
                dbproto_employee_del_req_t employee_del = {0};
                dbproto_unpack(buf, &employee_del, sizeof(employee_del));

                int num_del = delete_employees(*header, employees,
                                employee_del.name);

                if (output_file(dbfd, *header, *employees)
                                == STATUS_ERROR) {
                        perror("output file");
                        send_err_res(pfds->fds[i].fd,
                                        "failed to commit changes");
                        return -1;
                }

                res = dbproto_new_buf(MSG_EMPLOYEE_DEL_RES, 1);
                dbproto_del_res_t del_res = {.num = htonl(num_del)};
                dbproto_pack(&res, &del_res, sizeof(del_res));
                dbproto_send(pfds->fds[i].fd, &res);
                printf("%s removed %s\n", addr_str, employee_del.name);
                break;

        case MSG_EMPLOYEE_LIST_REQ:
                res = dbproto_new_buf(MSG_EMPLOYEE_LIST_RES, (*header)->count);
                dbproto_employee_t employee;

                for (intmax_t i = 0; i < (*header)->count; i += 1) {
                        memset(employee.employee_str, 0,
                                        sizeof(employee.employee_str));
                        sprintf(employee.employee_str,
                                        "%ld\t%s\t%s\t%d",
                                        i,
                                        (*employees)[i].name,
                                        (*employees)[i].address,
                                        (*employees)[i].hours);
                        employee.len = strlen(employee.employee_str);
                        dbproto_pack(&res, &employee, sizeof(employee));
                }

                dbproto_send(pfds->fds[i].fd, &res);
                printf("%s requested a list\n", addr_str);
                break;

        default:
                char err_msg[] = "unsupported message type.";
                const uint16_t len = (uint16_t) strlen(err_msg);
                fprintf(stderr, "%s gave an %s\n", addr_str, err_msg);
                send_err_res(pfds->fds[i].fd, err_msg);

        }
        return 0;
}

int handle_hello(sp_pfds_t *pfds, uint16_t i, dbproto_buf_t *buf,
                char addr_str[INET6_ADDRSTRLEN]) {

        static dbproto_buf_t res;
        static dbproto_hdr_t hdr = {0};

        dbproto_unpack(buf, &hdr, sizeof(hdr));
        hdr.type = ntohl(hdr.type);
        hdr.len = ntohs(hdr.len);

        if (hdr.type != MSG_HELLO_REQ) {
                const char *err_msg = "Expected hello request";
                const uint16_t len = (uint16_t) strlen(err_msg);
                fprintf(stderr, "%s\n", err_msg);
                send_err_res(pfds->fds[i].fd, err_msg);
                return 0;
        }

        dbproto_hello_req_t hello = {0};
        dbproto_unpack(buf, &hello, sizeof(hello));
        hello.version = ntohs(hello.version);

        if (hello.version != PROTO_VERSION) {
                char err_msg[32] = "";
                snprintf(err_msg, 32, "Expected version %d",
                                PROTO_VERSION);
                const uint16_t len = (uint16_t) strlen(err_msg);
                fprintf(stderr, "%s\n", err_msg);

                send_err_res(pfds->fds[i].fd, err_msg);
                return 0;
        }

        pfds->states[i] = STATE_MSG;
        printf("%s upgraded to STATE_MSG\n", addr_str);

        res = dbproto_new_buf(MSG_HELLO_RES, 1);
        dbproto_hello_res_t hello_res = {.version = htons(PROTO_VERSION)};
        dbproto_pack(&res, &hello_res, sizeof(hello_res));
        dbproto_send(pfds->fds[i].fd, &res);

        return 0;
}

int handle_client(int dbfd, dbheader_t **header, employee_t **employees,
                sp_pfds_t *pfds, uint16_t i, dbproto_buf_t *buf) {

        char addr_str[INET6_ADDRSTRLEN] = "";
        if (sp_get_addr(pfds->fds[i].fd, addr_str) != 0) return -1;

        switch (pfds->states[i]) {
        case STATE_HELLO:
                handle_hello(pfds, i, buf, addr_str);
                break;

        case STATE_MSG:
                handle_message(dbfd, header, employees,
                                pfds, i, buf, addr_str);
                break;

        default:
                char err_msg[] = "unsupported state.";
                const uint16_t len = (uint16_t) strlen(err_msg);
                fprintf(stderr, "%s has %s\n", addr_str, err_msg);
                send_err_res(pfds->fds[i].fd, err_msg);
        }

        return 0;
}

bool handle_leave(sp_pfds_t *pfds, uint16_t leave_id,
                char leave_msg[MSG_SIZE]) {

        char addr_str[INET6_ADDRSTRLEN] = "";
        if (sp_get_addr(pfds->fds[leave_id].fd, addr_str) == 0) {
                snprintf(leave_msg, MSG_SIZE, "%s left", addr_str);
        } else {
                perror("sp_get_addr");
        }

        close(pfds->fds[leave_id].fd);
        bool resize;
        sp_rm_pfd(pfds, leave_id, &resize);

        return resize;
}

int handle_events(int dbfd, dbheader_t **header, employee_t **employees,
                sp_pfds_t **pfds, int *num_events) {
        static dbproto_buf_t buf;
        static bool resize;
        static int is_readable, nread;
        static uint16_t i, new_len;
        static ssize_t new_size;
        
        for (i = 1; sp_next_event(*pfds, &i, num_events); ) {
        
                dbproto_buf_init(&buf);
                is_readable= sp_is_event_readable(*pfds, i);

                if (is_readable == 1) {
                        nread = dbproto_recv((*pfds)->fds[i].fd, &buf);
                        if (nread > 0) {
                                handle_client(dbfd, header, employees,
                                                *pfds, i, &buf);
                                continue;
                        }
                }

                char leave_msg[MSG_SIZE];
                resize = handle_leave(*pfds, i, leave_msg);
                if (resize) {
                        new_len = (*pfds)->len / 2;
                        new_size = sp_pfds_get_size(new_len);
                        (*pfds) = realloc((*pfds), new_size);
                        if (*pfds == NULL) {
                                perror("decreasing pfds");
                                return -1;
                        }

                        (*pfds)->len = new_len;
                        (*pfds)->states = realloc((*pfds)->states, new_len);
                        if (*pfds == NULL) {
                                perror("decreasing client states");
                                return -1;
                        }
                }

                if (is_readable == -1) {
                        perror(leave_msg);
                } else {
                        printf("%s\n", leave_msg);
                }
 
        }
        return 0;
}

int main(int argc, char *argv[]) { 

        /* 
         * USER INPUT
         */

        int c;
	char *file_path = NULL;
        bool new_file = false;
        char *port = NULL;

        while ((c = getopt(argc, argv, "f:np:")) != -1) {

                switch (c) {
                case 'f':
                        file_path = optarg;
                        break;
                case 'n':
                        new_file = true;
                        break;
                case 'p':
                        port = optarg;
                        break;
                case '?':
                        print_usage(argv);
                        return 0;
                default:
                        return -1;
                }
        }

        if (file_path == NULL || port == NULL) {
                print_usage(argv);
                return 0;
        }

        if (strlen(port) > PORTSTRLEN) {
                puts("Port number is too long, would fall out of range");
                print_usage(argv);
                return 0;
        }

        /* 
         * DATABASE SETUP
         */

        int dbfd = -1;
        dbheader_t *header = NULL;
        employee_t *employees = NULL;

        if (new_file == true) {

                dbfd = create_db_file(file_path);
                if (dbfd == STATUS_ERROR) {
                        fprintf(stderr, "Cannot create database file\n");
                        return -1;
                }

                if (create_db_header(dbfd, &header) == STATUS_ERROR) {
                        fprintf(stderr, "Failed to create database header");
                        close_db_file(dbfd);
                        return -1;
                }

                if (output_file(dbfd, header, employees) == STATUS_ERROR) {
                        perror("Writing file");
                        close_db_file(dbfd);
                        return -1;
                }
        } else {

                dbfd = open_db_file(file_path);
                if (dbfd == STATUS_ERROR) {
                        fprintf(stderr, "Cannot open database file\n");
                        return -1;
                }
                
                if (validate_db_header(dbfd, &header) == STATUS_ERROR) {
                        fprintf(stderr, "Validation failed\n");
                        return -1;
                }
        }

        if (read_employees(dbfd, header, &employees) == STATUS_ERROR) {
                fprintf(stderr, "Failed to read employee data");
                return -1;
        }

        /*
         * NETWORK STUFF
         */

        sp_pfds_t *pfds = malloc(sp_pfds_get_size(PFDS_MIN));
        if (pfds == NULL) {
                perror("initialising pfds");
                return -1;
        }
        memset(pfds, 0, sp_pfds_get_size(PFDS_MIN));

        pfds->states = malloc(PFDS_MIN * sizeof(clientstate_e));
        if (pfds->states == NULL) {
                perror("initialising client states");
                return -1;
        }

        sp_error_e ie;
        if ((ie = sp_pfds_init(pfds, port)) != SPE_SUCCESS) {
                const char *err_str = ie == SPE_PFDS_NULL ? "pfds"        :
                                      ie == SPE_FDS_NULL  ? "pfds->fds"   :
                                                            "pfds->states";
                fprintf(stderr, "Init failed, %s was NULL\n", err_str);
                return -1;
        }

        if (pfds->fds[0].fd == -1) {
                fprintf(stderr, "Error getting listening socket on init\n");
                free(pfds);
                return -1;
        }

        int num_events, new_fd, con_err;
        uint16_t new_len;
        ssize_t new_size;
        while (1) {
                num_events = poll(pfds->fds, pfds->count, -1);

                if (num_events == -1) {
                        perror("poll");
                        free(pfds);
                        return -1;
                } else if (num_events == 0) {
                        fprintf(stderr, "\n\nNo events after poll somehow.\n\n\n");
                        continue;
                }

                con_err = sp_new_connection(pfds, &num_events, &new_fd);
                if (con_err == SPE_RESIZE) {
                        new_len = pfds->len * 2;
                        new_size = sp_pfds_get_size(new_len);
                        pfds = realloc(pfds, new_size);
                        if (pfds == NULL) {
                                perror("increasing pfds");
                                return -1;
                        }
                        pfds->len = new_len;
                        pfds->states = realloc(pfds->states, new_len);
                        if (pfds == NULL) {
                                perror("increasing client states");
                                return -1;
                        }
                        con_err = sp_add_pfd(pfds, new_fd);
                }

                if (con_err == -1) {
                        perror("new connection");
                        free(pfds);
                        return -1;

                } else if ((sp_error_e) con_err == SPE_SUCCESS) {
                        char addr_str[INET6_ADDRSTRLEN] = "";
                        if (sp_get_addr(new_fd, addr_str) != 0) {
                                perror("get addr");
                        } else {
                                printf("%s joined\n", addr_str);
                        }

                } else if (con_err != 0) {
                        fprintf(stderr, "Connection error: %d\n", con_err);
                }
                
                if (handle_events(dbfd, &header, &employees, &pfds, &num_events)
                                == -1) {
                        return -1;
                }

        }

        close_db_file(dbfd);
        free(pfds);
        return 0;
}
