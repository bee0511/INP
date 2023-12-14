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

struct icmpheader {
    unsigned char icmp_type;
    unsigned char icmp_code;
    unsigned short int icmp_chksum;
};

struct icmpecho {
    struct icmpheader icmp;
    unsigned short int id;
    unsigned short int seq;
};

struct ipheader {
    unsigned char iph_ihl : 4, iph_ver : 4;
    unsigned char iph_tos;
    unsigned short int iph_len;
    unsigned short int iph_ident;
    unsigned char iph_flag : 3, iph_offset : 13;
    unsigned char iph_ttl;
    unsigned char iph_protocol;
    unsigned short int iph_chksum;
    unsigned int iph_sourceip;
    unsigned int iph_destip;
};

int tun_alloc(char *dev);
int ifreq_set_mtu(int fd, const char *dev, int mtu);
int ifreq_get_flag(int fd, const char *dev, short *flag);
int ifreq_set_flag(int fd, const char *dev, short flag);
int ifreq_set_sockaddr(int fd, const char *dev, int cmd, unsigned int addr);
int ifreq_set_addr(int fd, const char *dev, unsigned int addr);
int ifreq_set_netmask(int fd, const char *dev, unsigned int addr);
int ifreq_set_broadcast(int fd, const char *dev, unsigned int addr);
int tunvpn_server(int port);
int tunvpn_client(const char *server, int port);
int usage(const char *progname);