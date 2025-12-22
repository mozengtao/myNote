# Linux Kernel net_device and TCP/IP Architecture (v3.2)

## Table of Contents

1. [Phase 1 — Netdev Architecture Overview](#phase-1--netdev-architecture-overview)
2. [Phase 2 — Ops-Based Polymorphism](#phase-2--ops-based-polymorphism)
3. [Phase 3 — sk_buff Ownership Model](#phase-3--sk_buff-ownership-model)
4. [Phase 4 — TCP FSM as Architecture](#phase-4--tcp-fsm-as-architecture)
5. [Phase 5 — Fast Path vs Slow Path](#phase-5--fast-path-vs-slow-path)
6. [Phase 6 — Common Bugs & Lessons](#phase-6--common-bugs--lessons)
7. [Appendix A — User-Space High-Performance Patterns](#appendix-a--user-space-high-performance-patterns)

---

## Phase 1 — Netdev Architecture Overview

### 1.1 Why net_device Exists

```
+------------------------------------------------------------------------+
|                    THE PROBLEM: HARDWARE DIVERSITY                      |
+------------------------------------------------------------------------+

Without net_device:
    Protocol Stack (TCP/IP)
         |
         | Send packet to Ethernet?
         v
    +----------+
    | e1000    |  <-- Hardcoded driver!
    +----------+
    
         | Send packet to WiFi?
         v
    +----------+
    | iwlwifi  |  <-- Different driver, different API!
    +----------+
    
         | Send packet to loopback?
         v
    +----------+
    | loopback |  <-- Yet another API!
    +----------+

With net_device:
    Protocol Stack (TCP/IP)
         |
         | dev_queue_xmit(skb)  <-- ONE API for all devices
         v
    +-------------+
    | net_device  |  <-- Uniform abstraction
    +-------------+
         |
    +----+----+----+----+
    |    |    |    |    |
  e1000 iwlwifi loop  veth ...
   ops   ops   ops   ops

net_device provides:
1. UNIFORM INTERFACE to all network hardware
2. QUEUING and SCHEDULING (qdisc)
3. POLYMORPHISM via net_device_ops
4. STATISTICS and CONFIGURATION
```

**中文解释：**
- **问题**：Linux支持数百种网络设备（以太网、WiFi、虚拟设备等），每种有不同硬件接口
- **解决方案**：`net_device` 提供统一的抽象层，协议栈只需调用通用API
- **核心机制**：通过 `net_device_ops` 函数指针表实现多态，每种驱动填充自己的操作

### 1.2 The Role of struct net_device

```c
/* include/linux/netdevice.h */
struct net_device {
    /*
     * ===== IDENTITY =====
     */
    char            name[IFNAMSIZ];     /* "eth0", "wlan0", etc. */
    unsigned long   state;              /* Device state flags */
    
    /*
     * ===== I/O CONFIGURATION =====
     */
    unsigned long   mem_end;            /* Shared memory end */
    unsigned long   mem_start;          /* Shared memory start */
    unsigned long   base_addr;          /* Device I/O address */
    unsigned int    irq;                /* Device IRQ number */
    
    /*
     * ===== OPERATIONS TABLE =====
     */
    const struct net_device_ops *netdev_ops;  /* [KEY] Device operations */
    const struct ethtool_ops    *ethtool_ops; /* Ethtool operations */
    
    /*
     * ===== TRANSMIT QUEUES =====
     */
    struct netdev_queue *_tx;           /* TX queue(s) */
    unsigned int    num_tx_queues;      /* Number of TX queues */
    unsigned int    real_num_tx_queues; /* Active TX queues */
    struct Qdisc    *qdisc;             /* Root qdisc */
    
    /*
     * ===== RECEIVE PATH =====
     */
    struct netdev_rx_queue *_rx;        /* RX queue(s) */
    rx_handler_func_t *rx_handler;      /* RX handler (bridging, bonding) */
    
    /*
     * ===== LINK LAYER =====
     */
    unsigned short  type;               /* ARPHRD_ETHER, etc. */
    unsigned short  hard_header_len;    /* Hardware header length */
    unsigned char   dev_addr[MAX_ADDR_LEN]; /* MAC address */
    unsigned int    mtu;                /* MTU (Maximum Transfer Unit) */
    
    /*
     * ===== FEATURES =====
     */
    u32             features;           /* NETIF_F_* capabilities */
    u32             hw_features;        /* Hardware-changeable features */
    
    /* ... many more fields ... */
};
```

**Key Relationships:**

```
+------------------------------------------------------------------------+
|                    net_device RELATIONSHIPS                             |
+------------------------------------------------------------------------+

                      +------------------+
                      |    net_device    |
                      +--------+---------+
                               |
         +---------------------+---------------------+
         |                     |                     |
+--------v--------+   +--------v--------+   +--------v--------+
| netdev_ops      |   |    TX queues    |   |   RX path       |
| .ndo_open()     |   | qdisc, skb_list |   | netif_rx()      |
| .ndo_stop()     |   | DMA rings       |   | NAPI poll       |
| .ndo_start_xmit |   |                 |   |                 |
+-----------------+   +-----------------+   +-----------------+
        |                     |                     |
        v                     v                     v
    DRIVER CODE           HARDWARE              PROTOCOL STACK
```

### 1.3 Network Stack Layering

```
+------------------------------------------------------------------------+
|                    COMPLETE NETWORK STACK LAYERING                      |
+------------------------------------------------------------------------+

USER SPACE
    |
    | sendmsg(fd, ...)
    |
+===|====================================================================+
    |
    v
+-----------------------------------------------------------------------+
|                     SOCKET LAYER                                       |
|  - struct socket                                                       |
|  - struct sock (sk)                                                    |
|  - proto_ops (SOCK_STREAM → tcp_prot)                                 |
+-----------------------------------------------------------------------+
    |
    | tcp_sendmsg() / udp_sendmsg()
    v
+-----------------------------------------------------------------------+
|                     TRANSPORT LAYER (TCP/UDP)                          |
|  - Segmentation, reassembly                                            |
|  - Congestion control, retransmission                                  |
|  - sk_buff creation                                                    |
+-----------------------------------------------------------------------+
    |
    | ip_queue_xmit()
    v
+-----------------------------------------------------------------------+
|                     NETWORK LAYER (IP)                                 |
|  - Routing decision (dst_entry)                                        |
|  - Fragmentation                                                       |
|  - IP header construction                                              |
+-----------------------------------------------------------------------+
    |
    | dst->output() → ip_output() → ip_finish_output()
    v
+-----------------------------------------------------------------------+
|                     DEVICE-AGNOSTIC LAYER                              |
|  - dev_queue_xmit()                                                    |
|  - Qdisc processing (traffic shaping)                                  |
|  - Queue selection                                                     |
+-----------------------------------------------------------------------+
    |
    | dev->netdev_ops->ndo_start_xmit(skb, dev)
    v
+-----------------------------------------------------------------------+
|                     DRIVER LAYER (net_device)                          |
|  - DMA mapping                                                         |
|  - Hardware register access                                            |
|  - Interrupt handling                                                  |
+-----------------------------------------------------------------------+
    |
    | Hardware TX
    v
+-----------------------------------------------------------------------+
|                     PHYSICAL HARDWARE (NIC)                            |
+-----------------------------------------------------------------------+

DIRECTION:
    ↓ TX: Socket → Transport → IP → Device → NIC
    ↑ RX: NIC → Device → IP → Transport → Socket
```

**中文解释：**
- **Socket层**：处理用户空间系统调用，管理连接状态
- **传输层**：TCP分段/重组，拥塞控制，可靠传输
- **网络层**：路由决策，IP头构建，分片
- **设备无关层**：排队规则(qdisc)，流量整形
- **驱动层**：DMA操作，硬件寄存器访问
- **物理硬件**：实际网卡

---

## Phase 2 — Ops-Based Polymorphism

### 2.1 struct net_device_ops Overview

```c
/* include/linux/netdevice.h */
struct net_device_ops {
    /*
     * ===== LIFECYCLE CALLBACKS =====
     */
    int  (*ndo_init)(struct net_device *dev);
    void (*ndo_uninit)(struct net_device *dev);
    int  (*ndo_open)(struct net_device *dev);
    int  (*ndo_stop)(struct net_device *dev);
    
    /*
     * ===== DATA PATH (HOT PATH) =====
     */
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
                                   struct net_device *dev);
    u16  (*ndo_select_queue)(struct net_device *dev,
                             struct sk_buff *skb);
    
    /*
     * ===== CONFIGURATION (SLOW PATH) =====
     */
    void (*ndo_set_rx_mode)(struct net_device *dev);
    int  (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int  (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
    int  (*ndo_do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);
    
    /*
     * ===== STATISTICS =====
     */
    struct rtnl_link_stats64* (*ndo_get_stats64)(struct net_device *dev,
                                                  struct rtnl_link_stats64 *storage);
    
    /*
     * ===== ERROR HANDLING =====
     */
    void (*ndo_tx_timeout)(struct net_device *dev);
    
    /* ... many more callbacks ... */
};
```

### 2.2 Critical Callbacks Analysis

#### ndo_open: Device Activation

```
+------------------------------------------------------------------------+
|                    ndo_open() CONTRACT                                  |
+------------------------------------------------------------------------+

CALLER: dev_open() in net/core/dev.c
        Called when: `ip link set eth0 up` or socket bind

CONTEXT: Process context, may sleep
         RTNL lock held

MUST DO:
    1. Request and enable IRQ
    2. Allocate DMA resources (rings, buffers)
    3. Initialize hardware registers
    4. Start NAPI if using NAPI
    5. netif_start_queue() to enable TX

MUST NOT DO:
    - Assume device is already configured
    - Leave device in inconsistent state on error

RETURN:
    0       = Success
    -ERRNO  = Failure (device stays down)
```

**Example Implementation:**

```c
/* Typical ndo_open implementation */
static int my_driver_open(struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    int err;
    
    /* [1] Allocate DMA resources */
    err = alloc_dma_rings(priv);
    if (err)
        return err;
    
    /* [2] Request IRQ */
    err = request_irq(dev->irq, my_irq_handler, 
                      IRQF_SHARED, dev->name, dev);
    if (err)
        goto err_free_dma;
    
    /* [3] Initialize hardware */
    my_hw_init(priv);
    
    /* [4] Enable NAPI */
    napi_enable(&priv->napi);
    
    /* [5] Allow transmit */
    netif_start_queue(dev);  /* [KEY] Enables ndo_start_xmit calls */
    
    return 0;

err_free_dma:
    free_dma_rings(priv);
    return err;
}
```

#### ndo_stop: Device Deactivation

```
+------------------------------------------------------------------------+
|                    ndo_stop() CONTRACT                                  |
+------------------------------------------------------------------------+

CALLER: dev_close() in net/core/dev.c
        Called when: `ip link set eth0 down`

CONTEXT: Process context, may sleep
         RTNL lock held

MUST DO:
    1. netif_stop_queue() to block new TX
    2. napi_disable() to stop polling
    3. Disable hardware interrupts
    4. Free IRQ
    5. Free DMA resources
    6. Wait for in-flight operations to complete

MUST NOT DO:
    - Leave resources allocated
    - Leave interrupts enabled
    - Return while DMA is active

RETURN:
    0 = Always succeeds (by convention)
```

#### ndo_start_xmit: The HOT PATH

```
+------------------------------------------------------------------------+
|                    ndo_start_xmit() CONTRACT                            |
+------------------------------------------------------------------------+

CALLER: dev_hard_start_xmit() in net/core/dev.c
        Called when: Packet ready to transmit

CONTEXT: *** SOFTIRQ CONTEXT *** (NET_TX_SOFTIRQ)
         BH disabled, preempt disabled
         CANNOT SLEEP!

WHO OWNS skb:
    - On entry: CALLER owns skb
    - On NETDEV_TX_OK: DRIVER owns skb (must free eventually)
    - On NETDEV_TX_BUSY: CALLER still owns skb

MUST DO:
    1. Map skb data for DMA
    2. Write descriptors to TX ring
    3. Notify hardware (doorbell)
    4. Return immediately (async operation)

MUST NOT DO:
    - Sleep (GFP_KERNEL, mutex_lock, etc.)
    - Hold spinlock too long
    - Modify skb after returning TX_OK
    - Free skb synchronously (use completion interrupt)

RETURN:
    NETDEV_TX_OK   = Driver accepted skb, will free later
    NETDEV_TX_BUSY = Queue full, caller should retry
```

**Example Implementation:**

```c
/* High-performance ndo_start_xmit */
static netdev_tx_t my_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    struct tx_ring *ring = &priv->tx_ring;
    struct tx_desc *desc;
    dma_addr_t dma;
    
    /* [1] Check queue space */
    if (unlikely(!my_tx_avail(ring))) {
        netif_stop_queue(dev);           /* [KEY] Stop queue before return */
        return NETDEV_TX_BUSY;           /* [KEY] Caller retains ownership */
    }
    
    /* [2] Map for DMA - CANNOT FAIL in softirq! */
    dma = dma_map_single(priv->dma_dev, skb->data, skb->len, DMA_TO_DEVICE);
    if (unlikely(dma_mapping_error(priv->dma_dev, dma))) {
        dev_kfree_skb_any(skb);          /* [KEY] Free on error */
        return NETDEV_TX_OK;             /* [KEY] Return OK (we handled it) */
    }
    
    /* [3] Fill descriptor */
    desc = &ring->desc[ring->tail];
    desc->addr = dma;
    desc->len = skb->len;
    desc->skb = skb;                     /* [KEY] Save for completion */
    
    /* [4] Memory barrier before doorbell */
    wmb();
    
    /* [5] Ring doorbell */
    ring->tail = (ring->tail + 1) % RING_SIZE;
    writel(ring->tail, priv->regs + TX_DOORBELL);
    
    return NETDEV_TX_OK;                 /* [KEY] Driver owns skb now */
}
```

### 2.3 Ops Pattern Summary

```
+------------------------------------------------------------------------+
|                    net_device_ops DISPATCH PATTERN                      |
+------------------------------------------------------------------------+

Generic Code (dev.c):
    int dev_open(struct net_device *dev)
    {
        const struct net_device_ops *ops = dev->netdev_ops;
        
        /* [DISPATCH] Call driver-specific implementation */
        if (ops->ndo_open)
            ret = ops->ndo_open(dev);
        
        return ret;
    }

Driver Registration:
    static const struct net_device_ops my_netdev_ops = {
        .ndo_open       = my_open,
        .ndo_stop       = my_stop,
        .ndo_start_xmit = my_xmit,
        .ndo_set_rx_mode = my_set_rx_mode,
        /* NULL for unsupported operations */
    };
    
    dev->netdev_ops = &my_netdev_ops;

PATTERN:
    object->ops->method(object, args...)
    
    - Generic code NEVER knows which driver runs
    - Driver fills ops table at registration
    - NULL means "not supported" or "use default"
```

---

## Phase 3 — sk_buff Ownership Model

### 3.1 What sk_buff Represents

```
+------------------------------------------------------------------------+
|                    sk_buff STRUCTURE                                    |
+------------------------------------------------------------------------+

struct sk_buff represents ONE PACKET traversing the network stack.

+---------------+
|   sk_buff     |  <-- Metadata (240 bytes in v3.2)
|---------------|
| next, prev    |  <-- List linkage (queues)
| tstamp        |  <-- Timestamp
| sk            |  <-- Owning socket (if any)
| dev           |  <-- Associated net_device
| cb[48]        |  <-- Per-layer control block
| len           |  <-- Total data length
| data_len      |  <-- Non-linear data length
| protocol      |  <-- L3 protocol (ETH_P_IP, etc.)
| users         |  <-- Reference count
|---------------|
| head          |-----+
| data          |-----|-----> ACTUAL PACKET DATA
| tail          |-----|        +------------------+
| end           |-----+        | headroom        |
+---------------+              |------------------|
                               | MAC header       |
                               | IP header        |
                               | TCP header       |
                               | payload          |
                               |------------------|
                               | tailroom         |
                               +------------------+

MEMORY LAYOUT:
    head                  data                tail              end
      |                     |                  |                 |
      v                     v                  v                 v
    +-------+---------------+------------------+-----------------+
    |headroom| headers + payload (skb->len)  |    tailroom     |
    +-------+---------------+------------------+-----------------+
    
    headroom: Space to prepend headers (going down stack)
    tailroom: Space to append data
```

**中文解释：**
- **sk_buff**：代表一个网络数据包，包含元数据和数据指针
- **head/data/tail/end**：指向实际数据区域的不同位置
- **headroom**：用于在数据前添加协议头（如封装时）
- **tailroom**：用于在数据后添加内容
- **cb[48]**：每层协议的私有控制块，可被当前所有者自由使用

### 3.2 Ownership Rules

```
+------------------------------------------------------------------------+
|                    sk_buff OWNERSHIP RULES                              |
+------------------------------------------------------------------------+

RULE 1: EXACTLY ONE OWNER AT ANY TIME
    - Owner may read, modify, or free the skb
    - Non-owners MUST NOT access skb
    
RULE 2: OWNERSHIP TRANSFER IS EXPLICIT
    - Calling a function often transfers ownership
    - Check documentation for each function
    
RULE 3: REFERENCE COUNT (users) TRACKS SHARING
    - users = 1: Single owner, may modify freely
    - users > 1: Shared, must clone before modify

OWNERSHIP TRANSFER EXAMPLES:

netif_rx(skb):
    BEFORE: Caller owns skb
    AFTER:  Kernel network stack owns skb
    Caller MUST NOT touch skb after call!
    
dev_queue_xmit(skb):
    BEFORE: Caller owns skb
    AFTER:  Driver owns skb (on TX_OK)
    Caller MUST NOT touch skb after call!

skb_clone(skb):
    Creates NEW skb sharing same data
    Original: Still owned by caller
    Clone:    Owned by caller (new reference)
    Both share data (read-only until users=1)
```

**Ownership State Machine:**

```
+------------------------------------------------------------------------+
|                    sk_buff OWNERSHIP STATE MACHINE                      |
+------------------------------------------------------------------------+

                    alloc_skb()
                         |
                         v
                 +---------------+
                 |   ALLOCATED   |  users = 1
                 | Owner: Caller |
                 +-------+-------+
                         |
        +----------------+----------------+
        |                                 |
        | netif_rx(skb)                   | kfree_skb(skb)
        v                                 v
+---------------+                 +---------------+
|   QUEUED      |                 |    FREED      |
| Owner: Stack  |                 |               |
+-------+-------+                 +---------------+
        |
        | Protocol processing
        v
+---------------+
|   DELIVERED   |
| Owner: Socket |
+-------+-------+
        |
        | consume_skb() or kfree_skb()
        v
+---------------+
|    FREED      |
+---------------+

CRITICAL: Once ownership transfers, original holder CANNOT:
    - Read skb fields
    - Modify skb
    - Free skb
    
    The skb may be freed immediately by new owner!
```

### 3.3 Why Violating Ownership Crashes Systems

```
+------------------------------------------------------------------------+
|                    OWNERSHIP VIOLATION BUGS                             |
+------------------------------------------------------------------------+

BUG 1: USE-AFTER-FREE
    void broken_transmit(struct sk_buff *skb) {
        dev_queue_xmit(skb);      /* Ownership transferred to driver */
        printk("Sent %d bytes\n", skb->len);  /* [BUG] skb may be freed! */
    }
    
    RESULT: Read garbage or crash
    
BUG 2: DOUBLE-FREE
    void broken_receive(struct sk_buff *skb) {
        netif_rx(skb);            /* Ownership transferred to stack */
        kfree_skb(skb);           /* [BUG] Double free! */
    }
    
    RESULT: Memory corruption, crash

BUG 3: CONCURRENT MODIFICATION
    void broken_forward(struct sk_buff *skb) {
        struct sk_buff *clone = skb_clone(skb, GFP_ATOMIC);
        netif_rx(clone);          /* Stack processing clone */
        skb->data[0] = 0x42;      /* [BUG] Modifying shared data! */
        dev_queue_xmit(skb);
    }
    
    RESULT: Corrupted packet on RX side
    
CORRECT PATTERN:
    void correct_forward(struct sk_buff *skb) {
        struct sk_buff *clone = skb_clone(skb, GFP_ATOMIC);
        netif_rx(clone);
        
        /* Make skb writable before modification */
        if (skb_cloned(skb)) {
            struct sk_buff *new = skb_copy(skb, GFP_ATOMIC);
            kfree_skb(skb);
            skb = new;
        }
        skb->data[0] = 0x42;      /* Now safe */
        dev_queue_xmit(skb);
    }
```

### 3.4 Reference vs Clone Semantics

```
+------------------------------------------------------------------------+
|                    REFERENCE vs CLONE                                   |
+------------------------------------------------------------------------+

skb_get(skb):
    - Increments users count
    - Both references point to SAME skb
    - Both must call kfree_skb()
    
    Original: +-------+     users = 2
              |  skb  |<----+
              +-------+     |
    skb_get:  +-------+-----+
              | ref   |

skb_clone(skb):
    - Creates NEW sk_buff header
    - SHARES underlying data (copy-on-write)
    - Each has independent control (len, headers, cb)
    
    Original: +-------+
              |  skb  |----> +--------+
              +-------+      |  DATA  |  shared_info.dataref++
    Clone:    +-------+      +--------+
              | clone |------^
              +-------+

skb_copy(skb):
    - Creates completely INDEPENDENT copy
    - New header AND new data
    - Most expensive, but fully isolated
    
    Original: +-------+----> +--------+
              |  skb  |      | DATA 1 |
              +-------+      +--------+
    
    Copy:     +-------+----> +--------+
              | copy  |      | DATA 2 |  <-- Separate copy
              +-------+      +--------+

WHEN TO USE WHAT:
    skb_get():   Need reference, won't modify (rare)
    skb_clone(): Need to queue/deliver, may modify headers only
    skb_copy():  Need to modify data
```

---

## Phase 4 — TCP FSM as Architecture

### 4.1 TCP State Machine Location

```
+------------------------------------------------------------------------+
|                    TCP STATE DEFINITION                                 |
+------------------------------------------------------------------------+

/* include/net/tcp_states.h */
enum {
    TCP_ESTABLISHED = 1,   /* Data transfer state */
    TCP_SYN_SENT,          /* SYN sent, waiting for SYN-ACK */
    TCP_SYN_RECV,          /* SYN received, SYN-ACK sent */
    TCP_FIN_WAIT1,         /* FIN sent, waiting for ACK or FIN */
    TCP_FIN_WAIT2,         /* FIN ACKed, waiting for FIN */
    TCP_TIME_WAIT,         /* Waiting for old segments to expire */
    TCP_CLOSE,             /* Socket closed */
    TCP_CLOSE_WAIT,        /* Remote FIN received, waiting for close() */
    TCP_LAST_ACK,          /* FIN sent after remote FIN, waiting for ACK */
    TCP_LISTEN,            /* Listening for connections */
    TCP_CLOSING,           /* Both sides sent FIN simultaneously */
    TCP_MAX_STATES
};

WHY CENTRALIZED:
    1. RFC 793 compliance - states are PROTOCOL-DEFINED
    2. Multiple files need state checks
    3. Statistics and debugging reference same enum
    4. Bitmap operations (TCPF_*) for state tests
```

### 4.2 State Transition Diagram

```
+------------------------------------------------------------------------+
|                    TCP STATE MACHINE                                    |
+------------------------------------------------------------------------+

                             CLOSED
                               |
              +----------------+----------------+
              |                                 |
         passive open                      active open
         listen()                          connect()
              |                                 |
              v                                 v
           LISTEN                          SYN_SENT
              |                                 |
         rcv SYN                           rcv SYN+ACK
         snd SYN+ACK                       snd ACK
              |                                 |
              v                                 v
          SYN_RCVD --------------------------> ESTABLISHED
                    rcv ACK                         |
                                                    |
              +-------------------------------------+
              |                                     |
         active close                          passive close
         close()                               rcv FIN
         snd FIN                               snd ACK
              |                                     |
              v                                     v
         FIN_WAIT_1                            CLOSE_WAIT
              |                                     |
         +----+----+                           close()
         |         |                           snd FIN
    rcv ACK   rcv FIN+ACK                          |
         |    snd ACK                              v
         v         |                           LAST_ACK
    FIN_WAIT_2     |                               |
         |         |                           rcv ACK
    rcv FIN        |                               |
    snd ACK        |                               v
         |         +---> CLOSING                 CLOSED
         |                   |
         |              rcv ACK
         v                   |
    TIME_WAIT <--------------+
         |
    2MSL timeout
         |
         v
       CLOSED
```

**中文解释：**
- **主动打开**：客户端调用`connect()`，发送SYN，进入`SYN_SENT`
- **被动打开**：服务器调用`listen()`，进入`LISTEN`状态
- **三次握手**：`SYN` → `SYN+ACK` → `ACK`，双方进入`ESTABLISHED`
- **主动关闭**：调用`close()`发送FIN，经过`FIN_WAIT_1/2`
- **被动关闭**：收到FIN进入`CLOSE_WAIT`，发送FIN后进入`LAST_ACK`
- **TIME_WAIT**：等待2MSL确保旧报文过期

### 4.3 Why TCP FSM is Split Across Files

```
+------------------------------------------------------------------------+
|                    TCP CODE ORGANIZATION                                |
+------------------------------------------------------------------------+

tcp_states.h:
    - State DEFINITIONS (enum)
    - Used by all TCP files
    - RFC compliance

tcp_input.c:
    - RX path transitions
    - tcp_rcv_state_process(): Main RX state machine
    - Handles: SYN, ACK, FIN, RST
    - ~6000 lines of complex state handling

tcp_output.c:
    - TX path transitions
    - tcp_connect(): SYN_SENT transition
    - tcp_close(): FIN_WAIT transitions
    - Segment construction

tcp.c:
    - tcp_set_state(): CENTRALIZED STATE MUTATION
    - Socket API (tcp_sendmsg, tcp_recvmsg)
    - Statistics update on state change

tcp_timer.c:
    - Timer-driven transitions
    - Retransmit timeout
    - TIME_WAIT expiration
    - Keep-alive timeout

WHY SPLIT:
    1. RX and TX are independent code paths
    2. Each file is already ~2000-6000 lines
    3. Different locking requirements
    4. Separation of concerns

CRITICAL: All state changes go through tcp_set_state()!
```

### 4.4 Centralized State Mutation

```c
/* net/ipv4/tcp.c */
void tcp_set_state(struct sock *sk, int state)
{
    int oldstate = sk->sk_state;
    
    /* [1] Handle state-specific side effects */
    switch (state) {
    case TCP_ESTABLISHED:
        if (oldstate != TCP_ESTABLISHED)
            TCP_INC_STATS(sock_net(sk), TCP_MIB_CURRESTAB);
        break;
    
    case TCP_CLOSE:
        if (oldstate == TCP_CLOSE_WAIT || oldstate == TCP_ESTABLISHED)
            TCP_INC_STATS(sock_net(sk), TCP_MIB_ESTABRESETS);
        
        /* [KEY] Unhash BEFORE state change */
        sk->sk_prot->unhash(sk);
        
        if (inet_csk(sk)->icsk_bind_hash &&
            !(sk->sk_userlocks & SOCK_BINDPORT_LOCK))
            inet_put_port(sk);
        /* fall through */
    
    default:
        if (oldstate == TCP_ESTABLISHED)
            TCP_DEC_STATS(sock_net(sk), TCP_MIB_CURRESTAB);
    }
    
    /* [2] Change state AFTER socket is unhashed */
    sk->sk_state = state;
    
    /* [3] Debug tracing */
#ifdef STATE_TRACE
    SOCK_DEBUG(sk, "TCP sk=%p, State %s -> %s\n",
               sk, statename[oldstate], statename[state]);
#endif
}
```

**Why Centralized:**

```
+------------------------------------------------------------------------+
|                    CENTRALIZED STATE MUTATION BENEFITS                  |
+------------------------------------------------------------------------+

1. SINGLE POINT OF CONTROL
   - All transitions go through one function
   - Easy to add logging, tracing, statistics
   
2. INVARIANT ENFORCEMENT
   - Unhash before CLOSE (prevents new connections)
   - Statistics always accurate
   
3. AUDITING
   - grep "tcp_set_state" finds all transitions
   - Easy to verify RFC compliance
   
4. DEBUGGING
   - STATE_TRACE shows all transitions
   - No hidden state changes

ANTI-PATTERN (what NOT to do):
    /* BAD: Direct state assignment scattered everywhere */
    sk->sk_state = TCP_ESTABLISHED;  /* Missing stats update! */
    sk->sk_state = TCP_CLOSE;        /* Missing unhash! */
```

---

## Phase 5 — Fast Path vs Slow Path

### 5.1 Fast Path Identification

```
+------------------------------------------------------------------------+
|                    FAST PATH vs SLOW PATH                               |
+------------------------------------------------------------------------+

FAST PATH (HOT PATH):
    - Runs for EVERY PACKET
    - Must be extremely optimized
    - No sleeping, minimal locking
    - Branch prediction hints (likely/unlikely)

SLOW PATH (COLD PATH):
    - Runs for CONFIGURATION CHANGES
    - May sleep, take mutexes
    - Not performance-critical
    
RX FAST PATH:
    NIC IRQ → napi_schedule()
           → napi_poll() [softirq]
               → driver rx
               → netif_receive_skb()
               → __netif_receive_skb()
               → ip_rcv()
               → tcp_v4_rcv()
               → tcp_rcv_established()  [FAST]
                         |
                         v
                   tcp_data_queue()
                         |
                         v
                   sk_data_ready() → wake user

TX FAST PATH:
    tcp_sendmsg()
         |
         v
    tcp_write_xmit()
         |
         v
    ip_queue_xmit()
         |
         v
    dev_queue_xmit()
         |
         v
    netdev_ops->ndo_start_xmit()  [softirq]
         |
         v
    Hardware TX
```

### 5.2 Fast Path Optimizations

```c
/* Example: Fast path in tcp_rcv_established() */
int tcp_rcv_established(struct sock *sk, struct sk_buff *skb,
                        const struct tcphdr *th, unsigned int len)
{
    struct tcp_sock *tp = tcp_sk(sk);
    
    /*
     * [FAST PATH PREDICTION]
     * Most packets are:
     * - In-order ACKs with data
     * - Expected sequence number
     * - No special flags
     */
    if ((tcp_flag_word(th) & TCP_HP_BITS) == tp->pred_flags &&
        TCP_SKB_CB(skb)->seq == tp->rcv_nxt &&
        !after(TCP_SKB_CB(skb)->ack_seq, tp->snd_nxt))
    {
        int tcp_header_len = tp->tcp_header_len;
        
        /* [FAST] Header prediction hit */
        if (tcp_header_len == th->doff << 2) {
            if (!skb_shinfo(skb)->gso_segs)
                __skb_pull(skb, tcp_header_len);
            
            /* [FAST] Queue data directly */
            tcp_queue_rcv(sk, skb, tcp_header_len);
            
            /* [FAST] Send ACK */
            tcp_ack(sk, th, TCP_SKB_CB(skb)->ack_seq);
            
            return 0;  /* Success, fast path complete */
        }
    }
    
    /* [SLOW PATH] Complex cases: OOO, retransmit, etc. */
    return tcp_rcv_slow_path(sk, skb, th, len);
}
```

**Fast Path Techniques:**

```
+------------------------------------------------------------------------+
|                    FAST PATH OPTIMIZATION TECHNIQUES                    |
+------------------------------------------------------------------------+

1. PREDICTION
   - Guess common case (in-order packet)
   - Check with single comparison (pred_flags)
   - Fall through to slow path if wrong
   
2. BRANCH HINTS
   if (likely(fast_case)) {    /* likely() = __builtin_expect */
       /* fast path */
   } else {
       /* slow path */
   }
   
3. CACHE LOCALITY
   - Hot fields in sk_buff first
   - Prefetch next skb while processing current
   - Per-CPU data structures
   
4. LOCK-FREE WHERE POSSIBLE
   - RCU for read-mostly data
   - Per-CPU queues
   - Atomic operations
   
5. BATCHING
   - NAPI: Poll multiple packets per interrupt
   - GRO: Merge packets before stack
   - TSO/GSO: Offload segmentation
```

### 5.3 Why Fast and Slow Paths Must Not Mix

```
+------------------------------------------------------------------------+
|                    PATH SEPARATION RULES                                |
+------------------------------------------------------------------------+

RULE 1: FAST PATH MUST NOT SLEEP
    /* [BUG] Sleeping in softirq context */
    netdev_tx_t broken_xmit(struct sk_buff *skb, struct net_device *dev)
    {
        mutex_lock(&priv->lock);      /* [BUG] May sleep in softirq! */
        /* ... */
        mutex_unlock(&priv->lock);
        return NETDEV_TX_OK;
    }
    
    RESULT: BUG, scheduling while atomic

RULE 2: SLOW PATH MUST NOT HOLD FAST PATH LOCKS
    /* [BUG] Configuration holding spinlock too long */
    int broken_set_mtu(struct net_device *dev, int new_mtu)
    {
        spin_lock_bh(&dev->tx_lock);
        /* ... slow reconfiguration ... */
        msleep(100);                  /* [BUG] Sleeping with spinlock! */
        spin_unlock_bh(&dev->tx_lock);
    }
    
    RESULT: Deadlock or massive latency

RULE 3: CONFIGURATION MUST SYNCHRONIZE WITH DATA PATH
    /* CORRECT: Quiesce data path before reconfiguration */
    int correct_set_mtu(struct net_device *dev, int new_mtu)
    {
        netif_tx_disable(dev);        /* Stop TX */
        napi_disable(&priv->napi);    /* Stop RX */
        synchronize_net();            /* Wait for in-flight packets */
        
        /* Now safe to reconfigure */
        dev->mtu = new_mtu;
        
        napi_enable(&priv->napi);     /* Resume RX */
        netif_tx_start_all_queues(dev); /* Resume TX */
    }
```

---

## Phase 6 — Common Bugs & Lessons

### 6.1 Sleeping in Softirq Context

```
+------------------------------------------------------------------------+
|                    BUG: SLEEPING IN SOFTIRQ                             |
+------------------------------------------------------------------------+

CONTEXT RULES:
    IRQ Context:      in_irq() == true, CANNOT sleep
    Softirq Context:  in_softirq() == true, CANNOT sleep
    Process Context:  Can sleep

COMMON MISTAKES:
    1. GFP_KERNEL in ndo_start_xmit()
    2. mutex_lock() in NAPI poll
    3. copy_from_user() in packet processing
    
EXAMPLE BUG:
    static int broken_napi_poll(struct napi_struct *napi, int budget)
    {
        struct sk_buff *skb;
        
        while ((skb = get_next_skb())) {
            /* [BUG] copy_to_user may sleep! */
            copy_to_user(user_buf, skb->data, skb->len);
        }
    }
    
CORRECT APPROACH:
    - Use GFP_ATOMIC for memory allocation
    - Use spin_lock() instead of mutex
    - Queue to process context if sleeping needed:
    
    static int correct_napi_poll(struct napi_struct *napi, int budget)
    {
        struct sk_buff *skb;
        
        while ((skb = get_next_skb())) {
            /* Queue to socket buffer, wake process */
            sock_queue_rcv_skb(sk, skb);
        }
    }
    
    /* In process context, user calls recvmsg() */
    ssize_t tcp_recvmsg(...) {
        /* May sleep waiting for data */
        wait_event_interruptible(...);
        copy_to_user(...);  /* Safe here */
    }
```

### 6.2 Ownership Violations

```
+------------------------------------------------------------------------+
|                    BUG: OWNERSHIP VIOLATIONS                            |
+------------------------------------------------------------------------+

BUG 1: USE-AFTER-TRANSMIT
    static void broken_tx_complete(struct sk_buff *skb)
    {
        dev_queue_xmit(skb);
        priv->stats.tx_bytes += skb->len;  /* [BUG] skb freed! */
    }
    
    FIX:
    static void correct_tx_complete(struct sk_buff *skb)
    {
        int len = skb->len;  /* Save before transfer */
        dev_queue_xmit(skb);
        priv->stats.tx_bytes += len;
    }

BUG 2: SHARED SKB MODIFICATION
    static int broken_forward(struct sk_buff *skb)
    {
        struct sk_buff *clone = skb_clone(skb, GFP_ATOMIC);
        
        netif_rx(clone);          /* Stack owns clone */
        
        /* [BUG] clone and skb share data! */
        skb_push(skb, ETH_HLEN);  /* Modifies shared data */
        
        return dev_queue_xmit(skb);
    }
    
    FIX:
    static int correct_forward(struct sk_buff *skb)
    {
        struct sk_buff *clone = skb_clone(skb, GFP_ATOMIC);
        
        netif_rx(clone);
        
        /* Make writable before modification */
        if (skb_cow_head(skb, ETH_HLEN) < 0) {
            kfree_skb(skb);
            return NET_XMIT_DROP;
        }
        
        skb_push(skb, ETH_HLEN);  /* Now safe */
        return dev_queue_xmit(skb);
    }
```

### 6.3 Mixing Control and Data Paths

```
+------------------------------------------------------------------------+
|                    BUG: PATH MIXING                                     |
+------------------------------------------------------------------------+

BUG: CONFIGURATION BLOCKS DATA PATH
    static void broken_config(struct net_device *dev)
    {
        /* Taking lock also used by ndo_start_xmit */
        spin_lock(&priv->tx_lock);
        
        /* Slow operation while holding fast-path lock */
        for (i = 0; i < 1000; i++) {
            msleep(1);  /* [BUG] 1 second of TX blocked! */
        }
        
        spin_unlock(&priv->tx_lock);
    }
    
RESULT:
    - All TX blocked for 1 second
    - TCP timeouts
    - Connection drops

CORRECT PATTERN:
    static void correct_config(struct net_device *dev)
    {
        /* [1] Quiesce data path */
        netif_tx_disable(dev);
        
        /* [2] Wait for in-flight operations */
        synchronize_rcu();
        
        /* [3] Now safe to do slow configuration */
        for (i = 0; i < 1000; i++) {
            msleep(1);  /* Safe, no data path */
        }
        
        /* [4] Resume data path */
        netif_tx_wake_all_queues(dev);
    }
```

### 6.4 Architecture Lessons Summary

```
+------------------------------------------------------------------------+
|                    REUSABLE ARCHITECTURE PATTERNS                       |
+------------------------------------------------------------------------+

PATTERN 1: OPS-BASED POLYMORPHISM
    struct device_ops {
        int (*init)(struct device *);
        int (*process)(struct device *, void *data);
        void (*cleanup)(struct device *);
    };
    
    - Generic code dispatches through ops
    - Implementations fill ops table
    - NULL = not supported

PATTERN 2: OWNERSHIP TRANSFER
    enum ownership { CALLER, SUBSYSTEM, FREED };
    
    - Document ownership in function contracts
    - Save needed fields BEFORE transfer
    - NEVER touch object after transfer

PATTERN 3: PATH SEPARATION
    - Identify hot path (per-packet)
    - Identify cold path (configuration)
    - Never mix their locks
    - Quiesce hot path for cold operations

PATTERN 4: REFERENCE COUNTING
    struct object {
        atomic_t refcount;
    };
    
    - Clone for shared read
    - Copy for independent modification
    - get/put for temporary references

PATTERN 5: STATE MACHINE CENTRALIZATION
    void set_state(struct object *obj, int new_state) {
        /* All side effects here */
        /* Statistics, unhash, cleanup */
        obj->state = new_state;
    }
    
    - Single point of state mutation
    - Easy to audit, debug, trace
```

---

## Appendix A — User-Space High-Performance Patterns

### A.1 Ownership-Based Packet Pipeline

```c
/*
 * packet_pipeline.c - Ownership-based zero-copy packet processing
 *
 * Demonstrates:
 * 1. Explicit ownership transfer
 * 2. Reference counting
 * 3. Fast path / slow path separation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>

/*
 * ============================================================
 * PACKET BUFFER (like sk_buff)
 * ============================================================
 */

#define PKT_SIZE 2048

struct packet {
    atomic_int refcount;          /* [REF] Reference count */
    atomic_int data_refcount;     /* [REF] Data sharing count */
    size_t len;                   /* [DATA] Packet length */
    unsigned char *data;          /* [DATA] Pointer to payload */
    unsigned char buf[PKT_SIZE];  /* [DATA] Inline buffer */
    void (*destructor)(struct packet *); /* [CALLBACK] Cleanup */
};

/* Allocate new packet - caller owns */
struct packet *packet_alloc(void) {
    struct packet *pkt = calloc(1, sizeof(*pkt));
    if (!pkt) return NULL;
    
    atomic_store(&pkt->refcount, 1);
    atomic_store(&pkt->data_refcount, 1);
    pkt->data = pkt->buf;
    pkt->len = 0;
    return pkt;
}

/* Get reference - caller gets shared ownership */
struct packet *packet_get(struct packet *pkt) {
    atomic_fetch_add(&pkt->refcount, 1);
    return pkt;
}

/* Put reference - may free if last */
void packet_put(struct packet *pkt) {
    if (atomic_fetch_sub(&pkt->refcount, 1) == 1) {
        if (pkt->destructor)
            pkt->destructor(pkt);
        free(pkt);
    }
}

/* Clone packet - new header, shared data */
struct packet *packet_clone(struct packet *pkt) {
    struct packet *clone = calloc(1, sizeof(*clone));
    if (!clone) return NULL;
    
    atomic_store(&clone->refcount, 1);
    clone->len = pkt->len;
    clone->data = pkt->data;  /* Share data */
    
    /* Track shared data */
    atomic_fetch_add(&pkt->data_refcount, 1);
    
    return clone;
}

/* Check if data is exclusively owned */
int packet_writable(struct packet *pkt) {
    return atomic_load(&pkt->data_refcount) == 1;
}

/* Make packet writable (copy if shared) */
int packet_make_writable(struct packet *pkt) {
    if (packet_writable(pkt))
        return 0;  /* Already writable */
    
    /* Copy data */
    unsigned char *new_buf = malloc(pkt->len);
    if (!new_buf) return -1;
    
    memcpy(new_buf, pkt->data, pkt->len);
    pkt->data = new_buf;
    atomic_store(&pkt->data_refcount, 1);
    
    return 0;
}

/*
 * ============================================================
 * PIPELINE STAGES (demonstrate ownership transfer)
 * ============================================================
 */

typedef void (*stage_fn)(struct packet *pkt);

struct pipeline_stage {
    const char *name;
    stage_fn process;
    struct pipeline_stage *next;
};

/* Stage that CONSUMES packet (takes ownership) */
void stage_consumer(struct packet *pkt) {
    printf("[CONSUMER] Processing %zu bytes, freeing packet\n", pkt->len);
    packet_put(pkt);  /* [KEY] Consumer frees packet */
}

/* Stage that FORWARDS packet (transfers ownership) */
void stage_forwarder(struct packet *pkt) {
    printf("[FORWARDER] Forwarding %zu bytes\n", pkt->len);
    /* Ownership passes to next stage via return */
}

/* Stage that CLONES for multipath (shared data) */
void stage_multicast(struct packet *pkt) {
    printf("[MULTICAST] Cloning packet for multiple paths\n");
    
    struct packet *clone1 = packet_clone(pkt);
    struct packet *clone2 = packet_clone(pkt);
    
    /* Original and clones share data - read only */
    printf("  Clone 1: data=%p\n", clone1->data);
    printf("  Clone 2: data=%p (same address = shared)\n", clone2->data);
    
    packet_put(clone1);
    packet_put(clone2);
    packet_put(pkt);  /* Original also consumed */
}

/* Stage that MODIFIES packet (must copy if shared) */
void stage_modifier(struct packet *pkt) {
    printf("[MODIFIER] Modifying packet\n");
    
    if (!packet_writable(pkt)) {
        printf("  Packet is shared, copying data first\n");
        packet_make_writable(pkt);
    }
    
    /* Now safe to modify */
    pkt->data[0] = 0x42;
    
    packet_put(pkt);  /* Consume after processing */
}

/*
 * ============================================================
 * FAST PATH / SLOW PATH SEPARATION
 * ============================================================
 */

/* Simulated fast-path packet queue */
#define QUEUE_SIZE 1024
struct packet_queue {
    struct packet *ring[QUEUE_SIZE];
    atomic_uint head;
    atomic_uint tail;
};

/* [FAST PATH] Enqueue without locking (single producer) */
int queue_enqueue_fast(struct packet_queue *q, struct packet *pkt) {
    unsigned int tail = atomic_load(&q->tail);
    unsigned int next = (tail + 1) % QUEUE_SIZE;
    
    if (next == atomic_load(&q->head))
        return -1;  /* Full */
    
    q->ring[tail] = pkt;  /* [KEY] Ownership transfer to queue */
    atomic_store(&q->tail, next);
    return 0;
}

/* [FAST PATH] Dequeue without locking (single consumer) */
struct packet *queue_dequeue_fast(struct packet_queue *q) {
    unsigned int head = atomic_load(&q->head);
    
    if (head == atomic_load(&q->tail))
        return NULL;  /* Empty */
    
    struct packet *pkt = q->ring[head];  /* [KEY] Ownership from queue */
    atomic_store(&q->head, (head + 1) % QUEUE_SIZE);
    return pkt;
}

/*
 * ============================================================
 * DEMONSTRATION
 * ============================================================
 */

int main(void) {
    printf("=== Ownership-Based Packet Pipeline Demo ===\n\n");
    
    /* Test 1: Basic ownership transfer */
    printf("--- Test 1: Ownership Transfer ---\n");
    {
        struct packet *pkt = packet_alloc();
        memcpy(pkt->data, "Hello", 5);
        pkt->len = 5;
        
        printf("Created packet, refcount=%d\n", 
               atomic_load(&pkt->refcount));
        
        stage_consumer(pkt);  /* Transfers ownership, pkt freed inside */
        /* pkt is INVALID here - must not access! */
    }
    
    printf("\n--- Test 2: Cloning and Sharing ---\n");
    {
        struct packet *pkt = packet_alloc();
        memcpy(pkt->data, "Multicast", 9);
        pkt->len = 9;
        
        stage_multicast(pkt);  /* Clones and consumes all */
    }
    
    printf("\n--- Test 3: Copy-on-Write ---\n");
    {
        struct packet *pkt = packet_alloc();
        memcpy(pkt->data, "Original", 8);
        pkt->len = 8;
        
        struct packet *clone = packet_clone(pkt);
        printf("Clone shares data: writable=%d\n", packet_writable(clone));
        
        stage_modifier(clone);  /* Will copy before modify */
        
        packet_put(pkt);  /* Original still valid */
    }
    
    printf("\n--- Test 4: Fast Path Queue ---\n");
    {
        struct packet_queue queue = {0};
        
        /* Producer (fast path) */
        for (int i = 0; i < 3; i++) {
            struct packet *pkt = packet_alloc();
            pkt->len = i + 1;
            queue_enqueue_fast(&queue, pkt);
            printf("Enqueued packet %d\n", i);
        }
        
        /* Consumer (fast path) */
        struct packet *pkt;
        while ((pkt = queue_dequeue_fast(&queue))) {
            printf("Dequeued packet, len=%zu\n", pkt->len);
            packet_put(pkt);  /* Consumer owns and frees */
        }
    }
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

### A.2 Architecture Diagram

```
+------------------------------------------------------------------------+
|                    USER-SPACE PACKET PIPELINE                           |
+------------------------------------------------------------------------+

    Producer                Queue                 Consumer
    (Fast Path)         (Lock-free)             (Fast Path)
        |                   |                       |
        | packet_alloc()    |                       |
        v                   |                       |
    +-------+               |                       |
    | pkt A |               |                       |
    +---+---+               |                       |
        |                   |                       |
        | enqueue (transfer)|                       |
        +------------------>|                       |
        |                   |                       |
        | (INVALID pkt A)   |<----------------------+
        |                   |     dequeue (receive) |
        |                   |                       v
        |               +---+---+               +-------+
        |               | pkt A |-------------->| pkt A | (consumer owns)
        |               +-------+               +---+---+
        |                   |                       |
        |                   |                       | packet_put()
        |                   |                       v
        |                   |                   (FREED)

OWNERSHIP RULES:
    1. packet_alloc() → caller owns
    2. enqueue → queue owns (caller loses)
    3. dequeue → caller owns (queue loses)
    4. packet_put() → may free if last ref
```

### A.3 Key Lessons for High-Performance Systems

| Kernel Pattern | User-Space Application |
|----------------|----------------------|
| **net_device_ops** | Plugin systems with function pointer tables |
| **sk_buff ownership** | Zero-copy message passing, ownership transfer |
| **Reference counting** | Shared buffer management, garbage collection |
| **Fast/slow path split** | Hot path optimization, configuration isolation |
| **TCP FSM centralization** | State machine with single mutation point |
| **Lock-free queues** | High-throughput producer-consumer systems |
| **Clone vs copy** | Copy-on-write for large data structures |
| **NAPI batching** | Amortize per-item overhead with batching |

```
+------------------------------------------------------------------------+
|                    DECISION FRAMEWORK                                   |
+------------------------------------------------------------------------+

IF you have per-packet processing:
    → Identify hot path, optimize aggressively
    → Never sleep or take slow locks
    → Use lock-free queues and batching

IF you have shared data:
    → Use reference counting
    → Clone for shared read, copy for write
    → Document ownership transfer clearly

IF you have configuration changes:
    → Quiesce data path before changes
    → Use RCU or generation counters
    → Never hold hot-path locks during slow ops

IF you have state machines:
    → Centralize state mutation
    → Log all transitions
    → Use switch for visible states
    → Use ops for behavioral states
```

---

## Summary

### Core Architectural Principles

1. **Ops-Based Polymorphism**
   - `net_device_ops` for hardware abstraction
   - Generic code never knows driver details
   - NULL = not supported

2. **Ownership Discipline**
   - sk_buff has exactly one owner
   - Transfer is explicit (document it!)
   - Clone for sharing, copy for modification

3. **Path Separation**
   - Fast path: softirq, no sleeping, minimal locks
   - Slow path: process context, may sleep
   - Never mix their synchronization

4. **Centralized State Mutation**
   - `tcp_set_state()` for all TCP transitions
   - Side effects (stats, unhash) in one place
   - Easy to audit and debug

5. **Reference Counting**
   - `users` in sk_buff
   - `refcount` in net_device
   - Match get/put, clone/free

### Performance Techniques

| Technique | Benefit |
|-----------|---------|
| **NAPI polling** | Batch interrupts, reduce overhead |
| **Header prediction** | Skip parsing for common packets |
| **Lock-free queues** | No contention on hot path |
| **Per-CPU data** | No cache bouncing |
| **RCU** | Read-mostly without locks |
| **GRO/GSO** | Reduce per-packet overhead |

These patterns have evolved over 20+ years of Linux networking development, handling millions of packets per second on commodity hardware.

