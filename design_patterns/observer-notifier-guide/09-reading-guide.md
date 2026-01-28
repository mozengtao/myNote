# Source Reading Guide: Observer/Notifier Pattern

A guided path through Linux kernel v3.2 source code for understanding the Observer/Notifier pattern.

---

## Reading Path Overview

```
+=============================================================================+
|                    READING PATH FOR NOTIFIER PATTERN                         |
+=============================================================================+

    PHASE 1: Core Infrastructure
    ============================
    include/linux/notifier.h      <- Data structures, macros
    kernel/notifier.c             <- Implementation
    
    PHASE 2: Simple Example
    =======================
    kernel/sys.c                  <- Reboot notifier
    
    PHASE 3: Complex Example
    ========================
    net/core/dev.c                <- Network device notifier
    net/ipv4/fib_frontend.c       <- Subscriber example
    
    PHASE 4: Tracing Exercise
    =========================
    Follow NETDEV_UP notification from source to handlers
```

---

## Phase 1: Core Infrastructure

### File: include/linux/notifier.h

```
    WHAT TO LOOK FOR:
    =================
    
    Line ~20-40:   struct notifier_block definition
    Line ~50-80:   Chain head definitions (BLOCKING, ATOMIC, RAW, SRCU)
    Line ~90-120:  Initialization macros
    Line ~130-160: Return value definitions (NOTIFY_OK, etc.)
    Line ~170+:    Function declarations


    KEY STRUCTURES:
    ===============

    struct notifier_block {
        int (*notifier_call)(...);   <- The callback function
        struct notifier_block *next; <- Linked list pointer
        int priority;                <- Ordering (higher = first)
    };

    struct blocking_notifier_head {
        struct rw_semaphore rwsem;   <- Read-write lock
        struct notifier_block *head; <- List head
    };

    struct atomic_notifier_head {
        spinlock_t lock;
        struct notifier_block *head;
    };
```

### File: kernel/notifier.c

```
    WHAT TO LOOK FOR:
    =================
    
    Line ~20-50:   notifier_chain_register() - insertion by priority
    Line ~60-90:   notifier_chain_unregister() - removal
    Line ~100-140: __notifier_call_chain() - core iteration logic
    
    Line ~150+:    Type-specific wrappers:
                   - atomic_notifier_chain_register()
                   - blocking_notifier_chain_register()
                   - raw_notifier_chain_register()


    KEY FUNCTION TO STUDY:
    ======================

    static int __notifier_call_chain(...)
    {
        while (nb) {
            ret = nb->notifier_call(nb, val, v);
            if ((ret & NOTIFY_STOP_MASK) == NOTIFY_STOP_MASK)
                break;
            nb = nb->next;
        }
        return ret;
    }
    
    This is the core iteration - understand this thoroughly.
```

**中文说明：**

阶段1：核心基础设施。在include/linux/notifier.h中查找notifier_block结构体定义、各种链头定义、初始化宏、返回值定义。在kernel/notifier.c中查找注册函数（按优先级插入）、注销函数、核心迭代逻辑__notifier_call_chain。理解迭代函数是关键——它遍历链表，调用每个回调，检查是否应停止。

---

## Phase 2: Simple Example - Reboot Notifier

### File: kernel/sys.c

```
    WHAT TO LOOK FOR:
    =================
    
    Search for: "reboot_notifier_list"
    
    Declaration:
        BLOCKING_NOTIFIER_HEAD(reboot_notifier_list);
    
    Registration API:
        int register_reboot_notifier(struct notifier_block *nb)
        {
            return blocking_notifier_chain_register(
                &reboot_notifier_list, nb);
        }
    
    Usage in kernel_restart_prepare():
        blocking_notifier_call_chain(&reboot_notifier_list,
                                     SYS_RESTART, cmd);


    TRACE THIS PATH:
    ================
    
    1. Find BLOCKING_NOTIFIER_HEAD declaration
    2. Find register_reboot_notifier()
    3. Find where chain is called (kernel_restart_prepare)
    4. Search for "register_reboot_notifier" to find subscribers
```

### Finding Subscribers

```bash
# In kernel source directory:
grep -r "register_reboot_notifier" --include="*.c"

# Expected results (v3.2):
drivers/watchdog/watchdog_dev.c
drivers/acpi/sleep.c
drivers/char/ipmi/ipmi_*.c
arch/x86/kernel/*.c
...
```

---

## Phase 3: Complex Example - Network Device Notifier

### File: net/core/dev.c

```
    WHAT TO LOOK FOR:
    =================
    
    Search for: "netdev_chain"
    
    Declaration (~line 1400):
        static RAW_NOTIFIER_HEAD(netdev_chain);
    
    Registration (~line 1420):
        int register_netdevice_notifier(struct notifier_block *nb)
        {
            rtnl_lock();
            err = raw_notifier_chain_register(&netdev_chain, nb);
            /* ... notify about existing devices ... */
            rtnl_unlock();
        }
    
    Notification (~line 1500):
        int call_netdevice_notifiers(unsigned long val,
                                     struct net_device *dev)
        {
            return raw_notifier_call_chain(&netdev_chain, val, dev);
        }


    WHY RAW_NOTIFIER?
    =================
    
    Network code uses rtnl_lock() for synchronization.
    RAW_NOTIFIER provides no locking - caller (network code) manages it.
    This allows fine-grained control over when locking happens.
```

