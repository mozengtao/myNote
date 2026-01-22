# Case 3: NAPI Polling Framework

## Subsystem Context

```
+=============================================================================+
|                         NAPI ARCHITECTURE                                    |
+=============================================================================+

    TRADITIONAL INTERRUPT-DRIVEN RX           NAPI POLLING RX
    =============================           ================

    +-------------+                         +-------------+
    | Packet      |                         | Packet      |
    | Arrives     |                         | Arrives     |
    +-------------+                         +-------------+
          |                                       |
          v                                       v
    +-------------+                         +-------------+
    | INTERRUPT   |  (for each packet)      | INTERRUPT   |  (first packet)
    | handler()   |                         | handler()   |
    +-------------+                         +-------------+
          |                                       |
          v                                       v
    +-------------+                         +------------------+
    | process     |                         | Disable IRQs     |
    | ONE packet  |                         | Schedule NAPI    |
    +-------------+                         +------------------+
          |                                       |
          v                                       v
    +-------------+                         +------------------+
    | Re-enable   |                         | Softirq context  |
    | interrupt   |                         | net_rx_action()  |
    +-------------+                         +------------------+
                                                  |
    PROBLEM:                                      v
    - Interrupt per packet              +------------------+
    - High CPU overhead                 | NAPI poll loop   |  <-- TEMPLATE
    - Interrupt livelock                | (budget limited) |      METHOD
                                        +------------------+
                                                  |
                                                  v
                                        +------------------+
                                        | driver->poll()   |  <-- HOOK
                                        | Process MULTIPLE |
                                        | packets          |
                                        +------------------+
                                                  |
                                                  v
                                        +------------------+
                                        | Budget exhausted?|
                                        | Re-enable IRQ    |
                                        +------------------+
```

**中文说明：**

NAPI（New API）是Linux网络子系统的轮询框架。传统中断驱动模式下，每个数据包触发一次中断，导致高CPU开销和中断活锁。NAPI解决这个问题：第一个数据包触发中断后禁用中断并调度NAPI，然后在软中断上下文中轮询处理多个数据包。`net_rx_action()`是模板方法，它控制轮询循环和预算管理，驱动的`poll()`钩子只负责从硬件获取数据包。

---

## The Template Method: napi_poll() / net_rx_action()

### Components

| Component | Role |
|-----------|------|
| **Template Method** | `net_rx_action()` (softirq handler) |
| **Fixed Steps** | Budget allocation, time limits, completion handling |
| **Variation Point** | `napi->poll()` |
| **Structure** | `struct napi_struct` |

### Control Flow Diagram

```
    net_rx_action() -- Softirq Handler
    ===================================

    +----------------------------------+
    |  1. GET NAPI POLL LIST           |
    |     - Local CPU's poll_list      |
    |     - NAPI instances to poll     |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  2. SET TIME BUDGET              |
    |     - 2 jiffies max              |
    |     - Prevent starvation         |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  3. SET PACKET BUDGET            |
    |     - netdev_budget (300 default)|
    |     - Shared across all NAPI     |
    +----------------------------------+
                   |
                   v
    +=====================================+
    | LOOP: While budget > 0 && time OK  |
    |=====================================|
    |                                     |
    |    +-----------------------------+  |
    |    |  4. GET NEXT NAPI           |  |
    |    |     - From poll_list        |  |
    |    +-----------------------------+  |
    |                  |                  |
    |                  v                  |
    |    +=============================+  |
    |    ||  5. VARIATION POINT       ||  |
    |    ||     work = napi->poll()   ||  |
    |    ||     (driver fills work)   ||  |
    |    +=============================+  |
    |                  |                  |
    |                  v                  |
    |    +-----------------------------+  |
    |    |  6. SUBTRACT FROM BUDGET   |  |
    |    |     budget -= work         |  |
    |    +-----------------------------+  |
    |                  |                  |
    |                  v                  |
    |    +-----------------------------+  |
    |    |  7. CHECK COMPLETION       |  |
    |    |     work < weight?         |  |
    |    |     -> napi_complete()     |  |
    |    |     -> re-enable IRQ       |  |
    |    +-----------------------------+  |
    |                                     |
    +=====================================+
                   |
                   v
    +----------------------------------+
    |  8. RESCHEDULE IF NEEDED         |
    |     - More work pending?         |
    |     - Raise softirq again        |
    +----------------------------------+
```

