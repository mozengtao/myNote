# Linux Kernel Network Subsystem Deep Dive (v3.2)

This document provides a comprehensive, code-level walkthrough of the Linux 3.2 Network subsystem. It covers the core networking infrastructure—from socket creation to packet transmission and reception—focusing on the fundamental mechanisms that all protocol families build upon.

---

## 1. Subsystem Context (Big Picture)

### What Is the Network Subsystem?

The **Linux Network Subsystem** is the kernel infrastructure that enables network communication. It provides:

1. **Socket Layer**: BSD-compatible API for user-space network programming
2. **Protocol Stack**: Implementation of network protocols (TCP/IP, UDP, etc.)
3. **Network Device Layer**: Abstraction for network interface cards (NICs)
4. **Packet Processing**: Efficient path for moving data between user space and hardware

### What Problem Does It Solve?

1. **Unified Network API**: Provides consistent `socket()` interface regardless of underlying protocol
2. **Protocol Abstraction**: Separates protocol logic from device drivers
3. **High-Performance I/O**: Handles millions of packets per second with NAPI and softirq
4. **Hardware Independence**: Same protocol code works with any NIC driver
5. **Namespace Isolation**: Network namespaces for containers and virtualization

### Where It Sits in the Kernel Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            USER SPACE                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  Application                                                         │    │
│  │    socket(AF_INET, SOCK_STREAM, 0)                                  │    │
│  │    connect(fd, &addr, len)                                          │    │
│  │    send(fd, buf, len, 0) / recv(fd, buf, len, 0)                    │    │
│  └───────────────────────────────┬─────────────────────────────────────┘    │
└──────────────────────────────────│──────────────────────────────────────────┘
                                   │ System Call
═══════════════════════════════════│══════════════════════════════════════════
                                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                            KERNEL SPACE                                      │
│                                                                              │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                    SOCKET LAYER (net/socket.c)                         │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐  │  │
│  │  │   struct socket                                                  │  │  │
│  │  │     └── proto_ops (connect, send, recv, bind, listen, accept)   │  │  │
│  │  └─────────────────────────────────────────────────────────────────┘  │  │
│  │                              │                                         │  │
│  │                              ▼                                         │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐  │  │
│  │  │   PROTOCOL FAMILY REGISTRATION                                   │  │  │
│  │  │     net_families[AF_INET] → inet_family_ops                     │  │  │
│  │  │     net_families[AF_UNIX] → unix_family_ops                     │  │  │
│  │  │     net_families[AF_PACKET] → packet_family_ops                 │  │  │
│  │  └─────────────────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                  │                                           │
│  ┌───────────────────────────────▼───────────────────────────────────────┐  │
│  │              TRANSPORT/NETWORK PROTOCOLS                               │  │
│  │  ┌────────────────┐  ┌────────────────┐  ┌────────────────────────┐   │  │
│  │  │   TCP (net/    │  │   UDP (net/    │  │   IP (net/ipv4/        │   │  │
│  │  │   ipv4/tcp.c)  │  │   ipv4/udp.c)  │  │   ip_input.c,          │   │  │
│  │  │                │  │                │  │   ip_output.c)         │   │  │
│  │  │  struct sock   │  │  struct sock   │  │                        │   │  │
│  │  └───────┬────────┘  └───────┬────────┘  └───────────┬────────────┘   │  │
│  │          │                   │                       │                 │  │
│  │          └───────────────────┴───────────────────────┘                 │  │
│  │                              │                                         │  │
│  │                              ▼                                         │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐  │  │
│  │  │                    struct sk_buff (skb)                          │  │  │
│  │  │    The universal packet container throughout the network stack   │  │  │
│  │  └─────────────────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                  │                                           │
│  ┌───────────────────────────────▼───────────────────────────────────────┐  │
│  │              NETWORK DEVICE LAYER (net/core/dev.c)                     │  │
│  │                                                                        │  │
│  │    TX Path                            RX Path                          │  │
│  │  ┌─────────────┐                   ┌─────────────┐                    │  │
│  │  │dev_queue_   │                   │ netif_rx()  │                    │  │
│  │  │  xmit()     │                   │ NAPI poll() │                    │  │
│  │  └──────┬──────┘                   └──────┬──────┘                    │  │
│  │         │                                 │                            │  │
│  │         ▼                                 ▼                            │  │
│  │  ┌──────────────────────────────────────────────────────────────┐     │  │
│  │  │               struct net_device                               │     │  │
│  │  │  ┌─────────────────────────────────────────────────────────┐ │     │  │
│  │  │  │  net_device_ops                                          │ │     │  │
│  │  │  │    .ndo_start_xmit = driver_xmit                        │ │     │  │
│  │  │  │    .ndo_open = driver_open                              │ │     │  │
│  │  │  │    .ndo_stop = driver_stop                              │ │     │  │
│  │  │  └─────────────────────────────────────────────────────────┘ │     │  │
│  │  └──────────────────────────────────────────────────────────────┘     │  │
│  │                                                                        │  │
│  │  ┌──────────────────────────────────────────────────────────────┐     │  │
│  │  │              SOFTIRQ Processing (NET_RX, NET_TX)              │     │  │
│  │  │  ┌──────────────────────────────────────────────────────┐    │     │  │
│  │  │  │   Per-CPU: struct softnet_data                        │    │     │  │
│  │  │  │     - poll_list (NAPI devices to poll)                │    │     │  │
│  │  │  │     - input_pkt_queue (backlog)                       │    │     │  │
│  │  │  │     - completion_queue (TX completions)               │    │     │  │
│  │  │  └──────────────────────────────────────────────────────┘    │     │  │
│  │  └──────────────────────────────────────────────────────────────┘     │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                  │                                           │
│  ┌───────────────────────────────▼───────────────────────────────────────┐  │
│  │                     DEVICE DRIVERS                                     │  │
│  │  ┌────────────────┐  ┌────────────────┐  ┌────────────────────────┐   │  │
│  │  │  e1000 driver  │  │  ixgbe driver  │  │  virtio_net driver     │   │  │
│  │  └────────────────┘  └────────────────┘  └────────────────────────┘   │  │
│  │                              │                                         │  │
│  │                              ▼                                         │  │
│  │                    ┌─────────────────┐                                │  │
│  │                    │    HARDWARE     │                                │  │
│  │                    │   (NIC/DMA)     │                                │  │
│  │                    └─────────────────┘                                │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### How This Subsystem Interacts with Others

| Adjacent Subsystem | Interaction |
|-------------------|-------------|
| **VFS** | Sockets are file descriptors; socket file operations (`socket_file_ops`) |
| **Memory Management** | `sk_buff` allocation, page-based I/O, DMA mapping |
| **Interrupt System** | Hardware IRQs trigger NAPI; softirqs process packets |
| **Scheduler** | Process blocking on socket I/O, wait queues |
| **Netfilter** | Hooks for firewall, NAT, connection tracking |
| **Traffic Control (tc)** | Queueing disciplines for traffic shaping |

---

## 2. Directory & File Map (Code Navigation)

### Primary Directories

