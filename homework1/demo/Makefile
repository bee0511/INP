
CC		= gcc
PROGS	= server

all: server https

%.o: %.c
	$(CC) -c -g -Wall $<

server: server.o
	gcc -o server -O3 -flto -static server.c utils.c -lpthread

https: server_https.o
	gcc -o server_https -O3 -flto -static server_https.c utils_https.c http.c https.c response.c -lpthread -lssl -lcrypto

clean:
	rm -f *.o $(PROGS)
