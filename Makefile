CC = gcc
DEBUG = -g -ggdb
CFLAGS = -O2 -std=c99 -fPIC -Wall -Werror $(DEBUG)

LIB_OBJS = \
	./lib/command.o \
	./lib/crc32.o \
	./lib/storage.o

SHARED_LIB = ./libkv.so

$(SHARED_LIB): $(LIB_OBJS)
	$(CC) -shared -Wl,--export-dynamic $(LIB_OBJS) -o $(SHARED_LIB)

server:
	$(CC) $(DEBUG) -o server server.c $(SHARED_LIB)

clean:
	rm -f ./lib/*.o
	rm -f ./libkv.so
	rm -f ./server

.PHONY: clean server
