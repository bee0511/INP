#include "header.hpp"
std::vector<std::vector<bool>> stored_packet(1000);
std::vector<std::vector<std::string>> datas(1000);
std::vector<bool> expanded(1000), file_saved(1001);

void send_ack(int sock, struct sockaddr_in* client_addr, struct Ack& ack_packet) {
// Calculate the checksum
#ifdef ENABLE_ACK_CKSUM
    ack_packet.checksum = calculateCRC(&ack_packet, offsetof(struct Ack, checksum), 0xFFFF);
    ack_packet.checksum = calculateCRC((uint8_t*)&ack_packet.stored_packet, sizeof(ack_packet.stored_packet), ack_packet.checksum);
#endif
    socklen_t addr_len = sizeof(struct sockaddr_in);
    ssize_t sent_size = sendto(sock, &ack_packet, sizeof(struct Ack), 0, (struct sockaddr*)client_addr, addr_len);
    if (sent_size == -1) {
        perror("[Server] Error sending ACK");
    } else if (sent_size != sizeof(struct Ack)) {
        std::cout << "[Server] Warning: Not all data was sent. Only " << sent_size << " out of " << sizeof(struct Ack) << " bytes were sent." << std::endl;
    }
    if (ack_packet.file_number == FINISH) {
        printf("[Server] Sending FINISH ACK\n");
        return;
    }
#ifdef DUMPSRV
    // print the ack packet's file number
    printf("[Server] Send ack file %d\n", ack_packet.file_number);
    // print the ack packet's file number and stored packet
    printf("[Server] File %d's stored packet: ", ack_packet.file_number);
    for (int i = 0; i < MAX_PACKETS; i++) {
        printf("%d", ack_packet.stored_packet[i]);
    }
    printf("\n");
#endif
    return;
}
void store_file(const char* folder_path, std::vector<std::string> data, uint16_t file_num) {
    char filename[256];
    sprintf(filename, "%s/%06d", folder_path, file_num);

    FILE* file = fopen(filename, "wb");  // Open the file in write mode
    if (file == NULL) {
        perror("fopen");
        exit(-1);
    }

    // Write all the data in the vector to the file
    for (const std::string& packet_data : data) {
        fwrite(packet_data.c_str(), 1, packet_data.size(), file);
    }

    fclose(file);

    printf("[Server] Store file %d\n", file_num);
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

    int s, seq_num = 0, file_num = 0;
    struct sockaddr_in sin;

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(strtol(argv[3], NULL, 0));

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) err_quit("socket");

    if (bind(s, (struct sockaddr*)&sin, sizeof(sin)) < 0) err_quit("bind");

    struct timeval timeout;
    timeout.tv_sec = 0;  // second
    timeout.tv_usec = SRV_TIMEOUT;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) perror("setsockopt");

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int start_file = 0;
    int end_file = start_file + INITIAL_WIN_SIZE;

    while (1) {
        struct Packet packet;
        // Receive a packet
        ssize_t bytes_received = recvfrom(s, &packet, sizeof(struct Packet), 0, (struct sockaddr*)&client_addr, &client_addr_len);
        if (bytes_received < 0) {
            printf("[Server] Timeout\n");
            continue;
        }
        while (bytes_received > 0) {
            uint16_t calculated_checksum = calculateCRC(&packet, offsetof(struct Packet, checksum), 0xFFFF);
            calculated_checksum = calculateCRC((uint8_t*)&packet.data, sizeof(packet.data), calculated_checksum);
            if (packet.checksum != calculated_checksum) {
                printf("[Server] Checksum verification failed\n");
                printf("[Server] Cal checksum: %d, packet checksum: %d\n", calculated_checksum, packet.checksum);
                // usleep(1000);
                break;
            }
            // Resize the file's packet vector since the total_packets is known
            // Only resize once
            if (expanded[packet.file_number] == false) {
                stored_packet[packet.file_number].resize(packet.total_packets, false);
                datas[packet.file_number].resize(packet.total_packets);
                expanded[packet.file_number] = true;
#ifdef DUMPSRV
                printf("[Server] Resize file %d's packet vector to %d\n", packet.file_number, packet.total_packets);
#endif
            }

            // No need to save the file since it has been saved before
            if (stored_packet[packet.file_number].empty() == true) {
                // printf("[Server] File %d has already been saved\n", packet.file_number);
                break;
            }
            // Store the packet
            if (stored_packet[packet.file_number][packet.packet_number] == false) {
                datas[packet.file_number][packet.packet_number] = std::string(packet.data, packet.length);
                stored_packet[packet.file_number][packet.packet_number] = true;
#ifdef DUMPSRV
                // Print the packet number
                printf("[Server] Store file %d's packet %d\n", packet.file_number, packet.packet_number);
#endif
            }
            // Check if the file is ready to be saved
            // Iterate through the store_packet[packet.file_number] to check if all the packets are stored
            bool save_flg = true;
            for (int j = 0; j < stored_packet[packet.file_number].size(); j++) {
                if (stored_packet[packet.file_number][j] == true) continue;
                save_flg = false;
                break;
            }
            // If we can save the file, save it and clear the vector
            if (save_flg) {
                store_file(argv[1], datas[packet.file_number], packet.file_number);
                stored_packet[packet.file_number].clear();
                for (int j = 0; j < datas[packet.file_number].size(); j++) {
                    datas[packet.file_number][j].clear();
                }
                datas[packet.file_number].clear();
                file_saved[packet.file_number] = true;
                break;
            }

            // Require for the file again
            bytes_received = recvfrom(s, &packet, sizeof(struct Packet), 0, (struct sockaddr*)&client_addr, &client_addr_len);
        }

        // Set the start_file and end_file
        for (int i = start_file; i <= 1000; i++) {
            if (file_saved[i] == false) {
                start_file = i;
                end_file = i + INITIAL_WIN_SIZE;
                if (end_file >= 1000) end_file = 1000;
                break;
            }
        }
        if (start_file == end_file) {
            printf("[Server] All files have been saved\n");
            struct Ack ack_packet;

            ack_packet.file_number = FINISH;
            // Initialize the stored_packet vector
            for (int i = 0; i < MAX_PACKETS; i++) {
                ack_packet.stored_packet[i] = false;
            }
            while (true) {
                send_ack(s, &client_addr, ack_packet);
                usleep(10 * 1000);
            }
        }
        // Require for the file
        for (int i = start_file; i < end_file; i++) {
            if (file_saved[i] == true) {
#ifdef DUMPSRV
                printf("[Server] File %d has already been saved\n", i);
#endif
                continue;
            }
            struct Ack ack_packet;
            ack_packet.file_number = i;
            for (int j = 0; j < MAX_PACKETS; j++) {
                // Initial, all packets are not stored
                if (stored_packet[i].empty() == true) {
                    ack_packet.stored_packet[j] = false;
                    continue;
                }
                ack_packet.stored_packet[j] = stored_packet[i][j];
            }
            send_ack(s, &client_addr, ack_packet);
        }
    }

    close(s);

    return 0;
}