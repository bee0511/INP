
CC		= gcc
PROGS	= server

all: server utils

%.o: %.c
	$(CC) -c -g -Wall $<

server: server.o
	$(CC) -o $@ -static server.c utils.c

utils: utils.o
	$(CC) -o server -static server.c utils.c

clean:
	rm -f *.o $(PROGS)
