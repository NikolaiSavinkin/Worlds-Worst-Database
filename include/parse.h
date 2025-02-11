#ifndef PARSE_H
#define PARSE_H

#include <stdint.h>

#define HEADER_MAGIC 0x4c4c4144
#define EMPLOYEE_STR_SIZE 128

typedef struct {
	unsigned int magic;
	unsigned short version;
	unsigned short count;
	unsigned int filesize;
} dbheader_t;

typedef struct {
	char name[EMPLOYEE_STR_SIZE];
	char address[EMPLOYEE_STR_SIZE];
	unsigned int hours;
} employee_t;

int create_db_header(
        int fd,
        dbheader_t **headerOut
);
int validate_db_header(
        int fd,
        dbheader_t **headerOut
);
int read_employees(
        int fd,
        dbheader_t *,
        employee_t **employeesOut
);
int output_file(
        int fd,
        dbheader_t *,
        employee_t *employees
);
void list_employees(
        dbheader_t *dbhdr,
        employee_t *employees
);
int add_employee(
        dbheader_t *dbhdr,
        employee_t **employees,
        char *add_string
);

intmax_t delete_employees(
        dbheader_t *dbhdr,
        employee_t **employees,
        char *name
);
int update_employees(
        dbheader_t *dbhdr,
        employee_t *employees,
        char *update_string
);

#endif
