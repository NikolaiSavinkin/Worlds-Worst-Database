#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "file.h"
#include "dbproto.h"
static void _handle_open_error(char *filename);

static void _handle_open_error(char *filename) {
        switch(errno) {
                case EEXIST:
                        printf("File \"%s\" already exists\n",
                                        filename);
                        break;
                case ENOENT:
                        printf("File \"%s\" does not exist\n",
                                        filename);
                        break;
                case EACCES:
                        printf("Permission denied\n");
                        break;
                default:
                        perror("open");
        }
        return;
}

int create_db_file(char *filename) {
        int fd = open(filename,
                        O_CREAT | O_EXCL  | O_RDWR,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (fd == -1) {
                _handle_open_error(filename);
                return STATUS_ERROR;
        }
        
        return fd;
}

int open_db_file(char *filename) {
        int fd = open(filename,
                        O_RDWR,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (fd == -1) {
                _handle_open_error(filename);
                return STATUS_ERROR;
        }
        
        return fd;
}

int close_db_file(int fd) {
        if (close(fd) == -1) {
                return STATUS_ERROR;
        }
        return STATUS_SUCCESS;
}
