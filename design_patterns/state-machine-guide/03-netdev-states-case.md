# Case 1: Network Device States

The network device state machine demonstrates state management for complex hardware with multiple state dimensions.

---

## Subsystem Context

```
+=============================================================================+
|                    NETWORK DEVICE STATE MACHINE                              |
+=============================================================================+

    NETWORK DEVICES HAVE MULTIPLE STATE DIMENSIONS:
    ===============================================

    1. Administrative State (IFF_UP)
       - Controlled by admin (ifconfig up/down)
       - Indicates desired state
    
    2. Operational State (operstate)
       - Reflects actual link condition
       - RFC 2863 compliant
    
    3. Link State (carrier)
       - Physical layer status
       - Carrier present or not
    
    4. Queue State
       - Can transmit or not
       - Watchdog status


    STATE RELATIONSHIP:
    ===================

    Admin wants UP  +  Carrier present  =  Operstate UP
    Admin wants UP  +  No carrier       =  Operstate DOWN (or LOWERLAYERDOWN)
    Admin wants DOWN                    =  Operstate DOWN
```

**中文说明：**

网络设备状态机有多个状态维度：(1)管理状态(IFF_UP)——由管理员控制（ifconfig up/down）；(2)操作状态(operstate)——反映实际链路状态，符合RFC 2863；(3)链路状态(carrier)——物理层状态；(4)队列状态——能否传输。这些状态相互关联：管理员想UP + 有载波 = 操作状态UP；管理员想UP + 无载波 = 操作状态DOWN。

---

## State Definitions

```c
/* Operational states - include/linux/if.h */
enum {
    IF_OPER_UNKNOWN,        /* State unknown */
    IF_OPER_NOTPRESENT,     /* Hardware not present */
    IF_OPER_DOWN,           /* Not operational */
    IF_OPER_LOWERLAYERDOWN, /* Lower layer down */
    IF_OPER_TESTING,        /* In test mode */
    IF_OPER_DORMANT,        /* Waiting for event */
    IF_OPER_UP,             /* Operational */
};

/* Link state bits - include/linux/netdevice.h */
enum netdev_state_t {
    __LINK_STATE_START,     /* Device opened */
    __LINK_STATE_PRESENT,   /* Hardware present */
    __LINK_STATE_NOCARRIER, /* No carrier signal */
    __LINK_STATE_LINKWATCH_PENDING,
    __LINK_STATE_DORMANT,   /* Waiting for external event */
};
```

---

## State Transition Diagram

```
    OPERSTATE TRANSITIONS:
    ======================

                           dev_open()
    +----------+  --------------------------->  +----------+
    |          |                                |          |
    |   DOWN   |                                |    UP    |
    |          |  <---------------------------  |          |
    +----------+         dev_close()            +----------+
         |                                           |
         | (error)              carrier_off()       | carrier_on()
         v                           |               |
    +----------+                     v               v
    |NOTPRESENT|              +-----------+    +-----------+
    +----------+              | NO CARRIER|    |  RUNNING  |
                              +-----------+    +-----------+


    DETAILED STATE MACHINE:
    =======================

    User space                  Kernel                    Hardware
    ==========                  ======                    ========

    ifconfig up
         |
         +---> dev_open()
                  |
                  +---> set __LINK_STATE_START
                  |
                  +---> driver->ndo_open()
                               |
                               +---> hardware init
                               |
                               +---> netif_carrier_on() (if link)
                                         |
                                         +---> operstate = UP
```

**中文说明：**

操作状态转换：dev_open()使设备从DOWN变为UP，dev_close()使设备从UP变为DOWN。carrier_on()/carrier_off()控制载波状态。详细流程：用户空间ifconfig up -> 内核dev_open() -> 设置LINK_STATE_START -> 驱动ndo_open() -> 硬件初始化 -> 如果有链路则netif_carrier_on() -> operstate变为UP。

---

## Key Functions

```c
/* Opening a device - net/core/dev.c */
int dev_open(struct net_device *dev)
{
    int ret;
    
    /* Can't open if not present */
    if (!netif_device_present(dev))
        return -ENODEV;
    
    /* Already up? */
    if (dev->flags & IFF_UP)
        return 0;
    
    /* Call driver's open */
    ret = __dev_open(dev);
    if (ret)
        return ret;
    
    /* Set administrative state UP */
    dev->flags |= IFF_UP;
    
    /* Notify (observer pattern!) */
    call_netdevice_notifiers(NETDEV_UP, dev);
    
    return 0;
}

/* Carrier state change - net/core/link_watch.c */
void netif_carrier_on(struct net_device *dev)
{
    if (test_and_clear_bit(__LINK_STATE_NOCARRIER, &dev->state)) {
        /* Carrier detected */
        linkwatch_fire_event(dev);
    }
}

void netif_carrier_off(struct net_device *dev)
{
    if (!test_and_set_bit(__LINK_STATE_NOCARRIER, &dev->state)) {
        /* Carrier lost */
        linkwatch_fire_event(dev);
    }
}
```