### File: include/linux/netdevice.h

```
    WHAT TO LOOK FOR:
    =================
    
    Search for: "NETDEV_"
    
    Event definitions:
        #define NETDEV_UP       0x0001
        #define NETDEV_DOWN     0x0002
        #define NETDEV_CHANGE   0x0004
        ...
    
    Understanding events helps trace notification paths.
```

### File: net/ipv4/fib_frontend.c (Subscriber Example)

```
    WHAT TO LOOK FOR:
    =================
    
    Search for: "fib_netdev_event"
    
    Handler function:
        static int fib_netdev_event(struct notifier_block *this,
                                    unsigned long event, void *ptr)
        {
            struct net_device *dev = ptr;
            
            switch (event) {
            case NETDEV_UP:
                fib_add_ifaddr(...);
                break;
            case NETDEV_DOWN:
                fib_del_ifaddr(...);
                break;
            }
            return NOTIFY_DONE;
        }
    
    Registration:
        static struct notifier_block fib_netdev_notifier = {
            .notifier_call = fib_netdev_event,
        };
        
        register_netdevice_notifier(&fib_netdev_notifier);
```

**中文说明：**

阶段3：复杂示例——网络设备通知器。在net/core/dev.c中查找netdev_chain声明、注册函数register_netdevice_notifier、通知函数call_netdevice_notifiers。网络代码使用RAW_NOTIFIER是因为有自己的锁管理（rtnl_lock）。在net/ipv4/fib_frontend.c中可以看到一个订阅者示例：fib_netdev_event处理器根据事件类型执行不同操作。

---

## Phase 4: Tracing Exercise

### Exercise: Trace NETDEV_UP Notification

```
    GOAL: Trace what happens when a network interface comes up
    
    STEP 1: Find where NETDEV_UP is generated
    =========================================
    
    grep -r "NETDEV_UP" net/core/dev.c
    
    Look for:
        call_netdevice_notifiers(NETDEV_UP, dev);
    
    Find containing function (e.g., dev_open())
    
    
    STEP 2: Identify the notification call
    ======================================
    
    In dev_open() or __dev_open():
        ...
        call_netdevice_notifiers(NETDEV_UP, dev);
        ...
    
    
    STEP 3: Find all subscribers
    ============================
    
    grep -r "register_netdevice_notifier" --include="*.c"
    
    For each subscriber, find the handler function and
    look for "case NETDEV_UP:" to see what happens.
    
    
    STEP 4: Draw the call graph
    ===========================
    
    dev_open()
         |
         v
    call_netdevice_notifiers(NETDEV_UP, dev)
         |
         +---> fib_netdev_event()     (routing)
         +---> inetdev_event()        (IP config)
         +---> addrconf_notify()      (IPv6)
         +---> bond_netdev_event()    (bonding)
         +---> vlan_device_event()    (VLAN)
         +---> ...
```

---

## Key Files Summary

| File | What It Contains |
|------|------------------|
| `include/linux/notifier.h` | Data structures, macros, constants |
| `kernel/notifier.c` | Core notifier chain implementation |
| `kernel/sys.c` | Reboot notifier (simple example) |
| `net/core/dev.c` | Network device notifier (complex) |
| `include/linux/netdevice.h` | NETDEV_* event definitions |
| `net/ipv4/fib_frontend.c` | FIB subscriber example |
| `net/ipv4/devinet.c` | IPv4 device subscriber |

---

## Key Functions to Understand

```
    INFRASTRUCTURE:
    ===============
    notifier_chain_register()     - Add to chain (by priority)
    notifier_chain_unregister()   - Remove from chain
    __notifier_call_chain()       - Core iteration
    
    BLOCKING NOTIFIER:
    ==================
    blocking_notifier_chain_register()
    blocking_notifier_chain_unregister()
    blocking_notifier_call_chain()
    
    ATOMIC NOTIFIER:
    ================
    atomic_notifier_chain_register()
    atomic_notifier_chain_unregister()
    atomic_notifier_call_chain()
    
    RAW NOTIFIER:
    =============
    raw_notifier_chain_register()
    raw_notifier_chain_unregister()
    raw_notifier_call_chain()
```

---

## Reading Checklist

```
    [ ] Read struct notifier_block definition
    [ ] Read notifier head definitions (blocking, atomic, raw)
    [ ] Understand notifier_chain_register() insertion logic
    [ ] Understand __notifier_call_chain() iteration
    [ ] Study reboot_notifier_list as simple example
    [ ] Study netdev_chain as complex example
    [ ] Find and read one subscriber (e.g., fib_netdev_event)
    [ ] Trace one complete notification path (NETDEV_UP)
    [ ] Understand return value handling (NOTIFY_OK, NOTIFY_STOP)
```

---

## Version

This reading guide is for **Linux kernel v3.2**.

Source code can be browsed at:
- https://elixir.bootlin.com/linux/v3.2/source
- Local clone of kernel source
