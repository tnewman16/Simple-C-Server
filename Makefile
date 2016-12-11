CC=gcc
CFLAGS=-c -Wall -g

all: server

server: server.o server_graphics.o
	$(CC) server.o server_graphics.o -o server -lpthread -lX11

server.o: server.c
	$(CC) $(CFLAGS) server.c

server_graphics.o: server_graphics.c
	$(CC) $(CFLAGS) server_graphics.c

clean:
	/bin/rm -f server *.o *.gz

run:
	./server 8080

tarball:
	tar -cvzf newman.tar.gz *