# Module 6: Synthesis — Reading New Kernel Code Through an OOP Lens

> **Core question**: You've now learned six patterns. Let's put them all
> together. Open `drivers/net/ethernet/intel/e1000e/netdev.c` and identify
> EVERY OOP pattern we've discussed.

---

## 6.1 The Analysis Framework

When encountering unfamiliar kernel code, ask these eight questions in order:

```
┌─────────────────────────────────────────────────────────────┐
│           EIGHT QUESTIONS FOR KERNEL CODE ANALYSIS          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. What are the OBJECTS?                                   │
│     → Find the primary structs                              │
│                                                             │
│  2. What are the INTERFACES?                                │
│     → Find the *_operations / *_ops structs                 │
│                                                             │
│  3. What is the INHERITANCE hierarchy?                      │
│     → Look for embedded structs and container_of usage      │
│                                                             │
│  4. Where is the VTABLE assignment?                         │
│     → Find where ops structs are filled in (the "class      │
│       definition" moment)                                   │
│                                                             │
│  5. Where is the CONSTRUCTOR?                               │
│     → Find *_alloc(), *_init(), *_create(), or probe()      │
│                                                             │
│  6. Where is the DESTRUCTOR?                                │
│     → Find *_free(), *_destroy(), *_release(), or remove()  │
│                                                             │
│  7. How is LIFETIME managed?                                │
│     → Look for kref, atomic_t refcounts, or kobject         │
│       embedding                                             │
│                                                             │
│  8. What PATTERNS are at play?                              │
│     → Observer? Iterator? Strategy? Factory? Template       │
│       Method?                                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

For the one-sentence mental model, "what would break without it," and pitfalls
for each pattern, see [Mental models and v3.2 anchors](00b_mental_models_and_anchors.md).
Use the **Quick "How & Why" checklist** there when analyzing any example (including
the e1000e below).

---

## 6.2 Case Study: e1000e Network Driver

Let's apply the framework to Intel's e1000e Ethernet driver, a
real-world driver of moderate complexity (~6400 lines).

### Question 1: What Are the Objects?

The primary structs in the e1000e driver:

| Struct                | Role                                         |
|-----------------------|----------------------------------------------|
| `struct e1000_adapter`| The main driver state — per-device instance  |
| `struct net_device`   | The generic network device (VFS-level object)|
| `struct pci_dev`      | The PCI bus device (hardware identity)       |
| `struct napi_struct`  | NAPI polling context (interrupt coalescing)  |
| `struct e1000_ring`   | TX/RX descriptor ring                        |

### Question 2: What Are the Interfaces?

The driver implements three vtables:

**a) `struct net_device_ops`** — network operations vtable

`drivers/net/ethernet/intel/e1000e/netdev.c`, lines 5882–5897:

```c
static const struct net_device_ops e1000e_netdev_ops = {
    .ndo_open           = e1000_open,
    .ndo_stop           = e1000_close,
    .ndo_start_xmit     = e1000_xmit_frame,
    .ndo_get_stats64    = e1000e_get_stats64,
    .ndo_set_rx_mode    = e1000_set_multi,
    .ndo_set_mac_address = e1000_set_mac,
    .ndo_change_mtu     = e1000_change_mtu,
    .ndo_do_ioctl       = e1000_ioctl,
    .ndo_tx_timeout     = e1000_tx_timeout,
    .ndo_validate_addr  = eth_validate_addr,
    .ndo_vlan_rx_add_vid = e1000_vlan_rx_add_vid,
    .ndo_vlan_rx_kill_vid = e1000_vlan_rx_kill_vid,
    /* ... */
};
```

**b) `struct ethtool_ops`** — configuration/diagnostics vtable
(defined in `ethtool.c`)

**c) `struct pci_driver`** — PCI bus integration vtable

`drivers/net/ethernet/intel/e1000e/netdev.c`, lines 6392–6402:

```c
static struct pci_driver e1000_driver = {
    .name       = e1000e_driver_name,
    .id_table   = e1000_pci_tbl,
    .probe      = e1000_probe,
    .remove     = __devexit_p(e1000_remove),
    .driver.pm  = &e1000_pm_ops,
    .shutdown   = e1000_shutdown,
    .err_handler = &e1000_err_handler,
};
```

**Pattern**: Polymorphism (Module 3) — each of these is a vtable. The
network stack, ethtool, and PCI subsystem dispatch through these function
pointers without knowing the specific driver.

### Question 3: What Is the Inheritance Hierarchy?

```
struct pci_dev
    └── contains: struct device dev         (Module 2: embedding)
                    └── registered in sysfs as /sys/devices/pci*/...

struct net_device
    └── contains: struct device dev         (Module 2: embedding)
                    └── registered in sysfs as /sys/class/net/eth0

struct e1000_adapter
    └── contains: struct napi_struct napi   (Module 2: embedding)
