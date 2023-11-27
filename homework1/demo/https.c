#include "header.h"

void handleHTTPSRequest(struct ClientInfo* client_info, const char* request) {
    char method[10];
    char path[256];
    struct HttpResponse response;

    sscanf(request, "%9s %255s", method, path);
    if (strcmp(method, "GET") != 0) {
        response = get501Response(client_info->socket);
        sendHTTPSResponse(client_info, &response);
        return;
    }

    char* file_path = extractFilePath(path);
    char full_path[270];
    snprintf(full_path, sizeof(full_path), "html%s", file_path);

    struct stat path_stat;

    if (stat(full_path, &path_stat) != 0) {
        response = get404Response(client_info->socket);
        sendHTTPSResponse(client_info, &response);
        return;
    }

    if (S_ISDIR(path_stat.st_mode)) {  // If it is a directory, check 301 and 403
        if (full_path[strlen(full_path) - 1] != '/') {
            response = get301Response(client_info->socket, file_path);
            sendHTTPSResponse(client_info, &response);
            return;
        }
        // Check for the existence of index.html
        char index_path[300];
        snprintf(index_path, sizeof(index_path), "%s/index.html", full_path);
        if (access(index_path, F_OK) == -1) {
            response = get403Response(client_info->socket);
            sendHTTPSResponse(client_info, &response);
            return;
        }
    }

    response = get200Response(client_info->socket, full_path);
    sendHTTPSResponse(client_info, &response);
    return;
}

void sendHTTPSResponse(struct ClientInfo* client_info, const struct HttpResponse* response) {
    SSL* ssl = client_info->ssl_connection;
    int client_fd = client_info->socket;

    setlocale(LC_ALL, "en_US.UTF-8");
    size_t response_length;
    ssize_t written;

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

    return;
}
