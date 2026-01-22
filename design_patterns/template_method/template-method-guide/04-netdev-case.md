# Case 2: Network Device Transmit Path

## Subsystem Context

```
+=============================================================================+
|                    NETWORK TX ARCHITECTURE                                   |
+=============================================================================+

                           USER SPACE
    +----------------------------------------------------------+
    |   Application:  send(fd, buf, len, flags)                |
    +----------------------------------------------------------+
                              |
                              | System Call
                              v
    +----------------------------------------------------------+
    |                    SOCKET LAYER                           |
    |                    sock_sendmsg()                         |
    +----------------------------------------------------------+
                              |
                              v
    +----------------------------------------------------------+
    |                   PROTOCOL LAYER                          |
    |              TCP: tcp_sendmsg() / tcp_transmit_skb()      |
    |              UDP: udp_sendmsg()                           |
    +----------------------------------------------------------+
                              |
                              v
    +----------------------------------------------------------+
    |                    IP LAYER                               |
    |                    ip_queue_xmit()                        |
    +----------------------------------------------------------+
                              |
                              v
    +----------------------------------------------------------+
    |                   NETWORK CORE                            |
    |  +--------------------------------------------------+    |
    |  |  dev_queue_xmit()  <-- TEMPLATE METHOD           |    |
    |  |                                                  |    |
    |  |  1. Check device state                           |    |
    |  |  2. Acquire queue lock                           |    |
    |  |  3. Traffic shaping (qdisc)                      |    |
    |  |  4. CALL ndo_start_xmit() ----------------+      |    |
    |  |  5. Handle completion                     |      |    |
    |  |  6. Update statistics                     |      |    |
    |  +-------------------------------------------|------+    |
    +------------------------------------------------|----------+
                                                     |
                                                     v
    +----------------------------------------------------------+
    |                    DRIVER LAYER                           |
    |  +----------------+  +----------------+  +---------------+|
    |  |    e1000       |  |    rtl8139     |  |   virtio-net  ||
    |  | e1000_xmit_    |  | rtl8139_start_ |  | start_xmit    ||
    |  | frame()        |  | xmit()         |  |               ||
    |  +----------------+  +----------------+  +---------------+|
    +----------------------------------------------------------+
                              |
                              v
                      [ Physical Network ]
```

**中文说明：**

网络发送路径从用户空间的`send()`开始，经过socket层、协议层（TCP/UDP）、IP层，最终到达网络核心层的`dev_queue_xmit()`。`dev_queue_xmit()`是模板方法：它检查设备状态、获取队列锁、执行流量整形（qdisc），然后调用驱动的`ndo_start_xmit()`实际发送数据包，最后处理完成状态和更新统计。不同网络驱动只需实现发送逻辑。

---

## The Template Method: dev_queue_xmit()

### Components

| Component | Role |
|-----------|------|
| **Template Method** | `dev_queue_xmit()` / `dev_hard_start_xmit()` |
| **Fixed Steps** | Queue lock, device state check, qdisc, completion handling |
| **Variation Point** | `ops->ndo_start_xmit()` |
| **Ops Table** | `struct net_device_ops` |

### Control Flow Diagram

```
    dev_queue_xmit(skb)
    ===================

    +----------------------------------+
    |  1. VALIDATE SKB AND DEVICE      |
    |     - Check skb is valid         |
    |     - Get net_device from skb    |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  2. TRAFFIC CONTROL (QDISC)      |
    |     - __dev_xmit_skb()           |
    |     - Queue to qdisc if needed   |
    |     - Traffic shaping            |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  3. ACQUIRE TX QUEUE LOCK        |
    |     - __netif_tx_lock()          |
    |     - Serialize TX on queue      |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  4. CHECK DEVICE STATE           |
    |     - netif_tx_queue_stopped()?  |
    |     - Device carrier OK?         |
    +----------------------------------+
                   |
                   v
    +==========================================+
    ||  5. VARIATION POINT                    ||
    ||     ops->ndo_start_xmit(skb, dev)      ||
    ||     Driver puts packet on wire         ||
    +==========================================+
                   |
                   v
    +----------------------------------+
    |  6. HANDLE RETURN STATUS         |
    |     - NETDEV_TX_OK: done         |
    |     - NETDEV_TX_BUSY: requeue    |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  7. RELEASE TX QUEUE LOCK        |
    |     - __netif_tx_unlock()        |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  8. UPDATE STATISTICS            |
    |     - Byte/packet counters       |
    +----------------------------------+
```

