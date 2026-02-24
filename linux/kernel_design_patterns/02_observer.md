# Module 2: Observer / Publish–Subscribe

> **Pattern**: Observer (GoF)
> **Kernel idioms**: Notifier chains, event callback lists, registration/notification loops

---

## Mental Model

One component emits "events"; others register to be notified without the
emitter knowing who they are. Decouples event source from listeners.

In C: lists of (callback + data), and a "notify" loop that walks the list
calling each callback in turn.

```
  EVENT SOURCE (Subject)
  "something happened"
       │
       │  walks the subscriber list
       ▼
  ┌───────────┐    ┌───────────┐    ┌───────────┐
  │ Observer A│──→ │ Observer B│──→ │ Observer C│
  │ callback()│    │ callback()│    │ callback()│
  └───────────┘    └───────────┘    └───────────┘
       │                │                │
       ▼                ▼                ▼
  bonding reacts   bridge reacts   your module reacts
```

**GoF mapping:**
- **Subject** → the notifier chain head (+ the code that calls `*_notifier_call_chain`)
- **Observer** → each `struct notifier_block` registration
- **notify()** → `blocking_notifier_call_chain()` / `atomic_notifier_call_chain()`
- **attach/detach** → `register_*_notifier()` / `unregister_*_notifier()`

---

## In the Kernel (v3.2)

### The Infrastructure

`include/linux/notifier.h`, lines 50–74:

```c
struct notifier_block {
    int (*notifier_call)(struct notifier_block *, unsigned long, void *);
    struct notifier_block __rcu *next;
    int priority;
};

struct atomic_notifier_head {
    spinlock_t lock;
    struct notifier_block __rcu *head;
};

struct blocking_notifier_head {
    struct rw_semaphore rwsem;
    struct notifier_block __rcu *head;
};

struct raw_notifier_head {
    struct notifier_block __rcu *head;
};

struct srcu_notifier_head {
    struct mutex mutex;
    struct srcu_struct srcu;
    struct notifier_block __rcu *head;
};
```

Each `notifier_block` is one **observer**: it holds a callback and a link
to the next observer. The chain head is the **subject** — it holds the
list and the lock that protects it.

### The Four Chain Variants

The kernel provides four variants because observers run in different
concurrency contexts:

```
                       ┌─────────────────────────────────────────────┐
                       │       Notifier Chain Variants (v3.2)        │
                       ├─────────────────┬──────────┬────────────────┤
                       │ Type            │ Lock     │ Context        │
                       ├─────────────────┼──────────┼────────────────┤
                       │ atomic_notifier │ spinlock │ interrupt/atom │
                       │ blocking_notif. │ rwsem    │ process (sleep)│
                       │ raw_notifier    │ none     │ caller manages │
                       │ srcu_notifier   │ SRCU     │ high-freq read │
                       └─────────────────┴──────────┴────────────────┘
```

**Why four?** Kernel code runs in many contexts. An observer in interrupt
context cannot sleep, so `atomic_notifier_head` uses a spinlock. An
observer that needs to allocate memory or do I/O needs `blocking_notifier`.
High-frequency paths (many readers, rare registration changes) use SRCU
for near-zero read overhead.

### Example: Network Device Events

**Subscribing (attach):**

Any subsystem that cares about network events registers a `notifier_block`:

```c
/* Example: bonding driver subscribes to netdev events */
static struct notifier_block bond_netdev_notifier = {
    .notifier_call = bond_netdev_event,
};

/* In module init: */
register_netdevice_notifier(&bond_netdev_notifier);
```

**Publishing (notify):**

When a network interface goes up, the networking core broadcasts:

```c
/* net/core/dev.c */
int call_netdevice_notifiers(unsigned long val, struct net_device *dev)
{
    return raw_notifier_call_chain(&netdev_chain, val, dev);
}
```

This walks the chain and calls every registered callback:

