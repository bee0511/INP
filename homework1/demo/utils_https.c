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

struct HttpResponse get501Response(int client_fd) {
    struct HttpResponse response;
    response.StatusCode = 501;
    response.StatusDescription = "Not Implemented";
    response.Content = "<html><body><h1>501 Not Implemented</h1></body></html>";
    response.ContentType = "text/html";
    response.Location = "";
    response.ContentLength = strlen(response.Content);
    return response;
}

struct HttpResponse get404Response(int client_fd) {
    struct HttpResponse response;
    response.StatusCode = 404;
    response.StatusDescription = "Not Found";
    response.Content = "<html><body><h1>404 Not Found</h1></body></html>";
    response.ContentType = "text/html";
    response.Location = "";
    response.ContentLength = strlen(response.Content);
    return response;
}

struct HttpResponse get301Response(int client_fd, char* file_path) {
    size_t redirect_url_size = strlen(file_path) + 2;  // 1 for the '/', 1 for the null terminator
    struct HttpResponse response;

    // Allocate memory for the redirect_url buffer
    char* redirect_url = malloc(redirect_url_size);
    if (redirect_url != NULL) {
        strcpy(redirect_url, file_path);
        strcat(redirect_url, "/");
    }
    response.StatusCode = 301;
    response.StatusDescription = "Moved Permanently";
    response.Content = "";
    response.ContentType = "text/html";
    response.Location = redirect_url;
    response.ContentLength = 0;
    return response;
}

struct HttpResponse get403Response(int client_fd) {
    struct HttpResponse response;
    response.StatusCode = 403;
    response.StatusDescription = "Forbidden";
    response.Content = "<html><body><h1>403 Forbidden</h1></body></html>";
    response.ContentType = "text/html";
    response.Location = "";
    response.ContentLength = strlen(response.Content);
    return response;
}

struct HttpResponse get200Response(int client_fd, char* full_path) {
    setlocale(LC_ALL, "en_US.UTF-8");

    if (strcmp(full_path, "html/") == 0)
        strcpy(full_path, "html/index.html");
    // Open and read the file
    FILE* file = fopen(full_path, "rb");
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    // Get file content and read size
    char* file_content = (char*)malloc(file_size);
    size_t read_size = fread(file_content, 1, file_size, file);

    const char* mime_type = "text/plain";
    if (strstr(full_path, ".html"))
        mime_type = "text/html";
    else if (strstr(full_path, ".jpg"))
        mime_type = "image/jpeg";
    else if (strstr(full_path, ".mp3"))
        mime_type = "audio/mpeg";
    else if (strstr(full_path, ".png"))
        mime_type = "image/png";

    struct HttpResponse response;
    response.StatusCode = 200;
    response.StatusDescription = "OK";
    response.Content = file_content;
    response.ContentType = mime_type;
    response.Location = "";
    response.ContentLength = read_size;

    fclose(file);
    return response;
}

void handleHTTPRequest(struct ClientInfo* client_info, const char* request) {
    char method[10];
    char path[256];
    struct HttpResponse response;

    sscanf(request, "%9s %255s", method, path);
    if (strcmp(method, "GET") != 0) {
        response = get501Response(client_info->socket);
        sendHTTPResponse(client_info, &response);
        return;
    }

    char* file_path = extractFilePath(path);
    char full_path[270];
    snprintf(full_path, sizeof(full_path), "html%s", file_path);

    struct stat path_stat;

    if (stat(full_path, &path_stat) != 0) {
        response = get404Response(client_info->socket);
        sendHTTPResponse(client_info, &response);
        return;
    }

    if (S_ISDIR(path_stat.st_mode)) {  // If it is a directory, check 301 and 403
        if (full_path[strlen(full_path) - 1] != '/') {
            response = get301Response(client_info->socket, file_path);
            sendHTTPResponse(client_info, &response);
            return;
        }
        // Check for the existence of index.html
        char index_path[300];
        snprintf(index_path, sizeof(index_path), "%s/index.html", full_path);
        if (access(index_path, F_OK) == -1) {
            response = get403Response(client_info->socket);
            sendHTTPResponse(client_info, &response);
            return;
        }
    }

    response = get200Response(client_info->socket, full_path);
    sendHTTPResponse(client_info, &response);
    return;
}

