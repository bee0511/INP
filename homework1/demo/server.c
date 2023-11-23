#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <iconv.h>
#include <langinfo.h>
#include <libgen.h>
#include <locale.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_CONNECTIONS 10  // Adjust as needed
#define MAX_EVENTS 10

#define errquit(m) \
    {              \
        perror(m); \
        exit(-1);  \
    }

void handle_request(int client_socket);

// URL-decode function
int url_decode(const char *encoded, char *decoded, size_t decoded_size) {
    size_t i, j;
    for (i = 0, j = 0; i < strlen(encoded) && j < decoded_size - 1; ++i, ++j) {
        if (encoded[i] == '%' && i + 2 < strlen(encoded)) {
            int value;
            if (sscanf(encoded + i + 1, "%02x", &value) == 1) {
                decoded[j] = (char)value;
                i += 2;
            } else {
                return -1;  // Invalid percent encoding
            }
        } else if (encoded[i] == '+') {
            decoded[j] = ' ';
        } else {
            decoded[j] = encoded[i];
        }
    }
    decoded[j] = '\0';
    return 0;
}

int main(int argc, char *argv[]) {
    int server_socket, client_socket;
    struct sockaddr_in sin, csin;
    socklen_t csinlen = sizeof(csin);

    if ((server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        errquit("socket");

    do {
        int v = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v)) < 0)
            errquit("setsockopt");
    } while (0);

    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(80);

    if (bind(server_socket, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        errquit("bind");

    if (listen(server_socket, SOMAXCONN) < 0)
        errquit("listen");

    int max_fd = server_socket;
    fd_set active_fds, read_fds;
    FD_ZERO(&active_fds);
    FD_SET(server_socket, &active_fds);

    while (1) {
        read_fds = active_fds;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
            errquit("select");

        for (int i = 0; i <= max_fd; ++i) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_socket) {
                    // New connection
                    if ((client_socket = accept(server_socket, (struct sockaddr *)&csin, &csinlen)) < 0) {
                        perror("accept");
                        continue;
                    }

                    if (client_socket >= FD_SETSIZE) {
                        fprintf(stderr, "Too many connections. Connection limit: %d\n", FD_SETSIZE);
                        close(client_socket);
                        continue;
                    }

                    FD_SET(client_socket, &active_fds);
                    max_fd = (client_socket > max_fd) ? client_socket : max_fd;
                } else {
                    // Data to read on existing connection
                    handle_request(i);

                    // Shutdown the socket for further sends and receives
                    if (shutdown(i, SHUT_RDWR) < 0) {
                        perror("shutdown");
                    }

                    // Close the file descriptor
                    if (close(i) < 0) {
                        perror("close");
                    }

                    // Remove the socket from the active set
                    FD_CLR(i, &active_fds);
                }
            }
        }
    }

    return 0;
}

