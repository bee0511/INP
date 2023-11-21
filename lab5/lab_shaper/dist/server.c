#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define PORT 12345
#define BUFFER_SIZE 1024 * 1024 * 2  // 2MB


int main() {
    int sockfd, connfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t clilen = sizeof(cliaddr);

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("server socket fail\n");
        return 0;
    }

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt failed\n");
        return 0;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("server bind fail\n");
        return 0;
    }

    // Listen for incoming connections
    if (listen(sockfd, MAX_CLIENTS * 10) == -1) {
        perror("Socket listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        if ((connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen)) < 0) {
            perror("server accept fail\n");
            return 0;
        }

        // ----- delay -------
        char ans[] = "server";
        send(connfd, ans, strlen(ans), 0);

        char buffer[BUFFER_SIZE];
        memset(buffer, 'a', sizeof(buffer));
        for (int i = 0; i < 2 ; i++)
            send(connfd, buffer, sizeof(buffer), 0);

        close(connfd);
    }

    close(sockfd);

    return 0;
}
