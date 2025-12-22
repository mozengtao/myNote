# Linux Kernel Layered Architecture & Hard Boundaries (v3.2)

## Overview

This document explains how **Linux kernel v3.2 enforces layered architecture**, focusing on dependency direction and forbidden access rules.

---

## VFS Layering Analysis

```
+------------------------------------------------------------------+
|  VFS LAYERED ARCHITECTURE                                        |
+------------------------------------------------------------------+

    User Space
    ═══════════════════════════════════════════════════════════════
         │
         │ System calls (read, write, open, ...)
         ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                    VFS (Virtual File System)                │
    │                                                             │
    │   Responsibilities:                                         │
    │   - Unified file API                                        │
    │   - Path resolution                                         │
    │   - File descriptor management                              │
    │   - Permission checking                                     │
    │                                                             │
    │   CAN call: Filesystem operations (f_op, i_op, s_op)        │
    │   CANNOT call: Block layer directly                         │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                │ Filesystem-specific operations
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │              Filesystem Layer (ext4, xfs, btrfs)            │
    │                                                             │
    │   Responsibilities:                                         │
    │   - On-disk format handling                                 │
    │   - Block allocation                                        │
    │   - Journaling                                              │
    │                                                             │
    │   CAN call: Block layer, page cache                         │
    │   CANNOT call: VFS internals, other filesystems             │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                │ submit_bio()
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                      Block Layer                            │
    │                                                             │
    │   Responsibilities:                                         │
    │   - Request scheduling                                      │
    │   - Merging, sorting                                        │
    │   - Device abstraction                                      │
    │                                                             │
    │   CAN call: Block device drivers                            │
    │   CANNOT call: Filesystem, VFS                              │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                │ Block device operations
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                   Block Device Driver                       │
    │                                                             │
    │   CAN call: Hardware                                        │
    │   CANNOT call: Block layer internals                        │
    └─────────────────────────────────────────────────────────────┘

    DEPENDENCY DIRECTION:
    
    VFS ────▶ Filesystem ────▶ Block Layer ────▶ Driver
    
    ✓ Each layer only knows about the layer below
    ✗ No upward dependencies
```

**中文解释：**
- VFS 层次：VFS → 文件系统 → 块层 → 驱动
- 每层只知道下层，无向上依赖
- VFS 不能直接调用块层，文件系统不能调用 VFS 内部

---

## Networking Layering