**中文说明：**

`net_rx_action()`的控制流：(1) 获取本CPU的NAPI轮询列表；(2) 设置时间预算（最多2个jiffies）；(3) 设置数据包预算（默认300个）；(4-7) 循环处理：获取下一个NAPI实例，调用驱动的`poll()`钩子，从预算中减去处理的工作量，检查是否完成（工作量<权重则调用`napi_complete()`并重新启用中断）；(8) 如果还有待处理工作，重新调度软中断。

---

## Why Template Method is Required Here

### 1. Budget Must Be Framework-Controlled

```
    WHY BUDGET MATTERS:

    WITHOUT BUDGET CONTROL:
    +-----------------------+
    | Network driver poll   |
    | while (has_packets)   |  <-- Could run forever!
    |     process_packet(); |
    +-----------------------+

    PROBLEMS:
    - Driver could monopolize CPU
    - Other devices starved
    - System unresponsive
    - No fairness


    WITH FRAMEWORK BUDGET:
    +------------------------------------+
    | net_rx_action() {                  |
    |     budget = 300;                  |  Framework sets limit
    |     while (budget > 0) {           |
    |         work = napi->poll(budget); |  Driver given budget
    |         budget -= work;            |  Framework tracks
    |     }                              |
    | }                                  |
    +------------------------------------+

    GUARANTEES:
    - No driver can hog CPU
    - Fair time sharing
    - System stays responsive
```

**中文说明：**

预算必须由框架控制。没有预算控制，驱动可能永远运行、独占CPU、饿死其他设备、系统无响应。框架设置300个数据包的预算，在循环中跟踪消耗，确保没有驱动能独占CPU，实现公平时间分配，系统保持响应。

### 2. Completion Detection Must Be Consistent

```
    NAPI COMPLETION PROTOCOL:

    +------------------+     +------------------+
    | Driver poll()    |     | Framework        |
    +------------------+     +------------------+
           |                        |
           | return work_done       |
           |----------------------->|
           |                        |
           |                        | if (work_done < weight)
           |                        |     napi_complete_done()
           |                        |     re-enable_irq()
           |                        |
           |                        | else
           |                        |     keep polling
           |                        |

    DRIVER CANNOT:
    - Decide when to re-enable IRQ
    - Call napi_complete() with wrong state
    - Override the completion decision
```

### 3. Interrupt Re-enabling Must Be Safe

```
    RACE CONDITION PREVENTION:

    +--------------------------------------------------+
    | net_rx_action() handles the IRQ re-enable        |
    | because:                                         |
    |                                                  |
    | 1. IRQ must be disabled during poll              |
    | 2. napi_complete() has atomic state transition   |
    | 3. New IRQ can only occur after complete         |
    |                                                  |
    | If driver controlled this:                       |
    | - Could re-enable too early (race)               |
    | - Could forget to re-enable (hang)               |
    | - Could re-enable while still in poll (crash)    |
    +--------------------------------------------------+
```

---

## Minimal C Code Simulation

