#ifndef DBPROTO_H
#define DBPROTO_H

#include <sys/types.h>
#include <stdint.h>


#define STATUS_ERROR   -1
#define STATUS_SUCCESS 0

#define PORTSTRLEN 5
#define BUFF_SIZE 4096
#define MSG_SIZE 512

#define PROTO_VERSION 100

typedef enum {
        MSG_ERROR_RES,
        MSG_HELLO_REQ,
        MSG_HELLO_RES,
        MSG_EMPLOYEE_LIST_REQ,
        MSG_EMPLOYEE_LIST_RES,
        MSG_EMPLOYEE_ADD_REQ,
        MSG_EMPLOYEE_ADD_RES,
        MSG_EMPLOYEE_DEL_REQ,
        MSG_EMPLOYEE_DEL_RES,
} dbproto_type_e;

typedef struct {
        uint16_t len;
        uint16_t offset;
        uint8_t body[BUFF_SIZE];
} dbproto_buf_t;

typedef enum {
        DPB_SEEK_SET,
        DPB_SEEK_CUR,
        DPB_SEEK_END,
} dbproto_whence_e;

typedef struct {
        dbproto_type_e type;
        uint16_t len;
} dbproto_hdr_t;

typedef struct {
        uint16_t version;
} dbproto_hello_req_t;

typedef struct {
        uint16_t version;
} dbproto_hello_res_t;

typedef struct {
        uint16_t len;
        uint8_t msg[MSG_SIZE];
} dbproto_err_res_t;

typedef struct {
        uint16_t len;
        uint8_t add_str[MSG_SIZE];
} dbproto_employee_add_req_t;

typedef struct {
        uint16_t len;
        uint8_t name[MSG_SIZE];
} dbproto_employee_del_req_t;

typedef struct {
        intmax_t num;
} dbproto_del_res_t;

typedef struct {
        uint16_t len;
        uint8_t employee_str[MSG_SIZE];
} dbproto_employee_t;

void dbproto_buf_init(dbproto_buf_t *buf);

void dbproto_print_buf_body(dbproto_buf_t *buf);

/* The pack/unpack functions are to give the buffer a similar API to files
 * but it might be better to use built in functions like stpncpy()
 * */
int dbproto_pack(dbproto_buf_t *buf, void *data, size_t size);

int dbproto_unpack(dbproto_buf_t *buf, void *out, size_t size);

int dbproto_seek(dbproto_buf_t *buf, int32_t offset, dbproto_whence_e whence);

dbproto_buf_t dbproto_new_buf(const dbproto_type_e type,
                const uint16_t len);

ssize_t dbproto_send(int fd, dbproto_buf_t *buf);

ssize_t dbproto_recv(int fd, dbproto_buf_t *buf);

#endif