void handle_request(int client_socket) {
    setlocale(LC_ALL, "en_US.UTF-8");
    char buf[4096];
    FILE *fp;
    char method[10], path[255];

    if ((fp = fdopen(client_socket, "r+")) == NULL) {
        perror("fdopen");
        return;
    }

    setvbuf(fp, NULL, _IONBF, 0);

    // Read the request line
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fclose(fp);
        return;
    }

    // Parse the request line
    sscanf(buf, "%s %s", method, path);

    // Ignore additional request parameters (query string)
    strtok(path, "?");

    // URL-decode the path
    char decoded_path[255];
    if (url_decode(path, decoded_path, sizeof(decoded_path)) != 0) {
        // URL decoding failed
        fprintf(fp, "HTTP/1.0 400 Bad Request\r\n\r\n");
        fprintf(fp, "Connection: close\r\n");  // Indicate that the connection will be closed
        fprintf(fp, "\r\n");                   // End of headers
        fflush(fp);                            // Flush the FILE* to ensure the response is sent
        return;
    }

    // Construct the full file path by appending "/html" to the decoded path
    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "html%s", decoded_path);

    // Debug print to check the decoded full_path
    // printf("full_path: %s\n", full_path);

    // Handle only GET requests
    if (strcmp(method, "GET") != 0) {
        // Unsupported HTTP method
        fprintf(fp, "HTTP/1.0 501 Not Implemented\r\n\r\n");
        fprintf(fp, "Connection: close\r\n");  // Indicate that the connection will be closed
        fprintf(fp, "\r\n");                   // End of headers
        fflush(fp);                            // Flush the FILE* to ensure the response is sent
        return;
    }
    // If the requested path is a directory without a trailing slash, redirect
    struct stat stat_buf;
    if (stat(full_path, &stat_buf) == 0 && S_ISDIR(stat_buf.st_mode) && path[strlen(path) - 1] != '/') {
        // Redirect to the directory with a trailing slash
        fprintf(fp, "HTTP/1.0 301 Moved Permanently\r\nLocation: %s/\r\n\r\n", path);
        fprintf(fp, "Connection: close\r\n");  // Indicate that the connection will be closed
        fprintf(fp, "\r\n");                   // End of headers
        fflush(fp);                            // Flush the FILE* to ensure the response is sent
        return;
    }

    // If the requested path is a directory and there is no index.html file, return 403 Forbidden
    if (stat(full_path, &stat_buf) == 0 && S_ISDIR(stat_buf.st_mode) && path[strlen(path) - 1] == '/') {
        // Check if there is an index.html file in the directory
        char index_path[4096 + sizeof("/index.html")];
        snprintf(index_path, sizeof(index_path), "%s/index.html", full_path);
        if (access(index_path, F_OK) != 0) {
            // No index.html file, return 403 Forbidden
            fprintf(fp, "HTTP/1.0 403 Forbidden\r\n");
            fprintf(fp, "Content-Type: text/html\r\n");
            fprintf(fp, "Connection: close\r\n");
            fprintf(fp, "\r\n");  // End of headers

            // Custom HTML message for 403 Forbidden
            fprintf(fp, "<html>\n");
            fprintf(fp, " <head>\n");
            fprintf(fp, "  <title>403 Forbidden</title>\n");
            fprintf(fp, " </head>\n");
            fprintf(fp, " <body>\n");
            fprintf(fp, "  <h1>403 Forbidden</h1>\n");
            fprintf(fp, " </body>\n");
            fprintf(fp, "</html>\n");

            fflush(fp);  // Flush the FILE* to ensure the response is sent

            return;
        }
    }

    // If the requested path is the root ("/") without a trailing slash, check for "index.html"
    if (strcmp(decoded_path, "/") == 0) {
        char temp_full_path[4096];  // Temporary buffer for snprintf result

        // Use snprintf to ensure null-termination
        int snprintf_result = snprintf(temp_full_path, sizeof(temp_full_path), "%s/index.html", full_path);

        // Check if "index.html" exists
        if (snprintf_result > 0 && snprintf_result < sizeof(temp_full_path) &&
            access(temp_full_path, F_OK) == 0) {
            // "index.html" found, update full_path
            strncpy(full_path, temp_full_path, sizeof(full_path) - 1);
            full_path[sizeof(full_path) - 1] = '\0';
        }
    }

    // Open the file for reading
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        // File not found
        fprintf(fp, "HTTP/1.0 404 Not Found\r\n");
        fprintf(fp, "Content-Type: text/html\r\n");
        fprintf(fp, "Connection: close\r\n");
        fprintf(fp, "\r\n");  // End of headers

        // Custom XML message for 404 Not Found
        fprintf(fp, "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n");
        fprintf(fp, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n");
        fprintf(fp, "         \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
        fprintf(fp, "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n");
        fprintf(fp, " <head>\n");
        fprintf(fp, "  <title>404 Not Found</title>\n");
        fprintf(fp, " </head>\n");
        fprintf(fp, " <body>\n");
        fprintf(fp, "  <h1>404 Not Found</h1>\n");
        fprintf(fp, " </body>\n");
        fprintf(fp, "</html>\n");

        fflush(fp);  // Flush the FILE* to ensure the response is sent
        return;
    }

    // Determine the MIME type based on the file extension
    char *mime_type = "text/plain";
    char *file_extension = strrchr(full_path, '.');
    if (file_extension != NULL) {
        if (strcmp(file_extension, ".html") == 0)
            mime_type = "text/html";
        else if (strcmp(file_extension, ".jpg") == 0)
            mime_type = "image/jpeg";
        else if (strcmp(file_extension, ".png") == 0)
            mime_type = "image/png";
        else if (strcmp(file_extension, ".mp3") == 0)
            mime_type = "audio/mpeg";
    }

    // Determine the size of the file
    off_t file_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);  // Reset file position to the beginning
                                  // Send the HTTP headers
    fprintf(fp, "HTTP/1.0 200 OK\r\n");
    fprintf(fp, "Content-Length: %lld\r\n", (long long)file_size);
    fprintf(fp, "Content-Type: %s; charset=utf-8\r\n", mime_type);
    fprintf(fp, "\r\n");
    fflush(fp);

    // Read the entire file content into a buffer
    char *file_content = (char *)malloc(file_size);
    if (file_content == NULL) {
        perror("Error allocating memory for file content");
        // Handle memory allocation error
        return;
    }

    ssize_t total_bytes_read = 0;
    while (total_bytes_read < file_size) {
        ssize_t bytes_read = read(file_fd, file_content + total_bytes_read, file_size - total_bytes_read);
        if (bytes_read <= 0) {
            // Handle read error or end of file
            perror("Error reading file");
            free(file_content);
            return;
        }
        total_bytes_read += bytes_read;
    }

    // Use fwrite to send the entire file content to the client via fp
    if (fwrite(file_content, 1, file_size, fp) != file_size) {
        // Handle write error
        perror("Error writing file content to client");
    }

    // Free the allocated memory
    free(file_content);
}