```c
/*
 * MINIMAL NAPI TEMPLATE METHOD SIMULATION
 * 
 * Demonstrates the Template Method pattern in NAPI polling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
struct napi_struct;
struct net_device;

/* ==========================================================
 * NAPI STATE FLAGS
 * ========================================================== */
#define NAPI_STATE_SCHED     0x01   /* Scheduled for polling */
#define NAPI_STATE_DISABLE   0x02   /* Disabled */

/* ==========================================================
 * NAPI STRUCTURE
 * ========================================================== */
struct napi_struct {
    int (*poll)(struct napi_struct *napi, int budget);
    int weight;                /* Max work per poll */
    unsigned int state;        /* NAPI state flags */
    struct net_device *dev;    /* Associated device */
    
    /* For simulation: packets waiting */
    int pending_packets;
    const char *driver_name;
};

/* ==========================================================
 * NET DEVICE (simplified)
 * ========================================================== */
struct net_device {
    const char *name;
    int irq_enabled;
    unsigned long rx_packets;
};

/* ==========================================================
 * FRAMEWORK: NAPI Core Functions
 * ========================================================== */

/* Check if NAPI is scheduled */
static int napi_is_scheduled(struct napi_struct *napi)
{
    return (napi->state & NAPI_STATE_SCHED) != 0;
}

/* Schedule NAPI for polling (called from IRQ handler) */
static void napi_schedule(struct napi_struct *napi)
{
    if (!(napi->state & NAPI_STATE_SCHED)) {
        napi->state |= NAPI_STATE_SCHED;
        printf("  [NAPI] %s scheduled for polling\n", napi->driver_name);
    }
}

/* Complete NAPI polling (re-enables IRQ) */
static void napi_complete(struct napi_struct *napi)
{
    napi->state &= ~NAPI_STATE_SCHED;
    napi->dev->irq_enabled = 1;
    printf("  [NAPI] %s polling complete, IRQ re-enabled\n", 
           napi->driver_name);
}

/* Disable interrupts (called from IRQ handler) */
static void disable_irq(struct net_device *dev)
{
    dev->irq_enabled = 0;
    printf("  [IRQ] %s: IRQ disabled\n", dev->name);
}

/* ==========================================================
 * TEMPLATE METHOD: net_rx_action()
 * 
 * This is the softirq handler that polls all scheduled NAPI.
 * Framework controls budget and completion.
 * ========================================================== */

#define NETDEV_BUDGET  300  /* Max packets per softirq */

/* Global poll list (simplified: just an array for demo) */
static struct napi_struct *poll_list[8];
static int poll_list_count = 0;

void net_rx_action(void)
{
    int budget = NETDEV_BUDGET;
    int i;

    printf("[net_rx_action] TEMPLATE METHOD START\n");
    printf("  [NAPI] Initial budget: %d packets\n", budget);

    /* ========== FIXED STEP: Process poll list ========== */
    for (i = 0; i < poll_list_count && budget > 0; i++) {
        struct napi_struct *napi = poll_list[i];
        int work, weight;

        if (!napi_is_scheduled(napi))
            continue;

        printf("\n  [NAPI] Polling %s (budget=%d)\n", 
               napi->driver_name, budget);

        weight = napi->weight;
        if (weight > budget)
            weight = budget;

        /* ========== VARIATION POINT: Call driver poll ========== */
        printf("  [NAPI] >>> Calling driver poll(weight=%d)\n", weight);
        work = napi->poll(napi, weight);
        printf("  [NAPI] <<< Driver poll returned work=%d\n", work);

        /* ========== FIXED STEP: Update budget ========== */
        budget -= work;
        printf("  [NAPI] Remaining budget: %d\n", budget);

        /* ========== FIXED STEP: Check completion ========== */
        if (work < weight) {
            /* Driver processed all packets, done polling */
            napi_complete(napi);
        } else {
            printf("  [NAPI] More work pending, keep polling\n");
        }

        /* Update statistics */
        napi->dev->rx_packets += work;
    }

    printf("\n[net_rx_action] TEMPLATE METHOD END\n");
    printf("  [NAPI] Total budget consumed: %d packets\n\n", 
           NETDEV_BUDGET - budget);
}

/* ==========================================================
 * DRIVER IMPLEMENTATIONS (Variation Points)
 * ========================================================== */

/* --- e1000-like driver poll --- */
static int e1000_poll(struct napi_struct *napi, int budget)
{
    int work_done = 0;

    printf("    [e1000] Polling RX ring (budget=%d, pending=%d)\n",
           budget, napi->pending_packets);

    /* Process packets up to budget */
    while (work_done < budget && napi->pending_packets > 0) {
        printf("    [e1000] Processing packet from RX descriptor\n");
        napi->pending_packets--;
        work_done++;
    }

    printf("    [e1000] Processed %d packets\n", work_done);
    return work_done;
}

/* --- virtio-net-like driver poll --- */
static int virtio_net_poll(struct napi_struct *napi, int budget)
{
    int work_done = 0;

    printf("    [virtio] Polling RX virtqueue (budget=%d, pending=%d)\n",
           budget, napi->pending_packets);

    /* Process packets up to budget */
    while (work_done < budget && napi->pending_packets > 0) {
        printf("    [virtio] Getting buffer from virtqueue\n");
        napi->pending_packets--;
        work_done++;
    }

    printf("    [virtio] Processed %d packets\n", work_done);
    return work_done;
}

/* --- Driver with high packet rate (tests budget exhaustion) --- */
static int highrate_poll(struct napi_struct *napi, int budget)
{
    int work_done = 0;

    printf("    [highrate] Polling (budget=%d, pending=%d)\n",
           budget, napi->pending_packets);

    /* Always exhaust budget if packets available */
    while (work_done < budget && napi->pending_packets > 0) {
        napi->pending_packets--;
        work_done++;
    }

    printf("    [highrate] Processed %d packets (budget %s)\n", 
           work_done, work_done == budget ? "EXHAUSTED" : "not exhausted");
    return work_done;
}

/* ==========================================================
 * INTERRUPT HANDLER SIMULATION
 * Called when hardware has packets
 * ========================================================== */
void simulate_irq_handler(struct napi_struct *napi, int packet_count)
{
    printf("[IRQ] %s: Received %d packets\n", 
           napi->dev->name, packet_count);

    /* Add packets to pending */
    napi->pending_packets += packet_count;

    /* Disable IRQ and schedule NAPI */
    disable_irq(napi->dev);
    napi_schedule(napi);
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */
int main(void)
{
    printf("==============================================\n");
    printf("NAPI TEMPLATE METHOD DEMONSTRATION\n");
    printf("==============================================\n\n");

    /* Create network devices */
    struct net_device e1000_dev = {
        .name = "eth0",
        .irq_enabled = 1,
    };

    struct net_device virtio_dev = {
        .name = "eth1",
        .irq_enabled = 1,
    };

    struct net_device highrate_dev = {
        .name = "eth2",
        .irq_enabled = 1,
    };

    /* Create NAPI instances */
    struct napi_struct e1000_napi = {
        .poll = e1000_poll,
        .weight = 64,
        .state = 0,
        .dev = &e1000_dev,
        .pending_packets = 0,
        .driver_name = "e1000",
    };

    struct napi_struct virtio_napi = {
        .poll = virtio_net_poll,
        .weight = 64,
        .state = 0,
        .dev = &virtio_dev,
        .pending_packets = 0,
        .driver_name = "virtio-net",
    };

    struct napi_struct highrate_napi = {
        .poll = highrate_poll,
        .weight = 64,
        .state = 0,
        .dev = &highrate_dev,
        .pending_packets = 0,
        .driver_name = "highrate",
    };

    /* Register NAPI instances with poll list */
    poll_list[0] = &e1000_napi;
    poll_list[1] = &virtio_napi;
    poll_list[2] = &highrate_napi;
    poll_list_count = 3;

    /* === Scenario 1: Normal packet reception === */
    printf("=== SCENARIO 1: Normal Packet Reception ===\n\n");
    
    simulate_irq_handler(&e1000_napi, 10);     /* 10 packets */
    simulate_irq_handler(&virtio_napi, 5);     /* 5 packets */
    printf("\n");
    
    net_rx_action();  /* Process all */

    /* === Scenario 2: High packet rate === */
    printf("\n=== SCENARIO 2: Budget Exhaustion ===\n\n");
    
    /* High rate driver gets many packets */
    simulate_irq_handler(&highrate_napi, 500);  /* Exceeds budget! */
    printf("\n");
    
    net_rx_action();  /* First poll - budget limits processing */
    
    printf("\n--- Second softirq (more packets remain) ---\n\n");
    napi_schedule(&highrate_napi);  /* Re-schedule */
    net_rx_action();  /* Continue processing */

    /* Print statistics */
    printf("\n=== STATISTICS ===\n");
    printf("eth0: RX packets=%lu\n", e1000_dev.rx_packets);
    printf("eth1: RX packets=%lu\n", virtio_dev.rx_packets);
    printf("eth2: RX packets=%lu\n", highrate_dev.rx_packets);

    return 0;
}
```

