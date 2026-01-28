# Identification Rules for Observer/Notifier Pattern

## Structural Signals

```
+=============================================================================+
|                    NOTIFIER PATTERN ANATOMY                                  |
+=============================================================================+

    CHAIN DECLARATION:
    ==================

    static BLOCKING_NOTIFIER_HEAD(my_chain);
    /* OR */
    static ATOMIC_NOTIFIER_HEAD(my_chain);
    /* OR */
    static RAW_NOTIFIER_HEAD(my_chain);


    NOTIFIER BLOCK:
    ===============

    static struct notifier_block my_nb = {
        .notifier_call = my_callback,
        .priority = 0,
    };


    CALLBACK SIGNATURE:
    ===================

    int callback(struct notifier_block *nb,
                 unsigned long event,
                 void *data);


    OPERATIONS:
    ===========

    xxx_notifier_chain_register(&chain, &nb);
    xxx_notifier_chain_unregister(&chain, &nb);
    xxx_notifier_call_chain(&chain, event, data);
```

---

## The Five Identification Rules

### Rule 1: Look for NOTIFIER_HEAD Declaration

```c
/* Notifier chain declarations */
static BLOCKING_NOTIFIER_HEAD(reboot_notifier_list);
static ATOMIC_NOTIFIER_HEAD(panic_notifier_list);
static RAW_NOTIFIER_HEAD(netdev_chain);

/* If you see xxx_NOTIFIER_HEAD, it's notifier pattern */
```

### Rule 2: Look for struct notifier_block

```c
/* Notifier block structure */
static struct notifier_block my_notifier = {
    .notifier_call = my_handler,
    .priority = 0,
};

/* Kernel subsystems define notifier blocks to subscribe to events */
```

### Rule 3: Look for register/unregister Calls

```c
/* Registration functions */
register_reboot_notifier(&my_nb);
unregister_reboot_notifier(&my_nb);

/* OR generic form */
blocking_notifier_chain_register(&chain, &nb);
blocking_notifier_chain_unregister(&chain, &nb);

/* Registration/unregistration pairs indicate notifier pattern */
```

### Rule 4: Look for call_chain Invocations

```c
/* Notification calls */
blocking_notifier_call_chain(&reboot_list, SYS_RESTART, NULL);
atomic_notifier_call_chain(&panic_list, 0, NULL);
call_netdev_notifiers(NETDEV_UP, dev);

/* call_chain calls broadcast events to all subscribers */
```

### Rule 5: Check Callback Signature

```c
/* Standard notifier callback */
int callback(struct notifier_block *nb,
             unsigned long action,
             void *data)
{
    switch (action) {
    case SOME_EVENT:
        /* Handle */
        return NOTIFY_OK;
    default:
        return NOTIFY_DONE;
    }
}

/* Three parameters: nb, event, data */
```

---

## Summary Checklist

```
+=============================================================================+
|                    NOTIFIER IDENTIFICATION CHECKLIST                         |
+=============================================================================+

    [ ] 1. NOTIFIER_HEAD
        BLOCKING_NOTIFIER_HEAD, ATOMIC_NOTIFIER_HEAD, etc.

    [ ] 2. NOTIFIER_BLOCK
        struct notifier_block with .notifier_call

    [ ] 3. REGISTER/UNREGISTER
        xxx_register(), xxx_unregister() pairs

    [ ] 4. CALL_CHAIN
        xxx_notifier_call_chain() or wrapper

    [ ] 5. CALLBACK RETURNS
        NOTIFY_OK, NOTIFY_DONE, NOTIFY_STOP, etc.

    SCORING:
    3+ indicators = Definitely notifier pattern
    2 indicators  = Likely notifier pattern
```

---

## Common Kernel Notifier Chains

| Chain | Purpose | Type |
|-------|---------|------|
| `netdev_chain` | Network device events | RAW |
| `reboot_notifier_list` | System reboot | BLOCKING |
| `panic_notifier_list` | Kernel panic | ATOMIC |
| `pm_chain_head` | Power management | BLOCKING |
| `inetaddr_chain` | IPv4 address changes | BLOCKING |
| `inet6addr_chain` | IPv6 address changes | ATOMIC |
