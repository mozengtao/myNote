# Linux Networking Subsystem: Concurrency, Performance, and Zero-Copy Design

## 1. Locking Strategies

```
+------------------------------------------------------------------+
|  NETWORKING LOCKING HIERARCHY                                    |
+------------------------------------------------------------------+

    LOCK TYPES USED:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  SPINLOCKS (Non-sleeping, short critical sections):        │
    │    • ptype_lock       → Protocol handler list              │
    │    • dev->tx_lock     → Device transmit                    │
    │    • sk->sk_lock.slock → Socket (from softirq)             │
    │                                                              │
    │  SOCKET LOCK (Hybrid, can sleep):                          │
    │    • lock_sock() / release_sock()                          │
    │    • Process context can sleep                             │
    │    • Softirq uses backlog if locked                        │
    │                                                              │
    │  RCU (Read-Copy-Update, lockless reads):                   │
    │    • Device list traversal                                 │
    │    • Protocol handler lookup                               │
    │    • Socket hash table lookup                              │
    │                                                              │
    │  RTNL_LOCK (Routing netlink mutex):                        │
    │    • Device registration/unregistration                    │
    │    • Route table modifications                             │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    SOCKET LOCKING MODEL:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  struct sock {                                              │
    │      socket_lock_t sk_lock;                                │
    │      /* Contains: spinlock + owned flag + wait queue */    │
    │  };                                                         │
    │                                                              │
    │  PROCESS CONTEXT:                                          │
    │  ┌───────────────────────────────────────────────────────┐ │
    │  │  lock_sock(sk);     /* May sleep if owned */           │ │
    │  │  /* Critical section */                                │ │
    │  │  release_sock(sk);  /* Process backlog */              │ │
    │  └───────────────────────────────────────────────────────┘ │
    │                                                              │
    │  SOFTIRQ CONTEXT (Cannot sleep):                           │
    │  ┌───────────────────────────────────────────────────────┐ │
    │  │  bh_lock_sock(sk);  /* Spinlock only */                │ │
    │  │  if (sock_owned_by_user(sk)) {                         │ │
    │  │      /* Add skb to sk_backlog, process later */        │ │
    │  │      __sk_add_backlog(sk, skb);                        │ │
    │  │  } else {                                               │ │
    │  │      /* Process immediately */                         │ │
    │  │      sk_backlog_rcv(sk, skb);                          │ │
    │  │  }                                                       │ │
    │  │  bh_unlock_sock(sk);                                   │ │
    │  └───────────────────────────────────────────────────────┘ │
    │                                                              │
    │  When release_sock() is called, backlog is processed:      │
    │    while ((skb = sk->sk_backlog.head)) {                   │
    │        sk->sk_backlog_rcv(sk, skb);                        │
    │    }                                                        │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. Per-CPU Data Structures

```
+------------------------------------------------------------------+
|  PER-CPU DESIGN FOR SCALABILITY                                  |
+------------------------------------------------------------------+

    WHY PER-CPU:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Eliminate cache line bouncing between CPUs              │
    │  • No locks needed for per-CPU data                        │
    │  • Linear scaling with CPU count                           │
    │  • Critical for high packet rates                          │
    └─────────────────────────────────────────────────────────────┘

    PER-CPU STRUCTURES IN NETWORKING:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  1. SOFTNET_DATA (per-CPU receive queue):                  │
    │  ┌───────────────────────────────────────────────────────┐ │
    │  │  struct softnet_data {                                 │ │
    │  │      struct sk_buff_head input_pkt_queue; /* RX queue */│ │
    │  │      struct list_head poll_list;  /* NAPI poll list */ │ │
    │  │      struct sk_buff *completion_queue; /* TX complete */│ │
    │  │      /* statistics */                                  │ │
    │  │      unsigned int processed;                           │ │
    │  │      unsigned int time_squeeze;                        │ │
    │  │  };                                                     │ │
    │  │  DEFINE_PER_CPU_ALIGNED(struct softnet_data, softnet_data);│
    │  └───────────────────────────────────────────────────────┘ │
    │                                                              │
    │  2. SNMP STATISTICS (per-CPU counters):                    │
    │  ┌───────────────────────────────────────────────────────┐ │
    │  │  /* No locks for statistics updates */                 │ │
    │  │  DEFINE_SNMP_STAT(struct ipstats_mib, ip_statistics); │ │
    │  │                                                         │ │
    │  │  /* Increment: */                                      │ │
    │  │  IP_INC_STATS(net, IPSTATS_MIB_INPKTS);               │ │
    │  │                                                         │ │
    │  │  /* Read: Sum all CPUs */                              │ │
    │  │  for_each_possible_cpu(cpu)                            │ │
    │  │      sum += per_cpu_ptr(stats, cpu)->value;            │ │
    │  └───────────────────────────────────────────────────────┘ │
    │                                                              │
    │  3. TRANSMIT QUEUES (per-CPU TX):                          │
    │  ┌───────────────────────────────────────────────────────┐ │
    │  │  struct netdev_queue {                                 │ │
    │  │      struct Qdisc *qdisc;                              │ │
    │  │      spinlock_t _xmit_lock;                            │ │
    │  │      int xmit_lock_owner;                              │ │
    │  │      /* ... */                                         │ │
    │  │  };                                                     │ │
    │  │                                                         │ │
    │  │  /* Device has multiple TX queues */                   │ │
    │  │  struct net_device {                                   │ │
    │  │      struct netdev_queue *_tx;                         │ │
    │  │      unsigned int num_tx_queues;                       │ │
    │  │  };                                                     │ │
    │  └───────────────────────────────────────────────────────┘ │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    DATA FLOW WITH PER-CPU:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    CPU 0                    CPU 1                          │
    │    ┌─────────────────┐      ┌─────────────────┐            │
    │    │ softnet_data[0] │      │ softnet_data[1] │            │
    │    │   input_queue   │      │   input_queue   │            │
    │    │   poll_list     │      │   poll_list     │            │
    │    └────────┬────────┘      └────────┬────────┘            │
    │             │                        │                      │
    │             ▼                        ▼                      │
    │    ┌─────────────────┐      ┌─────────────────┐            │
    │    │ NET_RX_SOFTIRQ  │      │ NET_RX_SOFTIRQ  │            │
    │    │ processes own   │      │ processes own   │            │
    │    │ queue           │      │ queue           │            │
    │    └─────────────────┘      └─────────────────┘            │
    │                                                              │
    │    No cross-CPU contention for receive processing!         │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. Softirq and NAPI

