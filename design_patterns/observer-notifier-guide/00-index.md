# Observer/Notifier Pattern in Linux Kernel (v3.2)

Event notification chains for decoupled one-to-many communication.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept: Event Notification Chains |
| [02-identification-rules.md](02-identification-rules.md) | Identification Rules |
| [03-blocking-notifier-case.md](03-blocking-notifier-case.md) | Case 1: Blocking Notifier Chains |
| [04-netdev-notifier-case.md](04-netdev-notifier-case.md) | Case 2: Network Device Notifier |
| [05-reboot-notifier-case.md](05-reboot-notifier-case.md) | Case 3: Reboot Notifier Chain |
| [06-unified-skeleton.md](06-unified-skeleton.md) | Unified Skeleton |
| [07-vs-callback.md](07-vs-callback.md) | Notifier vs Direct Callback |
| [08-antipatterns.md](08-antipatterns.md) | Anti-Patterns |
| [09-reading-guide.md](09-reading-guide.md) | Source Reading Guide |
| [10-mental-model.md](10-mental-model.md) | Final Mental Model |

---

## Overview Diagram

```
+=============================================================================+
|                    OBSERVER/NOTIFIER PATTERN                                 |
+=============================================================================+

    THE PROBLEM:
    ============

    Event Source                    Multiple Interested Parties
    ============                    ==========================

    Network Interface               - Routing subsystem
    goes UP/DOWN                    - Firewall (netfilter)
                                    - Bonding driver
                                    - VLAN subsystem
                                    - Many more...

    How does source notify ALL interested parties
    without knowing who they are?


    THE SOLUTION: NOTIFIER CHAIN
    ============================

    +-------------+
    | Event       |
    | Source      |
    +------+------+
           |
           | call_notifier_chain(event)
           v
    +------+------+     +------+------+     +------+------+
    | Notifier 1  | --> | Notifier 2  | --> | Notifier 3  |
    | (Routing)   |     | (Firewall)  |     | (Bonding)   |
    +-------------+     +-------------+     +-------------+
           |                  |                   |
           v                  v                   v
       callback()         callback()          callback()


    KEY PROPERTIES:
    - Source doesn't know subscribers
    - Subscribers register dynamically
    - One event, many handlers
    - Decoupled architecture
```

**中文说明：**

观察者/通知者模式解决的问题：事件源（如网络接口状态变化）需要通知多个感兴趣的订阅者（路由、防火墙、绑定驱动等），但事件源不应该知道具体有谁在订阅。解决方案：通知者链——订阅者动态注册到链上，事件发生时遍历链调用所有订阅者的回调函数。关键特性：源不知道订阅者、订阅者动态注册、一个事件多个处理器、解耦的架构。

---

## Notifier Chain Types

```
    KERNEL PROVIDES FOUR TYPES:
    ===========================

    1. ATOMIC_NOTIFIER_HEAD
       - Callbacks run in atomic context
       - Cannot sleep
       - RCU protected
       - For interrupt handlers, etc.

    2. BLOCKING_NOTIFIER_HEAD
       - Callbacks can sleep
       - Protected by rwsem
       - Most common type

    3. RAW_NOTIFIER_HEAD
       - No locking provided
       - Caller responsible for synchronization
       - Lightweight

    4. SRCU_NOTIFIER_HEAD
       - Sleepable RCU
       - Callbacks can sleep
       - SRCU protected
```

---

## Key Terminology

| Term | Meaning |
|------|---------|
| **Notifier chain** | List of callbacks to invoke on event |
| **Notifier block** | Structure describing one subscriber |
| **Register** | Add callback to chain |
| **Unregister** | Remove callback from chain |
| **Notify** | Invoke all callbacks in chain |
| **Event** | The notification type (e.g., NETDEV_UP) |
| **Priority** | Order of callback invocation |

---

## Basic Structure

```c
/* include/linux/notifier.h */

struct notifier_block {
    int (*notifier_call)(struct notifier_block *nb,
                         unsigned long event, void *data);
    struct notifier_block *next;
    int priority;
};

/* Blocking notifier chain head */
struct blocking_notifier_head {
    struct rw_semaphore rwsem;
    struct notifier_block *head;
};

/* Declare a chain */
BLOCKING_NOTIFIER_HEAD(my_chain);

/* Register */
int blocking_notifier_chain_register(
    struct blocking_notifier_head *nh,
    struct notifier_block *nb);

/* Unregister */
int blocking_notifier_chain_unregister(
    struct blocking_notifier_head *nh,
    struct notifier_block *nb);

/* Notify all */
int blocking_notifier_call_chain(
    struct blocking_notifier_head *nh,
    unsigned long val, void *v);
```

---

## Version

This guide targets **Linux kernel v3.2**.