**中文说明：**

`dev_queue_xmit()`的控制流：(1) 验证skb和设备；(2) 流量控制（qdisc排队、流量整形）；(3) 获取发送队列锁；(4) 检查设备状态（队列是否停止、载波是否正常）；(5) **变化点**——调用驱动的`ndo_start_xmit()`将数据包发送到物理网络；(6) 处理返回状态（成功或忙需重新排队）；(7) 释放队列锁；(8) 更新统计信息。驱动只实现第5步。

---

## Why Template Method is Required Here

### 1. Transmit Timing Must Be Controlled

```
    WITHOUT TEMPLATE METHOD (DANGEROUS):

    /* Driver controls when to transmit */
    void driver_send(skb) {
        // No coordination with other packets
        // No qdisc interaction
        hardware_send(skb);
    }

    PROBLEMS:
    - No traffic shaping
    - No fair queuing between flows
    - Race conditions on TX queue
    - Cannot implement QoS
```

### 2. Queue Locking Must Be Consistent

```
    TX QUEUE SERIALIZATION:

    +-------------+     +-------------+     +-------------+
    |   CPU 0     |     |   CPU 1     |     |   CPU 2     |
    +-------------+     +-------------+     +-------------+
          |                   |                   |
          v                   v                   v
    +----------------------------------------------------------+
    |                    TX QUEUE LOCK                          |
    |  Only ONE CPU can call ndo_start_xmit() at a time        |
    +----------------------------------------------------------+
                              |
                              v
                      +----------------+
                      |  HARDWARE TX   |
                      +----------------+

    WITHOUT THIS:
    - Multiple CPUs could call driver simultaneously
    - Hardware registers could be corrupted
    - Packets could be interleaved incorrectly
```

**中文说明：**

发送队列锁确保同一时间只有一个CPU调用`ndo_start_xmit()`。没有这个锁：多个CPU可能同时调用驱动、硬件寄存器可能被破坏、数据包可能错误交错。框架控制这个锁，驱动不需要（也不应该）管理它。

### 3. State Checks Before Transmission

```
    DEVICE STATE MACHINE:

    +----------+       +----------+       +----------+
    |   DOWN   | ----> |    UP    | ----> | RUNNING  |
    +----------+       +----------+       +----------+
                                               |
                                               v
                       +----------+       +----------+
                       | CARRIER  | <---- | NO QUEUE |
                       |   OK     |       |  STOP    |
                       +----------+       +----------+

    FRAMEWORK CHECKS:
    - Is device UP and RUNNING?
    - Is carrier present (cable connected)?
    - Is TX queue not stopped?

    DRIVER CANNOT BYPASS THESE CHECKS
```

---

## Minimal C Code Simulation

