CC = gcc
DEFS = -D_XOPEN_SOURCE=500 -D_BSD_SOURCE
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)
LDFLAGS =
.PHONY: all clean

all: server client

server: server.o
	$(CC) $(LDFLAGS) -o $@ $^
client: client.o
	$(CC) $(LDFLAGS) -o $@ $^
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

server.o : server.c

client.o : client.c

clean:
	rm -f $(OBJECTFILES) main