```
+------------------------------------------------------------------+
|  NETWORK STACK LAYERED ARCHITECTURE                              |
+------------------------------------------------------------------+

    User Space
    ═══════════════════════════════════════════════════════════════
         │
         │ Socket system calls
         ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                    Socket Layer                             │
    │                                                             │
    │   - Protocol-agnostic socket interface                      │
    │   - File descriptor management                              │
    │   - Socket options                                          │
    │                                                             │
    │   CAN call: Protocol layer (TCP, UDP)                       │
    │   CANNOT call: Device drivers                               │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                │ Protocol operations (proto_ops)
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │               Transport Layer (TCP, UDP, SCTP)              │
    │                                                             │
    │   - Connection management                                   │
    │   - Congestion control                                      │
    │   - Reliability                                             │
    │                                                             │
    │   CAN call: IP layer                                        │
    │   CANNOT call: Socket internals, netdev directly            │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                │ ip_queue_xmit(), ip_rcv()
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                  Network Layer (IP, IPv6)                   │
    │                                                             │
    │   - Routing                                                 │
    │   - Fragmentation                                           │
    │   - ICMP                                                    │
    │                                                             │
    │   CAN call: Netdev layer                                    │
    │   CANNOT call: Transport internals                          │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                │ dev_queue_xmit(), netif_rx()
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                    Netdev Layer                             │
    │                                                             │
    │   - Device abstraction (net_device)                         │
    │   - Queuing discipline                                      │
    │   - Traffic control                                         │
    │                                                             │
    │   CAN call: NIC drivers                                     │
    │   CANNOT call: IP layer, transport                          │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                │ ndo_start_xmit()
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                     NIC Driver                              │
    │                                                             │
    │   - Hardware register access                                │
    │   - DMA management                                          │
    │   - Interrupt handling                                      │
    │                                                             │
    │   CAN call: Hardware only                                   │
    │   CANNOT call: Netdev internals                             │
    └─────────────────────────────────────────────────────────────┘

    BOUNDARY CONTRACTS:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  sk_buff ownership transfers DOWN the stack on TX           │
    │  sk_buff ownership transfers UP the stack on RX             │
    │  Each layer must not peek into adjacent layer internals     │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 网络层次：Socket → 传输层 → 网络层 → Netdev → 驱动
- sk_buff 所有权：TX 向下转移，RX 向上转移
- 每层不能窥探相邻层内部

---

## Forbidden Dependencies

```
+------------------------------------------------------------------+
|  FORBIDDEN DEPENDENCY PATTERNS                                   |
+------------------------------------------------------------------+

    1. UPWARD DEPENDENCIES (Lower calling higher)
    +----------------------------------------------------------+
    |                                                           |
    |  VFS Layer                                                |
    |       ▲                                                   |
    |       │  FORBIDDEN: Filesystem calling VFS internals      |
    |       │                                                   |
    |  Filesystem                                               |
    |                                                           |
    |  Example violation:                                       |
    |  - ext4 calling vfs_read() on another file                |
    |  - Block driver calling filesystem functions              |
    |                                                           |
    |  Why dangerous:                                           |
    |  - Circular dependencies                                  |
    |  - Deadlocks (locking order)                              |
    |  - Breaks abstraction                                     |
    +----------------------------------------------------------+

    2. CROSS-LAYER BYPASS (Skipping a layer)
    +----------------------------------------------------------+
    |                                                          |
    |  VFS Layer ─────────────────────┐                        |
    |       │                         │ FORBIDDEN              |
    |       ▼                         │                        |
    |  Filesystem                     │                        |
    |       │                         │                        |
    |       ▼                         ▼                        |
    |  Block Layer ◀──────────────────┘                       |
    |                                                          |
    |  Example violation:                                      |
    |  - VFS directly calling submit_bio()                     |
    |  - Socket layer calling NIC driver                       |
    |                                                          |
    |  Why dangerous:                                          |
    |  - Bypasses filesystem logic                             |
    |  - Breaks consistency guarantees                         |
    +----------------------------------------------------------+

    3. PEER DEPENDENCIES (Same layer calling same layer)
    +----------------------------------------------------------+
    |                                                           |
    |  ┌──────────┐        ┌──────────┐                         |
    |  │   ext4   │ ←────→ │   xfs    │  FORBIDDEN              |
    |  └──────────┘        └──────────┘                         |
    |                                                           |
    |  Example violation:                                       |
    |  - ext4 calling xfs functions                             |
    |  - TCP calling UDP internals                              |
    |                                                           |
    |  Why dangerous:                                           |
    |  - Creates coupling between implementations               |
    |  - Prevents independent evolution                         |
    +----------------------------------------------------------+

    ALLOWED PATTERN: Callbacks (Inversion of Control)
    +----------------------------------------------------------+
    |                                                           |
    |  Higher Layer                                             |
    |       │                                                   |
    |       │ Registers callback                                |
    |       ▼                                                   |
    |  Lower Layer                                              |
    |       │                                                   |
    |       │ Calls callback (controlled by lower layer)        |
    |       ▼                                                   |
    |  Callback executes in higher layer context                |
    |                                                           |
    |  Example: File operations (f_op->read)                    |
    |  - VFS calls f_op->read()                                 |
    |  - Filesystem provides implementation                     |
    |  - Control flow: VFS → Filesystem (correct direction)     |
    +----------------------------------------------------------+
```

**中文解释：**
- 禁止向上依赖：低层调用高层
- 禁止跨层绕过：跳过中间层
- 禁止同级依赖：同层相互调用
- 允许回调：高层注册，低层调用（控制反转）

---

## Why Violations are Dangerous

```
+------------------------------------------------------------------+
|  VIOLATION CONSEQUENCES                                          |
+------------------------------------------------------------------+

    CONSEQUENCE 1: Deadlocks
    +----------------------------------------------------------+
    |  VFS holds inode lock                                     |
    |       │                                                   |
    |       ▼                                                   |
    |  Calls filesystem                                         |
    |       │                                                   |
    |       ▼ (VIOLATION: upward call)                          |
    |  Filesystem calls VFS function                            |
    |       │                                                   |
    |       ▼                                                   |
    |  VFS tries to acquire same lock → DEADLOCK                |
    +----------------------------------------------------------+

    CONSEQUENCE 2: Infinite Recursion
    +----------------------------------------------------------+
    |  A calls B                                                |
    |       │                                                   |
    |       ▼                                                   |
    |  B calls C                                                |
    |       │                                                   |
    |       ▼ (VIOLATION: upward call)                          |
    |  C calls A → Stack overflow                               |
    +----------------------------------------------------------+

    CONSEQUENCE 3: Broken Invariants
    +----------------------------------------------------------+
    |  Block layer maintains request ordering                   |
    |       │                                                   |
    |       ▼ (VIOLATION: filesystem bypasses)                  |
    |  Filesystem directly accesses device                      |
    |       │                                                   |
    |       ▼                                                   |
    |  Ordering guarantees violated → Data corruption           |
    +----------------------------------------------------------+

    CONSEQUENCE 4: Untestable Code
    +----------------------------------------------------------+
    |  With clean layers:                                       |
    |  - Test each layer independently                          |
    |  - Mock interfaces between layers                         |
    |                                                           |
    |  With violations:                                         |
    |  - Must test entire stack together                        |
    |  - Bugs hide in layer interactions                        |
    +----------------------------------------------------------+
```

**中文解释：**
- 死锁：向上调用可能再获取已持有的锁
- 无限递归：循环调用导致栈溢出
- 不变量破坏：绕过层导致保证失效
- 不可测试：层间耦合导致必须整体测试

---

## User-Space Layered Design

```c
/* User-space layered architecture pattern */

