#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <libgen.h>
#include <locale.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define INITIAL_CAPACITY 10

#define errquit(m) \
    {              \
        perror(m); \
        exit(-1);  \
    }

#ifndef HEADER_H_
#define HEADER_H_

struct HttpResponse {
    int StatusCode;
    const char* StatusDescription;
    const char* Content;
    const char* ContentType;
    const char* Location;
    size_t ContentLength;
};

struct ClientInfo {
    int socket;
    pthread_t thread;
    SSL_CTX* ssl_context;
    SSL* ssl_connection;
};

void urlDecode(const char* url, char* decoded);
char* extractFilePath(const char* path);
int createServerSocket(int port);
struct HttpResponse get200Response(int client_fd, char* full_path);
struct HttpResponse get301Response(int client_fd, char* file_path);
struct HttpResponse get403Response(int client_fd);
struct HttpResponse get404Response(int client_fd);
struct HttpResponse get501Response(int client_fd);

void sendHTTPResponse(struct ClientInfo* client_info, const struct HttpResponse* response);
void handleHTTPRequest(struct ClientInfo*, const char* request);

#endif