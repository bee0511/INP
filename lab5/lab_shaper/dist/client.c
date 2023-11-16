#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define SERVER_IP "127.0.0.1"
#define PORT 12345
#define BUFFER_SIZE 1024

long measureLatency(int client_socket) {
    // Measure latency
    struct timeval start, end;
    gettimeofday(&start, NULL);

    char message[] = "Hello, server!";
    send(client_socket, message, strlen(message), 0);

    char response[BUFFER_SIZE];
    recv(client_socket, response, sizeof(response), 0);

    gettimeofday(&end, NULL);
    long latency = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
	
	return latency;
}

double measureThroughput(int client_socket) {
    // Measure throughput
    int data_size = 1024 * 1024;  // 1 MB
    char data[data_size];
    memset(data, 'A', sizeof(data));

    struct timeval start, end;
    gettimeofday(&start, NULL);

    send(client_socket, data, sizeof(data), 0);

    gettimeofday(&end, NULL);
    long elapsed_time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    double throughput = (data_size * 8.0) / (elapsed_time);

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

    // Measure latency
    long latency = measureLatency(client_socket);

    // Measure throughput
    double throughtput = measureThroughput(client_socket);

	printf("# RESULTS: delay = %ld ms, bandwidth = %.2f Mbps\n", latency, throughtput);
    // Close socket
    close(client_socket);

    return 0;
}
