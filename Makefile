CC=gcc
CFLAGS=-g -O2 -Wall
LDLIBS=-lpthread

all: webserver

webserver: webserver.c

clean:
	rm -rf *.o *~ *.dSYM webserver

