#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define err_quit(m) \
    {               \
        perror(m);  \
        exit(-1);   \
    }
#define PACKET_SIZE 1024
#define PORT 12345

#ifndef HEADER_H
#define HEADER_H
struct Packet {
    uint32_t file_number;
    uint32_t packet_number;
    uint32_t total_packets;
    uint16_t checksum;
    char data[PACKET_SIZE];
};
#endif