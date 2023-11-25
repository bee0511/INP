#include "header.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct ClientInfo {
    int socket;
    pthread_t thread;
};

struct ClientInfo *client_infos;
size_t client_capacity = INITIAL_CAPACITY;
size_t num_clients = 0;

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    char request_buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, request_buffer, sizeof(request_buffer), 0);

    if (bytes_received > 0) {
        char decoded_buffer[BUFFER_SIZE];
        urlDecode(request_buffer, decoded_buffer);
        handleHTTPRequest(client_socket, decoded_buffer);
    }

    close(client_socket);

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int server_fd = createServerSocket();
    client_infos = malloc(sizeof(struct ClientInfo) * client_capacity);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        pthread_mutex_lock(&mutex);

        if (num_clients >= client_capacity) {
            // Resize the array if needed
            client_capacity *= 2;
            client_infos = realloc(client_infos, sizeof(struct ClientInfo) * client_capacity);
        }

        struct ClientInfo *client_info = &client_infos[num_clients++];
        client_info->socket = client_fd;
        pthread_create(&client_info->thread, NULL, handle_client, (void *)&client_info->socket);
        pthread_detach(client_info->thread);  // Detach the thread

        pthread_mutex_unlock(&mutex);
    }

    close(server_fd);
    free(client_infos);
    return 0;
}
