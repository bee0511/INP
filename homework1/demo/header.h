#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <libgen.h>
#include <locale.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void urlDecode(const char* url, char* decoded);
char* extractFilePath(const char* path);
int createServerSocket();
void handle200Response(int client_fd, char* full_path);
void handle301Response(int client_fd, char* file_path);
void handle403Response(int client_fd);
void handle404Response(int client_fd);
void handle501Response(int client_fd);

void sendHTTPResponse(int client_fd, const struct HttpResponse* response);
void handleHTTPRequest(int client_fd, const char* request);

#endif