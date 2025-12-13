# Linux SKB (sk_buff) Internals: A Deep Dive

This document provides a comprehensive, code-level explanation of the Linux Socket Buffer (`struct sk_buff`) - the fundamental data structure for network packet handling in the Linux kernel.

---

## Table of Contents

1. [SKB Overview](#1-skb-overview)
2. [SKB Memory Layout](#2-skb-memory-layout)
3. [SKB Structure Field Breakdown](#3-skb-structure-field-breakdown)
4. [Linear vs Non-Linear SKBs](#4-linear-vs-non-linear-skbs)
5. [Protocol Header Offsets](#5-protocol-header-offsets)
6. [Zero-Copy Data Flow](#6-zero-copy-data-flow)
7. [SKB Lifecycle - TX Path](#7-skb-lifecycle---tx-path)
8. [SKB Lifecycle - RX Path](#8-skb-lifecycle---rx-path)
9. [Common SKB Helper APIs](#9-common-skb-helper-apis)
10. [SKB and RX/TX Rings Integration](#10-skb-and-rxtx-rings-integration)

---

## 1. SKB Overview

```
+============================================================================+
|                           SKB CONCEPTUAL MODEL                              |
+============================================================================+
|                                                                             |
|   The sk_buff (SKB) is the kernel's universal packet representation.       |
|   It serves multiple purposes:                                              |
|                                                                             |
|   +-------------------------------------------------------------------+    |
|   |                                                                    |    |
|   |   1. METADATA CONTAINER                                           |    |
|   |      - Packet length, checksums, timestamps                       |    |
|   |      - Protocol information, device references                    |    |
|   |      - Routing decisions, socket associations                     |    |
|   |                                                                    |    |
|   |   2. DATA BUFFER MANAGER                                          |    |
|   |      - Points to actual packet data                               |    |
|   |      - Supports linear and scattered data                         |    |
|   |      - Manages headroom and tailroom                              |    |
|   |                                                                    |    |
|   |   3. PROTOCOL STACK NAVIGATOR                                     |    |
|   |      - Header offset tracking (MAC, IP, Transport)                |    |
|   |      - Zero-copy header parsing                                   |    |
|   |      - Layer traversal without data copying                       |    |
|   |                                                                    |    |
|   |   4. QUEUE ELEMENT                                                |    |
|   |      - next/prev pointers for linking                             |    |
|   |      - Can be queued in socket, device, or scheduler queues       |    |
|   |                                                                    |    |
|   +-------------------------------------------------------------------+    |
|                                                                             |
+============================================================================+
```

**中文说明**：
- SKB（`sk_buff`）是 Linux 内核中网络数据包的通用表示结构
- 它同时承担四个角色：元数据容器、数据缓冲区管理器、协议栈导航器、队列元素
- 这种设计使得数据包能够高效地在协议栈各层之间传递，而无需复制数据

---

## 2. SKB Memory Layout

### 2.1 Basic Memory Structure

```
+============================================================================+
|                        SKB MEMORY LAYOUT (LINEAR)                           |
+============================================================================+
|                                                                             |
|   struct sk_buff                                                            |
|   (Metadata - allocated from slab cache)                                    |
|   +------------------------------------------+                              |
|   | next, prev (queue linkage)               |                              |
|   | sk, dev (socket, device)                 |                              |
|   | cb[48] (control buffer)                  |                              |
|   | len, data_len                            |                              |
|   | protocol, ip_summed                      |                              |
|   | mac_header, network_header               |                              |
|   | transport_header                         |                              |
|   +------------------------------------------+                              |
|   | head ----+                               |                              |
|   | data ----|-+                             |                              |
|   | tail ----|-|-+                           |                              |
|   | end  ----|-|-|-+                         |                              |
|   +----------|---|---|------------------------                              |
|              |   |   |                                                      |
|              v   v   v                                                      |
|                                                                             |
|   Data Buffer (allocated via kmalloc)                                       |
|   +---------------------------------------------------------------------+   |
|   |          |                                        |                 |   |
|   |<-------->|<-------------------------------------->|<--------------->|   |
|   | headroom |              packet data               |    tailroom     |   |
|   |          |                                        |                 |   |
|   +---------------------------------------------------------------------+   |
|   ^          ^                                        ^                 ^   |
|   |          |                                        |                 |   |
|   head      data                                     tail              end  |
|                                                                             |
|   At the end of data buffer:                                                |
|   +---------------------------------------------------------------------+   |
|   |                      struct skb_shared_info                         |   |
|   |  - nr_frags, gso_size, gso_segs                                    |   |
|   |  - frag_list (for chained SKBs)                                    |   |
|   |  - frags[MAX_SKB_FRAGS] (page fragments)                           |   |
|   |  - dataref (reference count)                                       |   |
|   +---------------------------------------------------------------------+   |
|   ^                                                                         |
|   |                                                                         |
|   Located at: skb->end (or skb->head + skb->end on 64-bit)                 |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **sk_buff 结构体**：存储元数据，从 slab 缓存分配
- **数据缓冲区**：通过 kmalloc 分配，包含实际的数据包内容
- **headroom**：data 指针之前的空间，用于在包前添加头部
- **tailroom**：tail 指针之后的空间，用于追加数据
- **skb_shared_info**：位于缓冲区末尾，存储分片信息和引用计数

### 2.2 Key Pointer Relationships

```
+============================================================================+
|                      SKB POINTER RELATIONSHIPS                              |
+============================================================================+
|                                                                             |
|   INVARIANTS (must always hold):                                            |
|                                                                             |
|       head <= data <= tail <= end                                           |
|                                                                             |
|   LENGTH CALCULATIONS:                                                      |
|                                                                             |
|       headroom     = data - head      (space before data)                   |
|       linear_len   = tail - data      (linear data length)                  |
|       tailroom     = end - tail       (space after data)                    |
|       buffer_size  = end - head       (total buffer size)                   |
|                                                                             |
|   SKB LENGTH FIELDS:                                                        |
|                                                                             |
|       len          = linear_len + data_len (total packet length)            |
|       data_len     = sum of all fragment sizes (non-linear data)            |
|       headlen(skb) = skb->len - skb->data_len (linear portion)              |
|                                                                             |
|   EXAMPLE (1500-byte packet with 500 bytes in fragments):                   |
|                                                                             |
|   +-------------------------+--------------------------------------------+  |
|   |     Linear Data         |            Fragment Data                   |  |
|   |     (1000 bytes)        |            (500 bytes)                     |  |
|   +-------------------------+--------------------------------------------+  |
|         ^                                                                   |
|         |                                                                   |
|   skb->len = 1500                                                           |
|   skb->data_len = 500                                                       |
|   skb_headlen(skb) = 1000                                                   |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **四个指针的关系**：`head <= data <= tail <= end` 必须始终成立
- **len**：整个数据包的总长度（线性部分 + 分片部分）
- **data_len**：非线性数据（分片）的长度
- **headlen**：线性数据的长度，等于 `len - data_len`

---

## 3. SKB Structure Field Breakdown

### 3.1 struct sk_buff Definition

```c
/* File: include/linux/skbuff.h */

struct sk_buff {
    /* These two members must be first for queue operations */
    struct sk_buff      *next;          /* Next buffer in list */
    struct sk_buff      *prev;          /* Previous buffer in list */

    ktime_t             tstamp;         /* Timestamp of arrival/departure */

    struct sock         *sk;            /* Socket owner */
    struct net_device   *dev;           /* Device we arrived on/leaving by */

    /* 48-byte control buffer - each layer can use freely */
    char                cb[48] __aligned(8);

    unsigned long       _skb_refdst;    /* Destination routing entry */

    unsigned int        len;            /* Total packet length */
    unsigned int        data_len;       /* Length of paged data */

    __u16               mac_len;        /* Length of link layer header */
    __u16               hdr_len;        /* Writable header length */

    union {
        __wsum          csum;           /* Checksum for CHECKSUM_COMPLETE */
        struct {
            __u16       csum_start;     /* Offset to start checksumming */
            __u16       csum_offset;    /* Offset to store checksum */
        };
    };

    __u32               priority;       /* Packet queueing priority */

    /* Bit fields for various flags */
    __u8                local_df:1,     /* Allow local fragmentation */
                        cloned:1,       /* Head may be cloned */
                        ip_summed:2,    /* Checksum status */
                        nohdr:1,        /* Payload only, no header mod */
                        nfctinfo:3;     /* Netfilter conntrack info */

    __u8                pkt_type:3,     /* Packet class (HOST/BROADCAST/etc) */
                        fclone:2,       /* Fast clone status */
                        ipvs_property:1,
                        peeked:1,
                        nf_trace:1;

    __be16              protocol;       /* Packet protocol (ETH_P_IP, etc) */

    void (*destructor)(struct sk_buff *skb);  /* Destructor function */

    __u16               vlan_tci;       /* VLAN tag control info */

    /* Header offsets - stored as offsets from head (or pointers) */
    sk_buff_data_t      transport_header;   /* TCP/UDP header offset */
    sk_buff_data_t      network_header;     /* IP header offset */
    sk_buff_data_t      mac_header;         /* Ethernet header offset */

    /* These must be at the end - see alloc_skb() */
    sk_buff_data_t      tail;           /* Tail of data */
    sk_buff_data_t      end;            /* End of buffer */
    unsigned char       *head;          /* Head of buffer */
    unsigned char       *data;          /* Data head pointer */

    unsigned int        truesize;       /* True buffer size (for memory accounting) */
    atomic_t            users;          /* Reference count */
};
```

### 3.2 Field Categories

```
+============================================================================+
|                       SKB FIELDS BY CATEGORY                                |
+============================================================================+
|                                                                             |
|   +-------------------+--------------------------------------------------+  |
|   |     CATEGORY      |                    FIELDS                        |  |
|   +-------------------+--------------------------------------------------+  |
|   |                   |                                                  |  |
|   | Queue Linkage     | next, prev                                       |  |
|   |                   | (doubly-linked list for socket/device queues)   |  |
|   +-------------------+--------------------------------------------------+  |
|   |                   |                                                  |  |
|   | Ownership         | sk (socket), dev (device)                        |  |
|   |                   | destructor (cleanup callback)                    |  |
|   +-------------------+--------------------------------------------------+  |
|   |                   |                                                  |  |
|   | Length Info       | len (total), data_len (fragments)                |  |
|   |                   | mac_len, hdr_len                                 |  |
|   +-------------------+--------------------------------------------------+  |
|   |                   |                                                  |  |
|   | Buffer Pointers   | head, data, tail, end                            |  |
|   |                   | (define buffer boundaries)                       |  |
|   +-------------------+--------------------------------------------------+  |
|   |                   |                                                  |  |
|   | Header Offsets    | mac_header, network_header, transport_header     |  |
|   |                   | (enable zero-copy header access)                 |  |
|   +-------------------+--------------------------------------------------+  |
|   |                   |                                                  |  |
|   | Protocol Info     | protocol (ETH_P_*), pkt_type                     |  |
|   |                   | (identify packet type and destination)           |  |
|   +-------------------+--------------------------------------------------+  |
|   |                   |                                                  |  |
|   | Checksum          | ip_summed, csum, csum_start, csum_offset         |  |
|   |                   | (hardware offload support)                       |  |
|   +-------------------+--------------------------------------------------+  |
|   |                   |                                                  |  |
|   | Control Buffer    | cb[48] (layer-private storage)                   |  |
|   |                   | (TCP uses for tcp_skb_cb, etc.)                  |  |
|   +-------------------+--------------------------------------------------+  |
|   |                   |                                                  |  |
|   | Memory Tracking   | truesize, users (ref count)                      |  |
|   |                   | (memory accounting and lifetime)                 |  |
|   +-------------------+--------------------------------------------------+  |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **队列链接**：`next/prev` 用于将 SKB 链接成双向链表
- **所有权**：`sk` 指向拥有该 SKB 的 socket，`dev` 指向网络设备
- **长度信息**：`len` 是总长度，`data_len` 是分片数据长度
- **缓冲区指针**：`head/data/tail/end` 定义缓冲区边界
- **头部偏移**：协议头的位置，实现零拷贝解析
- **控制缓冲区**：48 字节的私有存储空间，每层可自由使用

---

## 4. Linear vs Non-Linear SKBs

### 4.1 Linear SKB

```
+============================================================================+
|                           LINEAR SKB                                        |
+============================================================================+
|                                                                             |
|   All data is contiguous in the main buffer:                                |
|                                                                             |
|   struct sk_buff                                                            |
|   +------------------+                                                      |
|   | len = 1500       |                                                      |
|   | data_len = 0     |  <-- data_len is 0 for linear SKB                   |
|   | head ------------|--+                                                   |
|   | data ------------|--|--+                                                |
|   | tail ------------|--|--|--+                                             |
|   | end  ------------|--|--|--|--+                                          |
|   +------------------+  |  |  |  |                                          |
|                         v  v  v  v                                          |
|   Data Buffer:                                                              |
|   +------------------------------------------------------------------+      |
|   |     |  Eth  |   IP    |  TCP   |        Payload          |       |      |
|   |     | Hdr   |  Hdr    |  Hdr   |        (1448 bytes)     |       |      |
|   |     | (14)  |  (20)   |  (20)  |                         |       |      |
|   +------------------------------------------------------------------+      |
|   ^     ^                                                    ^       ^      |
|   head  data                                                tail    end     |
|                                                                             |
|   skb_shared_info:                                                          |
|   +------------------+                                                      |
|   | nr_frags = 0     |  <-- No fragments                                    |
|   | frag_list = NULL |  <-- No chained SKBs                                 |
|   | dataref = 1      |                                                      |
|   +------------------+                                                      |
|                                                                             |
|   Characteristics:                                                          |
|   - All data accessible via skb->data                                       |
|   - Simple, fast access                                                     |
|   - Used for small packets                                                  |
|   - skb_is_nonlinear(skb) returns false                                     |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **线性 SKB**：所有数据都在主缓冲区中连续存储
- `data_len = 0`，`nr_frags = 0`
- 适合小数据包，访问简单快速
- 可以通过 `skb->data` 直接访问所有数据

### 4.2 Non-Linear SKB (Paged/Fragmented)

```
+============================================================================+
|                         NON-LINEAR SKB (FRAGMENTED)                         |
+============================================================================+
|                                                                             |
|   Data is split between linear buffer and page fragments:                   |
|                                                                             |
|   struct sk_buff                                                            |
|   +------------------+                                                      |
|   | len = 64000      |  <-- Total packet length                             |
|   | data_len = 63000 |  <-- Data in fragments                               |
|   | head, data, ...  |                                                      |
|   +------------------+                                                      |
|            |                                                                |
|            v                                                                |
|   Linear Data Buffer (headers only):                                        |
|   +------------------------------------------+                              |
|   |  Eth  |   IP    |  TCP   |  1st 946     |                               |
|   | (14)  |  (20)   |  (20)  |  bytes data  |                               |
|   +------------------------------------------+                              |
|   ^       ^                                  ^                              |
|   data                                      tail                            |
|   skb_headlen(skb) = 1000 bytes                                             |
|                                                                             |
|   skb_shared_info at end:                                                   |
|   +------------------------------------------------------------------+      |
|   | nr_frags = 16                                                     |     |
|   | gso_size = 1448 (for TSO)                                         |     |
|   | gso_segs = 45                                                     |     |
|   | frag_list = NULL (or points to chained SKBs)                      |     |
|   | dataref = 1                                                       |     |
|   |                                                                   |     |
|   | frags[0]:  page_0, offset=0,    size=4096                         |     |
|   | frags[1]:  page_1, offset=0,    size=4096                         |     |
|   | frags[2]:  page_2, offset=0,    size=4096                         |     |
|   | ...                                                               |     |
|   | frags[15]: page_15, offset=0,   size=2776                         |     |
|   +-------------------------------------------------------------------+     |
|                   |       |       |                                         |
|                   v       v       v                                         |
|   +-------+   +-------+   +-------+                                         |
|   | Page0 |   | Page1 |   | Page2 |  ...  (separate memory pages)           |
|   | 4KB   |   | 4KB   |   | 4KB   |                                         |
|   +-------+   +-------+   +-------+                                         |
|                                                                             |
|   ALTERNATIVE: frag_list (chained SKBs)                                     |
|   +-------------+    +-------------+    +-------------+                     |
|   | SKB (main)  |--->| SKB frag 1  |--->| SKB frag 2  |--->NULL             |
|   | with header |    | payload     |    | payload     |                     |
|   +-------------+    +-------------+    +-------------+                     |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **非线性 SKB**：数据分布在主缓冲区和多个页面分片中
- **线性部分**：通常包含协议头和少量数据
- **frags[] 数组**：指向包含剩余数据的页面
- **frag_list**：用于链接多个 SKB（如 IP 分片重组）
- 常用于大数据包、TSO/GSO、零拷贝传输

### 4.3 skb_frag_t Structure

```c
/* Page fragment descriptor */
struct skb_frag_struct {
    struct {
        struct page *p;      /* Page containing data */
    } page;
    __u32 page_offset;       /* Offset within page */
    __u32 size;              /* Size of this fragment */
};

/* skb_shared_info at end of buffer */
struct skb_shared_info {
    unsigned short  nr_frags;       /* Number of page fragments */
    unsigned short  gso_size;       /* GSO segment size */
    unsigned short  gso_segs;       /* Number of GSO segments */
    unsigned short  gso_type;       /* GSO type (TCP, UDP, etc) */
    struct sk_buff  *frag_list;     /* List of chained SKBs */
    atomic_t        dataref;        /* Data buffer reference count */
    skb_frag_t      frags[MAX_SKB_FRAGS];  /* Page fragments */
};
```

---

## 5. Protocol Header Offsets

### 5.1 Header Offset Mechanism

```
+============================================================================+
|                    PROTOCOL HEADER OFFSETS                                  |
+============================================================================+
|                                                                             |
|   The key to zero-copy header processing is maintaining offsets:            |
|                                                                             |
|   Data Buffer:                                                              |
|   +---------------------------------------------------------------------+   |
|   |        |  Ethernet  |    IP     |   TCP    |      Payload          |   |
|   |        |   Header   |  Header   |  Header  |                        |   |
|   |        |  (14 B)    | (20-60 B) | (20-60 B)|                        |   |
|   +---------------------------------------------------------------------+   |
|   ^        ^            ^           ^                                       |
|   |        |            |           |                                       |
|   head   mac_header  network_    transport_                                 |
|            |          header      header                                    |
|            |            |           |                                       |
|            v            v           v                                       |
|   +---------------------------------------------------------------------+   |
|   | Offset |     14     |    34     |    54    |                        |   |
|   | (from  |            |           |          |                        |   |
|   |  head) |            |           |          |                        |   |
|   +---------------------------------------------------------------------+   |
|                                                                             |
|   On 64-bit systems: offsets are integers relative to skb->head            |
|   On 32-bit systems: offsets are actual pointers                           |
|                                                                             |
|   Access Functions:                                                         |
|   - skb_mac_header(skb)       -> returns pointer to Ethernet header        |
|   - skb_network_header(skb)   -> returns pointer to IP header              |
|   - skb_transport_header(skb) -> returns pointer to TCP/UDP header         |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **头部偏移**：存储各层协议头相对于 `head` 的偏移量
- **零拷贝**：通过偏移量直接访问原始数据，无需复制
- **64 位优化**：使用整数偏移而非指针，节省内存
- **访问函数**：`skb_mac_header()` 等返回实际指针

### 5.2 Header Offset Operations

```
+============================================================================+
|                     HEADER POINTER MANIPULATION                             |
+============================================================================+
|                                                                             |
|   INITIAL STATE (after driver receives packet):                             |
|   +---------------------------------------------------------------------+   |
|   |  headroom  |  Eth  |  IP   |  TCP  |  Payload  |  tailroom         |   |
|   +---------------------------------------------------------------------+   |
|                ^                                                            |
|                data (points to start of Ethernet header)                    |
|                mac_header = data                                            |
|                                                                             |
|   STEP 1: eth_type_trans() - process Ethernet header                       |
|   +---------------------------------------------------------------------+   |
|   |  headroom  |  Eth  |  IP   |  TCP  |  Payload  |  tailroom         |   |
|   +---------------------------------------------------------------------+   |
|                ^       ^                                                    |
|                |       data (moved past Ethernet header)                    |
|                mac_header (still points to Ethernet header)                 |
|                                                                             |
|   STEP 2: ip_rcv() - process IP header                                     |
|   +---------------------------------------------------------------------+   |
|   |  headroom  |  Eth  |  IP   |  TCP  |  Payload  |  tailroom         |   |
|   +---------------------------------------------------------------------+   |
|                ^       ^       ^                                            |
|                |       |       data (moved past IP header)                  |
|                |       network_header (points to IP header)                 |
|                mac_header                                                   |
|                                                                             |
|   STEP 3: tcp_rcv() - process TCP header                                   |
|   +---------------------------------------------------------------------+   |
|   |  headroom  |  Eth  |  IP   |  TCP  |  Payload  |  tailroom         |   |
|   +---------------------------------------------------------------------+   |
|                ^       ^       ^       ^                                    |
|                |       |       |       data (at payload start)              |
|                |       |       transport_header (TCP header)                |
|                |       network_header                                       |
|                mac_header                                                   |
|                                                                             |
|   RESULT: All headers accessible without data copy!                         |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **初始状态**：驱动接收数据后，`data` 指向以太网头
- **eth_type_trans()**：处理以太网头，`data` 前移，记录 `mac_header`
- **ip_rcv()**：处理 IP 头，`data` 前移，记录 `network_header`
- **tcp_rcv()**：处理 TCP 头，`data` 前移，记录 `transport_header`
- **最终**：所有协议头都可以通过偏移访问，无需复制数据

### 5.3 Header Access Code Examples

```c
/* Setting header pointers during RX */

/* Driver sets mac_header when receiving */
skb_reset_mac_header(skb);  /* mac_header = data */

/* eth_type_trans() processes Ethernet, updates data */
skb->protocol = eth_type_trans(skb, dev);
/* Internally: skb_pull(skb, ETH_HLEN) moves data past Ethernet header */

/* IP layer sets network_header */
skb_reset_network_header(skb);  /* network_header = data */
struct iphdr *iph = ip_hdr(skb);  /* same as skb_network_header() cast */

/* After pulling IP header, set transport_header */
skb_set_transport_header(skb, iph->ihl * 4);
struct tcphdr *th = tcp_hdr(skb);  /* same as skb_transport_header() cast */

/* Convenient type-casting macros */
#define ip_hdr(skb)   ((struct iphdr *)skb_network_header(skb))
#define tcp_hdr(skb)  ((struct tcphdr *)skb_transport_header(skb))
#define udp_hdr(skb)  ((struct udphdr *)skb_transport_header(skb))
#define eth_hdr(skb)  ((struct ethhdr *)skb_mac_header(skb))
```

---

## 6. Zero-Copy Data Flow

### 6.1 Zero-Copy Principle

```
+============================================================================+
|                        ZERO-COPY DATA FLOW                                  |
+============================================================================+
|                                                                             |
|   The goal: Move packet through the entire stack WITHOUT copying data      |
|                                                                             |
|   TRADITIONAL APPROACH (with copies):                                       |
|   +----------+    +----------+    +----------+    +----------+             |
|   |  Driver  |--->|    IP    |--->|   TCP    |--->|  Socket  |             |
|   |  buffer  |copy|  buffer  |copy|  buffer  |copy|  buffer  |             |
|   +----------+    +----------+    +----------+    +----------+             |
|                                                                             |
|   ZERO-COPY APPROACH (SKB design):                                          |
|   +--------------------------------------------------------------------+   |
|   |                     Single Data Buffer                              |   |
|   +--------------------------------------------------------------------+   |
|         ^               ^               ^               ^                   |
|         |               |               |               |                   |
|      Driver            IP              TCP            Socket               |
|      (data ptr)    (skb_pull)      (skb_pull)      (just read)             |
|                                                                             |
|   HOW IT WORKS:                                                             |
|                                                                             |
|   1. Driver allocates buffer, DMA fills it                                 |
|   2. Each layer moves skb->data pointer (skb_pull)                         |
|   3. Header offsets track where each header starts                         |
|   4. No data is ever copied between layers                                  |
|   5. Only metadata (sk_buff struct) is modified                            |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **零拷贝目标**：数据包在整个协议栈中传递时不复制数据
- **传统方式**：每层都复制数据到自己的缓冲区
- **SKB 方式**：共享同一个数据缓冲区，通过移动指针访问不同层的数据
- **实现机制**：`skb_pull()` 移动 `data` 指针，头部偏移记录各层位置

### 6.2 TX Zero-Copy (sendfile/MSG_ZEROCOPY)

```
+============================================================================+
|                      TX ZERO-COPY MECHANISM                                 |
+============================================================================+
|                                                                             |
|   User Space                                                                |
|   +------------------+                                                      |
|   | Application      |                                                      |
|   | buffer (mmap'd)  |                                                      |
|   +--------+---------+                                                      |
|            |                                                                |
|            | (pages pinned, not copied)                                     |
|            v                                                                |
|   Kernel Space                                                              |
|   +------------------+                                                      |
|   | sk_buff          |                                                      |
|   | - len = 64000    |                                                      |
|   | - data_len = 64K |  <-- all data in fragments                          |
|   +--------+---------+                                                      |
|            |                                                                |
|            v                                                                |
|   +------------------------------------------------------------------+     |
|   | skb_shared_info                                                   |     |
|   | nr_frags = 16                                                     |     |
|   | frags[0] --> User's page 0 (get_user_pages)                      |     |
|   | frags[1] --> User's page 1                                       |     |
|   | ...                                                               |     |
|   +------------------------------------------------------------------+     |
|                                                                             |
|   Driver DMA from user pages directly:                                      |
|   +------------------+                                                      |
|   | NIC DMA Engine   |-----> User's memory pages                           |
|   +------------------+       (no kernel buffer copy!)                       |
|                                                                             |
|   Completion notification via SO_ZEROCOPY/MSG_ZEROCOPY                     |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **用户态零拷贝**：`sendfile()` 或 `MSG_ZEROCOPY` 避免用户态到内核态的复制
- **页面固定**：用户空间的页面被固定（pin），直接用作 SKB 分片
- **DMA 传输**：NIC 直接从用户页面 DMA 数据，完全绕过内核缓冲区
- **完成通知**：通过 `SO_ZEROCOPY` 通知应用何时可以重用缓冲区

---

## 7. SKB Lifecycle - TX Path

```
+============================================================================+
|                         SKB TX LIFECYCLE                                    |
+============================================================================+
|                                                                             |
|   APPLICATION                                                               |
|   send(socket, data, len)                                                  |
|          |                                                                  |
|          v                                                                  |
|   +------------------------+                                                |
|   | sock_sendmsg()         |                                                |
|   +-----------|------------+                                                |
|               |                                                             |
|               v                                                             |
|   +========================================+                                |
|   |            SOCKET LAYER                |                                |
|   +========================================+                                |
|   |                                        |                                |
|   |   +------------------------+           |                                |
|   |   | tcp_sendmsg()          |           |                                |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------+           |                                |
|   |   | sk_stream_alloc_skb()  |  <-- SKB ALLOCATION                       |
|   |   | or alloc_skb()         |           |                                |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   |   1. Allocate sk_buff from slab       |                                |
|   |   2. Allocate data buffer (kmalloc)   |                                |
|   |   3. Initialize: head=data=tail       |                                |
|   |   4. skb_reserve() for headroom       |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------+           |                                |
|   |   | Copy user data         |           |                                |
|   |   | skb_put(skb, len)      |  <-- tail moves, len increases            |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   +===============|========================+                                |
|                   |                                                         |
|                   v                                                         |
|   +========================================+                                |
|   |            TCP LAYER                   |                                |
|   +========================================+                                |
|   |                                        |                                |
|   |   +------------------------+           |                                |
|   |   | tcp_transmit_skb()     |           |                                |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------+           |                                |
|   |   | skb_push(skb, tcp_hdr) |  <-- data moves back, add TCP header      |
|   |   | Build TCP header       |           |                                |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   +===============|========================+                                |
|                   |                                                         |
|                   v                                                         |
|   +========================================+                                |
|   |             IP LAYER                   |                                |
|   +========================================+                                |
|   |                                        |                                |
|   |   +------------------------+           |                                |
|   |   | ip_queue_xmit()        |           |                                |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------+           |                                |
|   |   | skb_push(skb, ip_hdr)  |  <-- data moves back, add IP header       |
|   |   | skb_reset_network_hdr()|           |                                |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   +===============|========================+                                |
|                   |                                                         |
|                   v                                                         |
|   +========================================+                                |
|   |           DEVICE LAYER                 |                                |
|   +========================================+                                |
|   |                                        |                                |
|   |   +------------------------+           |                                |
|   |   | dev_hard_header()      |           |                                |
|   |   | skb_push(skb, eth_hdr) |  <-- Add Ethernet header                  |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------+           |                                |
|   |   | dev_queue_xmit()       |           |                                |
|   |   | -> qdisc enqueue       |           |                                |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   +===============|========================+                                |
|                   |                                                         |
|                   v                                                         |
|   +========================================+                                |
|   |            NIC DRIVER                  |                                |
|   +========================================+                                |
|   |                                        |                                |
|   |   +------------------------+           |                                |
|   |   | ndo_start_xmit()       |           |                                |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------------+     |                                |
|   |   | skb_to_sgvec() or similar    |     |                                |
|   |   | Map SKB to scatter-gather    |     |                                |
|   |   | Add to TX ring               |     |                                |
|   |   | DMA transfer                 |     |                                |
|   |   +-----------|------------------+     |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------------+     |                                |
|   |   | TX completion interrupt      |     |                                |
|   |   | dev_kfree_skb(skb)           |  <-- SKB FREED                      |
|   |   +------------------------------+     |                                |
|   |                                        |                                |
|   +========================================+                                |
|                                                                             |
+============================================================================+
```

**中文说明**：
1. **分配阶段**：`alloc_skb()` 分配 SKB 和数据缓冲区
2. **数据填充**：`skb_put()` 扩展 tail，复制用户数据
3. **TCP 处理**：`skb_push()` 向前扩展 data，添加 TCP 头
4. **IP 处理**：`skb_push()` 继续向前扩展，添加 IP 头
5. **设备层**：`skb_push()` 添加以太网头
6. **驱动**：映射为 scatter-gather，DMA 发送
7. **完成**：`dev_kfree_skb()` 释放 SKB

---

## 8. SKB Lifecycle - RX Path

```
+============================================================================+
|                         SKB RX LIFECYCLE                                    |
+============================================================================+
|                                                                             |
|   HARDWARE / NIC                                                            |
|   Packet arrives on wire                                                    |
|          |                                                                  |
|          v                                                                  |
|   +========================================+                                |
|   |            NIC DRIVER                  |                                |
|   +========================================+                                |
|   |                                        |                                |
|   |   +------------------------------+     |                                |
|   |   | Pre-allocated RX buffer      |     |                                |
|   |   | (from RX ring or page pool)  |     |                                |
|   |   +-----------|------------------+     |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------------+     |                                |
|   |   | DMA fills buffer             |     |                                |
|   |   | IRQ signals completion       |     |                                |
|   |   +-----------|------------------+     |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------------+     |                                |
|   |   | netdev_alloc_skb_ip_align()  |  <-- SKB ALLOCATION                 |
|   |   | (or build_skb from page)     |     |                                |
|   |   +-----------|------------------+     |                                |
|   |               |                        |                                |
|   |   1. Allocate sk_buff                  |                                |
|   |   2. Point to DMA buffer               |                                |
|   |   3. skb_reserve(NET_IP_ALIGN)        |                                |
|   |   4. skb_put(skb, pkt_len)            |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------------+     |                                |
|   |   | skb_reset_mac_header()       |     |                                |
|   |   | eth_type_trans(skb, dev)     |  <-- Process Ethernet, skb_pull    |
|   |   +-----------|------------------+     |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------------+     |                                |
|   |   | napi_gro_receive() or        |  <-- Pass to network stack         |
|   |   | netif_receive_skb()          |     |                                |
|   |   +-----------|------------------+     |                                |
|   |                                        |                                |
|   +===============|========================+                                |
|                   |                                                         |
|                   v                                                         |
|   +========================================+                                |
|   |           NETWORK CORE                 |                                |
|   +========================================+                                |
|   |                                        |                                |
|   |   +------------------------------+     |                                |
|   |   | __netif_receive_skb_core()   |     |                                |
|   |   | Packet type dispatch         |     |                                |
|   |   +-----------|------------------+     |                                |
|   |               |                        |                                |
|   +===============|========================+                                |
|                   |                                                         |
|                   v                                                         |
|   +========================================+                                |
|   |             IP LAYER                   |                                |
|   +========================================+                                |
|   |                                        |                                |
|   |   +------------------------------+     |                                |
|   |   | ip_rcv()                     |     |                                |
|   |   | skb_reset_network_header()   |     |                                |
|   |   | Validate IP header           |     |                                |
|   |   +-----------|------------------+     |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------------+     |                                |
|   |   | ip_rcv_finish()              |     |                                |
|   |   | Route lookup                 |     |                                |
|   |   +-----------|------------------+     |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------------+     |                                |
|   |   | skb_pull(skb, ip_hdr_len)    |  <-- Move data past IP header       |
|   |   | ip_local_deliver()           |     |                                |
|   |   +-----------|------------------+     |                                |
|   |                                        |                                |
|   +===============|========================+                                |
|                   |                                                         |
|                   v                                                         |
|   +========================================+                                |
|   |            TCP LAYER                   |                                |
|   +========================================+                                |
|   |                                        |                                |
|   |   +------------------------------+     |                                |
|   |   | tcp_v4_rcv()                 |     |                                |
|   |   | skb_reset_transport_header() |     |                                |
|   |   | Validate TCP header          |     |                                |
|   |   +-----------|------------------+     |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------------+     |                                |
|   |   | skb_pull(skb, tcp_hdr_len)   |  <-- data now at payload            |
|   |   | tcp_queue_rcv()              |     |                                |
|   |   +-----------|------------------+     |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------------+     |                                |
|   |   | Add to socket receive queue  |     |                                |
|   |   | sk_data_ready(sk)            |  <-- Wake up application            |
|   |   +------------------------------+     |                                |
|   |                                        |                                |
|   +========================================+                                |
|                   |                                                         |
|                   v                                                         |
|   +========================================+                                |
|   |           APPLICATION                  |                                |
|   +========================================+                                |
|   |                                        |                                |
|   |   recv(socket, buffer, len)            |                                |
|   |   +------------------------------+     |                                |
|   |   | skb_copy_datagram_iovec()    |  <-- Copy payload to user           |
|   |   +-----------|------------------+     |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------------+     |                                |
|   |   | kfree_skb(skb)               |  <-- SKB FREED                      |
|   |   +------------------------------+     |                                |
|   |                                        |                                |
|   +========================================+                                |
|                                                                             |
+============================================================================+
```

**中文说明**：
1. **硬件接收**：NIC DMA 将数据写入预分配的缓冲区
2. **驱动处理**：分配 SKB，设置 `data`/`tail`，调用 `eth_type_trans()`
3. **网络核心**：根据协议类型分发到相应处理函数
4. **IP 层**：验证头部，路由查找，`skb_pull()` 跳过 IP 头
5. **TCP 层**：验证头部，`skb_pull()` 跳过 TCP 头，加入接收队列
6. **应用层**：`recv()` 复制数据到用户空间，释放 SKB

---

## 9. Common SKB Helper APIs

### 9.1 Allocation Functions

```c
/*=====================================================
 * SKB ALLOCATION APIS
 *=====================================================*/

/* Basic allocation - size is for data buffer */
struct sk_buff *alloc_skb(unsigned int size, gfp_t priority);

/* Device-aware allocation with built-in headroom */
struct sk_buff *netdev_alloc_skb(struct net_device *dev, 
                                  unsigned int length);

/* With IP alignment (2 bytes) for efficient IP header access */
struct sk_buff *netdev_alloc_skb_ip_align(struct net_device *dev,
                                           unsigned int length);

/* Build SKB from existing data buffer (zero-copy) */
struct sk_buff *build_skb(void *data, unsigned int frag_size);

/* Clone SKB (shares data, copies metadata) */
struct sk_buff *skb_clone(struct sk_buff *skb, gfp_t priority);

/* Full copy (new data buffer) */
struct sk_buff *skb_copy(const struct sk_buff *skb, gfp_t priority);

/* Copy with modified headroom */
struct sk_buff *skb_copy_expand(const struct sk_buff *skb,
                                 int newheadroom, int newtailroom,
                                 gfp_t priority);
```

### 9.2 Data Manipulation

```
+============================================================================+
|                    SKB DATA MANIPULATION APIS                               |
+============================================================================+
|                                                                             |
|   BEFORE ANY OPERATION:                                                     |
|   +------------------------------------------------------------------+     |
|   |  headroom  |        data region          |  tailroom            |     |
|   +------------------------------------------------------------------+     |
|   ^            ^                             ^                       ^     |
|   head        data                          tail                    end    |
|                                                                             |
|   ========================================================================  |
|   skb_reserve(skb, len) - Reserve headroom (use on empty SKB only)          |
|   ========================================================================  |
|   AFTER skb_reserve(skb, 16):                                               |
|   +------------------------------------------------------------------+     |
|   |  headroom (16 more) |     data region     |  tailroom (16 less) |     |
|   +------------------------------------------------------------------+     |
|   ^                     ^                     ^                      ^     |
|   head                 data/tail                                    end    |
|                                                                             |
|   ========================================================================  |
|   skb_put(skb, len) - Add data to tail (extend packet)                     |
|   ========================================================================  |
|   AFTER skb_put(skb, 100):                                                  |
|   +------------------------------------------------------------------+     |
|   |  headroom  |     data (100 bytes)        |  tailroom            |     |
|   +------------------------------------------------------------------+     |
|   ^            ^                             ^                       ^     |
|   head        data                          tail                    end    |
|                         |<-- 100 bytes -->|                                 |
|   Returns: pointer to start of added space (old tail)                       |
|                                                                             |
|   ========================================================================  |
|   skb_push(skb, len) - Add header (prepend data)                           |
|   ========================================================================  |
|   AFTER skb_push(skb, 20):                                                  |
|   +------------------------------------------------------------------+     |
|   |  headroom |  new  |     original data      |  tailroom          |     |
|   |  (less)   | (20B) |                        |                    |     |
|   +------------------------------------------------------------------+     |
|   ^           ^                                ^                     ^     |
|   head       data                             tail                  end    |
|              |<-20->|                                                       |
|   Returns: new data pointer                                                 |
|                                                                             |
|   ========================================================================  |
|   skb_pull(skb, len) - Remove header (consume data from front)             |
|   ========================================================================  |
|   AFTER skb_pull(skb, 14):                                                  |
|   +------------------------------------------------------------------+     |
|   |    headroom (14 more)    |   remaining data   |  tailroom       |     |
|   +------------------------------------------------------------------+     |
|   ^                          ^                    ^                  ^     |
|   head                      data                 tail               end    |
|                              |<- len decreased ->|                         |
|   Returns: new data pointer                                                 |
|                                                                             |
|   ========================================================================  |
|   skb_trim(skb, len) - Trim packet to specified length                     |
|   ========================================================================  |
|   AFTER skb_trim(skb, 64):                                                  |
|   +------------------------------------------------------------------+     |
|   |  headroom  |     data (64 bytes)   |       tailroom (more)      |     |
|   +------------------------------------------------------------------+     |
|   ^            ^                       ^                             ^     |
|   head        data                    tail                          end    |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **skb_reserve()**：在空 SKB 中预留头部空间
- **skb_put()**：在尾部添加数据，`tail` 后移，`len` 增加
- **skb_push()**：在头部添加数据，`data` 前移，`len` 增加
- **skb_pull()**：从头部移除数据，`data` 后移，`len` 减少
- **skb_trim()**：将数据包裁剪到指定长度

### 9.3 Header Offset Functions

```c
/*=====================================================
 * HEADER OFFSET FUNCTIONS
 *=====================================================*/

/* Reset header pointers to current data position */
void skb_reset_mac_header(struct sk_buff *skb);
void skb_reset_network_header(struct sk_buff *skb);
void skb_reset_transport_header(struct sk_buff *skb);

/* Set header offset with displacement from data */
void skb_set_mac_header(struct sk_buff *skb, int offset);
void skb_set_network_header(struct sk_buff *skb, int offset);
void skb_set_transport_header(struct sk_buff *skb, int offset);

/* Get header pointers */
unsigned char *skb_mac_header(const struct sk_buff *skb);
unsigned char *skb_network_header(const struct sk_buff *skb);
unsigned char *skb_transport_header(const struct sk_buff *skb);

/* Type-cast header access macros */
#define eth_hdr(skb)  ((struct ethhdr *)skb_mac_header(skb))
#define ip_hdr(skb)   ((struct iphdr *)skb_network_header(skb))
#define ipv6_hdr(skb) ((struct ipv6hdr *)skb_network_header(skb))
#define tcp_hdr(skb)  ((struct tcphdr *)skb_transport_header(skb))
#define udp_hdr(skb)  ((struct udphdr *)skb_transport_header(skb))

/* Calculate header lengths and offsets */
unsigned int skb_headlen(const struct sk_buff *skb);  /* Linear data len */
int skb_network_offset(const struct sk_buff *skb);
int skb_transport_offset(const struct sk_buff *skb);
u32 skb_network_header_len(const struct sk_buff *skb);
```

### 9.4 Space and Size Functions

```c
/*=====================================================
 * SPACE AND SIZE FUNCTIONS
 *=====================================================*/

/* Available space */
unsigned int skb_headroom(const struct sk_buff *skb);  /* data - head */
int skb_tailroom(const struct sk_buff *skb);           /* end - tail */

/* Check if SKB is non-linear (has fragments) */
static inline bool skb_is_nonlinear(const struct sk_buff *skb)
{
    return skb->data_len;
}

/* Linear data length */
static inline unsigned int skb_headlen(const struct sk_buff *skb)
{
    return skb->len - skb->data_len;
}

/* Ensure header is in linear part (pull if needed) */
static inline int pskb_may_pull(struct sk_buff *skb, unsigned int len)
{
    if (likely(len <= skb_headlen(skb)))
        return 1;
    if (unlikely(len > skb->len))
        return 0;
    return __pskb_pull_tail(skb, len - skb_headlen(skb)) != NULL;
}

/* Expand headroom */
int pskb_expand_head(struct sk_buff *skb, int nhead, int ntail, gfp_t gfp);
```

### 9.5 Fragment and Non-Linear Operations

```c
/*=====================================================
 * FRAGMENT OPERATIONS
 *=====================================================*/

/* Access skb_shared_info */
#define skb_shinfo(skb) ((struct skb_shared_info *)(skb_end_pointer(skb)))

/* Add a page fragment for RX */
void skb_add_rx_frag(struct sk_buff *skb, int i, struct page *page,
                     int off, int size);

/* Fill page descriptor */
void skb_fill_page_desc(struct sk_buff *skb, int i,
                        struct page *page, int off, int size);

/* Check for fragments */
static inline bool skb_has_frag_list(const struct sk_buff *skb)
{
    return skb_shinfo(skb)->frag_list != NULL;
}

/* Walk through fragment list */
#define skb_walk_frags(skb, iter) \
    for (iter = skb_shinfo(skb)->frag_list; iter; iter = iter->next)

/* Linearize non-linear SKB (copies all fragments to linear buffer) */
int skb_linearize(struct sk_buff *skb);

/* Convert SKB to scatter-gather list for DMA */
int skb_to_sgvec(struct sk_buff *skb, struct scatterlist *sg,
                 int offset, int len);
```

### 9.6 Checksum Functions

```c
/*=====================================================
 * CHECKSUM OPERATIONS
 *=====================================================*/

/* Checksum modes (skb->ip_summed) */
#define CHECKSUM_NONE        0  /* No checksum info */
#define CHECKSUM_UNNECESSARY 1  /* HW verified, no need to check */
#define CHECKSUM_COMPLETE    2  /* HW computed csum of entire packet */
#define CHECKSUM_PARTIAL     3  /* HW should compute csum from csum_start */

/* Set partial checksum info for TX offload */
static inline void skb_set_csum_partial(struct sk_buff *skb,
                                         u16 start, u16 off)
{
    skb->ip_summed = CHECKSUM_PARTIAL;
    skb->csum_start = start + skb_headroom(skb);
    skb->csum_offset = off;
}

/* Check if checksum verification is needed */
static inline bool skb_csum_unnecessary(const struct sk_buff *skb)
{
    return skb->ip_summed == CHECKSUM_UNNECESSARY ||
           skb->ip_summed == CHECKSUM_PARTIAL;
}
```

### 9.7 Memory and Reference Management

```c
/*=====================================================
 * MEMORY AND REFERENCE MANAGEMENT
 *=====================================================*/

/* Free SKB */
void kfree_skb(struct sk_buff *skb);        /* Normal free */
void consume_skb(struct sk_buff *skb);      /* "Consumed" free (for perf) */
void dev_kfree_skb(struct sk_buff *skb);    /* Alias for consume_skb */
void dev_kfree_skb_any(struct sk_buff *skb);/* Safe in any context */
void dev_kfree_skb_irq(struct sk_buff *skb);/* Deferred free from IRQ */

/* Reference counting */
static inline struct sk_buff *skb_get(struct sk_buff *skb)
{
    atomic_inc(&skb->users);
    return skb;
}

/* Clone vs Copy */
/* skb_clone: new sk_buff, SHARES data buffer (dataref incremented) */
/* skb_copy:  new sk_buff, new data buffer (full copy) */

/* Check if data is shared (cloned) */
static inline int skb_shared(const struct sk_buff *skb)
{
    return atomic_read(&skb->users) != 1;
}

/* Check if data buffer is shared */
static inline int skb_cloned(const struct sk_buff *skb)
{
    return skb->cloned &&
           (atomic_read(&skb_shinfo(skb)->dataref) & SKB_DATAREF_MASK) != 1;
}

/* Make private copy if shared */
struct sk_buff *skb_share_check(struct sk_buff *skb, gfp_t pri);
int skb_unclone(struct sk_buff *skb, gfp_t pri);
```

---

## 10. SKB and RX/TX Rings Integration

### 10.1 RX Ring Integration

```
+============================================================================+
|                     SKB AND RX RING INTEGRATION                             |
+============================================================================+
|                                                                             |
|   NIC Hardware                                                              |
|   +---------------------------------------------------------------+        |
|   |                        RX DMA Ring                             |        |
|   +---------------------------------------------------------------+        |
|   | Desc[0] | Desc[1] | Desc[2] | Desc[3] | ... | Desc[N-1] |              |
|   | addr0   | addr1   | addr2   | addr3   |     | addrN     |              |
|   | len0    | len1    | len2    | len3    |     | lenN      |              |
|   | status0 | status1 | status2 | status3 |     | statusN   |              |
|   +---|----------|----------|----------|-------------|------+              |
|       |          |          |          |             |                      |
|       v          v          v          v             v                      |
|   +-------+  +-------+  +-------+  +-------+     +-------+                 |
|   |Buffer0|  |Buffer1|  |Buffer2|  |Buffer3| ... |BufferN|                 |
|   | (DMA) |  | (DMA) |  | (DMA) |  | (DMA) |     | (DMA) |                 |
|   +-------+  +-------+  +-------+  +-------+     +-------+                 |
|                                                                             |
|   Driver Buffer Management (Two approaches):                                |
|                                                                             |
|   APPROACH 1: Pre-allocated SKBs                                            |
|   +------------------------------------------------------------------+     |
|   | rx_buffer[i] = {                                                  |     |
|   |     .skb = netdev_alloc_skb_ip_align(dev, MTU)                   |     |
|   |     .dma = dma_map_single(skb->data, ...)                        |     |
|   | }                                                                 |     |
|   | RX descriptor[i].addr = rx_buffer[i].dma                         |     |
|   +------------------------------------------------------------------+     |
|                                                                             |
|   On packet arrival:                                                        |
|   1. Get completed descriptor                                               |
|   2. dma_unmap_single()                                                     |
|   3. skb = rx_buffer[i].skb                                                |
|   4. skb_put(skb, pkt_len)                                                 |
|   5. netif_receive_skb(skb)                                                |
|   6. Allocate new SKB for this slot                                        |
|                                                                             |
|   APPROACH 2: Page Pool (more efficient)                                    |
|   +------------------------------------------------------------------+     |
|   | rx_buffer[i] = {                                                  |     |
|   |     .page = page_pool_alloc_pages(pp, GFP_ATOMIC)                |     |
|   |     .dma = dma_map_page(page, ...)                               |     |
|   |     .offset = 0                                                   |     |
|   | }                                                                 |     |
|   +------------------------------------------------------------------+     |
|                                                                             |
|   On packet arrival:                                                        |
|   1. Get completed descriptor                                               |
|   2. dma_sync_single_for_cpu()  (no unmap - page reused)                   |
|   3. skb = build_skb(page_address(page) + offset, ...)                     |
|   4. skb_put(skb, pkt_len)                                                 |
|   5. OR: skb_add_rx_frag(skb, ..., page, offset, len)                     |
|   6. netif_receive_skb(skb)                                                |
|   7. Allocate new page (or reuse if page not consumed)                     |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **方法一（预分配 SKB）**：每个 RX 描述符对应一个预分配的 SKB
  - 简单但效率较低，每次都要分配新 SKB
- **方法二（页面池）**：使用页面池，SKB 按需构建
  - 页面可以重用，减少分配开销
  - 使用 `build_skb()` 或 `skb_add_rx_frag()` 构建 SKB

### 10.2 TX Ring Integration

```
+============================================================================+
|                     SKB AND TX RING INTEGRATION                             |
+============================================================================+
|                                                                             |
|   SKB arrives from network stack (via ndo_start_xmit):                      |
|                                                                             |
|   struct sk_buff                                                            |
|   +------------------+                                                      |
|   | len = 1500       |                                                      |
|   | data_len = 0     |  (linear SKB example)                               |
|   | data ------------|---> [Eth|IP|TCP|Payload]                            |
|   +------------------+                                                      |
|                                                                             |
|   STEP 1: Map SKB to scatter-gather list                                    |
|   +------------------------------------------------------------------+     |
|   | if (skb_is_nonlinear(skb)) {                                      |     |
|   |     /* Handle fragments */                                        |     |
|   |     num_frags = skb_shinfo(skb)->nr_frags;                       |     |
|   |     for (i = 0; i < num_frags; i++) {                            |     |
|   |         frag = &skb_shinfo(skb)->frags[i];                       |     |
|   |         dma_addr = skb_frag_dma_map(dev, frag, ...);            |     |
|   |     }                                                             |     |
|   | }                                                                 |     |
|   | /* Map linear part */                                             |     |
|   | dma_addr = dma_map_single(dev, skb->data, skb_headlen(skb), ...);|     |
|   +------------------------------------------------------------------+     |
|                                                                             |
|   STEP 2: Fill TX descriptors                                               |
|   +---------------------------------------------------------------+        |
|   |                        TX DMA Ring                             |        |
|   +---------------------------------------------------------------+        |
|   | Desc[i]   | Desc[i+1] | Desc[i+2] | ... (for fragments)        |        |
|   | addr=dma0 | addr=dma1 | addr=dma2 |                            |        |
|   | len=1500  | len=4096  | len=4096  |                            |        |
|   | flags=SOP | flags=0   | flags=EOP |  (Start/End of Packet)     |        |
|   +---------------------------------------------------------------+        |
|                                                                             |
|   STEP 3: Save SKB for completion                                           |
|   +------------------------------------------------------------------+     |
|   | tx_buffer[first_desc].skb = skb;                                  |     |
|   | tx_buffer[first_desc].dma = dma_addr;                            |     |
|   | tx_buffer[first_desc].len = skb_headlen(skb);                    |     |
|   | /* Fragment descriptors don't need SKB pointer */                 |     |
|   +------------------------------------------------------------------+     |
|                                                                             |
|   STEP 4: Ring doorbell                                                     |
|   +------------------------------------------------------------------+     |
|   | wmb();  /* Ensure descriptors are visible before notify */        |     |
|   | writel(new_tail, adapter->tx_tail);  /* Notify hardware */       |     |
|   +------------------------------------------------------------------+     |
|                                                                             |
|   STEP 5: TX Completion (interrupt or polling)                              |
|   +------------------------------------------------------------------+     |
|   | while (tx_desc[cleanup_idx].status & TX_DONE) {                   |     |
|   |     skb = tx_buffer[cleanup_idx].skb;                            |     |
|   |     if (skb) {                                                    |     |
|   |         dma_unmap_single(tx_buffer[cleanup_idx].dma, ...);       |     |
|   |         dev_kfree_skb_any(skb);                                  |     |
|   |     }                                                             |     |
|   |     cleanup_idx = (cleanup_idx + 1) % ring_size;                 |     |
|   | }                                                                 |     |
|   +------------------------------------------------------------------+     |
|                                                                             |
+============================================================================+
```

**中文说明**：
1. **SKB 到达**：通过 `ndo_start_xmit()` 从网络栈接收
2. **DMA 映射**：将 SKB 数据映射为 DMA 地址
3. **填充描述符**：一个 SKB 可能需要多个描述符（分片情况）
4. **保存 SKB**：在第一个描述符的缓冲区信息中保存 SKB 指针
5. **通知硬件**：写尾指针，触发 DMA
6. **完成处理**：解除 DMA 映射，释放 SKB

### 10.3 Example: virtio_net SKB Handling

```c
/* virtio_net TX - from drivers/net/virtio_net.c */

static int xmit_skb(struct virtnet_info *vi, struct sk_buff *skb)
{
    struct skb_vnet_hdr *hdr = skb_vnet_hdr(skb);

    /* Setup virtio header (stored in skb->cb) */
    if (skb->ip_summed == CHECKSUM_PARTIAL) {
        hdr->hdr.flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
        hdr->hdr.csum_start = skb_checksum_start_offset(skb);
        hdr->hdr.csum_offset = skb->csum_offset;
    }

    /* Build scatter-gather list */
    /* First element: virtio header */
    sg_set_buf(vi->tx_sg, &hdr->hdr, sizeof(hdr->hdr));

    /* Remaining elements: SKB data (linear + fragments) */
    hdr->num_sg = skb_to_sgvec(skb, vi->tx_sg + 1, 0, skb->len) + 1;

    /* Add to virtqueue - SKB is the "token" for completion */
    return virtqueue_add_buf(vi->svq, vi->tx_sg, hdr->num_sg, 0, skb);
}

/* virtio_net RX */
static void receive_buf(struct net_device *dev, void *buf, unsigned int len)
{
    struct virtnet_info *vi = netdev_priv(dev);
    struct sk_buff *skb;

    if (!vi->mergeable_rx_bufs && !vi->big_packets) {
        /* Small packet: buffer IS the SKB */
        skb = buf;
        len -= sizeof(struct virtio_net_hdr);
        skb_trim(skb, len);
    } else {
        /* Large packet: buffer is a page, build SKB */
        struct page *page = buf;
        skb = page_to_skb(vi, page, len);
    }

    /* Process virtio header for checksum offload */
    hdr = skb_vnet_hdr(skb);
    if (hdr->hdr.flags & VIRTIO_NET_HDR_F_DATA_VALID)
        skb->ip_summed = CHECKSUM_UNNECESSARY;

    /* Determine protocol and pass to stack */
    skb->protocol = eth_type_trans(skb, dev);
    netif_receive_skb(skb);
}
```

---

## Summary: SKB Quick Reference

```
+============================================================================+
|                         SKB QUICK REFERENCE                                 |
+============================================================================+
|                                                                             |
|   ALLOCATION:                                                               |
|   alloc_skb(size, GFP_ATOMIC)           General allocation                 |
|   netdev_alloc_skb(dev, len)            For RX, with headroom              |
|   netdev_alloc_skb_ip_align(dev, len)   For RX, IP aligned                 |
|   build_skb(data, frag_size)            From existing buffer               |
|                                                                             |
|   DATA MANIPULATION:                                                        |
|   skb_reserve(skb, len)     Reserve headroom (empty SKB only)              |
|   skb_put(skb, len)         Add data at tail                               |
|   skb_push(skb, len)        Add header at front                            |
|   skb_pull(skb, len)        Remove data from front                         |
|   skb_trim(skb, len)        Truncate to length                             |
|                                                                             |
|   HEADER ACCESS:                                                            |
|   skb_reset_mac_header(skb)         Set mac_header = data                  |
|   skb_reset_network_header(skb)     Set network_header = data              |
|   skb_reset_transport_header(skb)   Set transport_header = data            |
|   eth_hdr(skb), ip_hdr(skb), tcp_hdr(skb)   Type-cast accessors           |
|                                                                             |
|   SPACE QUERIES:                                                            |
|   skb_headroom(skb)         Bytes before data                              |
|   skb_tailroom(skb)         Bytes after tail                               |
|   skb_headlen(skb)          Linear data length                             |
|   skb_is_nonlinear(skb)     Has fragments?                                 |
|                                                                             |
|   FRAGMENT ACCESS:                                                          |
|   skb_shinfo(skb)           Get skb_shared_info                            |
|   skb_shinfo(skb)->nr_frags Number of page fragments                       |
|   skb_shinfo(skb)->frags[]  Fragment array                                 |
|   skb_shinfo(skb)->frag_list Chained SKBs                                  |
|                                                                             |
|   FREE:                                                                     |
|   kfree_skb(skb)            Normal free (drop)                             |
|   consume_skb(skb)          Free after processing                          |
|   dev_kfree_skb_any(skb)    Safe in any context                            |
|                                                                             |
|   KEY INVARIANTS:                                                           |
|   head <= data <= tail <= end                                               |
|   len = linear_len + data_len                                               |
|   skb_headlen = tail - data = len - data_len                               |
|                                                                             |
+============================================================================+
```

**中文说明**：
- 这是 SKB 常用 API 的快速参考表
- 涵盖分配、数据操作、头部访问、空间查询、分片访问和释放等类别
- 记住关键不变量：`head <= data <= tail <= end`

---

## References

1. Linux Kernel Source - `include/linux/skbuff.h`
2. Linux Kernel Source - `net/core/skbuff.c`
3. [Linux Foundation - sk_buff](https://wiki.linuxfoundation.org/networking/sk_buff)
4. Understanding Linux Network Internals - Christian Benvenuti

