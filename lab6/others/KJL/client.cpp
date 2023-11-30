#include <arpa/inet.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <bitset>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <random>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include "header.hpp"
#include "utils.hpp"

int connect(char* host, uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0)
        perror("sockfd"), exit(-1);

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0)
        fail("inet_pton");

    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
        fail("connect");

    return sockfd;
}

void wrap_send(uint32_t data_seq, int sockfd, const void* buf, size_t len) {
    hdr_t hdr;
    bzero(&hdr, PACKET_SIZE);
    hdr.data_seq = data_seq;
    memcpy(hdr.data, buf, len);
    // dump_hdr(&hdr);
    if (send(sockfd, &hdr, PACKET_SIZE, 0) < 0)
        fail("[cli] fail send");
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <path-to-read-files> <total-number-of-files> <port> <server-ip-address>\n", argv[0]);
        exit(0);
    }

    char* path = argv[1];
    uint32_t total = (uint32_t)atoi(argv[2]);
    uint16_t port = (uint16_t)atoi(argv[3]);
    char* ip = argv[4];

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    std::vector<file_t> files(SUB_SIZE);
    std::map<uint32_t, data_t> data_map{};
    int connfd = connect(ip, port);
    set_sockopt(connfd);
    for (int st = 0; st < total; st += SUB_SIZE) {
        size_t total_bytes = 0;
        for (uint32_t idx = st; idx < st + SUB_SIZE; idx++) {
            char filename[256];
            sprintf(filename, "%s/%06d", path, idx);
            int filefd = open(filename, O_RDONLY);
            if (filefd < 0) fail("open");
            auto size = (uint32_t)lseek(filefd, 0, SEEK_END);
            auto data = new char[size];
            lseek(filefd, 0, SEEK_SET);
            read(filefd, data, size);
            close(filefd);

            files[idx] = {
                .filename = idx,
                .size = size,
                .init = {
                    .filename = idx,
                    .filesize = size,
                },
                .data = data,
            };
            // printf("[cli] read '%s' (%u bytes)\n", filename, size);
            total_bytes += size;
        }

        printf("[Client] Iter %d Connect suceess\n", st / SUB_SIZE);

        auto orig_size = SUB_SIZE * sizeof(init_t) + total_bytes;
        auto orig_data = new char[orig_size];
        {
            auto ptr = orig_data;
            for (auto& file : files) {
                memcpy(ptr, &file.init, sizeof(init_t));
                ptr += sizeof(init_t);
                memcpy(ptr, file.data, file.size);
                ptr += file.size;
            }
        }
        data_t zero{
            .data_size = orig_size};
        data_map.emplace(0, data_t{.data_size = sizeof(data_t), .data = (char*)&zero});
        for (size_t i = 0; i * DATA_SIZE < orig_size; i++) {
            size_t offset = i * DATA_SIZE;
            data_map.emplace(
                i + 1,
                data_t{
                    .data_size = std::min(DATA_SIZE, orig_size - offset),
                    .data = orig_data + offset,
                });
        }
        hdr_t s_res;
        auto& donebit = *(std::bitset<DATA_SIZE * 8>*)&s_res.data;
        auto read_resps = [&]() {
            hdr_t res;
            while (recv(connfd, &res, sizeof(res), MSG_WAITALL) == sizeof(res)) {
                s_res = res;
                if (res.data_seq == UINT32_MAX) {
                    printf("[cli] sent done for (%lu bytes)\n", orig_size);
                    // usleep(100);
                    exit(0);
                    continue;
                }
            }
        };

        int r = 0;
        auto last_send = std::chrono::steady_clock::now();
        while (!data_map.empty()) {
            // ï½?fprintf(stderr, "[cli] %d: %lu data packets left\n", ++r, data_map.size());
            for (auto [key, data] : data_map)
                if (!donebit[key]) {
                    wrap_send(key, connfd, data.data, data.data_size);
                    usleep(500);
                }
            if (data_map.size() > 100) {
                auto wait = std::chrono::milliseconds(SEND_TIME);
                std::this_thread::sleep_until(last_send + wait);
            }
            last_send = std::chrono::steady_clock::now();
            read_resps();
            for (size_t i = 0; i < DATA_SIZE * 8; i++)
                if (donebit[i]) data_map.erase((uint32_t)i);
        }
        for (const auto& file : files) {
            delete[] file.data;
        }
        delete[] orig_data;
        data_map.clear();
    }
    close(connfd);
}