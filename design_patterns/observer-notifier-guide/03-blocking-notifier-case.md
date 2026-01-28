# Case 1: Blocking Notifier Chains

## The Most Common Type

```
+=============================================================================+
|                    BLOCKING NOTIFIER CHAIN                                   |
+=============================================================================+

    PROPERTIES:
    ===========

    - Callbacks CAN sleep
    - Protected by rwsem
    - Registration/unregistration can block
    - Most common notifier type


    STRUCTURE:
    ==========

    struct blocking_notifier_head {
        struct rw_semaphore rwsem;
        struct notifier_block __rcu *head;
    };


    OPERATIONS:
    ===========

    BLOCKING_NOTIFIER_HEAD(name)
    blocking_notifier_chain_register(nh, nb)
    blocking_notifier_chain_unregister(nh, nb)
    blocking_notifier_call_chain(nh, val, v)
```

---

## Minimal C Code Simulation

```c
/*
 * BLOCKING NOTIFIER CHAIN SIMULATION
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ==========================================================
 * NOTIFY RETURN VALUES
 * ========================================================== */

#define NOTIFY_DONE     0x0000
#define NOTIFY_OK       0x0001
#define NOTIFY_STOP_MASK 0x8000
#define NOTIFY_BAD      (NOTIFY_STOP_MASK|0x0002)
#define NOTIFY_STOP     (NOTIFY_STOP_MASK|NOTIFY_OK)

/* ==========================================================
 * NOTIFIER STRUCTURES
 * ========================================================== */

struct notifier_block;

typedef int (*notifier_fn_t)(struct notifier_block *nb,
                             unsigned long action, void *data);

struct notifier_block {
    notifier_fn_t notifier_call;
    struct notifier_block *next;
    int priority;
};

struct blocking_notifier_head {
    pthread_rwlock_t rwlock;
    struct notifier_block *head;
};

/* ==========================================================
 * NOTIFIER OPERATIONS
 * ========================================================== */

void blocking_notifier_head_init(struct blocking_notifier_head *nh)
{
    pthread_rwlock_init(&nh->rwlock, NULL);
    nh->head = NULL;
}

int blocking_notifier_chain_register(struct blocking_notifier_head *nh,
                                      struct notifier_block *nb)
{
    struct notifier_block **p;
    
    pthread_rwlock_wrlock(&nh->rwlock);
    
    /* Insert in priority order (highest first) */
    for (p = &nh->head; *p; p = &(*p)->next) {
        if (nb->priority > (*p)->priority)
            break;
    }
    nb->next = *p;
    *p = nb;
    
    pthread_rwlock_unlock(&nh->rwlock);
    
    printf("[REGISTER] Registered notifier (priority %d)\n", nb->priority);
    return 0;
}

int blocking_notifier_chain_unregister(struct blocking_notifier_head *nh,
                                        struct notifier_block *nb)
{
    struct notifier_block **p;
    
    pthread_rwlock_wrlock(&nh->rwlock);
    
    for (p = &nh->head; *p; p = &(*p)->next) {
        if (*p == nb) {
            *p = nb->next;
            pthread_rwlock_unlock(&nh->rwlock);
            printf("[UNREGISTER] Unregistered notifier\n");
            return 0;
        }
    }
    
    pthread_rwlock_unlock(&nh->rwlock);
    printf("[UNREGISTER] Notifier not found\n");
    return -1;
}

int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
                                  unsigned long val, void *v)
{
    struct notifier_block *nb;
    int ret = NOTIFY_DONE;
    
    pthread_rwlock_rdlock(&nh->rwlock);
    
    printf("[NOTIFY] Calling chain with event %lu\n", val);
    
    for (nb = nh->head; nb; nb = nb->next) {
        ret = nb->notifier_call(nb, val, v);
        printf("  [CALLBACK] Returned %d\n", ret);
        
        if (ret & NOTIFY_STOP_MASK) {
            printf("  [NOTIFY] Chain stopped\n");
            break;
        }
    }
    
    pthread_rwlock_unlock(&nh->rwlock);
    return ret;
}

/* ==========================================================
 * EXAMPLE: REBOOT NOTIFIER
 * ========================================================== */

/* Event types */
#define SYS_RESTART     0x0001
#define SYS_HALT        0x0002
#define SYS_POWER_OFF   0x0003

/* Global reboot notifier chain */
static struct blocking_notifier_head reboot_notifier_list;

/* Subscriber 1: Save data before reboot */
int save_data_notifier(struct notifier_block *nb,
                       unsigned long action, void *data)
{
    printf("  [SAVE_DATA] Saving data before action %lu\n", action);
    return NOTIFY_OK;
}

static struct notifier_block save_data_nb = {
    .notifier_call = save_data_notifier,
    .priority = 10,  /* High priority - run first */
};

/* Subscriber 2: Stop hardware */
int stop_hardware_notifier(struct notifier_block *nb,
                           unsigned long action, void *data)
{
    printf("  [STOP_HW] Stopping hardware for action %lu\n", action);
    return NOTIFY_OK;
}

static struct notifier_block stop_hw_nb = {
    .notifier_call = stop_hardware_notifier,
    .priority = 5,
};

/* Subscriber 3: Log event */
int log_notifier(struct notifier_block *nb,
                 unsigned long action, void *data)
{
    printf("  [LOG] Logging action %lu\n", action);
    return NOTIFY_DONE;
}

static struct notifier_block log_nb = {
    .notifier_call = log_notifier,
    .priority = 0,  /* Low priority - run last */
};

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */

int main(void)
{
    printf("=================================================\n");
    printf("BLOCKING NOTIFIER CHAIN DEMONSTRATION\n");
    printf("=================================================\n\n");
    
    /* Initialize chain */
    blocking_notifier_head_init(&reboot_notifier_list);
    
    /* Register subscribers */
    printf("--- Registering notifiers ---\n");
    blocking_notifier_chain_register(&reboot_notifier_list, &log_nb);
    blocking_notifier_chain_register(&reboot_notifier_list, &stop_hw_nb);
    blocking_notifier_chain_register(&reboot_notifier_list, &save_data_nb);
    
    /* Trigger reboot event */
    printf("\n--- Triggering SYS_RESTART ---\n");
    blocking_notifier_call_chain(&reboot_notifier_list, SYS_RESTART, NULL);
    
    /* Unregister one subscriber */
    printf("\n--- Unregistering save_data notifier ---\n");
    blocking_notifier_chain_unregister(&reboot_notifier_list, &save_data_nb);
    
    /* Trigger another event */
    printf("\n--- Triggering SYS_HALT ---\n");
    blocking_notifier_call_chain(&reboot_notifier_list, SYS_HALT, NULL);
    
    printf("\n=================================================\n");
    printf("KEY INSIGHTS:\n");
    printf("- Chain head holds list of notifier blocks\n");
    printf("- Notifiers called in priority order (high first)\n");
    printf("- Each callback receives event type and data\n");
    printf("- NOTIFY_STOP stops chain traversal\n");
    printf("- rwlock allows concurrent notifications\n");
    printf("=================================================\n");
    
    return 0;
}
```

---

## Real Kernel Usage

```c
/* kernel/sys.c - Reboot notifier */
static BLOCKING_NOTIFIER_HEAD(reboot_notifier_list);

int register_reboot_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_register(&reboot_notifier_list, nb);
}
EXPORT_SYMBOL(register_reboot_notifier);

/* kernel/reboot.c - Calling the chain */
void kernel_restart(char *cmd)
{
    blocking_notifier_call_chain(&reboot_notifier_list, SYS_RESTART, cmd);
    /* ... continue with restart ... */
}
```

---

## Key Takeaways

1. **Blocking = can sleep**: Callbacks can do blocking operations
2. **rwsem protection**: Safe concurrent access
3. **Priority ordering**: Higher priority called first
4. **Dynamic registration**: Subscribers add/remove at runtime
5. **Event broadcasting**: One call notifies all subscribers
