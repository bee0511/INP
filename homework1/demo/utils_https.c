#include "header_https.h"

void urlDecode(const char* url, char* decoded) {
    size_t len = strlen(url);
    size_t decoded_pos = 0;

    for (size_t i = 0; i < len; i++) {
        if (url[i] == '%' && i + 2 < len && isxdigit(url[i + 1]) && isxdigit(url[i + 2])) {
            char hex[3] = {url[i + 1], url[i + 2], '\0'};
            int value = (int)strtol(hex, NULL, 16);
            decoded[decoded_pos++] = (char)value;
            i += 2;
        } else {
            decoded[decoded_pos++] = url[i];
        }
    }
    // Null-terminate the decoded string
    decoded[decoded_pos] = '\0';
}
char* extractFilePath(const char* path) {
    const char* question_mark = strchr(path, '?');
    size_t path_length = question_mark ? (size_t)(question_mark - path) : strlen(path);

    char* file_path = malloc(path_length + 1);
    if (!file_path) return NULL;
    strncpy(file_path, path, path_length);
    file_path[path_length] = '\0';

    return file_path;
}

int createServerSocket(int port, SSL_CTX* ssl_ctx) {
    int server_fd;
    struct sockaddr_in sin;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int v = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));

    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        fprintf(stderr, "Error binding to port %d\n", port);
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (port == 443) {
        // HTTPS configuration
        if (listen(server_fd, SOMAXCONN) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        // Return the server file descriptor
        return server_fd;
    } else {
        // HTTP configuration
        if (listen(server_fd, SOMAXCONN) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        // Return the server file descriptor
        return server_fd;
    }
}