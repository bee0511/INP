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

#define MAX_CLIENTS 32

#define errquit(m) \
    {              \
        perror(m); \
        exit(-1);  \
    }

int hex_to_int(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    return -1;  // Invalid hex digit
}

void url_decode(const char* url, char* decoded) {
    size_t len = strlen(url);
    size_t decoded_pos = 0;

    for (size_t i = 0; i < len; i++) {
        if (url[i] == '%' && i + 2 < len && ((url[i + 1] >= '0' && url[i + 1] <= '9') || (url[i + 1] >= 'A' && url[i + 1] <= 'F') || (url[i + 1] >= 'a' && url[i + 1] <= 'f')) && ((url[i + 2] >= '0' && url[i + 2] <= '9') || (url[i + 2] >= 'A' && url[i + 2] <= 'F') || (url[i + 2] >= 'a' && url[i + 2] <= 'f'))) {
            int value = (hex_to_int(url[i + 1]) << 4) | hex_to_int(url[i + 2]);
            decoded[decoded_pos++] = (char)value;
            i += 2;
        } else
            decoded[decoded_pos++] = url[i];
    }
    // Null-terminate the decoded string
    decoded[decoded_pos] = '\0';
}

const char* extract_file_path(const char* path) {
    const char* question_mark = strchr(path, '?');
    size_t path_length = question_mark ? (size_t)(question_mark - path) : strlen(path);

    char* file_path = malloc(path_length + 1);
    if (!file_path) return NULL;
    strncpy(file_path, path, path_length);
    file_path[path_length] = '\0';

    return file_path;
}

struct HttpResponse {
    int StatusCode;
    const char* StatusDescription;
    const char* Content;
    const char* ContentType;
    size_t ContentLength;  // for 200
};

void send_http_response(int client_fd, const struct HttpResponse* response) {
    time_t now = time(0);
    struct tm* timeinfo = gmtime(&now);
    char date_str[80];
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);

    const char* response_format =
        "HTTP/1.0 %d %s\r\n"
        "Date: %s\r\n"
        "Server: MyServer\r\n"
        "Content-Length: %zu\r\n"
        "Content-Type: %s\r\n"
        "\r\n"
        "%s";

    const char* response_format_redirect =
        "HTTP/1.0 %d %s\r\n"
        "Date: %s\r\n"
        "Server: MyServer\r\n"
        "Location: %s\r\n"
        "Content-Length: 0\r\n"
        "Content-Type: %s\r\n\r\n";

    size_t response_length;
    if (response->StatusCode == 301) {
        response_length = snprintf(NULL, 0, response_format_redirect,
                                   response->StatusCode, response->StatusDescription, date_str,
                                   response->Content, response->ContentType);
    } else if (response->StatusCode == 200 && strcmp(response->ContentType, "text/html") != 0) {
        response_length = snprintf(NULL, 0, response_format,
                                   response->StatusCode, response->StatusDescription, date_str,
                                   response->ContentLength, response->ContentType, "");
    } else {
        response_length = snprintf(NULL, 0, response_format,
                                   response->StatusCode, response->StatusDescription, date_str,
                                   strlen(response->Content), response->ContentType, response->Content);
    }

    char* full_response = malloc(response_length + 1);
    if (full_response == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return;
    }
    if (response->StatusCode == 301) {
        snprintf(full_response, response_length + 1, response_format_redirect,
                 response->StatusCode, response->StatusDescription, date_str,
                 response->Content, response->ContentType);
    } else if (response->StatusCode == 200 && strcmp(response->ContentType, "text/html") != 0) {
        snprintf(full_response, response_length + 1, response_format,
                 response->StatusCode, response->StatusDescription, date_str,
                 response->ContentLength, response->ContentType, "");
    } else {
        snprintf(full_response, response_length + 1, response_format,
                 response->StatusCode, response->StatusDescription, date_str,
                 strlen(response->Content), response->ContentType, response->Content);
    }

    dprintf(client_fd, "%s", full_response);
    if (response->StatusCode == 200 && strcmp(response->ContentType, "text/html") != 0) write(client_fd, response->Content, response->ContentLength);
    free(full_response);
}

