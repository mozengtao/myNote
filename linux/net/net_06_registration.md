# Linux Networking Subsystem: Registration and Plug-in Architecture

## 1. Registration Pattern Overview

```
+------------------------------------------------------------------+
|  REGISTRATION-BASED EXTENSIBILITY                                |
+------------------------------------------------------------------+

    CORE INSIGHT:
    ┌─────────────────────────────────────────────────────────────┐
    │  Linux networking uses REGISTRATION to achieve:            │
    │                                                              │
    │  • New protocols without core changes                      │
    │  • New device drivers without stack changes                │
    │  • Runtime loading/unloading (modules)                     │
    │  • Unified management and discovery                        │
    │                                                              │
    │  Pattern: Entity registers with central registry           │
    │           Core dispatches to registered entities           │
    └─────────────────────────────────────────────────────────────┘

    KEY REGISTRATION POINTS:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  ┌─────────────────────────────────────────────────────┐   │
    │  │  NETWORK DEVICES                                    │   │
    │  │    register_netdev() / unregister_netdev()          │   │
    │  └─────────────────────────────────────────────────────┘   │
    │                           │                                 │
    │  ┌─────────────────────────────────────────────────────┐   │
    │  │  PROTOCOL FAMILIES (AF_INET, AF_UNIX, ...)          │   │
    │  │    sock_register() / sock_unregister()              │   │
    │  └─────────────────────────────────────────────────────┘   │
    │                           │                                 │
    │  ┌─────────────────────────────────────────────────────┐   │
    │  │  TRANSPORT PROTOCOLS (TCP, UDP, ...)                │   │
    │  │    proto_register() / proto_unregister()            │   │
    │  │    inet_add_protocol() / inet_del_protocol()        │   │
    │  └─────────────────────────────────────────────────────┘   │
    │                           │                                 │
    │  ┌─────────────────────────────────────────────────────┐   │
    │  │  SOCKET TYPES (stream, dgram, ...)                  │   │
    │  │    inet_register_protosw() / inet_unregister_protosw() │
    │  └─────────────────────────────────────────────────────┘   │
    │                           │                                 │
    │  ┌─────────────────────────────────────────────────────┐   │
    │  │  PACKET HANDLERS (Ethernet types)                   │   │
    │  │    dev_add_pack() / dev_remove_pack()               │   │
    │  └─────────────────────────────────────────────────────┘   │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. register_netdev - Device Registration

```
+------------------------------------------------------------------+
|  NETWORK DEVICE REGISTRATION                                     |
+------------------------------------------------------------------+

    FLOW:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  Driver Probe                                               │
    │       │                                                     │
    │       ▼                                                     │
    │  alloc_netdev(sizeof(priv), "eth%d", ether_setup)          │
    │       │                                                     │
    │       ▼                                                     │
    │  Fill in net_device:                                       │
    │    netdev->netdev_ops = &my_netdev_ops;                    │
    │    netdev->ethtool_ops = &my_ethtool_ops;                  │
    │       │                                                     │
    │       ▼                                                     │
    │  register_netdev(netdev)                                   │
    │       │                                                     │
    │       ├──► Validate device                                 │
    │       ├──► Assign unique name (eth0, eth1, ...)            │
    │       ├──► Add to global device list                       │
    │       ├──► Create sysfs entries                            │
    │       ├──► Notify userspace (netlink)                      │
    │       └──► Device now visible to stack                     │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    KEY CODE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/core/dev.c */                                      │
    │                                                              │
    │  int register_netdev(struct net_device *dev)               │
    │  {                                                          │
    │      int err;                                               │
    │                                                              │
    │      rtnl_lock();  /* Take routing netlink lock */         │
    │      err = register_netdevice(dev);                        │
    │      rtnl_unlock();                                        │
    │                                                              │
    │      return err;                                            │
    │  }                                                          │
    │                                                              │
    │  int register_netdevice(struct net_device *dev)            │
    │  {                                                          │
    │      /* Validate */                                        │
    │      if (!dev->netdev_ops)                                 │
    │          return -EINVAL;                                    │
    │                                                              │
    │      /* Assign name */                                     │
    │      if (dev_get_valid_name(dev, dev->name, 0))            │
    │          goto out;                                          │
    │                                                              │
    │      /* Add to namespace's device list */                  │
    │      list_add_tail_rcu(&dev->dev_list,                     │
    │                        &net->dev_base_head);                │
    │                                                              │
    │      /* Add to hash tables for fast lookup */              │
    │      hlist_add_head_rcu(&dev->name_hlist,                  │
    │                         dev_name_hash(net, dev->name));     │
    │                                                              │
    │      /* Create sysfs entries */                            │
    │      ret = netdev_register_kobject(dev);                   │
    │                                                              │
    │      /* Notify listeners */                                │
    │      call_netdevice_notifiers(NETDEV_REGISTER, dev);       │
    │                                                              │
    │      return ret;                                            │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    DRIVER EXAMPLE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* drivers/net/ethernet/intel/e1000e/netdev.c */          │
    │                                                              │
    │  static int e1000_probe(struct pci_dev *pdev, ...)         │
    │  {                                                          │
    │      struct net_device *netdev;                            │
    │      struct e1000_adapter *adapter;                        │
    │                                                              │
    │      /* Allocate net_device + private data */              │
    │      netdev = alloc_etherdev(sizeof(*adapter));            │
    │      if (!netdev)                                          │
    │          return -ENOMEM;                                    │
    │                                                              │
    │      /* Set up ops tables */                               │
    │      netdev->netdev_ops = &e1000e_netdev_ops;              │
    │      e1000e_set_ethtool_ops(netdev);                       │
    │                                                              │
    │      /* Hardware initialization... */                      │
    │                                                              │
    │      /* Register with networking stack */                  │
    │      err = register_netdev(netdev);                        │
    │      if (err)                                              │
    │          goto err_register;                                │
    │                                                              │
    │      return 0;                                              │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. inet_register_protosw - Socket Type Registration

```
+------------------------------------------------------------------+
|  SOCKET TYPE REGISTRATION (inet_protosw)                         |
+------------------------------------------------------------------+

    PURPOSE:
    ┌─────────────────────────────────────────────────────────────┐
    │  Maps (family, type, protocol) → (proto_ops, proto)        │
    │                                                              │
    │  When user calls socket(AF_INET, SOCK_STREAM, 0):          │
    │    • family = AF_INET                                      │
    │    • type = SOCK_STREAM                                    │
    │    • protocol = 0 (default)                                │
    │                                                              │
    │  Registry lookup returns:                                  │
    │    • ops = inet_stream_ops                                 │
    │    • proto = tcp_prot                                      │
    └─────────────────────────────────────────────────────────────┘

    STRUCTURE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* include/net/protocol.h */                              │
    │                                                              │
    │  struct inet_protosw {                                     │
    │      struct list_head list;                                │
    │      unsigned short type;      /* SOCK_STREAM, SOCK_DGRAM */│
    │      unsigned short protocol;  /* IPPROTO_TCP, IPPROTO_UDP */│
    │      struct proto *prot;       /* tcp_prot, udp_prot */    │
    │      const struct proto_ops *ops; /* inet_stream_ops, etc */│
    │      char no_check;                                        │
    │      unsigned char flags;                                  │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘

    TCP REGISTRATION:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/ipv4/af_inet.c */                                  │
    │                                                              │
    │  static struct inet_protosw inetsw_array[] = {             │
    │      {                                                      │
    │          .type     = SOCK_STREAM,                          │
    │          .protocol = IPPROTO_TCP,                          │
    │          .prot     = &tcp_prot,                            │
    │          .ops      = &inet_stream_ops,                     │
    │          .no_check = 0,                                    │
    │          .flags    = INET_PROTOSW_PERMANENT |              │
    │                      INET_PROTOSW_ICSK,                    │
    │      },                                                     │
    │      {                                                      │
    │          .type     = SOCK_DGRAM,                           │
    │          .protocol = IPPROTO_UDP,                          │
    │          .prot     = &udp_prot,                            │
    │          .ops      = &inet_dgram_ops,                      │
    │          .no_check = UDP_CSUM_DEFAULT,                     │
    │          .flags    = INET_PROTOSW_PERMANENT,               │
    │      },                                                     │
    │      /* ... raw sockets, etc. */                           │
    │  };                                                         │
    │                                                              │
    │  /* At init time */                                        │
    │  static int __init inet_init(void)                         │
    │  {                                                          │
    │      for (q = inetsw_array;                                │
    │           q < &inetsw_array[INETSW_ARRAY_LEN]; ++q)        │
    │          inet_register_protosw(q);                         │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    REGISTRATION FUNCTION:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/ipv4/af_inet.c */                                  │
    │                                                              │
    │  void inet_register_protosw(struct inet_protosw *p)        │
    │  {                                                          │
    │      struct list_head *lh;                                 │
    │      struct inet_protosw *answer;                          │
    │      int protocol = p->protocol;                           │
    │                                                              │
    │      spin_lock_bh(&inetsw_lock);                           │
    │                                                              │
    │      /* Check for duplicates */                            │
    │      list_for_each(lh, &inetsw[p->type]) {                 │
    │          answer = list_entry(lh, struct inet_protosw, list);│
    │          if (answer->protocol == protocol)                 │
    │              break;  /* Already registered */              │
    │      }                                                       │
    │                                                              │
    │      /* Add to list */                                     │
    │      list_add_rcu(&p->list, &inetsw[p->type]);             │
    │                                                              │
    │      spin_unlock_bh(&inetsw_lock);                         │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    LOOKUP AT SOCKET CREATION:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/ipv4/af_inet.c */                                  │
    │                                                              │
    │  static int inet_create(struct net *net, struct socket *sock,│
    │                         int protocol, int kern)             │
    │  {                                                          │
    │      struct inet_protosw *answer;                          │
    │                                                              │
    │      /* Find matching protosw */                           │
    │      list_for_each_entry_rcu(answer, &inetsw[sock->type],  │
    │                              list) {                        │
    │          if (protocol == answer->protocol) {               │
    │              /* Found it! */                               │
    │              sock->ops = answer->ops;                      │
    │              sk = sk_alloc(net, PF_INET, GFP_KERNEL,       │
    │                            answer->prot);                   │
    │              break;                                         │
    │          }                                                   │
    │      }                                                       │
    │                                                              │
    │      /* Now sock->ops points to inet_stream_ops (for TCP) */│
    │      /* And sk->sk_prot points to tcp_prot */              │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. proto_register - Transport Protocol Registration

```
+------------------------------------------------------------------+
|  TRANSPORT PROTOCOL REGISTRATION (struct proto)                  |
+------------------------------------------------------------------+

    PURPOSE:
    ┌─────────────────────────────────────────────────────────────┐
    │  Register transport protocol (TCP, UDP, SCTP, etc.)        │
    │  with the kernel's protocol infrastructure.                │
    │                                                              │
    │  Creates per-protocol slab cache for socket allocation.    │
    └─────────────────────────────────────────────────────────────┘

    REGISTRATION:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/core/sock.c */                                     │
    │                                                              │
    │  int proto_register(struct proto *prot, int alloc_slab)    │
    │  {                                                          │
    │      /* Create slab cache for this protocol's socks */    │
    │      if (alloc_slab) {                                     │
    │          prot->slab = kmem_cache_create(prot->name,        │
    │                                         prot->obj_size, 0, │
    │                                         SLAB_HWCACHE_ALIGN, │
    │                                         NULL);              │
    │          if (!prot->slab)                                  │
    │              return -ENOBUFS;                              │
    │      }                                                       │
    │                                                              │
    │      /* Add to global protocol list */                     │
    │      mutex_lock(&proto_list_mutex);                        │
    │      list_add(&prot->node, &proto_list);                   │
    │      mutex_unlock(&proto_list_mutex);                      │
    │                                                              │
    │      return 0;                                              │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    TCP EXAMPLE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/ipv4/tcp_ipv4.c */                                 │
    │                                                              │
    │  struct proto tcp_prot = {                                 │
    │      .name           = "TCP",                              │
    │      .owner          = THIS_MODULE,                        │
    │      .close          = tcp_close,                          │
    │      .connect        = tcp_v4_connect,                     │
    │      .disconnect     = tcp_disconnect,                     │
    │      .accept         = inet_csk_accept,                    │
    │      .ioctl          = tcp_ioctl,                          │
    │      .init           = tcp_v4_init_sock,                   │
    │      .destroy        = tcp_v4_destroy_sock,                │
    │      .shutdown       = tcp_shutdown,                       │
    │      .setsockopt     = tcp_setsockopt,                     │
    │      .getsockopt     = tcp_getsockopt,                     │
    │      .recvmsg        = tcp_recvmsg,                        │
    │      .sendmsg        = tcp_sendmsg,                        │
    │      .hash           = inet_hash,                          │
    │      .unhash         = inet_unhash,                        │
    │      .obj_size       = sizeof(struct tcp_sock),            │
    │      /* ... */                                             │
    │  };                                                         │
    │                                                              │
    │  /* At init time */                                        │
    │  static int __init tcp_v4_init(void)                       │
    │  {                                                          │
    │      return proto_register(&tcp_prot, 1);                  │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
```

---

## 5. dev_add_pack - Protocol Handler Registration

```
+------------------------------------------------------------------+
|  PACKET TYPE (PROTOCOL) HANDLER REGISTRATION                     |
+------------------------------------------------------------------+

    PURPOSE:
    ┌─────────────────────────────────────────────────────────────┐
    │  Register handler for specific Ethernet protocol types:    │
    │                                                              │
    │  ETH_P_IP   (0x0800) → ip_rcv()                           │
    │  ETH_P_IPV6 (0x86DD) → ipv6_rcv()                         │
    │  ETH_P_ARP  (0x0806) → arp_rcv()                          │
    │  ETH_P_ALL           → Capture all packets                 │
    │                                                              │
    │  When NIC receives frame, netif_receive_skb() looks up     │
    │  handler by skb->protocol and calls it.                    │
    └─────────────────────────────────────────────────────────────┘

    STRUCTURE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* include/linux/netdevice.h */                           │
    │                                                              │
    │  struct packet_type {                                      │
    │      __be16 type;         /* Ethernet protocol type */     │
    │      struct net_device *dev;  /* NULL = all devices */     │
    │      int (*func)(struct sk_buff *skb,                      │
    │                  struct net_device *dev,                    │
    │                  struct packet_type *pt,                    │
    │                  struct net_device *orig_dev);              │
    │      void *af_packet_priv;                                 │
    │      struct list_head list;                                │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘

    IP REGISTRATION:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/ipv4/af_inet.c */                                  │
    │                                                              │
    │  static struct packet_type ip_packet_type __read_mostly = {│
    │      .type = cpu_to_be16(ETH_P_IP),                        │
    │      .func = ip_rcv,                                       │
    │  };                                                         │
    │                                                              │
    │  static int __init inet_init(void)                         │
    │  {                                                          │
    │      /* Register IP packet handler */                      │
    │      dev_add_pack(&ip_packet_type);                        │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    REGISTRATION FUNCTION:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/core/dev.c */                                      │
    │                                                              │
    │  void dev_add_pack(struct packet_type *pt)                 │
    │  {                                                          │
    │      struct list_head *head = ptype_head(pt);              │
    │                                                              │
    │      spin_lock(&ptype_lock);                               │
    │      list_add_rcu(&pt->list, head);                        │
    │      spin_unlock(&ptype_lock);                             │
    │  }                                                          │
    │                                                              │
    │  static inline struct list_head *ptype_head(                │
    │      const struct packet_type *pt)                          │
    │  {                                                          │
    │      if (pt->type == htons(ETH_P_ALL))                     │
    │          return &ptype_all;  /* Capture all */             │
    │      else                                                   │
    │          return &ptype_base[ntohs(pt->type) & PTYPE_HASH_MASK];│
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    DISPATCH AT RECEIVE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/core/dev.c - netif_receive_skb() path */           │
    │                                                              │
    │  static int __netif_receive_skb_core(struct sk_buff *skb,  │
    │                                      bool pfmemalloc)       │
    │  {                                                          │
    │      struct packet_type *ptype;                            │
    │      __be16 type = skb->protocol;                          │
    │                                                              │
    │      /* Deliver to ptype_all (tcpdump, etc.) */            │
    │      list_for_each_entry_rcu(ptype, &ptype_all, list) {    │
    │          deliver_skb(skb, ptype, orig_dev);                │
    │      }                                                       │
    │                                                              │
    │      /* Deliver to protocol handler */                     │
    │      list_for_each_entry_rcu(ptype,                        │
    │                  &ptype_base[ntohs(type) & PTYPE_HASH_MASK],│
    │                  list) {                                    │
    │          if (ptype->type == type) {                        │
    │              ret = ptype->func(skb, skb->dev, ptype,       │
    │                                orig_dev);                   │
    │              /* e.g., ip_rcv() for IP packets */           │
    │          }                                                   │
    │      }                                                       │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
```

---

## 6. VFS-Style Extensibility Comparison

```
+------------------------------------------------------------------+
|  NETWORKING vs VFS REGISTRATION PATTERNS                         |
+------------------------------------------------------------------+

    VFS (Filesystem) Pattern:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct file_system_type ext4_fs_type = {                  │
    │      .name     = "ext4",                                   │
    │      .mount    = ext4_mount,                               │
    │      ...                                                    │
    │  };                                                         │
    │                                                              │
    │  register_filesystem(&ext4_fs_type);                       │
    │                                                              │
    │  → VFS core can mount ext4 without knowing ext4 code       │
    │  → mount() system call dispatches via file_system_type     │
    └─────────────────────────────────────────────────────────────┘

    Networking Pattern:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct proto tcp_prot = {                                 │
    │      .name     = "TCP",                                    │
    │      .connect  = tcp_v4_connect,                           │
    │      ...                                                    │
    │  };                                                         │
    │                                                              │
    │  proto_register(&tcp_prot, 1);                             │
    │  inet_register_protosw(&tcp_protosw);                      │
    │                                                              │
    │  → Socket core can use TCP without knowing TCP code        │
    │  → socket() system call dispatches via protosw + proto     │
    └─────────────────────────────────────────────────────────────┘

    SIMILARITIES:
    ┌─────────────────────────────────────────────────────────────┐
    │  Both use:                                                  │
    │    • Descriptor struct with ops table                      │
    │    • Central registry                                      │
    │    • Registration function at init                         │
    │    • Dispatch via pointer (no switch)                      │
    │    • Support for loadable modules                          │
    └─────────────────────────────────────────────────────────────┘

    COMPLETE INIT SEQUENCE:
    ┌─────────────────────────────────────────────────────────────┐
    │  /* net/ipv4/af_inet.c - inet_init() */                    │
    │                                                              │
    │  static int __init inet_init(void)                         │
    │  {                                                          │
    │      /* 1. Register protocol family */                     │
    │      sock_register(&inet_family_ops);                      │
    │                                                              │
    │      /* 2. Register transport protocols */                 │
    │      proto_register(&tcp_prot, 1);                         │
    │      proto_register(&udp_prot, 1);                         │
    │      proto_register(&raw_prot, 1);                         │
    │                                                              │
    │      /* 3. Register socket types */                        │
    │      for (q = inetsw_array; ...; q++)                      │
    │          inet_register_protosw(q);                         │
    │                                                              │
    │      /* 4. Initialize ARP */                               │
    │      arp_init();                                            │
    │                                                              │
    │      /* 5. Initialize IP layer */                          │
    │      ip_init();                                             │
    │                                                              │
    │      /* 6. Initialize TCP */                               │
    │      tcp_v4_init();                                        │
    │                                                              │
    │      /* 7. Register IP packet handler */                   │
    │      dev_add_pack(&ip_packet_type);                        │
    │                                                              │
    │      /* Now AF_INET sockets work! */                       │
    │  }                                                          │
    │                                                              │
    │  fs_initcall(inet_init);  /* Called early in boot */       │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  REGISTRATION ARCHITECTURE SUMMARY                               |
+------------------------------------------------------------------+

    REGISTRATION POINTS:
    ┌─────────────────────────────────────────────────────────────┐
    │  register_netdev()       → Network device drivers          │
    │  sock_register()         → Protocol families (AF_*)        │
    │  proto_register()        → Transport protocols             │
    │  inet_register_protosw() → Socket type mappings            │
    │  dev_add_pack()          → Ethernet type handlers          │
    └─────────────────────────────────────────────────────────────┘

    WHY REGISTRATION SCALES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • O(1) dispatch (pointer call, not switch)                │
    │  • No core changes for new protocols/drivers               │
    │  • Support for loadable modules                            │
    │  • Central management and discovery                        │
    │  • Clean unregistration (module unload)                    │
    └─────────────────────────────────────────────────────────────┘

    PATTERN ELEMENTS:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Descriptor struct with ops table                       │
    │  2. Central registry (list or hash table)                  │
    │  3. register_*() / unregister_*() functions                │
    │  4. Lookup function for dispatch                           │
    │  5. Proper locking (RCU for readers)                       │
    └─────────────────────────────────────────────────────────────┘
```

**中文总结：**
- **设备注册**：register_netdev()将网络设备加入全局列表
- **协议家族**：sock_register()注册AF_INET等协议家族
- **传输协议**：proto_register()注册TCP/UDP等传输协议
- **套接字类型**：inet_register_protosw()映射(家族,类型,协议)→ops
- **包处理器**：dev_add_pack()注册以太网类型对应的处理函数
- **与VFS相似**：都使用描述符结构+ops表+中央注册表+模块支持
- **扩展性**：O(1)分发、无需修改核心、支持模块加载卸载

