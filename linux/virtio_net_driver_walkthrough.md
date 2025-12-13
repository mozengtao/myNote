# Linux virtio_net Driver: End-to-End Code-Level Walkthrough

This document provides a comprehensive, code-level walkthrough of the Linux `virtio_net` network driver, covering RX/TX rings, DMA, NAPI, SKB, and the complete data path.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Key Data Structures](#2-key-data-structures)
3. [Driver Initialization](#3-driver-initialization)
4. [Transmit (TX) Path](#4-transmit-tx-path)
5. [Receive (RX) Path](#5-receive-rx-path)
6. [NAPI Polling Mechanism](#6-napi-polling-mechanism)
7. [Virtqueue Ring Operations](#7-virtqueue-ring-operations)
8. [Complete Data Flow Summary](#8-complete-data-flow-summary)

---

## 1. Architecture Overview

```
+===========================================================================+
|                         GUEST (Linux Kernel)                               |
+===========================================================================+
|                                                                            |
|   +------------------+      +------------------+      +-----------------+  |
|   |   Application    |      |   TCP/IP Stack   |      |   Socket Layer  |  |
|   +--------|---------+      +--------|---------+      +--------|--------+  |
|            |                         |                         |           |
|            v                         v                         v           |
|   +------------------------------------------------------------------------+
|   |                        Network Core (net_device)                       |
|   |     - netif_receive_skb() (RX)    - dev_queue_xmit() (TX)             |
|   +------------------------------|--------|-------------------------------+
|                                  |        |                                |
|                                  v        v                                |
|   +------------------------------------------------------------------------+
|   |                          virtio_net Driver                             |
|   |  +------------------+  +------------------+  +------------------+      |
|   |  |    NAPI Poll     |  |  start_xmit()   |  |  virtnet_info   |      |
|   |  | virtnet_poll()   |  |  xmit_skb()     |  |  (driver state) |      |
|   |  +--------|--------+  +--------|--------+  +------------------+      |
|   |           |                    |                                      |
|   +-----------|--------------------|----- --------------------------------+
|               |                    |                                       |
|               v                    v                                       |
|   +------------------------------------------------------------------------+
|   |                         Virtqueue Layer                                |
|   |  +------------------+                    +------------------+          |
|   |  |    RX Virtqueue  |                    |   TX Virtqueue   |          |
|   |  |      (rvq)       |                    |      (svq)       |          |
|   |  +--------|--------+                    +--------|--------+          |
|   |           |                                      |                    |
|   +-----------|--------------------------------------|----- --------------+
|               |                                      |                     |
|               v                                      v                     |
|   +------------------------------------------------------------------------+
|   |                     Vring (Shared Memory)                              |
|   |                                                                        |
|   |   +-------------+    +-------------+    +-------------+               |
|   |   | Descriptor  |    | Available   |    |    Used     |               |
|   |   |    Table    |    |    Ring     |    |    Ring     |               |
|   |   +-------------+    +-------------+    +-------------+               |
|   |                                                                        |
+---|------------------------------------------------------------------------|
    |                    Shared Memory (DMA-able)                            |
    v                                                                        v
+===========================================================================+
|                           HOST (Hypervisor/QEMU)                           |
|                                                                            |
|   +------------------+                         +------------------+        |
|   | virtio-net-pci   |<---- PCI/MMIO/IO ----->|   TAP Device     |        |
|   |   (backend)      |                         |  (Host Network)  |        |
|   +------------------+                         +------------------+        |
|                                                                            |
+===========================================================================+
```

**中文说明**：
- Guest（虚拟机）中的 Linux 内核运行 virtio_net 驱动
- 驱动通过 Virtqueue 与 Host（宿主机/QEMU）通信
- Vring 是共享内存区域，包含描述符表、可用环和已用环
- Host 通过 TAP 设备连接到实际网络

---

## 2. Key Data Structures

### 2.1 virtnet_info - Driver Private Data

```c
/* File: drivers/net/virtio_net.c */

struct virtnet_info {
    struct virtio_device *vdev;         /* virtio device reference */
    struct virtqueue *rvq, *svq, *cvq;  /* RX, TX, Control queues */
    struct net_device *dev;             /* network device */
    struct napi_struct napi;            /* NAPI polling structure */
    unsigned int status;                /* link status */

    unsigned int num, max;              /* buffer counts */
    bool big_packets;                   /* support large packets */
    bool mergeable_rx_bufs;             /* mergeable RX buffers */

    struct virtnet_stats __percpu *stats;  /* per-CPU statistics */
    struct delayed_work refill;            /* buffer refill work */
    struct page *pages;                    /* page pool */

    struct scatterlist rx_sg[MAX_SKB_FRAGS + 2];  /* RX scatter-gather */
    struct scatterlist tx_sg[MAX_SKB_FRAGS + 2];  /* TX scatter-gather */
};
```

**中文说明**：
- `virtnet_info` 是驱动的私有数据结构，存储在 `net_device->priv` 中
- 包含 RX/TX/Control 三个 virtqueue 的引用
- `napi` 结构用于 NAPI 轮询机制
- `rx_sg` 和 `tx_sg` 是 scatter-gather 列表，用于 DMA 操作

### 2.2 Virtqueue and Vring Structures

```
+============================================================================+
|                         VRING MEMORY LAYOUT                                 |
+============================================================================+
|                                                                             |
|   Descriptor Table (desc[])                                                 |
|   +------+------+------+------+------+------+------+------+                |
|   |  0   |  1   |  2   |  3   |  4   |  5   |  6   |  7   | ... [N-1]      |
|   +------+------+------+------+------+------+------+------+                |
|   | addr | addr | addr | addr | addr | addr | addr | addr |                |
|   | len  | len  | len  | len  | len  | len  | len  | len  |                |
|   |flags |flags |flags |flags |flags |flags |flags |flags |                |
|   | next | next | next | next | next | next | next | next |                |
|   +------+------+------+------+------+------+------+------+                |
|                                                                             |
|   Available Ring (avail)                 Used Ring (used)                  |
|   +-------------------+                  +-------------------+             |
|   | flags             |                  | flags             |             |
|   | idx (next to use) |                  | idx (next to use) |             |
|   +-------------------+                  +-------------------+             |
|   | ring[0]  -> desc  |                  | ring[0].id        |             |
|   | ring[1]  -> desc  |                  | ring[0].len       |             |
|   | ring[2]  -> desc  |                  | ring[1].id        |             |
|   | ...               |                  | ring[1].len       |             |
|   | ring[N-1]         |                  | ...               |             |
|   +-------------------+                  +-------------------+             |
|                                                                             |
|   Direction:                                                                |
|   Guest --> Host: avail ring points to descriptors                         |
|   Host --> Guest: used ring returns processed descriptors                  |
|                                                                             |
+============================================================================+
```

```c
/* File: include/linux/virtio_ring.h */

/* Descriptor: 16 bytes each */
struct vring_desc {
    __u64 addr;    /* Guest-physical address of buffer */
    __u32 len;     /* Length of buffer */
    __u16 flags;   /* VRING_DESC_F_NEXT, VRING_DESC_F_WRITE, etc. */
    __u16 next;    /* Index of next descriptor in chain */
};

/* Available ring - Guest adds descriptors here */
struct vring_avail {
    __u16 flags;
    __u16 idx;       /* Where driver would put next descriptor */
    __u16 ring[];    /* Array of descriptor indices */
};

/* Used ring - Host returns completed descriptors here */
struct vring_used_elem {
    __u32 id;        /* Index of start of used descriptor chain */
    __u32 len;       /* Total bytes written to buffer */
};

struct vring_used {
    __u16 flags;
    __u16 idx;       /* Where device would put next descriptor */
    struct vring_used_elem ring[];
};
```

**中文说明**：
- **描述符表**：存储缓冲区的物理地址、长度和标志
- **可用环（Available Ring）**：Guest 向 Host 发送数据时使用，存放描述符索引
- **已用环（Used Ring）**：Host 处理完成后返回描述符给 Guest
- 这是一种无锁的生产者-消费者模型

### 2.3 virtio_net_hdr - Packet Header

```c
/* File: include/linux/virtio_net.h */

struct virtio_net_hdr {
    __u8 flags;           /* VIRTIO_NET_HDR_F_NEEDS_CSUM, etc. */
    __u8 gso_type;        /* GSO type: NONE, TCPV4, UDP, TCPV6 */
    __u16 hdr_len;        /* Ethernet + IP + TCP/UDP header length */
    __u16 gso_size;       /* Bytes to append per frame for GSO */
    __u16 csum_start;     /* Checksum start offset */
    __u16 csum_offset;    /* Checksum field offset from csum_start */
};

/* For mergeable RX buffers */
struct virtio_net_hdr_mrg_rxbuf {
    struct virtio_net_hdr hdr;
    __u16 num_buffers;    /* Number of merged buffers */
};
```

**中文说明**：
- 每个网络包前都有一个 `virtio_net_hdr` 头
- 包含校验和卸载（checksum offload）和 GSO（Generic Segmentation Offload）信息
- Host 和 Guest 通过这个头交换卸载功能的元数据

---

## 3. Driver Initialization

### 3.1 Module and Driver Registration

```c
/* File: drivers/net/virtio_net.c */

static struct virtio_driver virtio_net_driver = {
    .feature_table = features,
    .feature_table_size = ARRAY_SIZE(features),
    .driver.name = KBUILD_MODNAME,
    .driver.owner = THIS_MODULE,
    .id_table = id_table,
    .probe = virtnet_probe,        /* <-- Called when device found */
    .remove = virtnet_remove,
    .config_changed = virtnet_config_changed,
};

static int __init init(void)
{
    return register_virtio_driver(&virtio_net_driver);
}
module_init(init);
```

### 3.2 Device Probe - virtnet_probe()

```
+============================================================================+
|                     VIRTNET_PROBE INITIALIZATION FLOW                       |
+============================================================================+
|                                                                             |
|   +------------------+                                                      |
|   |  virtnet_probe() |                                                      |
|   +--------|---------+                                                      |
|            |                                                                |
|            v                                                                |
|   +--------------------+                                                    |
|   | alloc_etherdev()   |  Allocate net_device + virtnet_info                |
|   +--------|-----------+                                                    |
|            |                                                                |
|            v                                                                |
|   +--------------------+                                                    |
|   | Setup net_device   |  Set netdev_ops, features, MAC address             |
|   +--------|-----------+                                                    |
|            |                                                                |
|            v                                                                |
|   +--------------------+                                                    |
|   | netif_napi_add()   |  Register NAPI with virtnet_poll()                 |
|   +--------|-----------+                                                    |
|            |                                                                |
|            v                                                                |
|   +--------------------+                                                    |
|   | find_vqs()         |  Create RX, TX, Control virtqueues                 |
|   +--------|-----------+                                                    |
|            |                                                                |
|            v                                                                |
|   +--------------------+                                                    |
|   | register_netdev()  |  Register with network stack                       |
|   +--------|-----------+                                                    |
|            |                                                                |
|            v                                                                |
|   +--------------------+                                                    |
|   | try_fill_recv()    |  Pre-allocate RX buffers                           |
|   +--------------------+                                                    |
|                                                                             |
+============================================================================+
```

```c
static int virtnet_probe(struct virtio_device *vdev)
{
    int err;
    struct net_device *dev;
    struct virtnet_info *vi;
    struct virtqueue *vqs[3];
    vq_callback_t *callbacks[] = { skb_recv_done, skb_xmit_done, NULL };
    const char *names[] = { "input", "output", "control" };

    /* Step 1: Allocate network device with private data */
    dev = alloc_etherdev(sizeof(struct virtnet_info));
    if (!dev)
        return -ENOMEM;

    /* Step 2: Configure network device */
    dev->netdev_ops = &virtnet_netdev;
    dev->features = NETIF_F_HIGHDMA;
    
    /* Step 3: Setup checksum and GSO features */
    if (virtio_has_feature(vdev, VIRTIO_NET_F_CSUM)) {
        dev->hw_features |= NETIF_F_HW_CSUM | NETIF_F_SG | NETIF_F_FRAGLIST;
        /* ... more feature setup ... */
    }

    /* Step 4: Get MAC address from config or generate random */
    if (virtio_config_val_len(vdev, VIRTIO_NET_F_MAC, ...) < 0)
        random_ether_addr(dev->dev_addr);

    /* Step 5: Initialize driver private data */
    vi = netdev_priv(dev);
    netif_napi_add(dev, &vi->napi, virtnet_poll, napi_weight);
    vi->dev = dev;
    vi->vdev = vdev;

    /* Step 6: Find and setup virtqueues */
    err = vdev->config->find_vqs(vdev, nvqs, vqs, callbacks, names);
    vi->rvq = vqs[0];  /* Receive queue */
    vi->svq = vqs[1];  /* Send queue */
    vi->cvq = vqs[2];  /* Control queue (optional) */

    /* Step 7: Register network device */
    err = register_netdev(dev);

    /* Step 8: Pre-fill receive buffers */
    try_fill_recv(vi, GFP_KERNEL);

    return 0;
}
```

**中文说明**：
1. 分配 `net_device` 结构和私有数据空间
2. 设置网络设备操作函数（`netdev_ops`）
3. 协商硬件特性（校验和卸载、GSO 等）
4. 获取或生成 MAC 地址
5. 初始化 NAPI 结构，注册轮询函数
6. 创建 RX/TX virtqueue
7. 向网络栈注册设备
8. 预分配接收缓冲区

---

## 4. Transmit (TX) Path

### 4.1 TX Path Overview

```
+============================================================================+
|                         TRANSMIT (TX) DATA PATH                             |
+============================================================================+
|                                                                             |
|   Application                                                               |
|       |                                                                     |
|       v                                                                     |
|   +--------------------+                                                    |
|   | Socket send()      |                                                    |
|   +--------|-----------+                                                    |
|            |                                                                |
|            v                                                                |
|   +--------------------+                                                    |
|   | TCP/IP Stack       |  Build headers, checksum (or defer)                |
|   +--------|-----------+                                                    |
|            |                                                                |
|            v                                                                |
|   +--------------------+                                                    |
|   | dev_queue_xmit()   |  Network core                                      |
|   +--------|-----------+                                                    |
|            |                                                                |
|            v                                                                |
|   +========================================+                                |
|   |           virtio_net Driver            |                                |
|   +========================================+                                |
|   |                                        |                                |
|   |   +------------------------+           |                                |
|   |   |    start_xmit()        |           |                                |
|   |   |   (ndo_start_xmit)     |           |                                |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------+           |                                |
|   |   |  free_old_xmit_skbs()  |  Reclaim completed TX buffers             |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------+           |                                |
|   |   |     xmit_skb()         |           |                                |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +---------------------------+        |                                |
|   |   | Fill virtio_net_hdr      |        |                                |
|   |   | (csum offload, GSO info)  |        |                                |
|   |   +-----------|---------------+        |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +---------------------------+        |                                |
|   |   | Build scatter-gather list |        |                                |
|   |   | sg[0] = header            |        |                                |
|   |   | sg[1..N] = skb data       |        |                                |
|   |   +-----------|---------------+        |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +---------------------------+        |                                |
|   |   | virtqueue_add_buf()       |  Add to TX vring                       |
|   |   +-----------|---------------+        |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +---------------------------+        |                                |
|   |   | virtqueue_kick()          |  Notify Host                           |
|   |   +---------------------------+        |                                |
|   |                                        |                                |
|   +========================================+                                |
|               |                                                             |
|               v                                                             |
|   +--------------------+                                                    |
|   | Host (QEMU/KVM)    |  Process packet, send to network                   |
|   +--------------------+                                                    |
|                                                                             |
+============================================================================+
```

**中文说明**：
1. 应用程序通过 socket 发送数据
2. TCP/IP 协议栈构建协议头
3. 网络核心层调用驱动的 `start_xmit()`
4. 驱动回收已完成的发送缓冲区
5. 填充 `virtio_net_hdr` 头（校验和、GSO 信息）
6. 构建 scatter-gather 列表
7. 通过 `virtqueue_add_buf()` 添加到发送队列
8. 通过 `virtqueue_kick()` 通知 Host

### 4.2 start_xmit() - Main TX Entry Point

```c
static netdev_tx_t start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct virtnet_info *vi = netdev_priv(dev);
    int capacity;

    /* Step 1: Reclaim completed TX buffers */
    free_old_xmit_skbs(vi);

    /* Step 2: Add new packet to TX queue */
    capacity = xmit_skb(vi, skb);

    /* Step 3: Handle queue full */
    if (unlikely(capacity < 0)) {
        dev->stats.tx_dropped++;
        kfree_skb(skb);
        return NETDEV_TX_OK;
    }

    /* Step 4: Notify host */
    virtqueue_kick(vi->svq);

    /* Step 5: Orphan skb (don't wait for completion) */
    skb_orphan(skb);
    nf_reset(skb);

    /* Step 6: Flow control - stop queue if nearly full */
    if (capacity < 2 + MAX_SKB_FRAGS) {
        netif_stop_queue(dev);
        if (unlikely(!virtqueue_enable_cb_delayed(vi->svq))) {
            capacity += free_old_xmit_skbs(vi);
            if (capacity >= 2 + MAX_SKB_FRAGS) {
                netif_start_queue(dev);
                virtqueue_disable_cb(vi->svq);
            }
        }
    }

    return NETDEV_TX_OK;
}
```

### 4.3 xmit_skb() - Prepare and Queue Packet

```c
static int xmit_skb(struct virtnet_info *vi, struct sk_buff *skb)
{
    struct skb_vnet_hdr *hdr = skb_vnet_hdr(skb);

    /* Step 1: Setup checksum offload in header */
    if (skb->ip_summed == CHECKSUM_PARTIAL) {
        hdr->hdr.flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
        hdr->hdr.csum_start = skb_checksum_start_offset(skb);
        hdr->hdr.csum_offset = skb->csum_offset;
    } else {
        hdr->hdr.flags = 0;
    }

    /* Step 2: Setup GSO (segmentation offload) in header */
    if (skb_is_gso(skb)) {
        hdr->hdr.hdr_len = skb_headlen(skb);
        hdr->hdr.gso_size = skb_shinfo(skb)->gso_size;
        if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV4)
            hdr->hdr.gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
        /* ... other GSO types ... */
    } else {
        hdr->hdr.gso_type = VIRTIO_NET_HDR_GSO_NONE;
    }

    /* Step 3: Build scatter-gather list */
    /* sg[0] = virtio_net_hdr */
    if (vi->mergeable_rx_bufs)
        sg_set_buf(vi->tx_sg, &hdr->mhdr, sizeof(hdr->mhdr));
    else
        sg_set_buf(vi->tx_sg, &hdr->hdr, sizeof(hdr->hdr));

    /* sg[1..N] = skb data fragments */
    hdr->num_sg = skb_to_sgvec(skb, vi->tx_sg + 1, 0, skb->len) + 1;

    /* Step 4: Add to virtqueue */
    return virtqueue_add_buf(vi->svq, vi->tx_sg, hdr->num_sg, 0, skb);
}
```

**中文说明**：
- **校验和卸载**：如果上层设置了 `CHECKSUM_PARTIAL`，填充校验和起始位置和偏移
- **GSO 卸载**：如果是大包需要分段，填充 GSO 类型和大小
- **Scatter-Gather**：将 `virtio_net_hdr` 和 SKB 数据映射为 SG 列表
- **添加到队列**：调用 `virtqueue_add_buf()` 将描述符添加到可用环

### 4.4 TX Completion - free_old_xmit_skbs()

```c
static unsigned int free_old_xmit_skbs(struct virtnet_info *vi)
{
    struct sk_buff *skb;
    unsigned int len, tot_sgs = 0;
    struct virtnet_stats *stats = this_cpu_ptr(vi->stats);

    /* Poll used ring for completed transmissions */
    while ((skb = virtqueue_get_buf(vi->svq, &len)) != NULL) {
        /* Update statistics */
        u64_stats_update_begin(&stats->syncp);
        stats->tx_bytes += skb->len;
        stats->tx_packets++;
        u64_stats_update_end(&stats->syncp);

        tot_sgs += skb_vnet_hdr(skb)->num_sg;
        dev_kfree_skb_any(skb);  /* Free the skb */
    }
    return tot_sgs;
}
```

**中文说明**：
- 从已用环中获取 Host 处理完成的缓冲区
- 更新统计信息
- 释放 SKB 内存

---

## 5. Receive (RX) Path

### 5.1 RX Path Overview

```
+============================================================================+
|                         RECEIVE (RX) DATA PATH                              |
+============================================================================+
|                                                                             |
|   Host (QEMU/KVM)                                                           |
|       |                                                                     |
|       v                                                                     |
|   +--------------------+                                                    |
|   | Packet arrives     |  From TAP device / network                         |
|   +--------|-----------+                                                    |
|            |                                                                |
|            v                                                                |
|   +--------------------+                                                    |
|   | Host fills buffer  |  Write to guest memory via DMA                     |
|   | Update used ring   |                                                    |
|   +--------|-----------+                                                    |
|            |                                                                |
|            v                                                                |
|   +--------------------+                                                    |
|   | Raise Interrupt    |  Notify guest                                      |
|   +--------|-----------+                                                    |
|            |                                                                |
|   =========|============= GUEST KERNEL ===================================  |
|            |                                                                |
|            v                                                                |
|   +========================================+                                |
|   |          virtio_net Driver             |                                |
|   +========================================+                                |
|   |                                        |                                |
|   |   +------------------------+           |                                |
|   |   |   skb_recv_done()      |  IRQ handler (callback)                   |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------+           |                                |
|   |   | napi_schedule()        |  Schedule NAPI poll                       |
|   |   | virtqueue_disable_cb() |  Disable further IRQs                     |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +------------------------+           |                                |
|   |   |   virtnet_poll()       |  NAPI poll callback                       |
|   |   +-----------|------------+           |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +---------------------------+        |                                |
|   |   | while (budget && buffers) |        |                                |
|   |   |   virtqueue_get_buf()     |  Get completed RX buffers              |
|   |   |   receive_buf()           |  Process each buffer                   |
|   |   +-----------|---------------+        |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +---------------------------+        |                                |
|   |   | receive_buf():            |        |                                |
|   |   |   page_to_skb()           |  Convert page to SKB                   |
|   |   |   Handle csum/GSO flags   |        |                                |
|   |   |   netif_receive_skb()     |  Pass to network stack                 |
|   |   +---------------------------+        |                                |
|   |               |                        |                                |
|   |               v                        |                                |
|   |   +---------------------------+        |                                |
|   |   | try_fill_recv()           |  Refill RX buffers                     |
|   |   +---------------------------+        |                                |
|   |                                        |                                |
|   +========================================+                                |
|               |                                                             |
|               v                                                             |
|   +--------------------+                                                    |
|   | TCP/IP Stack       |                                                    |
|   +--------------------+                                                    |
|               |                                                             |
|               v                                                             |
|   +--------------------+                                                    |
|   | Application        |                                                    |
|   +--------------------+                                                    |
|                                                                             |
+============================================================================+
```

**中文说明**：
1. Host 从网络收到数据包
2. Host 将数据写入 Guest 预分配的缓冲区
3. Host 更新已用环并触发中断
4. Guest 中断处理程序调度 NAPI
5. NAPI 轮询函数处理多个数据包
6. 将页面转换为 SKB 并传递给网络栈
7. 补充 RX 缓冲区

### 5.2 IRQ Handler - skb_recv_done()

```c
static void skb_recv_done(struct virtqueue *rvq)
{
    struct virtnet_info *vi = rvq->vdev->priv;

    /* Schedule NAPI, suppress further interrupts if successful */
    if (napi_schedule_prep(&vi->napi)) {
        virtqueue_disable_cb(rvq);   /* Disable interrupts */
        __napi_schedule(&vi->napi);  /* Schedule polling */
    }
}
```

**中文说明**：
- 这是 RX virtqueue 的回调函数，在中断上下文中执行
- 禁用进一步的中断（避免中断风暴）
- 调度 NAPI 轮询，转为轮询模式处理

### 5.3 NAPI Poll - virtnet_poll()

```c
static int virtnet_poll(struct napi_struct *napi, int budget)
{
    struct virtnet_info *vi = container_of(napi, struct virtnet_info, napi);
    void *buf;
    unsigned int len, received = 0;

again:
    /* Process received packets up to budget */
    while (received < budget &&
           (buf = virtqueue_get_buf(vi->rvq, &len)) != NULL) {
        receive_buf(vi->dev, buf, len);  /* Process packet */
        --vi->num;
        received++;
    }

    /* Refill RX buffers if running low */
    if (vi->num < vi->max / 2) {
        if (!try_fill_recv(vi, GFP_ATOMIC))
            schedule_delayed_work(&vi->refill, 0);
    }

    /* Check if we've processed all packets */
    if (received < budget) {
        napi_complete(napi);  /* Exit polling mode */
        
        /* Re-enable interrupts, check for race */
        if (unlikely(!virtqueue_enable_cb(vi->rvq)) &&
            napi_schedule_prep(napi)) {
            virtqueue_disable_cb(vi->rvq);
            __napi_schedule(napi);
            goto again;  /* More packets arrived! */
        }
    }

    return received;
}
```

**中文说明**：
- **Budget（预算）**：每次轮询最多处理的包数量（默认 128）
- **轮询循环**：从已用环获取缓冲区并处理
- **缓冲区补充**：如果缓冲区数量低于一半，尝试补充
- **退出轮询**：处理完所有包后，重新启用中断
- **竞态处理**：重新启用中断时检查是否有新包到达

### 5.4 Buffer Processing - receive_buf()

```c
static void receive_buf(struct net_device *dev, void *buf, unsigned int len)
{
    struct virtnet_info *vi = netdev_priv(dev);
    struct sk_buff *skb;
    struct skb_vnet_hdr *hdr;

    /* Step 1: Validate packet length */
    if (unlikely(len < sizeof(struct virtio_net_hdr) + ETH_HLEN)) {
        dev->stats.rx_length_errors++;
        /* Free buffer and return */
        return;
    }

    /* Step 2: Convert buffer to SKB */
    if (!vi->mergeable_rx_bufs && !vi->big_packets) {
        skb = buf;  /* Buffer is already an SKB */
        len -= sizeof(struct virtio_net_hdr);
        skb_trim(skb, len);
    } else {
        struct page *page = buf;
        skb = page_to_skb(vi, page, len);  /* Convert page to SKB */
        if (vi->mergeable_rx_bufs)
            receive_mergeable(vi, skb);  /* Handle merged buffers */
    }

    hdr = skb_vnet_hdr(skb);

    /* Step 3: Update statistics */
    stats->rx_bytes += skb->len;
    stats->rx_packets++;

    /* Step 4: Handle checksum offload */
    if (hdr->hdr.flags & VIRTIO_NET_HDR_F_NEEDS_CSUM) {
        /* Partial checksum - tell stack where to compute */
        skb_partial_csum_set(skb, hdr->hdr.csum_start, hdr->hdr.csum_offset);
    } else if (hdr->hdr.flags & VIRTIO_NET_HDR_F_DATA_VALID) {
        skb->ip_summed = CHECKSUM_UNNECESSARY;  /* Host verified */
    }

    /* Step 5: Determine protocol */
    skb->protocol = eth_type_trans(skb, dev);

    /* Step 6: Handle GSO (LRO) */
    if (hdr->hdr.gso_type != VIRTIO_NET_HDR_GSO_NONE) {
        /* Setup GSO info for stack to segment */
        switch (hdr->hdr.gso_type & ~VIRTIO_NET_HDR_GSO_ECN) {
        case VIRTIO_NET_HDR_GSO_TCPV4:
            skb_shinfo(skb)->gso_type = SKB_GSO_TCPV4;
            break;
        /* ... other types ... */
        }
        skb_shinfo(skb)->gso_size = hdr->hdr.gso_size;
    }

    /* Step 7: Pass to network stack */
    netif_receive_skb(skb);
}
```

**中文说明**：
1. **验证包长**：确保数据包长度有效
2. **转换为 SKB**：根据缓冲区类型转换为 `sk_buff`
3. **统计更新**：记录接收字节数和包数
4. **校验和处理**：根据 Host 提供的信息设置校验和状态
5. **协议识别**：通过以太网头确定上层协议
6. **GSO 处理**：如果是合并的大包，设置 GSO 信息
7. **传递给协议栈**：调用 `netif_receive_skb()`

### 5.5 Pre-allocating RX Buffers - try_fill_recv()

```c
static bool try_fill_recv(struct virtnet_info *vi, gfp_t gfp)
{
    int err;
    bool oom;

    do {
        /* Allocate buffer based on configuration */
        if (vi->mergeable_rx_bufs)
            err = add_recvbuf_mergeable(vi, gfp);
        else if (vi->big_packets)
            err = add_recvbuf_big(vi, gfp);
        else
            err = add_recvbuf_small(vi, gfp);

        oom = (err == -ENOMEM);
        if (err < 0)
            break;
        ++vi->num;
    } while (err > 0);  /* Continue while queue has space */

    /* Update max buffer count */
    if (unlikely(vi->num > vi->max))
        vi->max = vi->num;

    /* Notify host of new buffers */
    virtqueue_kick(vi->rvq);

    return !oom;
}

/* Add a single-page mergeable buffer */
static int add_recvbuf_mergeable(struct virtnet_info *vi, gfp_t gfp)
{
    struct page *page;

    page = get_a_page(vi, gfp);  /* Allocate or reuse page */
    if (!page)
        return -ENOMEM;

    sg_init_one(vi->rx_sg, page_address(page), PAGE_SIZE);

    return virtqueue_add_buf_gfp(vi->rvq, vi->rx_sg, 0, 1, page, gfp);
}
```

**中文说明**：
- 根据配置选择不同的缓冲区分配策略
- **Mergeable**：单页缓冲区，可合并多个接收大包
- **Big**：多页缓冲区链，支持 GSO 大包
- **Small**：预分配 SKB，适合小包
- 分配后通过 `virtqueue_kick()` 通知 Host

---

## 6. NAPI Polling Mechanism

### 6.1 NAPI State Machine

```
+============================================================================+
|                         NAPI STATE MACHINE                                  |
+============================================================================+
|                                                                             |
|                        +-------------------+                                |
|                        |    NAPI DISABLED  |                                |
|                        +--------+----------+                                |
|                                 |                                           |
|                         napi_enable()                                       |
|                                 |                                           |
|                                 v                                           |
|   +-------------------+        +-----------------------+                    |
|   |  Interrupt Mode   |<-------|     NAPI IDLE        |                    |
|   | (IRQs enabled)    |        | (waiting for packets) |                    |
|   +--------+----------+        +-----------+-----------+                    |
|            |                               ^                                |
|            |                               |                                |
|    Packet arrives                   napi_complete()                         |
|    (IRQ triggered)                  + enable_cb()                          |
|            |                               |                                |
|            v                               |                                |
|   +-------------------+                    |                                |
|   | skb_recv_done()   |                    |                                |
|   | IRQ callback      |                    |                                |
|   +--------+----------+                    |                                |
|            |                               |                                |
|   napi_schedule_prep()                     |                                |
|   virtqueue_disable_cb()                   |                                |
|   __napi_schedule()                        |                                |
|            |                               |                                |
|            v                               |                                |
|   +-------------------+                    |                                |
|   |   Polling Mode    |                    |                                |
|   | (IRQs disabled)   |                    |                                |
|   +--------+----------+                    |                                |
|            |                               |                                |
|            v                               |                                |
|   +------------------------+               |                                |
|   | net_rx_action()        |               |                                |
|   | (softirq context)      |               |                                |
|   +--------+---------------+               |                                |
|            |                               |                                |
|            v                               |                                |
|   +------------------------+               |                                |
|   | napi->poll()           |               |                                |
|   | = virtnet_poll()       |               |                                |
|   +--------+---------------+               |                                |
|            |                               |                                |
|            +-------------------------------+                                |
|                 received < budget                                           |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **中断模式**：等待数据包，收到中断后进入轮询模式
- **轮询模式**：禁用中断，高效处理多个包
- **Budget 机制**：每次轮询最多处理一定数量的包，保证公平性
- **模式切换**：处理完所有包后，重新启用中断

### 6.2 NAPI Benefits

```
+============================================================================+
|                    INTERRUPT vs NAPI COMPARISON                             |
+============================================================================+
|                                                                             |
|   Traditional Interrupt Mode:                                               |
|   +---------------------------------------------------------+              |
|   | Packet1 | IRQ | Process | Packet2 | IRQ | Process | ... |              |
|   |         | ^   |    ^    |         | ^   |    ^    |     |              |
|   |         | |   |    |    |         | |   |    |    |     |              |
|   |      Context Switch  Context Switch  Context Switch     |              |
|   |         (expensive)    (expensive)    (expensive)        |              |
|   +---------------------------------------------------------+              |
|                                                                             |
|   NAPI Polling Mode:                                                        |
|   +-------------------------------------------------------------+          |
|   | Pkt1 | IRQ | Poll: Pkt1 Pkt2 Pkt3 Pkt4 ... | Enable IRQ   |          |
|   |      | ^   |   ^                            |      ^       |          |
|   |      | |   |   |                            |      |       |          |
|   |   Single   Batch process multiple packets   Re-enable      |          |
|   |   context  (no context switches)            interrupts     |          |
|   |   switch                                                    |          |
|   +-------------------------------------------------------------+          |
|                                                                             |
|   Key Benefits:                                                             |
|   1. Reduced interrupt overhead (batch processing)                          |
|   2. Better CPU cache utilization                                           |
|   3. Adaptive: reverts to IRQ mode when traffic is low                     |
|   4. Prevents livelock under high load                                      |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **传统中断模式**：每个包都触发中断和上下文切换，开销大
- **NAPI 模式**：批量处理多个包，减少中断和上下文切换
- **自适应**：低负载时用中断（低延迟），高负载时用轮询（高吞吐）
- **防活锁**：Budget 机制防止 CPU 被网络处理占满

---

## 7. Virtqueue Ring Operations

### 7.1 Adding Buffer to Queue - virtqueue_add_buf()

```c
/* File: drivers/virtio/virtio_ring.c */

int virtqueue_add_buf_gfp(struct virtqueue *_vq,
                          struct scatterlist sg[],
                          unsigned int out,  /* # readable by host */
                          unsigned int in,   /* # writable by host */
                          void *data,        /* token for identification */
                          gfp_t gfp)
{
    struct vring_virtqueue *vq = to_vvq(_vq);
    unsigned int i, avail, prev;
    int head;

    /* Check for indirect buffer support */
    if (vq->indirect && (out + in) > 1 && vq->num_free) {
        head = vring_add_indirect(vq, sg, out, in, gfp);
        if (likely(head >= 0))
            goto add_head;
    }

    /* Check for space */
    if (vq->num_free < out + in)
        return -ENOSPC;

    vq->num_free -= out + in;

    /* Fill in descriptors */
    head = vq->free_head;
    
    /* Output (readable by host) descriptors */
    for (i = vq->free_head; out; i = vq->vring.desc[i].next, out--) {
        vq->vring.desc[i].flags = VRING_DESC_F_NEXT;
        vq->vring.desc[i].addr = sg_phys(sg);  /* Physical address! */
        vq->vring.desc[i].len = sg->length;
        prev = i;
        sg++;
    }
    
    /* Input (writable by host) descriptors */
    for (; in; i = vq->vring.desc[i].next, in--) {
        vq->vring.desc[i].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
        vq->vring.desc[i].addr = sg_phys(sg);
        vq->vring.desc[i].len = sg->length;
        prev = i;
        sg++;
    }
    vq->vring.desc[prev].flags &= ~VRING_DESC_F_NEXT;  /* End of chain */

    vq->free_head = i;  /* Update free list */

add_head:
    vq->data[head] = data;  /* Store token */

    /* Add to available ring */
    avail = (vq->vring.avail->idx + vq->num_added++) % vq->vring.num;
    vq->vring.avail->ring[avail] = head;

    return vq->num_free;
}
```

**中文说明**：
- **Scatter-Gather**：将多个不连续的内存块组织成描述符链
- **物理地址**：使用 `sg_phys()` 获取物理地址（DMA 需要）
- **描述符链**：用 `VRING_DESC_F_NEXT` 标志链接多个描述符
- **可用环更新**：将描述符链头索引添加到可用环

### 7.2 Notifying Host - virtqueue_kick()

```c
void virtqueue_kick(struct virtqueue *_vq)
{
    struct vring_virtqueue *vq = to_vvq(_vq);
    u16 new, old;

    /* Memory barrier: ensure descriptors are visible before index update */
    virtio_wmb();

    old = vq->vring.avail->idx;
    new = vq->vring.avail->idx = old + vq->num_added;
    vq->num_added = 0;

    /* Memory barrier: ensure index is visible before checking notify flag */
    virtio_mb();

    /* Notify if needed */
    if (vq->event ?
        vring_need_event(vring_avail_event(&vq->vring), new, old) :
        !(vq->vring.used->flags & VRING_USED_F_NO_NOTIFY))
        vq->notify(&vq->vq);  /* Trigger host notification (e.g., PIO write) */
}
```

**中文说明**：
- **内存屏障**：确保描述符写入在索引更新之前对 Host 可见
- **索引更新**：更新可用环的索引
- **通知优化**：只在需要时通知 Host（EVENT_IDX 或 NO_NOTIFY 标志）
- **通知机制**：通常是 PIO 写或 MMIO 写触发 VM-Exit

### 7.3 Getting Completed Buffer - virtqueue_get_buf()

```c
void *virtqueue_get_buf(struct virtqueue *_vq, unsigned int *len)
{
    struct vring_virtqueue *vq = to_vvq(_vq);
    void *ret;
    unsigned int i;
    u16 last_used;

    /* Check if there are completed buffers */
    if (!more_used(vq))
        return NULL;

    /* Memory barrier: ensure we see used->ring update */
    virtio_rmb();

    /* Get the used element */
    last_used = vq->last_used_idx & (vq->vring.num - 1);
    i = vq->vring.used->ring[last_used].id;
    *len = vq->vring.used->ring[last_used].len;

    /* Get the stored token */
    ret = vq->data[i];

    /* Free the descriptors */
    detach_buf(vq, i);

    vq->last_used_idx++;

    /* Event suppression */
    if (!(vq->vring.avail->flags & VRING_AVAIL_F_NO_INTERRUPT)) {
        vring_used_event(&vq->vring) = vq->last_used_idx;
        virtio_mb();
    }

    return ret;
}
```

**中文说明**：
- 检查已用环是否有新的完成项
- 获取已用描述符的索引和长度
- 返回之前存储的 token（通常是 SKB 或 page 指针）
- 将描述符归还到空闲列表
- 更新事件抑制索引

---

## 8. Complete Data Flow Summary

### 8.1 TX Complete Flow

```
+============================================================================+
|                    COMPLETE TX FLOW (Code Path)                             |
+============================================================================+
|                                                                             |
|   Application: send(socket, data, len)                                      |
|                      |                                                      |
|                      v                                                      |
|   Socket Layer: sock_sendmsg() --> tcp_sendmsg()                           |
|                      |                                                      |
|                      v                                                      |
|   TCP/IP Stack: tcp_transmit_skb() --> ip_queue_xmit()                     |
|                      |                                                      |
|                      v                                                      |
|   Network Core: dev_queue_xmit() --> __dev_queue_xmit()                    |
|                      |                                                      |
|                      v                                                      |
|   Qdisc: sch_direct_xmit() --> dev_hard_start_xmit()                       |
|                      |                                                      |
|                      v                                                      |
|   +-----------------------------------------------------------------+      |
|   |                      virtio_net Driver                           |      |
|   +-----------------------------------------------------------------+      |
|   |                                                                  |      |
|   |   ndo_start_xmit = start_xmit()                                 |      |
|   |          |                                                       |      |
|   |          +---> free_old_xmit_skbs()  [reclaim completed buffers]|      |
|   |          |            |                                          |      |
|   |          |            +---> virtqueue_get_buf(svq)              |      |
|   |          |            +---> dev_kfree_skb_any(skb)              |      |
|   |          |                                                       |      |
|   |          +---> xmit_skb()                                       |      |
|   |          |            |                                          |      |
|   |          |            +---> Fill virtio_net_hdr (csum, GSO)     |      |
|   |          |            +---> sg_set_buf(tx_sg, &hdr)             |      |
|   |          |            +---> skb_to_sgvec(skb, tx_sg+1)          |      |
|   |          |            +---> virtqueue_add_buf(svq, tx_sg, N, 0) |      |
|   |          |                        |                              |      |
|   |          |                        +---> Fill vring descriptors  |      |
|   |          |                        +---> Update avail ring       |      |
|   |          |                                                       |      |
|   |          +---> virtqueue_kick(svq)                              |      |
|   |          |            |                                          |      |
|   |          |            +---> virtio_wmb()  [memory barrier]      |      |
|   |          |            +---> Update avail->idx                   |      |
|   |          |            +---> vq->notify()  [PIO/MMIO write]      |      |
|   |          |                        |                              |      |
|   +-----------------------------------------------------------------+      |
|                                       |                                     |
|                                       v                                     |
|   +--------------------------- VM-Exit ---------------------------------+  |
|                                       |                                     |
|                                       v                                     |
|   Host/QEMU: virtio_net_handle_tx()                                        |
|          |                                                                  |
|          +---> Read descriptors from guest memory                          |
|          +---> Send packet to TAP device                                   |
|          +---> Update used ring                                             |
|          +---> Inject IRQ (optional, for TX completion)                    |
|                                                                             |
+============================================================================+
```

### 8.2 RX Complete Flow

```
+============================================================================+
|                    COMPLETE RX FLOW (Code Path)                             |
+============================================================================+
|                                                                             |
|   Network: Packet arrives at Host NIC                                       |
|                      |                                                      |
|                      v                                                      |
|   Host TAP: Read packet from TAP device                                    |
|                      |                                                      |
|                      v                                                      |
|   Host/QEMU: virtio_net_receive()                                          |
|          |                                                                  |
|          +---> Get available buffer from guest's avail ring                |
|          +---> Write packet to guest memory (DMA)                          |
|          +---> Update used ring (id, len)                                  |
|          +---> Inject IRQ to guest                                         |
|                      |                                                      |
|   +--------------------------- VM-Entry --------------------------------+  |
|                      |                                                      |
|                      v                                                      |
|   +-----------------------------------------------------------------+      |
|   |                      virtio_net Driver                           |      |
|   +-----------------------------------------------------------------+      |
|   |                                                                  |      |
|   |   IRQ Handler: vring_interrupt()                                |      |
|   |          |                                                       |      |
|   |          +---> vq->callback() = skb_recv_done()                 |      |
|   |                      |                                           |      |
|   |                      +---> napi_schedule_prep(&vi->napi)        |      |
|   |                      +---> virtqueue_disable_cb(rvq)            |      |
|   |                      +---> __napi_schedule(&vi->napi)           |      |
|   |                                  |                               |      |
|   |   [Context switch to softirq]    |                               |      |
|   |                                  v                               |      |
|   |   Softirq: net_rx_action()                                      |      |
|   |          |                                                       |      |
|   |          +---> n->poll() = virtnet_poll()                       |      |
|   |                      |                                           |      |
|   |                      +---> while (received < budget):           |      |
|   |                      |        buf = virtqueue_get_buf(rvq)      |      |
|   |                      |        receive_buf(dev, buf, len)        |      |
|   |                      |              |                            |      |
|   |                      |              +---> page_to_skb()         |      |
|   |                      |              +---> Parse virtio_net_hdr  |      |
|   |                      |              +---> Setup csum/GSO        |      |
|   |                      |              +---> eth_type_trans()      |      |
|   |                      |              +---> netif_receive_skb()   |      |
|   |                      |                          |                |      |
|   |                      +---> try_fill_recv()  [refill buffers]    |      |
|   |                      +---> napi_complete() + enable_cb()        |      |
|   |                                                                  |      |
|   +-----------------------------------------------------------------+      |
|                                       |                                     |
|                                       v                                     |
|   Network Stack: netif_receive_skb()                                       |
|          |                                                                  |
|          +---> __netif_receive_skb_core()                                  |
|          +---> deliver_skb() --> ip_rcv() --> tcp_v4_rcv()                 |
|                      |                                                      |
|                      v                                                      |
|   Socket: sk_data_ready() --> wake_up(sk->sk_wq)                           |
|                      |                                                      |
|                      v                                                      |
|   Application: recv(socket, buffer, len) returns                           |
|                                                                             |
+============================================================================+
```

### 8.3 Memory and DMA Considerations

```
+============================================================================+
|                    VIRTIO DMA / SHARED MEMORY                               |
+============================================================================+
|                                                                             |
|   Guest Physical Memory Layout:                                             |
|                                                                             |
|   +---------------------------------------------------------------+        |
|   |                     Guest Physical RAM                         |        |
|   +---------------------------------------------------------------+        |
|   |                                                                |        |
|   |   +-------------------+    +---------------------------+       |        |
|   |   | Vring (TX)        |    | Vring (RX)                |       |        |
|   |   |  - Descriptors    |    |  - Descriptors            |       |        |
|   |   |  - Avail Ring     |    |  - Avail Ring             |       |        |
|   |   |  - Used Ring      |    |  - Used Ring              |       |        |
|   |   +-------------------+    +---------------------------+       |        |
|   |                                                                |        |
|   |   +-------------------+    +---------------------------+       |        |
|   |   | TX Data Buffers   |    | RX Data Buffers           |       |        |
|   |   |  - SKB data       |    |  - Pre-allocated pages    |       |        |
|   |   |  - virtio_net_hdr |    |  - virtio_net_hdr space   |       |        |
|   |   +-------------------+    +---------------------------+       |        |
|   |                                                                |        |
|   +---------------------------------------------------------------+        |
|                                                                             |
|   Key Points:                                                               |
|                                                                             |
|   1. No actual DMA hardware - Host directly accesses guest memory           |
|   2. Guest physical addresses used (not virtual addresses)                  |
|   3. sg_phys() converts scatterlist to physical addresses                  |
|   4. Host translates GPA to HVA for memory access                          |
|   5. No IOMMU needed (trusted host)                                        |
|                                                                             |
|   Memory Barriers:                                                          |
|                                                                             |
|   Guest (Driver)                    Host (Device)                           |
|   +-----------------+               +-----------------+                     |
|   | Write desc      |               |                 |                     |
|   | virtio_wmb()    |               |                 |                     |
|   | Write avail.idx |               | Read avail.idx  |                     |
|   | virtio_mb()     |               | virtio_rmb()    |                     |
|   | Check notify    |               | Read desc       |                     |
|   | notify()        | ============> | Process         |                     |
|   |                 |               | Write used.ring |                     |
|   |                 |               | virtio_wmb()    |                     |
|   |                 |               | Write used.idx  |                     |
|   | virtio_rmb()    | <============ | Inject IRQ      |                     |
|   | Read used.idx   |               |                 |                     |
|   | Read used.ring  |               |                 |                     |
|   +-----------------+               +-----------------+                     |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **无真实 DMA**：virtio 是半虚拟化，Host 直接访问 Guest 内存
- **物理地址**：描述符中存储的是 Guest 物理地址（GPA）
- **地址转换**：Host 将 GPA 转换为 Host 虚拟地址（HVA）访问
- **内存屏障**：确保多核/虚拟化环境下的内存可见性
- **无需 IOMMU**：Host 是可信的，不需要 IOMMU 保护

---

## Appendix: Key Source Files

| File | Description |
|------|-------------|
| `drivers/net/virtio_net.c` | Main virtio_net driver |
| `drivers/virtio/virtio_ring.c` | Virtqueue/vring implementation |
| `include/linux/virtio.h` | Virtio core structures |
| `include/linux/virtio_ring.h` | Vring structures and helpers |
| `include/linux/virtio_net.h` | Virtio-net protocol definitions |
| `include/linux/netdevice.h` | Network device and NAPI definitions |

---

## References

1. [Virtio Specification](https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.html)
2. Linux Kernel Source - `drivers/net/virtio_net.c`
3. [NAPI Documentation](https://www.kernel.org/doc/Documentation/networking/NAPI_HOWTO.txt)
4. [Understanding QEMU Devices](https://www.qemu.org/docs/master/devel/virtio.html)