```c
/*
 * MINIMAL NETWORK TX TEMPLATE METHOD SIMULATION
 * 
 * Demonstrates the Template Method pattern in network transmit path.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
struct sk_buff;
struct net_device;

/* ==========================================================
 * SKB: Socket Buffer (packet container)
 * ========================================================== */
struct sk_buff {
    void *data;
    size_t len;
    struct net_device *dev;
};

/* ==========================================================
 * NET DEVICE OPS: Driver operations table
 * ========================================================== */
typedef int netdev_tx_t;
#define NETDEV_TX_OK     0
#define NETDEV_TX_BUSY   1

struct net_device_ops {
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb, 
                                   struct net_device *dev);
    int (*ndo_open)(struct net_device *dev);
    int (*ndo_stop)(struct net_device *dev);
};

/* ==========================================================
 * NET DEVICE: Network interface
 * ========================================================== */
#define IFF_UP          0x01
#define IFF_RUNNING     0x02

struct net_device {
    const char *name;
    unsigned int flags;
    int tx_queue_stopped;
    int carrier_ok;
    
    /* Statistics */
    unsigned long tx_packets;
    unsigned long tx_bytes;
    unsigned long tx_dropped;
    
    /* TX queue lock (simulated) */
    int tx_lock_held;
    
    const struct net_device_ops *netdev_ops;
};

/* ==========================================================
 * FRAMEWORK FIXED STEPS (Network Core)
 * ========================================================== */

static int __netif_tx_trylock(struct net_device *dev)
{
    if (dev->tx_lock_held) {
        printf("  [NET] TX lock already held, busy\n");
        return 0;
    }
    dev->tx_lock_held = 1;
    printf("  [NET] TX lock acquired\n");
    return 1;
}

static void __netif_tx_unlock(struct net_device *dev)
{
    dev->tx_lock_held = 0;
    printf("  [NET] TX lock released\n");
}

static int netif_tx_queue_stopped(struct net_device *dev)
{
    return dev->tx_queue_stopped;
}

static int netif_carrier_ok(struct net_device *dev)
{
    return dev->carrier_ok;
}

static int netif_running(struct net_device *dev)
{
    return (dev->flags & IFF_RUNNING) != 0;
}

static void kfree_skb(struct sk_buff *skb)
{
    printf("  [NET] SKB freed (packet dropped)\n");
    free(skb->data);
    free(skb);
}

/* ==========================================================
 * TEMPLATE METHOD: dev_hard_start_xmit()
 * 
 * Simplified version of the actual network transmit path.
 * ========================================================== */
static netdev_tx_t dev_hard_start_xmit(struct sk_buff *skb,
                                        struct net_device *dev)
{
    const struct net_device_ops *ops = dev->netdev_ops;
    netdev_tx_t rc;

    printf("  [NET] dev_hard_start_xmit: calling driver\n");
    
    /* ========== VARIATION POINT: Driver transmit ========== */
    if (ops->ndo_start_xmit) {
        rc = ops->ndo_start_xmit(skb, dev);
    } else {
        printf("  [NET] ERROR: no xmit function\n");
        rc = NETDEV_TX_BUSY;
    }

    /* ========== FIXED: Handle return status ========== */
    if (rc == NETDEV_TX_OK) {
        dev->tx_packets++;
        dev->tx_bytes += skb->len;
    }
    
    return rc;
}

/* ==========================================================
 * TEMPLATE METHOD: dev_queue_xmit()
 * 
 * This is the main entry point for packet transmission.
 * Framework controls all aspects except actual hardware TX.
 * ========================================================== */
int dev_queue_xmit(struct sk_buff *skb)
{
    struct net_device *dev = skb->dev;
    netdev_tx_t rc;

    printf("[dev_queue_xmit] TEMPLATE METHOD START\n");
    printf("  [NET] Device: %s, Packet size: %zu bytes\n", 
           dev->name, skb->len);

    /* ========== FIXED STEP 1: Validate device state ========== */
    if (!netif_running(dev)) {
        printf("  [NET] ERROR: device not running\n");
        goto drop;
    }
    printf("  [NET] Device state: running\n");

    /* ========== FIXED STEP 2: Check carrier ========== */
    if (!netif_carrier_ok(dev)) {
        printf("  [NET] ERROR: no carrier (cable unplugged?)\n");
        goto drop;
    }
    printf("  [NET] Carrier: OK\n");

    /* ========== FIXED STEP 3: Check queue state ========== */
    if (netif_tx_queue_stopped(dev)) {
        printf("  [NET] ERROR: TX queue stopped\n");
        goto drop;
    }
    printf("  [NET] TX queue: not stopped\n");

    /* ========== FIXED STEP 4: Acquire TX lock ========== */
    /* In real kernel: this is per-queue lock */
    if (!__netif_tx_trylock(dev)) {
        printf("  [NET] Cannot acquire TX lock, busy\n");
        goto drop;
    }

    /* ========== FIXED STEP 5: Call driver (variation point inside) ========== */
    rc = dev_hard_start_xmit(skb, dev);

    /* ========== FIXED STEP 6: Release TX lock ========== */
    __netif_tx_unlock(dev);

    /* ========== FIXED STEP 7: Handle result ========== */
    if (rc == NETDEV_TX_BUSY) {
        printf("  [NET] TX busy, would requeue in real kernel\n");
        goto drop;
    }

    printf("[dev_queue_xmit] TEMPLATE METHOD END, success\n\n");
    return 0;

drop:
    dev->tx_dropped++;
    kfree_skb(skb);
    printf("[dev_queue_xmit] TEMPLATE METHOD END, dropped\n\n");
    return -1;
}

/* ==========================================================
 * DRIVER IMPLEMENTATIONS (Variation Points)
 * ========================================================== */

/* --- e1000-like driver implementation --- */
static netdev_tx_t e1000_xmit_frame(struct sk_buff *skb,
                                     struct net_device *dev)
{
    printf("    [e1000] Writing to TX descriptor ring\n");
    printf("    [e1000] Setting up DMA address: %p\n", skb->data);
    printf("    [e1000] Triggering hardware TX\n");
    printf("    [e1000] TX complete\n");
    
    /* In real driver: skb ownership transfers to hardware */
    /* For simulation, we don't free it here */
    return NETDEV_TX_OK;
}

static const struct net_device_ops e1000_netdev_ops = {
    .ndo_start_xmit = e1000_xmit_frame,
    .ndo_open = NULL,
    .ndo_stop = NULL,
};

/* --- virtio-net-like driver implementation --- */
static netdev_tx_t virtio_net_xmit(struct sk_buff *skb,
                                    struct net_device *dev)
{
    printf("    [virtio] Adding buffer to TX virtqueue\n");
    printf("    [virtio] Kicking hypervisor\n");
    printf("    [virtio] TX submitted to virtqueue\n");
    
    return NETDEV_TX_OK;
}

static const struct net_device_ops virtio_net_ops = {
    .ndo_start_xmit = virtio_net_xmit,
    .ndo_open = NULL,
    .ndo_stop = NULL,
};

/* --- Loopback-like driver implementation --- */
static netdev_tx_t loopback_xmit(struct sk_buff *skb,
                                  struct net_device *dev)
{
    printf("    [loopback] Looping packet back to RX path\n");
    printf("    [loopback] No hardware involved\n");
    
    return NETDEV_TX_OK;
}

static const struct net_device_ops loopback_ops = {
    .ndo_start_xmit = loopback_xmit,
    .ndo_open = NULL,
    .ndo_stop = NULL,
};

/* ==========================================================
 * HELPER: Create a test packet
 * ========================================================== */
static struct sk_buff *create_test_skb(struct net_device *dev, 
                                        const char *payload)
{
    struct sk_buff *skb = malloc(sizeof(*skb));
    skb->len = strlen(payload);
    skb->data = malloc(skb->len);
    memcpy(skb->data, payload, skb->len);
    skb->dev = dev;
    return skb;
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */
int main(void)
{
    printf("==============================================\n");
    printf("NETWORK TX TEMPLATE METHOD DEMONSTRATION\n");
    printf("==============================================\n\n");

    /* Create network devices with different drivers */
    struct net_device e1000_dev = {
        .name = "eth0",
        .flags = IFF_UP | IFF_RUNNING,
        .carrier_ok = 1,
        .tx_queue_stopped = 0,
        .netdev_ops = &e1000_netdev_ops,
    };

    struct net_device virtio_dev = {
        .name = "eth1",
        .flags = IFF_UP | IFF_RUNNING,
        .carrier_ok = 1,
        .tx_queue_stopped = 0,
        .netdev_ops = &virtio_net_ops,
    };

    struct net_device lo_dev = {
        .name = "lo",
        .flags = IFF_UP | IFF_RUNNING,
        .carrier_ok = 1,
        .tx_queue_stopped = 0,
        .netdev_ops = &loopback_ops,
    };

    /* 
     * All use the SAME dev_queue_xmit() template method.
     * Only the driver-specific xmit differs.
     */
    printf("--- Transmit via e1000 (Intel NIC) ---\n");
    struct sk_buff *skb1 = create_test_skb(&e1000_dev, "Hello e1000!");
    dev_queue_xmit(skb1);

    printf("--- Transmit via virtio-net (VM) ---\n");
    struct sk_buff *skb2 = create_test_skb(&virtio_dev, "Hello virtio!");
    dev_queue_xmit(skb2);

    printf("--- Transmit via loopback ---\n");
    struct sk_buff *skb3 = create_test_skb(&lo_dev, "Hello loopback!");
    dev_queue_xmit(skb3);

    /* Demonstrate state checks */
    printf("--- Transmit with no carrier (cable unplugged) ---\n");
    e1000_dev.carrier_ok = 0;
    struct sk_buff *skb4 = create_test_skb(&e1000_dev, "Will be dropped");
    dev_queue_xmit(skb4);
    e1000_dev.carrier_ok = 1;

    printf("--- Transmit with stopped queue ---\n");
    e1000_dev.tx_queue_stopped = 1;
    struct sk_buff *skb5 = create_test_skb(&e1000_dev, "Will be dropped");
    dev_queue_xmit(skb5);

    /* Print statistics */
    printf("\n=== STATISTICS ===\n");
    printf("eth0: TX packets=%lu, bytes=%lu, dropped=%lu\n",
           e1000_dev.tx_packets, e1000_dev.tx_bytes, e1000_dev.tx_dropped);
    printf("eth1: TX packets=%lu, bytes=%lu, dropped=%lu\n",
           virtio_dev.tx_packets, virtio_dev.tx_bytes, virtio_dev.tx_dropped);
    printf("lo:   TX packets=%lu, bytes=%lu, dropped=%lu\n",
           lo_dev.tx_packets, lo_dev.tx_bytes, lo_dev.tx_dropped);

    return 0;
}
```

