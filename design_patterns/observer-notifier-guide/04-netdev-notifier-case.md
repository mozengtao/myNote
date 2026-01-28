# Case 2: Network Device Notifier Chain

The network device notifier chain demonstrates the Observer pattern for hardware/subsystem events propagating throughout the kernel.

---

## Subsystem Context

```
+=============================================================================+
|                    NETWORK DEVICE NOTIFICATION                               |
+=============================================================================+

    NETWORK SUBSYSTEM HAS MANY INTERESTED PARTIES:
    ==============================================

    Network Interface              Subsystems That Care
    =================             ====================
    
    eth0 comes UP          -->    - Routing (update routes)
                                  - Firewall (apply rules)
                                  - Bonding (update bond status)
                                  - VLAN (configure VLANs)
                                  - IPv6 (autoconfiguration)
                                  - Bridge (update STP)
                                  - Wireless (update state)
                                  - Many loadable modules...

    Without notifier chains:
    - net core would need to know ALL subsystems
    - Adding new subsystem = modifying net core
    - Tight coupling nightmare


    WITH NOTIFIER CHAIN:
    ====================

    +----------------+
    | net core       |
    +----------------+
           |
           | call_netdevice_notifiers(NETDEV_UP, dev)
           v
    +------+------+     +------+------+     +------+------+
    | routing     | --> | firewall    | --> | bonding     | --> ...
    | notifier    |     | notifier    |     | notifier    |
    +-------------+     +-------------+     +-------------+
           |                  |                   |
           v                  v                   v
      fib_netdev_event   nf_netdev_event   bond_netdev_event
```

**中文说明：**

网络设备通知器链展示了观察者模式在网络子系统中的应用。当网络接口状态变化时（如UP/DOWN），许多子系统需要响应：路由需要更新路由表、防火墙需要应用规则、绑定驱动需要更新状态、VLAN需要配置等。没有通知器链，网络核心需要知道所有这些子系统，添加新子系统就要修改核心代码。使用通知器链后，核心只需调用call_netdevice_notifiers，所有注册的订阅者都会收到通知。

---

## The Network Device Notifier Chain

```
    CHAIN DECLARATION:
    ==================

    /* net/core/dev.c */
    RAW_NOTIFIER_HEAD(netdev_chain);

    /* Why RAW_NOTIFIER_HEAD? */
    - Network code manages its own locking (rtnl_lock)
    - Chain used in both process and softirq context
    - Custom synchronization requirements


    NOTIFICATION EVENTS:
    ====================

    Event               When Triggered              What Changed
    -----               --------------              ------------
    NETDEV_UP           Device opened               IFF_UP set
    NETDEV_DOWN         Device closed               IFF_UP cleared
    NETDEV_REGISTER     Device registered           dev added to list
    NETDEV_UNREGISTER   Device unregistered         dev being removed
    NETDEV_CHANGE       Device state change         flags/features
    NETDEV_CHANGEMTU    MTU changed                 dev->mtu
    NETDEV_CHANGEADDR   MAC address changed         dev->dev_addr
    NETDEV_CHANGENAME   Name changed                dev->name
    NETDEV_GOING_DOWN   About to close              Prepare for down
```

---

## Key Functions

```c
/* Registration - net/core/dev.c */
int register_netdevice_notifier(struct notifier_block *nb)
{
    struct net_device *dev;
    int err;

    rtnl_lock();
    
    /* Add to chain */
    err = raw_notifier_chain_register(&netdev_chain, nb);
    if (err)
        goto unlock;

    /* Notify about existing devices */
    for_each_netdev(&init_net, dev) {
        err = nb->notifier_call(nb, NETDEV_REGISTER, dev);
        err = notifier_to_errno(err);
        if (err)
            goto rollback;
        
        if (dev->flags & IFF_UP) {
            nb->notifier_call(nb, NETDEV_UP, dev);
        }
    }

unlock:
    rtnl_unlock();
    return err;
}

/* Notification - net/core/dev.c */
int call_netdevice_notifiers(unsigned long val, 
                             struct net_device *dev)
{
    return raw_notifier_call_chain(&netdev_chain, val, dev);
}
```

**中文说明：**

网络设备通知器使用RAW_NOTIFIER_HEAD，因为网络代码有自己的锁管理（rtnl_lock），链在进程和软中断上下文中都使用，有自定义的同步需求。register_netdevice_notifier函数不仅将订阅者添加到链上，还会为已存在的设备发送通知，确保新订阅者知道当前状态。call_netdevice_notifiers是触发通知的函数。