```
net/
├── core/                       → Core networking infrastructure
│   ├── dev.c                   → Network device management
│   │                              - register/unregister_netdev()
│   │                              - dev_queue_xmit() (TX path entry)
│   │                              - netif_rx() / netif_receive_skb() (RX path)
│   │                              - NAPI implementation
│   │                              - softirq handlers (net_tx_action, net_rx_action)
│   │
│   ├── skbuff.c                → sk_buff management
│   │                              - alloc_skb(), kfree_skb()
│   │                              - skb_clone(), skb_copy()
│   │                              - skb_push(), skb_pull(), skb_reserve()
│   │
│   ├── sock.c                  → Generic socket support
│   │                              - sock_alloc(), sock_release()
│   │                              - sk_alloc(), sk_free()
│   │                              - Socket buffer management
│   │
│   ├── datagram.c              → Datagram socket helpers
│   ├── stream.c                → Stream socket helpers
│   ├── dst.c                   → Destination cache (routing cache)
│   ├── neighbour.c             → ARP/neighbor discovery
│   ├── rtnetlink.c             → Netlink for routing/device config
│   ├── filter.c                → BPF (Berkeley Packet Filter)
│   ├── ethtool.c               → Ethtool interface
│   └── net_namespace.c         → Network namespace support
│
├── socket.c                    → Socket system call interface
│                                  - sys_socket(), sys_bind(), sys_connect()
│                                  - sock_register() (protocol family registration)
│                                  - socket_file_ops (file operations for sockets)
│
├── ipv4/                       → IPv4 protocol implementation
│   ├── af_inet.c               → AF_INET socket creation
│   ├── ip_input.c              → IP receive path
│   ├── ip_output.c             → IP transmit path
│   ├── ip_forward.c            → IP forwarding
│   ├── tcp.c                   → TCP implementation
│   ├── tcp_input.c             → TCP receive state machine
│   ├── tcp_output.c            → TCP transmit
│   ├── udp.c                   → UDP implementation
│   ├── raw.c                   → Raw IP sockets
│   ├── route.c                 → IP routing
│   ├── fib_*.c                 → Forwarding Information Base
│   └── arp.c                   → ARP protocol
│
├── ipv6/                       → IPv6 protocol implementation
├── unix/                       → Unix domain sockets
├── packet/                     → Raw packet sockets (AF_PACKET)
├── netlink/                    → Netlink sockets
├── netfilter/                  → Netfilter framework
├── sched/                      → Traffic control (qdisc)
└── bridge/                     → Ethernet bridging

include/linux/
├── netdevice.h                 → struct net_device, net_device_ops
│                                  struct softnet_data, struct napi_struct
│
├── skbuff.h                    → struct sk_buff definition
│
├── net.h                       → struct socket, proto_ops
│
└── if.h                        → Interface flags and structures

include/net/
├── sock.h                      → struct sock, socket-level definitions
├── tcp.h                       → TCP-specific structures
├── ip.h                        → IP-specific structures
└── dst.h                       → Destination cache structures
```

### Why Is the Code Split This Way?

1. **`net/socket.c`**: User-space entry point; protocol-agnostic socket operations
2. **`net/core/dev.c`**: Device-agnostic packet processing; links protocols to drivers
3. **`net/core/skbuff.c`**: Universal packet buffer management used throughout
4. **`net/ipv4/`**: IPv4-specific code separate from generic infrastructure
5. **Protocol modularity**: Each protocol family (`unix/`, `packet/`, etc.) is independent

---

## 3. Core Data Structures

### 3.1 struct sk_buff — The Universal Packet Buffer

**Location**: `include/linux/skbuff.h`

```c
struct sk_buff {
    /* These two members must be first - for sk_buff_head compatibility */
    struct sk_buff      *next;
    struct sk_buff      *prev;

    ktime_t             tstamp;         /* Timestamp */
    
    struct sock         *sk;            /* Socket owning this skb */
    struct net_device   *dev;           /* Device we arrived on/are leaving by */

    /*
     * Control buffer - free for each layer to use
     * 48 bytes available for protocol-specific data
     */
    char                cb[48] __aligned(8);

    unsigned long       _skb_refdst;    /* Destination entry */
    
    unsigned int        len,            /* Length of actual data */
                        data_len;       /* Data length (for paged data) */
    __u16               mac_len,        /* Link layer header length */
                        hdr_len;        /* Writable header length of cloned skb */
    
    union {
        __wsum          csum;           /* Checksum */
        struct {
            __u16       csum_start;     /* Offset to start checksumming */
            __u16       csum_offset;    /* Offset to store checksum */
        };
    };
    
    __u32               priority;       /* Packet queueing priority */
    
    /* Bit fields */
    __u8                local_df:1,     /* Allow local fragmentation */
                        cloned:1,       /* Head may be cloned */
                        ip_summed:2,    /* Driver fed us an IP checksum */
                        nohdr:1,        /* Payload reference only */
                        nfctinfo:3;     /* Netfilter conntrack info */
    __u8                pkt_type:3,     /* Packet class (PACKET_HOST, etc.) */
                        fclone:2,       /* Clone status */
                        ipvs_property:1,
                        peeked:1,
                        nf_trace:1;
    
    __be16              protocol;       /* Packet protocol from driver */
    
    void                (*destructor)(struct sk_buff *skb);
    
    __u32               rxhash;         /* Receive hash for RPS */
    __u16               queue_mapping;  /* TX queue selection */
    __u16               vlan_tci;       /* VLAN tag control info */
    
    sk_buff_data_t      transport_header;  /* Transport layer header */
    sk_buff_data_t      network_header;    /* Network layer header */
    sk_buff_data_t      mac_header;        /* Link layer header */
    
    /* These elements must be at the end */
    sk_buff_data_t      tail;           /* Tail pointer */
    sk_buff_data_t      end;            /* End pointer */
    unsigned char       *head,          /* Head of buffer */
                        *data;          /* Data head pointer */
    unsigned int        truesize;       /* Buffer size */
    atomic_t            users;          /* Reference count */
};
```

**Memory Layout**:
```
       head                    data                    tail                end
        │                       │                       │                   │
        ▼                       ▼                       ▼                   ▼
        ┌───────────────────────┬───────────────────────┬───────────────────┐
        │     headroom          │      packet data      │    tailroom       │
        │  (for prepending      │   (len bytes)         │  (for appending   │
        │   headers)            │                       │   trailers)       │
        └───────────────────────┴───────────────────────┴───────────────────┘
        
                                │◄─── mac_header
                                    │◄─── network_header
                                        │◄─── transport_header
```

**Key Operations**:

| Function | Purpose |
|----------|---------|
| `alloc_skb(size, flags)` | Allocate new sk_buff with data buffer |
| `kfree_skb(skb)` | Decrement refcount, free if zero |
| `skb_reserve(skb, len)` | Reserve headroom before adding data |
| `skb_put(skb, len)` | Add data to tail, increase len |
| `skb_push(skb, len)` | Prepend data (decrease data pointer) |
| `skb_pull(skb, len)` | Remove data from head (increase data) |
| `skb_clone(skb, flags)` | Clone skb (shared data buffer) |
| `skb_copy(skb, flags)` | Full copy (new data buffer) |

**Lifetime**:
- **Allocation**: `alloc_skb()`, `netdev_alloc_skb()`, driver-specific
- **Reference counting**: `atomic_t users`; `skb_get()` increments, `kfree_skb()` decrements
- **Freeing**: When `users` reaches 0, data buffer and skb struct are freed

### 3.2 struct net_device — Network Interface

**Location**: `include/linux/netdevice.h`

