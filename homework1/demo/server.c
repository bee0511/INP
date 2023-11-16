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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

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

// Function to set a socket to non-blocking mode
int set_non_blocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        return -1;
    }

    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        return -1;
    }

    return 0;
}

// Function to send data using non-blocking sockets
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
                perror("send");
                fprintf(stderr, "Error sending data to client: %s\n", strerror(errno));

                // You might choose to break the loop or return an error code based on your requirements
                // break;
                // return -1;
            }
        } else if (result == 0) {
            // Connection closed by the other end
            break;
        }

        total_sent += result;
    }

    return total_sent;
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

                    FD_SET(client_socket, &active_fds);
                    max_fd = (client_socket > max_fd) ? client_socket : max_fd;
                } else {
                    // Data to read on existing connection
                    handle_request(i);

                    // Shutdown the socket for further sends and receives
                    shutdown(i, SHUT_RDWR);

                    // Close the file descriptor
                    close(i);

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
    char buf[8192];
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
        close(client_socket);                  // Close the socket
        return;
    }

    // Construct the full file path by appending "/html" to the decoded path
    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "html%s", decoded_path);

    // Debug print to check the decoded full_path
    printf("full_path: %s\n", full_path);

    // Handle only GET requests
    if (strcmp(method, "GET") != 0) {
        // Unsupported HTTP method
        fprintf(fp, "HTTP/1.0 501 Not Implemented\r\n\r\n");
        fprintf(fp, "Connection: close\r\n");  // Indicate that the connection will be closed
        fprintf(fp, "\r\n");                   // End of headers
        fflush(fp);                            // Flush the FILE* to ensure the response is sent

        // Shutdown the socket for further sends and receives
        shutdown(client_socket, SHUT_RDWR);

        // Close the FILE* and the socket
        fclose(fp);
        close(client_socket);
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

        // Shutdown the socket for further sends and receives
        shutdown(client_socket, SHUT_RDWR);

        // Close the FILE* and the socket
        fclose(fp);
        close(client_socket);
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

            // Shutdown the socket for further sends and receives
            shutdown(client_socket, SHUT_RDWR);

            // Close the FILE* and the socket
            fclose(fp);
            close(client_socket);
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

        // Shutdown the socket for further sends and receives
        shutdown(client_socket, SHUT_RDWR);

        // Close the FILE* and the socket
        fclose(fp);
        close(client_socket);
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

    // Send the HTTP headers
    fprintf(fp, "HTTP/1.0 200 OK\r\n");
    fprintf(fp, "Content-Type: %s; charset=utf-8\r\n", mime_type);
    fprintf(fp, "\r\n");

    // Set the client socket to non-blocking mode
    if (set_non_blocking(client_socket) == -1) {
        // Handle error
        perror("set_non_blocking");
        fclose(fp);
        close(file_fd);
        close(client_socket);
        return;
    }

    // Send the file content
    while (1) {
        ssize_t bytes_read = read(file_fd, buf, sizeof(buf));
        if (bytes_read <= 0) {
            // End of file or error
            break;
        }

        ssize_t bytes_sent = send_non_blocking(client_socket, buf, bytes_read);

        if (bytes_sent < 0) {
            // Handle send error
            perror("send_non_blocking");

            // Log the error
            fprintf(stderr, "Error sending data to client: %s\n", strerror(errno));

            // You might choose to continue or break the loop based on your requirements
            break;
        }
    }

    // Flush the FILE* to ensure all data is written
    fflush(fp);

    // Shutdown the socket for further sends and receives
    shutdown(client_socket, SHUT_RDWR);

    // Close the FILE* and the file descriptor
    fclose(fp);
    close(file_fd);

    // Close the socket after ensuring all data is sent
    close(client_socket);
}