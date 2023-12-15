/*
 *  Lab problem set for INP course
 *  by Chun-Ying Huang <chuang@cs.nctu.edu.tw>
 *  License: GPLv2
 */
#include "header.hpp"

// #define DUMPIP 1
// #define DUMPINFO 1

// XXX: implement your server codes here ...
int tunvpn_server(int port) {
    // create socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) errquit("UDP socket");

    // create tun0 device
    char tun_name[IFNAMSIZ];
    int32_t tun_fd = tun_alloc(tun_name);
    int16_t flag;
    int32_t ip = htonl(MYADDR);
    int32_t broadcast_ip = htonl(MYADDR + 1);
    int32_t netmask = htonl(NETMASK);
    int32_t mtu = 1400;
    if (tun_fd < 0) errquit("tun_alloc");

    // set tun0 device
    ifreq_set_mtu(sock, "tun0", mtu);
    ifreq_get_flag(sock, "tun0", &flag);
    ifreq_set_flag(sock, "tun0", flag | IFF_UP | IFF_RUNNING);
    ifreq_set_addr(sock, "tun0", ip);
    ifreq_set_netmask(sock, "tun0", netmask);
    ifreq_set_broadcast(sock, "tun0", broadcast_ip);
    printf("Virtual IP: %u.%u.%u.%u\n", NIPQUAD(ip));

    // bind UDP socket
    struct sockaddr_in UDP_addr;
    UDP_addr.sin_family = AF_INET;
    UDP_addr.sin_port = htons(port);
    // get server IP by server's name: server
    struct hostent *he = gethostbyname("server");
    if (he == NULL) errquit("gethostbyname");
    UDP_addr.sin_addr = *(struct in_addr *)he->h_addr;
    printf("Server address: %u.%u.%u.%u\n", NIPQUAD(UDP_addr.sin_addr.s_addr));
    if (bind(sock, (struct sockaddr *)&UDP_addr, sizeof(UDP_addr)) < 0) errquit("bind");

    struct Packet packet;
    std::map<uint32_t, struct sockaddr_in> client_addr;

    // set up select
    fd_set readset;
    int maxfd = std::max(sock, tun_fd);
    char buffer[BUF_SIZE];
    memset(buffer, 0, sizeof(buffer));

    uint32_t client_num = 0;

    // Use while loop recv message from UDP socket or raw socket
    while (true) {
        FD_ZERO(&readset);
        FD_SET(tun_fd, &readset);
        FD_SET(sock, &readset);
        int nready = select(maxfd + 1, &readset, NULL, NULL, NULL);
        if (nready < 0) errquit("select");
        if (FD_ISSET(sock, &readset)) {
            // recv packet from client and save into buffer
            socklen_t addrlen = sizeof(UDP_addr);
            ssize_t n = recvfrom(sock, &buffer, sizeof(buffer), 0, (struct sockaddr *)&UDP_addr, &addrlen);
            if (n < 0) errquit("UDP recvfrom");
#ifdef DUMPINFO
            printf("Received UDP packet from client\n");
#endif
            // use the return value of recvfrom to get the actual size of data
            ssize_t size = n;

            if (size == sizeof(packet)) {
                // cast buffer to packet
                memcpy(&packet, buffer, sizeof(packet));
                // check if the packet is config packet
                if (packet.virtual_ip == 0) {
                    // send config packet to client
                    uint32_t virtual_ip = htonl(ADDRBASE + client_num);
                    packet.virtual_ip = virtual_ip;
                    n = sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&UDP_addr, sizeof(UDP_addr));
                    if (n < 0) errquit("sendto");
                    printf("Assign virtual IP: %u.%u.%u.%u to client %d\n", NIPQUAD(packet.virtual_ip), client_num + 1);
                    // save the virtual IP of the client i
                    client_addr[virtual_ip] = UDP_addr;
                    client_num++;

#ifdef DUMPIP
                    printf("Client address table:\n");
                    for (auto it = client_addr.begin(); it != client_addr.end(); it++) {
                        printf("Virtual IP: %u.%u.%u.%u\n", NIPQUAD(it->first));
                        printf("Client address: %u.%u.%u.%u\n", NIPQUAD(it->second.sin_addr.s_addr));
                    }
#endif
                    // print the UDP address of client
                    printf("Client actual address: %u.%u.%u.%u\n", NIPQUAD(UDP_addr.sin_addr.s_addr));
                    continue;
                } else {
                    // Error
                    printf("Error: virtual IP is not 0\n");
                }
                continue;
            }
            // cast the buffer to ip header
            struct iphdr *ip = (struct iphdr *)buffer;

            if (ip->daddr == htonl(MYADDR)) {
#ifdef DUMPINFO
                printf("Send packet to tun0\n");
#endif
                // send packet to tun0
                n = write(tun_fd, buffer, n);
                if (n < 0) errquit("tun write");
                continue;
            }
            // lookup the address table
            auto it = client_addr.find(ip->daddr);
            if (it == client_addr.end()) {
                printf("Error: cannot find the virtual IP\n");
                continue;
            }
            struct sockaddr_in client = it->second;
#ifdef DUMPINFO
            printf("Send packet to IP: %u.%u.%u.%u\n", NIPQUAD(it->first));
#endif
            n = sendto(sock, buffer, n, 0, (struct sockaddr *)&client, sizeof(client));
            if (n < 0) errquit("sendto");
        }
        if (FD_ISSET(tun_fd, &readset)) {
// recv packet from tun0
#ifdef DUMPINFO
            printf("Recv packet from tun0\n");
#endif
            ssize_t n = read(tun_fd, buffer, sizeof(buffer));
            if (n < 0) errquit("tun read");

            // dump the ip header
            struct iphdr *ip = (struct iphdr *)buffer;
#ifdef DUMPIP
            printf("IP header: ");
            printf("saddr: %u.%u.%u.%u, ", NIPQUAD(ip->saddr));
            printf("daddr: %u.%u.%u.%u\n", NIPQUAD(ip->daddr));
#endif

            uint32_t virtual_ip = ip->daddr;
            // lookup the address table
            auto it = client_addr.find(virtual_ip);
            if (it == client_addr.end()) {
                printf("Error: cannot find the virtual IP\n");
                continue;
            }

            // send packet to client
            struct sockaddr_in client = it->second;
#ifdef DUMPINFO
            printf("Send packet to IP: %u.%u.%u.%u\n", NIPQUAD(it->first));
#endif
            n = sendto(sock, buffer, n, 0, (struct sockaddr *)&client, sizeof(client));
            if (n < 0) errquit("sendto");
        }
        // sleep(3);
    }

    return 0;
}

