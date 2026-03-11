[](socket/tcp_server.c)  
[](socket/tcp_client.c)  
[](socket/udp_server.c)  
[](socket/udp_client.c)  
[](socket/unix_server.c)  
[](socket/unix_client.c)  
[raw socket](socket/raw_packet_sniffer.c)  
[](socket/raw_packet_sender.c)  
[](socket/raw_icmp_ping.c)  
[](socket/raw_)  
[](socket/)  
[](socket/)  
## socket
```c
//
#include <sys/types.h>
#include <sys/socket.h>

/*
 * domain: specifies an address family.
 * type: specifies communication semantics.
 * protocol: specifies a concrete protocol type
*/
int socket(int domain, int type, int protocol);

// domain
/*
| Macro                  | Description                     | Struct Used               | Typical Use                       |
| ---------------------- | ------------------------------- | ------------------------- | --------------------------------- |
| `AF_INET`              | IPv4 Internet protocols         | `struct sockaddr_in`      | IPv4 TCP/UDP communication        |
| `AF_INET6`             | IPv6 Internet protocols         | `struct sockaddr_in6`     | IPv6 TCP/UDP communication        |
| `AF_UNIX` / `AF_LOCAL` | Local communication (same host) | `struct sockaddr_un`      | UNIX domain sockets               |
| `AF_PACKET`            | Raw packet access (Layer 2)     | `struct sockaddr_ll`      | Network sniffers, raw Ethernet    |
| `AF_NETLINK`           | Kernel-user communication       | `struct sockaddr_nl`      | Routing, Netfilter, kernel events |
| `AF_BLUETOOTH`         | Bluetooth protocol              | `struct sockaddr_rc` etc. | Bluetooth communication           |

// type
| Macro            | Description                          | Semantics                          |
| ---------------- | ------------------------------------ | ---------------------------------- |
| `SOCK_STREAM`    | Stream-oriented (connection-based)   | Reliable byte stream (TCP)         |
| `SOCK_DGRAM`     | Datagram-oriented (connectionless)   | Unreliable message (UDP)           |
| `SOCK_RAW`       | Raw network protocol access          | Direct access to IP layer          |
| `SOCK_SEQPACKET` | Sequenced, reliable, record-oriented | Used with UNIX domain or Bluetooth |
| `SOCK_PACKET`    | (Deprecated) raw packets             | Legacy code only                   |

// protocol
| Domain      | Type          | Protocol           | Meaning                     |
| ----------- | ------------- | ------------------ | --------------------------- |
| `AF_INET`   | `SOCK_STREAM` | `IPPROTO_TCP`      | TCP                         |
| `AF_INET`   | `SOCK_DGRAM`  | `IPPROTO_UDP`      | UDP                         |
| `AF_INET`   | `SOCK_RAW`    | `IPPROTO_ICMP`     | Raw ICMP socket             |
| `AF_PACKET` | `SOCK_RAW`    | `htons(ETH_P_ALL)` | Capture all Ethernet frames |
| `AF_UNIX`   | `SOCK_STREAM` | `0`                | Local stream socket         |
*/

struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
}



// ipv4
struct sockaddr_in {
    sa_family_t    sin_family; // Address family (AF_INET)
    in_port_t      sin_port;   // Port number (network byte order)
    struct in_addr sin_addr;   // IP address
    unsigned char  sin_zero[8]; // Padding
};

struct in_addr {
  uint32_t       s_addr;     /* address in network byte order */
};

// ipv6
struct sockaddr_in6 {
    sa_family_t     sin6_family;   // AF_INET6
    in_port_t       sin6_port;     // Port number
    uint32_t        sin6_flowinfo; // Flow information
    struct in6_addr sin6_addr;     // IPv6 address
    uint32_t        sin6_scope_id; // Scope ID
};

struct in6_addr {
  unsigned char   s6_addr[16];   /* IPv6 address */
};

// unix domain socket
struct sockaddr_un {
    sa_family_t sun_family;              // AF_UNIX
    char        sun_path[108];           // Pathname
};


// function prototype for setsockopt
int setsockopt(int sockfd, int level, int optname,
               const void *optval, socklen_t optlen);

// SOL_SOCKET: socket level options
| Option                                |       Value type | Purpose / typical use                                                             |
| ------------------------------------- | ---------------: | --------------------------------------------------------------------------------- |
| `SO_REUSEADDR`                        |      `int` (0/1) | Allow binding to an address in `TIME_WAIT`. Common on servers to restart quickly. |
| `SO_REUSEPORT`                        |      `int` (0/1) | Allow multiple sockets bind same (addr,port) (load-sharing on some kernels).      |
| `SO_KEEPALIVE`                        |      `int` (0/1) | Enable TCP keepalive probes (basic on/off; details set via TCP_* options).        |
| `SO_BROADCAST`                        |      `int` (0/1) | Enable sending datagrams to broadcast addresses (UDP).                            |
| `SO_RCVBUF` / `SO_SNDBUF`             |    `int` (bytes) | Set kernel receive / send buffer sizes.                                           |
| `SO_RCVTIMEO` / `SO_SNDTIMEO`         | `struct timeval` | Set blocking I/O timeouts for `recv`/`send`.                                      |
| `SO_LINGER`                           |  `struct linger` | Control close behavior (block until sent or drop).                                |
| `SO_ERROR`                            |   `int` (output) | Query pending error on socket (use with `getsockopt`).                            |
| `SO_OOBINLINE`                        |      `int` (0/1) | Receive TCP OOB data inline with normal data.                                     |
| `SO_ACCEPTCONN`                       |   `int` (output) | Check if socket is listening (from `getsockopt`).                                 |
| `SO_DOMAIN`, `SO_TYPE`, `SO_PROTOCOL` |   `int` (output) | Query socket properties.                                                          |

// IPPROTO_TCP: TCP level options
| Option          |      Value type | Purpose                                                                         |
| --------------- | --------------: | ------------------------------------------------------------------------------- |
| `TCP_NODELAY`   |     `int` (0/1) | Disable Nagle (send small packets immediately). Useful for low-latency apps.    |
| `TCP_CORK`      |     `int` (0/1) | Linux: hold back partial frames until cork cleared (for packetization control). |
| `TCP_KEEPIDLE`  | `int` (seconds) | Idle time before first keepalive probe.                                         |
| `TCP_KEEPINTVL` | `int` (seconds) | Interval between keepalive probes.                                              |
| `TCP_KEEPCNT`   |           `int` | Number of probes before declaring connection dead.                              |
| `TCP_SYNCNT`    |           `int` | Number of SYN retransmits before aborting connect() (Linux).                    |
| `TCP_QUICKACK`  |           `int` | Control delayed ACK behavior (Linux).                                           |

// IPPROTO_IP / IP: IPv4 level options
| Option               | Value type / struct       | Purpose                                                  |
| -------------------- | ------------------------- | -------------------------------------------------------- |
| `IP_TTL`             | `int`                     | Set IP time-to-live for outgoing packets.                |
| `IP_MULTICAST_TTL`   | `unsigned char`           | TTL for IPv4 multicast packets.                          |
| `IP_MULTICAST_LOOP`  | `unsigned char`           | Loopback for multicast (0/1).                            |
| `IP_MULTICAST_IF`    | `struct in_addr` or `int` | Choose outgoing interface for multicast.                 |
| `IP_ADD_MEMBERSHIP`  | `struct ip_mreq`          | Join IPv4 multicast group.                               |
| `IP_DROP_MEMBERSHIP` | `struct ip_mreq`          | Leave multicast group.                                   |
| `IP_PKTINFO`         | `int`                     | Receive destination address and iface info in `recvmsg`. |
| `IP_HDRINCL`         | `int` (0/1)               | Include custom IP header when sending (raw sockets).     |

// IPPROTO_IPV6: IPv6 level options
| Option                |         Value type | Purpose                                                          |
| --------------------- | -----------------: | ---------------------------------------------------------------- |
| `IPV6_JOIN_GROUP`     | `struct ipv6_mreq` | Join IPv6 multicast group.                                       |
| `IPV6_LEAVE_GROUP`    | `struct ipv6_mreq` | Leave group.                                                     |
| `IPV6_MULTICAST_HOPS` |              `int` | Multicast hop limit (TTL).                                       |
| `IPV6_V6ONLY`         |              `int` | If set, socket will accept only IPv6 (no IPv4-mapped addresses). |
| `IPV6_PKTINFO`        |              `int` | Similar to `IP_PKTINFO` for IPv6.                                |

// SOL_SOCKET for advanced use
SO_ATTACH_FILTER / SO_DETACH_FILTER — attach BPF filter (raw capture) (Linux).
SO_PASSCRED — get sender credentials on Unix domain sockets.
SO_TIMESTAMP / SO_TIMESTAMPNS / SO_TIMESTAMPING — enable kernel packet timestamps.

// Typical application usage
TCP server that wants fast restart
	SO_REUSEADDR (and sometimes SO_REUSEPORT) before bind().
High-throughput network app
	Increase SO_RCVBUF / SO_SNDBUF and tune TCP window scaling.
Low-latency app (e.g., RPC, games)
	TCP_NODELAY (disable Nagle) + smaller send batching.
Long-lived idle connections (NAT/firewall keepalive)
	SO_KEEPALIVE + TCP_KEEPIDLE / TCP_KEEPINTVL / TCP_KEEPCNT.
UDP multicast receiver
	IP_ADD_MEMBERSHIP to join group, IP_MULTICAST_IF to pick interface.
Raw packet generation
	IP_HDRINCL if you want to provide the IP header.
Non-blocking I/O with timeout fallback
	SO_RCVTIMEO / SO_SNDTIMEO (or set non-blocking + select/poll/epoll).
Graceful close vs force close
	SO_LINGER with l_onoff controls whether close() blocks to send pending data.

```