```
  call_netdevice_notifiers(NETDEV_UP, eth0)
             │
             │  raw_notifier_call_chain(&netdev_chain, ...)
             │
             ▼
  ┌─────────────────────┐
  │ bond_netdev_event() │  → bonding: check if this device is a slave
  └──────────┬──────────┘
             │ next
             ▼
  ┌─────────────────────┐
  │ bridge_event()      │  → bridging: update forwarding tables
  └──────────┬──────────┘
             │ next
             ▼
  ┌─────────────────────┐
  │ vlan_event()        │  → VLAN: propagate state to VLAN devices
  └──────────┬──────────┘
             │ next
             ▼
            NULL (end of chain)
```

**Unsubscribing (detach):**

```c
/* In module exit: */
unregister_netdevice_notifier(&bond_netdev_notifier);
```

### Example: Reboot Notification

`include/linux/notifier.h`, line 209:

```c
extern struct blocking_notifier_head reboot_notifier_list;
```

Subsystems that need to do cleanup before reboot register here. When the
kernel reboots, it calls `blocking_notifier_call_chain(&reboot_notifier_list, ...)`,
giving every registered observer a chance to flush caches, park disk heads,
save state, etc.

### The Callback Signature

Every observer callback has the same signature:

```c
int (*notifier_call)(struct notifier_block *self,
                     unsigned long event,
                     void *data);
```

- `self` → the observer's own `notifier_block` (use `container_of` to get
  your private struct)
- `event` → an `unsigned long` event code (e.g. `NETDEV_UP`, `NETDEV_DOWN`)
- `data` → event-specific payload (e.g. `struct net_device *`)

Return values control chain traversal:

```
  NOTIFY_DONE   (0x0000)  → "I don't care, continue"
  NOTIFY_OK     (0x0001)  → "Handled, continue"
  NOTIFY_BAD    (0x8002)  → "Veto! Stop the chain"
  NOTIFY_STOP   (0x8001)  → "OK, but stop the chain"
```

### The Notification Loop (Internals)

`kernel/notifier.c` — simplified:

```c
static int
__notifier_call_chain(struct notifier_block **nl,
                      unsigned long val, void *v,
                      int nr_to_call, int *nr_calls)
{
    int ret = NOTIFY_DONE;
    struct notifier_block *nb, *next_nb;

    nb = rcu_dereference_raw(*nl);

    while (nb && nr_to_call) {
        next_nb = rcu_dereference_raw(nb->next);

        ret = nb->notifier_call(nb, val, v);

        if (nr_calls)
            (*nr_calls)++;

        if ((ret & NOTIFY_STOP_MASK))
            break;

        nb = next_nb;
        nr_to_call--;
    }
    return ret;
}
```

This is the classic Observer dispatch loop: walk the list, call each
observer, check for stop conditions.

### Real Code Path Walkthrough: `dev_open()` → NETDEV_UP Notification

Trace what happens when a network interface is brought up (`ip link set eth0 up`).
The networking core publishes a NETDEV_UP event and every registered observer reacts.