```c
struct net_device {
    char                name[IFNAMSIZ];     /* Interface name: "eth0", "lo" */
    
    /* I/O specific fields */
    unsigned long       mem_end;            /* Shared memory end */
    unsigned long       mem_start;          /* Shared memory start */
    unsigned long       base_addr;          /* Device I/O address */
    unsigned int        irq;                /* Device IRQ number */
    
    unsigned long       state;              /* Device state flags */
    
    struct list_head    dev_list;           /* Global device list */
    struct list_head    napi_list;          /* NAPI instances for this device */
    
    /* Features - offload capabilities */
    u32                 features;           /* NETIF_F_SG, NETIF_F_IP_CSUM, etc. */
    u32                 hw_features;        /* User-changeable features */
    
    int                 ifindex;            /* Unique interface index */
    
    struct net_device_stats stats;          /* Statistics */
    
    /* Operations */
    const struct net_device_ops *netdev_ops;    /* Driver callbacks */
    const struct ethtool_ops *ethtool_ops;      /* Ethtool interface */
    const struct header_ops *header_ops;         /* Link layer header ops */
    
    unsigned int        flags;              /* IFF_UP, IFF_BROADCAST, etc. */
    unsigned int        mtu;                /* Maximum Transfer Unit */
    unsigned short      type;               /* Interface hardware type */
    unsigned short      hard_header_len;    /* Hardware header length */
    
    /* Address information */
    unsigned char       *dev_addr;          /* Hardware address */
    unsigned char       broadcast[MAX_ADDR_LEN]; /* Broadcast address */
    unsigned char       addr_len;           /* Hardware address length */
    
    /* Protocol pointers */
    struct in_device __rcu  *ip_ptr;        /* IPv4 specific data */
    struct inet6_dev __rcu  *ip6_ptr;       /* IPv6 specific data */
    
    /* RX/TX queues */
    struct netdev_rx_queue  *_rx;           /* RX queues */
    unsigned int        num_rx_queues;      /* Number of RX queues */
    struct netdev_queue *_tx;               /* TX queues */
    unsigned int        num_tx_queues;      /* Number of TX queues */
    
    /* Traffic control */
    struct Qdisc        *qdisc;             /* Root queueing discipline */
    unsigned long       tx_queue_len;       /* Max frames per queue */
    
    /* Reference counting */
    int __percpu        *pcpu_refcnt;       /* Per-CPU reference count */
    
    /* Registration state */
    enum {
        NETREG_UNINITIALIZED = 0,
        NETREG_REGISTERED,
        NETREG_UNREGISTERING,
        NETREG_UNREGISTERED,
        NETREG_RELEASED,
    } reg_state;
};
```

### 3.3 struct net_device_ops — Driver Interface

**Location**: `include/linux/netdevice.h`

```c
struct net_device_ops {
    int  (*ndo_init)(struct net_device *dev);
    void (*ndo_uninit)(struct net_device *dev);
    int  (*ndo_open)(struct net_device *dev);
    int  (*ndo_stop)(struct net_device *dev);
    
    /* Transmit packet - THE critical driver entry point */
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
                                   struct net_device *dev);
    
    /* TX queue selection for multi-queue devices */
    u16  (*ndo_select_queue)(struct net_device *dev, struct sk_buff *skb);
    
    int  (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int  (*ndo_validate_addr)(struct net_device *dev);
    int  (*ndo_do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);
    int  (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
    void (*ndo_tx_timeout)(struct net_device *dev);
    
    struct net_device_stats* (*ndo_get_stats)(struct net_device *dev);
    
    /* Receive mode changes */
    void (*ndo_set_rx_mode)(struct net_device *dev);
    
    /* VLAN support */
    void (*ndo_vlan_rx_add_vid)(struct net_device *dev, unsigned short vid);
    void (*ndo_vlan_rx_kill_vid)(struct net_device *dev, unsigned short vid);
    
    /* ... more operations ... */
};
```

### 3.4 struct socket — User-Visible Socket

**Location**: `include/linux/net.h`

```c
struct socket {
    socket_state        state;          /* SS_FREE, SS_CONNECTED, etc. */
    short               type;           /* SOCK_STREAM, SOCK_DGRAM, etc. */
    unsigned long       flags;          /* SOCK_ASYNC_NOSPACE, etc. */
    struct socket_wq __rcu *wq;         /* Wait queue for blocking ops */
    struct file         *file;          /* Associated file descriptor */
    struct sock         *sk;            /* Internal network-layer socket */
    const struct proto_ops *ops;        /* Protocol-specific operations */
};
```

### 3.5 struct sock — Internal Socket Representation

**Location**: `include/net/sock.h`

```c
struct sock {
    struct sock_common  __sk_common;    /* Common fields */
    
    /* Receive queue */
    struct sk_buff_head sk_receive_queue;
    
    /* Backlog queue - packets received while socket is locked */
    struct {
        atomic_t        rmem_alloc;
        int             len;
        struct sk_buff  *head;
        struct sk_buff  *tail;
    } sk_backlog;
    
    int                 sk_forward_alloc;   /* Space allocated for future use */
    int                 sk_rcvbuf;          /* Receive buffer size limit */
    int                 sk_sndbuf;          /* Send buffer size limit */
    
    struct sk_filter __rcu *sk_filter;      /* Socket BPF filter */
    struct socket_wq __rcu *sk_wq;          /* Waitqueue */
    
    /* Send queue */
    struct sk_buff_head sk_write_queue;
    atomic_t            sk_wmem_alloc;      /* TX buffer bytes allocated */
    
    /* Protocol info */
    unsigned int        sk_shutdown  : 2,
                        sk_no_check  : 2,
                        sk_protocol  : 8,
                        sk_type      : 16;
    
    struct dst_entry    *sk_dst_cache;      /* Cached route */
    struct proto        *sk_prot;           /* Protocol callbacks */
    struct proto        *sk_prot_creator;   /* Creator's protocol */
    
    /* Callbacks for state changes */
    void (*sk_state_change)(struct sock *sk);
    void (*sk_data_ready)(struct sock *sk, int bytes);
    void (*sk_write_space)(struct sock *sk);
    void (*sk_error_report)(struct sock *sk);
    
    /* ... many more fields ... */
};
```

### 3.6 struct softnet_data — Per-CPU Packet Processing State

**Location**: `include/linux/netdevice.h`

```c
struct softnet_data {
    struct Qdisc        *output_queue;      /* TX qdiscs needing service */
    struct Qdisc        **output_queue_tailp;
    struct list_head    poll_list;          /* NAPI devices to poll */
    struct sk_buff      *completion_queue;  /* TX skbs to free */
    struct sk_buff_head process_queue;      /* RX packets being processed */
    
    /* Stats */
    unsigned int        processed;          /* Packets processed */
    unsigned int        time_squeeze;       /* Ran out of time */
    unsigned int        cpu_collision;      /* TX lock collisions */
    unsigned int        received_rps;       /* RPS packets received */
    
#ifdef CONFIG_RPS
    struct softnet_data *rps_ipi_list;      /* Remote CPUs to notify */
    struct call_single_data csd ____cacheline_aligned_in_smp;
    unsigned int        cpu;
    unsigned int        input_queue_head;
    unsigned int        input_queue_tail;
#endif
    unsigned            dropped;            /* Dropped packets */
    struct sk_buff_head input_pkt_queue;    /* Backlog queue */
    struct napi_struct  backlog;            /* Backlog NAPI instance */
};
```

### 3.7 struct napi_struct — NAPI Polling Instance

**Location**: `include/linux/netdevice.h`

```c
struct napi_struct {
    struct list_head    poll_list;      /* Link in softnet_data.poll_list */
    unsigned long       state;          /* NAPI_STATE_SCHED, etc. */
    int                 weight;         /* Max packets to process per poll */
    int                 (*poll)(struct napi_struct *, int);  /* Poll function */
    
    unsigned int        gro_count;      /* GRO packets held */
    struct net_device   *dev;           /* Associated device */
    struct list_head    dev_list;       /* Link in net_device.napi_list */
    struct sk_buff      *gro_list;      /* GRO packet chain */
    struct sk_buff      *skb;           /* Current GRO skb */
};

enum {
    NAPI_STATE_SCHED,       /* Poll is scheduled */
    NAPI_STATE_DISABLE,     /* Disable pending */
    NAPI_STATE_NPSVC,       /* Netpoll - don't dequeue */
};
```

---

## 4. Entry Points & Call Paths

### 4.1 Socket Creation Path