```

The driver uses `container_of` extensively:

```c
/* netdev.c, line 2486 */
struct e1000_adapter *adapter =
    container_of(napi, struct e1000_adapter, napi);
```

Given a `struct napi_struct *` (which the NAPI framework passes), the
driver recovers its `e1000_adapter *` — the full driver state.

**Pattern**: Inheritance via embedding + downcast (Module 2).

### Question 4: Where Is the vtable Assignment?

Inside `e1000_probe()` (the constructor), the driver installs its vtables:

```c
/* In e1000_probe(), approximately line 5990 */
netdev->netdev_ops = &e1000e_netdev_ops;
e1000e_set_ethtool_ops(netdev);
```

This is the moment the "class is defined" — the generic `net_device` is
told which concrete operations to use.

### Question 5: Where Is the Constructor?

`e1000_probe()` at line 5913 is the constructor. It:

1. Allocates a `net_device` (which includes space for the adapter)
2. Initializes PCI resources (BAR mappings, DMA)
3. Sets up hardware (MAC address, PHY)
4. Installs the vtables
5. Registers the device with the network stack

```c
static int __devinit
e1000_probe(struct pci_dev *pdev,
            const struct pci_device_id *ent)
```

**Pattern**: Factory (Module 5) — `alloc_etherdev_mq()` allocates the
correctly-sized `net_device` with private data appended.

### Question 6: Where Is the Destructor?

`e1000_remove()` at line 6248 is the destructor. It:

1. Unregisters from the network stack
2. Releases hardware resources
3. Frees the `net_device`

```c
static void __devexit
e1000_remove(struct pci_dev *pdev)
```

### Question 7: How Is Lifetime Managed?

- `struct net_device` has an internal refcount managed by `dev_hold()`/
  `dev_put()` — similar to `kref`
- `struct pci_dev` inherits `struct device` which embeds a `struct kobject`
  — full reference counting via `kobject_get()`/`kobject_put()`
- The driver's adapter struct is allocated as "private data" appended to
  `net_device` — its lifetime is tied to the `net_device`

**Pattern**: Reference counting (Module 4) via the kobject system.

### Question 8: What Patterns Are at Play?

| Pattern         | Instance in e1000e                                  |
|-----------------|-----------------------------------------------------|
| Encapsulation   | `static` functions throughout `netdev.c`            |
| Inheritance     | `struct net_device` embeds `struct device`           |
| Polymorphism    | `net_device_ops`, `ethtool_ops`, `pci_driver`       |
| kobject System  | `struct device` in both `pci_dev` and `net_device`  |
| Factory         | `alloc_etherdev_mq()` allocates typed device        |
| Template Method | Network stack calls `ndo_open`, `ndo_start_xmit`    |
| Observer        | NAPI polling, watchdog timer via `work_struct`       |
| Strategy        | `struct e1000_info` selects hardware-specific behavior|

---

## 6.3 The Architecture at a Glance

```
                    PCI Subsystem
                         │
                    .probe = e1000_probe    (constructor)
                    .remove = e1000_remove  (destructor)
                         │
                         ▼
    ┌──────────────────────────────────────────────┐
    │           struct pci_dev                     │
    │  ┌──────────────────────────────────────┐    │
    │  │  struct device dev                   │    │  ← kobject system
    │  │    struct kobject kobj               │    │    (Module 4)
    │  └──────────────────────────────────────┘    │
    └──────────────────────────────────────────────┘
                         │
                         │ pci_get_drvdata(pdev)
                         ▼
    ┌──────────────────────────────────────────────┐
    │           struct net_device                  │
    │                                              │
    │  .netdev_ops → e1000e_netdev_ops  ◄──────────┼── vtable (Module 3)
    │  .ethtool_ops → e1000_ethtool_ops            │
    │  ┌──────────────────────────────────────┐    │
    │  │  struct device dev                   │    │  ← inheritance
    │  │    struct kobject kobj               │    │    (Module 2)
    │  └──────────────────────────────────────┘    │
    │                                              │
    │  [private data area] ────────────────────────┼── encapsulation
    │    struct e1000_adapter {                    │    (Module 1)
    │      struct napi_struct napi;                │
    │      struct e1000_ring *tx_ring;             │
    │      struct e1000_ring *rx_ring;             │
    │      /* ... hardware state ... */            │
    │    }                                         │
    └──────────────────────────────────────────────┘
                         │
                Network Stack dispatches via
                netdev->netdev_ops->ndo_start_xmit()
                         │
                         ▼
              e1000_xmit_frame()
              (the concrete implementation)