// XXX: implement your client codes here ...
int tunvpn_client(const char *server, int port) {
    // create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) errquit("socket");

    // connect to the server
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // get server IP by server's name: server
    struct hostent *he = gethostbyname("server");
    if (he == NULL) errquit("gethostbyname");
    addr.sin_addr = *(struct in_addr *)he->h_addr;
    // print address
    printf("Server address: %u.%u.%u.%u\n", NIPQUAD(addr.sin_addr.s_addr));
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) errquit("connect");

    // use sendto to send packet to server
    struct Packet config;
    config.virtual_ip = 0;

    printf("Send config packet to server\n");
    int n = sendto(sock, &config, sizeof(config), 0, (struct sockaddr *)&addr, sizeof(addr));
    if (n < 0) errquit("sendto");

    printf("Receiving config packet from server...\n");
    // recv config packet from server
    socklen_t addrlen = sizeof(addr);
    n = recvfrom(sock, &config, sizeof(config), 0, (struct sockaddr *)&addr, &addrlen);
    if (n < 0) errquit("recvfrom");
    printf("Recv config packet from server\n");
    // create tun0 device
    char tun_name[IFNAMSIZ] = "tun0";
    int32_t tun_fd = tun_alloc(tun_name);
    int16_t flag;
    uint32_t virtual_ip = config.virtual_ip;
    int32_t broadcast_ip = htonl(MYADDR + 1);
    int32_t netmask = htonl(NETMASK);
    int32_t mtu = 1400;
    if (tun_fd < 0) errquit("tun_alloc");

    // set tun0 device
    ifreq_set_mtu(sock, "tun0", mtu);
    ifreq_get_flag(sock, "tun0", &flag);
    ifreq_set_flag(sock, "tun0", flag | IFF_UP | IFF_RUNNING);
    ifreq_set_addr(sock, "tun0", virtual_ip);
    ifreq_set_netmask(sock, "tun0", netmask);
    ifreq_set_broadcast(sock, "tun0", broadcast_ip);
    printf("Virtual IP: %u.%u.%u.%u\n", NIPQUAD(virtual_ip));

    // set up select
    fd_set readset;
    int maxfd = std::max(sock, tun_fd);
    char buffer[BUF_SIZE];
    memset(buffer, 0, sizeof(buffer));

    while (1) {
        FD_ZERO(&readset);
        FD_SET(sock, &readset);
        FD_SET(tun_fd, &readset);
        int nready = select(maxfd + 1, &readset, NULL, NULL, NULL);
        if (nready < 0) errquit("select");

        // recv packet from server
        if (FD_ISSET(sock, &readset)) {
#ifdef DUMPINFO
            printf("Recv UDP packet from server\n");
#endif

            // recv packet from server and save into buffer
            socklen_t addrlen = sizeof(addr);
            ssize_t n = recvfrom(sock, &buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &addrlen);
            if (n < 0) errquit("UDP recvfrom");

            // cast the buffer to ip header
            struct iphdr *ip = (struct iphdr *)buffer;

// dump the ip header
#ifdef DUMPIP
            printf("IP header: ");
            printf("saddr: %u.%u.%u.%u, ", NIPQUAD(ip->saddr));
            printf("daddr: %u.%u.%u.%u\n", NIPQUAD(ip->daddr));
#endif
            if (ip->daddr != virtual_ip) {
                printf("Error: virtual IP is not correct\n");
                printf("Virtual IP: %u.%u.%u.%u\n", NIPQUAD(virtual_ip));
                continue;
            }
            // send packet to tun0
            n = write(tun_fd, buffer, n);
            if (n < 0) errquit("tun write");
        }
        // recv packet from tun0
        if (FD_ISSET(tun_fd, &readset)) {
#ifdef DUMPINFO
            printf("Recv packet from tun0\n");
#endif

            // recv packet from tun0 and save into buffer
            ssize_t n = read(tun_fd, buffer, sizeof(buffer));
            if (n < 0) errquit("tun read");

            // cast the buffer to ip header
            struct iphdr *ip = (struct iphdr *)buffer;

// dump the ip header
#ifdef DUMP
            printf("IP header: ");
            // printf("tos: %d, ", ip->tos);
            // printf("tot_len: %d, ", ip->tot_len);
            // printf("id: %d, ", ip->id);
            // printf("frag_off: %d, ", ip->frag_off);
            // printf("ttl: %d, ", ip->ttl);
            // printf("protocol: %d, ", ip->protocol);
            // printf("check: %d, ", ip->check);
            printf("saddr: %u.%u.%u.%u, ", NIPQUAD(ip->saddr));
            printf("daddr: %u.%u.%u.%u\n", NIPQUAD(ip->daddr));
#endif

            // send the buffer to server
            n = sendto(sock, buffer, n, 0, (struct sockaddr *)&addr, sizeof(addr));
            if (n < 0) errquit("sendto");
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        return usage(argv[0]);
    }
    if (strcmp(argv[1], "server") == 0) {
        if (argc < 3) return usage(argv[0]);
        return tunvpn_server(strtol(argv[2], NULL, 0));
    } else if (strcmp(argv[1], "client") == 0) {
        if (argc < 4) return usage(argv[0]);
        return tunvpn_client(argv[2], strtol(argv[3], NULL, 0));
    } else {
        fprintf(stderr, "## unknown mode %s\n", argv[1]);
    }
    return 0;
}
