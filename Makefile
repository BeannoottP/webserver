CC=gcc
CFLAGS=-g -O2 -Wall
LDLIBS=-lpthread

all: server

server: server.c request.c
	$(CC) $(CFLAGS) server.c $(LDLIBS) -o $@

clean:
	rm -rf *.o *~ *.dSYM server