## AF_UNIX (local IPC)
```c
// server.c
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(void) {
	int fd;
	struct sockaddr_un addr;
	const char *path = "/tmp/example.sock";
	char buf[256];
	ssize_t n;
	struct sockaddr_un peer;
	socklen_t peerlen = sizeof(peer);

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) return 1;

	unlink(path); // ensure clean
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;

	n = recvfrom(fd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&peer, &peerlen);
	if (n > 0) {
		buf[n] = '\0';
		printf("got: %s\n", buf);
	}
	close(fd);
	unlink(path);
	return 0;
}

// client.c
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>

int main(void) {
	int fd;
	struct sockaddr_un dst;
	const char *path = "/tmp/example.sock";
	const char *msg = "hello";

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) return 1;

	memset(&dst, 0, sizeof(dst));
	dst.sun_family = AF_UNIX;
	strncpy(dst.sun_path, path, sizeof(dst.sun_path) - 1);

	sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&dst, sizeof(dst));
	close(fd);
	return 0;
}

// AF_INET (TCP)
// server.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(void) {
	int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in addr = {0};
	int c;
	if (s < 0) return 1;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(8080);

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
	if (listen(s, 16) < 0) return 1;

	c = accept(s, NULL, NULL);
	if (c >= 0) {
		const char *resp = "hi\n";
		send(c, resp, 3, 0);
		close(c);
	}
	close(s);
	return 0;
}

// client.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

int main(void) {
	int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in addr = {0};
	if (s < 0) return 1;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
	send(s, "hello", 5, 0);
	close(s);
	return 0;
}

// AF_INET6 (IPv6 TCP)
// server.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

int main(void) {
	int s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in6 addr = {0};
	int v6only = 1;

	if (s < 0) return 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

	addr.sin6_family = AF_INET6;
	addr.sin6_addr = in6addr_any;
	addr.sin6_port = htons(8080);

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
	if (listen(s, 16) < 0) return 1;

	int c = accept(s, NULL, NULL);
	if (c >= 0) close(c);
	close(s);
	return 0;
}

// client.c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(void) {
	int s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in6 addr = {0};
	if (s < 0) return 1;

	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(8080);
	inet_pton(AF_INET6, "::1", &addr.sin6_addr);

	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
	close(s);
	return 0;
}
```