void handle_get_request(int client_fd, const char* request) {
    char method[10];
    char path[1024];
    sscanf(request, "%9s %255s", method, path);
    if (strcmp(method, "GET") != 0)  // 501 OKAY
    {
        struct HttpResponse response =
            {
                501,
                "Not Implemented",
                "<html><body><h1>501 Not Implemented</h1></body></html>",
                "text/html",
                0};
        send_http_response(client_fd, &response);
    } else  // GET request
    {
        const char* file_path = extract_file_path(path);  // remove ?
        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "html%s", file_path);

        struct stat path_stat;
        if (stat(full_path, &path_stat) == 0)  // find file
        {
            if (S_ISDIR(path_stat.st_mode))  // is a directory
            {
                if (full_path[strlen(full_path) - 1] != '/')  // 301 missing / OKAY
                {
                    size_t redirect_url_size = strlen(file_path) + 2;  // 1 for the '/', 1 for the null terminator

                    // Allocate memory for the redirect_url buffer
                    char* redirect_url = malloc(redirect_url_size);
                    if (redirect_url != NULL)  // 301 OKAY
                    {
                        strcpy(redirect_url, file_path);
                        strcat(redirect_url, "/");
                        struct HttpResponse response =
                            {
                                301,
                                "Moved Permanently",
                                redirect_url,
                                "text/html",
                                0};
                        send_http_response(client_fd, &response);

                        free(redirect_url);
                    }
                    return;
                }
                // Check for the existence of index.html
                char index_path[4500];
                snprintf(index_path, sizeof(index_path), "%s/index.html", full_path);
                if (access(index_path, F_OK) == -1)  // 403 OKAY
                {
                    struct HttpResponse response =
                        {
                            403,
                            "Forbidden",
                            "<html><body><h1>403 Forbidden</h1></body></html>",
                            "text/plain",
                            0};
                    send_http_response(client_fd, &response);
                    return;
                }
            }
        } else  // 404 OKAY
        {
            struct HttpResponse response =
                {
                    404,
                    "Not Found",
                    "<html><body><h1>404 Not Found</h1></body></html>",
                    "text/plain",
                    0};
            send_http_response(client_fd, &response);
            return;
        }

        // Open and read the file
        if (strcmp(full_path, "html/") == 0) strcpy(full_path, "html/index.html");
        FILE* file = fopen(full_path, "rb");
        if (file)  // 200
        {
            const char* mime_type = "application/octet-stream";
            if (strstr(full_path, ".html") || strstr(full_path, ".txt")) {
                mime_type = "text/html";
                fseek(file, 0, SEEK_END);
                long file_size = ftell(file);
                rewind(file);
                char* file_content = malloc(file_size + 1);
                size_t bytesRead = fread(file_content, 1, file_size, file);
                file_content[bytesRead] = '\0';
                struct HttpResponse response =
                    {
                        200,
                        "OK",
                        file_content,
                        mime_type,
                        0};
                send_http_response(client_fd, &response);

                // Clean up
                free(file_content);
                fclose(file);
                return;
            } else if (strstr(full_path, ".jpg"))
                mime_type = "image/jpeg";
            else if (strstr(full_path, ".mp3"))
                mime_type = "audio/mpeg";
            else if (strstr(full_path, ".png"))
                mime_type = "image/png";

            fseek(file, 0, SEEK_END);
            long file_size = ftell(file);
            rewind(file);
            char* full_content = (char*)malloc(file_size);
            size_t read_size = fread(full_content, 1, file_size, file);

            struct HttpResponse response =
                {
                    200,
                    "OK",
                    full_content,
                    mime_type,
                    read_size};

            send_http_response(client_fd, &response);

            fclose(file);
            free(full_content);

            return;
        }
    }
}

int createServerSocket() {
    int server_fd;
    struct sockaddr_in sin;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) errquit("socket");

    int v = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));

    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(80);

    if (bind(server_fd, (struct sockaddr*)&sin, sizeof(sin)) < 0) errquit("bind");
    if (listen(server_fd, SOMAXCONN) < 0) errquit("listen");
    return server_fd;
}

int main(int argc, char* argv[]) {
    int server_fd = createServerSocket();
    int client_fds[MAX_CLIENTS], max_fd;  // max 10

    fd_set read_fds;
    for (int i = 0; i < MAX_CLIENTS; ++i) client_fds[i] = -1;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        max_fd = server_fd;

        for (int i = 0; i < 10; i++) {
            if (client_fds[i] != -1) {
                FD_SET(client_fds[i], &read_fds);
                max_fd = (client_fds[i] > max_fd) ? client_fds[i] : max_fd;
            }
        }

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(server_fd, &read_fds)) {
            int client_fd = accept(server_fd, NULL, NULL);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_fds[i] == -1) {
                    client_fds[i] = client_fd;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] != -1 && FD_ISSET(client_fds[i], &read_fds)) {
                char request_buffer[4096];
                ssize_t bytes_received = recv(client_fds[i], request_buffer, sizeof(request_buffer), 0);

                if (bytes_received > 0) {
                    char decoded_buffer[4096];
                    url_decode(request_buffer, decoded_buffer);
                    handle_get_request(client_fds[i], decoded_buffer);
                } else {
                    close(client_fds[i]);
                    client_fds[i] = -1;
                }
            }
        }
    }
    close(server_fd);
    return 0;
}
