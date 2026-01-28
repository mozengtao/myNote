# Core Concept: Chain of Responsibility Pattern

What Chain of Responsibility means in kernel architecture and how it differs from Observer.

---

## What Problem Does Chain of Responsibility Solve?

```
+=============================================================================+
|                    THE CHAIN OF RESPONSIBILITY PROBLEM                       |
+=============================================================================+

    THE PROBLEM:
    ============

    An event needs to be handled, but:
    - Multiple handlers could potentially handle it
    - Don't know which handler will succeed in advance
    - Want to try handlers in order until one succeeds
    - Handler that processes should "consume" the event


    EXAMPLES:
    =========

    Network Packet Filtering (netfilter):
    - Packet arrives
    - Check rule 1: no match, continue
    - Check rule 2: no match, continue
    - Check rule 3: MATCH! DROP packet
    - (remaining rules not checked)

    Shared IRQ Handling:
    - Interrupt fires on shared line
    - Ask device 1: "Is this yours?" NO
    - Ask device 2: "Is this yours?" YES -> handle it
    - (stop asking other devices)

    Exception Handling:
    - Exception occurs
    - Try handler 1: can't recover, pass
    - Try handler 2: RECOVERED!
    - (stop chain)


    THE SOLUTION: HANDLER CHAIN
    ===========================

    Request --> Handler1 --> Handler2 --> Handler3 --> ...
                   |            |            |
              can handle?  can handle?  can handle?
                   |            |            |
                  NO           YES          ---
                   |            |
              pass along    HANDLE IT
                             (stop chain)
```

**中文说明：**

责任链解决的问题：事件需要处理，但不知道哪个处理器会成功处理，需要按顺序尝试直到一个成功。例如网络包过滤（规则匹配）、共享IRQ处理（找到引发中断的设备）、异常处理（找到能恢复的处理器）。解决方案：处理器链——请求依次传递，每个处理器决定是否处理或传递。

---

## Chain of Responsibility vs Observer

```
+=============================================================================+
|                    CHAIN vs OBSERVER                                         |
+=============================================================================+

    KEY DIFFERENCE: Who processes the event?
    ========================================

    OBSERVER (Notifier):
    - ALL handlers are called
    - Broadcast / one-to-many
    - Handlers don't "consume" event
    - Event source doesn't care who handles

    CHAIN OF RESPONSIBILITY:
    - ONE handler processes (first capable)
    - Try until success
    - Handler "consumes" event
    - Processing stops after match


    VISUAL COMPARISON:
    ==================

    OBSERVER:                          CHAIN:
    =========                          ======

    Event                              Event
      |                                  |
      +--> Handler1 [called]             +--> Handler1 [pass]
      |                                  |
      +--> Handler2 [called]             +--> Handler2 [HANDLE]
      |                                  |
      +--> Handler3 [called]             +--> Handler3 [not reached]

    All handlers notified              First match wins


    RETURN VALUES:
    ==============

    OBSERVER:                          CHAIN:
    NOTIFY_OK - processed, continue    HANDLED - stop chain
    NOTIFY_DONE - not interested       NOT_HANDLED - continue
    NOTIFY_STOP - stop (rare)          (return determines flow)
```

**中文说明：**

责任链vs观察者：观察者是所有处理器都被调用（广播），责任链是第一个匹配的处理器处理（停止链）。观察者用于通知，责任链用于找处理器。返回值不同：观察者用NOTIFY_*，责任链用HANDLED/NOT_HANDLED。

---

## How Chain Works

```
    CHAIN MECHANISM:
    ================

    struct handler {
        int (*handle)(void *data);
        int priority;
        struct handler *next;
    };

    int process_chain(struct handler *chain, void *data)
    {
        struct handler *h;

        for (h = chain; h != NULL; h = h->next) {
            int ret = h->handle(data);

            if (ret == HANDLED) {
                return HANDLED;    /* Stop! Someone handled it */
            }
            /* NOT_HANDLED: continue to next handler */
        }

        return NOT_HANDLED;  /* No one handled it */
    }


    KEY PROPERTIES:
    ===============

    1. Early exit on success
    2. Order matters (priority)
    3. Return value controls flow
    4. One handler "wins"
```

---

## Kernel Use Cases

```
    NETFILTER:
    ==========
    - Packet passes through hook chain
    - Each hook can ACCEPT, DROP, or pass
    - First decisive hook wins

    SHARED IRQ:
    ===========
    - Multiple devices share one IRQ line
    - Each handler checks if its device caused interrupt
    - First matching handler processes

    EXCEPTION HANDLING:
    ===================
    - die_chain notifier for exceptions
    - Handlers try to recover
    - First successful recovery wins

    INPUT HANDLERS:
    ===============
    - Input event passed through handler chain
    - First handler that claims event processes it
```

---

## Version

Based on **Linux kernel v3.2** chain patterns.
