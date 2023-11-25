#include "header.h"

#define MAX_EVENTS 10

struct ClientInfo {
    int socket;
    pthread_t thread;
};

struct ClientInfo *client_infos;
size_t client_capacity = INITIAL_CAPACITY;
size_t num_clients = 0;

// Function to handle an incoming client
void handle_client(int client_socket) {
    char request_buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, request_buffer, sizeof(request_buffer), 0);

    if (bytes_received > 0) {
        char decoded_buffer[BUFFER_SIZE];
        urlDecode(request_buffer, decoded_buffer);
        handleHTTPRequest(client_socket, decoded_buffer);
    }

    close(client_socket);
}

int main(int argc, char *argv[]) {
    int server_fd = createServerSocket();
    client_infos = malloc(sizeof(struct ClientInfo) * client_capacity);

    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // Register the server socket for epoll events
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl");
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
            if (events[i].data.fd == server_fd) {
                // Accept new connections and create threads
                int client_fd = accept(server_fd, NULL, NULL);
                if (client_fd < 0) {
                    perror("accept");
                    continue;
                }

                if (num_clients >= client_capacity) {
                    // Resize the array if needed
                    client_capacity *= 2;
                    client_infos = realloc(client_infos, sizeof(struct ClientInfo) * client_capacity);
                }

                struct ClientInfo *client_info = &client_infos[num_clients++];
                client_info->socket = client_fd;
                pthread_create(&client_info->thread, NULL, (void *(*)(void *))handle_client, (void *)(intptr_t)client_fd);
                pthread_detach(client_info->thread);  // Detach the thread
            }
        }
    }

    // Clean up
    close(epoll_fd);
    close(server_fd);
    free(client_infos);
    return 0;
}
