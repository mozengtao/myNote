# Linux Kernel v3.2 Network Stack Flashcards

> Comprehensive Anki-style flashcards covering socket layer, TCP/IP protocols, packet processing, netfilter, and network device drivers.

---

## Section 1: Core Fundamentals

---

Q: What are the four main layers of the Linux network stack from top to bottom?
A: 
1. **Socket Layer** - User-space interface (socket syscalls)
2. **Transport Layer** - TCP, UDP, SCTP (end-to-end communication)
3. **Network Layer** - IP routing, forwarding, fragmentation
4. **Link/Device Layer** - Ethernet, ARP, NIC drivers

This maps roughly to OSI layers 7-5, 4, 3, and 2-1 respectively.
[Basic]

---

Q: What is the fundamental problem that the kernel network stack solves?
A: Connecting user-space processes to the physical network wire. It handles:
- **Multiplexing**: Thousands of connections on one interface
- **Protocol processing**: TCP segmentation, IP routing, checksums
- **Abstraction**: Simple byte stream (TCP) or datagram (UDP) API
- **Hardware interface**: DMA, interrupts, offloads
[Basic]

---

Q: What is `struct sk_buff` and why is it the most important network data structure?
A: `struct sk_buff` (socket buffer) is the container that carries a single packet through all layers of the network stack. It contains:
- Pointers to packet data (head, data, tail, end)
- Protocol header offsets
- Metadata (device, socket, timestamp)
- Reference counting for safe sharing

Every packet, whether received or transmitted, is wrapped in an sk_buff.
[Basic]

---

Q: What is `struct sock` and how does it differ from `struct socket`?
A: 
- `struct socket` - VFS-facing structure, represents the file descriptor interface
- `struct sock` - Network-facing structure, contains protocol state (TCP state machine, buffers, timers)

`struct socket` has a pointer to `struct sock`. The socket is the user interface; the sock is the protocol implementation.
[Basic]

---

Q: What is `struct net_device` and what does it represent?
A: `struct net_device` represents a network interface (eth0, lo, etc.). Key contents:
- Device name and ifindex
- MAC address and MTU
- TX/RX queues
- Operations table (`net_device_ops`)
- NAPI contexts for polling
- Statistics counters
[Basic]

---

Q: (Cloze) The three core data structures in Linux networking are _____, _____, and _____.
A: `struct sk_buff` (packet container), `struct sock` (protocol state), and `struct net_device` (network interface).
[Basic]

---

Q: How does the Linux network stack process packets - queue-based or function-call-based?
A: Primarily **function-call-based** (pipeline model). Each stage calls the next directly:
```
ip_rcv() → ip_rcv_finish() → ip_local_deliver() → tcp_v4_rcv()
```
Queues exist only at the endpoints:
- RX: Socket receive queue
- TX: Qdisc queue, driver ring buffer

This minimizes latency for the common case.
[Intermediate]

---

Q: What is the "fast path" concept in Linux networking?
A: The fast path is the optimized code path for common cases (e.g., established TCP connection receiving in-order data). It:
- Minimizes branches and cache misses
- Avoids locks when possible
- Uses direct function calls instead of queuing

Slow path handles exceptions (out-of-order packets, connection setup, errors).
[Intermediate]

---

Q: (ASCII Diagram) Draw the high-level packet receive (RX) path.
A:
```
Wire
  │
  ▼
┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐
│   NIC   │────►│  NAPI   │────►│   IP    │────►│   TCP   │
│ Driver  │     │  Poll   │     │  Input  │     │  Input  │
└─────────┘     └─────────┘     └─────────┘     └─────────┘
     │               │               │               │
  DMA to         GRO merge      Route lookup    Socket queue
  sk_buff        packets        + netfilter     + wake recv()
```
[Basic]

---

Q: (ASCII Diagram) Draw the high-level packet transmit (TX) path.
A:
```
User send()
     │
     ▼
┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐
│ Socket  │────►│   TCP   │────►│   IP    │────►│  Qdisc  │
│  Layer  │     │ Output  │     │ Output  │     │ + Driver│
└─────────┘     └─────────┘     └─────────┘     └─────────┘
     │               │               │               │
  Copy data      Segment +       Add IP hdr      Schedule +
  to kernel      TCP header      + route         DMA to NIC
```
[Basic]

---

Q: Where is the core networking code located in the Linux kernel source tree?
A:
- `net/core/` - Core infrastructure (dev.c, skbuff.c, sock.c)
- `net/ipv4/` - IPv4 stack (tcp.c, udp.c, ip_input.c, ip_output.c)
- `net/ipv6/` - IPv6 stack
- `net/netfilter/` - Packet filtering framework
- `net/sched/` - Traffic control (qdiscs)
- `net/socket.c` - Socket syscall entry points
- `drivers/net/` - Network device drivers
[Basic]

---

Q: What file contains the `netif_receive_skb()` and `dev_queue_xmit()` functions?
A: `net/core/dev.c` - This is the central file for packet RX/TX at the device layer. It contains:
- `netif_receive_skb()` - Core RX entry point after NAPI
- `dev_queue_xmit()` - Core TX entry point
- Protocol handler registration
- Network device management
[Intermediate]

---

Q: What is the purpose of the `protocol` field in `struct sk_buff`?
A: The `protocol` field (`__be16`) identifies the L3 protocol type (e.g., `ETH_P_IP` for IPv4, `ETH_P_IPV6` for IPv6, `ETH_P_ARP` for ARP). It's used by `netif_receive_skb()` to dispatch packets to the correct protocol handler.
[Basic]

---

Q: What is protocol demultiplexing and where does it happen in the RX path?
A: Protocol demultiplexing routes packets to the correct handler based on protocol fields:
1. **L2→L3**: `netif_receive_skb()` uses `skb->protocol` to call `ip_rcv()`, `arp_rcv()`, etc.
2. **L3→L4**: `ip_local_deliver_finish()` uses IP protocol field to call `tcp_v4_rcv()`, `udp_rcv()`, etc.
3. **L4→Socket**: TCP/UDP uses port numbers to find the destination socket.
[Intermediate]

---

Q: What are the two main software interrupt (softirq) types used in networking?
A:
- `NET_RX_SOFTIRQ` - Handles received packets (NAPI polling, `net_rx_action()`)
- `NET_TX_SOFTIRQ` - Handles transmit completion and queue processing (`net_tx_action()`)

Softirqs run after hardware interrupts with interrupts enabled, allowing batch processing.
[Intermediate]

---

Q: (Reverse) This function is the core entry point for received packets after NAPI polling.
A: Q: What is `netif_receive_skb()`?
[Intermediate]

---

Q: (Reverse) This function is called to transmit a packet from the protocol stack to the device layer.
A: Q: What is `dev_queue_xmit()`?
[Intermediate]

---

Q: What is the relationship between `net/socket.c` and `net/ipv4/af_inet.c`?
A: 
- `net/socket.c` - Generic socket syscall handlers (socket, bind, connect, etc.)
- `net/ipv4/af_inet.c` - AF_INET (IPv4) specific socket implementation

`socket.c` dispatches to protocol-family-specific code via `net_proto_family` registration. For AF_INET sockets, this leads to `af_inet.c`.
[Intermediate]

---

Q: What does "zero-copy" mean in networking and where is it used?
A: Zero-copy avoids copying packet data between buffers. Used in:
- **sendfile()**: File → socket without user-space copy
- **MSG_ZEROCOPY**: Send from user buffer via page pinning
- **NAPI**: DMA directly to sk_buff data area
- **splice()**: Pipe-based zero-copy

Reduces CPU overhead for high-throughput workloads.
[Intermediate]

---

Q: (Understanding) Why does the network stack use per-CPU data structures?
A: To avoid lock contention on multi-core systems:
- Per-CPU NAPI poll lists
- Per-CPU backlog queues
- Per-CPU statistics counters

This allows parallel packet processing across cores without synchronization overhead.
[Intermediate]

---

Q: What is a network namespace and what does it isolate?
A: A network namespace (`struct net`) provides isolated network stack instances. Each namespace has its own:
- Network interfaces (net_device list)
- Routing tables
- Firewall rules (iptables)
- Sockets

Used by containers (Docker, LXC) for network isolation.
[Intermediate]

---

Q: (Code Interpretation) What does this code pattern represent?
```c
struct net_protocol tcp_protocol = {
    .handler     = tcp_v4_rcv,
    .err_handler = tcp_v4_err,
    .no_policy   = 1,
};
inet_add_protocol(&tcp_protocol, IPPROTO_TCP);
```
A: This registers TCP as a transport protocol handler with the IPv4 stack. When IP receives a packet with protocol number IPPROTO_TCP (6), it calls `tcp_v4_rcv()`. The `inet_protos[]` array stores these registrations.
[Intermediate]

---

Q: What is the difference between "local delivery" and "forwarding" in IP processing?
A: 
- **Local delivery**: Packet's destination IP matches the host → delivered to transport layer (TCP/UDP)
- **Forwarding**: Packet's destination IP is elsewhere → routed to another interface

The routing decision (`ip_route_input()`) determines which path: `ip_local_deliver()` vs `ip_forward()`.
[Basic]

---

Q: (Understanding) Why does Linux use a pipeline model rather than a producer-consumer queue model for packet processing?
A: 
1. **Latency**: Direct function calls are faster than queue operations
2. **Cache locality**: Packet data stays in cache across layers
3. **Simplicity**: No queue management overhead for common case
4. **Flexibility**: Hooks (netfilter) can intercept at any stage

Queues are used only where necessary (socket buffers, qdisc, ring buffers).
[Intermediate]

---

Q: What is the `cb[]` (control buffer) field in sk_buff used for?
A: The `cb[48]` array is scratch space for protocols to store per-packet private data without allocating separate structures. Each layer can cast it to its own type:
- TCP uses `struct tcp_skb_cb`
- IP uses it for fragment reassembly info
- Netfilter uses it for connection tracking

The 48-byte size accommodates the largest user (TCP).
[Intermediate]

---

## Section 2: sk_buff Data Structure

---

Q: (ASCII Diagram) Draw the memory layout of an sk_buff and its data buffer.
A:
```
struct sk_buff (~232 bytes)          Data Buffer
┌─────────────────────┐              ┌─────────────────────────────────────┐
│ next, prev          │              │ headroom │ headers │ payload │ tail │
│ sk (socket)         │         head─►──────────┼─────────┼─────────┼──────│
│ dev (device)        │         data────────────►─────────┼─────────│      │
│ head ───────────────┼─────────────►            │         │         │      │
│ data ───────────────┼──────────────────────────►         │         │      │
│ tail (offset)       │                                    tail◄─────┘      │
│ end (offset)        │                                              end◄───┘
│ len, data_len       │              └─────────────────────────────────────┘
│ protocol, pkt_type  │
│ transport_header    │
│ network_header      │
│ mac_header          │
│ cb[48]              │
└─────────────────────┘
```
[Basic]

---

Q: What are the four key pointers/offsets that define the data region in sk_buff?
A:
- `head` - Start of allocated buffer (fixed after allocation)
- `data` - Start of current packet data (moves as headers are added/removed)
- `tail` - End of current packet data (offset from head in v3.2)
- `end` - End of allocated buffer (fixed, offset from head)

`len = tail - data` represents the current data length.
[Basic]

---

Q: What is "headroom" in an sk_buff and why is it important?
A: Headroom is the space between `head` and `data`. It's reserved space where headers can be prepended during TX:
```c
skb_reserve(skb, headroom);  // Reserve space at allocation
skb_push(skb, hdr_len);      // Add header, data pointer moves back
```
Without headroom, adding headers would require reallocation or copying.
[Intermediate]

---

Q: What is "tailroom" in an sk_buff?
A: Tailroom is the space between `tail` and `end`. It's where data can be appended:
```c
void *ptr = skb_put(skb, len);  // Extend tail, return pointer to new space
```
Used when building packets or when the driver adds trailing data.
[Basic]

---

Q: What does `skb_push()` do and when is it used?
A: `skb_push(skb, len)` prepends data by moving `data` pointer backward:
```c
unsigned char *skb_push(struct sk_buff *skb, unsigned int len)
{
    skb->data -= len;
    skb->len  += len;
    return skb->data;
}
```
Used during TX to add headers (Ethernet, IP, TCP) in front of payload.
[Basic]

---

Q: What does `skb_pull()` do and when is it used?
A: `skb_pull(skb, len)` removes data from the front by moving `data` pointer forward:
```c
unsigned char *skb_pull(struct sk_buff *skb, unsigned int len)
{
    skb->data += len;
    skb->len  -= len;
    return skb->data;
}
```
Used during RX to strip headers as packet moves up the stack.
[Basic]

---

Q: What does `skb_put()` do?
A: `skb_put(skb, len)` appends data by extending the tail:
```c
unsigned char *skb_put(struct sk_buff *skb, unsigned int len)
{
    unsigned char *tmp = skb_tail_pointer(skb);
    skb->tail += len;
    skb->len  += len;
    return tmp;  // Return pointer to start of new space
}
```
Used to add payload data or trailing checksums.
[Basic]

---

Q: What does `skb_reserve()` do and when should it be called?
A: `skb_reserve(skb, len)` creates headroom by advancing both `data` and `tail`:
```c
void skb_reserve(struct sk_buff *skb, int len)
{
    skb->data += len;
    skb->tail += len;
}
```
Must be called on an empty sk_buff (immediately after allocation) before adding any data. Typical headroom: `NET_SKB_PAD + NET_IP_ALIGN`.
[Intermediate]

---

Q: (Cloze) To add a header during TX, use _____. To strip a header during RX, use _____.
A: `skb_push()`, `skb_pull()`
[Basic]

---

Q: What are `mac_header`, `network_header`, and `transport_header` in sk_buff?
A: These are offsets (from `head`) pointing to the start of each protocol header:
- `mac_header` - Ethernet header location
- `network_header` - IP header location  
- `transport_header` - TCP/UDP header location

