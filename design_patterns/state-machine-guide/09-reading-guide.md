# Source Reading Guide: State Machine Pattern

A guided path through Linux kernel v3.2 source code for understanding state machines.

---

## Reading Path Overview

```
    PHASE 1: TCP State Machine
    ==========================
    include/net/tcp_states.h  <- State definitions
    net/ipv4/tcp.c            <- tcp_set_state()
    net/ipv4/tcp_input.c      <- tcp_rcv_state_process()
    
    PHASE 2: USB State Machine
    ==========================
    include/linux/usb/ch9.h   <- State definitions
    drivers/usb/core/usb.c    <- usb_set_device_state()
    
    PHASE 3: Network Device
    =======================
    include/linux/if.h        <- Operstate definitions
    net/core/dev.c            <- dev_open(), dev_close()
    net/core/link_watch.c     <- Carrier handling
```

---

## Phase 1: TCP State Machine

```
    FILE: include/net/tcp_states.h
    ==============================
    
    Look for enum defining TCP states:
    - TCP_ESTABLISHED
    - TCP_SYN_SENT
    - TCP_LISTEN
    - etc.


    FILE: net/ipv4/tcp.c
    ====================
    
    Search for: tcp_set_state
    
    Study:
    - How state is changed
    - Side effects on state change
    - State-dependent behavior


    FILE: net/ipv4/tcp_input.c
    ==========================
    
    Search for: tcp_rcv_state_process
    
    Study:
    - switch(sk->sk_state) pattern
    - Different handling per state
    - State transitions on events
```

**中文说明：**

阶段1：TCP状态机。在tcp_states.h中查找状态定义，在tcp.c中学习tcp_set_state如何改变状态和副作用，在tcp_input.c中学习tcp_rcv_state_process如何根据状态处理数据包。

---

## Phase 2: USB State Machine

```
    FILE: include/linux/usb/ch9.h
    =============================
    
    Search for: enum usb_device_state
    - USB_STATE_NOTATTACHED
    - USB_STATE_ATTACHED
    - USB_STATE_CONFIGURED
    - etc.


    FILE: drivers/usb/core/usb.c
    ============================
    
    Search for: usb_set_device_state
    
    Study:
    - Validation logic
    - Transition rules
    - Locking
```

---

## Phase 3: Network Device

```
    FILE: include/linux/if.h
    ========================
    
    Search for: IF_OPER_
    - IF_OPER_UNKNOWN
    - IF_OPER_DOWN
    - IF_OPER_UP
    - etc.


    FILE: net/core/dev.c
    ====================
    
    Study: dev_open(), dev_close()
    - State flag manipulation
    - Transition sequence
    - Notifications
```

---

## Key Functions to Study

| Function | File | Purpose |
|----------|------|---------|
| `tcp_set_state()` | net/ipv4/tcp.c | TCP state change |
| `tcp_rcv_state_process()` | net/ipv4/tcp_input.c | State-based processing |
| `usb_set_device_state()` | drivers/usb/core/usb.c | USB state change |
| `dev_open()` | net/core/dev.c | Network device UP |
| `dev_close()` | net/core/dev.c | Network device DOWN |

---

## Reading Checklist

```
    [ ] Read TCP state enum definitions
    [ ] Trace tcp_set_state() implementation
    [ ] Study tcp_rcv_state_process() switch
    [ ] Read USB state definitions
    [ ] Trace usb_set_device_state()
    [ ] Study network device state handling
```

---

## Version

This reading guide is for **Linux kernel v3.2**.
