# Final Mental Model: Observer/Notifier Pattern

## One-Paragraph Summary

The Observer/Notifier pattern enables decoupled one-to-many event notification through notifier chains. A chain head maintains a priority-ordered list of notifier blocks, each containing a callback function. Subsystems register their notifier blocks to receive events; when an event occurs, the source calls the chain, which invokes all registered callbacks in priority order. Callbacks return NOTIFY_OK/NOTIFY_DONE to continue or NOTIFY_STOP to halt the chain. The kernel provides four chain types: BLOCKING (callbacks can sleep, rwsem protected), ATOMIC (no sleeping, RCU protected), RAW (no locking), and SRCU (sleepable RCU). This pattern allows adding new event handlers without modifying event sources.

**中文总结：**

观察者/通知者模式通过通知者链实现解耦的一对多事件通知。链头维护一个按优先级排序的notifier_block列表，每个block包含一个回调函数。子系统注册其notifier_block来接收事件；当事件发生时，源调用链，链按优先级顺序调用所有注册的回调。回调返回NOTIFY_OK/NOTIFY_DONE继续或NOTIFY_STOP停止链。内核提供四种链类型：BLOCKING（可睡眠，rwsem保护）、ATOMIC（不可睡眠，RCU保护）、RAW（无锁）、SRCU（可睡眠RCU）。此模式允许添加新的事件处理器而不修改事件源。

---

## Quick Reference Card

```
+=============================================================================+
|              NOTIFIER PATTERN QUICK REFERENCE                                |
+=============================================================================+

    CHAIN TYPES:
    ------------
    BLOCKING_NOTIFIER_HEAD   - Callbacks can sleep, rwsem lock
    ATOMIC_NOTIFIER_HEAD     - No sleeping, RCU protection
    RAW_NOTIFIER_HEAD        - No locking (caller manages)
    SRCU_NOTIFIER_HEAD       - Sleepable, SRCU protection

    STRUCTURE:
    ----------
    struct notifier_block {
        int (*notifier_call)(nb, event, data);
        struct notifier_block *next;
        int priority;
    };

    OPERATIONS:
    -----------
    xxx_notifier_chain_register(&head, &nb)
    xxx_notifier_chain_unregister(&head, &nb)
    xxx_notifier_call_chain(&head, event, data)

    RETURN VALUES:
    --------------
    NOTIFY_DONE   0x0000   Continue, not processed
    NOTIFY_OK     0x0001   Continue, processed
    NOTIFY_STOP   0x8001   Stop chain, OK
    NOTIFY_BAD    0x8002   Stop chain, error

    COMMON CHAINS:
    --------------
    netdev_chain          - Network device events
    reboot_notifier_list  - System reboot
    panic_notifier_list   - Kernel panic
    pm_chain_head         - Power management
```

---

## Decision Flowchart

```
    Do callbacks need to sleep?
            |
    +-------+-------+
    |               |
   YES              NO
    |               |
    v               v
BLOCKING        Is notification from
or SRCU         atomic context?
                    |
            +-------+-------+
            |               |
           YES              NO
            |               |
            v               v
        ATOMIC          RAW (if custom
                        locking needed)
```
