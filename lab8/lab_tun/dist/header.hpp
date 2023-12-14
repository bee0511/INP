#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <netdb.h>
#include <netinet/icmp6.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <vector>

#define NIPQUAD(m) ((unsigned char *)&(m))[0], ((unsigned char *)&(m))[1], ((unsigned char *)&(m))[2], ((unsigned char *)&(m))[3]
#define errquit(m) \
    {              \
        perror(m); \
        exit(-1);  \
    }

#define MYADDR 0x0a0000fe
#define ADDRBASE 0x0a00000a
#define NETMASK 0xffffff00

#define MAX_CLIENTS 10
#define MAX_EVENTS 10
#define BUF_SIZE 1500
struct Packet {
    uint32_t virtual_ip;
    uint32_t src_addr;  // source address
    uint32_t dst_addr;  // destination address
};
