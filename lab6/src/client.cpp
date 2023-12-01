#include "header.hpp"

bool send_packet(int sock, struct sockaddr_in* server_addr, struct Packet* packet) {
    size_t sent_size = sendto(sock, packet, sizeof(struct Packet), 0, (struct sockaddr*)server_addr, sizeof(struct sockaddr_in));
    if (sent_size == -1) {
        perror("[Client] Error sending packet");
        return false;
    } else if (sent_size != sizeof(struct Packet)) {
        printf("[Client] Warning: Not all data was sent. Only %ld out of %ld bytes were sent.\n", sent_size, sizeof(struct Packet));
        return false;
    }
    if (packet->packet_number == INIT) {
#ifdef DUMPINIT
        printf("[Client] Send init packet\n");
#endif
        return true;
    }
#ifdef DUMPCLI
    printf("[Client] Send file %d's packet %d\n", packet->file_number, packet->packet_number);
#endif
    return true;
}

struct Ack getAck(int sock, struct sockaddr_in* server_addr) {
    struct Ack ack_packet;
    ack_packet.file_number = NO_ACK;
    ssize_t recv_size;

    // Wait for an ACK
    socklen_t server_addr_len = sizeof(struct sockaddr_in);
    recv_size = recvfrom(sock, &ack_packet, sizeof(struct Ack), 0, (struct sockaddr*)server_addr, &server_addr_len);

// Calculate the checksum
#ifdef ENABLE_ACK_CKSUM
    uint16_t checksum = calculateCRC(&ack_packet, offsetof(struct Ack, checksum), 0xFFFF);
    checksum = calculateCRC((uint8_t*)&ack_packet.stored_packet, sizeof(ack_packet.stored_packet), checksum);
    if (checksum != ack_packet.checksum) {
        // dump file number
        printf("[Client] Original file number: %d\n", ack_packet.file_number);
        ack_packet.file_number = ACK_ERROR;
        // dump checksum and and ack_packet's checksum
        printf("[Client] Checksum: %d, Ack packet's checksum: %d\n", checksum, ack_packet.checksum);
    }
#endif
    return ack_packet;
}
void init_send(int sock, struct sockaddr_in* server_addr) {
    bool recv = false;

    struct Packet packet;
    packet.file_number = INIT;
    packet.packet_number = 0;
    packet.total_packets = 0;
    packet.checksum = 0;
    packet.length = 0;
    memset(packet.data, 0, sizeof(packet.data));
    struct Ack ack_packet;

    while (!recv) {
        send_packet(sock, server_addr, &packet);
        // If receive an ACK with file number = 10000, then the server is ready
        ack_packet = getAck(sock, server_addr);
        if (ack_packet.file_number == INIT) {
            recv = true;
#ifdef DUMPINIT
            printf("[Client] Init Ack received\n");
#endif
        }
#ifdef DUMPINIT
        if (!recv) printf("[Client] No init Ack, send again\n");
#endif
    }

    return;
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

    if (inet_pton(AF_INET, argv[argc - 1], &sin.sin_addr) != 1)
        return -fprintf(stderr, "** cannot convert IPv4 address for %s\n", argv[1]);

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        err_quit("socket");

    // Set a timeout for receiving an ACK
    struct timeval timeout;
    timeout.tv_sec = 0;         // second
    timeout.tv_usec = TIMEOUT;  // usecond
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) perror("setsockopt");

    int file_number = 0;
    bool end_of_file = false;

    init_send(sock, &sin);

    while (!end_of_file) {
        Ack ack_packet = getAck(sock, &sin);
        if (ack_packet.file_number == INIT) {
            printf("[Client] Cleaning init ack...\n");
            continue;
        }
        if (ack_packet.file_number == NO_ACK) {
            // WIN_SIZE = WIN_SIZE * 2;
            // if (WIN_SIZE > MAX_WIN_SIZE) WIN_SIZE = MAX_WIN_SIZE;
            printf("[Client] No Ack\n");
            continue;
        }
        if (ack_packet.file_number == ACK_ERROR) {
            printf("[Client] Ack error\n");
            continue;
        }
        if (ack_packet.file_number == FINISH) {
            printf("[Client] Finish\n");
            break;
        }
        file_number = ack_packet.file_number;
        if (file_number >= 1000) {
            printf("[Client] File %d invalid", file_number);
            continue;
        }

        // Initialize packet
        struct Packet packet;
        // Construct the filename
        char filename[20];
        sprintf(filename, "/files/%06d", file_number);
        FILE* file = fopen(filename, "rb");

        if (file == NULL) {
            perror("[Client] Error opening file");
            exit(EXIT_FAILURE);
        }

        // Determine file size
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        packet.total_packets = (file_size / PACKET_SIZE) + 1;
        packet.file_number = file_number;
        for (int i = 0; i < packet.total_packets; i++) {
            // The server has stored the packet, no need to send again
            if (ack_packet.stored_packet[i] == true) {
                continue;
            }
            if (fseek(file, i * PACKET_SIZE, SEEK_SET) != 0) {
                perror("[Client] fseek");
                exit(EXIT_FAILURE);
            }
            size_t read_size = fread(packet.data, 1, PACKET_SIZE, file);
            packet.length = read_size;
            packet.packet_number = i;
            packet.checksum = calculateCRC(&packet, offsetof(struct Packet, checksum), 0xFFFF);
            packet.checksum = calculateCRC((uint8_t*)&packet.data, sizeof(packet.data), packet.checksum);
            // dump checksum and packet_number
            // printf("[Client] Checksum: %d, Packet number: %d\n", packet.checksum, packet.packet_number);
            send_packet(sock, &sin, &packet);
#ifdef DUMPCLI
            printf("[Client] Send file %d's packet %d\n", file_number, i);
#endif
        }
        fclose(file);
    }

    close(sock);

    return 0;
}