```

---

## 6.4 Applying the Framework to Other Subsystems

### USB Core (`drivers/usb/core/`)

| Question | Answer |
|----------|--------|
| Objects | `struct usb_device`, `struct usb_interface`, `struct usb_driver` |
| Interfaces | `struct usb_driver` (probe/disconnect), `struct file_operations` (usbfs) |
| Inheritance | `usb_device` embeds `struct device`, `usb_interface` embeds `struct device` |
| Constructor | `usb_alloc_dev()`, driver's `.probe` |
| Destructor | `usb_disconnect()`, driver's `.disconnect` |
| Lifetime | `kobject` refcounting via `struct device` |
| Patterns | Factory (device allocation), Observer (hotplug notifiers), Strategy (driver matching) |

### Input Subsystem (`drivers/input/`)

| Question | Answer |
|----------|--------|
| Objects | `struct input_dev`, `struct input_handler`, `struct input_handle` |
| Interfaces | `struct input_handler` (connect/disconnect/event) |
| Inheritance | `input_dev` embeds `struct device` |
| Constructor | `input_allocate_device()`, handler's `.connect` |
| Destructor | `input_free_device()`, handler's `.disconnect` |
| Lifetime | `kobject` + `input_dev.users` refcount |
| Patterns | Observer (event dispatch to handlers), Strategy (different handlers for same device), Factory (device allocation) |

### Proc Filesystem (`fs/proc/`)

| Question | Answer |
|----------|--------|
| Objects | `struct proc_dir_entry`, `struct seq_file` |
| Interfaces | `struct file_operations`, `struct seq_operations` |
| Inheritance | Minimal — proc entries embed simple data |
| Constructor | `proc_create()`, `seq_open()` |
| Destructor | `remove_proc_entry()`, `seq_release()` |
| Lifetime | Module reference counting |
| Patterns | Iterator (`seq_file`), Template Method (seq framework calls `start/next/stop/show`) |

---

## 6.5 Final Exercise

Choose a kernel subsystem you've never read before. Suggestions:

- `drivers/input/` — input event handling
- `drivers/usb/core/` — USB device model
- `net/core/` — networking core
- `fs/proc/` — the proc filesystem

Write a one-page "OOP Architecture Summary" that identifies:

1. **The main "classes"** (structs) — what are the primary data structures?
2. **Their inheritance relationships** (struct embedding) — which structs
   embed others? Draw the hierarchy.
3. **Their interfaces** (ops structs) — what vtables exist? Who fills
   them in?
4. **The lifecycle** (constructor → use → destructor) — how are objects
   created, used, and destroyed?
5. **At least two GoF patterns in use** — Observer? Iterator? Strategy?
   Factory? Template Method? Proxy?

### Template for Your Summary

```markdown
# OOP Architecture Summary: [subsystem name]

## Primary Classes (structs)
- `struct X` — role: ...
- `struct Y` — role: ...

## Inheritance Hierarchy
    struct kobject
        └── struct device
              └── struct X

## Interfaces (vtables)
- `struct X_ops` — attached to `struct X`, methods: ...
- `struct Y_ops` — attached to `struct Y`, methods: ...

## Lifecycle
- Constructor: `X_alloc()` or `X_probe()`
- Usage: network stack / VFS / subsystem calls through ops
- Destructor: `X_free()` or `X_remove()`
- Lifetime: kobject refcounting / kref

## Design Patterns
1. [Pattern]: [where and how it's used]
2. [Pattern]: [where and how it's used]
```

---

## 6.6 What You've Learned

Across six modules, you've built a vocabulary for reading kernel C as
object-oriented code:

| Module | Key Insight |
|--------|-------------|
| 1. Encapsulation | `void *`, `static`, header splits — hiding internals |
| 2. Inheritance | Struct embedding + `container_of` — IS-A via HAS-A |
| 3. Polymorphism | `*_operations` structs — vtables for dispatch |
| 4. kobject | Formal object system — refcounting, type info, sysfs |
| 5. Design Patterns | Observer, Iterator, Strategy, Factory, Template Method |
| 6. Synthesis | The eight-question framework for any new code |

For a compact recap of each pattern (what it is, why, when, pitfalls) and the
how/why checklist, see [Mental models and v3.2 anchors](00b_mental_models_and_anchors.md).

These patterns are not accidental. They emerged from decades of engineering
pressure: the need to support thousands of hardware drivers, dozens of
filesystems, and multiple CPU architectures — all within a single,
monolithic C codebase. The result is an object-oriented architecture
that is more disciplined and more performant than many "real" OOP
codebases.

The next time you open an unfamiliar kernel file, you'll see classes,
inheritance, vtables, and design patterns — even though there isn't a
single `class` keyword in sight.

---

## Where to Go Next

- **Deep dive**: Pick any vtable in `include/linux/` and trace every
  implementation across the tree. How many "classes" implement
  `struct file_operations`? (Answer: thousands.)

- **Write your own**: Create a kernel module that defines a kobject
  with a custom `kobj_type`, registers it in sysfs, and implements
  reference counting correctly.

- **Cross-reference**: Compare the kernel's patterns with other large C
  projects (GLib/GObject, SQLite, CPython). Which patterns are shared?
  Which are kernel-specific?

- **Version evolution**: Compare v3.2's patterns with a modern kernel
  (6.x). What has changed? What has remained stable for 15+ years?
