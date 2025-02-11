#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>

#include "dbproto.h"

void hexdump(FILE *out, const void *data, size_t len);

void print_usage(char *argv[]) {
        printf("Usage %s -h <host> -p <port> -[r|a|l]\n", argv[0]);

        printf("\t-r <name>\n");
        printf("\t\tremove all employees that exactly match the given name\n");

        printf("\t-a <row data>\n");
        printf("\t\tadd a new row to the database file with the given data\n");

        printf("\t-l\n");
        printf("\t\tlist all rows in the database file\n\n");

        printf("Note: where multiple commands are given, ");
        printf("they will be performed in the order shown. ");
        printf("(i.e. r, then a, then l)\n");

        return;
}

void dump_response(const char buf[BUFF_SIZE]) {
        printf("hello\n");

        int dumpfd = creat("message.dump", S_IRUSR | S_IWUSR);
        if (dumpfd == -1 && errno == EEXIST) {
                dumpfd = open("message.dump", O_WRONLY | O_TRUNC,
                                S_IRUSR | S_IWUSR);
        }
        if (dumpfd == -1) {
                perror("open message dump");
                return;
        }
        
        int wr_err = write(dumpfd, buf, BUFF_SIZE);
        if (wr_err == -1) {
                perror("write message dump");
                fprintf(stderr, "%p\n", buf);
        }
        close(dumpfd);
}

void send_hello(int fd) {

        dbproto_buf_t buf = dbproto_new_buf(MSG_HELLO_REQ, 1);
        dbproto_hello_req_t hello = {.version = htons(PROTO_VERSION)};

        dbproto_pack(&buf, &hello, sizeof(hello));
        dbproto_send(fd, &buf);
}

void delete_employee(int fd, char name[MSG_SIZE]) {

        dbproto_buf_t buf = dbproto_new_buf(MSG_EMPLOYEE_DEL_REQ, 1);

        dbproto_employee_del_req_t del_req = {0};
        strncpy(del_req.name, name, MSG_SIZE);
        del_req.len = htons(strlen(name));

        dbproto_pack(&buf, &del_req, sizeof(del_req));
        dbproto_send(fd, &buf);
}

void add_employee(int fd, char add_string[MSG_SIZE]) {

        dbproto_buf_t buf = dbproto_new_buf(MSG_EMPLOYEE_ADD_REQ, 1);

        dbproto_employee_add_req_t add_req = {0};
        strncpy(add_req.add_str, add_string, MSG_SIZE);
        add_req.len = htons(strlen(add_string));

        dbproto_pack(&buf, &add_req, sizeof(add_req));
        dbproto_send(fd, &buf);
}

void list_employees(int fd) {
        dbproto_buf_t buf = dbproto_new_buf(MSG_EMPLOYEE_LIST_REQ, 0);
        dbproto_send(fd, &buf);
}

bool handle_response(int fd) {

        dbproto_buf_t buf;
        dbproto_buf_init(&buf);
        dbproto_recv(fd, &buf);

        dbproto_hdr_t hdr = {0};
        dbproto_unpack(&buf, &hdr, sizeof(hdr));
        hdr.type = ntohl(hdr.type);
        hdr.len = ntohs(hdr.len);
        dump_response(buf.body);

        switch (hdr.type) {
        case MSG_ERROR_RES:
                dbproto_err_res_t err_res = {0};
                dbproto_unpack(&buf, &err_res, sizeof(err_res));
                fprintf(stderr, "Server error: %s\n", err_res.msg);
                return false;

        case MSG_HELLO_RES:
                dbproto_hello_res_t hello = {0};
                dbproto_unpack(&buf, &hello, sizeof(hello));
                hello.version = ntohs(hello.version);
                if (hello.version != PROTO_VERSION) {
                        fprintf(stderr, "Unsupported version, %d\n",
                                        hello.version);
                        return false;
                }
                printf("Hello from server\n");
                break;

        case MSG_EMPLOYEE_ADD_RES:
                printf("Add successful\n");
                break;

        case MSG_EMPLOYEE_DEL_RES:
                dbproto_del_res_t del_res = {0};
                dbproto_unpack(&buf, &del_res, sizeof(del_res));
                del_res.num = ntohl(del_res.num);
                uint8_t suffix[4];
                strcpy(suffix, del_res.num == 1 ? "y" : "ies");
                printf("Deleted %ld entr%s\n", del_res.num, suffix);
                break;

        case MSG_EMPLOYEE_LIST_RES:
                dbproto_employee_t employee;
                printf("Employees:\n");
                for (intmax_t i = 0; i < hdr.len; i += 1) {
                        dbproto_unpack(&buf, &employee, sizeof(employee));
                        printf("%s\n", employee.employee_str);
                }
                break;

        default:
                fprintf(stderr, "Could no read server response. ");
                fprintf(stderr, "Writing full message in 'message_dump'\n");
                dump_response(buf.body);
                return false;
        }

        return true;
}

