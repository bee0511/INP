// server.cpp

#include <arpa/inet.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

#define MAX_BUFFER_SIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <path-to-store-files> <total-number-of-files> <port>" << std::endl;
        exit(1);
    }

    const char *path_to_store_files = argv[1];
    int total_number_of_files = std::atoi(argv[2]);
    int port = std::atoi(argv[3]);

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("Error opening socket");

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        error("Error on binding");

    for (int file_number = 0; file_number < total_number_of_files; ++file_number) {
        std::string file_path = std::string(path_to_store_files) + "/" + std::to_string(file_number);
        std::ofstream file(file_path, std::ios::binary);

        if (!file.is_open()) {
            std::cerr << "Error opening file: " << file_path << std::endl;
            continue;
        }

        char buffer[MAX_BUFFER_SIZE];
        int bytes_received = 0;

        while (bytes_received < MAX_BUFFER_SIZE) {
            int bytes = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);

            if (bytes < 0)
                error("Error receiving data");

            bytes_received += bytes;

            // Write the received data to the file
            file.write(buffer, bytes);

            // Check for the end of file transmission
            if (bytes < MAX_BUFFER_SIZE)
                break;
        }

        file.close();
    }

    close(sockfd);

    return 0;
}
