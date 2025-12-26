# Linux Networking Subsystem: Manual Polymorphism via ops Tables

## 1. Ops Tables Overview

```
+------------------------------------------------------------------+
|  OPS-BASED POLYMORPHISM IN NETWORKING                            |
+------------------------------------------------------------------+

    CORE INSIGHT:
    ┌─────────────────────────────────────────────────────────────┐
    │  The networking stack uses function pointer tables (ops)   │
    │  to achieve runtime polymorphism in C:                     │
    │                                                              │
    │  • net_device_ops   → Hardware driver interface            │
    │  • proto_ops        → Socket-level protocol interface      │
    │  • proto            → Transport protocol interface         │
    │  • af_ops           → Address family specific hooks        │
    │                                                              │
    │  This allows the core stack to call generic functions      │
    │  that dispatch to the correct implementation at runtime.   │
    └─────────────────────────────────────────────────────────────┘

    DISPATCH PATTERN:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    /* Generic code (core stack) */                         │
    │    int sock_sendmsg(struct socket *sock, ...)              │
    │    {                                                        │
    │        /* Dispatch via ops table - no switch! */           │
    │        return sock->ops->sendmsg(iocb, sock, msg, size);   │
    │    }                                                        │
    │                                                              │
    │    /* TCP implementation */                                 │
    │    const struct proto_ops inet_stream_ops = {              │
    │        .sendmsg = tcp_sendmsg,                             │
    │        ...                                                  │
    │    };                                                       │
    │                                                              │
    │    /* UDP implementation */                                 │
    │    const struct proto_ops inet_dgram_ops = {               │
    │        .sendmsg = udp_sendmsg,                             │
    │        ...                                                  │
    │    };                                                       │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. struct net_device_ops (Driver Interface)

```
+------------------------------------------------------------------+
|  NET_DEVICE_OPS: DRIVER POLYMORPHISM                             |
+------------------------------------------------------------------+

    PURPOSE:
    ┌─────────────────────────────────────────────────────────────┐
    │  Abstracts hardware differences:                           │
    │    • How to transmit a packet                              │
    │    • How to open/close the device                          │
    │    • How to configure hardware                             │
    │                                                              │
    │  Core stack calls same functions for ANY network device:   │
    │    • Ethernet NIC                                          │
    │    • WiFi adapter                                          │
    │    • Virtual tun/tap                                       │
    │    • Loopback                                              │
    └─────────────────────────────────────────────────────────────┘

    KEY OPERATIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct net_device_ops {                                   │
    │      /* Device lifecycle */                                │
    │      int (*ndo_init)(struct net_device *dev);              │
    │      void (*ndo_uninit)(struct net_device *dev);           │
    │      int (*ndo_open)(struct net_device *dev);              │
    │      int (*ndo_stop)(struct net_device *dev);              │
    │                                                              │
    │      /* THE critical function: transmit packet */          │
    │      netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,    │
    │                                    struct net_device *dev);│
    │                                                              │
    │      /* Configuration */                                   │
    │      void (*ndo_set_rx_mode)(struct net_device *dev);      │
    │      int (*ndo_set_mac_address)(struct net_device *dev,    │
    │                                  void *addr);               │
    │      int (*ndo_change_mtu)(struct net_device *dev,         │
    │                            int new_mtu);                    │
    │                                                              │
    │      /* Statistics */                                      │
    │      struct net_device_stats* (*ndo_get_stats)(            │
    │                                    struct net_device *dev); │
    │      /* ... ~40 more operations */                         │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘

    EXAMPLE: INTEL E1000E DRIVER
    ┌─────────────────────────────────────────────────────────────┐
    │  /* drivers/net/ethernet/intel/e1000e/netdev.c */          │
    │                                                              │
    │  static const struct net_device_ops e1000e_netdev_ops = {  │
    │      .ndo_open             = e1000_open,                   │
    │      .ndo_stop             = e1000_close,                  │
    │      .ndo_start_xmit       = e1000_xmit_frame,             │
    │      .ndo_get_stats        = e1000_get_stats,              │
    │      .ndo_set_rx_mode      = e1000_set_multi,              │
    │      .ndo_set_mac_address  = e1000_set_mac,                │
    │      .ndo_change_mtu       = e1000_change_mtu,             │
    │      .ndo_tx_timeout       = e1000_tx_timeout,             │
    │      /* ... */                                             │
    │  };                                                         │
    │                                                              │
    │  /* During probe */                                        │
    │  netdev->netdev_ops = &e1000e_netdev_ops;                  │
    └─────────────────────────────────────────────────────────────┘

    DISPATCH FROM CORE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/core/dev.c - dev_hard_start_xmit() */              │
    │                                                              │
    │  static inline int __dev_xmit_skb(struct sk_buff *skb,     │
    │                                   struct Qdisc *q,          │
    │                                   struct net_device *dev,   │
    │                                   ...)                      │
    │  {                                                          │
    │      /* Eventually calls: */                               │
    │      rc = ops->ndo_start_xmit(skb, dev);                   │
    │      /* ops = dev->netdev_ops */                           │
    │  }                                                          │
    │                                                              │
    │  /* Core never knows if it's talking to e1000, WiFi, etc. */│
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. struct proto_ops (Socket-Level Interface)