/*=================================================================
 * LAYER DEFINITION: Each layer has clear interface
 *================================================================*/

/* --- Application Layer (highest) --- */
struct app_request {
    int type;
    void *data;
};

int app_process(struct app_request *req);

/* --- Service Layer --- */
struct service_ops {
    int (*handle)(void *ctx, void *data);
    void (*cleanup)(void *ctx);
};

struct service {
    const struct service_ops *ops;
    void *private;
};

/* --- Transport Layer --- */
struct transport_ops {
    int (*send)(void *ctx, const void *buf, size_t len);
    int (*recv)(void *ctx, void *buf, size_t len);
    void (*close)(void *ctx);
};

struct transport {
    const struct transport_ops *ops;
    void *private;
};

/* --- Driver Layer (lowest) --- */
struct driver_ops {
    int (*write)(void *hw, const void *buf, size_t len);
    int (*read)(void *hw, void *buf, size_t len);
};

/*=================================================================
 * LAYER RULES ENFORCEMENT
 *================================================================*/

/* Each layer only has pointer to layer below */
struct service_context {
    struct transport *transport;  /* Can use transport */
    /* NO pointer to app layer */
};

struct transport_context {
    struct driver_ops *driver;    /* Can use driver */
    /* NO pointer to service layer */
};

/*=================================================================
 * CORRECT DOWNWARD CALL
 *================================================================*/
int service_send_message(struct service_context *ctx, 
                         const void *msg, size_t len)
{
    /* Service calls transport (allowed) */
    return ctx->transport->ops->send(ctx->transport->private, 
                                     msg, len);
}

/*=================================================================
 * CALLBACK PATTERN (Controlled upward flow)
 *================================================================*/
typedef void (*transport_callback_t)(void *ctx, void *data);

struct transport_with_callback {
    struct transport_ops *ops;
    void *private;
    
    /* Callback registered by higher layer */
    transport_callback_t on_data;
    void *callback_ctx;
};

/* Transport layer calls this when data arrives */
void transport_handle_data(struct transport_with_callback *t,
                           void *data)
{
    /* Call registered callback (from higher layer) */
    if (t->on_data) {
        t->on_data(t->callback_ctx, data);
    }
}

/*=================================================================
 * FORBIDDEN PATTERN DETECTION
 *================================================================*/

#define LAYER_APP       1
#define LAYER_SERVICE   2
#define LAYER_TRANSPORT 3
#define LAYER_DRIVER    4

static __thread int current_layer = 0;

#define ENTER_LAYER(layer) do { \
    if (current_layer != 0 && (layer) > current_layer) { \
        fprintf(stderr, "VIOLATION: Layer %d calling layer %d\n", \
                current_layer, (layer)); \
        abort(); \
    } \
    current_layer = (layer); \
} while (0)

#define EXIT_LAYER() do { \
    current_layer = 0; \
} while (0)

int checked_service_call(struct service *svc, void *data)
{
    ENTER_LAYER(LAYER_SERVICE);
    int ret = svc->ops->handle(svc->private, data);
    EXIT_LAYER();
    return ret;
}

/*=================================================================
 * BOUNDARY DIAGRAM
 *================================================================*/
/*
    ┌────────────────────────────────────────────────────────────┐
    │  Application Layer                                          │
    │                                                              │
    │  - User-facing API                                          │
    │  - Business logic                                           │
    │  - CAN call: Service layer                                  │
    │  - CANNOT call: Transport, Driver                           │
    └───────────────────────────────┬────────────────────────────┘
                                    │
                    ════════════════╪════════════════ API Boundary
                                    │
    ┌───────────────────────────────┴────────────────────────────┐
    │  Service Layer                                              │
    │                                                              │
    │  - Protocol handling                                        │
    │  - Session management                                       │
    │  - CAN call: Transport layer                                │
    │  - CANNOT call: Application, Driver                         │
    └───────────────────────────────┬────────────────────────────┘
                                    │
                    ════════════════╪════════════════ Interface Boundary
                                    │
    ┌───────────────────────────────┴────────────────────────────┐
    │  Transport Layer                                            │
    │                                                              │
    │  - Connection management                                    │
    │  - Buffering                                                │
    │  - CAN call: Driver layer                                   │
    │  - CANNOT call: Application, Service                        │
    └───────────────────────────────┬────────────────────────────┘
                                    │
                    ════════════════╪════════════════ Hardware Boundary
                                    │
    ┌───────────────────────────────┴────────────────────────────┐
    │  Driver Layer                                               │
    │                                                              │
    │  - Hardware abstraction                                     │
    │  - CAN call: Hardware only                                  │
    │  - CANNOT call: Any higher layer                            │
    └────────────────────────────────────────────────────────────┘
*/
```

**中文解释：**
- 每层有清晰接口：ops 结构体
- 每层只有下层指针，无上层指针
- 正确向下调用：service 调用 transport
- 回调模式：高层注册回调，低层调用
- 违规检测：运行时检查层次调用

