#include "header.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
size_t client_capacity = INITIAL_CAPACITY;
size_t num_clients = 0;

int main() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    // Create SSL context for HTTPS
    SSL_CTX *https_ssl_context = SSL_CTX_new(SSLv23_server_method());
    if (!https_ssl_context) {
        perror("SSL_CTX_new");
        exit(EXIT_FAILURE);
    }

    // Load server certificate and private key
    if (SSL_CTX_use_certificate_file(https_ssl_context, "/cert/server.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(https_ssl_context, "/cert/server.key", SSL_FILETYPE_PEM) <= 0) {
        perror("SSL_CTX_use_certificate_file / SSL_CTX_use_PrivateKey_file");
        exit(EXIT_FAILURE);
    }

    // Create HTTP server socket
    int http_server_fd = createServerSocket(80, NULL);

    // Create HTTPS server socket
    int https_server_fd = createServerSocket(443, https_ssl_context);

    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // Register the HTTP server socket for epoll events
    struct epoll_event http_event;
    http_event.events = EPOLLIN | EPOLLET;  // Edge-triggered mode
    http_event.data.fd = http_server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, http_server_fd, &http_event) == -1) {
        perror("epoll_ctl (HTTP)");
        exit(EXIT_FAILURE);
    }

    // Register the HTTPS server socket for epoll events
    struct epoll_event https_event;
    https_event.events = EPOLLIN | EPOLLET;  // Edge-triggered mode
    https_event.data.fd = https_server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, https_server_fd, &https_event) == -1) {
        perror("epoll_ctl (HTTPS)");
        exit(EXIT_FAILURE);
    }

    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == http_server_fd) {
                // Handle new HTTP connection
                int client_fd = accept(http_server_fd, NULL, NULL);
                if (client_fd < 0) {
                    perror("[Error] http accept");
                    continue;
                }

                struct ClientInfo client_info = {.socket = client_fd, .ssl_connection = NULL, .ssl_context = NULL};
                char request_buffer[BUFFER_SIZE];
                ssize_t bytes_received;
                bytes_received = read(client_info.socket, request_buffer, sizeof(request_buffer));
                if (bytes_received == -1) {
                    perror("HTTP read");
                }
                // Process the received data
                char decoded_buffer[BUFFER_SIZE];
                urlDecode(request_buffer, decoded_buffer);
                handleHTTPRequest(&client_info, decoded_buffer);

                if (close(client_info.socket) == -1) {
                    perror("[Error] closing socket");
                }

            } else if (events[i].data.fd == https_server_fd) {
                // Handle new HTTPS connection
                int client_fd = accept(https_server_fd, NULL, NULL);
                if (client_fd < 0) {
                    perror("[Error] https accept");
                    continue;
                }

                SSL *ssl_connection = SSL_new(https_ssl_context);
                SSL_set_fd(ssl_connection, client_fd);

                if (SSL_accept(ssl_connection) <= 0) {
                    // Handle SSL handshake error
                    ERR_print_errors_fp(stderr);
                    close(client_fd);
                    SSL_free(ssl_connection);
                    continue;
                }

                struct ClientInfo client_info = {.socket = client_fd, .ssl_connection = ssl_connection, .ssl_context = https_ssl_context};
                char request_buffer[BUFFER_SIZE];
                size_t bytes_received;
                bytes_received = SSL_read(client_info.ssl_connection, request_buffer, sizeof(request_buffer));
                if (bytes_received < 0) {
                    perror("[Error] SSL receive");
                }
                // Process the received data
                char decoded_buffer[BUFFER_SIZE];
                urlDecode(request_buffer, decoded_buffer);
                handleHTTPRequest(&client_info, decoded_buffer);

                // Graceful shutdown
                if (SSL_shutdown(ssl_connection) == 0) {
                    // Incomplete shutdown, perform a clean shutdown
                    SSL_shutdown(ssl_connection);
                }
                SSL_free(ssl_connection);

                // Close the client socket
                close(client_fd);
            }
        }
    }

    // Clean up
    close(epoll_fd);
    close(http_server_fd);
    close(https_server_fd);
    SSL_CTX_free(https_ssl_context);
    return 0;
}
