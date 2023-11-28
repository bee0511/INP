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

struct Packet getAck(int sock, struct sockaddr_in* server_addr) {
    struct Packet ack_packet;
    ack_packet.packet_number = -1;
    ssize_t recv_size;

    // Set a timeout for receiving an ACK
    struct timeval timeout;
    timeout.tv_sec = 0;  // second
    timeout.tv_usec = 500000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) perror("setsockopt");

    // Wait for an ACK
    socklen_t server_addr_len = sizeof(struct sockaddr_in);
    recv_size = recvfrom(sock, &ack_packet, sizeof(struct Packet), 0, (struct sockaddr*)server_addr, &server_addr_len);  // Corrected line

    return ack_packet;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <path-to-read-files> <total-number-of-files> <port> <server-ip-address>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* read_files_path = argv[1];
    int total_files = atoi(argv[2]);

    int sock;
    struct sockaddr_in sin;

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(strtol(argv[argc - 2], NULL, 0));

    if (inet_pton(AF_INET, argv[argc - 1], &sin.sin_addr) != 1) return -fprintf(stderr, "** cannot convert IPv4 address for %s\n", argv[1]);

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) err_quit("socket");

    int file_number = 0;
    uint32_t seq_num = 0;

    while (file_number < total_files) {
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
        packet.total_packets = (file_size / PACKET_SIZE) + 1;

        if (fseek(file, seq_num * PACKET_SIZE, SEEK_SET) != 0) {
            perror("[Client] fseek");
            exit(EXIT_FAILURE);
        }
        size_t read_size = fread(packet.data, 1, PACKET_SIZE, file);
        packet.packet_number = seq_num;
        packet.checksum = calculateChecksum(&packet, offsetof(struct Packet, checksum));
        packet.checksum += calculateChecksum((uint8_t*)&packet.data, sizeof(packet.data));
        send_packet(sock, &sin, &packet);
        printf("[Client] Send file %d's packet %d\n", file_number, seq_num);
        Packet ack_packet = getAck(sock, &sin);
        if (ack_packet.packet_number == -1) {
            printf("[Client] No Ack\n");
        } else {
            seq_num = ack_packet.packet_number;
            file_number = ack_packet.file_number;
        }
        printf("[Client] file %d total packet: %d\n", file_number, packet.total_packets);

        fclose(file);
    }

    close(sock);

    return 0;
}
