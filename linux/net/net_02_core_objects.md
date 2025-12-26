# Linux Networking Subsystem: Core Networking Objects and Boundaries

## 1. Object Relationship Overview

```
+------------------------------------------------------------------+
|  CORE NETWORKING OBJECT RELATIONSHIPS                            |
+------------------------------------------------------------------+

    ┌─────────────────────────────────────────────────────────────┐
    │                    APPLICATION LAYER                        │
    │                                                             │
    │    User Process                                             │
    │         │                                                   │
    │    file descriptor (int fd)                                 │
    │         │                                                   │
    └─────────┼───────────────────────────────────────────────────┘
              │
    ┌─────────▼───────────────────────────────────────────────────┐
    │                    VFS LAYER                                │
    │                                                             │
    │    struct file                                              │
    │         │                                                   │
    │         └──► struct socket  ◄── BSD socket abstraction      │
    │                   │                                         │
    │                   ├──► proto_ops  (protocol operations)     │
    │                   │                                         │
    │                   └──► struct sock  ◄── protocol socket     │
    │                             │                               │
    │                             ├──► struct proto (TCP/UDP)     │
    │                             │                               │
    │                             └──► sk_buff queues             │
    │                                                             │
    └─────────────────────────────────────────────────────────────┘
              │
    ┌─────────▼───────────────────────────────────────────────────┐
    │                    NETWORK LAYER                            │
    │                                                             │
    │    struct sk_buff  ◄── THE packet buffer                    │
    │         │                                                   │
    │         ├──► packet data (headers + payload)                │
    │         │                                                   │
    │         └──► metadata (protocol info, routing, ...)         │
    │                                                             │
    └─────────────────────────────────────────────────────────────┘
              │
    ┌─────────▼───────────────────────────────────────────────────┐
    │                    DEVICE LAYER                             │
    │                                                             │
    │    struct net_device  ◄── network interface                 │
    │         │                                                   │
    │         ├──► net_device_ops  (driver operations)            │
    │         │                                                   │
    │         └──► queues, statistics, hardware config            │
    │                                                             │
    └─────────────────────────────────────────────────────────────┘
              │
    ┌─────────▼───────────────────────────────────────────────────┐
    │                    HARDWARE                                 │
    │    NIC / WiFi / Virtual Device                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. struct sk_buff (Socket Buffer)

```
+------------------------------------------------------------------+
|  struct sk_buff - THE PACKET BUFFER                              |
+------------------------------------------------------------------+

    REAL-WORLD CONCEPT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Represents a single network packet as it travels through   │
    │  the entire networking stack. Contains both:                │
    │    • The actual packet data (headers + payload)             │
    │    • Metadata about the packet (source, destination, etc.)  │
    │                                                             │
    │  Think of it as an "envelope" that carries a "letter"       │
    │  (payload) with "stamps and labels" (headers/metadata)      │
    └─────────────────────────────────────────────────────────────┘

    OWNERSHIP:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Created by: driver (RX) or protocol layer (TX)           │
    │  • Passed between layers via pointer transfer               │
    │  • Single owner at any time (clear handoff)                 │
    │  • Freed by: final consumer (driver TX or socket RX)        │
    │                                                             │
    │  Ownership transfer is EXPLICIT:                            │
    │    • Pass skb pointer = transfer ownership                  │
    │    • Return skb pointer = return ownership                  │
    │    • kfree_skb() = give up ownership and free               │
    └─────────────────────────────────────────────────────────────┘

    LIFETIME MANAGEMENT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Reference counting via skb->users:                         │
    │                                                             │
    │  alloc_skb()          → users = 1                           │
    │  skb_get(skb)         → users++  (share reference)          │
    │  kfree_skb(skb)       → users--; if 0, free                 │
    │  consume_skb(skb)     → users--; if 0, free (success path)  │
    │                                                             │
    │  Data sharing via skb_shared_info->dataref:                 │
    │  skb_clone(skb)       → new skb, shared data, dataref++     │
    │  pskb_copy(skb)       → new skb, copied headers, shared pg  │
    │  skb_copy(skb)        → full deep copy                      │
    └─────────────────────────────────────────────────────────────┘

    KEY STRUCTURE (simplified):
    ┌─────────────────────────────────────────────────────────────┐
    │  struct sk_buff {                                           │
    │      struct sk_buff *next, *prev;  /* Queue linkage */      │
    │      struct sock *sk;               /* Owning socket */     │
    │      struct net_device *dev;        /* Device */            │
    │                                                             │
    │      char cb[48];                   /* Control buffer */    │
    │                                      /* Per-layer private */│
    │                                                             │
    │      unsigned int len;              /* Total data length */ │
    │      unsigned int data_len;         /* Paged data length */ │
    │                                                             │
    │      __be16 protocol;               /* L3 protocol type */  │
    │      __u8 pkt_type;                 /* Packet class */      │
    │                                                             │
    │      sk_buff_data_t transport_header;  /* L4 header */      │
    │      sk_buff_data_t network_header;    /* L3 header */      │
    │      sk_buff_data_t mac_header;        /* L2 header */      │
    │                                                             │
    │      unsigned char *head;           /* Buffer start */      │
    │      unsigned char *data;           /* Data start */        │
    │      sk_buff_data_t tail;           /* Data end */          │
    │      sk_buff_data_t end;            /* Buffer end */        │
    │                                                             │
    │      atomic_t users;                /* Reference count */   │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘

    MEMORY LAYOUT:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                             │
    │    head ──► ┌───────────────────────────┐                   │
    │             │        HEADROOM           │ ◄── Reserved for  │
    │             │    (for prepending)       │     lower headers │
    │    data ──► ├───────────────────────────┤                   │
    │             │    ┌─────────────────┐    │                   │
    │             │    │   MAC header    │    │                   │
    │             │    ├─────────────────┤    │                   │
    │             │    │   IP header     │    │                   │
    │             │    ├─────────────────┤    │                   │
    │             │    │   TCP header    │    │                   │
    │             │    ├─────────────────┤    │                   │
    │             │    │     PAYLOAD     │    │                   │
    │             │    └─────────────────┘    │                   │
    │    tail ──► ├───────────────────────────┤                   │
    │             │        TAILROOM           │ ◄── Reserved for  │
    │             │    (for appending)        │     trailers      │
    │     end ──► ├───────────────────────────┤                   │
    │             │   skb_shared_info         │ ◄── Frags, etc.   │
    │             └───────────────────────────┘                   │
    │                                                             │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. struct net_device (Network Device)

```
+------------------------------------------------------------------+
|  struct net_device - NETWORK INTERFACE ABSTRACTION               |
+------------------------------------------------------------------+

    REAL-WORLD CONCEPT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Represents a network interface (eth0, wlan0, lo, etc.)     │
    │    • Physical NIC (Ethernet, WiFi)                          │
    │    • Virtual device (tun/tap, bridge, veth)                 │
    │    • Loopback (lo)                                          │
    │                                                             │
    │  Provides uniform interface for:                            │
    │    • Transmitting packets                                   │
    │    • Receiving packets                                      │
    │    • Configuration (MTU, MAC address, flags)                │
    │    • Statistics                                             │
    └─────────────────────────────────────────────────────────────┘

    OWNERSHIP:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Created by: driver via alloc_netdev()                    │
    │  • Registered with: register_netdev()                       │
    │  • Owned by: kernel networking core                         │
    │  • Reference counted: dev_hold() / dev_put()                │
    │  • Freed by: free_netdev() after unregister                 │
    │                                                             │
    │  Namespace-aware: belongs to struct net                     │
    └─────────────────────────────────────────────────────────────┘

    LIFETIME MANAGEMENT:
    ┌─────────────────────────────────────────────────────────────┐
    │  alloc_netdev()    → Allocate device structure              │
    │  register_netdev() → Make visible to stack                  │
    │  dev_hold()        → Increment reference                    │
    │  dev_put()         → Decrement reference                    │
    │  unregister_netdev() → Remove from stack                    │
    │  free_netdev()     → Free memory                            │
    │                                                             │
    │  RCU protection for device lookup (dev_get_by_name, etc.)   │
    └─────────────────────────────────────────────────────────────┘

    KEY STRUCTURE (simplified):
    ┌─────────────────────────────────────────────────────────────┐
    │  struct net_device {                                        │
    │      char name[IFNAMSIZ];          /* "eth0", "lo", ... */  │
    │                                                             │
    │      /* I/O specific */                                     │
    │      unsigned long mem_start, mem_end;                      │
    │      unsigned long base_addr;                               │
    │      unsigned int irq;                                      │
    │                                                             │
    │      unsigned long state;           /* Device state */      │
    │      struct list_head dev_list;     /* Global list */       │
    │                                                             │
    │      /* Operations */                                       │
    │      const struct net_device_ops *netdev_ops;               │
    │      const struct ethtool_ops *ethtool_ops;                 │
    │                                                             │
    │      /* Hardware info */                                    │
    │      unsigned int flags;            /* IFF_UP, etc. */      │
    │      unsigned int mtu;              /* Interface MTU */     │
    │      unsigned short type;           /* ARPHRD_ETHER, ... */ │
    │      unsigned char dev_addr[MAX_ADDR_LEN]; /* MAC addr */   │
    │                                                             │
    │      /* Transmit queue */                                   │
    │      struct netdev_queue *_tx;                              │
    │      unsigned int num_tx_queues;                            │
    │                                                             │
    │      /* Receive queue */                                    │
    │      struct netdev_queue __rcu *ingress_queue;              │
    │                                                             │
    │      /* Statistics */                                       │
    │      struct net_device_stats stats;                         │
    │                                                             │
    │      atomic_t refcnt;               /* Reference count */   │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘

    NET_DEVICE_OPS (Driver polymorphism):
    ┌─────────────────────────────────────────────────────────────┐
    │  struct net_device_ops {                                   │
    │      int (*ndo_init)(struct net_device *dev);              │
    │      void (*ndo_uninit)(struct net_device *dev);           │
    │      int (*ndo_open)(struct net_device *dev);              │
    │      int (*ndo_stop)(struct net_device *dev);              │
    │      netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,    │
    │                                    struct net_device *dev);│
    │      void (*ndo_set_rx_mode)(struct net_device *dev);      │
    │      int (*ndo_set_mac_address)(struct net_device *dev,    │
    │                                 void *addr);                │
    │      int (*ndo_change_mtu)(struct net_device *dev,         │
    │                            int new_mtu);                    │
    │      /* ... many more */                                   │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. struct socket (BSD Socket)

```
+------------------------------------------------------------------+
|  struct socket - BSD SOCKET ABSTRACTION                          |
+------------------------------------------------------------------+

    REAL-WORLD CONCEPT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Represents the BSD socket API view:                       │
    │    • What userspace sees via file descriptor               │
    │    • Protocol-independent wrapper                          │
    │    • Thin layer between VFS and protocol                   │
    │                                                              │
    │  Key insight: struct socket is the "presentation layer"    │
    │  that wraps the protocol-specific struct sock              │
    └─────────────────────────────────────────────────────────────┘

    OWNERSHIP:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Created by: sys_socket() → sock_create()                │
    │  • Tied to: struct file via file->private_data             │
    │  • Contains: pointer to struct sock (protocol socket)      │
    │  • Freed by: sock_release() when file closed               │
    │                                                              │
    │  Lifetime tied to file descriptor                          │
    └─────────────────────────────────────────────────────────────┘

    KEY STRUCTURE:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct socket {                                           │
    │      socket_state state;           /* SS_CONNECTED, etc. */│
    │      short type;                   /* SOCK_STREAM, etc. */ │
    │      unsigned long flags;          /* Async flags */       │
    │                                                              │
    │      struct socket_wq *wq;         /* Wait queue */        │
    │      struct file *file;            /* Back pointer to VFS */│
    │      struct sock *sk;              /* Protocol socket */   │
    │      const struct proto_ops *ops;  /* Protocol operations */│
    │  };                                                         │
    │                                                              │
    │  SOCKET STATES:                                            │
    │  ┌───────────────────────────────────────────────────────┐ │
    │  │  SS_FREE          – not allocated                     │ │
    │  │  SS_UNCONNECTED   – unconnected                       │ │
    │  │  SS_CONNECTING    – connecting in progress            │ │
    │  │  SS_CONNECTED     – connected                         │ │
    │  │  SS_DISCONNECTING – disconnecting in progress         │ │
    │  └───────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────┘

    PROTO_OPS (Socket-level polymorphism):
    ┌─────────────────────────────────────────────────────────────┐
    │  struct proto_ops {                                        │
    │      int family;                                           │
    │      struct module *owner;                                 │
    │                                                              │
    │      int (*release)(struct socket *sock);                  │
    │      int (*bind)(struct socket *sock, struct sockaddr *,   │
    │                  int sockaddr_len);                         │
    │      int (*connect)(struct socket *sock, struct sockaddr *,│
    │                     int sockaddr_len, int flags);           │
    │      int (*accept)(struct socket *sock,                    │
    │                    struct socket *newsock, int flags);      │
    │      int (*listen)(struct socket *sock, int len);          │
    │      int (*sendmsg)(struct kiocb *iocb, struct socket *,   │
    │                     struct msghdr *m, size_t total_len);    │
    │      int (*recvmsg)(struct kiocb *iocb, struct socket *,   │
    │                     struct msghdr *m, size_t total_len,     │
    │                     int flags);                             │
    │      /* ... */                                             │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘
```

---

## 5. struct sock (Protocol Socket)

```
+------------------------------------------------------------------+
|  struct sock - PROTOCOL SOCKET (Network Layer Representation)    |
+------------------------------------------------------------------+

    REAL-WORLD CONCEPT:
    ┌─────────────────────────────────────────────────────────────┐
    │  The REAL network socket - contains:                       │
    │    • Protocol state (TCP sequence numbers, etc.)           │
    │    • Receive/send queues (sk_buff lists)                   │
    │    • Socket options and configuration                      │
    │    • Addressing information                                │
    │                                                              │
    │  struct socket is the "face" (BSD API)                     │
    │  struct sock is the "brain" (protocol implementation)      │
    └─────────────────────────────────────────────────────────────┘

    OWNERSHIP:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Created by: protocol's create function                  │
    │  • Pointed to by: struct socket->sk                        │
    │  • Reference counted: sk_refcnt (atomic)                   │
    │  • Freed by: sk_free() when refcnt → 0                     │
    │                                                              │
    │  Can outlive struct socket! (TIME_WAIT, orphan sockets)    │
    └─────────────────────────────────────────────────────────────┘

    LIFETIME MANAGEMENT:
    ┌─────────────────────────────────────────────────────────────┐
    │  sock_hold(sk)     → Increment reference                   │
    │  sock_put(sk)      → Decrement reference                   │
    │  sk_free(sk)       → Free when refcnt == 0                 │
    │                                                              │
    │  IMPORTANT: struct sock can exist after socket close!      │
    │  Example: TCP TIME_WAIT state keeps sock alive             │
    │           but struct socket is already freed               │
    └─────────────────────────────────────────────────────────────┘

    KEY STRUCTURE (simplified):
    ┌─────────────────────────────────────────────────────────────┐
    │  struct sock {                                             │
    │      struct sock_common __sk_common;  /* Shared header */  │
    │      #define sk_refcnt   __sk_common.skc_refcnt            │
    │      #define sk_family   __sk_common.skc_family            │
    │      #define sk_state    __sk_common.skc_state             │
    │      #define sk_prot     __sk_common.skc_prot              │
    │                                                              │
    │      socket_lock_t sk_lock;           /* Per-socket lock */│
    │      struct sk_buff_head sk_receive_queue;  /* RX queue */ │
    │      struct sk_buff_head sk_write_queue;    /* TX queue */ │
    │                                                              │
    │      int sk_rcvbuf;                   /* RX buffer size */ │
    │      int sk_sndbuf;                   /* TX buffer size */ │
    │                                                              │
    │      struct socket *sk_socket;        /* Back to socket */ │
    │      struct proto *sk_prot;           /* Protocol ops */   │
    │                                                              │
    │      /* Callbacks */                                       │
    │      void (*sk_state_change)(struct sock *sk);             │
    │      void (*sk_data_ready)(struct sock *sk, int bytes);    │
    │      void (*sk_write_space)(struct sock *sk);              │
    │      void (*sk_error_report)(struct sock *sk);             │
    │      void (*sk_destruct)(struct sock *sk);                 │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘

    STRUCT PROTO (Protocol-specific operations):
    ┌─────────────────────────────────────────────────────────────┐
    │  struct proto {                                            │
    │      char name[32];                  /* "TCP", "UDP", ... */│
    │                                                              │
    │      void (*close)(struct sock *sk, long timeout);         │
    │      int (*connect)(struct sock *sk, struct sockaddr *,    │
    │                     int addr_len);                          │
    │      int (*disconnect)(struct sock *sk, int flags);        │
    │      int (*sendmsg)(struct kiocb *iocb, struct sock *sk,   │
    │                     struct msghdr *msg, size_t len);        │
    │      int (*recvmsg)(struct kiocb *iocb, struct sock *sk,   │
    │                     struct msghdr *msg, size_t len,         │
    │                     int noblock, int flags, int *addr_len);│
    │      int (*bind)(struct sock *sk, struct sockaddr *,       │
    │                  int addr_len);                             │
    │      int (*hash)(struct sock *sk);                         │
    │      void (*unhash)(struct sock *sk);                      │
    │      /* ... */                                             │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘
```

---

## 6. Reference Counting, Cloning, and Ownership Transfer

```
+------------------------------------------------------------------+
|  OWNERSHIP AND REFERENCE COUNTING PATTERNS                       |
+------------------------------------------------------------------+

    SK_BUFF OWNERSHIP:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  RECEIVE PATH (ownership transfer):                        │
    │                                                              │
    │    NIC Driver                                               │
    │        │ alloc_skb() → owner                               │
    │        │                                                    │
    │        ▼ netif_receive_skb(skb) → transfer ownership       │
    │    Network Core                                             │
    │        │                                                    │
    │        ▼ ip_rcv(skb) → transfer ownership                  │
    │    IP Layer                                                 │
    │        │                                                    │
    │        ▼ tcp_v4_rcv(skb) → transfer ownership              │
    │    TCP Layer                                                │
    │        │                                                    │
    │        ▼ skb_queue_tail(&sk->sk_receive_queue, skb)        │
    │    Socket Queue (owns skb until recvmsg consumes)          │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    SK_BUFF CLONING (shared data):
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    Original skb                     Clone skb               │
    │    ┌─────────────┐                  ┌─────────────┐        │
    │    │ sk_buff     │                  │ sk_buff     │        │
    │    │ users = 1   │                  │ users = 1   │        │
    │    │ cloned = 1  │                  │ cloned = 1  │        │
    │    └──────┬──────┘                  └──────┬──────┘        │
    │           │                                │                │
    │           └────────────┬───────────────────┘                │
    │                        │                                    │
    │                        ▼                                    │
    │                  ┌───────────────┐                         │
    │                  │ Shared Data   │                         │
    │                  │ dataref = 2   │ ◄── Both reference      │
    │                  └───────────────┘                         │
    │                                                              │
    │    skb_clone() creates new sk_buff metadata                │
    │    but shares the actual packet data                       │
    │    Efficient for: broadcast, multicast, tcpdump            │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    NET_DEVICE REFERENCE COUNTING:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    dev_hold(dev)   → Increment refcnt, safe to use dev    │
    │    dev_put(dev)    → Decrement refcnt                      │
    │                                                              │
    │    RCU for lockless lookup:                                │
    │    ┌───────────────────────────────────────────────────────┐│
    │    │  rcu_read_lock();                                     ││
    │    │  dev = dev_get_by_name_rcu(net, "eth0");             ││
    │    │  if (dev) {                                           ││
    │    │      /* dev is valid within RCU read-side */         ││
    │    │      /* but may be unregistering */                  ││
    │    │  }                                                     ││
    │    │  rcu_read_unlock();                                   ││
    │    └───────────────────────────────────────────────────────┘│
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    SOCK REFERENCE COUNTING:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    sock_hold(sk)   → Increment sk_refcnt                   │
    │    sock_put(sk)    → Decrement; if 0, free                 │
    │                                                              │
    │    IMPORTANT: sock can outlive socket!                     │
    │                                                              │
    │    socket close() path:                                    │
    │    ┌───────────────────────────────────────────────────────┐│
    │    │  sock_release(socket)                                 ││
    │    │      → ops->release(socket)  /* e.g., inet_release */││
    │    │      → sock_orphan(sk)       /* sk->sk_socket = NULL */│
    │    │      → sock_put(sk)          /* decrement refcnt */  ││
    │    │                                                        ││
    │    │  But sk may stay alive if:                            ││
    │    │    • TCP TIME_WAIT                                    ││
    │    │    • Pending packets in queues                        ││
    │    │    • Other references (e.g., timer)                   ││
    │    └───────────────────────────────────────────────────────┘│
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 7. Why These Objects Are Separated

```
+------------------------------------------------------------------+
|  SEPARATION OF CONCERNS                                          |
+------------------------------------------------------------------+

    socket vs sock:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    struct socket               struct sock                  │
    │    ┌─────────────────────┐     ┌─────────────────────────┐  │
    │    │ BSD API abstraction │     │ Protocol implementation │  │
    │    │ • VFS integration   │     │ • TCP state machine     │  │
    │    │ • File operations   │     │ • Sequence numbers      │  │
    │    │ • Wait queues       │     │ • Congestion control    │  │
    │    │ • State (connected) │     │ • Retransmission        │  │
    │    └─────────────────────┘     │ • Buffer management     │  │
    │                                 └─────────────────────────┘  │
    │                                                              │
    │    WHY SEPARATE:                                            │
    │    • sock can outlive socket (TIME_WAIT)                   │
    │    • Multiple sockets can share sock (fork + shared fd)    │
    │    • sock is protocol-specific (polymorphism)              │
    │    • socket is protocol-agnostic (uniform API)             │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    sk_buff vs net_device:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    struct sk_buff              struct net_device            │
    │    ┌─────────────────────┐     ┌─────────────────────────┐  │
    │    │ Packet (transient)  │     │ Interface (persistent)  │  │
    │    │ • Packet data       │     │ • Hardware config       │  │
    │    │ • Per-packet meta   │     │ • Driver interface      │  │
    │    │ • Layer headers     │     │ • Queue management      │  │
    │    │ • Short-lived       │     │ • Long-lived            │  │
    │    └─────────────────────┘     └─────────────────────────┘  │
    │                                                              │
    │    WHY SEPARATE:                                            │
    │    • Millions of skbs, few net_devices                     │
    │    • skb is per-packet allocation                          │
    │    • net_device is system-wide resource                    │
    │    • Different lifetimes, different ownership              │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  CORE NETWORKING OBJECTS SUMMARY                                 |
+------------------------------------------------------------------+

    OBJECT          │ REPRESENTS           │ OWNER      │ LIFETIME
    ────────────────┼──────────────────────┼────────────┼──────────────
    sk_buff         │ Single packet        │ Passed     │ Per-packet
    net_device      │ Network interface    │ Core       │ System lifetime
    socket          │ BSD socket (VFS)     │ VFS/file   │ fd lifetime
    sock            │ Protocol socket      │ Protocol   │ Outlives socket

    REFERENCE COUNTING:
    ┌─────────────────────────────────────────────────────────────┐
    │  • sk_buff: users field + skb_shared_info->dataref         │
    │  • net_device: refcnt + RCU for lookup                     │
    │  • sock: sk_refcnt (survives socket close)                 │
    │  • socket: tied to file lifecycle                          │
    └─────────────────────────────────────────────────────────────┘

    KEY RELATIONSHIPS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • socket->sk points to sock                               │
    │  • sock->sk_socket points back to socket (nullable!)       │
    │  • sk_buff->sk points to owning sock (or NULL)             │
    │  • sk_buff->dev points to net_device                       │
    └─────────────────────────────────────────────────────────────┘
```

**中文总结：**
- **sk_buff**：代表单个网络数据包，生命周期短，所有权在层间传递
- **net_device**：代表网络接口，生命周期长，由内核核心管理
- **socket**：BSD套接字抽象，与文件描述符绑定，面向用户空间
- **sock**：协议套接字，包含真正的协议状态，可以比socket存活更久
- **分离原因**：不同生命周期、不同所有权模型、支持多态和扩展
- **引用计数**：每种对象有独立的引用计数机制防止释放后使用