Access via: `skb_mac_header(skb)`, `skb_network_header(skb)`, `skb_transport_header(skb)`
[Intermediate]

---

Q: (Code Interpretation) What does this code do?
```c
struct iphdr *iph = ip_hdr(skb);
struct tcphdr *th = tcp_hdr(skb);
```
A: It retrieves pointers to the IP and TCP headers from the sk_buff. These macros expand to:
```c
#define ip_hdr(skb)  ((struct iphdr *)skb_network_header(skb))
#define tcp_hdr(skb) ((struct tcphdr *)skb_transport_header(skb))
```
The header offset fields must be set correctly before using these macros.
[Intermediate]

---

Q: What is the difference between `skb_clone()` and `skb_copy()`?
A:
- `skb_clone(skb, gfp)` - Creates new sk_buff struct pointing to **same data buffer** (shared, reference counted). Fast but data is read-only.
- `skb_copy(skb, gfp)` - Creates new sk_buff with **copied data buffer**. Slower but independent copy.

Use clone for read-only access (e.g., multiple listeners); copy when modification needed.
[Intermediate]

---

Q: What does `skb_get()` do and when is it used?
A: `skb_get(skb)` increments the reference count (`skb->users`):
```c
static inline struct sk_buff *skb_get(struct sk_buff *skb)
{
    atomic_inc(&skb->users);
    return skb;
}
```
Used when multiple code paths need to hold a reference to the same sk_buff.
[Intermediate]

---

Q: What is the difference between `kfree_skb()` and `consume_skb()`?
A:
- `kfree_skb(skb)` - Decrements refcount, frees if zero. Used for error paths (packet dropped).
- `consume_skb(skb)` - Same but indicates normal consumption (packet delivered successfully).

Both decrement `users`; the difference is for tracing/debugging purposes.
[Intermediate]

---

Q: What is `skb->len` vs `skb->data_len`?
A:
- `skb->len` - Total data length (linear + fragments)
- `skb->data_len` - Length of data in fragments only

For linear buffers: `data_len = 0`, all data between `data` and `tail`.
For non-linear (fragmented): `data_len > 0`, fragments in `skb_shinfo(skb)->frags[]`.
[Intermediate]

---

Q: What is a non-linear (fragmented) sk_buff?
A: An sk_buff where data is split between:
1. **Linear part**: Direct buffer (head to tail)
2. **Fragments**: `skb_shinfo(skb)->frags[]` array of page references

Used for scatter-gather I/O, avoiding copies. Common with TSO/GSO.
Access fragment info via `skb_shinfo(skb)`.
[Intermediate]

---

Q: What is `skb_shared_info` and where is it located?
A: `skb_shared_info` contains metadata for non-linear sk_buffs:
- `frags[]` - Page fragment array
- `frag_list` - List of sk_buffs (for IP fragments)
- `gso_size`, `gso_type` - Segmentation offload info
- `nr_frags` - Number of fragments

Located at `skb->end`, accessed via `skb_shinfo(skb)`.
[Advanced]

---

Q: (Code Interpretation) What does this allocation pattern do?
```c
skb = alloc_skb(len, GFP_ATOMIC);
if (!skb)
    return -ENOMEM;
skb_reserve(skb, NET_SKB_PAD + NET_IP_ALIGN);
```
A: 
1. Allocates sk_buff with `len` bytes of data space
2. Reserves headroom for:
   - `NET_SKB_PAD` (64 bytes) - Room for headers during TX
   - `NET_IP_ALIGN` (2 bytes) - Aligns IP header to 4-byte boundary after 14-byte Ethernet header

This is the standard pattern for driver RX buffer allocation.
[Intermediate]

---

Q: What is `skb->ip_summed` and what values can it have?
A: `ip_summed` indicates checksum status:
- `CHECKSUM_NONE` - No checksum computed, software must verify
- `CHECKSUM_COMPLETE` - Hardware computed checksum over entire packet (RX)
- `CHECKSUM_PARTIAL` - Partial checksum done, hardware will complete (TX)
- `CHECKSUM_UNNECESSARY` - Checksum verified by hardware (RX)

Enables checksum offload optimization.
[Intermediate]

---

Q: What is `skb->pkt_type` used for?
A: Identifies packet destination type:
- `PACKET_HOST` - Destined for this host
- `PACKET_BROADCAST` - Broadcast packet
- `PACKET_MULTICAST` - Multicast packet
- `PACKET_OTHERHOST` - For another host (promiscuous mode)

Set by driver based on destination MAC address comparison.
[Intermediate]

---

Q: (Understanding) Why does sk_buff use offset integers for tail/end instead of pointers?
A: In kernel v3.2+, `tail` and `end` are offsets (unsigned int) rather than pointers to:
1. **Save memory** on 64-bit systems (4 bytes vs 8 bytes each)
2. **Simplify cloning** - Offsets remain valid when buffer is shared

Access via helper macros: `skb_tail_pointer(skb)`, `skb_end_pointer(skb)`.
[Advanced]

---

Q: What function allocates an sk_buff for receiving packets?
A: `netdev_alloc_skb(dev, length)` or `netdev_alloc_skb_ip_align(dev, length)`:
```c
struct sk_buff *netdev_alloc_skb_ip_align(struct net_device *dev,
                                          unsigned int length)
{
    struct sk_buff *skb = netdev_alloc_skb(dev, length + NET_IP_ALIGN);
    if (NET_IP_ALIGN && skb)
        skb_reserve(skb, NET_IP_ALIGN);
    return skb;
}
```
`_ip_align` variant ensures IP header alignment.
[Intermediate]

---

## Section 3: Socket Layer

---

Q: What is the socket() system call and what does it return?
A: `socket(domain, type, protocol)` creates a communication endpoint:
- `domain` - Protocol family (AF_INET, AF_INET6, AF_UNIX, AF_PACKET)
- `type` - Socket type (SOCK_STREAM, SOCK_DGRAM, SOCK_RAW)
- `protocol` - Specific protocol (usually 0 for default)

Returns a file descriptor or -1 on error. Kernel creates `struct socket` + `struct sock`.
[Basic]

---

Q: What is the difference between SOCK_STREAM and SOCK_DGRAM?
A:
- `SOCK_STREAM` - Connection-oriented, reliable byte stream (TCP). Guarantees delivery and ordering.
- `SOCK_DGRAM` - Connectionless, unreliable datagrams (UDP). No delivery guarantee, message boundaries preserved.

The type determines which protocol operations are used.
[Basic]

---

Q: What are the main protocol families (address families) supported by Linux?
A:
- `AF_INET` (2) - IPv4 Internet protocols
- `AF_INET6` (10) - IPv6 Internet protocols
- `AF_UNIX` / `AF_LOCAL` (1) - Local inter-process communication
- `AF_PACKET` (17) - Raw link-layer access
- `AF_NETLINK` (16) - Kernel/user communication

Each family has its own `struct net_proto_family` registration.
[Basic]

---

Q: (ASCII Diagram) Draw the relationship between struct socket and struct sock.
A:
```
User Space                    Kernel Space
    │
    │ fd                     ┌──────────────┐
    └────────────────────────► struct file  │
                             │   f_op       │
                             │   private ───┼──┐
                             └──────────────┘  │
                                               │
                             ┌──────────────┐  │
                             │struct socket │◄─┘
                             │   ops ───────┼──────► proto_ops (inet_stream_ops)
                             │   sk ────────┼──┐
                             │   state      │  │
                             └──────────────┘  │
                                               │
                             ┌──────────────┐  │
                             │ struct sock  │◄─┘
                             │ (tcp_sock)   │
                             │   sk_state   │   Protocol state machine
                             │   sk_rcvbuf  │   Buffer management
                             │   sk_receive_queue   Data queues
                             └──────────────┘
```
[Intermediate]

---

Q: What is `struct proto_ops` and what operations does it define?
A: `proto_ops` defines socket-level operations for a protocol family:
```c
struct proto_ops {
    int (*bind)(struct socket *, struct sockaddr *, int);
    int (*connect)(struct socket *, struct sockaddr *, int, int);
    int (*accept)(struct socket *, struct socket *, int);
    int (*listen)(struct socket *, int);
    int (*sendmsg)(struct kiocb *, struct socket *, struct msghdr *, size_t);
    int (*recvmsg)(struct kiocb *, struct socket *, struct msghdr *, size_t, int);
    // ... poll, ioctl, setsockopt, getsockopt ...
};
```
For TCP/IPv4: `inet_stream_ops`. For UDP: `inet_dgram_ops`.
[Intermediate]

---

Q: What is `struct proto` and how does it differ from `struct proto_ops`?
A:
- `struct proto_ops` - Socket layer operations (VFS interface)
- `struct proto` - Transport protocol operations (protocol implementation)

```c
struct proto tcp_prot = {
    .name       = "TCP",
    .connect    = tcp_v4_connect,
    .sendmsg    = tcp_sendmsg,
    .recvmsg    = tcp_recvmsg,
    .hash       = inet_hash,
    .unhash     = inet_unhash,
    // ...
};
```
`proto_ops` calls into `proto` methods.
[Intermediate]

---

Q: What does the bind() system call do?
A: `bind(sockfd, addr, addrlen)` associates a socket with a local address:
1. Validates the address format
2. Checks if address/port is available
3. Sets `inet_sk(sk)->inet_saddr` and `inet_sk(sk)->inet_sport`
4. For servers: necessary before listen()
5. For clients: optional (system assigns ephemeral port)

Kernel path: `sys_bind()` → `inet_bind()` → protocol's bind
[Basic]

---

Q: What does the listen() system call do?
A: `listen(sockfd, backlog)` marks a socket as passive (accepting connections):
1. Changes socket state to TCP_LISTEN
2. Allocates accept queue (completed connections)
3. Sets `backlog` - max pending connections

Kernel path: `sys_listen()` → `inet_listen()` → `inet_csk_listen_start()`
[Basic]

---

Q: What is the accept queue in TCP and how does it work?
A: TCP uses two queues for incoming connections:
1. **SYN queue** (request_sock_queue) - Half-open connections (SYN received, SYN-ACK sent)
2. **Accept queue** - Fully established connections waiting for accept()

When 3-way handshake completes, connection moves from SYN queue to accept queue. `accept()` dequeues from accept queue.
[Intermediate]

---

Q: What does accept() return and what happens in the kernel?
A: `accept(sockfd, addr, addrlen)` dequeues a connection and returns a new socket fd:
1. Blocks until connection available (or EAGAIN if non-blocking)
2. Dequeues `struct sock` from accept queue
3. Creates new `struct socket` and file descriptor
4. The new socket is in TCP_ESTABLISHED state

Original socket remains in LISTEN state for more connections.
[Basic]

---

Q: What is the difference between send()/recv() and sendto()/recvfrom()?
A:
- `send()/recv()` - For connected sockets (TCP), address implied
- `sendto()/recvfrom()` - Specify address per call, used for connectionless (UDP)

```c
send(fd, buf, len, flags);           // TCP
sendto(fd, buf, len, flags, addr, addrlen);  // UDP
```
For connected UDP sockets, send() also works.
[Basic]

---

Q: What are the main MSG_* flags used with send/recv?
A:
- `MSG_DONTWAIT` - Non-blocking operation
- `MSG_PEEK` - Peek at data without removing from queue
- `MSG_WAITALL` - Wait for full request (recv)
- `MSG_OOB` - Out-of-band data (TCP urgent)
- `MSG_NOSIGNAL` - Don't generate SIGPIPE on broken connection
- `MSG_MORE` - More data coming (cork-like behavior)
[Intermediate]

---

Q: What is `sk_receive_queue` in struct sock?
A: `sk_receive_queue` is a list of sk_buffs containing received data waiting to be read by the application:
```c
struct sk_buff_head sk_receive_queue;
```
TCP/UDP input handlers add packets here. `recvmsg()` dequeues and copies to user buffer. Protected by socket lock.
[Intermediate]

---

Q: What is `sk_write_queue` in struct sock?
A: `sk_write_queue` holds data from send() waiting to be transmitted:
```c
struct sk_buff_head sk_write_queue;
```
For TCP: data waiting for transmission window / congestion window. TCP output (`tcp_write_xmit()`) dequeues and sends. For UDP: typically bypassed (no queuing).
[Intermediate]

---

Q: What does setsockopt() do and give examples of common options?
A: `setsockopt(fd, level, optname, optval, optlen)` sets socket options:

**SOL_SOCKET level:**
- `SO_REUSEADDR` - Allow address reuse
- `SO_RCVBUF` / `SO_SNDBUF` - Buffer sizes
- `SO_KEEPALIVE` - Enable keepalive probes

**IPPROTO_TCP level:**
- `TCP_NODELAY` - Disable Nagle algorithm
- `TCP_CORK` - Cork output
- `TCP_KEEPIDLE` - Keepalive idle time
[Intermediate]

---

Q: (Code Interpretation) What does this code do?
```c
int optval = 1;
setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
```
A: Enables the SO_REUSEADDR option which allows binding to an address that's in TIME_WAIT state. Commonly used by servers to restart quickly without "address already in use" errors.
[Basic]

---

Q: What is the purpose of SO_RCVBUF and SO_SNDBUF?
A: These set the socket buffer sizes:
- `SO_RCVBUF` - Receive buffer size (max data queued before application reads)
- `SO_SNDBUF` - Send buffer size (max data queued for transmission)

Affects flow control (TCP advertised window) and memory usage. Kernel may double the requested value for overhead.
[Intermediate]

---

Q: How does blocking I/O work on sockets?
A: When calling recv() on a blocking socket with no data:
1. Process added to socket's wait queue (`sk->sk_wq`)
2. Process state set to TASK_INTERRUPTIBLE
3. Process sleeps via schedule()
4. When data arrives, TCP input wakes processes: `sk->sk_data_ready()`
5. Process wakes, continues recv()

Non-blocking returns EAGAIN immediately instead.
[Intermediate]