```
+------------------------------------------------------------------+
|  SOFTIRQ AND NAPI FOR EFFICIENT PACKET PROCESSING                |
+------------------------------------------------------------------+

    PROBLEM: Interrupt per Packet
    ┌─────────────────────────────────────────────────────────────┐
    │  At 10Gbps with 64-byte packets:                           │
    │    • ~15 million packets/second                            │
    │    • One interrupt per packet = 15M interrupts/sec         │
    │    • CPU spends all time in interrupt handling             │
    │    • "Receive livelock" - no progress on actual work       │
    └─────────────────────────────────────────────────────────────┘

    SOLUTION: NAPI (New API)
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  1. First packet: Interrupt fires                          │
    │  2. Driver disables further interrupts                     │
    │  3. Driver schedules NAPI poll                             │
    │  4. Softirq polls for more packets (no interrupts)         │
    │  5. When queue empty: Re-enable interrupts                 │
    │                                                              │
    │  ┌───────────────────────────────────────────────────────┐ │
    │  │  /* Driver interrupt handler */                        │ │
    │  │  irqreturn_t e1000_intr(int irq, void *data)           │ │
    │  │  {                                                      │ │
    │  │      /* Disable interrupts */                          │ │
    │  │      e1000_irq_disable(adapter);                       │ │
    │  │                                                         │ │
    │  │      /* Schedule NAPI poll */                          │ │
    │  │      napi_schedule(&adapter->napi);                    │ │
    │  │                                                         │ │
    │  │      return IRQ_HANDLED;                                │ │
    │  │  }                                                      │ │
    │  │                                                         │ │
    │  │  /* NAPI poll function */                              │ │
    │  │  int e1000_poll(struct napi_struct *napi, int budget)  │ │
    │  │  {                                                      │ │
    │  │      int work_done = 0;                                │ │
    │  │                                                         │ │
    │  │      /* Process up to 'budget' packets */              │ │
    │  │      while (work_done < budget) {                      │ │
    │  │          skb = e1000_clean_rx_irq(adapter);            │ │
    │  │          if (!skb)                                     │ │
    │  │              break;                                     │ │
    │  │          napi_gro_receive(napi, skb);                  │ │
    │  │          work_done++;                                   │ │
    │  │      }                                                   │ │
    │  │                                                         │ │
    │  │      if (work_done < budget) {                         │ │
    │  │          /* Queue empty, re-enable interrupts */       │ │
    │  │          napi_complete(napi);                          │ │
    │  │          e1000_irq_enable(adapter);                    │ │
    │  │      }                                                   │ │
    │  │                                                         │ │
    │  │      return work_done;                                  │ │
    │  │  }                                                      │ │
    │  └───────────────────────────────────────────────────────┘ │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    SOFTIRQ SCHEDULING:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    Interrupt                                                │
    │        │                                                    │
    │        ▼                                                    │
    │    napi_schedule()                                         │
    │        │                                                    │
    │        ├──► Add napi to softnet_data.poll_list             │
    │        │                                                    │
    │        └──► raise_softirq(NET_RX_SOFTIRQ)                  │
    │                  │                                          │
    │                  ▼                                          │
    │    ┌────────────────────────────────────────────────────┐  │
    │    │  NET_RX_SOFTIRQ runs (net_rx_action):              │  │
    │    │                                                     │  │
    │    │  for each napi in poll_list:                       │  │
    │    │      work = napi->poll(napi, budget);              │  │
    │    │      if (work < budget)                            │  │
    │    │          napi_complete();  /* Done with this NAPI */│  │
    │    └────────────────────────────────────────────────────┘  │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Zero-Copy Design

```
+------------------------------------------------------------------+
|  ZERO-COPY PATTERNS IN NETWORKING                                |
+------------------------------------------------------------------+

    PATTERN 1: sk_buff Headroom/Tailroom
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  Problem: Each layer adds headers                          │
    │                                                              │
    │  Naive approach: Copy packet with new header each time     │
    │  Linux approach: Pre-allocate headroom, use skb_push()     │
    │                                                              │
    │  ┌─────────────────────────────────────────────────────────┐│
    │  │  /* Allocate with headroom for all layers */           ││
    │  │  skb = netdev_alloc_skb(dev, len + NET_SKB_PAD);       ││
    │  │  skb_reserve(skb, NET_SKB_PAD);  /* Reserve headroom */││
    │  │                                                          ││
    │  │  /* Each layer prepends header without copying: */     ││
    │  │  skb_push(skb, sizeof(struct tcphdr));  /* TCP */      ││
    │  │  skb_push(skb, sizeof(struct iphdr));   /* IP */       ││
    │  │  skb_push(skb, ETH_HLEN);               /* Ethernet */ ││
    │  │                                                          ││
    │  │  /* Just pointer manipulation, no memcpy! */           ││
    │  └─────────────────────────────────────────────────────────┘│
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    PATTERN 2: Scatter-Gather I/O
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  Problem: Large data spans multiple pages                  │
    │                                                              │
    │  ┌─────────────────────────────────────────────────────────┐│
    │  │  struct skb_shared_info {                              ││
    │  │      atomic_t dataref;                                 ││
    │  │      unsigned short nr_frags;                          ││
    │  │      skb_frag_t frags[MAX_SKB_FRAGS];  /* Page refs */ ││
    │  │  };                                                     ││
    │  │                                                          ││
    │  │  skb_frag_t = { page, offset, size }                   ││
    │  │                                                          ││
    │  │  Linear data: skb->data → small header buffer          ││
    │  │  Paged data: skb_shared_info.frags → user pages        ││
    │  │                                                          ││
    │  │  NIC with scatter-gather DMA reads directly from pages ││
    │  └─────────────────────────────────────────────────────────┘│
    │                                                              │
    │    ┌─────────────┐                                         │
    │    │  sk_buff    │                                         │
    │    │  ┌───────┐  │                                         │
    │    │  │ head  │──┼──► ┌──────────────┐                     │
    │    │  │ data  │  │    │ Linear data  │ (headers)           │
    │    │  │ tail  │  │    │  < 256 bytes │                     │
    │    │  │ end   │  │    ├──────────────┤                     │
    │    │  └───────┘  │    │ shared_info  │                     │
    │    └─────────────┘    │  frags[0] ───┼──► Page 0           │
    │                       │  frags[1] ───┼──► Page 1           │
    │                       │  frags[2] ───┼──► Page 2           │
    │                       └──────────────┘                     │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    PATTERN 3: sendfile() / splice()
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  Problem: File → Network copy goes through user space      │
    │                                                              │
    │  Traditional:                                               │
    │    read(file_fd, buffer, size);   /* Kernel → User */     │
    │    write(socket_fd, buffer, size); /* User → Kernel */    │
    │                                                              │
    │  Zero-copy:                                                 │
    │    sendfile(socket_fd, file_fd, offset, count);            │
    │    /* Page cache → Socket directly, no user copy */       │
    │                                                              │
    │    ┌─────────────────────────────────────────────────────┐ │
    │    │  Page Cache                 Socket                  │ │
    │    │  ┌─────────┐                ┌─────────┐             │ │
    │    │  │ Page 0  │───────────────►│ skb frag│             │ │
    │    │  │ Page 1  │───────────────►│ skb frag│             │ │
    │    │  │ Page 2  │───────────────►│ skb frag│             │ │
    │    │  └─────────┘                └─────────┘             │ │
    │    │                                                      │ │
    │    │  Pages are reference-counted, not copied            │ │
    │    └─────────────────────────────────────────────────────┘ │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    PATTERN 4: MSG_ZEROCOPY
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  Problem: send() copies user data to kernel                │
    │                                                              │
    │  Zero-copy send (Linux 4.14+):                             │
    │    send(fd, buf, len, MSG_ZEROCOPY);                       │
    │    /* Kernel pins user pages, DMA directly */              │
    │    /* Notification via errqueue when done */               │
    │                                                              │
    │    User buffer must remain valid until notification!       │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 5. Backpressure

