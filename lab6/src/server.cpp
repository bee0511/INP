#include "header.hpp"

uint16_t calculateChecksum(const void* data, size_t length) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint16_t checksum = 0;

    for (size_t i = 0; i < length; ++i) {
        checksum += bytes[i];
    }

    return checksum;
}

void send_ack(int sock, struct sockaddr_in* client_addr, uint32_t file_number, uint32_t packet_number) {
    struct Packet ack_packet;
    ack_packet.packet_number = packet_number;  // Acknowledge the specified file number
    ack_packet.file_number = file_number;
    sendto(sock, &ack_packet, sizeof(struct Packet), 0, (struct sockaddr*)client_addr, sizeof(struct sockaddr_in));
    printf("[Server] Send ack for file %d's packet %d\n", ack_packet.file_number, ack_packet.packet_number);
}

void store_file(const char* folder_path, const struct Packet* packet) {
    char filename[256];
    sprintf(filename, "%s/%06d", folder_path, packet->file_number);

    FILE* file = fopen(filename, "ab");  // Open the file in append mode
    if (file == NULL) {
        perror("fopen");
        exit(-1);
    }

    fwrite(packet->data, 1, strlen(packet->data), file);
    // printf("store %d %d\n", packet->file_number, packet->packet_number);
    fclose(file);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <path-to-store-files> <total-number-of-files> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i <= 999; i++) {
        char filename[256];
        sprintf(filename, "%s/%06d", argv[1], i);
        remove(filename);
    }

    char* store_files_path = argv[1];
    int total_files = atoi(argv[2]);
    // int server_port = atoi(argv[3]);

    int s, expect_p = 0, previous_expect_p = 0;
    int expect_f = 0, previous_expect_f = 0;
    struct sockaddr_in sin;

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(strtol(argv[3], NULL, 0));

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) err_quit("socket");

    if (bind(s, (struct sockaddr*)&sin, sizeof(sin)) < 0) err_quit("bind");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        struct Packet packet;

        // Receive a packet
        ssize_t bytes_received = recvfrom(s, &packet, sizeof(struct Packet), 0, (struct sockaddr*)&client_addr, &client_addr_len);
        if (bytes_received == 0) {
            printf("[Server] No receive\n");
        }
        if (bytes_received < 0) {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }

        uint16_t calculated_checksum = calculateChecksum(&packet, offsetof(struct Packet, checksum)) + calculateChecksum((uint8_t*)&packet.data, sizeof(packet.data));
        if (packet.checksum != calculated_checksum) {
            printf("[Server] Checksum verification failed\n");
            // Handle the case when the checksum does not match (you may want to send a negative acknowledgment or take other actions)
        } else if (packet.file_number == expect_f && packet.packet_number == expect_p)  //&& packet.checksum == calculateChecksum(&packet, offsetof(struct Packet, checksum)) + calculateChecksum((uint8_t*)&packet.data, sizeof(packet.data))
        {
            store_file(argv[1], &packet);
            printf("[Server] Store file %d's packet %d\n", packet.file_number, packet.packet_number);
            send_ack(s, &client_addr, expect_f, expect_p);
            previous_expect_p = expect_p;
            previous_expect_f = expect_f;
            expect_p++;
            if (expect_p == packet.total_packets) {
                expect_p = 0;
                expect_f++;
                printf("[Server] reset expect\n");
            }
            printf("[Server] expecting file %d's packet %d\n", expect_f, expect_p);
        } else if (packet.packet_number == previous_expect_p && packet.file_number == previous_expect_f) {
            // Received an out-of-order packet or a duplicate
            // You can add handling logic here if needed
            printf("[Server] Received out-of-order packet or duplicate for file %d's packet %d\n", packet.file_number, packet.packet_number);
            send_ack(s, &client_addr, packet.file_number, packet.packet_number);  // Acknowledge the duplicate packet
        }
    }

    close(s);

    return 0;
}
