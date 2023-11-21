#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 12345
#define BUFFER_SIZE 1024 * 1024 * 2  // 2MB
#define NUM_MEASURE 100
#define NUM_SKIPS 10
#define NUM_SKIP_BEGIN 25
#define NUM_SKIP_END 10

float total_bytes = 0;
int skips = NUM_SKIPS;

int compare(const void* a, const void* b) {
    return *(int*)a - *(int*)b;
}
int createTCPConnection() {
    int sockfd;
    struct sockaddr_in servaddr;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("client socket fail\n");
        return 0;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    if ((connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) < 0) {
        perror("client connect fail\n");
        return 0;
    }
    return sockfd;
}

long getDelay(int sockfd) {
    struct timeval start, end;
    char buf[1];
    memset(buf, '\0', sizeof(buf));

    gettimeofday(&start, NULL);
    recv(sockfd, buf, sizeof(buf), 0);
    gettimeofday(&end, NULL);

    return ((end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec) / 1000;  // delay in ms
}

int getBandwidth(int sockfd) {
    char buf[BUFFER_SIZE];
    int num_packet = 0;
    int bw_arr[NUM_MEASURE];
    long bytesRead;
    struct timeval start, end;

    memset(bw_arr, 0, sizeof(bw_arr));

    for (int i = 0; i < NUM_MEASURE; i++) {
        gettimeofday(&start, NULL);

        memset(buf, '\0', sizeof(buf));
        bytesRead = recv(sockfd, buf, sizeof(buf), 0);
        buf[bytesRead] = '\0';

        gettimeofday(&end, NULL);
        bw_arr[num_packet] = bytesRead * 8 / ((end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec);

        num_packet++;
    }

    qsort(bw_arr, NUM_MEASURE, sizeof(int), compare);
    int total_count = 0;
    int total_bw = 0;

    for (int i = NUM_SKIP_BEGIN; i < NUM_MEASURE - NUM_SKIP_END; i++) {
        total_bw += bw_arr[i];
        total_count++;
    }

    return total_bw / total_count;
}

int getBandwidthHarmonic(int sockfd) {
    struct timeval start, end;
    char buf[BUFFER_SIZE];
    long bytesRead;
    int num_packet = 0;
    int bandwidth = -1;
    for (int i = 0; i < NUM_MEASURE; i++) {
        gettimeofday(&start, NULL);

        memset(buf, '\0', sizeof(buf));
        bytesRead = recv(sockfd, buf, sizeof(buf), 0);
        total_bytes += bytesRead;
        buf[bytesRead] = '\0';

        gettimeofday(&end, NULL);
        if (skips) {
            skips--;
            continue;
        }
        double delay = ((end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec);

        // bytesRead *= 8;                                    // 8 bits = 1 byte
        double cur_bw = (total_bytes / 1024 * 1024) / delay;  // Mbps
        if (bandwidth == -1) {
            bandwidth = cur_bw;
        } else {
            bandwidth = (num_packet + 1) / ((num_packet / bandwidth) + (1 / cur_bw));
        }
        num_packet++;
    }
    return bandwidth;
}

int main() {
    int sockfd = createTCPConnection();

    long delay = getDelay(sockfd);
    int bandwidth = getBandwidth(sockfd);
    // int bandwidth = getBandwidthHarmonic(sockfd);

    printf("# RESULTS: delay = %ld ms, bandwidth = %d Mbps\n", delay / 2, bandwidth);

    close(sockfd);

    return 0;
}