## AF_NETLINK  (kernel-user communication)
```c
// receiver.c
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(void) {
	int s = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	struct sockaddr_nl addr = {0};
	char buf[8192];
	struct nlmsghdr *nlh;

	if (s < 0) return 1;

	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid();      // unique user-space PID
	addr.nl_groups = RTMGRP_LINK; // subscribe to link events

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;

	ssize_t n = recv(s, buf, sizeof(buf), 0);
	if (n > 0) {
		nlh = (struct nlmsghdr *)buf;
		printf("got netlink msg type=%d len=%d\n", nlh->nlmsg_type, nlh->nlmsg_len);
	}
	close(s);
	return 0;
}

// sender.c
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <unistd.h>
#include <string.h>

int main(void) {
	int s = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	struct sockaddr_nl dst = {0};
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg gen;
	} req = {0};

	if (s < 0) return 1;

	dst.nl_family = AF_NETLINK;

	req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
	req.nlh.nlmsg_type = RTM_GETLINK;
	req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.gen.rtgen_family = AF_UNSPEC;

	struct iovec iov = { &req, req.nlh.nlmsg_len };
	struct msghdr msg = {0};
	msg.msg_name = &dst;
	msg.msg_namelen = sizeof(dst);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	if (sendmsg(s, &msg, 0) < 0) return 1;
	close(s);
	return 0;
}
```

## AF_PACKET (link layer, raw Ethernet; requires root)
```c
// receiver.c
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int main(void) {
	int s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	struct sockaddr_ll addr = {0};
	char buf[2048];
	ssize_t n;

	if (s < 0) return 1;

	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(ETH_P_ALL);
	addr.sll_ifindex = if_nametoindex("eth0");

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;

	n = recv(s, buf, sizeof(buf), 0);
	if (n > 0) printf("got %zd bytes\n", n);

	close(s);
	return 0;
}

// sender.c
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>

int main(void) {
	int s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
	struct sockaddr_ll dst = {0};
	unsigned char frame[64] = {0}; // fill with valid Ethernet frame

	if (s < 0) return 1;

	dst.sll_family = AF_PACKET;
	dst.sll_ifindex = if_nametoindex("eth0");
	dst.sll_halen = ETH_ALEN;
	// set dst.sll_addr[0..5] to target MAC

	sendto(s, frame, sizeof(frame), 0, (struct sockaddr *)&dst, sizeof(dst));
	close(s);
	return 0;
}
```