---

## What the Implementation is NOT Allowed to Do

```
+=============================================================================+
|              NETWORK DRIVER IMPLEMENTATION RESTRICTIONS                      |
+=============================================================================+

    DRIVER CANNOT:

    1. BYPASS QUEUE DISCIPLINE (QDISC)
       Driver receives packets after qdisc processing
       Cannot jump ahead of other queued packets

    2. IGNORE TX QUEUE LOCK
       Driver runs with lock held
       Cannot release lock and continue later

    3. CONTROL TRANSMISSION TIMING
       Framework decides when to call ndo_start_xmit
       Driver cannot "hold" a packet for later

    4. MODIFY OTHER QUEUES
       Driver only sees its own TX queue
       Cannot interfere with other interfaces

    5. CHANGE PRIORITY ORDERING
       Packet priority determined by qdisc before driver
       Driver transmits in order given

    6. SKIP STATISTICS
       Framework updates counters after driver returns
       Statistics are always accurate

    7. CALL BACK INTO FRAMEWORK TX PATH
       Cannot call dev_queue_xmit() from ndo_start_xmit
       Would cause recursion/deadlock

    +-----------------------------------------------------------------+
    |  THE DRIVER IS A LEAF NODE IN TX PATH                           |
    |  IT RECEIVES PACKETS AND SENDS THEM - NOTHING MORE              |
    +-----------------------------------------------------------------+
```

