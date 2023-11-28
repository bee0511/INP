#include "header.hpp"

uint16_t calculateChecksum(const void* data, size_t length) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint16_t checksum = 0;

    for (size_t i = 0; i < length; ++i) {
        checksum += bytes[i];
    }

    return checksum;
}

void send_packet(int sock, struct sockaddr_in* server_addr, struct Packet* packet) {
    sendto(sock, packet, sizeof(struct Packet), 0, (struct sockaddr*)server_addr, sizeof(struct sockaddr_in));
}

bool wait_for_ack(int sock, struct sockaddr_in* server_addr, uint32_t expected_file_number, uint32_t expected_packet_number) {
    struct Packet ack_packet;
    ssize_t recv_size;

    // Set a timeout for receiving an ACK
    struct timeval timeout;
    timeout.tv_sec = 0;  // second
    timeout.tv_usec = 500000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) perror("setsockopt");

    // Wait for an ACK
    socklen_t server_addr_len = sizeof(struct sockaddr_in);
    recv_size = recvfrom(sock, &ack_packet, sizeof(struct Packet), 0, (struct sockaddr*)server_addr, &server_addr_len);  // Corrected line

    // Reset the timeout
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    // printf("get a ack %d\n", ack_packet.packet_number);
    //  Check if the received ACK is for the expected file number
    if (ack_packet.packet_number == expected_packet_number && ack_packet.file_number == expected_file_number) {
        printf("[Client] receive ack for file %d's packet %d\n", expected_file_number, expected_packet_number);
        return true;  // Received the expected ACK
    }

    return false;  // Timeout or incorrect ACK received
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <path-to-read-files> <total-number-of-files> <port> <server-ip-address>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* read_files_path = argv[1];
    int total_files = atoi(argv[2]);

    int s;
    struct sockaddr_in sin;

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(strtol(argv[argc - 2], NULL, 0));

    if (inet_pton(AF_INET, argv[argc - 1], &sin.sin_addr) != 1) return -fprintf(stderr, "** cannot convert IPv4 address for %s\n", argv[1]);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) err_quit("socket");

    for (int file_number = 0; file_number < total_files; file_number++) {
        // Construct the filename
        char filename[20];
        sprintf(filename, "/files/%06d", file_number);
        FILE* file = fopen(filename, "rb");

        if (file == NULL) {
            perror("Error opening file");
            exit(EXIT_FAILURE);
        }

        // Determine file size
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Initialize packet
        struct Packet packet;
        packet.file_number = file_number;
        packet.total_packets = (file_size / 1024) + 1;

        // Loop over each packet
        for (uint32_t seq_num = 0; seq_num < packet.total_packets; seq_num++) {
            // ... (read a chunk of data from the file into packet.data)
            size_t read_size = fread(packet.data, 1, PACKET_SIZE, file);
            // Set packet information
            packet.packet_number = seq_num;
            packet.checksum = calculateChecksum(&packet, offsetof(struct Packet, checksum));
            packet.checksum += calculateChecksum((uint8_t*)&packet.data, sizeof(packet.data));
            // Send the packet
            send_packet(s, &sin, &packet);
            printf("[Client] Send file %d's packet %d\n", file_number, seq_num);
            int resent;
            while (!wait_for_ack(s, &sin, file_number, seq_num)) {
                // Timeout or incorrect ACK received, retransmit the packet
                printf("[Client] Send file %d's packet %d again\n", file_number, seq_num);
                send_packet(s, &sin, &packet);
            }
        }

        fclose(file);
    }

    close(s);

    return 0;
}