```
User: socket(AF_INET, SOCK_STREAM, 0)
    │
    ▼
sys_socket(family, type, protocol)                [net/socket.c]
    │
    ▼
sock_create(family, type, protocol, &sock)
    │
    ├── sock_alloc()
    │       └── Allocate struct socket from socket_cache (slab)
    │       └── inode = new_inode(sock_mnt->mnt_sb)
    │
    ├── net_families[family] lookup ◄── RCU protected
    │       │
    │       ▼
    │   For AF_INET: inet_family_ops
    │
    └── pf->create(net, sock, protocol, kern)
            │
            ▼
        inet_create()                             [net/ipv4/af_inet.c]
            │
            ├── Look up protocol in inetsw[sock->type]
            │       For SOCK_STREAM: tcp_prot
            │       For SOCK_DGRAM: udp_prot
            │
            ├── sk = sk_alloc(net, PF_INET, GFP_KERNEL, answer_prot)
            │       └── Allocate struct sock (or protocol-specific variant)
            │
            ├── sock_init_data(sock, sk)
            │       └── Initialize queues, callbacks
            │
            ├── sk->sk_prot->init(sk)
            │       │
            │       ▼  For TCP:
            │       tcp_v4_init_sock()
            │           └── Initialize TCP state machine
            │
            └── sock->ops = answer->ops
                    └── For TCP: inet_stream_ops
                    └── For UDP: inet_dgram_ops
    │
    ▼
sock_map_fd(sock, flags)
    │
    ├── get_unused_fd_flags()
    ├── sock_alloc_file(sock, flags, ...)
    │       └── file->f_op = &socket_file_ops
    └── fd_install(fd, file)
    │
    ▼
Return: file descriptor (int)
```

### 4.2 Transmit Path (Send)

```
User: send(fd, buf, len, flags)
    │
    ▼
sys_sendto(fd, buff, len, flags, addr, addr_len)    [net/socket.c]
    │
    ▼
sock_sendmsg(sock, &msg, len)
    │
    ▼
__sock_sendmsg(&iocb, sock, &msg, len)
    │
    └── sock->ops->sendmsg(iocb, sock, msg, len)
            │
            ▼  For TCP:
            tcp_sendmsg()                            [net/ipv4/tcp.c]
                │
                ├── Copy user data to sk_buff's in sk_write_queue
                │
                ├── tcp_push()
                │       │
                │       ▼
                │   tcp_write_xmit()
                │       │
                │       ▼
                │   tcp_transmit_skb()              [net/ipv4/tcp_output.c]
                │       │
                │       ├── Build TCP header
                │       ├── Calculate checksum
                │       │
                │       ▼
                │   ip_queue_xmit()                 [net/ipv4/ip_output.c]
                │       │
                │       ├── Route lookup (ip_route_output_flow)
                │       ├── Build IP header
                │       │
                │       ▼
                │   ip_local_out()
                │       │
                │       ├── NF_INET_LOCAL_OUT (netfilter)
                │       │
                │       ▼
                │   dst_output()
                │       │
                │       ▼
                │   ip_output()
                │       │
                │       ├── NF_INET_POST_ROUTING (netfilter)
                │       │
                │       ▼
                │   ip_finish_output()
                │       │
                │       ├── IP fragmentation if needed
                │       │
                │       ▼
                │   ip_finish_output2()
                │       │
                │       ├── Neighbor lookup (ARP)
                │       │
                │       ▼
                │   neigh_output()
                │       │
                │       ▼
                │   dev_queue_xmit()                [net/core/dev.c]
                            │
                            ▼
    ┌───────────────────────────────────────────────────────────────────┐
    │                     NETWORK DEVICE LAYER                           │
    │                                                                    │
    │   dev_queue_xmit(skb)                                             │
    │       │                                                            │
    │       ├── dev_pick_tx(dev, skb) → select TX queue                 │
    │       │                                                            │
    │       ├── if (qdisc->enqueue):                                    │
    │       │       __dev_xmit_skb(skb, q, dev, txq)                    │
    │       │           └── Queue to qdisc, trigger softirq             │
    │       │                                                            │
    │       └── else (no qdisc):                                        │
    │               dev_hard_start_xmit(skb, dev, txq)                  │
    │                   │                                                │
    │                   ▼                                                │
    │               ops->ndo_start_xmit(skb, dev)  ◄── DRIVER CALL     │
    │                   │                                                │
    │                   ▼                                                │
    │               [Driver transmits to hardware]                       │
    └───────────────────────────────────────────────────────────────────┘
```

### 4.3 Receive Path (NAPI)

```
    ┌───────────────────────────────────────────────────────────────────┐
    │                        HARDWARE INTERRUPT                          │
    │   Driver IRQ handler:                                              │
    │       e1000_intr()                                                │
    │           │                                                        │
    │           ├── napi_schedule(&adapter->napi)                       │
    │           │       │                                                │
    │           │       ├── test_and_set_bit(NAPI_STATE_SCHED, ...)     │
    │           │       │                                                │
    │           │       └── ____napi_schedule(sd, napi)                 │
    │           │               ├── list_add_tail(&napi->poll_list,     │
    │           │               │                 &sd->poll_list)       │
    │           │               └── __raise_softirq_irqoff(NET_RX_SOFTIRQ)
    │           │                                                        │
    │           └── Disable device interrupts                           │
    └───────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼ (softirq context)
    ┌───────────────────────────────────────────────────────────────────┐
    │                     NET_RX_SOFTIRQ Handler                         │
    │                                                                    │
    │   net_rx_action(h)                              [net/core/dev.c]  │
    │       │                                                            │
    │       ├── sd = &__get_cpu_var(softnet_data)                       │
    │       │                                                            │
    │       └── while (!list_empty(&sd->poll_list) && budget > 0):      │
    │               │                                                    │
    │               ├── napi = list_first_entry(&sd->poll_list, ...)    │
    │               │                                                    │
    │               └── work = napi->poll(napi, weight)                 │
    │                       │                                            │
    │                       ▼  For e1000:                                │
    │                   e1000_clean()                                    │
    │                       │                                            │
    │                       ├── Read packets from ring buffer           │
    │                       │                                            │
    │                       └── for each packet:                        │
    │                               │                                    │
    │                               ├── skb = netdev_alloc_skb(...)     │
    │                               ├── Copy/DMA data to skb            │
    │                               │                                    │
    │                               └── napi_gro_receive(napi, skb)     │
    │                                       │                            │
    │                                       ▼                            │
    │                                   netif_receive_skb(skb)          │
    └───────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
    ┌───────────────────────────────────────────────────────────────────┐
    │               PROTOCOL DEMULTIPLEXING                              │
    │                                                                    │
    │   netif_receive_skb(skb)                                          │
    │       │                                                            │
    │       └── __netif_receive_skb(skb)                                │
    │               │                                                    │
    │               ├── skb_reset_network_header(skb)                   │
    │               │                                                    │
    │               ├── Deliver to packet_type handlers (tcpdump)       │
    │               │       list_for_each_entry_rcu(ptype, &ptype_all)  │
    │               │                                                    │
    │               ├── RX handler (for bridging, bonding)              │
    │               │       rx_handler = rcu_dereference(skb->dev->rx_handler)
    │               │                                                    │
    │               └── type = skb->protocol                            │
    │                   ptype = ptype_head[hash(type)]                  │
    │                   ptype->func(skb, dev, ptype, orig_dev)          │
    │                       │                                            │
    │                       ▼  For ETH_P_IP:                            │
    │                   ip_rcv()                    [net/ipv4/ip_input.c]
    │                       │                                            │
    │                       ├── Validate IP header                      │
    │                       ├── NF_INET_PRE_ROUTING (netfilter)         │
    │                       │                                            │
    │                       ▼                                            │
    │                   ip_rcv_finish()                                  │
    │                       │                                            │
    │                       ├── Route lookup (ip_route_input_noref)     │
    │                       │                                            │
    │                       ▼  For local delivery:                      │
    │                   ip_local_deliver()                               │
    │                       │                                            │
    │                       ├── IP reassembly if fragmented             │
    │                       ├── NF_INET_LOCAL_IN (netfilter)            │
    │                       │                                            │
    │                       ▼                                            │
    │                   ip_local_deliver_finish()                        │
    │                       │                                            │
    │                       ├── protocol = ip_hdr(skb)->protocol        │
    │                       │                                            │
    │                       └── ipprot->handler(skb)                    │
    │                               │                                    │
    │                               ▼  For TCP (protocol=6):            │
    │                           tcp_v4_rcv()       [net/ipv4/tcp_ipv4.c]│
    │                               │                                    │
    │                               ├── Lookup socket (sk)              │
    │                               │                                    │
    │                               ├── tcp_v4_do_rcv(sk, skb)          │
    │                               │       │                            │
    │                               │       ▼                            │
    │                               │   tcp_rcv_state_machine()         │
    │                               │       └── Process TCP state       │
    │                               │                                    │
    │                               └── sock_queue_rcv_skb(sk, skb)     │
    │                                       │                            │
    │                                       └── skb_queue_tail(         │
    │                                              &sk->sk_receive_queue,│
    │                                              skb)                  │
    │                                                                    │
    │                                       └── sk->sk_data_ready(sk)   │
    │                                               └── Wake up reader  │
    └───────────────────────────────────────────────────────────────────┘
```

