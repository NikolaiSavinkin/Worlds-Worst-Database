#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>

#include "dbproto.h"
#include "parse.h"

static  int _parse_int(int *i, const char *str) {
        const long l = strtol(str, NULL, 10);
        if (errno == EINVAL || errno == ERANGE) {
                return STATUS_ERROR;
        }
        if (l > INT_MAX) {
                errno = ERANGE;
                return STATUS_ERROR;
        }

        *i = (int) l;
        return STATUS_SUCCESS;
}

static inline bool _has_name(
        const employee_t employee,
        const char *name
) {
        return !strcmp(employee.name, name);
}

static inline void _delete_employee(
        dbheader_t *dbhdr,
        employee_t **employees,
        const int index
) {
        /* decrement count */
        dbhdr->count -= 1;

        /* overwrite employee to be deleted with last employee
         * (assuming order doesn't matter)
         * */
        memcpy(&(*employees)[index],
                &(*employees)[dbhdr->count],
                sizeof(employee_t));

        return;
}

int update_employees(
        dbheader_t *dbhdr,
        employee_t *employees,
        char *update_string
) {

        const char *name = strtok(update_string, ",");
        if (name == NULL) {
                errno = EINVAL;
                return STATUS_ERROR;
        }

        const char *hours_str = strtok(NULL, ",");
        if (hours_str == NULL) {
                errno = EINVAL;
                return STATUS_ERROR;
        }

        int hours = 0;
        if (_parse_int(&hours, hours_str) == STATUS_ERROR) {
                return STATUS_ERROR;
        }

        int num_updated = 0;
        for (int i = 0; i < dbhdr->count; i += 1) {
                if (_has_name(employees[i], name)) {
                        employees[i].hours = hours;
                        num_updated += 1;
                }
        }

        return num_updated;
}

intmax_t delete_employees(
        dbheader_t *dbhdr,
        employee_t **employees,
        char *name
) {

        /* Loop through employees
         * if name matches, delete employee at index
         * else increment index
         * */
        int num_deleted = 0;
        int i = 0;
        while (i < dbhdr->count) {
                if (_has_name((*employees)[i], name)) {
                        _delete_employee(dbhdr, employees, i);
                        num_deleted += 1;
                } else {
                        i += 1;
                }
        }

        /* TODO: reallocate memory
         * (since this is now a long running process)
         * */
        return num_deleted;
}

void list_employees(
        dbheader_t *dbhdr,
        employee_t *employees
) {
        printf("ID\tName\tAddress\tHours\n");

        for (int i = 0; i < dbhdr->count; i += 1) {
                printf("%d\t%s\t%s\t%d\n",
                        i,
                        employees[i].name,
                        employees[i].address,
                        employees[i].hours);
        }

        return;
}

int add_employee(
        dbheader_t *dbhdr,
        employee_t **employees,
        char *add_string
) {
        errno = 0;

        char *name = strtok(add_string, ",");
        if (name == NULL) {
                errno = EINVAL;
                return STATUS_ERROR;
        }

        char *address = strtok(NULL, ",");
        if (address == NULL) {
                errno = EINVAL;
                return STATUS_ERROR;
        }

        char *hours_str = strtok(NULL, ",");
        if (hours_str == NULL) {
                errno = EINVAL;
                return STATUS_ERROR;
        }

        int hours = 0;
        if (_parse_int(&hours, hours_str) == STATUS_ERROR) {
                return STATUS_ERROR;
        }

        const int tail = dbhdr->count;
        dbhdr->count += 1;
        dbhdr->filesize += sizeof(employee_t);

        *employees = realloc(*employees, dbhdr->count * sizeof(employee_t));

        strncpy((*employees)[tail].name, name, sizeof((*employees)[0].name));
        strncpy((*employees)[tail].address, address,
                        sizeof((*employees)[0].address));
        (*employees)[tail].hours = hours;

        return STATUS_SUCCESS;
}

