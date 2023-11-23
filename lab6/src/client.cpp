// client.cpp

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
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <path-to-read-files> <total-number-of-files> <port> <server-ip-address>" << std::endl;
        exit(1);
    }

    const char *path_to_read_files = argv[1];
    int total_number_of_files = std::atoi(argv[2]);
    int port = std::atoi(argv[3]);
    const char *server_ip_address = argv[4];

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("Error opening socket");

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip_address, &server_addr.sin_addr) <= 0)
        error("Invalid server IP address");

    for (int file_number = 0; file_number < total_number_of_files; ++file_number) {
        std::string file_path = std::string(path_to_read_files) + "/" + std::to_string(file_number);
        std::ifstream file(file_path, std::ios::binary);

        if (!file.is_open()) {
            std::cerr << "Error opening file: " << file_path << std::endl;
            continue;
        }

        std::streamsize file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        char buffer[MAX_BUFFER_SIZE];
        int bytes_sent = 0;

        std::cout << "Transmitting file: " << file_path << std::endl;

        while (bytes_sent < file_size) {
            int bytes_to_send = std::min(MAX_BUFFER_SIZE, static_cast<int>(file_size - bytes_sent));
            file.read(buffer, bytes_to_send);

            // Send the buffer to the server
            sendto(sockfd, buffer, bytes_to_send, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

            bytes_sent += bytes_to_send;
        }

        file.close();
        std::cout << "File transmission complete for: " << file_path << std::endl;
    }

    close(sockfd);

    return 0;
}
