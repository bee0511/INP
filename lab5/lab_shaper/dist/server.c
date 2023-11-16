#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 12345
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

void handleLatency(int client_socket) {
    char buffer[BUFFER_SIZE];
    recv(client_socket, buffer, sizeof(buffer), 0);
    send(client_socket, buffer, strlen(buffer), 0);
}

void handleThroughput(int client_socket, int num_iterations) {
    long long data_size = 1024 * 1024;  // 1 MB
    char buffer[data_size];

    for (int i = 0; i < num_iterations; ++i) {
        // printf("%d th iteration.\n", i);

        ssize_t total_bytes_received = 0;

        while (total_bytes_received < data_size) {
            ssize_t bytes_received = recv(client_socket, buffer + total_bytes_received, data_size - total_bytes_received, 0);

            if (bytes_received == -1) {
                perror("Error receiving data");
                exit(EXIT_FAILURE);
            } else if (bytes_received == 0) {
                // Connection closed by client
                printf("Connection closed by client\n");
                break;
            }

            total_bytes_received += bytes_received;
        }

        ssize_t bytes_sent = send(client_socket, buffer, total_bytes_received, 0);

        if (bytes_sent == -1) {
            perror("Error sending acknowledgment");
            exit(EXIT_FAILURE);
        }
    }
}

int main() {
    int server_socket, client_sockets[MAX_CLIENTS];
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Enable address reuse option
    int enable = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    // Initialize client sockets array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = -1;
    }

    fd_set read_fds, master_fds;
    FD_ZERO(&master_fds);
    FD_SET(server_socket, &master_fds);
    int max_fd = server_socket;

    while (1) {
        read_fds = master_fds;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("Error in select");
            exit(EXIT_FAILURE);
        }

        // Check for incoming connection
        if (FD_ISSET(server_socket, &read_fds)) {
            if ((client_sockets[0] = accept(server_socket, (struct sockaddr *)&client_addr, &client_len)) == -1) {
                perror("Error accepting connection");
                exit(EXIT_FAILURE);
            }

            FD_SET(client_sockets[0], &master_fds);

            if (client_sockets[0] > max_fd) {
                max_fd = client_sockets[0];
            }
        }

        // Check for data from clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] != -1 && FD_ISSET(client_sockets[i], &read_fds)) {
                // Handle latency
                handleLatency(client_sockets[i]);

                // Handle throughput
                handleThroughput(client_sockets[i], 1000);
                // printf("[*] Closing socket...\n");
                close(client_sockets[i]);
                FD_CLR(client_sockets[i], &master_fds);
                client_sockets[i] = -1;
            }
        }
    }

    // Close the server socket (Note: This part may not be reached in this example)
    close(server_socket);

    return 0;
}
