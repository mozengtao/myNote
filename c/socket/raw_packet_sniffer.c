// raw socket (requires root privileges)
/*
 * Raw sockets allow user-space programs to:
 * Capture or send raw Ethernet frames,
 * Bypass the TCP/UDP/IP stack,
 * Inspect headers for tools like tcpdump, Wireshark, or custom packet sniffers.
*/
// packet_sniffer.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>	// For ETH_P_ALL
#include <net/if.h>			// For if_nametoindex()

int main() {
    int sockfd;
    struct sockaddr_ll sll;
    unsigned char buffer[2048];

    // 1. Create a raw socket
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Bind to a specific network interface (e.g., eth0)
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = if_nametoindex("eth0"); // replace with your NIC name
    if (sll.sll_ifindex == 0) {
        perror("if_nametoindex");
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Listening on interface eth0 for raw Ethernet frames...\n");

    // 3. Receive packets
    while (1) {
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
        if (n < 0) {
            perror("recvfrom");
            break;
        }

        struct ethhdr *eth = (struct ethhdr *)buffer;

        printf("\n--- Ethernet Frame ---\n");
        printf("Destination MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               eth->h_dest[0], eth->h_dest[1], eth->h_dest[2],
               eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);
        printf("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               eth->h_source[0], eth->h_source[1], eth->h_source[2],
               eth->h_source[3], eth->h_source[4], eth->h_source[5]);
        printf("EtherType: 0x%04x\n", ntohs(eth->h_proto));
        printf("Payload length: %zd bytes\n", n - sizeof(struct ethhdr));
    }

    close(sockfd);
    return 0;
}