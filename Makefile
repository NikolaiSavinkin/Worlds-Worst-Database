OPTIONS = -Wextra -std=c99 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=500 -ggdb

TARGET_SRV = bin/dbserver
TARGET_CLI = bin/dbclient

SRC_SRV = $(wildcard src/srv/*.c)
SRC_CLI = $(wildcard src/cli/*.c)
SRC_COMMON = $(wildcard src/common/*.c)

OBJ_SRV = $(SRC_SRV:src/srv/%.c=obj/srv/%.o)
OBJ_CLI = $(SRC_CLI:src/cli/%.c=obj/cli/%.o)
OBJ_COMMON = $(SRC_COMMON:src/common/%.c=obj/%.o)

run: clean build
	./$(TARGET_SRV) -f ./mynewdb.db -n -p 8080

build: $(TARGET_SRV) $(TARGET_CLI)

clean:
	rm -f obj/*.o
	rm -f obj/srv/*.o
	rm -f obj/cli/*.o
	rm -f bin/*
	rm -f *.db

$(TARGET_SRV): $(OBJ_SRV) $(OBJ_COMMON)
	gcc -o $@ $?

$(TARGET_CLI): $(OBJ_CLI) $(OBJ_COMMON)
	gcc -o $@ $?

$(OBJ_SRV): obj/srv/%.o: src/srv/%.c
	gcc $(OPTIONS) -c $< -o $@ -Iinclude

$(OBJ_CLI): obj/cli/%.o: src/cli/%.c
	gcc $(OPTIONS) -c $< -o $@ -Iinclude

$(OBJ_COMMON): obj/%.o: src/common/%.c
	gcc $(OPTIONS) -c $< -o $@ -Iinclude
