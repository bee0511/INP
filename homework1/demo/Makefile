
CC		= gcc
PROGS	= server

all: server utils

%.o: %.c
	$(CC) -c -g -Wall $<

server: server.o
	gcc -o server -O3 -flto -static server.c utils.c -lpthread -lssl -lcrypto

utils: utils.o
	gcc -o server -O3 -flto -static server.c utils.c -lpthread -lssl -lcrypto

clean:
	rm -f *.o $(PROGS)
