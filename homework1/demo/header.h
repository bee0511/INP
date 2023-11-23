#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
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

#define MAX_CLIENTS 64

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
    size_t ContentLength;  // for 200
};

int hexToInteger(char c);
void urlDecode(const char* url, char* decoded);
char* extractFilePath(const char* path);
int createServerSocket();
void handle200Response(int client_fd, char* full_path);
void handle301Response(int client_fd, char* file_path);
void handle403Response(int client_fd);
void handle404Response(int client_fd);
void handle501Response(int client_fd);

void sendHTTPResponse(int client_fd, const struct HttpResponse* response);
void handleGetRequest(int client_fd, const char* request);

#endif