---

## Minimal C Simulation

```c
/* Simplified network device state machine */

#include <stdio.h>
#include <string.h>

/* Operational states (RFC 2863) */
enum operstate {
    IF_OPER_UNKNOWN,
    IF_OPER_NOTPRESENT,
    IF_OPER_DOWN,
    IF_OPER_LOWERLAYERDOWN,
    IF_OPER_DORMANT,
    IF_OPER_UP,
};

/* Link state bits */
#define LINK_STATE_START     (1 << 0)
#define LINK_STATE_PRESENT   (1 << 1)
#define LINK_STATE_NOCARRIER (1 << 2)

/* Interface flags */
#define IFF_UP      (1 << 0)
#define IFF_RUNNING (1 << 1)

/* Network device */
struct net_device {
    char name[16];
    unsigned int flags;
    unsigned long state;
    unsigned char operstate;
};

/* Driver operations */
struct net_device_ops {
    int (*ndo_open)(struct net_device *dev);
    int (*ndo_stop)(struct net_device *dev);
};

/* ====== STATE QUERY FUNCTIONS ====== */

int netif_running(struct net_device *dev)
{
    return dev->state & LINK_STATE_START;
}

int netif_carrier_ok(struct net_device *dev)
{
    return !(dev->state & LINK_STATE_NOCARRIER);
}

const char *operstate_str(enum operstate state)
{
    static const char *names[] = {
        "UNKNOWN", "NOTPRESENT", "DOWN",
        "LOWERLAYERDOWN", "DORMANT", "UP"
    };
    return names[state];
}

/* ====== STATE TRANSITION FUNCTIONS ====== */

void update_operstate(struct net_device *dev)
{
    enum operstate new_state;
    
    if (!(dev->state & LINK_STATE_PRESENT)) {
        new_state = IF_OPER_NOTPRESENT;
    } else if (!(dev->flags & IFF_UP)) {
        new_state = IF_OPER_DOWN;
    } else if (dev->state & LINK_STATE_NOCARRIER) {
        new_state = IF_OPER_DOWN;  /* or LOWERLAYERDOWN */
    } else {
        new_state = IF_OPER_UP;
    }
    
    if (dev->operstate != new_state) {
        printf("  [STATE] %s: operstate %s -> %s\n",
               dev->name, 
               operstate_str(dev->operstate),
               operstate_str(new_state));
        dev->operstate = new_state;
    }
}

void netif_carrier_on(struct net_device *dev)
{
    if (dev->state & LINK_STATE_NOCARRIER) {
        printf("  [CARRIER] %s: carrier detected\n", dev->name);
        dev->state &= ~LINK_STATE_NOCARRIER;
        dev->flags |= IFF_RUNNING;
        update_operstate(dev);
    }
}

void netif_carrier_off(struct net_device *dev)
{
    if (!(dev->state & LINK_STATE_NOCARRIER)) {
        printf("  [CARRIER] %s: carrier lost\n", dev->name);
        dev->state |= LINK_STATE_NOCARRIER;
        dev->flags &= ~IFF_RUNNING;
        update_operstate(dev);
    }
}

/* ====== DEVICE OPERATIONS ====== */

int dev_open(struct net_device *dev, struct net_device_ops *ops)
{
    int ret;
    
    printf("[DEV] Opening %s...\n", dev->name);
    
    if (!(dev->state & LINK_STATE_PRESENT)) {
        printf("  [ERROR] Device not present\n");
        return -1;
    }
    
    if (dev->flags & IFF_UP) {
        printf("  [INFO] Already up\n");
        return 0;
    }
    
    /* Set link state START */
    dev->state |= LINK_STATE_START;
    
    /* Call driver open */
    if (ops && ops->ndo_open) {
        ret = ops->ndo_open(dev);
        if (ret) {
            dev->state &= ~LINK_STATE_START;
            return ret;
        }
    }
    
    /* Set admin state UP */
    dev->flags |= IFF_UP;
    
    printf("  [STATE] %s: IFF_UP set\n", dev->name);
    update_operstate(dev);
    
    return 0;
}

int dev_close(struct net_device *dev, struct net_device_ops *ops)
{
    printf("[DEV] Closing %s...\n", dev->name);
    
    if (!(dev->flags & IFF_UP)) {
        printf("  [INFO] Already down\n");
        return 0;
    }
    
    /* Call driver stop */
    if (ops && ops->ndo_stop)
        ops->ndo_stop(dev);
    
    /* Clear states */
    dev->flags &= ~(IFF_UP | IFF_RUNNING);
    dev->state &= ~LINK_STATE_START;
    
    printf("  [STATE] %s: IFF_UP cleared\n", dev->name);
    update_operstate(dev);
    
    return 0;
}

/* ====== EXAMPLE DRIVER ====== */

int example_driver_open(struct net_device *dev)
{
    printf("  [DRIVER] Hardware init...\n");
    
    /* Simulate: carrier detected after init */
    netif_carrier_on(dev);
    
    return 0;
}

int example_driver_stop(struct net_device *dev)
{
    printf("  [DRIVER] Hardware shutdown...\n");
    netif_carrier_off(dev);
    return 0;
}

static struct net_device_ops example_ops = {
    .ndo_open = example_driver_open,
    .ndo_stop = example_driver_stop,
};

/* ====== MAIN ====== */

int main(void)
{
    struct net_device eth0 = {
        .name = "eth0",
        .flags = 0,
        .state = LINK_STATE_PRESENT | LINK_STATE_NOCARRIER,
        .operstate = IF_OPER_DOWN,
    };
    
    printf("=== NETWORK DEVICE STATE MACHINE ===\n\n");
    
    printf("Initial state: operstate=%s, carrier=%s\n\n",
           operstate_str(eth0.operstate),
           netif_carrier_ok(&eth0) ? "OK" : "NO");
    
    /* Admin brings interface up */
    printf("--- Admin: ifconfig eth0 up ---\n");
    dev_open(&eth0, &example_ops);
    
    /* Simulate carrier loss */
    printf("\n--- Hardware: cable disconnected ---\n");
    netif_carrier_off(&eth0);
    
    /* Simulate carrier return */
    printf("\n--- Hardware: cable reconnected ---\n");
    netif_carrier_on(&eth0);
    
    /* Admin brings interface down */
    printf("\n--- Admin: ifconfig eth0 down ---\n");
    dev_close(&eth0, &example_ops);
    
    return 0;
}

/*
 * Output:
 *
 * === NETWORK DEVICE STATE MACHINE ===
 *
 * Initial state: operstate=DOWN, carrier=NO
 *
 * --- Admin: ifconfig eth0 up ---
 * [DEV] Opening eth0...
 *   [STATE] eth0: IFF_UP set
 *   [DRIVER] Hardware init...
 *   [CARRIER] eth0: carrier detected
 *   [STATE] eth0: operstate DOWN -> UP
 *
 * --- Hardware: cable disconnected ---
 *   [CARRIER] eth0: carrier lost
 *   [STATE] eth0: operstate UP -> DOWN
 *
 * --- Hardware: cable reconnected ---
 *   [CARRIER] eth0: carrier detected
 *   [STATE] eth0: operstate DOWN -> UP
 *
 * --- Admin: ifconfig eth0 down ---
 * [DEV] Closing eth0...
 *   [DRIVER] Hardware shutdown...
 *   [CARRIER] eth0: carrier lost
 *   [STATE] eth0: IFF_UP cleared
 */
```

---

## What Core Does NOT Control

```
    Core Controls:
    --------------
    [X] State definitions (enum)
    [X] State transition functions
    [X] State query functions
    [X] Notification on state change

    Core Does NOT Control:
    ----------------------
    [ ] When carrier appears/disappears
    [ ] Hardware-specific init/shutdown
    [ ] Physical layer behavior
    [ ] Link negotiation

    Driver Responsibilities:
    ------------------------
    - Call netif_carrier_on/off based on hardware
    - Implement ndo_open/ndo_stop
    - Report hardware state accurately
```

**中文说明：**

核心控制：状态定义、状态转换函数、状态查询函数、状态变化通知。核心不控制：载波何时出现/消失、硬件特定初始化/关闭、物理层行为、链路协商。驱动职责：根据硬件调用netif_carrier_on/off、实现ndo_open/ndo_stop、准确报告硬件状态。

---

## Version

This case study is based on **Linux kernel v3.2**.

Key source files:
- `net/core/dev.c` - dev_open(), dev_close()
- `net/core/link_watch.c` - carrier state handling
- `include/linux/netdevice.h` - state definitions
