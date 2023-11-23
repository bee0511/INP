
CC		= gcc
PROGS	= server

all: server

%.o: %.c
	$(CC) -c -g -Wall $<

server: server.o
	$(CC) -o $@ -static server.c utils.c

clean:
	rm -f *.o $(PROGS)
