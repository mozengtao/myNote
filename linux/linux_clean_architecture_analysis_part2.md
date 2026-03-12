# Linux Kernel v3.2 — Clean Architecture Analysis (Part 2)

## 5. Device Driver Model

### 5.1 Subsystem Overview

The Linux driver model provides a unified framework for discovering, binding,
and managing hardware devices across all bus types. It separates the bus
infrastructure from individual device drivers.

**Key Files:**

| File                          | Purpose                                    |
|-------------------------------|--------------------------------------------|
| `drivers/base/core.c`         | Device core: `device_register()`           |
| `drivers/base/driver.c`       | Driver core: `driver_register()`           |
| `drivers/base/bus.c`          | Bus infrastructure: `bus_add_driver()`     |
| `drivers/base/platform.c`     | Platform bus                               |
| `include/linux/device.h`      | `device`, `device_driver`, `bus_type`, `class` |
| `include/linux/platform_device.h` | `platform_device`, `platform_driver`   |

### 5.2 Entities (Stable Layer)

#### struct device (`include/linux/device.h:406`)

The universal hardware entity:

```c
struct device {
    struct device *parent;
    struct kobject kobj;                  /* sysfs representation */
    const char *init_name;
    struct device_type *type;
    struct bus_type *bus;                  /* bus this device is on */
    struct device_driver *driver;         /* bound driver */
    void *platform_data;
    struct dev_pm_info power;
    struct class *class;
    void (*release)(struct device *dev);
};
```

#### struct device_driver (`include/linux/device.h:194`)

The universal driver entity:

```c
struct device_driver {
    const char *name;
    struct bus_type *bus;
    struct module *owner;

    int (*probe)(struct device *dev);      /* -> bind callback */
    int (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);

    const struct attribute_group **groups;
    const struct dev_pm_ops *pm;
};
```

### 5.3 Use Cases (Policy Logic)

#### driver_register() — Driver Binding Policy

`drivers/base/driver.c:222`:

```c
int driver_register(struct device_driver *drv)
{
    int ret;
    struct device_driver *other;

    other = driver_find(drv->name, drv->bus);
    if (other) {
        put_driver(other);
        return -EBUSY;
    }

    ret = bus_add_driver(drv);
    if (ret)
        return ret;
    ret = driver_add_groups(drv, drv->groups);
    if (ret)
        bus_remove_driver(drv);
    return ret;
}
```

`bus_add_driver()` (`drivers/base/bus.c:625`) performs:

1. Allocate `driver_private`, add kobject under the bus's driver kset
2. If autoprobe is enabled, call `driver_attach(drv)` which iterates all
   unbound devices on the bus, calling `bus->match()` and then `bus->probe()`
   or `drv->probe()` for each match.

### 5.4 Interface Adapters

#### struct bus_type (`include/linux/device.h:87`)

The key interface that separates the driver core from specific bus implementations:

```c
struct bus_type {
    const char *name;
    struct bus_attribute *bus_attrs;
    struct device_attribute *dev_attrs;
    struct driver_attribute *drv_attrs;

    int (*match)(struct device *dev, struct device_driver *drv);
    int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);

    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);

    const struct dev_pm_ops *pm;
};
```

The `match()` function is the critical dispatch point: the driver core calls
`bus->match(dev, drv)` to determine if a driver can handle a device. Each bus
type implements its own matching logic.

#### struct class (`include/linux/device.h:287`)

Groups devices by function (network, input, block, etc.) rather than
connection topology:

```c
struct class {
    const char *name;
    int (*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);
    char *(*devnode)(struct device *dev, mode_t *mode);
    void (*class_release)(struct class *cls);
    void (*dev_release)(struct device *dev);
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);
};
```

### 5.5 Outer Implementation Layer — Platform Bus

The platform bus is one of many bus_type implementations:

#### struct platform_driver (`include/linux/platform_device.h:164`)

```c
struct platform_driver {
    int (*probe)(struct platform_device *);
    int (*remove)(struct platform_device *);
    void (*shutdown)(struct platform_device *);
    int (*suspend)(struct platform_device *, pm_message_t);
    int (*resume)(struct platform_device *);
    struct device_driver driver;
    const struct platform_device_id *id_table;
};
```

#### platform_match() — Bus-Specific Matching

`drivers/base/platform.c:660`:

```c
static int platform_match(struct device *dev, struct device_driver *drv)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);

    /* Try device tree match first */
    if (of_driver_match_device(dev, drv))
        return 1;

    /* Then try id_table match */
    if (pdrv->id_table)
        return platform_match_id(pdrv->id_table, pdev) != NULL;

    /* Fall back to name match */
    return (strcmp(pdev->name, drv->name) == 0);
}
```

#### platform_bus_type registration

```c
struct bus_type platform_bus_type = {
    .name       = "platform",
    .dev_attrs  = platform_dev_attrs,
    .match      = platform_match,
    .uevent     = platform_uevent,
    .pm         = &platform_dev_pm_ops,
};
```

Other bus types (PCI, USB, I2C, SPI) follow the same pattern, each providing
their own `match()` and `probe()` implementations.

