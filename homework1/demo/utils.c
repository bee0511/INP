#include "header.h"

int hexToInteger(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    return -1;  // Invalid hex digit
}

void urlDecode(const char* url, char* decoded) {
    size_t len = strlen(url);
    size_t decoded_pos = 0;

    for (size_t i = 0; i < len; i++) {
        if (url[i] == '%' && i + 2 < len && ((url[i + 1] >= '0' && url[i + 1] <= '9') || (url[i + 1] >= 'A' && url[i + 1] <= 'F') || (url[i + 1] >= 'a' && url[i + 1] <= 'f')) && ((url[i + 2] >= '0' && url[i + 2] <= '9') || (url[i + 2] >= 'A' && url[i + 2] <= 'F') || (url[i + 2] >= 'a' && url[i + 2] <= 'f'))) {
            int value = (hexToInteger(url[i + 1]) << 4) | hexToInteger(url[i + 2]);
            decoded[decoded_pos++] = (char)value;
            i += 2;
        } else
            decoded[decoded_pos++] = url[i];
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

void handle501Response(int client_fd) {
    struct HttpResponse response =
        {
            501,
            "Not Implemented",
            "<html><body><h1>501 Not Implemented</h1></body></html>",
            "text/html",
            0};
    sendHTTPResponse(client_fd, &response);
    return;
}

void handle404Response(int client_fd) {
    struct HttpResponse response =
        {
            404,
            "Not Found",
            "<html><body><h1>404 Not Found</h1></body></html>",
            "text/html",
            0};
    sendHTTPResponse(client_fd, &response);
    return;
}

void handle301Response(int client_fd, char* file_path) {
    size_t redirect_url_size = strlen(file_path) + 2;  // 1 for the '/', 1 for the null terminator

    // Allocate memory for the redirect_url buffer
    char* redirect_url = malloc(redirect_url_size);
    if (redirect_url != NULL) {
        strcpy(redirect_url, file_path);
        strcat(redirect_url, "/");
        struct HttpResponse response =
            {
                301,
                "Moved Permanently",
                redirect_url,
                "text/html",
                0};
        sendHTTPResponse(client_fd, &response);
        free(redirect_url);
    }
    return;
}

void handle403Response(int client_fd) {
    struct HttpResponse response =
        {
            403,
            "Forbidden",
            "<html><body><h1>403 Forbidden</h1></body></html>",
            "text/html",
            0};
    sendHTTPResponse(client_fd, &response);
    return;
}

void handle200Response(int client_fd, char* full_path) {
    // Open and read the file
    if (strcmp(full_path, "html/") == 0)
        strcpy(full_path, "html/index.html");
    FILE* file = fopen(full_path, "rb");
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
        sendHTTPResponse(client_fd, &response);

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

    sendHTTPResponse(client_fd, &response);

    fclose(file);
    free(full_content);
}

void handleGetRequest(int client_fd, const char* request) {
    char method[10];
    char path[1024];
    sscanf(request, "%9s %255s", method, path);
    if (strcmp(method, "GET") != 0) {
        handle501Response(client_fd);
        return;
    }

    char* file_path = extractFilePath(path);
    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "html%s", file_path);

    struct stat path_stat;

    if (stat(full_path, &path_stat) != 0) {
        handle404Response(client_fd);
        return;
    }

    if (S_ISDIR(path_stat.st_mode)) {  // If it is a directory, check 301 and 403
        if (full_path[strlen(full_path) - 1] != '/') {
            handle301Response(client_fd, file_path);
            return;
        }
        // Check for the existence of index.html
        char index_path[4500];
        snprintf(index_path, sizeof(index_path), "%s/index.html", full_path);
        if (access(index_path, F_OK) == -1)  // 403 OKAY
        {
            handle403Response(client_fd);
            return;
        }
    }

    handle200Response(client_fd, full_path);
    return;
}

void sendHTTPResponse(int client_fd, const struct HttpResponse* response) {
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

    time_t now = time(0);
    struct tm* timeinfo = gmtime(&now);
    char date_str[80];
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);

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
    return;
}
