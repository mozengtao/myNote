# Unified Chain of Responsibility Skeleton

A generic C skeleton capturing the Chain of Responsibility pattern.

---

## Complete Skeleton

```c
/*
 * Generic Chain of Responsibility Skeleton
 * Based on Linux kernel chain patterns
 */

#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 * PART 1: RETURN VALUES
 * ================================================================ */

#define HANDLED     1   /* I handled it - stop chain */
#define NOT_HANDLED 0   /* Not mine - continue chain */

/* ================================================================
 * PART 2: HANDLER STRUCTURE
 * ================================================================ */

/* Handler function type */
typedef int (*handler_fn)(void *data);

/* Handler node */
struct handler {
    handler_fn fn;
    int priority;          /* Lower = higher priority */
    const char *name;
    struct handler *next;
};

/* ================================================================
 * PART 3: CHAIN MANAGEMENT
 * ================================================================ */

/* Chain head */
static struct handler *chain_head = NULL;

/* Register handler by priority */
int register_handler(struct handler *h)
{
    struct handler **p;

    printf("[CHAIN] Registering: %s (priority %d)\n",
           h->name, h->priority);

    /* Insert in priority order */
    for (p = &chain_head; *p; p = &(*p)->next) {
        if (h->priority < (*p)->priority)
            break;
    }
    h->next = *p;
    *p = h;

    return 0;
}

/* Unregister handler */
void unregister_handler(struct handler *h)
{
    struct handler **p;

    for (p = &chain_head; *p; p = &(*p)->next) {
        if (*p == h) {
            *p = h->next;
            printf("[CHAIN] Unregistered: %s\n", h->name);
            return;
        }
    }
}

/* ================================================================
 * PART 4: CHAIN PROCESSING
 * ================================================================ */

/*
 * Process chain - stop on first handler
 *
 * KEY DIFFERENCE FROM OBSERVER:
 * - Observer calls ALL handlers
 * - Chain stops on first HANDLED
 */
int process_chain(void *data)
{
    struct handler *h;
    int ret;

    printf("[CHAIN] Processing request\n");

    for (h = chain_head; h; h = h->next) {
        printf("  [CHAIN] Trying: %s\n", h->name);

        ret = h->fn(data);

        if (ret == HANDLED) {
            printf("  [CHAIN] Handled by: %s\n", h->name);
            return HANDLED;  /* Stop chain! */
        }
        /* NOT_HANDLED: continue to next */
    }

    printf("[CHAIN] No handler found\n");
    return NOT_HANDLED;
}

/* ================================================================
 * PART 5: EXAMPLE HANDLERS
 * ================================================================ */

struct request {
    int type;
    int value;
};

/* Handler for type 1 */
int type1_handler(void *data)
{
    struct request *req = data;

    if (req->type != 1)
        return NOT_HANDLED;

    printf("    [TYPE1] Handling request (value=%d)\n", req->value);
    return HANDLED;
}

/* Handler for type 2 */
int type2_handler(void *data)
{
    struct request *req = data;

    if (req->type != 2)
        return NOT_HANDLED;

    printf("    [TYPE2] Handling request (value=%d)\n", req->value);
    return HANDLED;
}

/* Default handler (catches all) */
int default_handler(void *data)
{
    struct request *req = data;

    printf("    [DEFAULT] Handling unknown type %d\n", req->type);
    return HANDLED;
}

/* Handler instances */
static struct handler h1 = {
    .fn = type1_handler,
    .priority = 100,
    .name = "type1"
};

static struct handler h2 = {
    .fn = type2_handler,
    .priority = 100,
    .name = "type2"
};

static struct handler h_default = {
    .fn = default_handler,
    .priority = 999,  /* Low priority - last resort */
    .name = "default"
};

/* ================================================================
 * PART 6: USAGE
 * ================================================================ */

int main(void)
{
    struct request req1 = { .type = 1, .value = 100 };
    struct request req2 = { .type = 2, .value = 200 };
    struct request req3 = { .type = 3, .value = 300 };

    printf("=== CHAIN OF RESPONSIBILITY SKELETON ===\n\n");

    /* Register handlers */
    register_handler(&h1);
    register_handler(&h2);
    register_handler(&h_default);

    /* Process requests */
    printf("\n--- Request type 1 ---\n");
    process_chain(&req1);

    printf("\n--- Request type 2 ---\n");
    process_chain(&req2);

    printf("\n--- Request type 3 (unknown) ---\n");
    process_chain(&req3);

    return 0;
}
```

---

## Mapping to Kernel

```
    SKELETON            NETFILTER           IRQ
    ========            =========           ===

    handler             nf_hook_ops         irqaction
    register_handler    nf_register_hook    request_irq
    process_chain       nf_hook_slow        handle_IRQ_event
    HANDLED             NF_DROP/ACCEPT      IRQ_HANDLED
    NOT_HANDLED         (continue)          IRQ_NONE
```

---

## Key Implementation Points

```
    1. RETURN VALUE PROTOCOL
       - HANDLED = stop chain
       - NOT_HANDLED = continue

    2. EARLY EXIT
       - Loop breaks on HANDLED
       - Remaining handlers not called

    3. PRIORITY ORDERING
       - Important handlers first
       - Default handler last

    4. HANDLER RESPONSIBILITY
       - Each handler checks if it should handle
       - Returns appropriate value
```

---

## Version

Based on **Linux kernel v3.2** chain patterns.
