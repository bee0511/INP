#include "header.h"

int main(int argc, char* argv[]) {
    int server_fd = createServerSocket();
    int client_fds[MAX_CLIENTS], max_fd; 

    fd_set read_fds;
    for (int i = 0; i < MAX_CLIENTS; ++i) client_fds[i] = -1;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
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
                    urlDecode(request_buffer, decoded_buffer);
                    handleGetRequest(client_fds[i], decoded_buffer);
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