void sendHTTPResponse(struct ClientInfo* client_info, const struct HttpResponse* response) {
    SSL* ssl = client_info->ssl_connection;
    int client_fd = client_info->socket;

    setlocale(LC_ALL, "en_US.UTF-8");
    size_t response_length;
    ssize_t written;

    if (ssl) {  // If SSL is enabled, use SSL_write
        if (response->StatusCode == 301) {
            const char* response_format_redirect =
                "HTTP/1.0 %d %s\r\n"
                "Content-Length: %zu\r\n"
                "Content-Type: %s\r\n"
                "Location: %s\r\n"
                "\r\n";
            response_length = snprintf(NULL, 0, response_format_redirect,
                                       response->StatusCode,
                                       response->StatusDescription,
                                       response->ContentLength,
                                       response->ContentType,
                                       response->Location);
            char* full_response = malloc(response_length + 1);
            snprintf(full_response, response_length + 1, response_format_redirect,
                     response->StatusCode,
                     response->StatusDescription,
                     response->ContentLength,
                     response->ContentType,
                     response->Location);

            written = SSL_write(ssl, full_response, response_length);
            if (written <= 0) {
                int ssl_error = SSL_get_error(ssl, written);
                if (ssl_error == SSL_ERROR_ZERO_RETURN || ssl_error == SSL_ERROR_SYSCALL) {
                    // Connection closed or error, handle accordingly
                } else {
                    // Handle other SSL errors
                    ERR_print_errors_fp(stderr);
                }
            }
            free(full_response);
        } else {
            const char* response_format =
                "HTTP/1.0 %d %s\r\n"
                "Content-Length: %zu\r\n"
                "Content-Type: %s; charset=utf-8\r\n"
                "\r\n";
            response_length = snprintf(NULL, 0, response_format,
                                       response->StatusCode,
                                       response->StatusDescription,
                                       response->ContentLength,
                                       response->ContentType);
            char* full_response = malloc(response_length + 1);
            snprintf(full_response, response_length + 1, response_format,
                     response->StatusCode,
                     response->StatusDescription,
                     response->ContentLength,
                     response->ContentType);

            written = SSL_write(ssl, full_response, response_length);
            if (written <= 0) {
                int ssl_error = SSL_get_error(ssl, written);
                if (ssl_error == SSL_ERROR_ZERO_RETURN || ssl_error == SSL_ERROR_SYSCALL) {
                    // Connection closed or error, handle accordingly
                } else {
                    // Handle other SSL errors
                    ERR_print_errors_fp(stderr);
                }
            }
            // If it's not a redirect, also write the content
            written = SSL_write(ssl, response->Content, response->ContentLength);
            if (written < 0) {
                int ssl_error = SSL_get_error(ssl, written);
                if (ssl_error == SSL_ERROR_ZERO_RETURN || ssl_error == SSL_ERROR_SYSCALL) {
                    // Connection closed or error, handle accordingly
                } else {
                    // Handle other SSL errors
                    ERR_print_errors_fp(stderr);
                }
            }
            free(full_response);
        }
    } else {  // If SSL is not enabled, use write
        if (response->StatusCode == 301) {
            const char* response_format_redirect =
                "HTTP/1.0 %d %s\r\n"
                "Content-Length: %zu\r\n"
                "Content-Type: %s\r\n"
                "Location: %s\r\n"
                "\r\n";
            response_length = snprintf(NULL, 0, response_format_redirect,
                                       response->StatusCode,
                                       response->StatusDescription,
                                       response->ContentLength,
                                       response->ContentType,
                                       response->Location);
            char* full_response = malloc(response_length + 1);
            snprintf(full_response, response_length + 1, response_format_redirect,
                     response->StatusCode,
                     response->StatusDescription,
                     response->ContentLength,
                     response->ContentType,
                     response->Location);

            written = write(client_fd, full_response, response_length);
            if (written < 0) {
                // Handle the error, for example:
                perror("[Error] HTTP write");
                // Additional error handling as needed
            }

            free(full_response);
        } else {
            const char* response_format =
                "HTTP/1.0 %d %s\r\n"
                "Content-Length: %zu\r\n"
                "Content-Type: %s; charset=utf-8\r\n"
                "\r\n";
            response_length = snprintf(NULL, 0, response_format,
                                       response->StatusCode,
                                       response->StatusDescription,
                                       response->ContentLength,
                                       response->ContentType);
            char* full_response = malloc(response_length + 1);
            snprintf(full_response, response_length + 1, response_format,
                     response->StatusCode,
                     response->StatusDescription,
                     response->ContentLength,
                     response->ContentType);

            written = write(client_fd, full_response, response_length);
            if (written < 0) {
                // Handle the error, for example:
                perror("[Error] HTTP write not301, first");
                // Additional error handling as needed
            }

            // If it's not a redirect, also write the content
            written = write(client_fd, response->Content, response->ContentLength);
            if (written < 0) {
                // Handle the error, for example:
                perror("[Error] HTTP write not301, second");
                // Additional error handling as needed
            }

            free(full_response);
        }
    }

    return;
}