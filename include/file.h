#ifndef FILE_H
#define FILE_H

int create_db_file(char *filename);
int open_db_file(char *filename);
int close_db_file(int fd);

#endif