---

## What the Implementation is NOT Allowed to Do

```
+=============================================================================+
|              NAPI DRIVER IMPLEMENTATION RESTRICTIONS                         |
+=============================================================================+

    DRIVER CANNOT:

    1. EXCEED BUDGET
       poll() must return <= budget given
       Cannot process more packets than allowed

       WRONG:
       int poll(napi, budget) {
           while (has_packets())
               process_packet();  // Ignores budget!
           return work_done;
       }

    2. CALL napi_complete() INCORRECTLY
       Framework calls napi_complete when work < weight
       Driver should NOT call it directly in most cases

    3. RE-ENABLE IRQ DIRECTLY
       Framework manages IRQ state via napi_complete
       Driver hardware IRQ enable is coordinated

    4. MODIFY NAPI STATE DIRECTLY
       state field is framework-owned
       Driver only reports work done

    5. TAKE TOO LONG
       poll() should be bounded
       Cannot sleep or wait for events

    6. STARVE OTHER NAPI INSTANCES
       Budget is shared across all NAPI
       Greedy driver hurts all devices

    7. RETURN NEGATIVE VALUES
       Return value must be 0 <= work <= budget
       Framework interprets result directly

    +-----------------------------------------------------------------+
    |  DRIVER POLL FUNCTION IS SIMPLE:                                |
    |  1. Process up to 'budget' packets from hardware                |
    |  2. Return how many were processed                              |
    |  THAT'S ALL - FRAMEWORK HANDLES EVERYTHING ELSE                 |
    +-----------------------------------------------------------------+
```

