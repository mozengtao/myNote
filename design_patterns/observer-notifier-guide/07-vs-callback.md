# Observer/Notifier vs Direct Callback

Understanding when to use notifier chains versus direct callbacks in kernel architecture.

---

## Core Distinction

```
+=============================================================================+
|                    NOTIFIER vs DIRECT CALLBACK                               |
+=============================================================================+

    DIRECT CALLBACK:                    NOTIFIER CHAIN:
    ================                    ===============
    
    Source knows exactly                Source doesn't know
    who to call                         who will respond
    
    struct my_device {                  BLOCKING_NOTIFIER_HEAD(my_chain);
        void (*on_event)(dev);          
    };                                  /* Subscribers register */
                                        register_my_notifier(&nb);
    /* Explicit assignment */
    dev->on_event = handler;            /* Source broadcasts */
                                        call_my_notifiers(event, data);
    /* Direct call */
    dev->on_event(dev);


    RELATIONSHIP:
    =============

    Direct Callback: 1-to-1            Notifier: 1-to-many
    
    [Source] ---> [Handler]            [Source] ---> [Handler 1]
                                                ---> [Handler 2]
                                                ---> [Handler N]
```

**中文说明：**

直接回调vs通知器链的核心区别：直接回调时，源明确知道要调用谁（一对一关系）；通知器链时，源不知道谁会响应（一对多关系）。直接回调通过结构体内的函数指针显式赋值，通知器链通过订阅者动态注册。

---

## Architectural Comparison

```
    DIRECT CALLBACK PATTERN:
    ========================

    +------------+                     +------------+
    |  Device    |-------------------->|  Driver    |
    +------------+   dev->ops->read    +------------+
    |            |                     |            |
    | ops ----+  |                     | my_read()  |
    |         |  |                     |            |
    +---------+--+                     +------------+
              |
              +---> struct operations {
                        int (*read)(dev);
                    };

    - Source has pointer to ONE handler
    - Handler is set at device init
    - Tight coupling by design
    - Used for: polymorphism, driver ops


    NOTIFIER CHAIN PATTERN:
    =======================

    +------------+
    |  Source    |--+
    +------------+  |
    |            |  | call_chain()
    | chain_head |  |
    +------------+  |
                    v
            +-------+-------+-------+
            |       |       |       |
            v       v       v       v
          [nb1]   [nb2]   [nb3]   ...

    - Source has pointer to chain HEAD
    - Handlers register themselves
    - Loose coupling by design
    - Used for: events, notifications
```

---

## Decision Criteria

```
    USE DIRECT CALLBACK WHEN:
    =========================
    
    [X] There's exactly ONE handler per source
    [X] Handler is known at initialization time
    [X] You need polymorphism (Strategy/Template Method)
    [X] Handler is part of the object's identity
    
    Examples:
    - file_operations (one fs per file)
    - net_device_ops (one driver per netdev)
    - platform_driver.probe (one driver per device)


    USE NOTIFIER CHAIN WHEN:
    ========================
    
    [X] Multiple independent handlers possible
    [X] Handlers not known at compile time
    [X] Handlers may load/unload dynamically
    [X] Source should not know about handlers
    
    Examples:
    - netdev_chain (many subsystems care about net events)
    - reboot_notifier (many need to know about shutdown)
    - module_notify (many track module loading)
```

**中文说明：**

使用直接回调的情况：每个源只有一个处理器、处理器在初始化时已知、需要多态性、处理器是对象身份的一部分。使用通知器链的情况：可能有多个独立处理器、处理器在编译时未知、处理器可能动态加载/卸载、源不应该知道处理器。

---

## Code Comparison

```c
/* ============================================
 * DIRECT CALLBACK APPROACH
 * ============================================ */

struct device {
    struct device_ops *ops;
    /* ... */
};

struct device_ops {
    int (*read)(struct device *dev);
    int (*write)(struct device *dev, void *data);
};

/* Single handler per device */
static struct device_ops my_ops = {
    .read = my_read,
    .write = my_write,
};

void init_device(struct device *dev)
{
    dev->ops = &my_ops;  /* Explicit assignment */
}

void use_device(struct device *dev)
{
    dev->ops->read(dev);  /* Direct call to ONE handler */
}


/* ============================================
 * NOTIFIER CHAIN APPROACH
 * ============================================ */

BLOCKING_NOTIFIER_HEAD(device_chain);

struct notifier_block logging_nb = {
    .notifier_call = logging_handler,
};

struct notifier_block stats_nb = {
    .notifier_call = stats_handler,
};

/* Multiple handlers register */
void init_subsystems(void)
{
    register_device_notifier(&logging_nb);
    register_device_notifier(&stats_nb);
    /* More can register dynamically */
}

void device_event(struct device *dev, int event)
{
    /* Broadcast to ALL handlers */
    blocking_notifier_call_chain(&device_chain, event, dev);
}
```

---

## Hybrid Case: Both in Same Subsystem

```
    NETWORK DEVICE USES BOTH:
    =========================

    Direct Callback (ops):              Notifier Chain:
    - ndo_start_xmit                    - netdev_chain
    - ndo_open                          - Events like NETDEV_UP
    - ndo_stop
    
    +----------------+
    | net_device     |
    +----------------+
    | ops ---------> | net_device_ops    <-- Direct: ONE driver
    |                |                       implements these
    +----------------+
           |
           | When state changes...
           v
    call_netdevice_notifiers(NETDEV_UP)  <-- Notifier: MANY
           |                                 subsystems respond
           +---> routing
           +---> firewall
           +---> bonding
           +---> ...

    KEY INSIGHT:
    - ops: HOW to operate the device (Strategy/Template)
    - notifier: WHAT happened to the device (Observer)
```

**中文说明：**

混合案例：网络设备同时使用两种模式。直接回调（ops）用于设备操作——一个驱动实现这些操作；通知器链用于状态变化事件——多个子系统响应。关键洞察：ops定义如何操作设备（策略/模板方法），notifier通知设备发生了什么（观察者）。

---

## Anti-Pattern: Using Wrong Pattern

```c
/* WRONG: Using direct callback for broadcast */
struct system {
    /* Trying to notify multiple handlers with direct callback */
    void (*on_event1)(struct system *);
    void (*on_event2)(struct system *);
    void (*on_event3)(struct system *);
    /* What if we need more? Modify struct! */
};

/* WRONG: Using notifier for polymorphism */
struct device {
    /* Trying to implement operations via notifier */
    struct notifier_head read_chain;  /* Overkill! */
};

int read_device(struct device *dev)
{
    /* Which handler actually does the read? */
    /* Multiple handlers all trying to read? */
    return notifier_call_chain(&dev->read_chain, ...);  /* Confused */
}


/* CORRECT USAGE */

/* Direct callback for polymorphism */
struct device {
    struct device_ops *ops;  /* ONE implementation */
};

/* Notifier for events */
BLOCKING_NOTIFIER_HEAD(device_events);  /* MANY listeners */
```

---

## Summary Table

| Aspect | Direct Callback | Notifier Chain |
|--------|-----------------|----------------|
| **Relationship** | 1-to-1 | 1-to-many |
| **Handler count** | Exactly one | Zero to many |
| **Known at** | Init time | Runtime |
| **Coupling** | Tight (by design) | Loose |
| **Purpose** | Polymorphism | Events |
| **Registration** | Assignment | register() |
| **Examples** | file_ops, netdev_ops | netdev_chain, reboot |

---

## Version

This comparison is based on **Linux kernel v3.2** patterns.