### 5.6 Execution Flow — Driver Binding

```
  Driver module loads
        |
        v
  platform_driver_register(&my_drv)      [include/linux/platform_device.h]
        |--- wraps: driver_register(&my_drv.driver)
        |
        v
  driver_register(drv)                   [drivers/base/driver.c:222]
        |--- bus_add_driver(drv)
        |       |
        |       v
        |   driver_attach(drv)           [drivers/base/dd.c]
        |       |--- bus_for_each_dev(drv->bus, ...)
        |               |
        |               v
        |           __driver_attach(dev, drv)
        |               |--- driver_match_device(drv, dev)
        |               |       |--- drv->bus->match(dev, drv)
        |               |               |--- platform_match(dev, drv)
        |               |
        |               v (if match succeeds)
        |           driver_probe_device(drv, dev)
        |               |--- drv->probe(dev)  or  bus->probe(dev)
        |               |--- dev->driver = drv
        |
        v
  Device bound to driver
```

### 5.7 Dependency Flow

```
  drivers/net/e1000/    ---->  include/linux/device.h     ---->  kobject
  drivers/base/platform.c -->  include/linux/device.h     ---->  kobject
  drivers/base/bus.c    ---->  include/linux/device.h     ---->  kobject

  A specific driver (e1000) includes linux/device.h and linux/pci.h.
  linux/pci.h defines struct pci_driver which embeds struct device_driver.
  linux/device.h defines struct bus_type, struct device, struct device_driver.
  linux/device.h has NO knowledge of e1000 or any specific driver.

  Direction:
  concrete driver -> bus-type headers -> device core headers -> kobject (entity)
```

### 5.8 Architecture Diagram

```
                  EXECUTION FLOW
                  ==============

       Driver Module Init
              |
              v
      +-----------------------+
      | driver_register()     |       USE CASE (Binding policy)
      | driver_attach()       |
      | drivers/base/driver.c |
      +-----------+-----------+
                  |
         via bus->match(dev, drv)
                  |
                  v
      +-----------------------+
      | struct bus_type       |       INTERFACE ADAPTER
      | (.match, .probe,      |
      |  .remove, .shutdown)  |
      +-----------+-----------+
                  |
      +-----------+-----------+-----------+
      |           |           |           |
      v           v           v           v
  +--------+ +--------+ +--------+ +--------+
  |platform| |  PCI   | |  USB   | |  I2C   |  OUTER (Bus impls)
  |  bus   | |  bus   | |  bus   | |  bus   |
  +---+----+ +--------+ +--------+ +--------+
      |
      v
  +-------------------+
  | struct platform_  |       INTERFACE (Bus-specific)
  |   driver          |
  | (.probe, .remove) |
  +---------+---------+
            |
            v
  +-------------------+
  | Concrete driver   |       OUTER (Device driver)
  | (my_sensor_drv)   |
  +-------------------+


                  DEPENDENCY FLOW
                  ===============

  Concrete driver -> platform_device.h -> device.h -> kobject
  platform bus    -> device.h -> kobject
  PCI bus         -> device.h -> kobject

  All dependencies point inward toward device/kobject entities.
```

### 5.9 Clean Architecture Insights

**Alignment:**

- `struct bus_type` is a dependency inversion masterpiece. The driver core
  knows how to match and bind using `bus->match()` and `bus->probe()` without
  any knowledge of PCI, USB, or platform specifics.
- The driver model is a **double plugin architecture**: both buses and drivers
  are plugins that register with the core framework.
- The kobject/sysfs integration provides a uniform external interface (the
  `/sys` filesystem) regardless of the underlying bus or device type.

**Divergence:**

- The `struct device_driver` itself contains `probe()` and `remove()` callbacks,
  creating a dual dispatch path: the core may call either `bus->probe()` or
  `drv->probe()`. This is a pragmatic but architecturally messy pattern.
