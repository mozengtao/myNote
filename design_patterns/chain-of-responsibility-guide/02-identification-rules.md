# Identification Rules: Chain of Responsibility

Five concrete rules to identify Chain of Responsibility in kernel code.

---

## Rule 1: Handler Chain Structure

```c
/* Linked list of handlers with return values */
struct nf_hook_ops {
    struct list_head list;
    nf_hookfn *hook;      /* Handler function */
    int priority;
};

struct irqaction {
    irq_handler_t handler;
    struct irqaction *next;
};

/* SIGNAL: Linked list of handlers, processed in order */
```

**中文说明：**

规则1：处理器链结构——处理器的链表，按顺序处理。

---

## Rule 2: Return Value Protocol

```c
/* Return values control chain traversal */

/* Netfilter */
#define NF_DROP   0   /* Stop, drop packet */
#define NF_ACCEPT 1   /* Stop, accept packet */

/* IRQ */
#define IRQ_NONE    0  /* Not my interrupt, continue */
#define IRQ_HANDLED 1  /* Handled, stop */

/* SIGNAL: Return values that mean "stop" vs "continue" */
```

---

## Rule 3: Early Exit on Success

```c
/* Chain traversal with early exit */
int process_chain(struct handler *chain, void *data)
{
    struct handler *h;

    for (h = chain; h; h = h->next) {
        int ret = h->handle(data);

        if (ret == HANDLED)    /* <-- Early exit */
            return HANDLED;
    }
    return NOT_HANDLED;
}

/* SIGNAL: Loop exits early when handler succeeds */
```

**中文说明：**

规则3：成功时提前退出——处理器返回HANDLED时循环退出。

---

## Rule 4: Priority/Order Determines Behavior

```c
/* Registration by priority */
int nf_register_hook(struct nf_hook_ops *ops)
{
    /* Insert by priority - determines check order */
    list_for_each_entry(elem, &nf_hooks, list) {
        if (ops->priority < elem->priority) {
            list_add(&ops->list, elem->list.prev);
            return 0;
        }
    }
}

/* SIGNAL: Priority field, ordered registration */
```

---

## Rule 5: Handler Checks Responsibility

```c
/* Each handler checks if it should handle */
irqreturn_t my_irq_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    /* Check if this device caused interrupt */
    if (!my_device_has_interrupt(dev))
        return IRQ_NONE;  /* Not mine, pass to next */

    /* Handle the interrupt */
    process_interrupt(dev);
    return IRQ_HANDLED;  /* Stop chain */
}

/* SIGNAL: Handler first checks if responsible */
```

**中文说明：**

规则5：处理器检查责任——每个处理器首先检查自己是否应该处理。

---

## Summary Checklist

```
+=============================================================================+
|                    CHAIN IDENTIFICATION CHECKLIST                            |
+=============================================================================+

    [ ] 1. HANDLER CHAIN STRUCTURE
        Linked list of handlers
    
    [ ] 2. RETURN VALUE PROTOCOL
        HANDLED/NOT_HANDLED or similar
    
    [ ] 3. EARLY EXIT ON SUCCESS
        Loop breaks when handler succeeds
    
    [ ] 4. PRIORITY ORDERING
        Priority field, ordered registration
    
    [ ] 5. RESPONSIBILITY CHECK
        Handler verifies before processing

    SCORING:
    3+ indicators = Chain of Responsibility
```

---

## Red Flags: NOT Chain of Responsibility

```
    THESE ARE NOT CHAIN:
    ====================

    1. All handlers called regardless of return
       -> That's OBSERVER

    2. No return value checking
       -> That's broadcast

    3. No early exit
       -> Not Chain pattern
```

---

## Version

Based on **Linux kernel v3.2**.
