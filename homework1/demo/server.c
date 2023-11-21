#include <arpa/inet.h>
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

#define MAX_EVENTS 10

#define errquit(m) \
    {              \
        perror(m); \
        exit(-1);  \
    }

void handle_request(int client_socket);

void set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        errquit("fcntl");

    flags |= O_NONBLOCK;

    if (fcntl(fd, F_SETFL, flags) == -1)
        errquit("fcntl");
}

ssize_t send_non_blocking(int socket_fd, const void *buffer, size_t length) {
    ssize_t total_sent = 0;

    while (total_sent < length) {
        ssize_t result = send(socket_fd, buffer + total_sent, length - total_sent, MSG_DONTWAIT);

        if (result == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // The operation would block, try again later
                continue;
            } else {
                // Log the error and continue sending
                perror("send_non_blocking");
                return -1;  // or handle the error in a way that makes sense for your application
            }
        } else if (result == 0) {
            // Connection closed by the other end
            break;
        }

        total_sent += result;
    }

    return total_sent;
}

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
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
    } while (0);

    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(80);

    if (bind(server_socket, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        errquit("bind");

    if (listen(server_socket, SOMAXCONN) < 0)
        errquit("listen");

    set_non_blocking(server_socket);

    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
        errquit("epoll_create1");

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;  // EPOLLET for edge-triggered mode
    event.data.fd = server_socket;

    // Add server_socket to epoll
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1)
        errquit("epoll_ctl");

    struct epoll_event events[MAX_EVENTS];
    while (1) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1)
            errquit("epoll_wait");

        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.fd == server_socket) {
                // New connection
                if ((client_socket = accept(server_socket, (struct sockaddr *)&csin, &csinlen)) < 0) {
                    perror("accept");
                    continue;
                }

                // Set client socket to non-blocking
                set_non_blocking(client_socket);

                // Add client_socket to epoll
                event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP;  // EPOLLET for edge-triggered mode
                event.data.fd = client_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &event) == -1)
                    errquit("epoll_ctl");

            } else if (events[i].events & EPOLLIN) {  // Data to read on existing connection
                // handle_request should be modified to handle non-blocking reads
                handle_request(events[i].data.fd);

                /* check if the connection is closing */
                if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
                    // Shutdown the socket for further sends and receives
                    if (shutdown(events[i].data.fd, SHUT_RDWR) == -1 && errno != ENOTCONN) {
                        perror("shutdown");
                    }
                    // Close the file descriptor
                    if (close(events[i].data.fd) == -1 && errno != EBADF) {
                        perror("close");
                    }
                    // Remove the socket from epoll
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL) == -1 && errno != ENOENT) {
                        perror("epoll_ctl");
                    }

                    printf("[+] connection closed\n");
                }
            } else {
                printf("[+] unexpected\n");
            }
        }
    }

    close(server_socket);

    return 0;
}

void handle_request(int client_socket) {
    setlocale(LC_ALL, "en_US.UTF-8");
    char buf[4096];
    FILE *fp;
    char method[10], path[255];

    if ((fp = fdopen(client_socket, "r+")) == NULL) {
        perror("fdopen");
        close(client_socket);
        return;
    }

    setvbuf(fp, NULL, _IONBF, 0);

    // Read the request line
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fclose(fp);
        close(client_socket);
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
        fclose(fp);                            // Close the FILE*
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

        // Close the FILE*
        fclose(fp);
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

        // Close the FILE*
        fclose(fp);
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

            // Close the FILE*
            fclose(fp);
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

        // Close the FILE*
        fclose(fp);
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
    printf("[*] Sending %lld bytes file...", (long long)file_size);
    // Send the HTTP headers
    fprintf(fp, "HTTP/1.0 200 OK\r\n");
    fprintf(fp, "Content-Type: %s; charset=utf-8\r\n", mime_type);
    fprintf(fp, "Content-Length: %lld\r\n", (long long)file_size);
    fprintf(fp, "Connection: close\r\n");
    fprintf(fp, "\r\n");

    // Send the file content with non-blocking sockets
    while (1) {
        ssize_t bytes_read = read(file_fd, buf, sizeof(buf));
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data available at the moment, continue with other tasks
                break;
            } else {
                // Handle other read errors
                perror("read");
                break;
            }
        } else if (bytes_read == 0) {
            // End of file
            break;
        }

        ssize_t bytes_sent = send_non_blocking(client_socket, buf, bytes_read);
        if (bytes_sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // The socket buffer is full, try again later
                continue;
            } else {
                // Handle other send errors
                perror("send_non_blocking");
                break;
            }
        } else if (bytes_sent < bytes_read) {
            // Not all data was sent, adjust buffer position for the next iteration
            memmove(buf, buf + bytes_sent, bytes_read - bytes_sent);
        }
    }
    fflush(fp);

    // Close the FILE* and the file descriptor
    fclose(fp);
    close(file_fd);
}