# Final Mental Model: State Machine Pattern

## One-Paragraph Summary

The State Machine pattern models objects with distinct states and controlled transitions between them. Each object maintains a current state (typically an enum), and operations check the current state before proceeding. State transitions are explicit: code validates the current state, performs transition-specific actions, and updates to the new state. The kernel uses this pattern extensively for protocols (TCP states), device lifecycles (USB device states), and subsystem management (network interface states). Unlike boolean flags that can create invalid combinations, state machines ensure only valid states exist and only legal transitions occur.

**中文总结：**

状态机模式为具有不同状态和受控转换的对象建模。每个对象维护当前状态（通常是枚举），操作在执行前检查当前状态。状态转换是显式的：代码验证当前状态，执行转换特定的动作，然后更新到新状态。内核广泛使用此模式用于协议（TCP状态）、设备生命周期（USB设备状态）、子系统管理（网络接口状态）。与可能产生无效组合的布尔标志不同，状态机确保只存在有效状态且只发生合法转换。

---

## Quick Reference Card

```
+=============================================================================+
|              STATE MACHINE QUICK REFERENCE                                   |
+=============================================================================+

    STRUCTURE:
    ----------
    enum my_state {
        STATE_INIT,
        STATE_READY,
        STATE_RUNNING,
        STATE_STOPPED,
    };

    struct my_object {
        enum my_state state;
        spinlock_t lock;
    };

    TRANSITION PATTERN:
    -------------------
    int transition_to_running(struct my_object *obj)
    {
        spin_lock(&obj->lock);
        
        if (obj->state != STATE_READY) {
            spin_unlock(&obj->lock);
            return -EINVAL;  /* Invalid transition */
        }
        
        /* Perform transition actions */
        do_start_actions();
        
        obj->state = STATE_RUNNING;
        spin_unlock(&obj->lock);
        return 0;
    }

    COMMON STATES:
    --------------
    TCP: CLOSED, LISTEN, SYN_SENT, SYN_RCVD, ESTABLISHED,
         FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT, CLOSING,
         LAST_ACK, TIME_WAIT
    
    USB: USB_STATE_NOTATTACHED, USB_STATE_ATTACHED,
         USB_STATE_POWERED, USB_STATE_DEFAULT,
         USB_STATE_ADDRESS, USB_STATE_CONFIGURED
    
    NET: __LINK_STATE_START, __LINK_STATE_PRESENT,
         __LINK_STATE_NOCARRIER, __LINK_STATE_DORMANT
```

---

## State vs Flags Comparison

```
    FLAGS (Bad for complex state):     STATE MACHINE (Good):
    
    bool is_ready;                     enum state {
    bool is_running;                       INIT,
    bool is_stopped;                       READY,
                                           RUNNING,
    Problem:                               STOPPED
    is_ready=1, is_running=1,          };
    is_stopped=1 ???
    (invalid combination possible)     Only ONE state at a time
```
