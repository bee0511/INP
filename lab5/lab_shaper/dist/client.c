#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 12345
#define BUFFER_SIZE 1024 * 1024
#define NUM_ITERATIONS 1000

double measureLatency(int client_socket) {
    // printf("[Client] Start measureLatency\n");

    // Measure one-way latency
    struct timeval start, end;
    char message[] = "A";

    gettimeofday(&start, NULL);
    if (send(client_socket, message, strlen(message), 0) == -1) {
        perror("[Client] Error sending data");
        exit(EXIT_FAILURE);
    }

    // Receive acknowledgment from the server
    char ack[BUFFER_SIZE];
    if (recv(client_socket, ack, sizeof(ack), 0) == -1) {
        perror("[Client] Error receiving acknowledgment");
        exit(EXIT_FAILURE);
    }

    gettimeofday(&end, NULL);
    double latency = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;

    return latency / 2;
}

double measureThroughput(int client_socket) {
    struct timeval start, end;

    gettimeofday(&start, NULL);

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        // printf("%d th iteration.\n", i);
        char send_buffer[BUFFER_SIZE];
        memset(send_buffer, 'A', sizeof(send_buffer));

        // Send data to the server
        ssize_t bytes_sent = send(client_socket, send_buffer, sizeof(send_buffer), 0);
        if (bytes_sent <= 0) {
            perror("Error sending data");
            exit(EXIT_FAILURE);
        }

        // Receive acknowledgment from the server
        char recv_buffer[BUFFER_SIZE];
        ssize_t total_bytes_received = 0;

        while (total_bytes_received < BUFFER_SIZE) {
            ssize_t bytes_received = recv(client_socket, recv_buffer + total_bytes_received, BUFFER_SIZE - total_bytes_received, 0);

            if (bytes_received == -1) {
                perror("Error receiving data");
                exit(EXIT_FAILURE);
            } else if (bytes_received == 0) {
                // Connection closed by server
                printf("Connection closed by server\n");
                break;
            }

            total_bytes_received += bytes_received;
        }
    }

    gettimeofday(&end, NULL);

    double elapsed_time = (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_usec - start.tv_usec);
    double throughput = (BUFFER_SIZE * 8.0 * NUM_ITERATIONS) / (elapsed_time);

    return throughput;
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Configure server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error connecting to server");
        exit(EXIT_FAILURE);
    }

    // Measure one-way latency
    double latency = measureLatency(client_socket);

    // Measure throughput
    double throughput = measureThroughput(client_socket);

    printf("# RESULTS: delay = %.3f ms, bandwidth = %.3f Mbps\n", latency, throughput);
    // Close socket
    close(client_socket);

    return 0;
}
