# Core Concept: Event Notification Chains

## The Publish-Subscribe Problem

```
+=============================================================================+
|                    THE NOTIFICATION PROBLEM                                  |
+=============================================================================+

    SCENARIO: Network Interface State Change
    =========================================

    When eth0 goes UP:
    - Routing subsystem needs to update routes
    - Firewall needs to apply rules
    - Bonding driver needs to update bond state
    - VLAN driver needs to configure VLANs
    - IPv6 needs to start address configuration
    - Many more subsystems need to know...


    BAD SOLUTION: Direct Calls
    ==========================

    void netdev_set_state(struct net_device *dev, int state)
    {
        dev->state = state;
        
        /* Network code knows about ALL subscribers! */
        routing_notify(dev, state);
        firewall_notify(dev, state);
        bonding_notify(dev, state);
        vlan_notify(dev, state);
        ipv6_notify(dev, state);
        /* ... forever growing list ... */
    }

    PROBLEMS:
    - Network code must know all subscribers
    - Adding subscriber requires modifying network code
    - Tight coupling
    - No dynamic registration


    GOOD SOLUTION: Notifier Chain
    =============================

    static BLOCKING_NOTIFIER_HEAD(netdev_chain);

    void netdev_set_state(struct net_device *dev, int state)
    {
        dev->state = state;
        
        /* Network code knows NOTHING about subscribers */
        blocking_notifier_call_chain(&netdev_chain, state, dev);
    }

    /* Each subsystem registers itself */
    static struct notifier_block routing_nb = {
        .notifier_call = routing_netdev_event,
    };
    blocking_notifier_chain_register(&netdev_chain, &routing_nb);

    BENEFITS:
    - Source doesn't know subscribers
    - Subscribers register dynamically
    - Loose coupling
    - Easy to add/remove subscribers
```

**中文说明：**

发布-订阅问题：当网络接口状态变化时，多个子系统需要通知。坏的解决方案是直接调用——网络代码必须知道所有订阅者，添加订阅者需要修改网络代码，紧耦合。好的解决方案是通知者链——网络代码不知道订阅者，每个子系统自己注册，松耦合，易于添加/删除订阅者。

---

## How Notifier Chains Work

```
+=============================================================================+
|                    NOTIFIER CHAIN MECHANISM                                  |
+=============================================================================+

    STRUCTURE:
    ==========

    blocking_notifier_head
    +------------------+
    | rwsem (lock)     |
    | head ------------|---+
    +------------------+   |
                           v
    notifier_block        notifier_block        notifier_block
    +---------------+     +---------------+     +---------------+
    | callback -----+     | callback -----+     | callback -----+
    | next ---------|---->| next ---------|---->| next (NULL)   |
    | priority: 10  |     | priority: 5   |     | priority: 0   |
    +---------------+     +---------------+     +---------------+
                          (higher priority first)


    REGISTRATION:
    =============

    struct notifier_block my_nb = {
        .notifier_call = my_callback,
        .priority = 5,
    };
    
    blocking_notifier_chain_register(&chain, &my_nb);
    
    Result: my_nb inserted in priority order


    NOTIFICATION:
    =============

    blocking_notifier_call_chain(&chain, EVENT_TYPE, data);
    
    1. Acquire rwsem for reading
    2. Walk the chain from head to tail
    3. Call each notifier_block's callback
    4. Pass EVENT_TYPE and data to each callback
    5. Check return value (stop if NOTIFY_STOP)
    6. Release rwsem
```

**中文说明：**

通知者链机制：结构上是链表，每个节点是notifier_block（包含回调函数、下一个指针、优先级）。注册时按优先级顺序插入。通知时遍历链表，调用每个回调函数，传递事件类型和数据，如果回调返回NOTIFY_STOP则停止遍历。

---

## Callback Return Values

