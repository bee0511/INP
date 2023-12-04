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
#include <vector>

#define err_quit(m) \
    {               \
        perror(m);  \
        exit(-1);   \
    }

// Used to define the special file number
#define NO_ACK 65535
#define ACK_ERROR 20000
#define FINISH 30000

#define PACKET_SIZE 1200        // each packet size
#define INITIAL_WIN_SIZE 1000   // How many files can the server ask for in the same time
#define CLI_INIT_SEND 1000      // How many files can the client send in the beginning
#define CLI_TIMEOUT 500 * 1000  // usecond, 500 * 1000 = 500ms
#define SRV_TIMEOUT 500 * 1000  // usecond, 500 * 1000 = 500ms
#define INIT_RETRY 5            // How many times the server will send the init ack
#define MAX_PACKETS 48          // Used to set the size of the stored_packet vector in Ack struct
#define NUM_THREADS 4           // How many threads will be created
// #define ENABLE_ACK_CKSUM 1     // Whether to enable checksum

#define DUMPINIT 1  // dump the init ack info
// #define DUMPSRV 1   // dump the server info
// #define DUMPCLI 1   // dump the client info

#ifndef HEADER_H
#define HEADER_H
struct Packet {
    uint16_t file_number;
    uint16_t packet_number;
    uint16_t total_packets;
    uint16_t checksum;
    uint16_t length;
    char data[PACKET_SIZE];
};

struct Ack {
    uint16_t file_number;
    bool stored_packet[MAX_PACKETS];
#ifdef ENABLE_ACK_CKSUM
    uint16_t checksum;
#endif
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