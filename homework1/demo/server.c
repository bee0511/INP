#include "header.h"

#define MAX_EVENTS 10

struct ClientInfo *client_infos;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
size_t client_capacity = INITIAL_CAPACITY;
size_t num_clients = 0;

void cleanup_ssl(struct ClientInfo *client_info) {
    SSL_free(client_info->ssl_connection);
    SSL_CTX_free(client_info->ssl_context);
}

// Function to handle an incoming client
void handle_client(struct ClientInfo *client_info) {
    SSL *ssl = client_info->ssl_connection;

    char request_buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    if (ssl) {
        // HTTPS connection
        bytes_received = SSL_read(ssl, request_buffer, sizeof(request_buffer));
    } else {
        // HTTP connection
        bytes_received = read(client_info->socket, request_buffer, sizeof(request_buffer));
        if (bytes_received == -1) {
            perror("HTTP read");
        }
    }

    // Process the received data
    char decoded_buffer[BUFFER_SIZE];
    urlDecode(request_buffer, decoded_buffer);
    handleHTTPRequest(client_info, decoded_buffer);

    // Graceful shutdown
    if (ssl) {
        // HTTPS connection
        if (SSL_shutdown(ssl) == 0) {
            // Incomplete shutdown, perform a clean shutdown
            SSL_shutdown(ssl);
        }
        cleanup_ssl(client_info);
    }

    // Error message for close operation
    if (close(client_info->socket) == -1) {
        perror("[Error] closing socket");
    }
    // free(client_info);
    return;
}

int main() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    // Create HTTP server socket
    int http_server_fd = createServerSocket(80);
    client_infos = malloc(sizeof(struct ClientInfo) * client_capacity);

    // Create HTTPS server socket
    int https_server_fd = createServerSocket(443);

    // Load SSL certificate and key
    SSL_CTX *https_ssl_context = SSL_CTX_new(SSLv23_server_method());
    if (SSL_CTX_use_certificate_file(https_ssl_context, "/cert/server.crt", SSL_FILETYPE_PEM) <= 0) {
        perror("SSL_CTX_use_certificate_file");
        exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey_file(https_ssl_context, "/cert/server.key", SSL_FILETYPE_PEM) <= 0) {
        perror("SSL_CTX_use_PrivateKey_file");
        exit(EXIT_FAILURE);
    }

    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // Register the HTTP server socket for epoll events
    struct epoll_event http_event;
    http_event.events = EPOLLIN;
    http_event.data.fd = http_server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, http_server_fd, &http_event) == -1) {
        perror("epoll_ctl (HTTP)");
        exit(EXIT_FAILURE);
    }

    // Register the HTTPS server socket for epoll events
    struct epoll_event https_event;
    https_event.events = EPOLLIN;
    https_event.data.fd = https_server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, https_server_fd, &https_event) == -1) {
        perror("epoll_ctl (HTTPS)");
        exit(EXIT_FAILURE);
    }

    while (1) {
        struct epoll_event events[MAX_EVENTS];
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == http_server_fd) {
                // Accept new HTTP connections and create threads
                int client_fd = accept(http_server_fd, NULL, NULL);
                if (client_fd < 0) {
                    perror("[Error] http accept");
                    continue;
                }
                if (num_clients >= client_capacity) {
                    // Resize the array if needed
                    client_capacity *= 2;
                    client_infos = realloc(client_infos, sizeof(struct ClientInfo) * client_capacity);
                }

                struct ClientInfo *client_info = &client_infos[num_clients++];
                client_info->socket = client_fd;
                client_info->ssl_context = NULL;  // Indicate that this is not an SSL connection
                client_info->ssl_connection = NULL;

                // HTTP configuration
                pthread_create(&client_info->thread, NULL, (void *(*)(void *))handle_client, client_info);
                pthread_detach(client_info->thread);  // Detach the thread
            } else if (events[i].data.fd == https_server_fd) {
                // Accept new HTTPS connections and create threads
                int client_fd = accept(https_server_fd, NULL, NULL);
                if (client_fd < 0) {
                    perror("[Error] https accept");
                    continue;
                }

                if (num_clients >= client_capacity) {
                    // Resize the array if needed
                    client_capacity *= 2;
                    client_infos = realloc(client_infos, sizeof(struct ClientInfo) * client_capacity);
                }

                struct ClientInfo *client_info = &client_infos[num_clients++];
                client_info->socket = client_fd;

                // HTTPS configuration
                SSL *ssl_connection = SSL_new(https_ssl_context);
                SSL_set_fd(ssl_connection, client_fd);

                if (SSL_accept(ssl_connection) <= 0) {
                    // Handle SSL handshake error
                    ERR_print_errors_fp(stderr);
                    close(client_fd);
                    SSL_free(ssl_connection);
                    SSL_CTX_free(https_ssl_context);
                    continue;
                }

                client_info->ssl_context = https_ssl_context;
                client_info->ssl_connection = ssl_connection;

                pthread_create(&client_info->thread, NULL, (void *(*)(void *))handle_client, client_info);
                pthread_detach(client_info->thread);  // Detach the thread
            }
        }
    }

    // Clean up
    close(epoll_fd);
    close(http_server_fd);
    close(https_server_fd);
    free(client_infos);
    return 0;
}