int read_employees(
        int fd,
        dbheader_t *dbhdr,
        employee_t **employeesOut
) {
        errno = 0;
        if (fd < 0) {
                errno = EBADF;
                return STATUS_ERROR;
        }
        
        if (lseek(fd, sizeof(dbheader_t), SEEK_SET) == -1) {
                return STATUS_ERROR;
        }

        const int count = dbhdr->count;
        employee_t *employees = calloc(
                count,
                sizeof(employee_t)
        );
        if (employees ==  NULL) {
                return STATUS_ERROR;
        }

        const ssize_t result = read(
                fd,
                employees,
                sizeof(employee_t) * count
        );
        if ( result < 0) {
                free(employees);
                return STATUS_ERROR;
        } else if ((unsigned long) result < sizeof(employee_t) * count) {
                printf("File corrupt, header expected more data\n");
                free(employees);
                return STATUS_ERROR;
        }

        for (int i = 0; i < count; i += 1) {
                employees[i].hours = ntohl(employees[i].hours);
        }

        *employeesOut = employees;
        return STATUS_SUCCESS;
}

int output_file(
        int fd,
        dbheader_t *dbhdr,
        employee_t *employees
) {
        errno = 0;
        if (fd < 0) {
                errno = EBADF;
                return STATUS_ERROR;
        }

        /* resize file in case employees are removed */
        if (ftruncate(fd, dbhdr->filesize) == -1) {
                return STATUS_ERROR;
        }

        if (lseek(fd, 0, SEEK_SET) == -1) {
                return STATUS_ERROR;
        }

        const int count = dbhdr->count;

        /* Writing header */
        dbhdr->magic = htonl(dbhdr->magic);
        dbhdr->version = htons(dbhdr->version);
        dbhdr->count = htons(dbhdr->count);
        dbhdr->filesize = htonl(dbhdr->filesize);

        if (write(fd, dbhdr, sizeof(dbheader_t)) == -1) {
                return STATUS_ERROR;
        }

        /* Revert endian to avoid side effects */
        dbhdr->magic = ntohl(dbhdr->magic);
        dbhdr->version = ntohs(dbhdr->version);
        dbhdr->count = ntohs(dbhdr->count);
        dbhdr->filesize = ntohl(dbhdr->filesize);

        /* Writing employees */
        for (int i = 0; i < count; i += 1) {
                employees[i].hours = htonl(employees[i].hours);
                if (write(fd, &employees[i], sizeof(employee_t))
                        == -1) {
                        return STATUS_ERROR;
                }
                /* Revert endian to avoid side effects */
                employees[i].hours = ntohl(employees[i].hours);
        }

        return STATUS_SUCCESS;
}	

int validate_db_header(
        int fd,
        dbheader_t **headerOut
) {
        errno = 0;
        if (fd < 0) {
                errno = EBADF;
                return STATUS_ERROR;
        }

        if (lseek(fd, 0, SEEK_SET) == -1) {
                return STATUS_ERROR;
        }

        dbheader_t *header = calloc(1, sizeof(dbheader_t));
        if (header ==  NULL) {
                return STATUS_ERROR;
        }
        
        /* Set and check errno for read
         * because it can technically succeed without
         * reading a full header
         * */
        if ((read(fd, header, sizeof(dbheader_t)))
                != sizeof(dbheader_t)) {
                if (errno == 0) {
                        printf("Incomplete header\n");
                } 
                free(header);
                return STATUS_ERROR;
        }

        header->magic = ntohl(header->magic);
        header->version = ntohs(header->version);
        header->count = ntohs(header->count);
        header->filesize = ntohl(header->filesize);

        if (header->magic != HEADER_MAGIC) {
                printf("Invalid magic number in header\n");
                free(header);
                return STATUS_ERROR;
        }

        if (header->version != 1) {
                printf("Unsupported header version\n");
                free(header);
                return STATUS_ERROR;
        }

        struct stat stat_buff = {0};
        if (fstat(fd, &stat_buff) == -1) {
                free(header);
                return STATUS_ERROR;
        }

        if (header->filesize != stat_buff.st_size) {
                printf("Incorrect file size in header\n");
                free(header);
                return STATUS_ERROR;
        }

        *headerOut = header;
        return STATUS_SUCCESS;
}

int create_db_header(
        int fd,
        dbheader_t **headerOut
) {
	dbheader_t *header = calloc(1, sizeof(dbheader_t));
        if (header == NULL) {
                return STATUS_ERROR;
        }

        header->magic = HEADER_MAGIC;
        header->version = 0x1;
        header->count = 0;
        header->filesize = sizeof(dbheader_t);

        *headerOut = header;
        return STATUS_SUCCESS;
}