```
  USERSPACE
  ─────────
  ip link set eth0 up
       │
       │  ioctl / netlink → eventually calls:
       ▼
  net/core/dev.c:1197 — dev_open(dev)
  ┌──────────────────────────────────────────────────────────────┐
  │  if (dev->flags & IFF_UP)                                    │
  │      return 0;             /* already up, nothing to do */   │
  │                                                              │
  │  ret = __dev_open(dev);    /* enable the hardware:           │
  │       │                     *   dev->netdev_ops->ndo_open()  │
  │       │                     *   set IFF_UP flag              │
  │       │                     */                               │
  │       ▼                                                      │
  │  rtmsg_ifinfo(RTM_NEWLINK, dev, IFF_UP|IFF_RUNNING);         │
  │                            /* notify netlink listeners */    │
  │                                                              │
  │  call_netdevice_notifiers(NETDEV_UP, dev);   ◄─────────────  │ EVENT PUBLISHED
  └──────────────────────────────────────────────┬───────────────┘
                                                 │
                                                 ▼
  net/core/dev.c:1361 — call_netdevice_notifiers(NETDEV_UP, dev)
  ┌──────────────────────────────────────────────────────────────┐
  │  return raw_notifier_call_chain(&netdev_chain, val, dev);    │
  └──────────────────────────────────────────────┬───────────────┘
                                                 │
                                                 ▼
  kernel/notifier.c — __notifier_call_chain(&netdev_chain.head, ...)
  ┌──────────────────────────────────────────────────────────────┐
  │  nb = rcu_dereference_raw(*nl);    /* first observer */      │
  │                                                              │
  │  while (nb && nr_to_call) {                                  │
  │      next_nb = nb->next;                                     │
  │                                                              │
  │      ret = nb->notifier_call(nb, NETDEV_UP, dev); ◄────────  │ CALL OBSERVER
  │            │                                                 │
  │            ├── Observer 1: bond_netdev_event()               │
  │            │   "Is eth0 one of my slaves? If so, update      │
  │            │    bonding state."                              │
  │            │   returns NOTIFY_DONE                           │
  │            │                                                 │
  │            ├── Observer 2: br_device_event()                 │
  │            │   "Is eth0 a bridge port? If so, set port       │
  │            │    to forwarding state."                        │
  │            │   returns NOTIFY_DONE                           │
  │            │                                                 │
  │            └── Observer 3: vlan_device_event()               │
  │                "Does eth0 have VLAN sub-devices? Propagate   │
  │                 carrier state to them."                      │
  │                returns NOTIFY_DONE                           │
  │                                                              │
  │      if (ret & NOTIFY_STOP_MASK) break;                      │
  │      nb = next_nb;                                           │
  │  }                                                           │
  └──────────────────────────────────────────────────────────────┘
```

**Key observation:** `dev_open()` has no idea that bonding, bridging, or VLAN
subsystems exist. It publishes a single event (`NETDEV_UP`). Each subsystem
independently registered its `notifier_block` at module init time. The event
source and the reactors are fully decoupled — that's the Observer pattern.

---

## Why Observer Here

Many subsystems need to react to global events ("device going away,"
"low memory," "network interface up") without the event source depending
on every reactor.

**What would break without it:**

```
/* WITHOUT Observer — hard-coded dependencies: */
void netdev_up(struct net_device *dev)
{
    bonding_handle_up(dev);     /* bonding must exist */
    bridge_handle_up(dev);      /* bridge must exist */
    vlan_handle_up(dev);        /* vlan must exist */
    /* every new subsystem = edit this function */
}
```

With Observer, adding a new reactor is just another `register_netdevice_notifier`
call. The networking core never changes.

---

## Where Else the Pattern Appears

| Subject | Event Chain | Typical Observers |
|---------|------------|-------------------|
| Network devices | `netdev_chain` | bonding, bridge, VLAN, team |
| Reboot | `reboot_notifier_list` | watchdog, ACPI, storage drivers |
| CPU hotplug | `cpu_chain` | scheduler, RCU, slab allocator |
| PM suspend/resume | `pm_chain_head` | device drivers, filesystems |
| Inotify/fsnotify | inode watches | userspace notification |
| Keyboard events | `keyboard_notifier_list` | accessibility, console |

---

## Pitfalls

1. **Callback doing too much.** Atomic notifier callbacks cannot sleep.
   Even blocking callbacks should be brief — they run in a chain and
   delay every subsequent observer.

2. **Order dependencies.** Observers have a `priority` field, but relying
   on order is fragile. Design so each observer is independent.

3. **Unregistering from inside a callback.** The chain is being traversed;
   modifying it mid-walk can corrupt the list. The kernel header explicitly
   warns: "must not be called from within the call chain."

4. **Forgetting to unregister.** If a module unloads without unregistering
   its notifier, the chain holds a dangling pointer. Use module exit
   functions symmetrically.

---

## Check Your Understanding

1. Find one `notifier_block` registration and the code path that invokes
   that chain. Who is the "subject" and who are the "observers"?

2. What would break if the kernel had no notifier chain and each event
   source called specific subsystems by name?

3. Why does the kernel need four different notifier chain types instead
   of one? Give a concrete scenario for each.

4. Map to GoF: "The **Subject** is ___; the **Observer** is ___;
   `notify()` is ___; `attach()` is ___."

---

Proceed to [Module 3: Template Method](03_template_method.md).