### 4.4 Non-NAPI Legacy Path (netif_rx)

```
Driver IRQ (legacy):
    │
    └── netif_rx(skb)                                [net/core/dev.c]
            │
            ├── if (netpoll_rx(skb)) return       ← Netpoll intercept
            │
            ├── enqueue_to_backlog(skb, cpu, &qtail)
            │       │
            │       ├── sd = &per_cpu(softnet_data, cpu)
            │       │
            │       ├── if (queue_len <= netdev_max_backlog):
            │       │       __skb_queue_tail(&sd->input_pkt_queue, skb)
            │       │       ____napi_schedule(sd, &sd->backlog)
            │       │
            │       └── else: kfree_skb(skb)  ← Drop
            │
            └── return NET_RX_SUCCESS or NET_RX_DROP
```

---

## 5. Core Workflows (Code-Driven)

### 5.1 Network Device Registration

```c
// Typical driver initialization (e.g., e1000)
static int __devinit e1000_probe(struct pci_dev *pdev, ...)
{
    struct net_device *netdev;
    
    // 1. Allocate net_device with private data
    netdev = alloc_etherdev(sizeof(struct e1000_adapter));
    
    // 2. Set up device operations
    netdev->netdev_ops = &e1000_netdev_ops;
    netdev->ethtool_ops = &e1000_ethtool_ops;
    
    // 3. Initialize hardware
    // ... PCI setup, DMA allocation ...
    
    // 4. Set hardware address
    memcpy(netdev->dev_addr, hw->mac.addr, netdev->addr_len);
    
    // 5. Register with network stack
    err = register_netdev(netdev);
    
    return 0;
}
```

**register_netdev() internals**:

```c
int register_netdev(struct net_device *dev)
{
    int err;
    
    rtnl_lock();
    err = register_netdevice(dev);  // Does the real work
    rtnl_unlock();
    
    return err;
}

int register_netdevice(struct net_device *dev)
{
    // 1. Assign interface index
    dev->ifindex = dev_new_index(net);
    
    // 2. Initialize per-CPU reference count
    dev->pcpu_refcnt = alloc_percpu(int);
    
    // 3. Initialize TX queues
    netdev_init_queues(dev);
    
    // 4. Add to device lists
    list_netdevice(dev);
    
    // 5. Notify subsystems
    call_netdevice_notifiers(NETDEV_REGISTER, dev);
    
    dev->reg_state = NETREG_REGISTERED;
    return 0;
}
```

### 5.2 Device Open (ifconfig up)

```c
// net/core/dev.c
static int __dev_open(struct net_device *dev)
{
    const struct net_device_ops *ops = dev->netdev_ops;
    int ret;
    
    // 1. Call driver's open function
    if (ops->ndo_open)
        ret = ops->ndo_open(dev);
    
    // 2. Set device state
    set_bit(__LINK_STATE_START, &dev->state);
    
    // 3. Start TX queues
    dev_activate(dev);
    
    // 4. Add to poll list for watchdog
    dev_watchdog_up(dev);
    
    return ret;
}

// Driver's open (e1000 example)
static int e1000_open(struct net_device *netdev)
{
    struct e1000_adapter *adapter = netdev_priv(netdev);
    
    // 1. Allocate TX/RX ring buffers
    e1000_setup_all_tx_resources(adapter);
    e1000_setup_all_rx_resources(adapter);
    
    // 2. Configure hardware
    e1000_configure(adapter);
    
    // 3. Request IRQ
    err = request_irq(adapter->pdev->irq, e1000_intr, ...);
    
    // 4. Initialize NAPI
    napi_enable(&adapter->napi);
    
    // 5. Enable interrupts
    e1000_irq_enable(adapter);
    
    // 6. Start carrier detection
    netif_carrier_off(netdev);
    
    return 0;
}
```

### 5.3 Fast Path: Small UDP Send

```c
// Simplified UDP send path for small packets
ssize_t udp_sendmsg(struct kiocb *iocb, struct sock *sk,
                    struct msghdr *msg, size_t len)
{
    struct inet_sock *inet = inet_sk(sk);
    struct sk_buff *skb;
    
    // 1. Allocate sk_buff
    skb = sock_alloc_send_skb(sk, len + headroom, ...);
    
    // 2. Reserve headroom for headers
    skb_reserve(skb, headroom);
    
    // 3. Copy user data
    err = memcpy_fromiovec(skb_put(skb, len), msg->msg_iov, len);
    
    // 4. Build UDP header
    uh = udp_hdr(skb);
    uh->source = inet->inet_sport;
    uh->dest = dport;
    uh->len = htons(len + sizeof(struct udphdr));
    uh->check = 0;
    
    // 5. Send to IP layer
    err = ip_push_pending_frames(sk);
    
    return len;
}
```

### 5.4 Slow Path: TCP Connection Establishment

```
    Client                                    Server
       │                                         │
       │  connect(fd, &addr, len)               │
       │         │                               │
       │         ▼                               │
       │  tcp_v4_connect()                       │
       │         │                               │
       │         ├── Route lookup                │
       │         ├── Allocate local port         │
       │         ├── tcp_connect()               │
       │         │       │                       │
       │         │       ├── Send SYN ──────────►│ tcp_v4_rcv()
       │         │       │                       │      │
       │         │       └── TCP_SYN_SENT        │      ├── tcp_v4_do_rcv()
       │         │                               │      │
       │         │                               │      ├── tcp_rcv_state_machine()
       │         │                               │      │       │
       │         │                               │      │       └── tcp_v4_conn_request()
       │         │                               │      │               │
       │         │ tcp_v4_rcv()◄─────────────────│──────│───────────────┤ Send SYN+ACK
       │         │      │                        │      │               │
       │         │      ├── tcp_rcv_synsent_     │      │               │
       │         │      │   state_process()      │      └── Create request_sock
       │         │      │       │                │
       │         │      │       ├── tcp_ack()    │
       │         │      │       │                │
       │         │      │       └── TCP_ESTABLISHED
       │         │      │               │        │
       │         │      │               ├── Send ACK ───────────────────►│
       │         │      │               │        │                       │
       │         │      │               │        │    tcp_v4_rcv()       │
       │         │      │               │        │         │             │
       │         │      │               │        │         └── tcp_v4_hnd_req()
       │         │      │               │        │                  │
       │         │      │               │        │                  └── TCP_ESTABLISHED
       │         │      │               │        │
       │         └── Wake up connect()  │        │
       │                                         │
       └─────────────────────────────────────────┘
```

