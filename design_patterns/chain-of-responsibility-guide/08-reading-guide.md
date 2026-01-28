# Source Reading Guide: Chain of Responsibility

A guided path through Linux kernel v3.2 source code.

---

## Reading Path Overview

```
    PHASE 1: Netfilter
    ==================
    include/linux/netfilter.h     <- nf_hook_ops, return values
    net/netfilter/core.c          <- nf_hook_slow()
    
    PHASE 2: IRQ Handling
    =====================
    include/linux/interrupt.h     <- irqaction, irqreturn_t
    kernel/irq/handle.c           <- handle_IRQ_event()
    
    PHASE 3: Input Handlers
    =======================
    include/linux/input.h         <- input_handler
    drivers/input/input.c         <- input_handle_event()
```

---

## Phase 1: Netfilter

### File: include/linux/netfilter.h

```
    WHAT TO LOOK FOR:
    =================
    
    struct nf_hook_ops:
    - hook function pointer
    - priority field
    - list linkage
    
    Return values:
    - NF_DROP, NF_ACCEPT, NF_STOLEN
```

### File: net/netfilter/core.c

```
    WHAT TO LOOK FOR:
    =================
    
    nf_hook_slow():
    - Chain traversal
    - Verdict handling
    - Early exit on DROP/ACCEPT
    
    nf_register_hook():
    - Priority-ordered insertion
```

**中文说明：**

阶段1：Netfilter。在netfilter.h中查找nf_hook_ops结构和返回值定义。在core.c中学习nf_hook_slow如何遍历链、处理verdict、DROP/ACCEPT时提前退出。

---

## Phase 2: IRQ Handling

### File: include/linux/interrupt.h

```
    WHAT TO LOOK FOR:
    =================
    
    struct irqaction:
    - handler pointer
    - next (chain link)
    - dev_id
    
    irqreturn_t:
    - IRQ_NONE, IRQ_HANDLED
```

### File: kernel/irq/handle.c

```
    WHAT TO LOOK FOR:
    =================
    
    handle_IRQ_event():
    - Handler chain iteration
    - Return value checking
    - How multiple handlers are tried
```

---

## Key Functions to Trace

| Function | File | Purpose |
|----------|------|---------|
| `nf_hook_slow()` | net/netfilter/core.c | Netfilter chain processing |
| `nf_register_hook()` | net/netfilter/core.c | Hook registration |
| `handle_IRQ_event()` | kernel/irq/handle.c | IRQ chain processing |
| `request_irq()` | kernel/irq/manage.c | IRQ handler registration |

---

## Tracing Exercise

```
    TRACE: Netfilter Packet Flow
    ============================
    
    1. Start at ip_rcv() in net/ipv4/ip_input.c
    
    2. Find NF_HOOK() macro call
    
    3. Trace into nf_hook_slow()
    
    4. See how hooks are called in order
    
    5. Observe early exit on NF_DROP
```

---

## Reading Checklist

```
    [ ] Read struct nf_hook_ops
    [ ] Read nf_hook_slow implementation
    [ ] Read struct irqaction
    [ ] Read handle_IRQ_event implementation
    [ ] Understand return value semantics
```

---

## Version

This reading guide is for **Linux kernel v3.2**.