**中文说明：**

网络驱动实现的限制：(1) 不能绕过队列规则（qdisc）；(2) 不能忽略TX队列锁；(3) 不能控制发送时机——框架决定何时调用；(4) 不能修改其他队列；(5) 不能改变优先级顺序——已由qdisc确定；(6) 不能跳过统计；(7) 不能在`ndo_start_xmit`中回调`dev_queue_xmit`——会导致递归/死锁。驱动是发送路径的叶子节点，只负责接收数据包并发送，仅此而已。

---

## Real Kernel Code Reference (v3.2)

### dev_queue_xmit() in net/core/dev.c

```c
/* Simplified from actual kernel code */
int dev_queue_xmit(struct sk_buff *skb)
{
    struct net_device *dev = skb->dev;
    struct netdev_queue *txq;
    struct Qdisc *q;
    int rc = -ENOMEM;

    /* Select TX queue */
    txq = netdev_pick_tx(dev, skb);
    q = rcu_dereference_bh(txq->qdisc);

    if (q->enqueue) {
        rc = __dev_xmit_skb(skb, q, dev, txq);
        goto out;
    }

    /* No qdisc, direct transmit */
    if (dev->flags & IFF_UP) {
        HARD_TX_LOCK(dev, txq, cpu);
        if (!netif_tx_queue_stopped(txq))
            rc = dev_hard_start_xmit(skb, dev, txq);
        HARD_TX_UNLOCK(dev, txq);
    }

out:
    return rc;
}
```

### struct net_device_ops in include/linux/netdevice.h

```c
struct net_device_ops {
    int  (*ndo_init)(struct net_device *dev);
    void (*ndo_uninit)(struct net_device *dev);
    int  (*ndo_open)(struct net_device *dev);
    int  (*ndo_stop)(struct net_device *dev);
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
                                   struct net_device *dev);
    /* ... many more operations ... */
};
```

---

## Key Takeaways

1. **Network core owns transmit path**: All TX goes through `dev_queue_xmit()`
2. **Qdisc enforces policy**: Traffic shaping happens before driver sees packet
3. **Locking is automatic**: Framework handles TX queue serialization
4. **State checks are mandatory**: Driver only runs when device is ready
5. **Drivers are simple**: Just push packet to hardware and return
