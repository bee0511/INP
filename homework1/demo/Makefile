
CC		= gcc
PROGS	= server

all: server

%.o: %.c
	$(CC) -c -g -Wall $<

server: server.o
	gcc -o server -O3 -flto -static server.c utils.c http.c https.c response.c -lpthread -lssl -lcrypto

clean:
	rm -f *.o $(PROGS)