---

Q: What is select/poll/epoll and how do they relate to sockets?
A: These are I/O multiplexing mechanisms to wait on multiple file descriptors:
- `select()` - Original, limited to 1024 fds, O(n) scanning
- `poll()` - No fd limit, still O(n) scanning
- `epoll()` - Scalable O(1), uses callbacks

Kernel calls socket's `poll` method to check readiness:
```c
sock->ops->poll(file, sock, wait);  // Returns POLLIN, POLLOUT, etc.
```
[Intermediate]

---

Q: (Cloze) The socket syscall entry point in kernel is _____ which dispatches to protocol-specific _____.
A: `sys_socket()` (or `SYSCALL_DEFINE3(socket, ...)`), `net_proto_family->create()`
[Intermediate]

---

Q: What is `inet_sk()` and when is it used?
A: `inet_sk(sk)` casts a `struct sock *` to `struct inet_sock *`:
```c
static inline struct inet_sock *inet_sk(const struct sock *sk)
{
    return (struct inet_sock *)sk;
}
```
`inet_sock` extends `sock` with IP-specific fields: source/dest addresses and ports, IP options, TTL, etc.
[Intermediate]

---

Q: What fields does struct inet_sock add to struct sock?
A: `struct inet_sock` adds IP-specific fields:
```c
struct inet_sock {
    struct sock     sk;         // Base socket
    __be32          inet_saddr; // Source IP
    __be32          inet_daddr; // Dest IP (connected)
    __be16          inet_sport; // Source port
    __be16          inet_dport; // Dest port
    __u8            tos;        // Type of Service
    __u8            ttl;        // Time to Live
    // ... more IP options
};
```
[Intermediate]

---

Q: What is the socket state machine and what states exist?
A: Socket states track connection lifecycle:
```c
enum {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_CLOSE,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_LISTEN,
    TCP_CLOSING,
};
```
Stored in `sk->sk_state`. Transitions driven by protocol events and syscalls.
[Intermediate]

---

Q: (Understanding) Why do sockets use reference counting?
A: Reference counting (`sk->sk_refcnt`) prevents premature deallocation when:
1. Multiple file descriptors reference same socket (dup/fork)
2. Protocol timers hold references (TIME_WAIT, retransmit)
3. Packet processing in progress

`sock_hold(sk)` increments, `sock_put(sk)` decrements and frees at zero.
[Intermediate]

---

Q: What is the purpose of shutdown() vs close() on sockets?
A:
- `shutdown(fd, how)` - Partial close, keeps fd open
  - `SHUT_RD` - Stop receiving
  - `SHUT_WR` - Send FIN, stop sending
  - `SHUT_RDWR` - Both
- `close(fd)` - Full close, releases fd, decrements refcount

`shutdown(SHUT_WR)` allows reading remaining data after sending FIN.
[Intermediate]

---

## Section 4: Transport Layer - TCP

---

Q: (ASCII Diagram) Draw the TCP state machine showing all 11 states.
A:
```
                              ┌───────────┐
                    passive   │           │  active open
                    open      │  CLOSED   │  send SYN
           ┌─────────────────►│           │─────────────────┐
           │                  └───────────┘                 │
           │                        │                       │
           │                        │ recv SYN              ▼
           │                        │ send SYN,ACK    ┌───────────┐
           │                        ▼                 │ SYN_SENT  │
           │                  ┌───────────┐           └─────┬─────┘
           │                  │ SYN_RCVD  │                 │
           │                  └─────┬─────┘    recv SYN,ACK │
           │                        │          send ACK     │
           │           recv ACK     │                       │
           │                        ▼                       ▼
           │                  ┌─────────────────────────────────┐
           │                  │         ESTABLISHED             │
           │                  └─────────────┬───────────────────┘
           │                                │
           │             ┌──────────────────┼──────────────────┐
           │             │ close            │           recv FIN│
           │             │ send FIN         │           send ACK│
           │             ▼                  │                   ▼
           │       ┌───────────┐            │            ┌───────────┐
           │       │ FIN_WAIT1 │            │            │CLOSE_WAIT │
           │       └─────┬─────┘            │            └─────┬─────┘
           │             │                  │                  │
           │  recv ACK   │  recv FIN        │       close      │
           │             │  send ACK        │       send FIN   │
           │             ▼                  │                  ▼
           │       ┌───────────┐            │            ┌───────────┐
           │       │ FIN_WAIT2 │            │            │ LAST_ACK  │
           │       └─────┬─────┘            │            └─────┬─────┘
           │             │                  │                  │
           │    recv FIN │                  │         recv ACK │
           │    send ACK │                  │                  │
           │             ▼                  │                  │
           │       ┌───────────┐            │                  │
           │       │ TIME_WAIT │◄───────────┘                  │
           │       └─────┬─────┘                               │
           │             │ 2MSL timeout                        │
           └─────────────┴─────────────────────────────────────┘
```
[Intermediate]

---

Q: List all 11 TCP states and briefly describe each.
A:
1. **CLOSED** - No connection
2. **LISTEN** - Waiting for connection requests (server)
3. **SYN_SENT** - SYN sent, waiting for SYN-ACK (client connecting)
4. **SYN_RECV** - SYN received, SYN-ACK sent (server accepting)
5. **ESTABLISHED** - Connection open, data transfer
6. **FIN_WAIT1** - FIN sent, waiting for ACK
7. **FIN_WAIT2** - FIN acknowledged, waiting for peer's FIN
8. **CLOSE_WAIT** - Received FIN, waiting for application close
9. **LAST_ACK** - FIN sent after receiving FIN, waiting for ACK
10. **TIME_WAIT** - Waiting 2MSL before fully closing
11. **CLOSING** - Both sides sent FIN simultaneously
[Basic]

---

Q: Describe the TCP three-way handshake.
A:
```
Client                          Server
   │                               │
   │────── SYN (seq=x) ──────────►│  SYN_SENT
   │                               │  SYN_RECV
   │◄─── SYN-ACK (seq=y,ack=x+1)──│
   │                               │
   │────── ACK (ack=y+1) ─────────►│
   │                               │
ESTABLISHED                    ESTABLISHED
```
1. Client sends SYN with initial sequence number
2. Server responds with SYN-ACK
3. Client sends ACK, connection established
[Basic]

---

Q: What is the purpose of TIME_WAIT state and why is it 2MSL?
A: TIME_WAIT ensures:
1. **Reliable termination** - Last ACK may be lost; allows retransmission
2. **Prevent old duplicates** - Old packets from previous connection won't be accepted

2MSL (Maximum Segment Lifetime, typically 60s) ensures all packets from old connection have expired. Host that sends final ACK enters TIME_WAIT.
[Intermediate]

---

Q: What are SYN cookies and when are they used?
A: SYN cookies are a defense against SYN flood attacks. When SYN queue is full:
1. Server doesn't allocate state for SYN
2. Encodes connection info in ISN (Initial Sequence Number)
3. On valid ACK, reconstructs state from ISN

Enabled via `tcp_syncookies` sysctl. Trade-off: loses some TCP options.
[Intermediate]

---

Q: What is the TCP receive path starting from ip_local_deliver()?
A:
```
ip_local_deliver_finish()
    │
    ▼
tcp_v4_rcv()              ◄── Entry point, lookup socket
    │
    ▼
tcp_v4_do_rcv()           ◄── Main state dispatch
    │
    ├── tcp_rcv_established()   ◄── Fast path (established)
    │
    └── tcp_rcv_state_process() ◄── Slow path (other states)
```
[Intermediate]

---

Q: What makes the TCP fast path "fast" in tcp_rcv_established()?
A: The fast path handles the common case (in-order data on established connection):
1. **Header prediction** - Expected header pattern matched
2. **No reordering** - Sequence number matches expected
3. **No TCP options to process** - Simple ACK
4. **Direct queue** - Data added to receive queue immediately

Bypasses slow path's full state machine processing.
[Intermediate]

---

Q: What is tcp_rcv_state_process() and when is it called?
A: `tcp_rcv_state_process()` is the slow path handler for TCP states other than ESTABLISHED:
- Connection setup (SYN_SENT, SYN_RECV)
- Connection teardown (FIN_WAIT1, FIN_WAIT2, etc.)
- Out-of-order or special packets in ESTABLISHED

Contains the full TCP state machine implementation.
[Intermediate]

---

Q: What is the TCP send path starting from tcp_sendmsg()?
A:
```
tcp_sendmsg()             ◄── Socket layer entry
    │
    ▼
do_tcp_sendpages() or     ◄── Copy data to sk_write_queue
sk_stream_alloc_skb()
    │
    ▼
tcp_push()
    │
    ▼
__tcp_push_pending_frames()
    │
    ▼
tcp_write_xmit()          ◄── Main TX function
    │
    ▼
tcp_transmit_skb()        ◄── Build TCP header, send
    │
    ▼
ip_queue_xmit()           ◄── Hand to IP layer
```
[Intermediate]

---

Q: What does tcp_write_xmit() do?
A: `tcp_write_xmit()` is the main TCP transmission workhorse:
1. Check congestion window (cwnd) and receive window
2. Segment data into MSS-sized packets
3. For each sendable segment:
   - Clone or reference sk_buff from write_queue
   - Call `tcp_transmit_skb()` to add headers and send
4. Update send state (snd_nxt, packets_out)

Called from both data sending and retransmission.
[Intermediate]

---

Q: What is MSS (Maximum Segment Size) and how is it determined?
A: MSS is the maximum TCP payload size (excluding headers):
- Default: 536 bytes (minimum, assumes 576 MTU)
- Typical Ethernet: 1460 bytes (1500 MTU - 20 IP - 20 TCP)
- Negotiated during handshake via TCP option

Kernel stores in `tcp_sk(sk)->mss_cache`. Affects segmentation and congestion control.
[Basic]

---

