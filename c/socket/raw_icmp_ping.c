// ping_raw.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>

// ICMP Header (as defined in <netinet/ip_icmp.h>)
struct icmphdr {
    uint8_t  type;      // e.g., ICMP_ECHO
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
};

unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;
    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: sudo %s <destination IP>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in dest;
    char packet[64];
    struct icmphdr *icmp = (struct icmphdr*)packet;

    // 1. Create raw socket for ICMP
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Setup destination address
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &dest.sin_addr);

    // 3. Build ICMP Echo Request
    memset(packet, 0, sizeof(packet));
    icmp->type = ICMP_ECHO;   // 8
    icmp->code = 0;
    icmp->un.echo.id = getpid() & 0xFFFF;
    icmp->un.echo.sequence = 1;
    icmp->checksum = 0;
    icmp->checksum = checksum(packet, sizeof(packet));

    // 4. Record time and send
    struct timeval start, end;
    gettimeofday(&start, NULL);

    if (sendto(sockfd, packet, sizeof(packet), 0,
               (struct sockaddr*)&dest, sizeof(dest)) <= 0) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    // 5. Wait for reply
    char recvbuf[1024];
    struct sockaddr_in reply_addr;
    socklen_t addr_len = sizeof(reply_addr);

    ssize_t n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0,
                         (struct sockaddr*)&reply_addr, &addr_len);
    if (n < 0) {
        perror("recvfrom");
        exit(EXIT_FAILURE);
    }

    gettimeofday(&end, NULL);

    double rtt = (end.tv_sec - start.tv_sec) * 1000.0 +
                 (end.tv_usec - start.tv_usec) / 1000.0;

    printf("Reply from %s: bytes=%zd time=%.3f ms\n",
           argv[1], n, rtt);

    close(sockfd);
    return 0;
}