int main(int argc, char *argv[]) {

        int c;
        uint8_t *add_str = NULL;
        uint8_t *node = NULL;
        bool list = false;
        uint8_t *service = NULL;
        uint8_t *del_str = NULL;

        while ((c = getopt(argc, argv, "a:h:lp:r:")) != -1) {

                switch (c) {
                case 'a': /* add */
                        add_str = optarg;
                        break;
                case 'h': /* host (node) */
                        node = optarg;
                        break;
                case 'l': /* list */
                        list = true;
                        break;
                case 'p': /* port (service)*/
                        service = optarg;
                        break;
                case 'r': /* remove (delete)*/
                        del_str = optarg;
                        break;
                case '?':
                        print_usage(argv);
                        return 0;
                default:
                        return -1;
                }
        }

        if (node == NULL || service == NULL) {
                print_usage(argv);
                return -1;
        }

        bool arg_err = false;
        if (strlen(node) >= NI_MAXHOST) {
                fprintf(stderr, "Host argument too long\n");
                arg_err = true;
        }

        if (strlen(service) >= NI_MAXSERV) {
                fprintf(stderr, "Port argument too long\n");
                arg_err = true;
        }

        if (del_str != NULL && strlen(del_str) >= MSG_SIZE) {
                fprintf(stderr, "Remove argument too long\n");
                arg_err = true;
        }

        if (add_str != NULL && strlen(add_str) >= MSG_SIZE) {
                fprintf(stderr, "Add argument too long\n");
                arg_err = true;
        }

        if (arg_err) return -1;

        struct addrinfo hints, *res, *cur;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        /* Maybe validate node and service some more? 
         * getaddrinfo() doesn't really give descriptive errors for
         * an end user.
         * */
        int gai_err = getaddrinfo(node, service, &hints, &res);
        if (gai_err != 0) {
                fprintf(stderr, "Failed getting address info:\n%s\n",
                                gai_strerror(gai_err));
                return -1;
        }

        int sockfd;
        uint8_t host[NI_MAXHOST];
        uint16_t port;

        for (cur = res; cur != NULL; cur = cur->ai_next) {
                if (cur->ai_family == AF_INET) {
                        inet_ntop(cur->ai_family,
                                        &(((struct sockaddr_in *)cur->ai_addr)
                                                ->sin_addr),
                                        host, NI_MAXHOST);
                        port = ntohs(((struct sockaddr_in *)cur->ai_addr)
                                                ->sin_port);

                } else {
                        inet_ntop(cur->ai_family,
                                        &(((struct sockaddr_in6 *)cur->ai_addr)
                                                ->sin6_addr),
                                        host, NI_MAXHOST);
                        port = ntohs(((struct sockaddr_in6 *)cur->ai_addr)
                                                ->sin6_port);

                }

                printf("Trying %s on %d...\n", host, port);
                sockfd = socket(cur->ai_family, cur->ai_socktype,
                                cur->ai_protocol);
                if (sockfd == -1) {
                        perror("socket");
                        continue;
                }
                
                if (connect(sockfd, cur->ai_addr, cur->ai_addrlen) == -1) {
                        close(sockfd);
                        perror("connect");
                        continue;
                }

                printf("Connected to %s\n", argv[1]);
                break;
        }

        freeaddrinfo(res);

        if (cur == NULL) {
                fprintf(stderr, "Failed to connect to any hosts\n");
                return -1;
        }

        send_hello(sockfd);
        bool proceed = handle_response(sockfd);
        if (!proceed) return -1;

        if (del_str != NULL) {
                delete_employee(sockfd, del_str);
                proceed = handle_response(sockfd);
        }
        if (!proceed) return -1;

        if (add_str != NULL) {
                add_employee(sockfd, add_str);
                proceed = handle_response(sockfd);
        }
        if (!proceed) return -1;

        if (list) {
                list_employees(sockfd);
                proceed = handle_response(sockfd);
        }
        if (!proceed) return -1;

        close(sockfd);

        return 0;
}