Q: What is the sliding window in TCP?
A: The sliding window controls how much unacknowledged data can be in flight:
```
                Send Window
    ◄────────────────────────────────────►
┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
│ACK│ACK│   │   │   │   │   │   │CAN│CAN│
│'d │'d │SENT   │SENT   │SENDABLE │'T  │'T │
└───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
        ▲                   ▲
     snd_una             snd_una + snd_wnd
```
Window size = min(receiver's advertised window, congestion window)
[Basic]

---

Q: What is the difference between the receive window and congestion window?
A:
- **Receive Window (rwnd)** - Advertised by receiver, indicates available buffer space (flow control)
- **Congestion Window (cwnd)** - Maintained by sender, limits based on network congestion (congestion control)

Effective send window = min(rwnd, cwnd)
[Basic]

---

Q: What is slow start in TCP congestion control?
A: Slow start exponentially grows cwnd to probe network capacity:
1. Start with cwnd = Initial Window (typically 10 MSS in v3.2)
2. For each ACK, cwnd += 1 MSS
3. Cwnd doubles each RTT
4. Continue until cwnd >= ssthresh (slow start threshold)
5. Then switch to congestion avoidance

Prevents sudden burst overwhelming the network.
[Intermediate]

---

Q: What is congestion avoidance in TCP?
A: After slow start (when cwnd >= ssthresh), growth becomes linear:
- For each RTT, cwnd += 1 MSS (approximately)
- More precisely: cwnd += MSS * MSS / cwnd for each ACK

This additive increase allows gentle probing for additional bandwidth.
[Intermediate]

---

Q: What happens when TCP detects packet loss?
A: Response depends on how loss is detected:

**Triple duplicate ACKs (fast retransmit):**
- ssthresh = cwnd / 2
- cwnd = ssthresh + 3 (fast recovery)
- Retransmit lost segment

**Timeout:**
- ssthresh = cwnd / 2
- cwnd = 1 MSS (restart slow start)
- Retransmit lost segment

Timeout is more severe than fast retransmit.
[Intermediate]

---

Q: What is CUBIC congestion control and why is it the Linux default?
A: CUBIC is the default TCP congestion control since Linux 2.6.19:
- Uses a cubic function of time since last congestion event
- **Faster recovery** - Quickly reaches previous cwnd
- **Better for high BDP** - Bandwidth-Delay Product networks
- **RTT-fair** - Less dependent on RTT than Reno

Cwnd growth follows: W(t) = C(t - K)³ + Wmax
[Intermediate]

---

Q: (Code Interpretation) What does this struct represent?
```c
struct tcp_congestion_ops cubic = {
    .name           = "cubic",
    .init           = cubictcp_init,
    .ssthresh       = cubictcp_recalc_ssthresh,
    .cong_avoid     = cubictcp_cong_avoid,
    .set_state      = cubictcp_state,
    .pkts_acked     = cubictcp_acked,
};
```
A: This is the congestion control algorithm registration structure. Each algorithm implements callbacks:
- `ssthresh` - Calculate new threshold after loss
- `cong_avoid` - Called during congestion avoidance phase
- `pkts_acked` - Called when packets are acknowledged

Algorithms registered via `tcp_register_congestion_control()`.
[Intermediate]

---

Q: What TCP timers exist and what do they do?
A: Key TCP timers:
1. **Retransmit timer** - Triggers retransmission on timeout (RTO)
2. **Delayed ACK timer** - Delays ACK up to 40ms to batch
3. **Keepalive timer** - Detects dead connections (hours)
4. **TIME_WAIT timer** - 2MSL countdown
5. **Zero window probe** - Probes when peer's window is 0

Implemented in `tcp_timer.c` using kernel timers.
[Intermediate]

---

Q: What is RTO (Retransmission Timeout) and how is it calculated?
A: RTO is the timeout before retransmitting unacknowledged data:
```
RTO = SRTT + 4 * RTTVAR
```
Where:
- SRTT = Smoothed RTT (exponential moving average)
- RTTVAR = RTT variance

Minimum RTO: typically 200ms. After timeout, RTO doubles (exponential backoff).
[Intermediate]

---

Q: What is the delayed ACK mechanism?
A: Delayed ACK batches acknowledgments to reduce packet overhead:
1. When data received, start 40ms timer
2. If more data arrives, send ACK immediately
3. If timer expires, send ACK
4. Send ACK every 2 segments max (regardless of timer)

Trade-off: Reduces packets but adds latency. Disable with TCP_QUICKACK.
[Intermediate]

---

Q: What is Nagle's algorithm and how does TCP_NODELAY relate?
A: Nagle's algorithm prevents sending small packets when data is pending ACK:
```c
if (unacked_data && new_data < MSS)
    wait_for_ack();
else
    send_immediately();
```
Reduces small packet overhead but adds latency.

`TCP_NODELAY` disables Nagle, sending immediately. Used for interactive applications (SSH, gaming).
[Intermediate]

---

Q: What is TCP_CORK and how does it differ from Nagle?
A:
- **Nagle** - Holds small data only if ACK pending
- **TCP_CORK** - Holds ALL data until cork is removed (or 200ms timeout)

```c
setsockopt(fd, IPPROTO_TCP, TCP_CORK, &on, sizeof(on));
write(fd, header, header_len);
write(fd, data, data_len);
setsockopt(fd, IPPROTO_TCP, TCP_CORK, &off, sizeof(off));
// Now sends combined packet
```
Useful for building large messages from small writes.
[Intermediate]

---

Q: What is struct tcp_sock and what does it contain?
A: `struct tcp_sock` extends `inet_sock` with TCP-specific state:
```c
struct tcp_sock {
    struct inet_connection_sock inet_conn;
    
    /* Sequence numbers */
    u32 snd_una;    // First unacknowledged sequence
    u32 snd_nxt;    // Next sequence to send
    u32 rcv_nxt;    // Next expected receive sequence
    
    /* Windows */
    u32 snd_wnd;    // Receive window from peer
    u32 snd_cwnd;   // Congestion window
    
    /* RTT estimation */
    u32 srtt;       // Smoothed RTT
    u32 mdev;       // RTT variance
    // ... many more fields
};
```
[Intermediate]

---

Q: What is struct tcp_skb_cb and where is it stored?
A: `tcp_skb_cb` stores per-packet TCP metadata in sk_buff's control buffer:
```c
struct tcp_skb_cb {
    __u32       seq;        // Start sequence number
    __u32       end_seq;    // End sequence number
    __u32       ack_seq;    // ACK sequence (RX)
    __u8        tcp_flags;  // TCP flags
    __u8        sacked;     // SACK state
    // ...
};
#define TCP_SKB_CB(__skb) ((struct tcp_skb_cb *)&((__skb)->cb[0]))
```
Avoids separate allocation for TCP metadata per packet.
[Intermediate]

---

Q: What is SACK (Selective Acknowledgment)?
A: SACK allows receiver to report non-contiguous received blocks:
```
Received: [1-1000], [2001-3000], [4001-5000]
SACK reports: 2001-3000, 4001-5000
```
Sender can retransmit only truly lost segments (1001-2000, 3001-4000) instead of everything after first gap.

Negotiated in SYN via SACK-permitted option. Significantly improves performance with multiple losses.
[Intermediate]

---

Q: What is TCP Fast Open (TFO)?
A: TFO allows data in SYN packet, saving one RTT:
```
Traditional:  SYN → SYN-ACK → ACK + Data → Response
TFO:          SYN + Data → SYN-ACK + Response
```
Uses a cookie (stored by client) to prevent amplification attacks. Client requests cookie in first connection, uses it in subsequent connections.
[Advanced]

---

Q: (Reverse) This TCP data structure stores per-connection sequence numbers, windows, and RTT estimates.
A: Q: What is `struct tcp_sock`?
[Intermediate]

---

Q: What is the out-of-order queue in TCP?
A: `tp->out_of_order_queue` holds segments that arrived out of order:
```
Expected: seq 1000
Received: seq 2000-3000, seq 4000-5000 (stored in ofo_queue)
When 1000-2000 arrives: merge ofo_queue into receive queue
```
Implementation uses RB-tree for efficient insertion and overlap detection.
[Intermediate]

---

Q: What happens when TCP receive buffer is full?
A: When `sk_rmem_alloc >= sk_rcvbuf`:
1. Receiver advertises zero window
2. Sender stops sending (window closed)
3. Sender starts zero-window probe timer
4. When application reads data, window opens
5. Receiver sends window update

This is flow control in action.
[Intermediate]

---

Q: What is the TCP prequeue and why does it exist?
A: The prequeue (`tp->ucopy.prequeue`) is a fast path for receiving data:
1. If user is blocked in recv(), packets go to prequeue
2. User processes directly without softirq context switch
3. Reduces latency for interactive applications

If prequeue full or user not waiting, normal backlog path used.
[Advanced]

---

Q: What are TCP timestamps and what are they used for?
A: TCP timestamps (RFC 1323) provide two values per segment:
- **TSval** - Sender's timestamp
- **TSecr** - Echo of peer's timestamp

Used for:
1. **RTT measurement** - More accurate than timing data segments
2. **PAWS** - Protection Against Wrapped Sequences (32-bit seq wrap)

Negotiated during handshake, adds 12 bytes to header.
[Intermediate]

---

Q: What is window scaling in TCP?
A: Window scaling (RFC 1323) allows receive windows > 64KB:
- Original window field: 16 bits (max 65535)
- With scaling: window = field << scale_factor
- Scale factor: 0-14 (negotiated in SYN)

Max window = 65535 << 14 = ~1GB. Essential for high-bandwidth long-delay networks.
[Intermediate]

---

Q: (Cloze) TCP sends a FIN when the application calls _____ or _____ with SHUT_WR.
A: `close()`, `shutdown()`
[Basic]

---

Q: What is the TCP keepalive mechanism?
A: Keepalive detects dead connections when no data is exchanged:
1. After idle time (`TCP_KEEPIDLE`, default 2 hours), send probe
2. If no response, retry (`TCP_KEEPCNT` times, default 9)
3. Wait `TCP_KEEPINTVL` between probes (default 75s)
4. If all probes fail, connection reset

Enable with SO_KEEPALIVE socket option.
[Intermediate]

---

Q: What is TCP_USER_TIMEOUT socket option?
A: `TCP_USER_TIMEOUT` specifies max time for transmitted data to be unacknowledged before closing:
```c
int timeout_ms = 10000;  // 10 seconds
setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
```
Unlike keepalive, works when actively sending data. Provides application-level control over connection timeout.
[Intermediate]

---

Q: (Understanding) Why does TCP use cumulative acknowledgments instead of per-packet ACKs?
A: Cumulative ACKs (acknowledge all bytes up to a point):
1. **Reduces packets** - One ACK covers multiple segments
2. **Handles ACK loss** - Later ACK implies earlier data received
3. **Simpler sender state** - Track single `snd_una`

Downside: Can't distinguish which specific packets lost. SACK addresses this limitation.
[Intermediate]

---

Q: What is the initial congestion window (IW) in Linux v3.2?
A: Linux v3.2 uses IW = 10 MSS (RFC 6928):
- Allows more data in first RTT
- Significantly improves short flow performance (web pages)
- Previous values: 1, 2, or 4 MSS

Set in `tcp_init_cwnd()`. Can be tuned via `ip route ... initcwnd N`.
[Intermediate]

---

Q: (Code Interpretation) What does this code do?
```c
if (tp->snd_cwnd < tp->snd_ssthresh)
    tcp_slow_start(tp);
else
    tcp_cong_avoid_ai(tp, tp->snd_cwnd);
```
A: This implements the congestion control mode selection:
- If `cwnd < ssthresh`: In slow start phase, exponential growth
- If `cwnd >= ssthresh`: In congestion avoidance phase, linear growth (additive increase)

This is the core congestion control state machine.
[Intermediate]

---

## Section 5: Transport Layer - UDP

---

Q: What are the key characteristics of UDP compared to TCP?
A:
- **Connectionless** - No handshake, no state machine
- **Unreliable** - No delivery guarantee, no retransmission
- **No ordering** - Packets may arrive out of order
- **Message-oriented** - Preserves message boundaries (datagrams)
- **Low overhead** - 8-byte header vs 20+ for TCP

Used for: DNS, DHCP, VoIP, gaming, streaming.
[Basic]

---

Q: (ASCII Diagram) Show the UDP header structure.
A:
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Source Port          |        Destination Port       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            Length             |           Checksum            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                             Data                              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Header size: 8 bytes (vs 20+ for TCP)
```
[Basic]

---

Q: What is the UDP receive path in the kernel?
A:
```
ip_local_deliver_finish()
    │
    ▼
udp_rcv()                 ◄── Protocol handler
    │
    ▼
__udp4_lib_rcv()          ◄── Main receive function
    │
    ├── __udp4_lib_lookup()   ◄── Find destination socket
    │
    ▼
udp_queue_rcv_skb()       ◄── Queue to socket
    │
    ▼
__skb_queue_tail(&sk->sk_receive_queue, skb)
    │
    ▼
sk->sk_data_ready()       ◄── Wake up reader
```
[Intermediate]

---

Q: What is the UDP send path in the kernel?
A:
```
udp_sendmsg()             ◄── Socket layer entry
    │
    ├── Get destination from msg or connected socket
    │
    ▼
ip_route_output_flow()    ◄── Route lookup
    │
    ▼
ip_append_data()          ◄── Fragment if needed
    │
    ▼
udp_push_pending_frames()
    │
    ▼
ip_push_pending_frames()
    │
    ▼
ip_output()               ◄── IP layer takes over
```
[Intermediate]

---

Q: How does UDP socket lookup work?
A: `__udp4_lib_lookup()` finds matching socket:
1. **Exact match**: (src_ip, src_port, dst_ip, dst_port)
2. **Wildcard dst_ip**: (src_ip, src_port, 0.0.0.0, dst_port)
3. **Wildcard src_ip**: (0.0.0.0, 0, 0.0.0.0, dst_port)

Uses hash table indexed by local port (`udp_table.hash`). Connected sockets have priority.
[Intermediate]

---

Q: What happens to a UDP packet if no matching socket is found?
A: If no socket matches:
1. Packet is dropped
2. ICMP "Port Unreachable" is sent back to sender
3. `SNMP MIB_NOPORTS` counter incremented

```c
if (sk == NULL) {
    icmp_send(skb, ICMP_DEST_UNREACH, ICMP_PORT_UNREACH, 0);
    kfree_skb(skb);
}
```
[Intermediate]

---

Q: What is the difference between sendto() and send() for UDP sockets?
A:
- `sendto(fd, buf, len, flags, addr, addrlen)` - Specifies destination per call
- `send(fd, buf, len, flags)` - Uses connected socket's destination

After `connect()` on UDP socket:
- Destination is remembered
- Can use `send()` instead of `sendto()`
- Receives only from connected peer
- Gets ICMP errors delivered to socket
[Basic]

---

Q: What does connect() do on a UDP socket?
A: Unlike TCP, UDP `connect()` doesn't exchange packets:
1. Stores destination address in socket (`inet_sk->inet_daddr`)
2. Performs route lookup and caches result
3. Filters incoming packets to only connected peer
4. Enables receiving ICMP errors via `sk->sk_err`

Can be "disconnected" by connecting to AF_UNSPEC.
[Intermediate]

---

Q: What is UDP-Lite and how does it differ from UDP?
A: UDP-Lite (RFC 3828) provides partial checksum coverage:
- Standard UDP: Checksum covers entire packet
- UDP-Lite: Checksum covers only specified portion

Useful for multimedia where partial data is better than none. Corrupted unchecksummed portion is delivered instead of dropped.

Socket: `socket(AF_INET, SOCK_DGRAM, IPPROTO_UDPLITE)`
[Advanced]

---

Q: How does UDP handle multicast?
A: For multicast reception:
1. Socket joins multicast group via `IP_ADD_MEMBERSHIP`
2. Kernel adds entry to interface's multicast filter
3. `__udp4_lib_lookup()` can return multiple sockets
4. Packet is cloned and delivered to all matching sockets

```c
struct ip_mreq mreq;
mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.1");
mreq.imr_interface.s_addr = INADDR_ANY;
setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
```
[Intermediate]

---

Q: (Cloze) UDP preserves _____ boundaries while TCP provides a _____ abstraction.
A: message, byte stream
[Basic]

---

Q: (Understanding) Why might an application choose UDP over TCP despite unreliability?
A: Reasons to choose UDP:
1. **Lower latency** - No handshake, no retransmit delays
2. **Better for real-time** - Old data is useless (VoIP, gaming)
3. **Multicast/broadcast** - TCP doesn't support these
4. **Custom reliability** - Application knows best (QUIC builds on UDP)
5. **Stateless servers** - DNS, DHCP need no connection state

Application can implement its own reliability if needed.
[Intermediate]

---

## Section 6: Network Layer - IPv4

---

Q: (ASCII Diagram) Show the IPv4 header structure.
A:
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Version|  IHL  |Type of Service|          Total Length         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Identification        |Flags|      Fragment Offset    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Time to Live |    Protocol   |         Header Checksum       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Source Address                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Destination Address                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Options (if IHL > 5)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Standard header: 20 bytes (IHL=5), with options up to 60 bytes
```
[Basic]

---

Q: What is the IPv4 receive path in the kernel?
A:
```
netif_receive_skb()
    │
    ▼
ip_rcv()                  ◄── Entry from L2, validate header
    │
    ▼
NF_INET_PRE_ROUTING       ◄── Netfilter hook
    │
    ▼
ip_rcv_finish()           ◄── Route lookup
    │
    ├── ip_route_input()  ◄── Determine: local vs forward
    │
    ▼ (if local)
ip_local_deliver()
    │
    ▼
NF_INET_LOCAL_IN          ◄── Netfilter hook
    │
    ▼
ip_local_deliver_finish() ◄── Protocol demux
    │
    ▼
tcp_v4_rcv() / udp_rcv()  ◄── Transport handler
```
[Intermediate]

---

Q: What does ip_rcv() validate?
A: `ip_rcv()` performs basic sanity checks:
1. Packet length >= IP header size
2. Version == 4
3. Header length (IHL) >= 5
4. Total length <= packet length
5. Header checksum valid

Invalid packets are dropped with appropriate counters incremented.
[Intermediate]

---

Q: What is the IPv4 transmit path in the kernel?
A:
```
tcp_transmit_skb() / udp_sendmsg()
    │
    ▼
ip_queue_xmit() or ip_push_pending_frames()
    │
    ▼
ip_local_out()
    │
    ▼
NF_INET_LOCAL_OUT         ◄── Netfilter hook
    │
    ▼
dst_output()
    │
    ▼
ip_output()
    │
    ▼
NF_INET_POST_ROUTING      ◄── Netfilter hook
    │
    ▼
ip_finish_output()        ◄── Fragment if needed
    │
    ▼
ip_finish_output2()       ◄── Neighbor lookup
    │
    ▼
dev_queue_xmit()          ◄── Hand to device layer
```
[Intermediate]

---

Q: What is the Forwarding Information Base (FIB)?
A: FIB is the routing table data structure:
- `fib_table` contains routing entries
- Organized as trie (prefix tree) for fast lookup
- Key = destination IP prefix
- Value = next hop, output interface, metrics

`fib_lookup()` searches FIB for matching route. Multiple tables possible (policy routing).
[Intermediate]

---

Q: What is `struct rtable` and what does it represent?
A: `struct rtable` (route table entry) is a cached routing decision:
```c
struct rtable {
    struct dst_entry dst;     // Common destination cache entry
    __be32           rt_gateway;    // Next hop
    int              rt_iif;        // Input interface
    int              rt_oif;        // Output interface  
    u8               rt_type;       // RTN_LOCAL, RTN_UNICAST, etc.
    // ...
};
```
Attached to sk_buff for fast path processing.
[Intermediate]

---

Q: What is the difference between ip_route_input() and ip_route_output()?
A:
- `ip_route_input()` - For received packets, determines local delivery vs forwarding
- `ip_route_output()` - For locally generated packets, finds egress route

Both populate `skb_dst(skb)` with routing decision. Output version takes source/dest from socket.
[Intermediate]

---

Q: How does IP forwarding work?
A: When routing decides packet is not for local delivery:
```
ip_rcv_finish()
    │
    ├── ip_route_input() returns RTN_UNICAST, not local
    │
    ▼
ip_forward()              ◄── Main forwarding function
    │
    ├── Decrement TTL, recompute checksum
    │
    ▼
NF_INET_FORWARD           ◄── Netfilter hook
    │
    ▼
ip_forward_finish()
    │
    ▼
dst_output() → ip_output() → dev_queue_xmit()
```
Requires `ip_forward` sysctl enabled.
[Intermediate]

---

Q: What is IP fragmentation and when does it occur?
A: Fragmentation splits packets larger than MTU:
- TX path: `ip_finish_output()` calls `ip_fragment()` if packet > MTU
- Each fragment has same ID, different offset
- Fragments can be further fragmented in transit
- Only destination reassembles

Avoid fragmentation: Use Path MTU Discovery or set DF bit.
[Basic]

---

Q: What is IP reassembly and how is it implemented?
A: Reassembly reconstructs original packet from fragments:
1. `ip_defrag()` called for fragments (MF bit set or offset > 0)
2. Fragments stored in hash table keyed by (src, dst, id, protocol)
3. When all fragments received, merged into single sk_buff
4. Timeout (30s default) cleans up incomplete reassembly

Implemented in `net/ipv4/ip_fragment.c`.
[Intermediate]

---

Q: What is the Don't Fragment (DF) bit used for?
A: DF bit prevents fragmentation:
- If set and packet > MTU, router sends ICMP "Fragmentation Needed"
- Enables Path MTU Discovery (PMTUD)
- Sender learns true path MTU from ICMP messages

TCP sets DF by default. Without DF, large packets silently fragmented.
[Intermediate]

---

Q: What is Path MTU Discovery (PMTUD)?
A: PMTUD discovers the smallest MTU along a path:
1. Send packets with DF bit set
2. If packet too large, receive ICMP "Fragmentation Needed" with MTU
3. Reduce packet size and retry
4. Cache discovered MTU

Linux stores path MTU in routing cache. Avoids fragmentation overhead.
[Intermediate]

---

Q: What is TTL (Time To Live) and what happens when it reaches 0?
A: TTL limits packet lifetime:
- Each router decrements TTL by 1
- When TTL reaches 0, packet dropped
- ICMP "Time Exceeded" sent back to source

Purpose: Prevents routing loops from circulating packets forever.
Default value: 64 (configurable via sysctl).
[Basic]

---

Q: What are the main ICMP message types?
A: Key ICMP types:
- **Type 0** - Echo Reply (ping response)
- **Type 3** - Destination Unreachable
  - Code 0: Network unreachable
  - Code 1: Host unreachable
  - Code 3: Port unreachable
  - Code 4: Fragmentation needed
- **Type 8** - Echo Request (ping)
- **Type 11** - Time Exceeded (TTL=0)

Defined in `include/linux/icmp.h`.
[Basic]

---

Q: What is the inet_protos[] array?
A: `inet_protos[]` is the transport protocol handler table:
```c
struct net_protocol *inet_protos[MAX_INET_PROTOS];

// Registration
inet_add_protocol(&tcp_protocol, IPPROTO_TCP);  // index 6
inet_add_protocol(&udp_protocol, IPPROTO_UDP);  // index 17

// Dispatch in ip_local_deliver_finish()
ipprot = inet_protos[protocol];
ipprot->handler(skb);  // e.g., tcp_v4_rcv()
```
Indexed by IP protocol number (TCP=6, UDP=17, ICMP=1).
[Intermediate]

---

Q: What is IP source routing and why is it usually disabled?
A: Source routing allows sender to specify route via IP options:
- Loose Source Route (LSR): Packet must visit listed routers
- Strict Source Route (SSR): Packet must visit ONLY listed routers

Security risk: Can bypass firewalls, spoof internal addresses. Disabled by default (`accept_source_route = 0`).
[Advanced]

---

Q: What is the local routing table entry RTN_LOCAL?
A: `RTN_LOCAL` indicates destination IP belongs to this host:
```c
rt->rt_type = RTN_LOCAL;
```
When `ip_route_input()` returns this type:
- Packet delivered to local transport layer
- Uses `ip_local_deliver()` path
- Socket lookup finds local application

vs `RTN_UNICAST` which indicates forwarding.
[Intermediate]

---

Q: (Cloze) IP receive path: ip_rcv() → _____ hook → ip_rcv_finish() → _____ → ip_local_deliver_finish()
A: NF_INET_PRE_ROUTING, NF_INET_LOCAL_IN
[Intermediate]

---

Q: (Cloze) IP transmit path: ip_local_out() → _____ hook → ip_output() → _____ hook → ip_finish_output()
A: NF_INET_LOCAL_OUT, NF_INET_POST_ROUTING
[Intermediate]

---

Q: What is struct iphdr and how is it accessed?
A: `struct iphdr` represents the IPv4 header:
```c
struct iphdr {
    __u8    version:4, ihl:4;  // Or ihl:4, version:4 on big-endian
    __u8    tos;
    __be16  tot_len;
    __be16  id;
    __be16  frag_off;
    __u8    ttl;
    __u8    protocol;
    __sum16 check;
    __be32  saddr;
    __be32  daddr;
};

// Access from sk_buff
struct iphdr *iph = ip_hdr(skb);
```
[Intermediate]

---

Q: What is the ip_options structure used for?
A: `ip_options` stores parsed IP header options:
```c
struct ip_options {
    __be32  faddr;          // First hop for source routing
    unsigned char optlen;   // Option length
    unsigned char srr;      // Source route offset
    unsigned char rr;       // Record route offset
    unsigned char ts;       // Timestamp offset
    // ... flags and offsets
};
```
Options are rarely used today (security concerns, overhead).
[Advanced]

---

Q: (Understanding) Why does Linux use a routing cache (before 3.6)?
A: The routing cache (removed in 3.6) stored recent route lookups:
- FIB lookup is expensive (trie traversal)
- Most traffic goes to same destinations repeatedly
- Cache hit: O(1) hash lookup
- Cache miss: Full FIB lookup, then cache result

Removed due to DoS vulnerability (cache thrashing) and improved FIB algorithms.
[Advanced]

---

Q: What is the difference between ip_queue_xmit() and ip_push_pending_frames()?
A:
- `ip_queue_xmit()` - For TCP: packet already complete, just add IP header and send
- `ip_push_pending_frames()` - For UDP: assemble fragments from cork buffer, then send

```c
// TCP path
tcp_transmit_skb() → ip_queue_xmit()

// UDP path
udp_sendmsg() → ip_append_data() → udp_push_pending_frames() → ip_push_pending_frames()
```
[Intermediate]

---

Q: (Reverse) This function determines whether a received packet should be delivered locally or forwarded.
A: Q: What is `ip_route_input()`?
[Intermediate]

---

Q: What happens when ip_forward() finds TTL has reached 1?
A: When TTL would become 0 after decrement:
1. Packet is dropped
2. ICMP Time Exceeded (type 11, code 0) sent to source
3. Counter `IPSTATS_MIB_INHDRERRORS` incremented

```c
if (ip_hdr(skb)->ttl <= 1) {
    icmp_send(skb, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL, 0);
    goto drop;
}
```
[Intermediate]

---

## Section 7: Link Layer & Neighbor

---

Q: What is the ARP (Address Resolution Protocol) and what problem does it solve?
A: ARP maps IPv4 addresses to MAC (hardware) addresses:
- IP layer knows next-hop IP (from routing)
- Link layer needs MAC address to send frame
- ARP asks "who has IP X? Tell me your MAC"

```
Host A: ARP Request (broadcast): "Who has 192.168.1.1?"
Host B: ARP Reply (unicast): "192.168.1.1 is at aa:bb:cc:dd:ee:ff"
```
[Basic]

---

Q: (ASCII Diagram) Show the ARP packet structure.
A:
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Hardware Type         |         Protocol Type         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  HW Addr Len  | Proto Addr Len|           Operation           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Sender Hardware Address (6 bytes)             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Sender Protocol Address (4 bytes)             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Target Hardware Address (6 bytes)             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Target Protocol Address (4 bytes)             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Operation: 1=Request, 2=Reply
```
[Basic]

---

Q: What is the Linux neighbor subsystem?
A: The neighbor subsystem (`net/core/neighbour.c`) provides generic L3-to-L2 address resolution:
- ARP for IPv4
- NDP (Neighbor Discovery) for IPv6
- Generic framework for other protocols

Key structures: `struct neighbour`, `struct neigh_table`
Key functions: `neigh_lookup()`, `neigh_create()`, `neigh_resolve_output()`
[Intermediate]

---

Q: What is struct neighbour and what does it contain?
A: `struct neighbour` represents a neighbor cache entry:
```c
struct neighbour {
    struct net_device   *dev;        // Network device
    u8                  ha[ALIGN(MAX_ADDR_LEN, sizeof(long))];  // Hardware address
    u8                  nud_state;   // Neighbor state
    atomic_t            refcnt;      // Reference count
    struct sk_buff_head arp_queue;   // Packets waiting for resolution
    struct timer_list   timer;       // State timeout
    // ...
};
```
One entry per (IP, interface) pair.
[Intermediate]

---

Q: What are the NUD (Neighbor Unreachability Detection) states?
A: Key neighbor states (`nud_state`):
- `NUD_NONE` - No state
- `NUD_INCOMPLETE` - Resolution in progress, ARP sent
- `NUD_REACHABLE` - Recently confirmed reachable
- `NUD_STALE` - Not recently confirmed, may be stale
- `NUD_DELAY` - Waiting for upper-layer confirmation
- `NUD_PROBE` - Actively probing (sending ARP)
- `NUD_FAILED` - Resolution failed
- `NUD_PERMANENT` - Manually configured, never expires
[Intermediate]

---

Q: (ASCII Diagram) Show the neighbor state machine transitions.
A:
```
                   ┌──────────────┐
           ARP req │              │ Timeout
    ┌──────────────► NUD_INCOMPLETE├──────────────┐
    │              │              │               │
    │              └──────┬───────┘               ▼
    │                     │ ARP reply       ┌──────────┐
    │                     │                 │NUD_FAILED│
    │                     ▼                 └──────────┘
    │              ┌──────────────┐
    │              │ NUD_REACHABLE│◄──── Confirmation
    │              │              │       (TCP ACK, etc.)
    │              └──────┬───────┘
    │                     │ Timeout (no traffic)
    │                     ▼
    │              ┌──────────────┐
    │              │  NUD_STALE   │
    │              │              │
    │              └──────┬───────┘
    │                     │ Traffic sent
    │                     ▼
    │              ┌──────────────┐
    │              │  NUD_DELAY   │ Wait for upper confirm
    └──────────────│              │
       No confirm  └──────┬───────┘
       (probe)            │ Upper layer confirms
                          ▼
                   ┌──────────────┐
                   │ NUD_REACHABLE│
                   └──────────────┘
```
[Intermediate]

---

Q: What is struct neigh_table?
A: `struct neigh_table` represents a neighbor protocol (ARP, NDP):
```c
struct neigh_table {
    struct neigh_table  *next;
    int                 family;       // AF_INET, AF_INET6
    struct neighbour    **hash_buckets;
    int                 (*constructor)(struct neighbour *);
    void                (*solicit)(struct neighbour *, struct sk_buff *);
    // ...
};
```
`arp_tbl` for IPv4, `nd_tbl` for IPv6. Registered at init.
[Intermediate]

---

Q: How does the kernel handle packets when neighbor resolution is pending?
A: When MAC address unknown:
1. Packet triggers `neigh_resolve_output()`
2. If `NUD_INCOMPLETE`, packet queued to `neigh->arp_queue`
3. ARP request sent (if not recently sent)
4. When ARP reply arrives, queued packets sent
5. Queue has limit (`neigh->arp_queue_len_bytes`), excess dropped

Prevents blocking while waiting for ARP.
[Intermediate]

---

Q: What is the ARP receive path?
A:
```
netif_receive_skb()
    │
    ▼
arp_rcv()                 ◄── Protocol handler (ETH_P_ARP)
    │
    ▼
arp_process()             ◄── Main ARP processing
    │
    ├── Is it for us? (target IP matches)
    │
    ├── If ARP REQUEST: Send ARP REPLY
    │
    └── Update neighbor cache
```
Located in `net/ipv4/arp.c`.
[Intermediate]

---

Q: What is the Ethernet header structure?
A:
```c
struct ethhdr {
    unsigned char   h_dest[ETH_ALEN];    // Destination MAC (6 bytes)
    unsigned char   h_source[ETH_ALEN];  // Source MAC (6 bytes)
    __be16          h_proto;             // Protocol type (2 bytes)
};
```
Total: 14 bytes. Accessed via `eth_hdr(skb)`.

`h_proto` values: `ETH_P_IP` (0x0800), `ETH_P_ARP` (0x0806), `ETH_P_IPV6` (0x86DD)
[Basic]

---

Q: What is Gratuitous ARP and when is it used?
A: Gratuitous ARP is an ARP announcement without request:
- Sender IP = Target IP (announcing own address)
- Broadcast, no reply expected

Use cases:
1. **IP conflict detection** - Check if IP already in use
2. **Failover** - Update neighbors' caches when IP moves
3. **Boot announcement** - Populate neighbors' caches

```c
arp_send(ARPOP_REQUEST, ETH_P_ARP, own_ip, dev, own_ip, NULL, dev->dev_addr, NULL);
```
[Intermediate]

---

Q: What is ARP cache poisoning and how is Linux protected?
A: ARP cache poisoning: Attacker sends fake ARP replies to redirect traffic.

Linux protections:
- Only update cache for entries we requested (by default)
- `arp_accept` sysctl controls unsolicited update
- `arp_ignore` sysctl controls which interfaces respond
- Can use static ARP entries (`NUD_PERMANENT`)

Still vulnerable; consider 802.1X or IPsec for security.
[Advanced]

---

Q: (Cloze) ARP maps _____ addresses to _____ addresses, while RARP does the reverse.
A: IP (L3), MAC/hardware (L2)
[Basic]

---

Q: What is the `ip neigh` command showing?
A: `ip neigh` displays the kernel's neighbor cache:
```
192.168.1.1 dev eth0 lladdr aa:bb:cc:dd:ee:ff REACHABLE
192.168.1.2 dev eth0 lladdr 11:22:33:44:55:66 STALE
192.168.1.3 dev eth0  INCOMPLETE
```
Shows: IP, interface, MAC address (if known), NUD state.

Corresponds to `struct neighbour` entries in `arp_tbl`.
[Basic]

---

Q: (Understanding) Why does the neighbor cache have STALE and DELAY states instead of immediately probing?
A: Performance optimization:
- **STALE** - Entry might still be valid; don't waste bandwidth probing
- **DELAY** - Give upper layer (TCP) chance to confirm via ACKs

If TCP ACK arrives during DELAY, neighbor confirmed without extra ARP. Only probe if no confirmation. Reduces ARP traffic significantly.
[Intermediate]

---

## Section 8: Network Device & Drivers

---

Q: What is struct net_device and what are its key fields?
A: `struct net_device` represents a network interface:
```c
struct net_device {
    char                    name[IFNAMSIZ];     // "eth0", "lo"
    unsigned int            ifindex;            // Unique interface ID
    unsigned int            flags;              // IFF_UP, IFF_RUNNING
    unsigned int            mtu;                // Maximum transmission unit
    unsigned char           dev_addr[MAX_ADDR_LEN];  // MAC address
    
    const struct net_device_ops *netdev_ops;    // Driver operations
    struct netdev_queue     *_tx;               // TX queues
    struct napi_struct      *napi;              // NAPI contexts
    struct Qdisc            *qdisc;             // Traffic control
    // ...
};
```
[Intermediate]

---

Q: What is struct net_device_ops and what callbacks does it define?
A: `net_device_ops` defines driver callbacks:
```c
struct net_device_ops {
    int  (*ndo_open)(struct net_device *dev);           // ifconfig up
    int  (*ndo_stop)(struct net_device *dev);           // ifconfig down
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,  // Transmit packet
                                  struct net_device *dev);
    void (*ndo_set_rx_mode)(struct net_device *dev);    // Multicast/promisc
    int  (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int  (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
    struct net_device_stats *(*ndo_get_stats)(struct net_device *dev);
    // ...
};
```
[Intermediate]

---

Q: What does ndo_start_xmit() do and what are its return values?
A: `ndo_start_xmit()` transmits a packet:
```c
netdev_tx_t my_xmit(struct sk_buff *skb, struct net_device *dev)
{
    // DMA skb data to NIC ring buffer
    // Return status
}
```
Return values:
- `NETDEV_TX_OK` - Packet accepted
- `NETDEV_TX_BUSY` - Queue full, retry later (should be rare)

Driver owns skb after return; must free when TX complete.
[Intermediate]

---

Q: How does a driver register a network device?
A:
```c
struct net_device *dev = alloc_etherdev(sizeof(struct my_priv));

// Set up device
dev->netdev_ops = &my_netdev_ops;
dev->ethtool_ops = &my_ethtool_ops;
memcpy(dev->dev_addr, mac_addr, ETH_ALEN);

// Register with kernel
int err = register_netdev(dev);
if (err) {
    free_netdev(dev);
    return err;
}
```
`alloc_etherdev()` allocates device + private data. `register_netdev()` adds to global list.
[Intermediate]

---

Q: What is NAPI and why was it introduced?
A: NAPI (New API) is a polling-based packet receive mechanism:

**Problem with pure interrupts:**
- High packet rates = interrupt storm
- Context switches dominate CPU

**NAPI solution:**
1. Interrupt triggers poll scheduling
2. Disable interrupts, switch to polling
3. Process packets in batch
4. Re-enable interrupts when done

Dramatically improves high-throughput performance.
[Intermediate]

---

Q: (ASCII Diagram) Show the NAPI receive flow.
A:
```
                    Hardware IRQ
                         │
                         ▼
              ┌─────────────────────┐
              │  driver_irq_handler │
              │  napi_schedule()    │
              │  • Disable IRQ      │
              │  • Add to poll list │
              └──────────┬──────────┘
                         │
                         ▼
              ┌─────────────────────┐
              │  NET_RX_SOFTIRQ     │
              │  net_rx_action()    │
              └──────────┬──────────┘
                         │
         ┌───────────────┼───────────────┐
         │               │               │
         ▼               ▼               ▼
   ┌───────────┐   ┌───────────┐   ┌───────────┐
   │napi_poll()│   │napi_poll()│   │napi_poll()│
   │ (budget)  │   │ (budget)  │   │ (budget)  │
   └─────┬─────┘   └───────────┘   └───────────┘
         │
         ▼
   Process packets, call napi_gro_receive()
         │
         ▼
   if (work < budget) → napi_complete(), re-enable IRQ
```
[Intermediate]

---

Q: What is the NAPI budget and how is it used?
A: Budget limits packets processed per poll:
```c
int my_napi_poll(struct napi_struct *napi, int budget)
{
    int work_done = 0;
    
    while (work_done < budget && packets_available()) {
        process_packet();
        work_done++;
    }
    
    if (work_done < budget) {
        napi_complete(napi);    // Done polling
        enable_irq();           // Re-enable interrupts
    }
    
    return work_done;
}
```
Default budget: 64. Prevents one device from starving others.
[Intermediate]

---

Q: What are napi_schedule() and napi_complete()?
A:
- `napi_schedule(&napi)` - Schedule NAPI poll, called from IRQ handler
  - Sets NAPI_STATE_SCHED flag
  - Adds to per-CPU poll list
  - Raises NET_RX_SOFTIRQ

- `napi_complete(&napi)` - Mark poll complete
  - Clears NAPI_STATE_SCHED
  - Allows re-scheduling on next interrupt

```c
// In IRQ handler
if (napi_schedule_prep(&napi)) {
    __napi_schedule(&napi);
}
```
[Intermediate]

---

Q: What is a ring buffer in network drivers?
A: Ring buffers are circular queues for NIC DMA:
```
             ┌───┬───┬───┬───┬───┬───┬───┬───┐
Descriptors: │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │
             └───┴───┴───┴───┴───┴───┴───┴───┘
               ▲           ▲
               │           │
             Head        Tail
           (consumer)   (producer)
```
- **TX ring**: Driver writes descriptors, NIC reads and transmits
- **RX ring**: NIC writes received data, driver processes

DMA allows NIC to read/write without CPU involvement.
[Intermediate]

---

Q: What is the TX queue start/stop mechanism?
A: Flow control between stack and driver:
```c
// Driver: queue full, stop flow
netif_stop_queue(dev);

// Driver: TX complete, space available
netif_wake_queue(dev);

// Stack checks before sending
if (netif_queue_stopped(dev))
    return NETDEV_TX_BUSY;
```
Prevents stack from overwhelming driver when TX ring full.
[Intermediate]

---

Q: What are the IFF_* interface flags?
A: Key interface flags (`dev->flags`):
- `IFF_UP` - Interface administratively up
- `IFF_RUNNING` - Interface operationally up (link detected)
- `IFF_BROADCAST` - Supports broadcast
- `IFF_MULTICAST` - Supports multicast
- `IFF_PROMISC` - Promiscuous mode (receive all packets)
- `IFF_NOARP` - No ARP (point-to-point)
- `IFF_LOOPBACK` - Loopback interface

Viewable via `ifconfig` or `ip link show`.
[Basic]

---

Q: What is ethtool and what does the driver provide?
A: `ethtool` is the standard NIC configuration tool. Driver implements:
```c
struct ethtool_ops {
    int (*get_settings)(struct net_device *, struct ethtool_cmd *);
    int (*set_settings)(struct net_device *, struct ethtool_cmd *);
    void (*get_drvinfo)(struct net_device *, struct ethtool_drvinfo *);
    int (*get_regs_len)(struct net_device *);
    void (*get_regs)(struct net_device *, struct ethtool_regs *, void *);
    int (*nway_reset)(struct net_device *);  // Restart autoneg
    u32 (*get_link)(struct net_device *);    // Link status
    int (*get_coalesce)(struct net_device *, struct ethtool_coalesce *);
    // ...
};
```
[Intermediate]

---

Q: What is interrupt coalescing?
A: Coalescing delays interrupts to batch processing:
```
Without coalescing:        With coalescing:
Pkt→IRQ                    Pkt
Pkt→IRQ                    Pkt
Pkt→IRQ                    Pkt → IRQ (batch of 3)
Pkt→IRQ                    Pkt
Pkt→IRQ                    Pkt
Pkt→IRQ                    Pkt → IRQ (batch of 3)
```
Configured via ethtool: `ethtool -C eth0 rx-usecs 50 rx-frames 32`

Trade-off: Higher throughput vs higher latency.
[Intermediate]

---

Q: What are scatter-gather and TX checksumming offloads?
A: Hardware offloads reduce CPU work:

**Scatter-Gather (SG):**
- TX: Send from multiple memory regions without copying
- Uses `skb_shinfo(skb)->frags[]`
- Enables zero-copy from user pages

**TX Checksum:**
- NIC computes IP/TCP/UDP checksum
- `skb->ip_summed = CHECKSUM_PARTIAL`
- Stack only computes pseudo-header checksum
[Intermediate]

---

Q: (Cloze) When NIC ring buffer is full, driver calls _____ to stop the stack, and _____ when space available.
A: `netif_stop_queue()`, `netif_wake_queue()`
[Intermediate]

---

Q: What is multi-queue networking?
A: Multiple independent TX/RX queues per NIC:
```c
dev->num_tx_queues = 8;
dev->num_rx_queues = 8;
```
Benefits:
- Parallel processing on multiple CPUs
- Each queue has own NAPI context
- Reduces lock contention
- Can map flows to queues (RSS/RPS)

Access: `netdev_get_tx_queue(dev, i)`, per-queue stats.
[Intermediate]

---

Q: What is the loopback device and how is it special?
A: `lo` (loopback) is a software-only device:
- No hardware, no DMA
- `ndo_start_xmit` directly calls `netif_rx()`
- Used for localhost (127.0.0.1) communication
- Always up, no link state

```c
// loopback_xmit() simplified
skb->protocol = eth_type_trans(skb, dev);
netif_rx(skb);  // Directly to RX path
return NETDEV_TX_OK;
```
Defined in `drivers/net/loopback.c`.
[Intermediate]

---

Q: (Reverse) This callback in net_device_ops is called when the interface is brought up (ifconfig up).
A: Q: What is `ndo_open()`?
[Basic]

---

Q: (Understanding) Why does the driver need to free sk_buffs after TX?
A: The stack passes ownership to driver on TX:
1. Driver queues descriptor pointing to sk_buff data
2. NIC DMAs data to wire
3. NIC signals completion (interrupt or polling)
4. Driver MUST call `dev_kfree_skb()` or `consume_skb()`

If not freed: Memory leak. If freed too early: NIC reads freed memory.
[Intermediate]

---

Q: How does the driver allocate sk_buffs for RX?
A: Driver pre-allocates buffers for DMA:
```c
skb = netdev_alloc_skb_ip_align(dev, buf_len);
dma_addr = dma_map_single(&pdev->dev, skb->data, buf_len, DMA_FROM_DEVICE);
// Store in ring descriptor

// On RX:
dma_unmap_single(&pdev->dev, dma_addr, buf_len, DMA_FROM_DEVICE);
skb_put(skb, pkt_len);
skb->protocol = eth_type_trans(skb, dev);
napi_gro_receive(&napi, skb);
```
Must handle DMA mapping/unmapping correctly for cache coherency.
[Intermediate]

---

## Section 9: Packet Processing Paths

---

Q: (ASCII Diagram) Show the complete RX path from wire to application.
A:
```
  WIRE
    │
    ▼
┌─────────────┐
│  NIC DMA    │  Hardware writes to ring buffer
└──────┬──────┘
       │ IRQ
       ▼
┌─────────────┐
│napi_schedule│  Schedule softirq
└──────┬──────┘
       │
       ▼
┌─────────────┐
│net_rx_action│  NET_RX_SOFTIRQ handler
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ napi_poll() │  Driver processes ring, calls:
│   └─► napi_gro_receive()
└──────┬──────┘
       │
       ▼
┌─────────────┐
│netif_receive│  Protocol dispatch
│   _skb()    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  ip_rcv()   │  IP processing + netfilter
└──────┬──────┘
       │
       ▼
┌─────────────┐
│tcp_v4_rcv() │  TCP processing
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ sk_receive  │  Queue to socket
│   _queue    │
└──────┬──────┘
       │ wake
       ▼
  APPLICATION recv()
```
[Intermediate]

---

Q: (ASCII Diagram) Show the complete TX path from application to wire.
A:
```
  APPLICATION send()
       │
       ▼
┌─────────────┐
│tcp_sendmsg()│  Copy data, queue to sk_write_queue
└──────┬──────┘
       │
       ▼
┌─────────────┐
│tcp_write    │  Segment, add TCP header
│   _xmit()   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ip_queue_xmit│  Add IP header, route
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ ip_output() │  Netfilter + fragmentation
└──────┬──────┘
       │
       ▼
┌─────────────┐
│neigh_output │  Resolve MAC, add Ethernet header
└──────┬──────┘
       │
       ▼
┌─────────────┐
│dev_queue    │  Qdisc scheduling
│   _xmit()   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ndo_start    │  Driver DMA to NIC
│   _xmit()   │
└──────┬──────┘
       │
       ▼
  WIRE
```
[Intermediate]

---

Q: What is NET_RX_SOFTIRQ and when does it run?
A: `NET_RX_SOFTIRQ` is the receive path softirq:
- Raised by `napi_schedule()` in IRQ handler
- Runs in softirq context (interrupts enabled)
- Handler: `net_rx_action()`
- Processes NAPI poll list, calls driver poll functions

Runs either:
1. After hardware IRQ returns
2. In ksoftirqd thread if too much softirq work
[Intermediate]

---

Q: What is NET_TX_SOFTIRQ and what does it do?
A: `NET_TX_SOFTIRQ` handles transmit completion:
- Runs `net_tx_action()`
- Processes completion queues
- Frees transmitted sk_buffs
- Restarts stopped queues

Less critical than RX softirq; TX often handled inline or via driver cleanup.
[Intermediate]

---

Q: What is GRO (Generic Receive Offload)?
A: GRO merges multiple received packets into one large sk_buff:
```
Individual packets:     After GRO:
┌─────┐                 ┌─────────────────┐
│pkt 1│  64B            │  merged packet  │  192B
├─────┤                 │                 │
│pkt 2│  64B   ───►     │  (3 segments)   │
├─────┤                 │                 │
│pkt 3│  64B            └─────────────────┘
└─────┘
```
Benefits: Fewer packets through stack, better cache usage, amortizes per-packet overhead.

Called via `napi_gro_receive()` or `napi_gro_flush()`.
[Intermediate]

---

Q: What conditions must be met for GRO to merge packets?
A: Packets can merge if:
1. Same flow (5-tuple: src/dst IP, src/dst port, protocol)
2. Sequential TCP sequence numbers
3. Same TCP flags (no SYN, FIN, RST, URG)
4. Same IP header (except length, checksum)
5. No IP options that differ

If mismatch, current GRO batch flushed, new batch started.
[Intermediate]

---

Q: What is GSO (Generic Segmentation Offload)?
A: GSO segments large packets into MTU-sized packets:
```
Large packet (64KB):    After GSO:
┌─────────────────┐     ┌─────┐ ┌─────┐ ┌─────┐
│   TCP segment   │     │1.5KB│ │1.5KB│ │1.5KB│ ...
│    (64KB)       │ ──► └─────┘ └─────┘ └─────┘
└─────────────────┘     (many MTU-sized packets)
```
GSO can be:
- **TSO (TCP Segmentation Offload)**: Hardware does segmentation
- **Software GSO**: Kernel does segmentation if no hardware support

Delays segmentation as late as possible for efficiency.
[Intermediate]

---

Q: What is the difference between TSO and GSO?
A:
- **TSO (TCP Segmentation Offload)**: Hardware performs segmentation
  - NIC splits large TCP segment into MTU-sized packets
  - Requires NIC support (`NETIF_F_TSO` feature)
  - Most efficient

- **GSO (Generic Segmentation Offload)**: Software fallback
  - Kernel does segmentation in `dev_queue_xmit()`
  - Works on any NIC
  - Still better than segmenting early (one packet through most of stack)
[Intermediate]

---

Q: What are the checksum offload modes?
A: `skb->ip_summed` values:

**RX (receive):**
- `CHECKSUM_NONE` - No checksum done, software must verify
- `CHECKSUM_UNNECESSARY` - Hardware verified, known good
- `CHECKSUM_COMPLETE` - Hardware computed csum of whole packet

**TX (transmit):**
- `CHECKSUM_NONE` - Software computed full checksum
- `CHECKSUM_PARTIAL` - Software did pseudo-header, hardware completes
[Intermediate]

---

Q: What is RPS (Receive Packet Steering)?
A: RPS distributes RX processing across CPUs in software:
```
Without RPS:              With RPS:
  ┌─────┐                   ┌─────┐
  │NIC Q│                   │NIC Q│
  └──┬──┘                   └──┬──┘
     │                         │
     ▼                         ▼
  ┌─────┐               ┌─────────────┐
  │CPU 0│               │ Hash packet │
  │only │               └──────┬──────┘
  └─────┘                 ┌────┼────┐
                          ▼    ▼    ▼
                       CPU0  CPU1  CPU2
```
Software alternative to multi-queue NICs. Configured via `/sys/class/net/eth0/queues/rx-0/rps_cpus`.
[Intermediate]

---

Q: What is RFS (Receive Flow Steering)?
A: RFS steers packets to the CPU where the application runs:
```
Without RFS:              With RFS:
App on CPU2               App on CPU2
    │                         │
    │ recv()                  │ recv()
    ▼                         ▼
Packet on CPU0            Packet steered to CPU2
    │                         │
   (IPI needed)              (local processing)
```
Improves cache locality. Kernel tracks which CPU each flow's socket is active on.

Enable: `/proc/sys/net/core/rps_sock_flow_entries`
[Intermediate]

---

Q: What is XPS (Transmit Packet Steering)?
A: XPS maps TX to specific queues/CPUs:
```
CPU 0 ──► TX Queue 0
CPU 1 ──► TX Queue 1
CPU 2 ──► TX Queue 2
```
Benefits:
- Cache locality (CPU's packets stay on its queue)
- Avoid lock contention between CPUs
- Better interaction with RX on same queue

Configured: `/sys/class/net/eth0/queues/tx-0/xps_cpus`
[Intermediate]

---

Q: What happens in __netif_receive_skb_core()?
A: Core RX processing after NAPI:
```c
__netif_receive_skb_core(skb)
{
    // 1. Generic XDP
    // 2. Packet taps (tcpdump via AF_PACKET)
    // 3. Ingress qdisc (traffic control)
    // 4. Bridge check - forward to bridge port?
    // 5. Protocol handler dispatch:
    ptype = ptype_base[ntohs(skb->protocol)];
    ptype->func(skb);  // e.g., ip_rcv()
}
```
Dispatches to protocol handler based on `skb->protocol`.
[Intermediate]

---

Q: What is the backlog queue and when is it used?
A: Per-CPU backlog queue for deferred RX processing:
- Used when packets arrive faster than processing
- Used by `netif_rx()` (non-NAPI path)
- Used by RPS to send to other CPUs

```c
enqueue_to_backlog(skb, cpu);
// Triggers softirq on target CPU
```
Has limit (`netdev_budget`), excess dropped.
[Intermediate]

---

Q: (Cloze) GRO merges packets on _____, while GSO segments packets on _____.
A: receive (RX), transmit (TX)
[Basic]

---

Q: What is the packet_type structure and how is it used?
A: `struct packet_type` registers protocol handlers:
```c
struct packet_type {
    __be16          type;   // ETH_P_IP, ETH_P_ARP, etc.
    struct net_device *dev; // NULL for all devices
    int (*func)(struct sk_buff *, struct net_device *,
                struct packet_type *, struct net_device *);
    // ...
};

// Registration
dev_add_pack(&ip_packet_type);  // ip_rcv for ETH_P_IP
```
`netif_receive_skb()` iterates `ptype_base[]` hash table.
[Intermediate]

---

Q: (Understanding) Why is softirq context used instead of hardirq for packet processing?
A: Softirq advantages:
1. **Interrupts enabled** - Other IRQs can fire, better latency
2. **Preemptible (with RT patches)** - Can be interrupted
3. **Batch processing** - Process multiple packets per run
4. **Can sleep (sort of)** - Can be deferred to ksoftirqd

Hardirq should be minimal: Just acknowledge IRQ and schedule softirq.
[Intermediate]

---

Q: What is skb_dst(skb) and how is it used?
A: `skb_dst(skb)` returns the routing decision attached to the packet:
```c
struct dst_entry *dst = skb_dst(skb);
struct rtable *rt = (struct rtable *)dst;

// Use cached route for output
dst->output(skb);  // e.g., ip_output
```
Set by `ip_route_input()` or `ip_route_output()`. Avoids repeated route lookups for same connection.
[Intermediate]

---

Q: What is the difference between netif_rx() and napi_gro_receive()?
A:
- `netif_rx(skb)` - Old non-NAPI interface
  - Queues to backlog, triggers softirq
  - Used by loopback, virtual devices
  - Higher overhead

- `napi_gro_receive(napi, skb)` - Modern NAPI + GRO
  - Tries GRO merge first
  - Processes in current softirq context
  - More efficient
[Intermediate]

---

Q: (Reverse) This softirq handler processes the NAPI poll list.
A: Q: What is `net_rx_action()`?
[Intermediate]

---

## Section 10: Netfilter

---

Q: What is Netfilter and what is its purpose?
A: Netfilter is the packet filtering framework in the Linux kernel:
- Provides hooks at strategic points in packet path
- Allows modules to inspect/modify/drop packets
- Foundation for iptables, nftables, NAT, connection tracking

Key files: `net/netfilter/core.c`, `net/ipv4/netfilter/`
[Basic]

---

Q: (ASCII Diagram) Show the Netfilter hook points and packet flow.
A:
```
                        LOCAL PROCESS
                             ▲   │
                             │   │
                         [INPUT]  [OUTPUT]
                             │   │
                             │   ▼
  ┌─────────┐          ┌─────┴───────────┐          ┌─────────┐
  │PREROUTING│─────────►│ Routing Decision │◄─────────│POSTROUTING│
  └────┬────┘          └────────┬────────┘          └────▲────┘
       │                        │                        │
       │                   [FORWARD]                     │
       │                        │                        │
       │                        ▼                        │
  ┌────┴────────────────────────┴────────────────────────┴────┐
  │                         Network                            │
  └────────────────────────────────────────────────────────────┘

Packet Flow:
- Incoming: PREROUTING → (route) → INPUT → Local Process
- Outgoing: Local Process → OUTPUT → (route) → POSTROUTING
- Forwarded: PREROUTING → (route) → FORWARD → POSTROUTING
```
[Basic]

---

Q: What are the five Netfilter hook points for IPv4?
A:
```c
enum nf_inet_hooks {
    NF_INET_PRE_ROUTING,   // After sanity check, before routing
    NF_INET_LOCAL_IN,      // After routing, destined locally
    NF_INET_FORWARD,       // After routing, to be forwarded
    NF_INET_LOCAL_OUT,     // From local process, before routing
    NF_INET_POST_ROUTING,  // After routing, about to leave
    NF_INET_NUMHOOKS
};
```
Each hook called via `NF_HOOK()` macro in IP path.
[Basic]

---

Q: What are the possible return values from a Netfilter hook function?
A:
```c
#define NF_DROP   0   // Drop packet, free sk_buff
#define NF_ACCEPT 1   // Continue to next hook/normal processing
#define NF_STOLEN 2   // Handler took ownership, don't free
#define NF_QUEUE  3   // Queue to userspace (NFQUEUE)
#define NF_REPEAT 4   // Call this hook again
#define NF_STOP   5   // Stop hook traversal, accept packet
```
Most common: NF_ACCEPT (allow) and NF_DROP (block).
[Intermediate]

---

Q: What is struct nf_hook_ops and how is it registered?
A: `nf_hook_ops` defines a hook callback:
```c
struct nf_hook_ops {
    nf_hookfn       *hook;      // Callback function
    struct module   *owner;     // THIS_MODULE
    u_int8_t        pf;         // Protocol family (NFPROTO_IPV4)
    unsigned int    hooknum;    // Which hook (NF_INET_PRE_ROUTING)
    int             priority;   // Order (lower = earlier)
};

// Registration
nf_register_hook(&my_hook_ops);

// Callback signature
unsigned int my_hook(unsigned int hooknum, struct sk_buff *skb,
                     const struct net_device *in,
                     const struct net_device *out,
                     int (*okfn)(struct sk_buff *));
```
[Intermediate]

---

Q: What is the NF_HOOK() macro and how does it work?
A: `NF_HOOK()` invokes netfilter at a hook point:
```c
// In ip_rcv():
return NF_HOOK(NFPROTO_IPV4, NF_INET_PRE_ROUTING, skb, dev, NULL,
               ip_rcv_finish);
```
It:
1. Iterates registered hooks at this point
2. Calls each hook function in priority order
3. If all return NF_ACCEPT, calls `okfn` (ip_rcv_finish)
4. If any returns NF_DROP, drops packet

Defined in `include/linux/netfilter.h`.
[Intermediate]

---

Q: What is connection tracking (conntrack)?
A: Connection tracking tracks network connections state:
```
NEW       - First packet of connection (SYN)
ESTABLISHED - Bidirectional traffic seen
RELATED   - Related to existing connection (ICMP error, FTP data)
INVALID   - Doesn't match any connection
```
Used by:
- Stateful firewall rules (`-m state --state ESTABLISHED`)
- NAT (track original addresses)
- ALGs (Application Layer Gateways)

Implemented in `net/netfilter/nf_conntrack_*.c`.
[Intermediate]

---

Q: What is struct nf_conn?
A: `nf_conn` stores connection tracking entry:
```c
struct nf_conn {
    struct nf_conntrack_tuple_hash tuplehash[IP_CT_DIR_MAX];
    unsigned long status;       // Connection state
    u32 mark;                   // User mark
    struct nf_conn *master;     // Parent connection
    struct timer_list timeout;  // Entry expiration
    // ...
};
```
One per tracked connection. Stored in hash table, looked up by 5-tuple.
[Intermediate]

---

Q: What is NAT and how does Netfilter implement it?
A: NAT (Network Address Translation) rewrites addresses:
- **SNAT** (Source NAT): Change source IP/port (at POSTROUTING)
- **DNAT** (Destination NAT): Change dest IP/port (at PREROUTING)
- **Masquerade**: SNAT using outgoing interface's IP

Implementation:
1. DNAT modifies on first packet
2. Conntrack records translation
3. Reply packets automatically reversed

```bash
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
```
[Intermediate]

---

Q: What is the relationship between iptables tables and chains?
A:
```
Tables (contain rules for specific purpose):
┌─────────┬─────────────────────────────────────┐
│  Table  │  Chains Available                   │
├─────────┼─────────────────────────────────────┤
│ filter  │ INPUT, FORWARD, OUTPUT              │
│ nat     │ PREROUTING, OUTPUT, POSTROUTING     │
│ mangle  │ All five hooks                      │
│ raw     │ PREROUTING, OUTPUT                  │
└─────────┴─────────────────────────────────────┘

Chains map to Netfilter hooks.
```
Rules in chains evaluated in order; first match wins.
[Intermediate]

---

Q: What is the order of table evaluation at each hook?
A: Tables processed in this order:
```
PREROUTING:  raw → mangle → nat (DNAT)
INPUT:       mangle → filter
FORWARD:     mangle → filter
OUTPUT:      raw → mangle → nat (DNAT) → filter
POSTROUTING: mangle → nat (SNAT)
```
`raw` disables conntrack. `mangle` modifies packets. `filter` accepts/drops.
[Intermediate]

---

Q: What is NFQUEUE and how is it used?
A: NFQUEUE sends packets to userspace for decision:
```bash
iptables -A INPUT -j NFQUEUE --queue-num 0
```
Userspace program:
1. Opens netlink socket to queue
2. Receives packets via libnetfilter_queue
3. Inspects packet, decides NF_ACCEPT or NF_DROP
4. Sends verdict back to kernel

Use cases: Deep packet inspection, IDS, custom filtering.
[Advanced]

---

Q: (Cloze) NAT changes source addresses at _____ hook and destination addresses at _____ hook.
A: POSTROUTING (SNAT), PREROUTING (DNAT)
[Intermediate]

---

Q: (Understanding) Why does connection tracking need to happen before NAT?
A: Conntrack must see original addresses:
1. **PREROUTING**: Conntrack sees original dst, then DNAT changes it
2. **POSTROUTING**: SNAT changes src after conntrack recorded original

Reply packets:
1. Arrive with translated addresses
2. Conntrack finds entry, reverses NAT
3. Application sees original peer addresses

Conntrack priority is higher (earlier) than NAT.
[Intermediate]

---

Q: What are netfilter callback priorities?
A: Priority determines hook execution order (lower = earlier):
```c
enum nf_ip_hook_priorities {
    NF_IP_PRI_RAW            = -300,  // raw table
    NF_IP_PRI_CONNTRACK      = -200,  // connection tracking
    NF_IP_PRI_MANGLE         = -150,  // mangle table
    NF_IP_PRI_NAT_DST        = -100,  // DNAT
    NF_IP_PRI_FILTER         = 0,     // filter table
    NF_IP_PRI_NAT_SRC        = 100,   // SNAT
};
```
Negative priorities run before default (0).
[Intermediate]

---

## Section 11: Traffic Control

---

Q: What is the Linux Traffic Control (tc) subsystem?
A: Traffic Control manages packet queuing and scheduling:
- **Queuing disciplines (qdiscs)** - Control how packets are queued and dequeued
- **Classes** - Subdivisions for hierarchical qdiscs
- **Filters** - Classify packets into classes

Used for: Bandwidth limiting, prioritization, traffic shaping, latency control.

Configured via `tc` command. Code in `net/sched/`.
[Basic]

---

Q: What is a qdisc (queuing discipline)?
A: Qdisc defines the packet queuing/scheduling policy:
```c
struct Qdisc_ops {
    int  (*enqueue)(struct sk_buff *, struct Qdisc *);  // Add packet
    struct sk_buff *(*dequeue)(struct Qdisc *);         // Get next packet
    int  (*init)(struct Qdisc *, struct nlattr *arg);   // Initialize
    void (*reset)(struct Qdisc *);                      // Flush queue
    // ...
};
```
Each interface has at least one qdisc (root). `dev->qdisc` points to it.
[Intermediate]

---

Q: What is pfifo_fast and why is it the default?
A: `pfifo_fast` is a simple priority FIFO qdisc:
```
Band 0 (highest priority): TOS interactive (ssh, telnet)
Band 1 (normal):           Most traffic
Band 2 (lowest priority):  TOS bulk (ftp-data)
```
- Three bands, each FIFO
- Dequeue from lowest band number first
- Simple, zero-config, low overhead

Default for most interfaces.
[Intermediate]

---

Q: What are the main types of qdiscs?
A:
**Classless (simple):**
- `pfifo_fast` - Priority FIFO (default)
- `tbf` - Token Bucket Filter (rate limiting)
- `sfq` - Stochastic Fair Queuing
- `pfifo/bfifo` - Simple FIFO

**Classful (hierarchical):**
- `htb` - Hierarchical Token Bucket
- `cbq` - Class Based Queuing
- `prio` - Priority scheduler

Classful qdiscs contain classes with child qdiscs.
[Intermediate]

---

Q: What is HTB (Hierarchical Token Bucket)?
A: HTB provides bandwidth sharing with guarantees:
```
              root qdisc (htb)
                    │
        ┌───────────┼───────────┐
        ▼           ▼           ▼
   Class 1:1    Class 1:2    Class 1:3
   (10Mbit)     (20Mbit)     (30Mbit)
   guaranteed   guaranteed   guaranteed
```
Features:
- Rate limit (guaranteed bandwidth)
- Ceil (maximum bandwidth)
- Borrowing from parent when underutilized
- Priority between classes
[Intermediate]

---

Q: What is TBF (Token Bucket Filter)?
A: TBF implements simple rate limiting:
```
                ┌─────────────────────┐
Tokens fill at  │   Token Bucket      │
rate "rate" ───►│   (size = burst)    │
                │                     │
                └──────────┬──────────┘
                           │ Token per byte
                           ▼
    Packets ───► [Gate] ───► Output
                   ▲
                   │
             Pass if tokens available
             Else queue/drop
```
Parameters: `rate` (bytes/sec), `burst` (bucket size), `latency` (max wait).
[Intermediate]

---

Q: How does dev_queue_xmit() interact with qdiscs?
A:
```c
dev_queue_xmit(skb)
{
    txq = netdev_pick_tx(dev, skb);  // Select TX queue
    q = txq->qdisc;
    
    if (q->enqueue) {
        rc = q->enqueue(skb, q);     // Enqueue to qdisc
        __qdisc_run(q);              // Try to dequeue and send
    } else {
        // No qdisc, send directly
        dev->netdev_ops->ndo_start_xmit(skb, dev);
    }
}
```
Qdisc may delay, reorder, or drop packets.
[Intermediate]

---

Q: What is a tc filter and how is it used?
A: Filters classify packets into qdisc classes:
```bash
# Create HTB qdisc
tc qdisc add dev eth0 root handle 1: htb

# Create classes
tc class add dev eth0 parent 1: classid 1:1 htb rate 10mbit
tc class add dev eth0 parent 1: classid 1:2 htb rate 20mbit

# Filter: Port 80 traffic to class 1:1
tc filter add dev eth0 parent 1: protocol ip prio 1 \
    u32 match ip dport 80 0xffff flowid 1:1
```
Filter types: u32, fw (fwmark), flow, cgroup, bpf.
[Intermediate]

---

Q: (Cloze) Traffic control qdiscs are located at _____ and use the _____ function to send packets.
A: `dev->qdisc`, `ndo_start_xmit()`
[Intermediate]

---

Q: (Understanding) Why might you use ingress qdisc?
A: Ingress qdisc acts on incoming packets (before routing):
```bash
tc qdisc add dev eth0 ingress
tc filter add dev eth0 parent ffff: protocol ip \
    u32 match ip src 1.2.3.4 police rate 1mbit burst 10k drop
```
Use cases:
- Rate-limit incoming traffic per-source
- Drop unwanted traffic early
- Mark packets for later processing

Limited actions (can't queue, only police/drop).
[Intermediate]

---

## Section 12: Advanced Topics

---

Q: What is a network namespace and how is it created?
A: Network namespace provides isolated network stack:
```c
struct net {
    struct list_head    list;           // All namespaces
    struct net_device   *loopback_dev;  // Namespace's loopback
    struct hlist_head   *dev_name_head; // Devices by name
    struct proc_dir_entry *proc_net;    // /proc/net
    // ... routing tables, iptables, etc.
};
```
Create: `unshare(CLONE_NEWNET)` or `ip netns add myns`

Each namespace has own interfaces, routing, firewall rules.
[Intermediate]

---

Q: What is a veth (virtual ethernet) pair?
A: Veth creates two connected virtual interfaces:
```
┌─────────────────┐          ┌─────────────────┐
│  Namespace A    │          │  Namespace B    │
│                 │          │                 │
│     veth0  ◄────┼──────────┼────► veth1      │
│                 │          │                 │
└─────────────────┘          └─────────────────┘
```
Packets sent to veth0 appear on veth1 (and vice versa).

Used by containers to connect to host or bridges.
```bash
ip link add veth0 type veth peer name veth1
ip link set veth1 netns container_ns
```
[Intermediate]

---

Q: What is a Linux bridge and how does it work?
A: Bridge connects multiple interfaces at L2 (like a switch):
```
         ┌────────────────────────┐
         │       br0 (bridge)     │
         │   MAC learning table   │
         └──┬────────┬────────┬───┘
            │        │        │
          eth0    veth0    veth1
```
Functions:
- Learns MAC addresses from incoming frames
- Forwards frames to correct port (or floods if unknown)
- STP (Spanning Tree Protocol) for loop prevention

```bash
brctl addbr br0
brctl addif br0 eth0
```
[Intermediate]

---

Q: What is AF_PACKET and what is it used for?
A: AF_PACKET provides raw link-layer access:
```c
int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
```
Features:
- Receive all packets (not just for this host)
- Send raw Ethernet frames
- Used by tcpdump, Wireshark, dhclient

Packet types:
- `SOCK_RAW` - Full Ethernet frame including header
- `SOCK_DGRAM` - Frame without Ethernet header
[Intermediate]

---

Q: What is a socket filter (BPF)?
A: Socket filters allow userspace to filter packets in kernel:
```c
struct sock_filter code[] = {
    BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 12),    // Load ethertype
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ETH_P_IP, 0, 1),  // IP?
    BPF_STMT(BPF_RET | BPF_K, 65535),          // Accept
    BPF_STMT(BPF_RET | BPF_K, 0),              // Reject
};