```c
/*
 * Callback functions return these values:
 */

#define NOTIFY_DONE      0x0000  /* Don't care, continue */
#define NOTIFY_OK        0x0001  /* Processed, continue */
#define NOTIFY_STOP_MASK 0x8000  /* Stop bit */
#define NOTIFY_BAD       (NOTIFY_STOP_MASK|0x0002)  /* Stop, error */
#define NOTIFY_STOP      (NOTIFY_STOP_MASK|NOTIFY_OK)  /* Stop, OK */

/*
 * Usage in callback:
 */
int my_callback(struct notifier_block *nb, unsigned long event, void *data)
{
    switch (event) {
    case EVENT_I_CARE_ABOUT:
        /* Process event */
        if (error)
            return NOTIFY_BAD;  /* Stop chain, report error */
        return NOTIFY_OK;       /* Processed, continue chain */
    
    default:
        return NOTIFY_DONE;     /* Not my event, continue */
    }
}


/*
 * Chain caller checks result:
 */
ret = blocking_notifier_call_chain(&chain, event, data);
if (ret & NOTIFY_STOP_MASK) {
    /* Some callback stopped the chain */
}
```

---

## Types of Notifier Chains

```
+=============================================================================+
|                    NOTIFIER CHAIN TYPES                                      |
+=============================================================================+

    TYPE                    | CONTEXT    | LOCKING      | USE CASE
    ========================|============|==============|===================
    ATOMIC_NOTIFIER_HEAD    | Atomic     | RCU          | Interrupt handlers
    BLOCKING_NOTIFIER_HEAD  | Can sleep  | rwsem        | Most kernel code
    RAW_NOTIFIER_HEAD       | Any        | None (user)  | Special cases
    SRCU_NOTIFIER_HEAD      | Can sleep  | SRCU         | Sleepable + RCU


    CHOOSING THE RIGHT TYPE:
    ========================

    Can callbacks sleep?
        NO  --> ATOMIC_NOTIFIER_HEAD or RAW_NOTIFIER_HEAD
        YES --> BLOCKING_NOTIFIER_HEAD or SRCU_NOTIFIER_HEAD

    Is notification from atomic context?
        YES --> ATOMIC_NOTIFIER_HEAD
        NO  --> BLOCKING_NOTIFIER_HEAD

    Need custom locking?
        YES --> RAW_NOTIFIER_HEAD
        NO  --> Use standard type

    Need RCU for read-side?
        YES --> ATOMIC_NOTIFIER_HEAD or SRCU_NOTIFIER_HEAD
        NO  --> BLOCKING_NOTIFIER_HEAD
```

**中文说明：**

通知者链类型：(1) ATOMIC——原子上下文，RCU保护，用于中断处理；(2) BLOCKING——可以睡眠，rwsem保护，最常用；(3) RAW——无锁定，用户负责同步，特殊情况；(4) SRCU——可以睡眠，SRCU保护。选择标准：回调能否睡眠、通知是否在原子上下文、是否需要自定义锁定、是否需要RCU读侧。

---

## Pattern vs Strategy

```
    OBSERVER/NOTIFIER:                    STRATEGY:
    ==================                    =========

    One-to-MANY notification              One algorithm selected
    All callbacks called                  One implementation called
    Decoupled subscribers                 Pluggable algorithm

    Event source:                         Context:
    "Something happened"                  "Do this task using strategy"
           |                                      |
           v                                      v
    +------+------+------+                +---------------+
    | cb1  | cb2  | cb3  |                | one strategy  |
    +------+------+------+                +---------------+


    OBSERVER USE:                         STRATEGY USE:
    - Event broadcasting                  - Algorithm selection
    - Status changes                      - Policy implementation
    - Hooks for extensions                - Replaceable behavior
```

---

## Summary

The Observer/Notifier pattern enables:

1. **Decoupling**: Source doesn't know subscribers
2. **Dynamic registration**: Subscribers can register/unregister at runtime
3. **One-to-many**: Single event, multiple handlers
4. **Priority ordering**: Control callback execution order
5. **Extensibility**: Add subscribers without modifying source
