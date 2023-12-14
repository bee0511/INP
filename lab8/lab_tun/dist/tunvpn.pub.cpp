/*
 *  Lab problem set for INP course
 *  by Chun-Ying Huang <chuang@cs.nctu.edu.tw>
 *  License: GPLv2
 */
#include "header.hpp"

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
    std::vector<struct sockaddr_in> client_addr;  // client address table

    // set up select
    fd_set readset;
    int maxfd = std::max(sock, tun_fd);
    char buffer[BUF_SIZE];
    memset(buffer, 0, sizeof(buffer));

    // Use while loop recv message from UDP socket or raw socket
    while (true) {
        FD_ZERO(&readset);
        FD_SET(sock, &readset);
        int nready = select(maxfd + 1, &readset, NULL, NULL, NULL);
        if (nready < 0) errquit("select");
        if (FD_ISSET(sock, &readset)) {
            // recv packet from client and save into buffer
            socklen_t addrlen = sizeof(UDP_addr);
            ssize_t n = recvfrom(sock, &buffer, sizeof(buffer), 0, (struct sockaddr *)&UDP_addr, &addrlen);
            if (n < 0) errquit("UDP recvfrom");
            // printf("Received UDP packet from client\n");

            // use the return value of recvfrom to get the actual size of data
            ssize_t size = n;
            printf("Packet size: %zd\n", size);

            if (size == sizeof(packet)) {
                // cast buffer to packet
                memcpy(&packet, buffer, sizeof(packet));
                // check if the packet is config packet
                if (packet.virtual_ip == 0) {
                    // send config packet to client
                    packet.virtual_ip = htonl(ADDRBASE + client_addr.size());
                    n = sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&UDP_addr, sizeof(UDP_addr));
                    if (n < 0) errquit("sendto");
                    printf("Assign virtual IP: %u.%u.%u.%u to client\n", NIPQUAD(packet.virtual_ip));
                    // add virtual IP to address table
                    client_addr.push_back(UDP_addr);
                    // print the UDP address of client
                    printf("Client address: %u.%u.%u.%u\n", NIPQUAD(UDP_addr.sin_addr.s_addr));
                }
            } else {
                // send packet to tun0
                n = write(tun_fd, buffer, size);
                if (n < 0) errquit("tun write");

                // recv packet from tun0
                n = read(tun_fd, buffer, sizeof(buffer));
                if (n < 0) errquit("tun read");
                ssize_t size = n;

                // parse the packet
                struct ipheader *iph = (struct ipheader *)buffer;
                struct icmpheader *icmph = (struct icmpheader *)(buffer + sizeof(struct ipheader));
                if (icmph->icmp_type == ICMP_ECHO || icmph->icmp_type == ICMP_ECHOREPLY) {
                    struct icmpecho *echo = (struct icmpecho *)icmph;
                    printf("ICMP id: %u\n", ntohs(echo->id));
                    printf("ICMP seq: %u\n", ntohs(echo->seq));
                    printf("ICMP type: %u\n", icmph->icmp_type);
                    printf("ICMP code: %u\n", icmph->icmp_code);
                    //print source IP
                    printf("Source IP: %u.%u.%u.%u\n", NIPQUAD(iph->iph_sourceip));
                    //print destination IP
                    printf("Destination IP: %u.%u.%u.%u\n", NIPQUAD(iph->iph_destip));
                }

                // send packet to client, dont use table
                n = sendto(sock, buffer, size, 0, (struct sockaddr *)&UDP_addr, sizeof(UDP_addr));
                if (n < 0) errquit("sendto");
            }
        }
    }

    pause();

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
    int32_t ip = config.virtual_ip;
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

    while (1) {
        char buffer[BUF_SIZE];
        memset(buffer, 0, sizeof(buffer));
        int n = read(tun_fd, buffer, sizeof(buffer));
        if (n < 0) errquit("tun read");
        // send the buffer to server
        int sent = sendto(sock, buffer, n, 0, (struct sockaddr *)&addr, sizeof(addr));
        if (sent < 0) errquit("sendto");
        // printf("Send packet to server\n");
        int recv = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &addrlen);
        if (recv < 0) errquit("recvfrom");
        // printf("Recv packet from server\n");
        // write the buffer to tun0
        n = write(tun_fd, buffer, recv);
        if (n < 0) errquit("tun write");
    }
    pause();
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
