# Chain of Responsibility vs Observer

The critical distinction between these two patterns in kernel design.

---

## The Fundamental Difference

```
+=============================================================================+
|                    CHAIN vs OBSERVER                                         |
+=============================================================================+

    OBSERVER (Notifier Chain):
    ==========================
    
    Event --> Handler1 [called]
        |
        +--> Handler2 [called]
        |
        +--> Handler3 [called]
    
    ALL handlers notified
    Handlers don't "consume" event
    Return values rarely affect flow


    CHAIN OF RESPONSIBILITY:
    ========================
    
    Event --> Handler1 [pass]
                |
                +--> Handler2 [HANDLE] --> STOP
                |
                +--> Handler3 [not reached]
    
    ONE handler processes
    Handler "consumes" event
    Return value determines flow
```

**中文说明：**

基本区别：观察者是所有处理器都被调用（通知），责任链是第一个能处理的处理器处理后停止。观察者用于广播，责任链用于找处理器。

---

## When to Use Each

```
    USE OBSERVER WHEN:
    ==================
    
    - Multiple subsystems need to know about event
    - Event is informational (state change notification)
    - All interested parties should respond
    - Order doesn't affect correctness
    
    Examples:
    - Network device UP/DOWN
    - Reboot notification
    - CPU hotplug events


    USE CHAIN WHEN:
    ===============
    
    - Need to find one handler for event
    - First capable handler should process
    - Processing should stop after handling
    - Order determines who handles
    
    Examples:
    - Packet filtering (which rule matches)
    - Shared IRQ (which device caused it)
    - Exception recovery (which handler can fix)
```

---

## Code Comparison

```c
/* OBSERVER: All handlers called */
void notify_all(struct notifier_block *chain, int event)
{
    struct notifier_block *nb;
    
    for (nb = chain; nb; nb = nb->next) {
        nb->notifier_call(nb, event, NULL);
        /* Continue regardless of return */
    }
}


/* CHAIN: Stop on success */
int try_handlers(struct handler *chain, void *data)
{
    struct handler *h;
    
    for (h = chain; h; h = h->next) {
        if (h->handle(data) == HANDLED)
            return HANDLED;  /* STOP! */
    }
    return NOT_HANDLED;
}
```

---

## Real Kernel Examples

```
    OBSERVER:                       CHAIN:
    =========                       ======
    
    netdev_chain                    nf_hooks (netfilter)
    reboot_notifier_list            irqaction chain
    inetaddr_chain                  die_chain
    pm_chain_head                   input_handler_list
```

---

## Common Mistakes

```
    MISTAKE 1: Using Observer when Chain needed
    -------------------------------------------
    If only one handler should process, use Chain.
    Observer will call all handlers unnecessarily.
    
    
    MISTAKE 2: Using Chain when Observer needed
    -------------------------------------------
    If all interested parties should respond, use Observer.
    Chain will stop after first handler.
    
    
    MISTAKE 3: Mixed semantics
    --------------------------
    Don't make handler return values ambiguous.
    Clearly define: does return stop traversal or not?
```

---

## Version

Based on **Linux kernel v3.2** patterns.