### 5.5 Error Handling: TX Queue Full

```c
// net/core/dev.c
static inline int __dev_xmit_skb(struct sk_buff *skb, struct Qdisc *q,
                                  struct net_device *dev,
                                  struct netdev_queue *txq)
{
    spinlock_t *root_lock = qdisc_lock(q);
    
    spin_lock(root_lock);
    
    if (unlikely(test_bit(__QDISC_STATE_DEACTIVATED, &q->state))) {
        __qdisc_drop(skb, &to_free);
        rc = NET_XMIT_DROP;
    } else if (...) {
        // Queue packet to qdisc
        rc = q->enqueue(skb, q);
        
        if (rc == NET_XMIT_SUCCESS) {
            // Success - trigger TX softirq if needed
            if (q->flags & TCQ_F_CAN_BYPASS)
                qdisc_run(q);
        }
    }
    
    spin_unlock(root_lock);
    return rc;
}

// When driver queue is full
netdev_tx_t driver_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    if (tx_ring_full) {
        // Stop queue to prevent more packets
        netif_stop_queue(dev);
        
        // Return BUSY - packet will be requeued
        return NETDEV_TX_BUSY;
    }
    
    // ... transmit packet ...
    
    return NETDEV_TX_OK;
}

// Driver TX completion (interrupt or NAPI)
void driver_tx_complete(...)
{
    // Free transmitted skbs
    while (completed) {
        dev_kfree_skb_any(skb);
        completed--;
    }
    
    // Wake queue if it was stopped
    if (netif_queue_stopped(netdev) && space_available)
        netif_wake_queue(netdev);
}
```

---

## 6. Important Algorithms & Mechanisms

### 6.1 NAPI (New API) Polling

NAPI combines interrupt-driven and polling-based packet reception for high performance:

```
┌─────────────────────────────────────────────────────────────────────┐
│                        NAPI State Machine                            │
│                                                                      │
│   ┌──────────────┐                                                   │
│   │    IDLE      │◄───────────────────────────────────────┐         │
│   │   (no work)  │                                        │         │
│   └──────┬───────┘                                        │         │
│          │                                                │         │
│          │ Packet arrives (IRQ)                           │         │
│          │                                                │         │
│          ▼                                                │         │
│   ┌──────────────┐                                        │         │
│   │  SCHEDULED   │ napi_schedule()                        │         │
│   │  (in poll    │    - Disable IRQ                       │         │
│   │   list)      │    - Add to softnet_data.poll_list     │         │
│   └──────┬───────┘    - Raise NET_RX_SOFTIRQ              │         │
│          │                                                │         │
│          │ softirq runs                                   │         │
│          ▼                                                │         │
│   ┌──────────────┐                                        │         │
│   │   POLLING    │ napi->poll() called                    │         │
│   │  (processing │    - Process up to 'weight' packets    │         │
│   │   packets)   │    - Each packet: netif_receive_skb()  │         │
│   └──────┬───────┘                                        │         │
│          │                                                │         │
│          ├── work < weight: napi_complete() ──────────────┘         │
│          │       - Re-enable IRQ                                    │
│          │       - Remove from poll_list                            │
│          │                                                          │
│          └── work == weight: Stay scheduled                         │
│                  - Continue polling next softirq run                │
│                  - Prevents IRQ storm                               │
└─────────────────────────────────────────────────────────────────────┘
```

**Why NAPI?**
- **IRQ coalescing**: Instead of one IRQ per packet, batch processing
- **Adaptive**: Under low load, works like traditional IRQ; under high load, pure polling
- **Prevents livelock**: Limits work per softirq invocation

### 6.2 Receive Packet Steering (RPS)

RPS distributes RX processing across CPUs for single-queue NICs:

```c
// net/core/dev.c
static int get_rps_cpu(struct net_device *dev, struct sk_buff *skb,
                       struct rps_dev_flow **rflowp)
{
    struct rps_map *map;
    u32 hash;
    int cpu = -1;
    
    // Get flow hash (based on IP addresses and ports)
    hash = skb_get_rxhash(skb);
    
    // Look up CPU from RPS map
    map = rcu_dereference(rxqueue->rps_map);
    if (map) {
        cpu = map->cpus[((u64)hash * map->len) >> 32];
    }
    
    return cpu;
}
```

### 6.3 Generic Receive Offload (GRO)

GRO coalesces similar packets before passing them up the stack:

```
   Individual packets                      Coalesced packet
┌───────────────────────────┐           ┌───────────────────────────┐
│ TCP segment 1 (1500B)     │           │                           │
├───────────────────────────┤           │  Single large packet      │
│ TCP segment 2 (1500B)     │   ───►    │  (up to 64KB)             │
├───────────────────────────┤           │                           │
│ TCP segment 3 (1500B)     │           │  Same TCP connection      │
├───────────────────────────┤           │  Sequential data          │
│ TCP segment 4 (1500B)     │           │                           │
└───────────────────────────┘           └───────────────────────────┘

Benefits:
- Reduce per-packet overhead
- Fewer function calls
- Better cache utilization
- Works with any NIC (software GRO)
```

### 6.4 Traffic Control (qdisc)

Queueing disciplines control packet scheduling:

```
           dev_queue_xmit(skb)
                  │
                  ▼
         ┌───────────────────┐
         │   Root Qdisc      │ (e.g., pfifo_fast)
         │   (attached to    │
         │    net_device)    │
         └────────┬──────────┘
                  │
                  ▼
         ┌───────────────────┐
         │   q->enqueue()    │  Add packet to queue
         │                   │
         │   q->dequeue()    │  Get next packet to send
         │                   │
         │   q->reset()      │  Clear queue
         └────────┬──────────┘
                  │
                  ▼
         ndo_start_xmit(skb, dev)
```

**pfifo_fast** (default qdisc):
- 3 bands (priorities)
- Band 0 = highest priority (interactive)
- Band 2 = lowest priority (bulk)
- FIFO within each band

### 6.5 Route Caching and Destination Cache

```c
// Destination entry structure
struct dst_entry {
    struct rcu_head         rcu_head;
    struct dst_entry        *child;
    struct net_device       *dev;
    
    int                     (*input)(struct sk_buff *);   /* ip_local_deliver */
    int                     (*output)(struct sk_buff *);  /* ip_output */
    
    unsigned long           expires;
    unsigned long           lastuse;
    
    struct dst_ops          *ops;
    /* ... */
};

// Route cache lookup (simplified)
struct rtable *ip_route_output_flow(...)
{
    // 1. Check route cache
    rth = rt_hash_table[hash];
    if (rth && matches)
        return rth;
    
    // 2. FIB lookup
    fib_lookup(net, &fl4, &res);
    
    // 3. Create new route entry
    rth = rt_dst_alloc(...);
    
    // 4. Cache it
    rt_hash_insert(...);
    
    return rth;
}
```

---

## 7. Concurrency & Synchronization

### 7.1 Locking Hierarchy

| Lock | Type | Protects | Scope |
|------|------|----------|-------|
| `rtnl_lock` | Mutex | Device registration, configuration | Global |
| `dev_base_lock` | RW lock | Device list traversal | Per-namespace |
| `sk_lock` | Socket lock | Socket state | Per-socket |
| `qdisc_lock()` | Spinlock | TX qdisc | Per-queue |
| `softnet_data.input_pkt_queue.lock` | Spinlock | RX backlog | Per-CPU |

### 7.2 Socket Locking