**中文说明：**

NAPI驱动`poll()`函数的限制：(1) 不能超过预算——必须返回<=给定预算；(2) 不能错误调用`napi_complete()`；(3) 不能直接重新启用中断——由框架管理；(4) 不能直接修改NAPI状态；(5) 不能运行太长时间——不能睡眠或等待；(6) 不能饿死其他NAPI实例；(7) 不能返回负值。驱动的`poll()`函数很简单：从硬件处理最多budget个数据包，返回处理了多少个。其他一切由框架处理。

---

## Real Kernel Code Reference (v3.2)

### net_rx_action() in net/core/dev.c

```c
/* Simplified from actual kernel code */
static void net_rx_action(struct softirq_action *h)
{
    struct list_head *list = &__get_cpu_var(softnet_data).poll_list;
    unsigned long time_limit = jiffies + 2;
    int budget = netdev_budget;

    local_irq_disable();

    while (!list_empty(list)) {
        struct napi_struct *n;
        int work, weight;

        if (unlikely(budget <= 0 || time_after(jiffies, time_limit)))
            goto softnet_break;

        n = list_first_entry(list, struct napi_struct, poll_list);

        weight = n->weight;
        work = n->poll(n, weight);  /* HOOK: driver poll */

        budget -= work;

        if (work < weight) {
            /* Driver done, move off poll list */
            list_del(&n->poll_list);
        }
    }

    local_irq_enable();
}
```

### struct napi_struct in include/linux/netdevice.h

```c
struct napi_struct {
    struct list_head    poll_list;
    unsigned long       state;
    int                 weight;
    int                 (*poll)(struct napi_struct *, int);
    /* ... */
};
```

---

## Key Takeaways

1. **Framework owns polling loop**: `net_rx_action()` controls iteration
2. **Budget is mandatory**: Drivers cannot exceed their allocation
3. **Completion is automatic**: Framework detects when driver is done
4. **IRQ management is centralized**: Framework handles enable/disable
5. **Fairness is guaranteed**: No single driver can monopolize
