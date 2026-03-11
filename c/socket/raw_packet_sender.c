// packet_sender.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>

int main() {
    int sockfd;
    struct sockaddr_ll sll;
    unsigned char frame[1500];

    // 1. Create raw socket
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Get interface index
    int ifindex = if_nametoindex("eth0"); // replace with your interface
    if (ifindex == 0) {
        perror("if_nametoindex");
        exit(EXIT_FAILURE);
    }

    // 3. Prepare destination
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifindex;
    sll.sll_halen = ETH_ALEN;
    sll.sll_addr[0] = 0xff; // Broadcast address
    sll.sll_addr[1] = 0xff;
    sll.sll_addr[2] = 0xff;
    sll.sll_addr[3] = 0xff;
    sll.sll_addr[4] = 0xff;
    sll.sll_addr[5] = 0xff;

    // 4. Build Ethernet frame
    struct ethhdr *eth = (struct ethhdr *)frame;
    memset(eth->h_dest, 0xff, ETH_ALEN);       // Destination: broadcast
    memset(eth->h_source, 0x11, ETH_ALEN);     // Fake source
    eth->h_proto = htons(0x88B5);              // Custom Ethertype
    strcpy((char *)(frame + sizeof(struct ethhdr)), "Hello Raw World!");

    // 5. Send
    ssize_t frame_len = sizeof(struct ethhdr) + strlen("Hello Raw World!");
    if (sendto(sockfd, frame, frame_len, 0, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
        perror("sendto");
    } else {
        printf("Sent raw Ethernet frame (%zd bytes)\n", frame_len);
    }

    close(sockfd);
    return 0;
}