```c
// Two-level locking for sockets
struct socket_lock_t {
    spinlock_t      slock;          /* Fast path lock */
    int             owned;          /* Is socket locked by user? */
    wait_queue_head_t wq;           /* Wait for lock */
};

// Fast path: spin_lock(&sk->sk_lock.slock)
// User context: lock_sock(sk) - may sleep
// BH context: bh_lock_sock(sk) - spins

void lock_sock(struct sock *sk)
{
    might_sleep();
    spin_lock_bh(&sk->sk_lock.slock);
    if (sk->sk_lock.owned)
        __lock_sock(sk);  /* Wait */
    sk->sk_lock.owned = 1;
    spin_unlock(&sk->sk_lock.slock);
}
```

### 7.3 RCU Usage

```c
// Protocol family lookup (RCU read)
static struct socket *sock_create(int family, int type, int protocol)
{
    const struct net_proto_family *pf;
    
    rcu_read_lock();
    pf = rcu_dereference(net_families[family]);
    if (!pf || !try_module_get(pf->owner)) {
        rcu_read_unlock();
        return -EAFNOSUPPORT;
    }
    rcu_read_unlock();
    
    // Use pf...
}

// Protocol family registration (RCU write)
int sock_register(const struct net_proto_family *ops)
{
    spin_lock(&net_family_lock);
    RCU_INIT_POINTER(net_families[ops->family], ops);
    spin_unlock(&net_family_lock);
    
    return 0;
}

void sock_unregister(int family)
{
    spin_lock(&net_family_lock);
    RCU_INIT_POINTER(net_families[family], NULL);
    spin_unlock(&net_family_lock);
    
    synchronize_rcu();  /* Wait for readers */
}
```

### 7.4 Softirq vs. User Context

```
                    ┌─────────────────────────────────────────────┐
                    │              NET_RX_SOFTIRQ                  │
                    │                                              │
    User Context    │    Cannot sleep                              │
    ─────────────   │    Cannot take sleeping locks                │
    Can sleep       │    Uses spin_lock_bh() for socket lock       │
    Full lock       │    RCU read-side OK                          │
    access          │    Must be fast (< 2 jiffies budget)         │
                    │                                              │
                    │    Processing: netif_receive_skb()           │
                    │                protocol handlers             │
                    │                                              │
                    └─────────────────────────────────────────────┘
```

### 7.5 What Breaks Without Proper Synchronization

1. **Missing `rtnl_lock`**: Corrupt device list, use-after-free on unregistration
2. **Missing socket lock**: Corrupted socket state, data races on queues
3. **Missing RCU**: Protocol handler unregistered while in use
4. **Missing preempt_disable in NAPI**: Per-CPU data corruption

---

## 8. Performance Considerations

### 8.1 Hot Paths

| Path | Frequency | Critical Operations |
|------|-----------|---------------------|
| `netif_receive_skb()` | Every RX packet | Protocol lookup, per-CPU data |
| `dev_queue_xmit()` | Every TX packet | Queue selection, qdisc enqueue |
| `tcp_rcv_established()` | TCP data segments | Sequence check, ACK processing |
| NAPI poll | High rate | Ring buffer access, sk_buff allocation |

### 8.2 Per-CPU Data

```c
// Per-CPU softnet_data avoids cache bouncing
DEFINE_PER_CPU_ALIGNED(struct softnet_data, softnet_data);

// Per-CPU packet counters
__this_cpu_inc(softnet_data.processed);

// Per-CPU TX queue selection
cpu = smp_processor_id();
txq = netdev_get_tx_queue(dev, cpu % dev->real_num_tx_queues);
```

### 8.3 Cacheline Optimization in sk_buff

```c
struct sk_buff {
    /* First cacheline - most accessed fields */
    struct sk_buff      *next;          /* Queue linkage */
    struct sk_buff      *prev;
    ktime_t             tstamp;
    struct sock         *sk;
    struct net_device   *dev;
    char                cb[48];         /* Protocol scratch area */
    
    /* Second cacheline */
    unsigned long       _skb_refdst;
    /* ... */
} ____cacheline_aligned_in_smp;
```

### 8.4 Zero-Copy Techniques

```c
// sendfile() path - avoids user-space copies
sys_sendfile()
    → do_sendfile()
        → do_splice_direct()
            → splice_direct_to_actor()
                → Page reference passed to socket
                → DMA from page cache to NIC

// MSG_ZEROCOPY (newer kernels)
// User buffer pinned, DMA'd directly
```

### 8.5 Scalability Limits in v3.2

1. **Single RX queue NICs**: One CPU handles all RX (mitigated by RPS)
2. **Route cache size**: Memory pressure under high connection rates
3. **netfilter conntrack**: Lock contention with many connections
4. **Socket buffer limits**: Per-socket limits, not system-wide adaptive

---

## 9. Common Pitfalls & Bugs

### 9.1 sk_buff Manipulation Errors

```c
// BUG: Accessing data after skb_pull
skb_pull(skb, hdr_len);
old_header = (struct my_header *)skb->data - hdr_len;  // WRONG!

// CORRECT: Save pointer before pull
old_header = (struct my_header *)skb->data;
skb_pull(skb, hdr_len);
```

### 9.2 Reference Counting Mistakes

```c
// BUG: Double free
kfree_skb(skb);
kfree_skb(skb);  // Use-after-free!

// BUG: Forgetting to free after clone
skb2 = skb_clone(skb, GFP_ATOMIC);
// ... error path without kfree_skb(skb2) ...

// CORRECT: Always match get/put
skb_get(skb);
// ... use skb ...
kfree_skb(skb);
```

### 9.3 Locking Errors

```c
// BUG: Taking socket lock in softirq with spin_lock
// (should use spin_lock_bh in user context)
void user_function(struct sock *sk)
{
    spin_lock(&sk->sk_lock.slock);  // WRONG - softirq can deadlock
    // ... 
}

// CORRECT:
void user_function(struct sock *sk)
{
    lock_sock(sk);  // Handles BH correctly
    // ...
    release_sock(sk);
}
```

### 9.4 Network Byte Order

```c
// BUG: Forgetting byte order conversion
ip_hdr(skb)->daddr = 0x0a000001;  // Should be htonl()

// CORRECT:
ip_hdr(skb)->daddr = htonl(0x0a000001);  // 10.0.0.1
```

### 9.5 Missing netif_tx_stop_queue

```c
// BUG: Driver continues accepting packets when queue full
netdev_tx_t my_xmit(struct sk_buff *skb, struct net_device *dev)
{
    if (tx_ring_full) {
        // Missing: netif_stop_queue(dev);
        return NETDEV_TX_BUSY;  // Upper layer will retry infinitely
    }
}
```

### 9.6 Historical Issues in v3.2

1. **RCU grace period stalls**: Fixed in later kernels with better tracing
2. **TCP small queue**: Not present in 3.2 (added in 3.6)
3. **No busy polling**: Added in 3.11 for latency-sensitive workloads
4. **Limited multi-queue support**: Improved significantly after 3.2

---

## 10. How to Read This Code Yourself

### 10.1 Recommended Reading Order

1. **`include/linux/skbuff.h`**: Start with `struct sk_buff` - understand the universal packet buffer

2. **`net/core/skbuff.c`**: Read `alloc_skb()`, `kfree_skb()`, manipulation functions

3. **`include/linux/netdevice.h`**: `struct net_device`, `struct napi_struct`, `struct softnet_data`

4. **`net/socket.c`**: `sys_socket()`, `sock_create()`, `sock_sendmsg()` - user-space entry points

5. **`net/core/dev.c`** in this order:
   - `register_netdevice()` - device registration
   - `dev_queue_xmit()` - TX entry point
   - `netif_rx()` / `netif_receive_skb()` - RX entry points
   - `net_rx_action()` - softirq handler

6. **`net/ipv4/ip_input.c`**: `ip_rcv()`, `ip_local_deliver()` - IP RX path

