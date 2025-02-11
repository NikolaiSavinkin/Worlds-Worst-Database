#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

#include "dbproto.h"

void _print_bin(void *data, size_t size) {
        uint8_t *bin = (uint8_t *)data;
        for (size_t i = 0; i < size; i += 1) {
                if (i && i % 16 == 0) printf("\n");
                printf("%02x ", bin[i]);
        }
        printf("\n");
}

void dbproto_buf_init(dbproto_buf_t *buf) {
        buf->len = 0;
        buf->offset = 0;
        memset(buf->body, 0, sizeof(buf->body[0]) * BUFF_SIZE);
}

void dbproto_print_buf_body(dbproto_buf_t *buf) {
        _print_bin(buf->body, buf->len);
}

int dbproto_pack(dbproto_buf_t *buf, void *data, size_t size) {
        static size_t new_offset;
        new_offset = buf->offset + size;
        if (BUFF_SIZE < new_offset) {
                errno = ERANGE;
                return -1;
        }

        memcpy(buf->body + buf->offset, data, size);
        buf->offset = (uint16_t) new_offset;
        buf->len += size;
        return 0;
}

int dbproto_unpack(dbproto_buf_t *buf, void *out, size_t size) {
        static size_t new_offset;
        new_offset = buf->offset + size;
        if (BUFF_SIZE < new_offset) {
                errno = ERANGE;
                return -1;
        }
        
        memcpy(out, buf->body + buf->offset, size);
        buf->offset = (uint16_t) new_offset;
        return 0;
}

int dbproto_seek(dbproto_buf_t *buf, int32_t offset, dbproto_whence_e whence) {
        static int32_t new_offset;
        switch (whence) {
        case DPB_SEEK_SET:
                new_offset = offset;
                break;
        case DPB_SEEK_CUR:
                new_offset = buf->offset + offset;
                break;
        case DPB_SEEK_END:
                new_offset = (BUFF_SIZE - 1) + offset;
                break;
        default:
                errno = EINVAL;
                return -1;
        }

        if (new_offset < 0 || BUFF_SIZE <= new_offset) {
                errno = ERANGE;
                return -1;
        }

        buf->offset = (uint16_t) new_offset;
        return 0;
}

dbproto_buf_t dbproto_new_buf(const dbproto_type_e type,
                const uint16_t len) {
        dbproto_buf_t buf;
        dbproto_buf_init(&buf);

        dbproto_hdr_t hdr = {0};
        hdr.type = htonl(type);
        hdr.len = htons(len);

        dbproto_pack(&buf, &hdr, sizeof(hdr));
        return buf;
}

ssize_t dbproto_send(int fd, dbproto_buf_t *buf) {
        return send(fd, buf->body, buf->len, 0);
}

ssize_t dbproto_recv(int fd, dbproto_buf_t *buf) {
        return recv(fd, buf->body, BUFF_SIZE, 0);
}