---

## Minimal C Simulation

```c
/* Simplified netdev notifier simulation */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Event types */
#define NETDEV_UP       0x0001
#define NETDEV_DOWN     0x0002
#define NETDEV_REGISTER 0x0003
#define NETDEV_CHANGE   0x0004

/* Return values */
#define NOTIFY_OK       0x0001
#define NOTIFY_DONE     0x0000

/* Network device (simplified) */
struct net_device {
    char name[16];
    unsigned int flags;
    #define IFF_UP 0x1
};

/* Notifier block */
struct notifier_block {
    int (*notifier_call)(struct notifier_block *nb,
                         unsigned long event, void *data);
    struct notifier_block *next;
    int priority;
};

/* ====== CHAIN HEAD ====== */
static struct notifier_block *netdev_chain_head = NULL;

/* Register to chain */
int register_netdevice_notifier(struct notifier_block *nb)
{
    struct notifier_block **p;
    
    /* Insert by priority (higher first) */
    for (p = &netdev_chain_head; *p; p = &(*p)->next) {
        if (nb->priority > (*p)->priority)
            break;
    }
    nb->next = *p;
    *p = nb;
    
    printf("[NET] Registered notifier (priority %d)\n", nb->priority);
    return 0;
}

/* Call all notifiers */
int call_netdevice_notifiers(unsigned long event, 
                             struct net_device *dev)
{
    struct notifier_block *nb;
    int ret;
    
    printf("[NET] Broadcasting event %lu for %s\n", event, dev->name);
    
    for (nb = netdev_chain_head; nb; nb = nb->next) {
        ret = nb->notifier_call(nb, event, dev);
        /* Could check for NOTIFY_STOP here */
    }
    return NOTIFY_OK;
}

/* ====== ROUTING SUBSYSTEM (SUBSCRIBER 1) ====== */

int routing_netdev_event(struct notifier_block *nb,
                         unsigned long event, void *data)
{
    struct net_device *dev = data;
    
    switch (event) {
    case NETDEV_UP:
        printf("  [ROUTING] Interface %s is UP - adding routes\n", 
               dev->name);
        break;
    case NETDEV_DOWN:
        printf("  [ROUTING] Interface %s is DOWN - removing routes\n",
               dev->name);
        break;
    }
    return NOTIFY_OK;
}

static struct notifier_block routing_notifier = {
    .notifier_call = routing_netdev_event,
    .priority = 100,  /* High priority - routing first */
};

/* ====== FIREWALL SUBSYSTEM (SUBSCRIBER 2) ====== */

int firewall_netdev_event(struct notifier_block *nb,
                          unsigned long event, void *data)
{
    struct net_device *dev = data;
    
    switch (event) {
    case NETDEV_UP:
        printf("  [FIREWALL] Interface %s UP - applying rules\n",
               dev->name);
        break;
    case NETDEV_DOWN:
        printf("  [FIREWALL] Interface %s DOWN - clearing rules\n",
               dev->name);
        break;
    }
    return NOTIFY_OK;
}

static struct notifier_block firewall_notifier = {
    .notifier_call = firewall_netdev_event,
    .priority = 50,  /* Lower priority - after routing */
};

/* ====== BONDING SUBSYSTEM (SUBSCRIBER 3) ====== */

int bonding_netdev_event(struct notifier_block *nb,
                         unsigned long event, void *data)
{
    struct net_device *dev = data;
    
    switch (event) {
    case NETDEV_UP:
        printf("  [BONDING] Interface %s UP - checking bond status\n",
               dev->name);
        break;
    case NETDEV_DOWN:
        printf("  [BONDING] Interface %s DOWN - failover check\n",
               dev->name);
        break;
    }
    return NOTIFY_OK;
}

static struct notifier_block bonding_notifier = {
    .notifier_call = bonding_netdev_event,
    .priority = 0,  /* Default priority */
};

/* ====== USAGE EXAMPLE ====== */

/* Network core triggers notifications */
void netif_carrier_on(struct net_device *dev)
{
    dev->flags |= IFF_UP;
    call_netdevice_notifiers(NETDEV_UP, dev);
}

void netif_carrier_off(struct net_device *dev)
{
    dev->flags &= ~IFF_UP;
    call_netdevice_notifiers(NETDEV_DOWN, dev);
}

int main(void)
{
    struct net_device eth0 = { .name = "eth0", .flags = 0 };

    printf("=== NETWORK DEVICE NOTIFIER DEMONSTRATION ===\n\n");

    /* Subsystems register during initialization */
    printf("--- Subsystem Registration ---\n");
    register_netdevice_notifier(&routing_notifier);
    register_netdevice_notifier(&firewall_notifier);
    register_netdevice_notifier(&bonding_notifier);

    /* Network core broadcasts events */
    printf("\n--- Network Events ---\n");
    
    printf("\nBringing eth0 UP:\n");
    netif_carrier_on(&eth0);
    
    printf("\nTaking eth0 DOWN:\n");
    netif_carrier_off(&eth0);

    return 0;
}

/*
 * Output:
 *
 * === NETWORK DEVICE NOTIFIER DEMONSTRATION ===
 *
 * --- Subsystem Registration ---
 * [NET] Registered notifier (priority 100)
 * [NET] Registered notifier (priority 50)
 * [NET] Registered notifier (priority 0)
 *
 * --- Network Events ---
 *
 * Bringing eth0 UP:
 * [NET] Broadcasting event 1 for eth0
 *   [ROUTING] Interface eth0 is UP - adding routes
 *   [FIREWALL] Interface eth0 UP - applying rules
 *   [BONDING] Interface eth0 UP - checking bond status
 *
 * Taking eth0 DOWN:
 * [NET] Broadcasting event 2 for eth0
 *   [ROUTING] Interface eth0 is DOWN - removing routes
 *   [FIREWALL] Interface eth0 DOWN - clearing rules
 *   [BONDING] Interface eth0 DOWN - failover check
 */
```

