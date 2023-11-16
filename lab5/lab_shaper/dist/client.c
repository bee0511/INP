#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 12345
#define BUFFER_SIZE 1024 * 1024

double measureLatency(int client_socket) {
    // Measure one-way latency
    struct timeval start, end;
    char message[] = "A";

    gettimeofday(&start, NULL);
    ssize_t bytes_sent = send(client_socket, message, strlen(message), 0);
    if (bytes_sent == -1) {
        perror("Error sending data");
        exit(EXIT_FAILURE);
    }

    // Receive acknowledgment from the server
    char ack[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, ack, sizeof(ack), 0);
    if (bytes_received == -1) {
        perror("Error receiving acknowledgment");
        exit(EXIT_FAILURE);
    }

    gettimeofday(&end, NULL);
    double latency = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;

    return latency / 2;
}

double measureThroughput(int client_socket) {
    // Measure throughput
    char data[BUFFER_SIZE];
    memset(data, 'A', sizeof(data));

    struct timeval start, end;

    // Send data to the server
    gettimeofday(&start, NULL);
    ssize_t bytes_sent = send(client_socket, data, sizeof(data), 0);
    if (bytes_sent == -1) {
        perror("Error sending data");
        exit(EXIT_FAILURE);
    }

    // Receive acknowledgment from the server
    char ack[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, ack, sizeof(ack), 0);
    if (bytes_received == -1) {
        perror("Error receiving acknowledgment");
        exit(EXIT_FAILURE);
    }

    gettimeofday(&end, NULL);
    long elapsed_time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    double throughput = (BUFFER_SIZE * 8.0) / (elapsed_time);

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
