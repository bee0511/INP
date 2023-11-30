#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>

#include "header.hpp"
#include "utils.hpp"

using DATA_MAP_T = std::map<uint32_t, std::array<char, DATA_SIZE>>;

void do_reponse(int sock, struct sockaddr_in* cin, hdr_t& hdr) {
    cin->sin_family = AF_INET;

    // printf("[*] Sending response to %s:%d, seq=%d\n", inet_ntoa(cin->sin_addr), ntohs(cin->sin_port), hdr.data_seq);
    for (int i = 0; i < 3; ++i) {
        if (sendto(sock, (const void*)&hdr, sizeof(struct hdr_t), 0, (struct sockaddr*)cin, sizeof(sockaddr_in)) < 0) {
            fail("sendto");
        }
    }
}

hdr_t* recv_sender_data(hdr_t* hdr, int sock, struct sockaddr_in* cin) {
    bzero(hdr, sizeof(hdr_t));
    socklen_t len = sizeof(&cin);

    if (recvfrom(sock, (void*)hdr, PACKET_SIZE, MSG_WAITALL, (struct sockaddr*)cin, &len) < 0) {
        fail("recvfrom");
    }

    return hdr;
}

int save_to_file(const char* path, uint32_t comp_size, DATA_MAP_T& data) {
    std::vector<char> comp_data((1 + (comp_size - 1) / DATA_SIZE) * DATA_SIZE);
    for (const auto& [seq, data_frag] : data) {
        if (seq > 0) {
            // Calculate the starting index in comp_data
            size_t start_index = (seq - 1) * DATA_SIZE;

            // Ensure the range does not go beyond the size of comp_data
            size_t copy_size = std::min(DATA_SIZE, comp_data.size() - start_index);

            // Copy the data directly to the vector
            std::copy(data_frag.begin(), data_frag.begin() + copy_size, comp_data.begin() + start_index);
        }
    }

    // auto orig_size = ZSTD_getFrameContentSize(comp_data, comp_size);
    // auto orig_data = new char[orig_size];
    // auto ctx = ZSTD_createDCtx();
    // ZSTD_decompress_usingDict(ctx, orig_data, orig_size, comp_data, comp_size, dict, (size_t)dict_size);
    auto orig_size = comp_size;
    printf("[/] %u bytes -> %u bytes\n", comp_size, orig_size);

    // // dump map
    // for (auto it = data.begin(); it != data.end(); ++it) {
    //     printf("[Chunk %d] ", it->first);
    //     for (int i = 0; i < PACKET_SIZE; ++i) {
    //         printf("%02x ", (uint8_t)it->second[i]);
    //
    // }
    // printf("\n");

    int cnt = 0;
    auto ptr = comp_data.data();  // Get a pointer to the data
    while (ptr < comp_data.data() + comp_data.size()) {
        auto init = reinterpret_cast<init_t*>(ptr);
        ptr += sizeof(init_t);
        auto filesize = init->filesize;

        char filename[100];
        snprintf(filename, sizeof(filename), "%s/%06d", path, init->filename);

        // Open the file using C++ streams
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            fail("open");
            return 1;  // Exit or handle the error appropriately
        }

        // Write the data to the file
        file.write(ptr, filesize);
        if (!file) {
            fail("write");
            return 1;  // Exit or handle the error appropriately
        }

        // Move the pointer to the next block
        ptr += filesize;

        cnt++;
    }

    data.clear();
    return cnt;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        return -fprintf(stderr, "usage: %s <path-to-store-files> <total-number-of-files> <port>\n", argv[0]);
    }
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    char* path = argv[1];
    int total = atoi(argv[2]);
    auto port = (uint16_t)atoi(argv[3]);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    int listenfd;
    if ((listenfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        fail("socket");

    if (bind(listenfd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        fail("bind");
    }

    DATA_MAP_T data_map;
    for (int st = 0; st < total; st += SUB_SIZE) {
        uint32_t comp_size = UINT32_MAX, recv_size = 0;
        hdr_t recv_hdr;
        struct sockaddr_in csin;

        auto last_send = std::chrono::steady_clock::now();
        bool keep = false;
        int r = 0;
        hdr_t send_hdr{
            .data_seq = 0};
        auto& recvbit = *(std::bitset<DATA_SIZE * 8>*)&send_hdr.data;

        while (comp_size == UINT32_MAX || recv_size < comp_size) {
            bzero(&csin, sizeof(csin));
            if (recv_sender_data(&recv_hdr, listenfd, &csin) != nullptr) {
                auto data_seq = recv_hdr.data_seq;
                recvbit[data_seq] = 1;
                if (data_seq == 0) {
                    // create a new session
                    data_t tmp;
                    memcpy(&tmp, recv_hdr.data, sizeof(data_t));
                    comp_size = (uint32_t)tmp.data_size;
                    printf("[/] [ChunkID=%d] initiation %d bytes\n", data_seq, (uint32_t)tmp.data_size);
                } else {
                    // save the data chunk
                    // printf("[/] [ChunkID=%d] Received data chunk from %s:%d\n",data_seq,inet_ntoa(csin.sin_addr),ntohs(csin.sin_port));
                    if (!data_map.count(data_seq)) {
                        auto& data_frag = data_map[data_seq];
                        memcpy(&data_frag, recv_hdr.data, DATA_SIZE);
                        recv_size += DATA_SIZE;
                    }
                }
            }
            // Send ACK
            if (!keep && (comp_size / DATA_SIZE) - recvbit.count() < 100)
                keep = true;
            auto now = std::chrono::steady_clock::now();
            if (now - last_send > std::chrono::milliseconds(keep ? 10 : SEND_TIME)) {
                last_send = now;
                // fprintf(stderr, "[/] %2.2f send %lu ack\n", ++r / double(SEND_WAIT), recvbit.count());
                do_reponse(listenfd, &csin, send_hdr);
            }
            // printf("[/] recv size: %d\n", recv_size);
        }
        auto write = save_to_file(path, comp_size, data_map);
        fprintf(stderr, "[/] save %d / %d files\n", write, SUB_SIZE);
        // data_map.clear();
        // All data recvived, send FIN
        // fprintf(stderr, "[/] Received all data, sending FIN\n");

        // send_hdr.data_seq = UINT32_MAX;
        // while (true) {
        //     if (recv_sender_data(&recv_hdr, listenfd, &csin) == nullptr)
        //         continue;
        //     do_reponse(listenfd, &csin, send_hdr);
        // }
    }
    close(listenfd);
}