---

## What Core Does NOT Control

```
+=============================================================================+
|                    WHAT NETWORK CORE DOES NOT CONTROL                        |
+=============================================================================+

    Core Controls:
    --------------
    [X] When events are generated
    [X] What data is passed (struct net_device *)
    [X] Order of notification (by priority)
    [X] Chain traversal mechanics

    Core Does NOT Control:
    ----------------------
    [ ] Which subsystems subscribe
    [ ] What subscribers do with events
    [ ] How many subscribers exist
    [ ] Subscriber internal logic

    This separation enables:
    ------------------------
    - Modules can register without net core knowing
    - New subsystems added without modifying net/core/dev.c
    - Subsystems can be loaded/unloaded dynamically
    - Clean architecture boundary
```

**中文说明：**

网络核心控制：事件何时生成、传递什么数据、按优先级通知顺序、链遍历机制。网络核心不控制：哪些子系统订阅、订阅者如何处理事件、有多少订阅者、订阅者内部逻辑。这种分离使模块可以注册而核心不知道、新子系统可以添加而不修改核心代码、子系统可以动态加载/卸载。

---

## Real Kernel Subscribers (v3.2)

```
    REGISTERED NOTIFIERS IN KERNEL:
    ==============================

    File                          Function                Priority
    ----                          --------                --------
    net/ipv4/fib_frontend.c       fib_netdev_event        0
    net/ipv4/devinet.c            inetdev_event           0
    net/ipv6/addrconf.c           addrconf_notify         0
    drivers/net/bonding/bond.c    bond_netdev_event       0
    net/8021q/vlan.c              vlan_device_event       0
    net/bridge/br_notify.c        br_device_event         0
    net/netfilter/core.c          nf_netdev_event         0

    Each handles NETDEV_* events independently,
    network core has no knowledge of them.
```

---

## Why Observer Pattern Here

```
    WITHOUT OBSERVER:                  WITH OBSERVER:
    ================                   ===============

    netif_carrier_on() {               netif_carrier_on() {
        /* Hard-coded calls */             /* Single broadcast */
        fib_update();                      call_netdevice_notifiers(
        inetdev_update();                      NETDEV_UP, dev);
        addrconf_update();             }
        bond_update();
        vlan_update();
        bridge_update();
        /* What about new code? */
    }

    Problems:                          Benefits:
    - net core must know everyone      - Decoupled subsystems
    - Adding subscriber = modify       - Dynamic registration
    - Can't build modular kernels      - Works with loadable modules
    - Circular dependencies            - Clean dependency graph
```

---

## Version

This case study is based on **Linux kernel v3.2**.

Key source files:
- `net/core/dev.c` - netdev_chain definition and notification calls
- `include/linux/netdevice.h` - NETDEV_* event definitions
