# Case 2: Shared IRQ Handling

Shared interrupts use Chain of Responsibility to find the device that caused the interrupt.

---

## Subsystem Context

```
+=============================================================================+
|                    SHARED IRQ CHAIN                                          |
+=============================================================================+

    THE PROBLEM:
    ============

    Multiple devices can share one IRQ line:
    - PCI devices often share IRQs
    - When interrupt fires, don't know which device

    Question: Which device caused the interrupt?


    THE SOLUTION: HANDLER CHAIN
    ===========================

    IRQ Fires
        |
        v
    Handler1: "Is this mine?"
        |
       NO
        |
        v
    Handler2: "Is this mine?"
        |
       YES! -> Handle interrupt
        |
    return IRQ_HANDLED (stop chain)


    HANDLER RESPONSIBILITY:
    =======================

    Each handler MUST:
    1. Check if its device caused interrupt
    2. If NO: return IRQ_NONE (continue chain)
    3. If YES: handle it, return IRQ_HANDLED (stop)
```

**中文说明：**

共享IRQ问题：多个设备共享一条IRQ线，中断触发时不知道哪个设备引发。解决方案：处理器链——每个处理器检查是否是自己的设备，如果不是返回IRQ_NONE继续，如果是则处理并返回IRQ_HANDLED停止链。

---

## Return Values

```c
/* include/linux/irqreturn.h */

enum irqreturn {
    IRQ_NONE        = 0,  /* Not my interrupt - continue chain */
    IRQ_HANDLED     = 1,  /* I handled it - stop chain */
    IRQ_WAKE_THREAD = 2,  /* Wake threaded handler */
};

typedef enum irqreturn irqreturn_t;
```

---

## Key Structures

```c
/* IRQ action structure */
struct irqaction {
    irq_handler_t handler;     /* Handler function */
    unsigned long flags;
    void *dev_id;              /* Device identifier */
    struct irqaction *next;    /* Chain link */
    int irq;
    const char *name;
};

/* Handler function signature */
typedef irqreturn_t (*irq_handler_t)(int irq, void *dev_id);
```

---

## Key Functions

```c
/* kernel/irq/handle.c */
irqreturn_t handle_IRQ_event(unsigned int irq, struct irqaction *action)
{
    irqreturn_t ret, retval = IRQ_NONE;

    /* Walk the handler chain */
    do {
        ret = action->handler(irq, action->dev_id);

        switch (ret) {
        case IRQ_HANDLED:
            retval = ret;
            break;
        case IRQ_NONE:
            /* Continue to next handler */
            break;
        }

        action = action->next;
    } while (action);

    return retval;
}
```

---

## Minimal C Simulation

```c
/* Shared IRQ simulation */

#include <stdio.h>
#include <stdlib.h>

typedef enum { IRQ_NONE = 0, IRQ_HANDLED = 1 } irqreturn_t;

typedef irqreturn_t (*irq_handler_t)(int irq, void *dev_id);

struct irqaction {
    irq_handler_t handler;
    void *dev_id;
    const char *name;
    struct irqaction *next;
};

struct device {
    int id;
    int irq_pending;  /* 1 if this device caused interrupt */
};

/* IRQ chain */
static struct irqaction *irq_chain = NULL;

/* Register handler */
int request_irq(irq_handler_t handler, void *dev_id, const char *name)
{
    struct irqaction *action = malloc(sizeof(*action));
    action->handler = handler;
    action->dev_id = dev_id;
    action->name = name;
    action->next = irq_chain;
    irq_chain = action;
    
    printf("[IRQ] Registered handler: %s\n", name);
    return 0;
}

/* Handle IRQ - chain of responsibility */
irqreturn_t handle_irq(int irq)
{
    struct irqaction *action;
    irqreturn_t ret, retval = IRQ_NONE;

    printf("[IRQ] Interrupt %d fired\n", irq);

    for (action = irq_chain; action; action = action->next) {
        printf("  [IRQ] Trying handler: %s\n", action->name);
        
        ret = action->handler(irq, action->dev_id);

        if (ret == IRQ_HANDLED) {
            printf("  [IRQ] %s handled the interrupt\n", action->name);
            retval = ret;
            break;  /* Stop chain */
        } else {
            printf("  [IRQ] %s: not my interrupt\n", action->name);
        }
    }

    if (retval == IRQ_NONE) {
        printf("[IRQ] WARNING: No handler claimed interrupt!\n");
    }

    return retval;
}

/* Example device handlers */
irqreturn_t device1_handler(int irq, void *dev_id)
{
    struct device *dev = dev_id;
    
    if (!dev->irq_pending)
        return IRQ_NONE;  /* Not mine */
    
    printf("    [DEV1] Processing interrupt\n");
    dev->irq_pending = 0;
    return IRQ_HANDLED;
}

irqreturn_t device2_handler(int irq, void *dev_id)
{
    struct device *dev = dev_id;
    
    if (!dev->irq_pending)
        return IRQ_NONE;  /* Not mine */
    
    printf("    [DEV2] Processing interrupt\n");
    dev->irq_pending = 0;
    return IRQ_HANDLED;
}

irqreturn_t device3_handler(int irq, void *dev_id)
{
    struct device *dev = dev_id;
    
    if (!dev->irq_pending)
        return IRQ_NONE;  /* Not mine */
    
    printf("    [DEV3] Processing interrupt\n");
    dev->irq_pending = 0;
    return IRQ_HANDLED;
}

int main(void)
{
    struct device dev1 = { .id = 1, .irq_pending = 0 };
    struct device dev2 = { .id = 2, .irq_pending = 1 };  /* This one! */
    struct device dev3 = { .id = 3, .irq_pending = 0 };

    printf("=== SHARED IRQ SIMULATION ===\n\n");

    /* Register handlers */
    request_irq(device1_handler, &dev1, "device1");
    request_irq(device2_handler, &dev2, "device2");
    request_irq(device3_handler, &dev3, "device3");

    /* Simulate interrupt */
    printf("\n--- Interrupt occurs ---\n");
    handle_irq(5);

    return 0;
}

/*
 * Output:
 *
 * === SHARED IRQ SIMULATION ===
 *
 * [IRQ] Registered handler: device1
 * [IRQ] Registered handler: device2
 * [IRQ] Registered handler: device3
 *
 * --- Interrupt occurs ---
 * [IRQ] Interrupt 5 fired
 *   [IRQ] Trying handler: device3
 *   [IRQ] device3: not my interrupt
 *   [IRQ] Trying handler: device2
 *     [DEV2] Processing interrupt
 *   [IRQ] device2 handled the interrupt
 */
```

---

## What Core Does NOT Control

```
    IRQ Core Controls:
    ------------------
    [X] Chain traversal
    [X] Calling handlers
    [X] Stopping on IRQ_HANDLED

    Handlers Control:
    -----------------
    [X] How to check if interrupt is theirs
    [X] How to process interrupt
    [X] What to return
```

---

## Version

Based on **Linux kernel v3.2** kernel/irq/.
