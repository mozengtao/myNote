# Final Mental Model: Chain of Responsibility

## One-Paragraph Summary

Chain of Responsibility is used when an event needs one handler, but multiple handlers could potentially handle it. The event passes through a chain of handlers in priority order, each checking if they can handle it. The first handler that can handle the event processes it and returns HANDLED, stopping the chain. If a handler cannot handle the event, it returns NOT_HANDLED and the event continues to the next handler. This differs from Observer where all handlers are notified. Key kernel uses: netfilter packet filtering (NF_DROP/ACCEPT), shared IRQ handling (IRQ_HANDLED/NONE), exception recovery (die_chain).

**中文总结：**

责任链用于事件需要一个处理器但多个处理器可能处理的情况。事件按优先级顺序通过处理器链，每个检查是否能处理。第一个能处理的处理器处理事件并返回HANDLED，停止链。如果处理器不能处理，返回NOT_HANDLED，事件继续到下一个处理器。这与观察者（所有处理器都被通知）不同。内核关键用途：netfilter包过滤、共享IRQ处理、异常恢复。

---

## Decision Flowchart

```
    Event needs handling - how many handlers should process?
                |
        +-------+-------+
        |               |
       ONE           MULTIPLE
        |               |
        v               v
      CHAIN         OBSERVER

    
    CHAIN decision:
                |
    Does return value control flow?
                |
        +-------+-------+
        |               |
       YES              NO
        |               |
        v               v
      CHAIN        Simple callback
```

---

## Quick Reference

```
    CHAIN STRUCTURE:
    ================
    
    struct handler {
        handler_fn fn;
        int priority;
        struct handler *next;
    };


    CHAIN PROCESSING:
    =================
    
    for (h = chain; h; h = h->next) {
        if (h->fn(data) == HANDLED)
            return HANDLED;  // STOP
    }
    return NOT_HANDLED;


    KEY RETURNS:
    ============
    
    Netfilter: NF_DROP (stop), NF_ACCEPT (continue)
    IRQ: IRQ_HANDLED (stop), IRQ_NONE (continue)
```

---

## Version

Based on **Linux kernel v3.2** chain patterns.