7. **`net/ipv4/ip_output.c`**: `ip_queue_xmit()`, `ip_output()` - IP TX path

8. **`net/ipv4/tcp.c`**: `tcp_sendmsg()`, TCP socket operations

### 10.2 What to Ignore Initially

- **IPv6** (`net/ipv6/`): Study after mastering IPv4
- **Netfilter hooks**: Complex; study separately
- **Traffic control details**: `net/sched/` is extensive
- **SCTP, DCCP**: Specialized protocols
- **Network namespaces**: Study after basic flow

### 10.3 Useful Search Commands

```bash
# Find all packet type registrations
grep -r "dev_add_pack\|register_packet_handler" net/

# Find protocol handlers
grep -r "\.handler.*=" net/ipv4/*.c

# Find all NAPI poll functions
grep -r "\.poll.*=.*[a-z_]*_poll" drivers/net/

# Find socket operation implementations
grep -r "struct proto_ops.*=" net/

# Trace function calls
cscope -d
# Search for: netif_receive_skb
```

### 10.4 Debugging Tips

**Packet tracing**:
```bash
# Trace all packets (requires ftrace)
echo 1 > /sys/kernel/debug/tracing/events/net/enable
cat /sys/kernel/debug/tracing/trace_pipe

# Trace specific function
echo netif_receive_skb > /sys/kernel/debug/tracing/set_ftrace_filter
echo function > /sys/kernel/debug/tracing/current_tracer
```

**Network statistics**:
```bash
# Per-CPU softnet stats
cat /proc/net/softnet_stat

# Interface statistics
cat /proc/net/dev
ip -s link show eth0

# Socket statistics
ss -s
cat /proc/net/sockstat
```

**Protocol debugging**:
```bash
# TCP state
cat /proc/net/tcp
ss -tan state established

# Route cache
ip route show cache

# Neighbor table
ip neigh show
```

---

## 11. Summary & Mental Model

### One-Paragraph Summary

The Linux network subsystem is organized as a layered pipeline: user-space system calls (`socket()`, `send()`, `recv()`) enter through `net/socket.c`, which dispatches to protocol families (AF_INET, etc.) via registered handlers. Protocols build packets in `sk_buff` structures, routing determines the outgoing interface, and `dev_queue_xmit()` hands packets to the queueing discipline and then the device driver's `ndo_start_xmit()`. In the receive direction, NAPI-enabled drivers poll packets in softirq context, calling `napi_gro_receive()` / `netif_receive_skb()` to demultiplex by protocol (IP, ARP, etc.) and eventually deliver to sockets. Throughout, per-CPU `softnet_data` structures minimize lock contention, and RCU protects rapidly-read data like protocol handlers.

### Key Invariants

1. **sk_buff ownership**: Only one entity owns an skb at a time; cloning creates shared data with separate headers

2. **Socket lock hierarchy**: Never hold socket lock when calling into network device layer

3. **NAPI scheduling**: A device is either processing packets OR waiting for interrupts, never both

4. **Reference counting**: `dev_hold()`/`dev_put()` for net_device; `skb_get()`/`kfree_skb()` for sk_buff

5. **Softirq budget**: NET_RX_SOFTIRQ processes limited packets per invocation to maintain responsiveness

### Mental Model

Think of the network stack as a **packet factory with conveyor belts**:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        PACKET FACTORY                                    │
│                                                                          │
│   SHIPPING (TX)                      RECEIVING (RX)                      │
│   ─────────────                      ──────────────                      │
│                                                                          │
│   User writes                        Hardware delivers                   │
│       │                                  │                               │
│       ▼                                  ▼                               │
│   ┌─────────┐                       ┌─────────┐                          │
│   │ Socket  │                       │  NAPI   │   Conveyor belt          │
│   │  Layer  │                       │  Poll   │   (softirq)              │
│   └────┬────┘                       └────┬────┘                          │
│        │                                 │                               │
│        ▼                                 ▼                               │
│   ┌─────────┐                       ┌─────────┐                          │
│   │   TCP   │   Assembly line       │   IP    │   Sorting station        │
│   │   UDP   │   (add headers)       │  Layer  │   (route lookup)         │
│   └────┬────┘                       └────┬────┘                          │
│        │                                 │                               │
│        ▼                                 ▼                               │
│   ┌─────────┐                       ┌─────────┐                          │
│   │   IP    │                       │   TCP   │   Quality control        │
│   │  Layer  │                       │   UDP   │   (checksums)            │
│   └────┬────┘                       └────┬────┘                          │
│        │                                 │                               │
│        ▼                                 ▼                               │
│   ┌─────────┐                       ┌─────────┐                          │
│   │ Device  │   Outbound dock       │ Socket  │   Customer pickup        │
│   │  Queue  │                       │  Queue  │   (read/recv)            │
│   └────┬────┘                       └─────────┘                          │
│        │                                                                 │
│        ▼                                                                 │
│   ┌─────────┐                                                           │
│   │   NIC   │   Truck (DMA)                                             │
│   └─────────┘                                                           │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 12. What to Study Next

### Recommended Learning Order

| Order | Subsystem | Why It Matters |
|-------|-----------|----------------|
| 1 | **TCP Protocol** (`net/ipv4/tcp*.c`) | Most complex protocol; congestion control, state machine |
| 2 | **Netfilter** (`net/netfilter/`) | Firewall, NAT, connection tracking |
| 3 | **Traffic Control** (`net/sched/`) | QoS, bandwidth management |
| 4 | **Unix Domain Sockets** (`net/unix/`) | Simpler protocol, local IPC |
| 5 | **Network Namespaces** | Container networking foundation |
| 6 | **Bridge/VLAN** (`net/bridge/`) | Layer 2 networking |
| 7 | **Wireless (mac80211)** | 802.11 stack |

### Relevant Files for Further Study

**TCP Deep Dive**:
- `net/ipv4/tcp_input.c` — Receive state machine
- `net/ipv4/tcp_output.c` — Transmit logic
- `net/ipv4/tcp_congestion.c` — Congestion control

**Netfilter**:
- `net/netfilter/core.c` — Hook infrastructure
- `net/netfilter/nf_conntrack_core.c` — Connection tracking
- `net/ipv4/netfilter/iptable_filter.c` — iptables

**Driver Development**:
- `drivers/net/ethernet/intel/e1000e/` — Well-documented Intel driver
- `drivers/net/virtio_net.c` — Virtio networking (VMs)

---

## Appendix A: Key Functions Quick Reference

### Socket Layer
```c
sys_socket()      → Create socket
sys_bind()        → Bind to address
sys_listen()      → Mark as listening
sys_accept()      → Accept connection
sys_connect()     → Initiate connection
sys_sendto()      → Send data
sys_recvfrom()    → Receive data
sock_register()   → Register protocol family
```

### Network Device Layer
```c
register_netdev()       → Register network device
unregister_netdev()     → Unregister network device
dev_queue_xmit()        → Transmit packet
netif_rx()              → Legacy receive (interrupt context)
netif_receive_skb()     → Modern receive (NAPI context)
napi_schedule()         → Schedule NAPI poll
napi_complete()         → Complete NAPI poll cycle
netif_start_queue()     → Allow TX
netif_stop_queue()      → Stop TX (queue full)
netif_wake_queue()      → Resume TX
```

### sk_buff Management
```c
alloc_skb()         → Allocate sk_buff
kfree_skb()         → Free sk_buff
skb_clone()         → Clone (shared data)
skb_copy()          → Full copy
skb_reserve()       → Reserve headroom
skb_put()           → Add to tail
skb_push()          → Prepend
skb_pull()          → Remove from head
skb_headroom()      → Available headroom
skb_tailroom()      → Available tailroom
```

---

*Document generated for Linux kernel v3.2. Some details may differ in other versions.*