- Bus-specific driver structs (e.g., `platform_driver`, `pci_driver`) embed
  `device_driver` using C struct embedding (C's form of inheritance). This
  works but is less explicit than Clean Architecture's composition-over-
  inheritance preference.

---

## 6. Security Framework (LSM)

### 6.1 Subsystem Overview

The Linux Security Module (LSM) framework provides hook points throughout the
kernel where security policy decisions can be intercepted. It allows different
security models (SELinux, AppArmor, SMACK) to be plugged in.

**Key Files:**

| File                         | Purpose                                  |
|------------------------------|------------------------------------------|
| `security/security.c`        | LSM core: dispatch and registration      |
| `security/selinux/hooks.c`   | SELinux implementation                   |
| `security/apparmor/`         | AppArmor implementation                  |
| `include/linux/security.h`   | `security_operations`, hook declarations |

### 6.2 Entities (Stable Layer)

The LSM framework doesn't define its own entities — instead, it operates on
entities from other subsystems:

- `struct task_struct` — process being checked
- `struct inode` — file being accessed
- `struct file` — open file being operated on
- `struct super_block` — filesystem being mounted
- `struct sk_buff` — network packet being processed
- `struct cred` — credential set

This is a cross-cutting concern that attaches security policy to existing entities.

### 6.3 Use Cases (Policy Logic)

The LSM core dispatches security checks without implementing any specific policy.
Each hook function in `security/security.c` follows the same pattern:

```c
/* security/security.c:736 */
int security_dentry_open(struct file *file, const struct cred *cred)
{
    int ret;
    ret = security_ops->dentry_open(file, cred);
    /* ... */
    return ret;
}
```

These functions are called from throughout the kernel at decision points:

- `vfs_read()` → `security_file_permission()`
- `do_sys_open()` → `security_dentry_open()`
- `do_mmap()` → `security_file_mmap()`
- `sys_socket()` → `security_socket_create()`

### 6.4 Interface Adapters

#### struct security_operations (`include/linux/security.h:1380`)

This is one of the largest interface structs in the kernel — over 150 function
pointers covering every security-relevant operation:

```c
struct security_operations {
    char name[SECURITY_NAME_MAX + 1];

    /* Process/task hooks */
    int (*ptrace_access_check)(struct task_struct *child, unsigned int mode);
    int (*ptrace_traceme)(struct task_struct *parent);
    int (*capable)(struct task_struct *tsk, const struct cred *cred,
                   struct user_namespace *ns, int cap, int audit);

    /* Binary execution hooks */
    int (*bprm_set_creds)(struct linux_binprm *bprm);
    int (*bprm_check_security)(struct linux_binprm *bprm);
    int (*bprm_secureexec)(struct linux_binprm *bprm);

    /* Filesystem hooks */
    int (*sb_alloc_security)(struct super_block *sb);
    void (*sb_free_security)(struct super_block *sb);
    int (*sb_mount)(char *dev_name, struct path *path, char *type,
                    unsigned long flags, void *data);

    /* Inode hooks */
    int (*inode_alloc_security)(struct inode *inode);
    int (*inode_permission)(struct inode *inode, int mask);
    int (*inode_create)(struct inode *dir, struct dentry *dentry, int mode);

    /* File hooks */
    int (*file_permission)(struct file *file, int mask);
    int (*file_alloc_security)(struct file *file);
    int (*dentry_open)(struct file *file, const struct cred *cred);

    /* Socket hooks */
    int (*socket_create)(int family, int type, int protocol, int kern);
    int (*socket_sendmsg)(struct socket *sock, struct msghdr *msg, int size);

    /* Task hooks */
    int (*task_kill)(struct task_struct *p, struct siginfo *info,
                     int sig, u32 secid);

    /* ... 100+ more hooks ... */
};
```

#### Global dispatch pointer

```c
/* security/security.c:29 */
static struct security_operations *security_ops;
static struct security_operations default_security_ops = {
    .name = "default",
};
```

#### Registration

```c
/* security/security.c:113 */
int __init register_security(struct security_operations *ops)
{
    if (verify(ops))
        return -EINVAL;
    if (security_ops != &default_security_ops)
        return -EAGAIN;     /* only one LSM allowed */
    security_ops = ops;
    return 0;
}
```

### 6.5 Outer Implementation Layer — SELinux

**SELinux security_operations** (`security/selinux/hooks.c:5452`):

```c
static struct security_operations selinux_ops = {
    .name                   = "selinux",

    .ptrace_access_check    = selinux_ptrace_access_check,
    .capable                = selinux_capable,

    .bprm_set_creds         = selinux_bprm_set_creds,
    .bprm_check_security    = selinux_bprm_check_security,

    .sb_mount               = selinux_mount,
    .sb_alloc_security      = selinux_sb_alloc_security,

    .inode_permission       = selinux_inode_permission,
    .inode_create           = selinux_inode_create,

    .file_permission        = selinux_file_permission,
    .dentry_open            = selinux_dentry_open,

    .socket_create          = selinux_socket_create,
    .task_kill              = selinux_task_kill,

    /* ... fills in 100+ hooks ... */
};
```

**Registration** (`security/selinux/hooks.c:5650`):

```c
if (!security_module_enable(&selinux_ops))
    return 0;

if (register_security(&selinux_ops))
    panic("SELinux: Unable to register with kernel.\n");
```

### 6.6 Execution Flow — File Open Security Check

```
  User: open("/path/to/file", O_RDONLY)
        |
        v
  do_sys_open()                          [fs/open.c]
        |--- do_filp_open()
        |       |--- path_openat()
        |               |--- do_last()
        |                       |
        |                       v
        |               security_dentry_open(file, cred)  [USE CASE]
        |                       |
        |                       v
        |               security_ops->dentry_open(file, cred) [DISPATCH]
        |                       |
        |                       v
        |               selinux_dentry_open()  [OUTER: SELinux]
        |                       |--- check AVC (access vector cache)
        |                       |--- evaluate SELinux policy
        |                       |--- return 0 (allow) or -EACCES (deny)
        |
        v
  File opened (or access denied)
```

### 6.7 Dependency Flow

```
  security/selinux/hooks.c  ---->  include/linux/security.h
  (SELinux implementation)         (security_operations interface)

  security/security.c       ---->  include/linux/security.h
  (LSM core dispatch)              (security_operations interface)

  fs/open.c                 ---->  include/linux/security.h
  (VFS caller)                     (security_*() inline hooks)

  include/linux/security.h:
    - defines struct security_operations
    - defines security_dentry_open(), security_inode_permission(), etc.
    - has NO includes of security/selinux/ or security/apparmor/

  Direction:
  SELinux impl -> security.h interface -> kernel entities (task, inode, file)
  VFS callers  -> security.h interface
```

### 6.8 Architecture Diagram

```
                  EXECUTION FLOW
                  ==============

      Kernel code (VFS, net, process mgmt)
                    |
        calls security_*() hooks
                    |
                    v
            +-------------------+
            | security/security.c|      USE CASE (Dispatch)
            | security_ops->*() |
            +---------+---------+
                      |
                      v
            +-------------------+
            | struct security_  |       INTERFACE ADAPTER
            |   operations      |
            | (150+ hooks)      |
            +---------+---------+
                      |
          +-----------+-----------+
          |                       |
          v                       v
  +---------------+      +----------------+
  |   SELinux     |      |   AppArmor     |    OUTER LAYER
  | selinux_ops   |      | apparmor_ops   |    (Policy impl)
  +---------------+      +----------------+


                  DEPENDENCY FLOW
                  ===============

  SELinux / AppArmor  ---->  security_operations  ---->  task_struct, inode, file
  (outer impl)               (interface)                 (entities from other subsystems)

  VFS / net / sched   ---->  security_*() hooks   ---->  security_operations
  (callers)                  (include/linux/security.h)  (interface)
```

### 6.9 Clean Architecture Insights

**Alignment:**

- LSM is a textbook **Strategy Pattern** at kernel scale. The `security_operations`
  struct defines the complete interface; SELinux/AppArmor provide concrete
  strategies.
- The core kernel (VFS, networking, process management) calls security hooks
  without knowing which LSM is active — pure dependency inversion.
- Security policy is completely separated from mechanism: the VFS enforces DAC
  permissions, while the LSM layer adds MAC (Mandatory Access Control) as an
  independent, pluggable concern.

**Divergence:**

- Only one primary LSM can be active at a time (v3.2 limitation). This is a
  singleton pattern rather than the composable architecture Clean Architecture
  would prefer. (Later kernel versions added LSM stacking.)
- The `security_operations` struct is enormous (150+ function pointers). Clean
  Architecture's Interface Segregation Principle would suggest splitting this
  into smaller, focused interfaces.
- The global `security_ops` pointer is a mutable global singleton, which Clean
  Architecture would prefer to inject as a dependency.

---

## 7. Interrupt Handling

### 7.1 Subsystem Overview

The interrupt handling subsystem manages hardware interrupts through a layered
architecture that separates interrupt controller hardware from interrupt
handling logic.

**Key Files:**

| File                      | Purpose                                       |
|---------------------------|-----------------------------------------------|
| `kernel/irq/handle.c`     | Core IRQ handling, `handle_irq_event()`       |
| `kernel/irq/chip.c`       | IRQ chip flow handlers                        |
| `kernel/irq/manage.c`     | `request_threaded_irq()`, `__setup_irq()`     |
| `kernel/irq/irqdesc.c`    | IRQ descriptor management                     |
| `include/linux/irq.h`     | `irq_chip`, `irq_data`                        |
| `include/linux/irqdesc.h` | `irq_desc`                                    |
| `include/linux/interrupt.h` | `irqaction`, `request_irq()`                |

### 7.2 Entities (Stable Layer)

#### struct irq_desc (`include/linux/irqdesc.h:41`)

The per-interrupt descriptor — the core entity:

```c
struct irq_desc {
    struct irq_data     irq_data;
    irq_flow_handler_t  handle_irq;       /* flow handler (level, edge, etc.) */
    struct irqaction    *action;           /* chain of handlers */
    unsigned int        status_use_accessors;
    unsigned int        depth;            /* disable nesting depth */
    unsigned int        wake_depth;
    raw_spinlock_t      lock;
};
```

#### struct irqaction (`include/linux/interrupt.h:111`)

An individual handler registered for an IRQ:

```c
struct irqaction {
    irq_handler_t       handler;          /* ISR function */
    unsigned long       flags;            /* IRQF_SHARED, etc. */
    void                *dev_id;          /* device identifier */
    struct irqaction    *next;            /* next handler in chain */
    int                 irq;
    irq_handler_t       thread_fn;        /* threaded handler */
    struct task_struct  *thread;
    unsigned long       thread_flags;
};
```

#### struct irq_data (`include/linux/irq.h`)

Links an IRQ number to its chip:

```c
struct irq_data {
    unsigned int        irq;
    unsigned long       hwirq;
    unsigned int        node;
    struct irq_chip     *chip;            /* -> interface adapter */
    void                *chip_data;
    void                *handler_data;
};
```

### 7.3 Use Cases (Policy Logic)

#### generic_handle_irq() — IRQ Dispatch

`kernel/irq/irqdesc.c:307`:

```c
int generic_handle_irq(unsigned int irq)
{
    struct irq_desc *desc = irq_to_desc(irq);
    if (!desc)
        return -EINVAL;
    generic_handle_irq_desc(irq, desc);
    return 0;
}
```

```c
/* include/linux/irqdesc.h:112 */
static inline void generic_handle_irq_desc(unsigned int irq, struct irq_desc *desc)
{
    desc->handle_irq(irq, desc);   /* dispatches to flow handler */
}
```

#### handle_irq_event_percpu() — Handler Chain Execution

`kernel/irq/handle.c:117`:

Walks the `irqaction` chain for a given IRQ and calls each handler:

```c
irqreturn_t handle_irq_event_percpu(struct irq_desc *desc, struct irqaction *action)
{
    irqreturn_t retval = IRQ_NONE;

    do {
        irqreturn_t res;
        res = action->handler(irq, action->dev_id);
        /* ... handle threaded IRQs if needed ... */
        retval |= res;
        action = action->next;
    } while (action);

    return retval;
}
```

#### request_irq() — Handler Registration

`include/linux/interrupt.h:133`:

```c
static inline int request_irq(unsigned int irq, irq_handler_t handler,
                               unsigned long flags, const char *name, void *dev)
{
    return request_threaded_irq(irq, handler, NULL, flags, name, dev);
}
```

`request_threaded_irq()` → `__setup_irq()` (`kernel/irq/manage.c:879`):

1. Allocate and initialize `struct irqaction`
2. For shared IRQs, verify trigger type compatibility
3. Chain the new action into `desc->action`
4. If first handler, program the IRQ chip and unmask
5. For threaded handlers, create a kernel thread

### 7.4 Interface Adapters

#### struct irq_chip (`include/linux/irq.h:296`)

Abstracts the interrupt controller hardware:

```c
struct irq_chip {
    const char  *name;
    unsigned int (*irq_startup)(struct irq_data *data);
    void (*irq_shutdown)(struct irq_data *data);
    void (*irq_enable)(struct irq_data *data);
    void (*irq_disable)(struct irq_data *data);
    void (*irq_ack)(struct irq_data *data);
    void (*irq_mask)(struct irq_data *data);
    void (*irq_mask_ack)(struct irq_data *data);
    void (*irq_unmask)(struct irq_data *data);
    void (*irq_eoi)(struct irq_data *data);
    int (*irq_set_affinity)(struct irq_data *data,
                            const struct cpumask *dest, bool force);
    int (*irq_set_type)(struct irq_data *data, unsigned int flow_type);
    int (*irq_set_wake)(struct irq_data *data, unsigned int on);
};
```

#### Flow handlers (used in irq_desc.handle_irq)

The kernel provides generic flow handlers that use `irq_chip` methods:

- `handle_level_irq()` — for level-triggered interrupts
- `handle_edge_irq()` — for edge-triggered interrupts
- `handle_fasteoi_irq()` — for modern interrupt controllers with EOI
- `handle_simple_irq()` — direct dispatch

These are defined in `kernel/irq/chip.c` and call chip methods like
`irq_ack()`, `irq_mask()`, `irq_unmask()` as appropriate for the trigger type.

### 7.5 Outer Implementation Layer

Each architecture and interrupt controller provides its own `irq_chip`:

- **x86 APIC**: `arch/x86/kernel/apic/` — provides chip for local/IO APIC
- **ARM GIC**: `arch/arm/common/gic.c` — Generic Interrupt Controller
- **Legacy 8259 PIC**: `arch/x86/kernel/i8259.c`

Example (simplified):

```c
static struct irq_chip ioapic_chip = {
    .name           = "IO-APIC",
    .irq_startup    = startup_ioapic_irq,
    .irq_mask       = mask_ioapic_irq,
    .irq_unmask     = unmask_ioapic_irq,
    .irq_ack        = ack_apic_edge,
    .irq_eoi        = ack_apic_level,
    .irq_set_affinity = ioapic_set_affinity,
};
```

### 7.6 Execution Flow — Hardware Interrupt

```
  Hardware device asserts IRQ line
        |
        v
  CPU enters interrupt vector
        |
        v
  arch-specific entry (arch/x86/kernel/entry_*.S)
        |
        v
  do_IRQ()                               [arch/x86/kernel/irq.c]
        |--- irq = vector_to_irq(vector)
        |
        v
  generic_handle_irq(irq)               [kernel/irq/irqdesc.c:307]
        |
        v
  desc->handle_irq(irq, desc)           [flow handler dispatch]
        |
        v
  handle_level_irq() or handle_edge_irq() [kernel/irq/chip.c]
        |--- desc->irq_data.chip->irq_ack(data)     [chip: ACK]
        |
        v
  handle_irq_event(desc)                [kernel/irq/handle.c]
        |
        v
  handle_irq_event_percpu(desc, action) [kernel/irq/handle.c:117]
        |--- action->handler(irq, action->dev_id)   [DRIVER ISR]
        |--- action = action->next                   [shared IRQs]
        |
        v
  chip->irq_unmask(data) or chip->irq_eoi(data)    [chip: EOI]
        |
        v
  Return from interrupt
```

### 7.7 Dependency Flow

```
  arch/x86/kernel/apic/  ---->  include/linux/irq.h    ---->  irq_desc, irq_data
  (APIC irq_chip impl)         (irq_chip interface)           (entities)

  drivers/net/e1000/     ---->  include/linux/interrupt.h
  (device driver ISR)           (request_irq, irqaction)

  kernel/irq/chip.c      ---->  include/linux/irq.h
  (flow handlers)               (irq_chip interface)

  Direction:
  APIC chip impl  -> irq.h interface  -> irq_desc entity
  device driver   -> interrupt.h      -> irqaction entity
  flow handlers   -> irq.h interface  -> irq_desc entity
```

### 7.8 Architecture Diagram

```
                  EXECUTION FLOW
                  ==============

        Hardware Interrupt
              |
              v
      +-------------------+
      | Arch entry code   |       OUTER (Arch-specific)
      | do_IRQ()          |
      +--------+----------+
               |
               v
      +-------------------+
      | generic_handle_   |       USE CASE (Dispatch)
      |   irq()           |
      +--------+----------+
               |
      via desc->handle_irq
               |
               v
      +-------------------+
      | Flow handler      |       USE CASE (IRQ flow policy)
      | handle_level_irq()|
      +--------+----------+
               |
      via chip->irq_ack() etc.     via action->handler()
          |                             |
          v                             v
  +---------------+            +------------------+
  | struct        |            | struct irqaction  |
  |  irq_chip     |            | (driver ISR)      |
  | INTERFACE     |            | OUTER LAYER       |
  +-------+-------+            +------------------+
          |
          v
  +---------------+
  | APIC / GIC /  |            OUTER LAYER
  | PIC impl      |            (HW controller)
  +---------------+


                  DEPENDENCY FLOW
                  ===============

  APIC impl     -> irq.h (irq_chip interface) -> irq_desc (entity)
  Driver ISR    -> interrupt.h (request_irq)   -> irqaction (entity)
  Flow handlers -> irq.h (irq_chip interface) -> irq_desc (entity)
```

### 7.9 Clean Architecture Insights

**Alignment:**

- `irq_chip` cleanly separates interrupt controller hardware from flow handling
  logic. Adding support for a new interrupt controller requires only implementing
  `irq_chip` — the flow handlers and handler chain are reused unchanged.
- The flow handler pattern (level vs. edge vs. fasteoi) is itself a strategy
  pattern, selected per-IRQ based on the interrupt type.
- Driver ISR registration via `request_irq()` is completely decoupled from the
  interrupt controller — drivers don't need to know whether interrupts are
  managed by an APIC, GIC, or legacy PIC.

**Divergence:**

- The `desc->handle_irq` function pointer (flow handler) is set per-IRQ
  during setup, not dispatched through an interface struct. This is a practical
  optimization but breaks the uniform interface pattern.
- The `irqaction` chain for shared interrupts means all handlers are called
  for every interrupt on that line — a brute-force approach that Clean
  Architecture might replace with a more targeted dispatch.

---

## 8. Time Management

### 8.1 Subsystem Overview

The time management subsystem abstracts hardware timers and provides the kernel
with timekeeping, periodic ticks, one-shot timers, and high-resolution timers.

**Key Files:**

| File                          | Purpose                                    |
|-------------------------------|--------------------------------------------|
| `kernel/time/timekeeping.c`    | Wall clock, `getnstimeofday()`            |
| `kernel/time/clocksource.c`   | Clock source selection and registration    |
| `kernel/time/clockevents.c`   | Clock event device management              |
| `kernel/timer.c`              | Classic timer wheel, `add_timer()`         |
| `kernel/hrtimer.c`            | High-resolution timers                     |
| `include/linux/clocksource.h` | `struct clocksource`                       |
| `include/linux/clockchips.h`  | `struct clock_event_device`                |
| `include/linux/timer.h`       | `struct timer_list`                        |
| `include/linux/hrtimer.h`     | `struct hrtimer`                           |

### 8.2 Entities (Stable Layer)

#### struct timer_list (`include/linux/timer.h:12`)

The classic kernel timer:

```c
struct timer_list {
    struct list_head entry;
    unsigned long expires;               /* jiffies when timer fires */
    struct tvec_base *base;

    void (*function)(unsigned long);     /* callback */
    unsigned long data;                  /* callback argument */
    int slack;
};
```

#### struct hrtimer (`include/linux/hrtimer.h:108`)

High-resolution timer with nanosecond precision:

```c
struct hrtimer {
    struct timerqueue_node      node;
    ktime_t                     _softexpires;
    enum hrtimer_restart        (*function)(struct hrtimer *);
    struct hrtimer_clock_base   *base;
    unsigned long               state;
};
```

### 8.3 Use Cases (Policy Logic)

#### run_local_timers() — Tick Processing

`kernel/timer.c:1339`:

```c
void run_local_timers(void)
{
    hrtimer_run_queues();
    raise_softirq(TIMER_SOFTIRQ);    /* trigger timer wheel processing */
}
```

#### __run_timers() — Timer Wheel Execution

`kernel/timer.c:1092`:

```c
static inline void __run_timers(struct tvec_base *base)
{
    while (time_after_eq(jiffies, base->timer_jiffies)) {
        /* cascade from tv1 through tv5 if needed */
        list_replace_init(base->tv1.vec + index, &work_list);
        while (!list_empty(head)) {
            timer = list_first_entry(head, struct timer_list, entry);
            fn = timer->function;
            data = timer->data;
            detach_timer(timer, 1);
            spin_unlock_irq(&base->lock);
            call_timer_fn(timer, fn, data);   /* fn(data) */
            spin_lock_irq(&base->lock);
        }
    }
}
```

#### Softirq dispatcher

`kernel/timer.c:1307`:

```c
static void run_timer_softirq(struct softirq_action *h)
{
    struct tvec_base *base = __this_cpu_read(tvec_bases);
    hrtimer_run_pending();
    if (time_after_eq(jiffies, base->timer_jiffies))
        __run_timers(base);
}
```

### 8.4 Interface Adapters

#### struct clocksource (`include/linux/clocksource.h:167`)

Abstracts the hardware time counter:

```c
struct clocksource {
    cycle_t (*read)(struct clocksource *cs);   /* read raw cycles */
    cycle_t cycle_last;
    cycle_t mask;
    u32 mult;                                   /* cycles -> ns multiplier */
    u32 shift;                                  /* cycles -> ns shift */
    u64 max_idle_ns;
    const char *name;
    struct list_head list;
    int rating;                                 /* quality rating */
    int (*enable)(struct clocksource *cs);
    void (*disable)(struct clocksource *cs);
    void (*suspend)(struct clocksource *cs);
    void (*resume)(struct clocksource *cs);
};
```

The `read()` function pointer is the critical abstraction: the timekeeping
subsystem calls `cs->read(cs)` to get raw counter cycles, without knowing
whether the underlying hardware is a TSC, HPET, ACPI PM timer, or ARM
generic timer.

The `rating` field enables automatic best-source selection. Higher-rated
clocksources replace lower-rated ones when registered.

#### struct clock_event_device (`include/linux/clockchips.h:86`)

Abstracts programmable timer hardware (for generating interrupts):

```c
struct clock_event_device {
    void (*event_handler)(struct clock_event_device *);
    int (*set_next_event)(unsigned long evt, struct clock_event_device *);
    int (*set_next_ktime)(ktime_t expires, struct clock_event_device *);
    void (*set_mode)(enum clock_event_mode mode, struct clock_event_device *);
    void (*broadcast)(const struct cpumask *mask);
    const char *name;
    int rating;
    int irq;
};
```

`set_next_event()` programs the hardware to fire after a given number of cycles.
`event_handler()` is called when the programmed event fires — this is set by the
timekeeping core to either `tick_handle_periodic()` or `hrtimer_interrupt()`.

### 8.5 Outer Implementation Layer

Hardware-specific clocksource and clock_event_device implementations:

**x86 TSC clocksource** (`arch/x86/kernel/tsc.c`):

```c
static struct clocksource clocksource_tsc = {
    .name       = "tsc",
    .rating     = 300,
    .read       = read_tsc,
    .mask       = CLOCKSOURCE_MASK(64),
    .flags      = CLOCK_SOURCE_IS_CONTINUOUS | CLOCK_SOURCE_MUST_VERIFY,
};
```

**x86 HPET clocksource** (`arch/x86/kernel/hpet.c`):

```c
static struct clocksource clocksource_hpet = {
    .name       = "hpet",
    .rating     = 250,
    .read       = read_hpet,
    .mask       = HPET_MASK,
    .flags      = CLOCK_SOURCE_IS_CONTINUOUS,
};
```

**LAPIC clock_event_device** (`arch/x86/kernel/apic/apic.c`):

```c
static struct clock_event_device lapic_clockevent = {
    .name       = "lapic",
    .features   = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
    .set_mode   = lapic_timer_setup,
    .set_next_event = lapic_next_event,
    .rating     = 100,
};
```

### 8.6 Execution Flow — Timer Firing

```
  Hardware timer interrupt (LAPIC / HPET)
        |
        v
  Timer IRQ handler (arch-specific)
        |
        v
  clock_event_device->event_handler()     [DISPATCH]
        |
        v
  tick_handle_periodic()  or  hrtimer_interrupt()
        |
        v (periodic tick path)
  update_process_times()
        |--- account_process_tick()
        |
        v
  run_local_timers()                     [kernel/timer.c:1339]
        |--- hrtimer_run_queues()
        |--- raise_softirq(TIMER_SOFTIRQ)
        |
        v (deferred to softirq)
  run_timer_softirq()                    [kernel/timer.c:1307]
        |
        v
  __run_timers(base)                     [kernel/timer.c:1092]
        |--- cascade timer wheel
        |--- for each expired timer:
        |       timer->function(timer->data)    [CALLBACK]
        |
        v
  Driver/subsystem timer callback executes
```

### 8.7 Dependency Flow

```
  arch/x86/kernel/tsc.c    ---->  include/linux/clocksource.h
  (TSC implementation)             (clocksource interface)

  arch/x86/kernel/hpet.c   ---->  include/linux/clockchips.h
  (HPET implementation)            (clock_event_device interface)

  kernel/time/timekeeping.c ---->  include/linux/clocksource.h
  (timekeeping use case)           (clocksource interface)

  kernel/time/clockevents.c ---->  include/linux/clockchips.h
  (event management)               (clock_event_device interface)

  kernel/timer.c            ---->  include/linux/timer.h
  (timer wheel policy)             (timer_list entity)

  Direction:
  TSC / HPET / LAPIC  -> clocksource/clockchips interfaces -> timer entities
  (arch code)           (include/linux/ headers)              (stable core)
```

### 8.8 Architecture Diagram

```
                  EXECUTION FLOW
                  ==============

        Hardware Timer Interrupt
                |
                v
        +-------------------+
        | Arch timer ISR    |       OUTER (Arch-specific)
        +--------+----------+
                 |
                 v
        +-------------------+
        | clock_event_device|       INTERFACE ADAPTER
        | ->event_handler() |
        +--------+----------+
                 |
                 v
        +-------------------+
        | tick_handle_      |       USE CASE (Tick policy)
        |   periodic()      |
        | run_local_timers()|
        +--------+----------+
                 |
                 v
        +-------------------+
        | Timer Wheel /     |       USE CASE (Timer dispatch)
        | __run_timers()    |
        +--------+----------+
                 |
                 v
        +-------------------+
        | timer->function() |       OUTER (Subsystem callback)
        | (driver/subsystem)|
        +-------------------+


       CLOCKSOURCE CHAIN
       =================

        +-------------------+
        | timekeeping.c     |       USE CASE (Wall clock)
        | getnstimeofday()  |
        +--------+----------+
                 |
          cs->read(cs)
                 |
                 v
        +-------------------+
        | struct clocksource|       INTERFACE ADAPTER
        | (.read, .enable,  |
        |  .suspend)        |
        +--------+----------+
                 |
        +--------+--------+
        |        |        |
        v        v        v
     +-----+  +-----+  +------+
     | TSC |  | HPET|  | ACPI |    OUTER (HW implementations)
     +-----+  +-----+  | PM   |
                        +------+


                  DEPENDENCY FLOW
                  ===============

  TSC / HPET / LAPIC -> clocksource.h / clockchips.h -> timer_list / hrtimer
  (arch implementations)  (interfaces)                   (entities)

  timer.c / hrtimer.c -> timer.h / hrtimer.h
  (use cases)            (entity definitions)
```

### 8.9 Clean Architecture Insights

**Alignment:**

- `struct clocksource` provides clean dependency inversion for timekeeping.
  The `read()` callback abstracts all hardware complexity. Adding a new timer
  hardware requires only implementing a new `clocksource` and registering it.
- The `rating` system provides automatic quality-based selection — the best
  available clocksource is used without explicit configuration.
- `clock_event_device` separates the "what happens on timer fire" (set by the
  kernel core via `event_handler`) from "how to program the hardware" (provided
  by the implementation via `set_next_event`).
- The timer wheel and hrtimer subsystems are pure use-case logic that operate on
  entity structs (`timer_list`, `hrtimer`) without hardware awareness.

**Divergence:**

- The `timer_list.function` callback is a raw function pointer rather than
  dispatching through an interface struct. This is appropriate for the timer
  use case (callbacks are heterogeneous by nature) but differs from the
  structured interfaces used elsewhere.
- The tight coupling between tick handling and both timer wheel and hrtimer
  subsystems means the use-case layer is less modular than ideal.
- The `jiffies` global variable is a shared mutable state that many subsystems
  depend on — a practical necessity but architecturally unclean.

---

## Cross-Cutting Summary

### Universal Patterns Across All 8 Subsystems

1. **Function pointer structs as interfaces**: Every subsystem uses this pattern.
   It is the kernel's universal mechanism for dependency inversion.

2. **Registration functions as plugin entry points**: `register_filesystem()`,
   `driver_register()`, `register_security()`, `clocksource_register()`,
   `request_irq()` — all follow the same pattern of an outer component
   registering itself with an inner framework.

3. **Entities defined in include/linux/**: All core data structures live in
   the `include/linux/` header tree, accessible to all subsystems. Implementations
   live in subsystem directories and depend on these headers.

4. **Execution flows outward**: Syscalls enter through generic code, dispatch
   through interfaces, and reach implementations. The call chain moves from
   stable to volatile.

5. **Dependencies point inward**: Implementations include core headers. Core
   headers never include implementation files.

### The Kernel's Architectural Genius

The Linux kernel, despite being a monolithic kernel written in C without
language-level support for interfaces or abstract classes, achieves a degree
of architectural cleanliness that rivals many object-oriented systems. The
`struct of function pointers` pattern, combined with disciplined header
organization, creates a system where:

> **Stable policy at the center attracts dependencies.**
> **Volatile implementation at the periphery depends inward.**

This is the essence of Clean Architecture — achieved through C conventions
rather than language enforcement.

---

*Analysis based on Linux kernel source tree at `/home/morrism/repos/linux`,
version 3.2.0 ("Saber-toothed Squirrel").*
