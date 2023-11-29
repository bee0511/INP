#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

#define err_quit(m) \
    {               \
        perror(m);  \
        exit(-1);   \
    }
#define PACKET_SIZE 2048
#define PORT 12345
#define DEBUG 1

#ifndef HEADER_H
#define HEADER_H
struct Packet {
    uint32_t file_number;
    uint32_t packet_number;
    uint32_t total_packets;
    uint16_t checksum;
    uint16_t length;
    char data[PACKET_SIZE];
};

struct Ack {
    uint32_t file_number;
    uint32_t packet_number;
};

uint16_t calculateCRC(const void* data, size_t length, uint16_t initial_crc) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint16_t crc = initial_crc;

    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];

        for (int j = 0; j < 8; ++j) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ 0xA001;  // CRC-CCITT polynomial
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}
#endif