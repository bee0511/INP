#include "header.h"

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
