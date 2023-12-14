/*
 *  Lab problem set for INP course
 *  by Chun-Ying Huang <chuang@cs.nctu.edu.tw>
 *  License: GPLv2
 */
#include "header.hpp"

/*
Allocate a tun device.
The size of dev must be at least IFNAMSIZ long.
It can be an empty string, where the system will automatically generate the device name.
Alternatively, a user may choose a specific tunNN device name.
The return value is the descriptor to the opened tun device.
*/
int tun_alloc(char *dev) {
    struct ifreq ifr;
    int fd, err;
    if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
        return -1;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI; /* IFF_TUN (L3), IFF_TAP (L2), IFF_NO_PI (w/ header) */
    if (dev && dev[0] != '\0') strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
        close(fd);
        return err;
    }
    if (dev) strcpy(dev, ifr.ifr_name);
    return fd;
}

int ifreq_set_mtu(int fd, const char *dev, int mtu) {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_mtu = mtu;
    if (dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    return ioctl(fd, SIOCSIFMTU, &ifr);
}

/*
int ifreq_{set|get}_flag(int fd, const char *dev, ...):
Set the flag value of a given network device dev.
Generally, you must call ifreq_get_flag first to obtain the current flag value,
modify the flag value, and then call ifreq_set_flag to update the flag value.
To bring up a network interface, mark the IFF_UP in the flag using the OR operation.
*/
int ifreq_get_flag(int fd, const char *dev, short *flag) {
    int err;
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    if (dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    err = ioctl(fd, SIOCGIFFLAGS, &ifr);
    if (err == 0) {
        *flag = ifr.ifr_flags;
    }
    return err;
}

/*
int ifreq_{set|get}_flag(int fd, const char *dev, ...):
Set the flag value of a given network device dev.
Generally, you must call ifreq_get_flag first to obtain the current flag value,
modify the flag value, and then call ifreq_set_flag to update the flag value.
To bring up a network interface, mark the IFF_UP in the flag using the OR operation.
*/
int ifreq_set_flag(int fd, const char *dev, short flag) {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    if (dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    ifr.ifr_flags = flag;
    return ioctl(fd, SIOCSIFFLAGS, &ifr);
}

int ifreq_set_sockaddr(int fd, const char *dev, int cmd, unsigned int addr) {
    struct ifreq ifr;
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = addr;
    memset(&ifr, 0, sizeof(ifr));
    memcpy(&ifr.ifr_addr, &sin, sizeof(struct sockaddr));
    if (dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    return ioctl(fd, cmd, &ifr);
}

/*
Set the IPv4 network of a given network device dev.
The fd parameter must be a valid socket.
*/
int ifreq_set_addr(int fd, const char *dev, unsigned int addr) {
    return ifreq_set_sockaddr(fd, dev, SIOCSIFADDR, addr);
}

/*
Set the netmask of a given network device dev.
The fd parameter must be a valid socket.
*/
int ifreq_set_netmask(int fd, const char *dev, unsigned int addr) {
    return ifreq_set_sockaddr(fd, dev, SIOCSIFNETMASK, addr);
}

/*
Set the broadcast address of a given network device dev.
The fd parameter must be a valid socket.
*/
int ifreq_set_broadcast(int fd, const char *dev, unsigned int addr) {
    return ifreq_set_sockaddr(fd, dev, SIOCSIFBRDADDR, addr);
}

// XXX: implement your server codes here ...
int tunvpn_server(int port) {
    // create socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) errquit("socket");

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

    // bind socket
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // get server IP by server's name: server
    struct hostent *he = gethostbyname("server");
    if (he == NULL) errquit("gethostbyname");
    addr.sin_addr = *(struct in_addr *)he->h_addr;
    printf("Server address: %u.%u.%u.%u\n", NIPQUAD(addr.sin_addr.s_addr));
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) errquit("bind");

    // Use while loop to receive packet from client
    struct Packet packet;
    std::vector<uint32_t> address_table;  // store virtual client address
    while (true) {
        printf("Receiving packet from client...\n");
        socklen_t addrlen = sizeof(addr);
        int n = recvfrom(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&addr, &addrlen);
        if (n < 0) errquit("recvfrom");
        printf("Recv packet from client\n");
        // check if the packet is config packet
        if (packet.virtual_ip == 0) {
            // Assign virtual IP to client
            packet.virtual_ip = htonl(ADDRBASE + address_table.size());
            packet.src_addr = htonl(MYADDR);
            packet.dst_addr = packet.virtual_ip;
            // store virtual client address
            address_table.push_back(packet.virtual_ip);
            // send config packet to client
            printf("Send config packet to client\n");
            n = sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&addr, sizeof(addr));
            if (n < 0) errquit("sendto");
        } else {
            // send packet to client
            printf("Send packet to client\n");
            n = sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&addr, sizeof(addr));
            if (n < 0) errquit("sendto");
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

    pause();
    return 0;
}

int usage(const char *progname) {
    fprintf(stderr,
            "usage: %s {server|client} {options ...}\n"
            "# server mode:\n"
            "	%s server port\n"
            "# client mode:\n"
            "	%s client servername serverport\n",
            progname, progname, progname);
    return -1;
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