```
+------------------------------------------------------------------+
|  PROTO_OPS: SOCKET API POLYMORPHISM                              |
+------------------------------------------------------------------+

    PURPOSE:
    ┌─────────────────────────────────────────────────────────────┐
    │  Abstracts socket operations across protocol families:     │
    │    • AF_INET (IPv4)                                        │
    │    • AF_INET6 (IPv6)                                       │
    │    • AF_UNIX (Unix domain)                                 │
    │    • AF_NETLINK (Kernel messaging)                         │
    │                                                              │
    │  BSD socket calls dispatch via proto_ops to right handler  │
    └─────────────────────────────────────────────────────────────┘

    KEY OPERATIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct proto_ops {                                        │
    │      int family;                                           │
    │      struct module *owner;                                 │
    │                                                              │
    │      int (*release)(struct socket *sock);                  │
    │      int (*bind)(struct socket *sock,                      │
    │                  struct sockaddr *myaddr, int sockaddr_len);│
    │      int (*connect)(struct socket *sock,                   │
    │                     struct sockaddr *vaddr,                 │
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

    PROTOCOL-SPECIFIC INSTANCES:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/ipv4/af_inet.c */                                  │
    │                                                              │
    │  /* TCP over IPv4 */                                       │
    │  const struct proto_ops inet_stream_ops = {                │
    │      .family      = PF_INET,                               │
    │      .release     = inet_release,                          │
    │      .bind        = inet_bind,                             │
    │      .connect     = inet_stream_connect,                   │
    │      .accept      = inet_accept,                           │
    │      .listen      = inet_listen,                           │
    │      .sendmsg     = tcp_sendmsg,                           │
    │      .recvmsg     = inet_recvmsg,                          │
    │      /* ... */                                             │
    │  };                                                         │
    │                                                              │
    │  /* UDP over IPv4 */                                       │
    │  const struct proto_ops inet_dgram_ops = {                 │
    │      .family      = PF_INET,                               │
    │      .release     = inet_release,                          │
    │      .bind        = inet_bind,                             │
    │      .connect     = inet_dgram_connect,                    │
    │      .sendmsg     = inet_sendmsg,                          │
    │      .recvmsg     = inet_recvmsg,                          │
    │      /* ... */                                             │
    │  };                                                         │
    │                                                              │
    │  /* Unix domain sockets */                                 │
    │  /* net/unix/af_unix.c */                                  │
    │  static const struct proto_ops unix_stream_ops = {         │
    │      .family      = PF_UNIX,                               │
    │      .release     = unix_release,                          │
    │      .bind        = unix_bind,                             │
    │      .connect     = unix_stream_connect,                   │
    │      .accept      = unix_accept,                           │
    │      .sendmsg     = unix_stream_sendmsg,                   │
    │      .recvmsg     = unix_stream_recvmsg,                   │
    │      /* ... */                                             │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. struct proto (Transport Protocol Interface)

```
+------------------------------------------------------------------+
|  STRUCT PROTO: TRANSPORT LAYER POLYMORPHISM                      |
+------------------------------------------------------------------+

    PURPOSE:
    ┌─────────────────────────────────────────────────────────────┐
    │  Lower-level transport protocol operations:                │
    │    • TCP                                                   │
    │    • UDP                                                   │
    │    • SCTP                                                  │
    │    • DCCP                                                  │
    │                                                              │
    │  Operates on struct sock (not struct socket)               │
    │  More protocol-specific than proto_ops                     │
    └─────────────────────────────────────────────────────────────┘

    KEY OPERATIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct proto {                                            │
    │      char name[32];                                        │
    │                                                              │
    │      void (*close)(struct sock *sk, long timeout);         │
    │      int (*connect)(struct sock *sk,                       │
    │                     struct sockaddr *uaddr, int addr_len);  │
    │      int (*disconnect)(struct sock *sk, int flags);        │
    │      struct sock *(*accept)(struct sock *sk, int flags,    │
    │                             int *err);                      │
    │                                                              │
    │      int (*sendmsg)(struct kiocb *iocb, struct sock *sk,   │
    │                     struct msghdr *msg, size_t len);        │
    │      int (*recvmsg)(struct kiocb *iocb, struct sock *sk,   │
    │                     struct msghdr *msg, size_t len,         │
    │                     int noblock, int flags, int *addr_len);│
    │                                                              │
    │      int (*bind)(struct sock *sk, struct sockaddr *,       │
    │                  int addr_len);                             │
    │                                                              │
    │      int (*hash)(struct sock *sk);                         │
    │      void (*unhash)(struct sock *sk);                      │
    │                                                              │
    │      /* Memory management */                               │
    │      int (*init)(struct sock *sk);                         │
    │      void (*destroy)(struct sock *sk);                     │
    │      struct kmem_cache *slab;                              │
    │      unsigned int obj_size;                                │
    │                                                              │
    │      /* ... */                                             │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘

    PROTOCOL INSTANCES:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/ipv4/tcp_ipv4.c */                                 │
    │  struct proto tcp_prot = {                                 │
    │      .name          = "TCP",                               │
    │      .close         = tcp_close,                           │
    │      .connect       = tcp_v4_connect,                      │
    │      .disconnect    = tcp_disconnect,                      │
    │      .accept        = inet_csk_accept,                     │
    │      .sendmsg       = tcp_sendmsg,                         │
    │      .recvmsg       = tcp_recvmsg,                         │
    │      .bind          = inet_csk_get_port,                   │
    │      .hash          = inet_hash,                           │
    │      .unhash        = inet_unhash,                         │
    │      .init          = tcp_v4_init_sock,                    │
    │      .destroy       = tcp_v4_destroy_sock,                 │
    │      .slab          = NULL,  /* Per-protocol cache */      │
    │      .obj_size      = sizeof(struct tcp_sock),             │
    │      /* ... */                                             │
    │  };                                                         │
    │                                                              │
    │  /* net/ipv4/udp.c */                                      │
    │  struct proto udp_prot = {                                 │
    │      .name          = "UDP",                               │
    │      .close         = udp_lib_close,                       │
    │      .connect       = ip4_datagram_connect,                │
    │      .disconnect    = udp_disconnect,                      │
    │      .sendmsg       = udp_sendmsg,                         │
    │      .recvmsg       = udp_recvmsg,                         │
    │      .hash          = udp_lib_hash,                        │
    │      .unhash        = udp_lib_unhash,                      │
    │      .obj_size      = sizeof(struct udp_sock),             │
    │      /* ... */                                             │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘
```

---

## 5. Inversion of Control

```
+------------------------------------------------------------------+
|  INVERSION OF CONTROL IN THE NETWORKING STACK                    |
+------------------------------------------------------------------+

    TRADITIONAL APPROACH (NOT USED):
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  /* BAD: switch on protocol type */                        │
    │  int sock_sendmsg(struct socket *sock, ...)                │
    │  {                                                          │
    │      switch (sock->type) {                                 │
    │      case SOCK_STREAM:                                     │
    │          switch (sock->family) {                           │
    │          case AF_INET:                                     │
    │              return tcp_sendmsg_v4(...);                   │
    │          case AF_INET6:                                    │
    │              return tcp_sendmsg_v6(...);                   │
    │          case AF_UNIX:                                     │
    │              return unix_stream_sendmsg(...);              │
    │          default:                                          │
    │              return -ENOTSUP;                              │
    │          }                                                  │
    │      case SOCK_DGRAM:                                      │
    │          /* Another giant switch... */                     │
    │      }                                                      │
    │  }                                                          │
    │                                                              │
    │  PROBLEMS:                                                  │
    │  • Core code knows all protocols                           │
    │  • Adding protocol requires changing core                  │
    │  • Combinatorial explosion of cases                        │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    LINUX APPROACH (OPS-BASED):
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  /* GOOD: dispatch via ops pointer */                      │
    │  int sock_sendmsg(struct socket *sock, ...)                │
    │  {                                                          │
    │      return sock->ops->sendmsg(iocb, sock, msg, size);     │
    │  }                                                          │
    │                                                              │
    │  BENEFITS:                                                  │
    │  • Core code is protocol-agnostic                          │
    │  • New protocols register their ops                        │
    │  • No changes to core for new protocols                    │
    │  • Clean separation of concerns                            │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    CONTROL FLOW COMPARISON:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  TRADITIONAL:                                               │
    │  ┌──────────────┐                                          │
    │  │  Core Code   │────► switch(protocol)                    │
    │  │              │           │                              │
    │  │   CALLS      │───────────┼──────────────────────┐       │
    │  │              │           │                      │       │
    │  └──────────────┘           ▼                      ▼       │
    │                       ┌──────────┐           ┌──────────┐  │
    │                       │   TCP    │           │   UDP    │  │
    │                       └──────────┘           └──────────┘  │
    │                                                              │
    │  INVERSION OF CONTROL:                                      │
    │  ┌──────────────┐                                          │
    │  │  Core Code   │                                          │
    │  │              │◄────── ops->sendmsg() ──────┐            │
    │  │   CALLS      │◄────── ops->sendmsg() ───┐  │            │
    │  │              │                          │  │            │
    │  └──────────────┘                          │  │            │
    │         │                                  │  │            │
    │         │ dispatch via pointer             │  │            │
    │         │                                  │  │            │
    │         ▼                                  │  │            │
    │  ┌──────────────┐                          │  │            │
    │  │  tcp_prot    │ ─────────────────────────┘  │            │
    │  │  .sendmsg=   │                             │            │
    │  │  tcp_sendmsg │                             │            │
    │  └──────────────┘                             │            │
    │  ┌──────────────┐                             │            │
    │  │  udp_prot    │ ────────────────────────────┘            │
    │  │  .sendmsg=   │                                          │
    │  │  udp_sendmsg │                                          │
    │  └──────────────┘                                          │
    │                                                              │
    │  Protocol REGISTERS its ops, Core CALLS via pointer        │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 6. Complete Call Path Trace: sys_sendto

```
+------------------------------------------------------------------+
|  TRACING: sys_sendto → sock_sendmsg → sock->ops->sendmsg         |
+------------------------------------------------------------------+

    STEP 1: System Call Entry
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/socket.c */                                        │
    │  SYSCALL_DEFINE6(sendto, int, fd, void __user *, buff,     │
    │                  size_t, len, unsigned, flags,              │
    │                  struct sockaddr __user *, addr,            │
    │                  int, addr_len)                             │
    │  {                                                          │
    │      struct socket *sock;                                  │
    │      struct msghdr msg;                                    │
    │      struct iovec iov;                                     │
    │                                                              │
    │      /* Look up socket from fd */                          │
    │      sock = sockfd_lookup_light(fd, &err, &fput_needed);   │
    │                                                              │
    │      /* Build message header */                            │
    │      iov.iov_base = buff;                                  │
    │      iov.iov_len = len;                                    │
    │      msg.msg_iov = &iov;                                   │
    │      msg.msg_name = addr;                                  │
    │                                                              │
    │      /* Call sock_sendmsg */                               │
    │      err = sock_sendmsg(sock, &msg, len);                  │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    STEP 2: Socket Layer (Protocol-Agnostic)
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/socket.c */                                        │
    │  int sock_sendmsg(struct socket *sock, struct msghdr *msg, │
    │                   size_t size)                              │
    │  {                                                          │
    │      struct kiocb iocb;                                    │
    │      struct sock_iocb siocb;                               │
    │      int ret;                                               │
    │                                                              │
    │      init_sync_kiocb(&iocb, NULL);                         │
    │      iocb.private = &siocb;                                │
    │      siocb.sock = sock;                                    │
    │      siocb.msg = msg;                                      │
    │                                                              │
    │      /* DISPATCH VIA OPS TABLE */                          │
    │      ret = sock->ops->sendmsg(&iocb, sock, msg, size);     │
    │      /*    ^^^^^^^^^^^^^^^^^^^                             │
    │       *    This is the polymorphic dispatch!               │
    │       *    sock->ops points to:                            │
    │       *      inet_stream_ops (TCP)                         │
    │       *      inet_dgram_ops  (UDP)                         │
    │       *      unix_stream_ops (Unix)                        │
    │       */                                                   │
    │                                                              │
    │      return ret;                                           │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    STEP 3: Protocol-Specific Implementation (TCP)
    ┌─────────────────────────────────────────────────────────────┐
    │  /* sock->ops->sendmsg points here for TCP sockets */      │
    │  /* inet_stream_ops.sendmsg = tcp_sendmsg */               │
    │                                                              │
    │  /* net/ipv4/tcp.c */                                      │
    │  int tcp_sendmsg(struct kiocb *iocb, struct sock *sk,      │
    │                  struct msghdr *msg, size_t size)           │
    │  {                                                          │
    │      struct tcp_sock *tp = tcp_sk(sk);                     │
    │      struct sk_buff *skb;                                  │
    │      int copied = 0;                                       │
    │                                                              │
    │      /* Lock socket */                                     │
    │      lock_sock(sk);                                        │
    │                                                              │
    │      /* Copy user data into sk_buffs */                    │
    │      while (msg->msg_iov->iov_len > 0) {                   │
    │          /* Allocate or reuse skb */                       │
    │          skb = tcp_send_head(sk);                          │
    │          if (!skb) {                                        │
    │              skb = sk_stream_alloc_skb(sk, ...);           │
    │          }                                                  │
    │                                                              │
    │          /* Copy data from user space */                   │
    │          err = skb_add_data_nocache(sk, skb,               │
    │                                     msg->msg_iov, copy);    │
    │          copied += copy;                                   │
    │      }                                                       │
    │                                                              │
    │      /* Push data to transmit queue */                     │
    │      tcp_push(sk, flags, mss_now, tp->nonagle);            │
    │                                                              │
    │      release_sock(sk);                                     │
    │      return copied;                                        │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    STEP 4: For UDP (Different Path)
    ┌─────────────────────────────────────────────────────────────┐
    │  /* sock->ops->sendmsg points here for UDP sockets */      │
    │  /* inet_dgram_ops.sendmsg = inet_sendmsg */               │
    │  /* which calls sk->sk_prot->sendmsg = udp_sendmsg */      │
    │                                                              │
    │  /* net/ipv4/udp.c */                                      │
    │  int udp_sendmsg(struct kiocb *iocb, struct sock *sk,      │
    │                  struct msghdr *msg, size_t len)            │
    │  {                                                          │
    │      /* UDP-specific handling */                           │
    │      /* Build single datagram */                           │
    │      /* No connection state */                             │
    │      /* Single sk_buff per message */                      │
    │                                                              │
    │      /* Eventually calls ip_output() */                    │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    COMPLETE CALL GRAPH:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  sys_sendto(fd, buf, len, flags, addr, addr_len)           │
    │       │                                                     │
    │       ├──► sockfd_lookup_light(fd) ──► struct socket       │
    │       │                                                     │
    │       └──► sock_sendmsg(sock, msg, len)                    │
    │                 │                                           │
    │                 └──► sock->ops->sendmsg(...)               │
    │                           │                                 │
    │           ┌───────────────┼───────────────┐                │
    │           │               │               │                │
    │           ▼               ▼               ▼                │
    │     tcp_sendmsg     inet_sendmsg   unix_stream_sendmsg     │
    │           │               │               │                │
    │           │               └──► udp_sendmsg                 │
    │           │                                                 │
    │           ▼                                                 │
    │     tcp_push() ──► ip_output() ──► dev_queue_xmit()        │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 7. Why No Switch on Protocol Type

```
+------------------------------------------------------------------+
|  ARCHITECTURAL BENEFITS OF OPS-BASED DISPATCH                    |
+------------------------------------------------------------------+

    EXTENSIBILITY WITHOUT CORE CHANGES:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  Adding new protocol (e.g., SCTP):                         │
    │                                                              │
    │  1. Define ops tables:                                     │
    │     static const struct proto_ops sctp_stream_ops = { ... };│
    │     struct proto sctp_prot = { ... };                      │
    │                                                              │
    │  2. Register at init:                                      │
    │     proto_register(&sctp_prot, 1);                         │
    │     inet_register_protosw(&sctp_protosw);                  │
    │                                                              │
    │  3. DONE - No changes to net/socket.c, net/core/*          │
    │                                                              │
    │  Core sock_sendmsg() works automatically because:          │
    │     sock->ops->sendmsg points to sctp's implementation     │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    COMPILATION INDEPENDENCE:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  With switch:                                               │
    │  • Core must #include every protocol header                │
    │  • Changing any protocol header recompiles core            │
    │  • Ifdef hell for optional protocols                       │
    │                                                              │
    │  With ops:                                                  │
    │  • Core only needs struct proto_ops forward declaration    │
    │  • Protocol modules compile independently                  │
    │  • Modules can be loaded/unloaded at runtime               │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    TESTING AND DEBUGGING:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  • Can test core with mock ops table                       │
    │  • Can inject debug/tracing ops                            │
    │  • Can replace production ops for testing                  │
    │  • Protocols tested in isolation                           │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    PERFORMANCE:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  Switch statement:                                          │
    │  • Multiple comparisons (linear or jump table)             │
    │  • Branch prediction may help for common cases             │
    │  • Grows with protocol count                               │
    │                                                              │
    │  Ops dispatch:                                              │
    │  • Single indirect call                                    │
    │  • O(1) regardless of protocol count                       │
    │  • CPU can prefetch target if ops commonly used            │
    │  • Cache friendly (ops table fits in cache line)           │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  MANUAL POLYMORPHISM SUMMARY                                     |
+------------------------------------------------------------------+

    KEY OPS TABLES:
    ┌─────────────────────────────────────────────────────────────┐
    │  net_device_ops  → Hardware driver abstraction             │
    │  proto_ops       → BSD socket operations                   │
    │  proto           → Transport protocol operations           │
    │  af_ops          → Address family hooks                    │
    └─────────────────────────────────────────────────────────────┘

    DISPATCH PATTERN:
    ┌─────────────────────────────────────────────────────────────┐
    │  object->ops->method(object, args...)                      │
    │                                                              │
    │  Examples:                                                  │
    │  • sock->ops->sendmsg(...)                                 │
    │  • sk->sk_prot->connect(...)                               │
    │  • dev->netdev_ops->ndo_start_xmit(...)                    │
    └─────────────────────────────────────────────────────────────┘

    BENEFITS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Protocol/driver independence                            │
    │  • No switch statements in core                            │
    │  • Extension without core modification                     │
    │  • Runtime polymorphism in C                               │
    │  • Clean module boundaries                                 │
    └─────────────────────────────────────────────────────────────┘
```

**中文总结：**
- **ops表种类**：net_device_ops(驱动)、proto_ops(套接字)、proto(传输层)
- **分发模式**：object->ops->method()实现运行时多态
- **调用路径**：sys_sendto→sock_sendmsg→sock->ops->sendmsg
- **控制反转**：协议注册ops表，核心通过指针调用，无需switch语句
- **优势**：协议独立、扩展无需修改核心、编译独立、模块边界清晰