```
+------------------------------------------------------------------+
|  BACKPRESSURE MECHANISMS                                         |
+------------------------------------------------------------------+

    WHERE BACKPRESSURE IS APPLIED:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  1. SOCKET RECEIVE BUFFER:                                 │
    │  ┌───────────────────────────────────────────────────────┐ │
    │  │  if (atomic_read(&sk->sk_rmem_alloc) > sk->sk_rcvbuf) │ │
    │  │      /* Drop packet */                                 │ │
    │  │                                                         │ │
    │  │  /* Application controls via SO_RCVBUF */              │ │
    │  └───────────────────────────────────────────────────────┘ │
    │                                                              │
    │  2. SOCKET SEND BUFFER:                                    │
    │  ┌───────────────────────────────────────────────────────┐ │
    │  │  if (sk->sk_wmem_queued > sk->sk_sndbuf)              │ │
    │  │      /* Block or return EAGAIN */                     │ │
    │  │                                                         │ │
    │  │  /* TCP also uses congestion window */                 │ │
    │  └───────────────────────────────────────────────────────┘ │
    │                                                              │
    │  3. DEVICE TRANSMIT QUEUE:                                 │
    │  ┌───────────────────────────────────────────────────────┐ │
    │  │  if (qdisc->q.qlen >= qdisc->limit)                   │ │
    │  │      /* Drop or apply traffic control */              │ │
    │  │                                                         │ │
    │  │  /* Controlled via `tc qdisc` */                       │ │
    │  └───────────────────────────────────────────────────────┘ │
    │                                                              │
    │  4. NAPI BUDGET:                                           │
    │  ┌───────────────────────────────────────────────────────┐ │
    │  │  /* Don't process more than budget packets */         │ │
    │  │  if (work_done >= budget)                             │ │
    │  │      /* Yield CPU, process more later */              │ │
    │  │                                                         │ │
    │  │  /* Prevents receive livelock */                       │ │
    │  └───────────────────────────────────────────────────────┘ │
    │                                                              │
    │  5. TCP CONGESTION CONTROL:                                │
    │  ┌───────────────────────────────────────────────────────┐ │
    │  │  /* cwnd limits in-flight data */                     │ │
    │  │  if (tcp_packets_in_flight(tp) >= tp->snd_cwnd)       │ │
    │  │      /* Don't send more */                            │ │
    │  │                                                         │ │
    │  │  /* Network-level backpressure */                      │ │
    │  └───────────────────────────────────────────────────────┘ │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  PERFORMANCE DESIGN SUMMARY                                      |
+------------------------------------------------------------------+

    LOCKING STRATEGY:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Socket lock: Hybrid (spin + sleep + backlog)            │
    │  • RCU: Lockless reads for device/protocol lookup          │
    │  • Per-CPU: Statistics, queues to avoid contention         │
    │  • Spinlock: Short critical sections only                  │
    └─────────────────────────────────────────────────────────────┘

    NAPI DESIGN:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Interrupt coalescing via polling                        │
    │  • Budget limits prevent receive livelock                  │
    │  • GRO merges packets for efficiency                       │
    └─────────────────────────────────────────────────────────────┘

    ZERO-COPY PATTERNS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Headroom/tailroom for header manipulation               │
    │  • Scatter-gather for large data                           │
    │  • sendfile/splice for file→network                        │
    │  • MSG_ZEROCOPY for user buffer DMA                        │
    └─────────────────────────────────────────────────────────────┘

    BACKPRESSURE POINTS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Socket buffers (rcvbuf, sndbuf)                         │
    │  • Qdisc queue limits                                      │
    │  • NAPI budget                                             │
    │  • TCP congestion window                                   │
    └─────────────────────────────────────────────────────────────┘
```

**中文总结：**
- **锁策略**：套接字锁(混合)、RCU(无锁读)、每CPU数据(避免竞争)
- **NAPI设计**：中断合并、轮询预算、GRO合并包
- **零拷贝**：头部/尾部空间、分散-聚集I/O、sendfile/splice、MSG_ZEROCOPY
- **背压机制**：套接字缓冲区、Qdisc队列、NAPI预算、TCP拥塞窗口
- **每CPU优化**：softnet_data、统计计数器、发送队列

