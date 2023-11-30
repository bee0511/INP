#include "header.hpp"
std::vector<std::vector<bool>> stored_packet(1000);

void send_ack(int sock, struct sockaddr_in* client_addr, uint32_t file_number, uint32_t packet_number) {
    struct Packet ack_packet;
    ack_packet.packet_number = packet_number;  // Acknowledge the specified file number
    ack_packet.file_number = file_number;
    sendto(sock, &ack_packet, sizeof(struct Packet), 0, (struct sockaddr*)client_addr, sizeof(struct sockaddr_in));
    // printf("[Server] Require for file %d's packet %d\n", ack_packet.file_number, ack_packet.packet_number);
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

    char* store_files_path = argv[1];
    int total_files = atoi(argv[2]);
    // int server_port = atoi(argv[3]);

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

    std::vector<std::vector<std::string>> datas(1000);
    std::vector<bool> expanded(1000, false);
    int max_expanded = 0;

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

        uint16_t calculated_checksum = calculateCRC(&packet, offsetof(struct Packet, checksum), 0xFFFF);
        calculated_checksum = calculateCRC((uint8_t*)&packet.data, sizeof(packet.data), calculated_checksum);
        if (packet.checksum != calculated_checksum) {
            printf("[Server] Checksum verification failed\n");
            printf("[Server] Cal checksum: %d, packet checksum: %d\n", calculated_checksum, packet.checksum);
            continue;
        }
        if (stored_packet[packet.file_number].size() == 0 && expanded[packet.file_number] == false) {
            stored_packet[packet.file_number].resize(packet.total_packets, false);
            datas[packet.file_number].resize(packet.total_packets);
            expanded[packet.file_number] = true;
            max_expanded = packet.file_number;
        }
        if (!stored_packet[packet.file_number].empty()) {
            datas[packet.file_number][packet.packet_number] = std::string(packet.data, packet.length);
            stored_packet[packet.file_number][packet.packet_number] = true;
        }
        for (int i = file_num; i < max_expanded; i++) {
            bool save_flg = !stored_packet[i].empty();
            if (save_flg == false) {
                // printf("file %d has already saved\n", i);
                continue;
            }

            for (int j = 0; j < stored_packet[i].size(); j++) {
                if (stored_packet[i][j] == true) continue;
                seq_num = j;
                file_num = i;
                save_flg = false;
                break;
                // printf("[Server] File num: %d, Seq num: %d has not saved\n", file_num, seq_num);
            }

            if (save_flg) {
                store_file(argv[1], datas[i], i);
                stored_packet[i].clear();
                for (int j = 0; j < datas[i].size(); j++) {
                    datas[i][j].clear();
                }
                datas[i].clear();
                break;
            }
            send_ack(s, &client_addr, file_num, seq_num);

            break;
        }
    }

    close(s);

    return 0;
}