struct sock_fprog filter = {
    .len = sizeof(code) / sizeof(code[0]),
    .filter = code,
};

setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &filter, sizeof(filter));
```
Runs in kernel, reduces copies to userspace. tcpdump uses this.
[Advanced]

---

Q: What is bonding and what modes does it support?
A: Bonding combines multiple NICs into one logical interface:

**Modes:**
- **Mode 0 (balance-rr)** - Round-robin load balancing
- **Mode 1 (active-backup)** - Failover, one active
- **Mode 2 (balance-xor)** - XOR of MAC for load balance
- **Mode 3 (broadcast)** - Send on all interfaces
- **Mode 4 (802.3ad)** - LACP aggregation
- **Mode 5 (balance-tlb)** - TX load balance
- **Mode 6 (balance-alb)** - TX and RX load balance

```bash
modprobe bonding mode=1
ip link add bond0 type bond
ip link set eth0 master bond0
```
[Intermediate]

---

Q: (Cloze) Container networking typically uses _____ pairs connected to a _____ for host communication.
A: veth, bridge
[Basic]

---

Q: (Understanding) Why do containers often use network namespaces with veth pairs?
A: This provides:
1. **Isolation** - Container has own IP, routes, firewall
2. **Connectivity** - Veth connects to host bridge
3. **Portability** - Same networking regardless of host
4. **Security** - Container can't see host network traffic

```
Container NS:                  Host NS:
┌────────────┐                ┌────────────┐
│ eth0       │◄──── veth ────►│ veth123    │
│ 172.17.0.2 │                │    │       │
└────────────┘                │    ▼       │
                              │ docker0    │
                              │ (bridge)   │
                              │    │       │
                              │ eth0 (NAT) │
                              └────────────┘
```
[Intermediate]

---

