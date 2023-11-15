#include <fcntl.h>
#include <iconv.h>
#include <libgen.h>
#include <locale.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define errquit(m) \
    {              \
        perror(m); \
        exit(-1);  \
    }

void *handle_connection(void *arg);
void handle_request(int client_socket);
int url_decode(const char *encoded, char *decoded, size_t decoded_size);

int main(int argc, char *argv[]) {
    int s;
    struct sockaddr_in sin;

    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        errquit("socket");

    do {
        int v = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
    } while (0);

    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(80);

    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        errquit("bind");

    if (listen(s, SOMAXCONN) < 0)
        errquit("listen");

    while (1) {
        int c;
        struct sockaddr_in csin;
        socklen_t csinlen = sizeof(csin);

        if ((c = accept(s, (struct sockaddr *)&csin, &csinlen)) < 0) {
            perror("accept");
            continue;
        }

        // Create a new thread for each connection
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_connection, (void *)(intptr_t)c) != 0) {
            perror("pthread_create");
            close(c);
        }

        // Detach the thread to avoid memory leaks
        pthread_detach(tid);
    }

    return 0;
}

void *handle_connection(void *arg) {
    int client_socket = (intptr_t)arg;
    handle_request(client_socket);
    pthread_exit(NULL);
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

    // Print debug information
    printf("Received request: %s", buf);

    // Parse the request line
    sscanf(buf, "%s %s", method, path);

    // Ignore additional request parameters (query string)
    strtok(path, "?");

    // Convert URL-encoded characters to UTF-8
    char decoded_path[255];
    if (url_decode(path, decoded_path, sizeof(decoded_path)) != 0) {
        // URL decoding failed
        fprintf(fp, "HTTP/1.0 400 Bad Request\r\n\r\n");
        fclose(fp);
        close(client_socket);
        return;
    }

    // Construct the full file path by appending "/html" to the requested path
    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "html%s", decoded_path);

    // Handle only GET requests
    if (strcmp(method, "GET") != 0) {
        // Unsupported HTTP method
        fprintf(fp, "HTTP/1.0 501 Not Implemented\r\n\r\n");
        fclose(fp);
        close(client_socket);
        return;
    }

    // If the requested path is a directory without a trailing slash, redirect
    struct stat stat_buf;
    if (stat(full_path, &stat_buf) == 0 && S_ISDIR(stat_buf.st_mode) && path[strlen(path) - 1] != '/') {
        fprintf(fp, "HTTP/1.0 301 Moved Permanently\r\nLocation: %s/\r\n\r\n", path);
        fclose(fp);
        close(client_socket);
        return;
    }

    // If the requested path is a directory, append the default index.html
    if (S_ISDIR(stat_buf.st_mode)) {
        strncat(full_path, "/index.html", sizeof(full_path) - strlen(full_path) - 1);
    }

    // Open the file for reading
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        // File not found
        fprintf(fp, "HTTP/1.0 404 Not Found\r\n\r\n");
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

    // Send the file content
    while (1) {
        ssize_t bytes_read = read(file_fd, buf, sizeof(buf));
        if (bytes_read <= 0)
            break;
        fwrite(buf, 1, bytes_read, fp);
    }

    // Close file and socket
    close(file_fd);
    fclose(fp);
    close(client_socket);
}

// Function to URL-decode a string
int url_decode(const char *encoded, char *decoded, size_t decoded_size) {
    size_t i = 0, j = 0;

    iconv_t cd = iconv_open("UTF-8", "UTF-8");

    if (cd == (iconv_t)-1) {
        // Failed to initialize iconv
        return -1;
    }

    while (i < strlen(encoded) && j < decoded_size - 1) {
        if (encoded[i] == '%' && i + 2 < strlen(encoded)) {
            // Decode URL-encoded character
            char hex[3] = {encoded[i + 1], encoded[i + 2], '\0'};
            unsigned int value;
            sscanf(hex, "%x", &value);

            char utf8[4];
            utf8[0] = value & 0xFF;
            utf8[1] = (value >> 8) & 0xFF;
            utf8[2] = (value >> 16) & 0xFF;
            utf8[3] = '\0';

            char *inbuf = utf8;
            size_t inbytesleft = 3;
            size_t outbytesleft = 4;

            // Use a temporary variable for the output buffer
            char temp[4];
            char *outbuf = temp;

            if (iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)-1) {
                iconv_close(cd);
                // Failed to convert character
                return -1;
            }

            // Copy the converted character to the final buffer
            for (size_t k = 0; k < 4 - outbytesleft; ++k) {
                decoded[j++] = temp[k];
            }

            i += 3;
        } else if (encoded[i] == '+') {
            // Replace '+' with space
            decoded[j++] = ' ';
            i++;
        } else {
            // Copy the character as is
            decoded[j++] = encoded[i++];
        }
    }

    // Null-terminate the decoded string
    decoded[j] = '\0';

    iconv_close(cd);

    return 0;
}