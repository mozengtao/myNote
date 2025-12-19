# Linux GPIO Subsystem Architecture

A deep systems apprenticeship in GPIO driver design from Linux kernel v3.2.

---

## Table of Contents

- [Phase 1: GPIO Subsystem Overview](#phase-1--gpio-subsystem-overview)
- [Phase 2: gpiolib Core Architecture](#phase-2--gpiolib-core-architecture)
- [Phase 3: struct gpio_chip (Central Object)](#phase-3--struct-gpio_chip-central-object)
- [Phase 4: Ops, Callbacks, and Contracts](#phase-4--ops-callbacks-and-contracts)
- [Phase 5: Driver Registration & Lifetime](#phase-5--driver-registration--lifetime)
- [Phase 6: Board / Platform Integration](#phase-6--board--platform-integration)
- [Phase 7: End-to-End Data Flow](#phase-7--end-to-end-data-flow)
- [Phase 8: Concurrency, Locking, and Context](#phase-8--concurrency-locking-and-context)
- [Phase 9: Common GPIO Driver Bugs](#phase-9--common-gpio-driver-bugs)
- [Phase 10: Architecture Lessons](#phase-10--architecture-lessons)
- [Appendix A: Walking Through Real GPIO Controller Drivers](#appendix-a--walking-through-real-gpio-controller-drivers)

---

## Phase 1 — GPIO Subsystem Overview

### 1.1 Problems the GPIO Subsystem Solves

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                  WITHOUT GPIO SUBSYSTEM (CHAOS)                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   Driver A ────────────► SoC GPIO Registers                                 │
│   Driver B ────────────► SoC GPIO Registers   (conflict!)                   │
│   Driver C ────────────► I2C Expander         (different API!)              │
│   Driver D ────────────► SPI Expander         (yet another API!)            │
│                                                                             │
│   Problems:                                                                 │
│   1. Each driver directly accesses hardware → conflicts                     │
│   2. Each GPIO controller has different API → code duplication              │
│   3. No way to know if a GPIO is already in use → bugs                      │
│   4. Platform code tightly coupled to drivers → portability issues          │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                    WITH GPIO SUBSYSTEM (ORDER)                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│                      ┌───────────────────────────┐                          │
│                      │      GPIOLIB CORE          │                          │
│                      │   (Uniform API + Tracking) │                          │
│                      └─────────────┬─────────────┘                          │
│                                    │                                        │
│          ┌─────────────────────────┼─────────────────────────┐              │
│          │                         │                         │              │
│          ▼                         ▼                         ▼              │
│   ┌─────────────┐          ┌─────────────┐          ┌─────────────┐         │
│   │ SoC GPIO    │          │ I2C GPIO    │          │ SPI GPIO    │         │
│   │ Controller  │          │ Expander    │          │ Expander    │         │
│   │ (gpio_chip) │          │ (gpio_chip) │          │ (gpio_chip) │         │
│   └─────────────┘          └─────────────┘          └─────────────┘         │
│                                                                             │
│   Benefits:                                                                 │
│   1. One API for all GPIO types                                             │
│   2. Conflict detection via gpio_request()                                  │
│   3. Drivers don't know hardware details                                    │
│   4. sysfs interface for debugging                                          │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- GPIO 子系统提供统一的 API，隐藏不同控制器的差异
- `gpio_request()` 防止多个驱动同时使用同一个 GPIO
- 驱动程序不需要知道底层硬件细节（SoC、I2C、SPI 扩展器）
- sysfs 接口方便调试

### 1.2 Why GPIO is Centralized via gpiolib

| Problem | gpiolib Solution |
|---------|------------------|
| **Diverse hardware** | Uniform `gpio_chip` abstraction |
| **Resource conflicts** | `gpio_request()`/`gpio_free()` tracking |
| **GPIO numbering** | Global namespace with `base` + `offset` |
| **Debugging** | `/sys/class/gpio/` and `/sys/kernel/debug/gpio` |
| **Hot-plug expanders** | Dynamic registration via `gpiochip_add()` |

### 1.3 The Three Roles in the GPIO Subsystem

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         GPIO SUBSYSTEM ROLES                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   1. GPIO CORE (gpiolib.c)                                                  │
│   ─────────────────────────                                                 │
│   • Manages global GPIO number space                                        │
│   • Tracks which GPIOs are in use                                           │
│   • Routes API calls to correct gpio_chip                                   │
│   • Provides sysfs interface                                                │
│                                                                             │
│   2. GPIO CONTROLLER DRIVERS (gpio-xxx.c)                                   │
│   ─────────────────────────────────────────                                 │
│   • Implement struct gpio_chip                                              │
│   • Register with gpiochip_add()                                            │
│   • Provide callbacks: direction_input, get, set, etc.                      │
│   • Handle hardware-specific operations                                     │
│                                                                             │
│   3. GPIO CONSUMERS (any driver using GPIOs)                                │
│   ───────────────────────────────────────────                               │
│   • Request GPIOs with gpio_request()                                       │
│   • Use gpio_direction_input/output()                                       │
│   • Use gpio_get_value/gpio_set_value()                                     │
│   • Release with gpio_free()                                                │
│                                                                             │
│   INTERACTION:                                                              │
│   ────────────                                                              │
│                                                                             │
│   Consumer                 Core                    Controller               │
│      │                      │                          │                    │
│      │ gpio_set_value(42,1) │                          │                    │
│      │──────────────────────►                          │                    │
│      │                      │ gpio_desc[42].chip       │                    │
│      │                      │ = &my_gpio_chip          │                    │
│      │                      │                          │                    │
│      │                      │ chip->set(chip, offset=2, value=1)            │
│      │                      │──────────────────────────►                    │
│      │                      │                          │ [write HW reg]     │
│      │                      │                          │                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 核心层（gpiolib）负责路由和跟踪
- 控制器驱动实现硬件特定的操作
- 消费者驱动只使用统一的 API，不关心底层硬件

### 1.4 GPIO Code Locations in v3.2

| Component | Location | Purpose |
|-----------|----------|---------|
| **Core** | `drivers/gpio/gpiolib.c` | Central management, API implementation |
| **Headers** | `include/linux/gpio.h` | Consumer API |
| | `include/asm-generic/gpio.h` | `struct gpio_chip` definition |
| **Drivers** | `drivers/gpio/gpio-*.c` | Controller implementations |
| **Platform** | `arch/*/mach-*/board-*.c` | GPIO assignments |

---

## Phase 2 — gpiolib Core Architecture

### 2.1 Core Files

```c
/* drivers/gpio/gpiolib.c - The heart of GPIO subsystem */

/* Key includes */
#include <linux/gpio.h>       /* Consumer API */
#include <linux/of_gpio.h>    /* Device Tree support */

/* Key exports */
EXPORT_SYMBOL_GPL(gpio_request);
EXPORT_SYMBOL_GPL(gpio_free);
EXPORT_SYMBOL_GPL(gpio_direction_input);
EXPORT_SYMBOL_GPL(gpio_direction_output);
EXPORT_SYMBOL_GPL(__gpio_get_value);
EXPORT_SYMBOL_GPL(__gpio_set_value);
EXPORT_SYMBOL_GPL(gpiochip_add);
EXPORT_SYMBOL_GPL(gpiochip_remove);
```

### 2.2 Global GPIO Numbering (v3.2 Era)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    GLOBAL GPIO NUMBER SPACE (v3.2)                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   GPIO Number:  0    31   32   63   64   79   80        255                 │
│                 ├─────┤    ├─────┤   ├────┤    ├──────────┤                 │
│                 │SoC  │    │SoC  │   │I2C │    │Unassigned│                 │
│                 │Bank0│    │Bank1│   │Exp │    │          │                 │
│                 └─────┘    └─────┘   └────┘    └──────────┘                 │
│                                                                             │
│   chip[0]:      base=0   ngpio=32                                           │
│   chip[1]:      base=32  ngpio=32                                           │
│   chip[2]:      base=64  ngpio=16                                           │
│                                                                             │
│   FORMULA:                                                                  │
│   ─────────                                                                 │
│   gpio_number = chip->base + offset                                         │
│   offset = gpio_number - chip->base                                         │
│                                                                             │
│   EXAMPLE:                                                                  │
│   ──────────                                                                │
│   GPIO 70 belongs to chip[2] (base=64)                                      │
│   offset = 70 - 64 = 6                                                      │
│   → chip[2]->set(chip, 6, value)                                            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- v3.2 使用全局 GPIO 编号（整数）
- 每个 `gpio_chip` 有一个 `base` 和 `ngpio`
- `base` 可以是固定的（平台定义）或动态分配的（`base = -1`）
- 这种设计后来被 GPIO 描述符（descriptor）替代

### 2.3 Why Descriptors Were NOT Yet Dominant

In v3.2:
- GPIOs are identified by **global integer numbers**
- The `gpio_desc` array is a flat, global array
- This works but has problems:
  - Hard to manage GPIO namespace across platforms
  - Numbers assigned in board files → maintenance nightmare
  - No type safety (just an int)

**Later kernels** moved to `struct gpio_desc *` descriptors for:
- Type safety
- Dynamic lookup without global numbering
- Better Device Tree integration

### 2.4 Main Global Data Structures

```c
/* drivers/gpio/gpiolib.c */

/* [KEY] The global GPIO lock */
static DEFINE_SPINLOCK(gpio_lock);

/* [KEY] Per-GPIO descriptor */
struct gpio_desc {
    struct gpio_chip    *chip;      /* [KEY] Which controller owns this GPIO */
    unsigned long       flags;      /* Requested, direction, etc. */
#ifdef CONFIG_DEBUG_FS
    const char          *label;     /* Who requested it */
#endif
};

/* Flags */
#define FLAG_REQUESTED  0   /* GPIO has been requested */
#define FLAG_IS_OUT     1   /* GPIO is output */
#define FLAG_RESERVED   2   /* Reserved for platform */
#define FLAG_EXPORT     3   /* Exported to sysfs */
#define FLAG_SYSFS      4   /* Requested via sysfs */
#define FLAG_ACTIVE_LOW 7   /* Inverted value */

/* [KEY] The global GPIO table */
static struct gpio_desc gpio_desc[ARCH_NR_GPIOS];  /* Default: 256 */

/* [KEY] Helper to find chip from GPIO number */
static inline struct gpio_chip *gpio_to_chip(unsigned gpio)
{
    return gpio_desc[gpio].chip;
}
```

**说明:**
- `gpio_desc[]` 是全局数组，索引就是 GPIO 编号
- 每个描述符指向其控制器（`gpio_chip *chip`）
- `flags` 跟踪状态（是否被请求、方向等）
- `gpio_lock` 保护并发访问

---

## Phase 3 — struct gpio_chip (Central Object)

### 3.1 What gpio_chip Represents

`struct gpio_chip` is the **central abstraction** for a GPIO controller. It represents:
- A block of contiguous GPIO pins
- The operations to control those pins
- Metadata about the controller

```c
/* include/asm-generic/gpio.h */

/**
 * struct gpio_chip - abstract a GPIO controller
 * @label: for diagnostics
 * @dev: optional device providing the GPIOs
 * @owner: helps prevent removal of modules exporting active GPIOs
 * @request: optional hook for chip-specific activation
 * @free: optional hook for chip-specific deactivation
 * @direction_input: configures signal "offset" as input
 * @get: returns value for signal "offset"
 * @direction_output: configures signal "offset" as output
 * @set: assigns output value for signal "offset"
 * @to_irq: optional hook for GPIO-to-IRQ mapping
 * @dbg_show: optional debugfs hook
 * @base: first GPIO number handled by this chip
 * @ngpio: number of GPIOs handled by this controller
 * @can_sleep: flag set if get()/set() methods may sleep
 * @names: optional array of names for GPIOs
 */
struct gpio_chip {
    const char          *label;
    struct device       *dev;
    struct module       *owner;

    /* [KEY] Callbacks - the ops pattern! */
    int     (*request)(struct gpio_chip *chip, unsigned offset);
    void    (*free)(struct gpio_chip *chip, unsigned offset);
    int     (*direction_input)(struct gpio_chip *chip, unsigned offset);
    int     (*get)(struct gpio_chip *chip, unsigned offset);
    int     (*direction_output)(struct gpio_chip *chip, unsigned offset, int value);
    int     (*set_debounce)(struct gpio_chip *chip, unsigned offset, unsigned debounce);
    void    (*set)(struct gpio_chip *chip, unsigned offset, int value);
    int     (*to_irq)(struct gpio_chip *chip, unsigned offset);
    void    (*dbg_show)(struct seq_file *s, struct gpio_chip *chip);

    /* [KEY] GPIO number range */
    int                 base;       /* First GPIO number */
    u16                 ngpio;      /* Number of GPIOs */
    
    const char *const   *names;     /* Optional GPIO names */
    unsigned            can_sleep:1; /* [KEY] Sleeping allowed? */
    unsigned            exported:1;  /* Exported to sysfs? */

#if defined(CONFIG_OF_GPIO)
    struct device_node  *of_node;
    int of_gpio_n_cells;
    int (*of_xlate)(struct gpio_chip *gc, struct device_node *np,
                    const void *gpio_spec, u32 *flags);
#endif
};
```

### 3.2 Field Purpose Summary

| Field | Purpose |
|-------|---------|
| `label` | Human-readable name for debugging |
| `dev` | Parent device (for sysfs hierarchy) |
| `owner` | Module owning this chip (for refcounting) |
| `base` | First GPIO number (or -1 for dynamic) |
| `ngpio` | How many GPIOs this chip controls |
| `can_sleep` | **CRITICAL**: Set if ops may sleep (I2C, SPI) |
| `direction_input` | Configure pin as input |
| `direction_output` | Configure pin as output |
| `get` | Read pin value |
| `set` | Write pin value |
| `request` | Hook called when GPIO is requested |
| `free` | Hook called when GPIO is freed |
| `to_irq` | Map GPIO to IRQ number |

### 3.3 Ownership Rules

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        gpio_chip OWNERSHIP RULES                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   WHO ALLOCATES?                                                            │
│   ───────────────                                                           │
│   The driver allocates gpio_chip, usually embedded in driver-private struct │
│                                                                             │
│   struct my_gpio_driver {                                                   │
│       spinlock_t lock;                                                      │
│       void __iomem *base;                                                   │
│       struct gpio_chip chip;     /* [KEY] Embedded in driver struct */      │
│   };                                                                        │
│                                                                             │
│   WHO REGISTERS?                                                            │
│   ───────────────                                                           │
│   The driver calls gpiochip_add() in probe()                                │
│                                                                             │
│   static int my_probe(struct platform_device *pdev) {                       │
│       struct my_gpio_driver *priv = devm_kzalloc(...);                      │
│       priv->chip.base = -1;  /* Dynamic */                                  │
│       priv->chip.ngpio = 8;                                                 │
│       priv->chip.direction_input = my_direction_input;                      │
│       ...                                                                   │
│       return gpiochip_add(&priv->chip);   /* [KEY] Register */              │
│   }                                                                         │
│                                                                             │
│   WHO FREES?                                                                │
│   ──────────                                                                │
│   The driver calls gpiochip_remove() in remove(), then frees struct         │
│                                                                             │
│   static int my_remove(struct platform_device *pdev) {                      │
│       struct my_gpio_driver *priv = platform_get_drvdata(pdev);             │
│       gpiochip_remove(&priv->chip);  /* [KEY] Unregister first! */          │
│       /* priv freed by devm */                                              │
│       return 0;                                                             │
│   }                                                                         │
│                                                                             │
│   INVARIANT:                                                                │
│   ──────────                                                                │
│   - gpiochip_remove() FAILS if any GPIO is still requested                  │
│   - Driver must ensure all GPIOs are freed before removal                   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 驱动程序分配 `gpio_chip`（通常嵌入在私有结构中）
- 驱动程序在 `probe()` 中调用 `gpiochip_add()` 注册
- 驱动程序在 `remove()` 中调用 `gpiochip_remove()` 注销
- 如果有 GPIO 仍在使用，`gpiochip_remove()` 会失败

### 3.4 Attaching Private Driver Data

```c
/* Pattern 1: Embed gpio_chip in driver structure (PREFERRED) */
struct pl061_gpio {
    spinlock_t      lock;
    void __iomem    *base;
    unsigned        irq_base;
    struct gpio_chip gc;           /* [KEY] Embedded */
};

/* Recover driver struct from gpio_chip */
static int pl061_get_value(struct gpio_chip *gc, unsigned offset)
{
    /* [KEY] container_of pattern */
    struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
    return !!readb(chip->base + (1 << (offset + 2)));
}

/* Pattern 2: Use dev_set_drvdata (also common) */
static int my_probe(struct platform_device *pdev)
{
    struct my_priv *priv = devm_kzalloc(...);
    
    priv->gc.dev = &pdev->dev;
    platform_set_drvdata(pdev, priv);  /* Store priv in device */
    
    return gpiochip_add(&priv->gc);
}
```

---

## Phase 4 — Ops, Callbacks, and Contracts

### 4.1 Mandatory vs Optional Callbacks

| Callback | Mandatory? | Purpose |
|----------|------------|---------|
| `direction_input` | **YES** | Configure as input |
| `get` | **YES** | Read pin value |
| `direction_output` | **YES** | Configure as output |
| `set` | **YES** | Write pin value |
| `request` | Optional | Chip-specific activation |
| `free` | Optional | Chip-specific deactivation |
| `set_debounce` | Optional | Hardware debounce |
| `to_irq` | Optional | GPIO-to-IRQ mapping |
| `dbg_show` | Optional | Custom debugfs output |

### 4.2 Callback Contracts

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CALLBACK CONTRACT TABLE                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  CALLBACK              WHEN CALLED              CONTEXT          RETURN     │
│  ────────────────────────────────────────────────────────────────────────   │
│  request()             gpio_request()           may sleep        0 or -errno│
│  free()                gpio_free()              may sleep        void       │
│  direction_input()     gpio_direction_input()   depends*         0 or -errno│
│  direction_output()    gpio_direction_output()  depends*         0 or -errno│
│  get()                 gpio_get_value()         depends*         0 or 1     │
│  set()                 gpio_set_value()         depends*         void       │
│  to_irq()              gpio_to_irq()            atomic!          IRQ or -err│
│  set_debounce()        gpio_set_debounce()      may sleep        0 or -errno│
│                                                                             │
│  * "depends" = check can_sleep flag:                                        │
│    - can_sleep=0: MUST be atomic (no sleeping!)                             │
│    - can_sleep=1: MAY sleep (e.g., I2C/SPI access)                          │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                           INVARIANTS                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  1. Offset is ALWAYS in range [0, ngpio-1]                                  │
│     - Core validates before calling callback                                │
│                                                                             │
│  2. GPIO is ALWAYS requested before direction/get/set                       │
│     - Core ensures FLAG_REQUESTED is set                                    │
│                                                                             │
│  3. direction_output sets BOTH direction AND initial value                  │
│     - Single atomic operation expected                                      │
│                                                                             │
│  4. get() on output pin: behavior is chip-specific                          │
│     - May return actual pin value or last set value                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- `can_sleep=0` 的芯片（如 SoC GPIO）回调必须是原子的
- `can_sleep=1` 的芯片（如 I2C 扩展器）回调可以睡眠
- `to_irq()` 始终必须是原子的
- 核心层保证 `offset` 有效且 GPIO 已被请求

### 4.3 The Ops Pattern in gpio_chip

```c
/*
 * gpio_chip follows the kernel's xxx->ops->yyy(xxx, ...) pattern:
 * 
 * Consumer calls:          Core routes to:           Driver implements:
 * ──────────────────       ─────────────────         ──────────────────
 * gpio_get_value(42)  →    desc->chip->get(chip, 2)  →  pl061_get_value()
 * gpio_set_value(42,1)→    desc->chip->set(chip, 2, 1)→ pl061_set_value()
 */

/* Example from pl061 driver */
static int pl061_get_value(struct gpio_chip *gc, unsigned offset)
{
    struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
    return !!readb(chip->base + (1 << (offset + 2)));
}

static void pl061_set_value(struct gpio_chip *gc, unsigned offset, int value)
{
    struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
    writeb(!!value << offset, chip->base + (1 << (offset + 2)));
}

/* Registration wires up the ops */
chip->gc.get = pl061_get_value;
chip->gc.set = pl061_set_value;
chip->gc.direction_input = pl061_direction_input;
chip->gc.direction_output = pl061_direction_output;
```

### 4.4 How Incorrect Callbacks Break the System

```c
/* BUG 1: Sleeping in atomic context */
static int bad_get_value(struct gpio_chip *gc, unsigned offset)
{
    struct my_chip *chip = container_of(gc, struct my_chip, gc);
    
    /* [BUG] This sleeps! But can_sleep=0 was set */
    return i2c_smbus_read_byte(chip->client);  /* CRASH in IRQ context */
}

/* FIX: Set can_sleep=1 for I2C/SPI chips */
chip->gc.can_sleep = 1;

/* BUG 2: Not handling full offset range */
static int bad_direction_input(struct gpio_chip *gc, unsigned offset)
{
    /* [BUG] Only handles offsets 0-7, but ngpio=16 */
    u8 reg = readb(base + DIR_REG);
    reg &= ~(1 << offset);  /* Wrong for offset >= 8 */
    writeb(reg, base + DIR_REG);
}

/* BUG 3: Not setting value atomically with direction */
static int bad_direction_output(struct gpio_chip *gc, unsigned offset, int value)
{
    set_direction_out(offset);
    /* [BUG] Window where output has undefined value! */
    set_value(offset, value);
}

/* FIX: Set value first, then direction */
static int good_direction_output(struct gpio_chip *gc, unsigned offset, int value)
{
    set_value(offset, value);      /* Set level first */
    set_direction_out(offset);      /* Then enable output */
}
```

---

## Phase 5 — Driver Registration & Lifetime

### 5.1 How gpiochip_add() Works

```c
/* drivers/gpio/gpiolib.c */
int gpiochip_add(struct gpio_chip *chip)
{
    unsigned long   flags;
    int             status = 0;
    unsigned        id;
    int             base = chip->base;

    /* [STEP 1] Validate base and ngpio */
    if ((!gpio_is_valid(base) || !gpio_is_valid(base + chip->ngpio - 1))
            && base >= 0) {
        status = -EINVAL;
        goto fail;
    }

    spin_lock_irqsave(&gpio_lock, flags);

    /* [STEP 2] Dynamic base allocation if requested */
    if (base < 0) {
        base = gpiochip_find_base(chip->ngpio);
        if (base < 0) {
            status = base;
            goto unlock;
        }
        chip->base = base;  /* [KEY] Assign dynamic base */
    }

    /* [STEP 3] Check for conflicts */
    for (id = base; id < base + chip->ngpio; id++) {
        if (gpio_desc[id].chip != NULL) {
            status = -EBUSY;  /* [KEY] Conflict! */
            break;
        }
    }
    
    /* [STEP 4] Register in gpio_desc table */
    if (status == 0) {
        for (id = base; id < base + chip->ngpio; id++) {
            gpio_desc[id].chip = chip;  /* [KEY] Install chip pointer */
            gpio_desc[id].flags = !chip->direction_input
                ? (1 << FLAG_IS_OUT) : 0;
        }
    }

    of_gpiochip_add(chip);  /* Device Tree integration */

unlock:
    spin_unlock_irqrestore(&gpio_lock, flags);

    if (status)
        goto fail;

    /* [STEP 5] Export to sysfs */
    status = gpiochip_export(chip);
    
    return 0;
fail:
    pr_err("gpiochip_add: gpios %d..%d (%s) failed to register\n",
        chip->base, chip->base + chip->ngpio - 1, chip->label);
    return status;
}
```

### 5.2 How the Core Stores and Indexes Chips

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     gpio_desc[] ARRAY AFTER REGISTRATION                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   gpio_desc[0]   → chip = &chip_A (base=0, ngpio=32)                        │
│   gpio_desc[1]   → chip = &chip_A                                           │
│   ...                                                                       │
│   gpio_desc[31]  → chip = &chip_A                                           │
│   gpio_desc[32]  → chip = &chip_B (base=32, ngpio=16)                       │
│   ...                                                                       │
│   gpio_desc[47]  → chip = &chip_B                                           │
│   gpio_desc[48]  → chip = NULL (unassigned)                                 │
│   ...                                                                       │
│                                                                             │
│   LOOKUP:                                                                   │
│   ───────                                                                   │
│   gpio_to_chip(gpio) → gpio_desc[gpio].chip                                 │
│                                                                             │
│   This is O(1) lookup - just array indexing!                                │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 5.3 Teardown: gpiochip_remove()

```c
int gpiochip_remove(struct gpio_chip *chip)
{
    unsigned long   flags;
    int             status = 0;
    unsigned        id;

    spin_lock_irqsave(&gpio_lock, flags);

    of_gpiochip_remove(chip);

    /* [KEY] Check if any GPIO is still in use */
    for (id = chip->base; id < chip->base + chip->ngpio; id++) {
        if (test_bit(FLAG_REQUESTED, &gpio_desc[id].flags)) {
            status = -EBUSY;  /* [KEY] Can't remove! */
            break;
        }
    }
    
    /* [KEY] Only clear if nothing is in use */
    if (status == 0) {
        for (id = chip->base; id < chip->base + chip->ngpio; id++)
            gpio_desc[id].chip = NULL;
    }

    spin_unlock_irqrestore(&gpio_lock, flags);

    if (status == 0)
        gpiochip_unexport(chip);  /* Remove from sysfs */

    return status;
}
```

**Key rules:**
1. `gpiochip_remove()` **FAILS** if any GPIO is still requested
2. Driver must ensure consumers have freed all GPIOs
3. Only after successful removal can driver free its memory

### 5.4 Reference Counting and Implicit Lifetime

```c
/* gpio_request() takes module reference */
int gpio_request(unsigned gpio, const char *label)
{
    ...
    if (!try_module_get(chip->owner))  /* [KEY] Prevent module unload */
        goto done;
    
    if (test_and_set_bit(FLAG_REQUESTED, &desc->flags) == 0) {
        desc_set_label(desc, label);
        status = 0;
    } else {
        module_put(chip->owner);  /* Failed, release ref */
    }
    ...
}

/* gpio_free() releases module reference */
void gpio_free(unsigned gpio)
{
    ...
    if (chip && test_bit(FLAG_REQUESTED, &desc->flags)) {
        ...
        module_put(desc->chip->owner);  /* [KEY] Allow module unload */
        clear_bit(FLAG_REQUESTED, &desc->flags);
    }
    ...
}
```

**说明:**
- `gpio_request()` 增加模块引用计数，防止模块被卸载
- `gpio_free()` 减少引用计数
- 因此，当有 GPIO 在使用时，模块无法卸载

---

## Phase 6 — Board / Platform Integration (v3.2 Style)

### 6.1 How Platform GPIO Drivers Are Bound

```c
/* Example: ARM PrimeCell PL061 uses AMBA bus */
static struct amba_driver pl061_gpio_driver = {
    .drv = {
        .name   = "pl061_gpio",
    },
    .id_table   = pl061_ids,
    .probe      = pl061_probe,   /* Called when device matched */
};

static int __init pl061_gpio_init(void)
{
    return amba_driver_register(&pl061_gpio_driver);
}
subsys_initcall(pl061_gpio_init);

/* For platform_device style drivers */
static struct platform_driver my_gpio_driver = {
    .driver = {
        .name = "my-gpio",
        .of_match_table = my_gpio_of_match,
    },
    .probe = my_gpio_probe,
    .remove = my_gpio_remove,
};
module_platform_driver(my_gpio_driver);
```

### 6.2 Board Files vs Device Tree

```c
/* v3.2 BOARD FILE approach (arch/arm/mach-xxx/board-yyy.c) */

/* Step 1: Define platform data */
static struct pl061_platform_data gpio_pdata = {
    .gpio_base  = 0,        /* [KEY] Hardcoded base! */
    .irq_base   = IRQ_GPIO_START,
    .directions = 0x00,     /* All inputs initially */
    .values     = 0x00,
};

/* Step 2: Define device resource */
static struct amba_device gpio_device = {
    .dev = {
        .init_name = "gpio0",
        .platform_data = &gpio_pdata,
    },
    .res = {
        .start = GPIO_BASE_ADDR,
        .end   = GPIO_BASE_ADDR + 0xfff,
        .flags = IORESOURCE_MEM,
    },
    .irq = { IRQ_GPIO, NO_IRQ },
    .periphid = 0x00041061,
};

/* Step 3: Register in board init */
static void __init my_board_init(void)
{
    amba_device_register(&gpio_device, &iomem_resource);
}

/* DEVICE TREE approach (emerging in v3.2) */
/* dts file: */
gpio0: gpio@10000000 {
    compatible = "arm,pl061", "arm,primecell";
    reg = <0x10000000 0x1000>;
    interrupts = <0 42 4>;
    gpio-controller;
    #gpio-cells = <2>;
};
```

### 6.3 GPIO Number Assignment in Board Code

```c
/* Board file defines GPIO assignments */

/* arch/arm/mach-xxx/include/mach/gpio.h */
#define GPIO_LED_GREEN      0
#define GPIO_LED_RED        1
#define GPIO_BUTTON_POWER   32
#define GPIO_BUTTON_RESET   33
#define GPIO_I2C_EXPANDER   64   /* First GPIO on I2C expander */

/* Driver uses these numbers */
static int led_probe(struct platform_device *pdev)
{
    int ret;
    
    ret = gpio_request(GPIO_LED_GREEN, "led-green");
    if (ret)
        return ret;
    
    gpio_direction_output(GPIO_LED_GREEN, 0);
    return 0;
}
```

### 6.4 Why This Design Caused Long-Term Problems

| Problem | Description |
|---------|-------------|
| **Hardcoded numbers** | Board files specify exact GPIO numbers |
| **Platform-specific** | Code only works on one board |
| **Merge conflicts** | Everyone modifying same files |
| **No abstraction** | Drivers know hardware details |
| **Scaling issues** | Thousands of board files |

**Solution (later kernels):**
- Device Tree with `gpio-hog` and `gpio-ranges`
- GPIO descriptors (`struct gpio_desc *`)
- `devm_gpiod_get()` instead of `gpio_request()`
- No global GPIO numbers in drivers

---

## Phase 7 — End-to-End Data Flow

### 7.1 Complete Path: gpio_request() to Hardware Access

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    COMPLETE GPIO OPERATION FLOW                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   CONSUMER DRIVER                        GPIOLIB CORE                       │
│   ───────────────                        ────────────                       │
│                                                                             │
│   gpio_request(42, "my-led")                                                │
│          │                                                                  │
│          ▼                                                                  │
│   ┌─────────────────────────────────────────────────────────────┐           │
│   │ 1. spin_lock_irqsave(&gpio_lock)                            │           │
│   │ 2. Check gpio_is_valid(42)                                  │           │
│   │ 3. desc = &gpio_desc[42]                                    │           │
│   │ 4. chip = desc->chip     ───────► Get gpio_chip pointer     │           │
│   │ 5. try_module_get(chip->owner)   ───────► Prevent unload    │           │
│   │ 6. test_and_set_bit(FLAG_REQUESTED)                         │           │
│   │ 7. spin_unlock()                                            │           │
│   │ 8. chip->request(chip, 42-40=2)  ───────► Driver callback   │           │
│   └─────────────────────────────────────────────────────────────┘           │
│                                                   │                         │
│                                                   ▼                         │
│   ┌─────────────────────────────────────────────────────────────┐           │
│   │                     DRIVER CALLBACK                          │           │
│   │                                                              │           │
│   │ static int my_request(struct gpio_chip *gc, unsigned offset) │           │
│   │ {                                                            │           │
│   │     struct my_priv *p = container_of(gc, struct my_priv, gc);│           │
│   │     /* Enable clock, configure mux, etc. */                  │           │
│   │     return 0;                                                │           │
│   │ }                                                            │           │
│   └─────────────────────────────────────────────────────────────┘           │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   gpio_direction_output(42, 1)                                              │
│          │                                                                  │
│          ▼                                                                  │
│   ┌─────────────────────────────────────────────────────────────┐           │
│   │ 1. spin_lock_irqsave(&gpio_lock)                            │           │
│   │ 2. desc = &gpio_desc[42], chip = desc->chip                 │           │
│   │ 3. offset = 42 - chip->base = 2                             │           │
│   │ 4. Validate chip->direction_output exists                   │           │
│   │ 5. spin_unlock()                                            │           │
│   │ 6. chip->direction_output(chip, 2, 1) ───► Driver callback  │           │
│   │ 7. set_bit(FLAG_IS_OUT, &desc->flags)                       │           │
│   └─────────────────────────────────────────────────────────────┘           │
│                                                   │                         │
│                                                   ▼                         │
│   ┌─────────────────────────────────────────────────────────────┐           │
│   │                     DRIVER CALLBACK                          │           │
│   │                                                              │           │
│   │ static int pl061_direction_output(struct gpio_chip *gc,      │           │
│   │                                   unsigned offset, int value)│           │
│   │ {                                                            │           │
│   │     struct pl061_gpio *chip = container_of(gc, ...);         │           │
│   │     spin_lock_irqsave(&chip->lock, flags);                   │           │
│   │                                                              │           │
│   │     writeb(!!value << offset, chip->base + (1 << (offset+2)));│          │
│   │     gpiodir = readb(chip->base + GPIODIR);                   │           │
│   │     gpiodir |= 1 << offset;                                  │           │
│   │     writeb(gpiodir, chip->base + GPIODIR);                   │           │
│   │                                                              │           │
│   │     spin_unlock_irqrestore(&chip->lock, flags);              │           │
│   │     return 0;                                                │           │
│   │ }                                                            │           │
│   └─────────────────────────────────────────────────────────────┘           │
│                                                   │                         │
│                                                   ▼                         │
│                              ┌───────────────────────────────┐              │
│                              │        HARDWARE               │              │
│                              │  GPIO direction register set  │              │
│                              │  GPIO data register set       │              │
│                              └───────────────────────────────┘              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 7.2 gpio_set_value() Hot Path

```c
/* This is the HOT PATH - called frequently */

/* Consumer calls */
gpio_set_value(42, 1);

/* Expands to (for non-sleeping chips): */
void __gpio_set_value(unsigned gpio, int value)
{
    struct gpio_chip *chip;

    chip = gpio_to_chip(gpio);          /* [1] O(1) lookup */
    WARN_ON(chip->can_sleep);           /* [2] Verify atomic context OK */
    trace_gpio_value(gpio, 0, value);   /* [3] Tracing (optional) */
    chip->set(chip, gpio - chip->base, value);  /* [4] Call driver */
}

/* Driver callback */
static void pl061_set_value(struct gpio_chip *gc, unsigned offset, int value)
{
    struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
    
    /* [5] Direct register write - NO LOCKING for set */
    writeb(!!value << offset, chip->base + (1 << (offset + 2)));
}
```

**说明:**
- `gpio_to_chip()` 是 O(1) 数组查找
- 对于非睡眠芯片，`set()` 可以直接访问硬件，无需锁
- 驱动程序使用 `container_of` 获取私有数据

---

## Phase 8 — Concurrency, Locking, and Context

### 8.1 Where Locking Occurs in gpiolib

```c
/* Global lock protects gpio_desc[] table */
static DEFINE_SPINLOCK(gpio_lock);

/* Used when modifying gpio_desc state */
spin_lock_irqsave(&gpio_lock, flags);
    desc->chip = chip;          /* Registration */
    desc->flags = ...;          /* State change */
spin_unlock_irqrestore(&gpio_lock, flags);

/* NOTE: Lock is NOT held during callbacks! */
/* This is critical for sleeping chips (I2C, SPI) */

int gpio_request(unsigned gpio, const char *label)
{
    spin_lock_irqsave(&gpio_lock, flags);
    ...validate and set FLAG_REQUESTED...
    
    if (chip->request) {
        /* [KEY] Release lock before sleeping callback */
        spin_unlock_irqrestore(&gpio_lock, flags);
        status = chip->request(chip, gpio - chip->base);
        spin_lock_irqsave(&gpio_lock, flags);
    }
    
    spin_unlock_irqrestore(&gpio_lock, flags);
}
```

### 8.2 Callbacks and Sleeping

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   CALLBACK CONTEXT REQUIREMENTS                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   CHIP TYPE             can_sleep    CALLBACKS MAY SLEEP?                   │
│   ─────────────────────────────────────────────────────                     │
│   SoC GPIO (MMIO)          0         NO - atomic only                       │
│   I2C GPIO Expander        1         YES - I2C transfer sleeps              │
│   SPI GPIO Expander        1         YES - SPI transfer sleeps              │
│                                                                             │
│   API VARIANTS:                                                             │
│   ──────────────                                                            │
│   gpio_get_value()         │ Only for can_sleep=0 chips                     │
│   gpio_set_value()         │ WARN if called on sleeping chip                │
│                            │                                                │
│   gpio_get_value_cansleep()│ For any chip                                   │
│   gpio_set_value_cansleep()│ Calls might_sleep()                            │
│                                                                             │
│   EXAMPLE:                                                                  │
│   ─────────                                                                 │
│   /* I2C expander driver */                                                 │
│   struct pca953x_chip {                                                     │
│       struct mutex i2c_lock;    /* Protects I2C access */                   │
│       struct gpio_chip gpio_chip;                                           │
│   };                                                                        │
│                                                                             │
│   chip->gpio_chip.can_sleep = 1;  /* [KEY] Must set this! */                │
│                                                                             │
│   static int pca953x_gpio_get_value(struct gpio_chip *gc, unsigned off)     │
│   {                                                                         │
│       struct pca953x_chip *chip = container_of(...);                        │
│       mutex_lock(&chip->i2c_lock);        /* May sleep */                   │
│       i2c_smbus_read_byte_data(...);      /* May sleep */                   │
│       mutex_unlock(&chip->i2c_lock);                                        │
│       return val;                                                           │
│   }                                                                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 8.3 Atomic vs Non-Atomic GPIO Access

```c
/* ATOMIC context (IRQ handler, spinlock held, etc.) */
irqreturn_t my_irq_handler(int irq, void *data)
{
    /* OK - SoC GPIO with can_sleep=0 */
    int val = gpio_get_value(GPIO_STATUS);
    gpio_set_value(GPIO_ACK, 1);
    
    /* WRONG - I2C expander with can_sleep=1 */
    /* gpio_get_value() will WARN and may crash */
    int val = gpio_get_value(GPIO_I2C_EXPANDER_PIN);  /* BUG! */
    
    return IRQ_HANDLED;
}

/* NON-ATOMIC context (process context, work queue, etc.) */
static void my_work_func(struct work_struct *work)
{
    /* OK for any chip */
    int val = gpio_get_value_cansleep(GPIO_I2C_EXPANDER_PIN);
    gpio_set_value_cansleep(GPIO_I2C_EXPANDER_PIN, 1);
}
```

### 8.4 Why GPIO Drivers Must Be Extremely Careful

1. **Mixed contexts**: Same GPIO API called from IRQ and process context
2. **Driver callbacks must match `can_sleep`**: Set wrong → system crash
3. **Read-modify-write races**: Direction registers often need locking
4. **IRQ handlers calling GPIO**: Must verify chip is atomic-safe

```c
/* CORRECT: Driver uses its own lock for RMW operations */
static int pl061_direction_input(struct gpio_chip *gc, unsigned offset)
{
    struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
    unsigned long flags;
    unsigned char gpiodir;

    spin_lock_irqsave(&chip->lock, flags);  /* [KEY] Protect RMW */
    gpiodir = readb(chip->base + GPIODIR);
    gpiodir &= ~(1 << offset);
    writeb(gpiodir, chip->base + GPIODIR);
    spin_unlock_irqrestore(&chip->lock, flags);

    return 0;
}
```

---

## Phase 9 — Common GPIO Driver Bugs

### 9.1 Bug: Wrong Base Numbering

```c
/* BUG: Base conflicts with existing chip */
static int my_probe(...)
{
    chip->gc.base = 0;    /* [BUG] Already used by SoC GPIO! */
    chip->gc.ngpio = 8;
    
    ret = gpiochip_add(&chip->gc);
    /* Returns -EBUSY because GPIOs 0-7 are taken */
}

/* FIX: Use dynamic allocation */
chip->gc.base = -1;  /* Let gpiolib find a free range */
```

**Symptoms:**
- `gpiochip_add()` returns `-EBUSY`
- System boot fails if essential GPIO chip can't register

### 9.2 Bug: Sleeping in Atomic Context

```c
/* BUG: I2C access in callback but can_sleep=0 */
static int bad_get_value(struct gpio_chip *gc, unsigned offset)
{
    struct my_chip *chip = container_of(gc, ...);
    
    /* [BUG] i2c_smbus_read_byte_data() may sleep! */
    return i2c_smbus_read_byte_data(chip->client, REG_INPUT);
}

static int my_probe(...)
{
    chip->gc.can_sleep = 0;  /* [BUG] WRONG! */
    chip->gc.get = bad_get_value;
}

/* Consumer in IRQ handler */
irq_handler(...) {
    gpio_get_value(gpio);  /* System crashes here */
}
```

**Symptoms:**
- `BUG: scheduling while atomic`
- Kernel panic in IRQ context
- Random deadlocks

**FIX:**
```c
chip->gc.can_sleep = 1;  /* Set correctly */
```

### 9.3 Bug: Missing Direction Handling

```c
/* BUG: direction_output doesn't set value atomically */
static int bad_direction_output(struct gpio_chip *gc, unsigned offset, int value)
{
    /* Set direction first */
    u8 dir = readb(base + DIR_REG);
    dir |= (1 << offset);
    writeb(dir, base + DIR_REG);
    
    /* [BUG] GPIO is now output with UNDEFINED value! */
    /* Race window here */
    
    /* Then set value */
    u8 val = readb(base + VAL_REG);
    if (value)
        val |= (1 << offset);
    else
        val &= ~(1 << offset);
    writeb(val, base + VAL_REG);
}
```

**Symptoms:**
- GPIO glitches on transition to output
- Connected device sees spurious pulse

**FIX:**
```c
static int good_direction_output(struct gpio_chip *gc, unsigned offset, int value)
{
    /* Set value FIRST */
    u8 val = readb(base + VAL_REG);
    if (value)
        val |= (1 << offset);
    else
        val &= ~(1 << offset);
    writeb(val, base + VAL_REG);
    
    /* THEN enable output */
    u8 dir = readb(base + DIR_REG);
    dir |= (1 << offset);
    writeb(dir, base + DIR_REG);
}
```

### 9.4 Bug: Double Registration

```c
/* BUG: Registering same chip twice */
static int my_probe(...)
{
    ret = gpiochip_add(&chip->gc);
    if (ret)
        return ret;
    
    /* ... some error ... */
    
    ret = gpiochip_add(&chip->gc);  /* [BUG] Already registered! */
}
```

**Symptoms:**
- `-EBUSY` error
- System instability if error ignored

### 9.5 Bug: Lifetime Mismatches

```c
/* BUG: Freeing chip while GPIOs still in use */
static int my_remove(...)
{
    /* Some consumer driver still has gpio_request() active! */
    
    gpiochip_remove(&chip->gc);  /* Returns -EBUSY, ignored */
    kfree(chip);                  /* [BUG] Use-after-free! */
}

/* Later, consumer calls */
gpio_set_value(42, 1);  /* Accesses freed memory → crash */
```

**Symptoms:**
- Kernel oops with invalid memory access
- Random corruption

**FIX:**
```c
static int my_remove(...)
{
    int ret = gpiochip_remove(&chip->gc);
    if (ret) {
        dev_err(dev, "GPIOs still in use!\n");
        return ret;  /* Don't free! */
    }
    kfree(chip);
    return 0;
}
```

---

## Phase 10 — Architecture Lessons

### 10.1 What Makes a GOOD GPIO Driver

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      GOOD GPIO DRIVER CHECKLIST                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ✓ CORRECT can_sleep SETTING                                               │
│     • Set to 1 if any callback might sleep (I2C, SPI)                       │
│     • Set to 0 only for memory-mapped registers                             │
│                                                                             │
│   ✓ ATOMIC direction_output                                                 │
│     • Set value BEFORE enabling output                                      │
│     • Use hardware's combined write if available                            │
│                                                                             │
│   ✓ PROPER LOCKING                                                          │
│     • spinlock for RMW on SoC chips                                         │
│     • mutex for I2C/SPI chips                                               │
│     • Never hold lock across sleeping operation                             │
│                                                                             │
│   ✓ CORRECT BASE HANDLING                                                   │
│     • Use base=-1 for dynamic allocation (preferred)                        │
│     • Or use platform-specified base                                        │
│                                                                             │
│   ✓ COMPLETE CALLBACK SET                                                   │
│     • direction_input, direction_output (mandatory)                         │
│     • get, set (mandatory)                                                  │
│     • request, free (if chip needs activation)                              │
│     • to_irq (if GPIO can trigger interrupts)                               │
│                                                                             │
│   ✓ PROPER OWNERSHIP                                                        │
│     • Set owner = THIS_MODULE                                               │
│     • Set dev = parent device                                               │
│     • Set label for debugging                                               │
│                                                                             │
│   ✓ CORRECT TEARDOWN                                                        │
│     • gpiochip_remove() before freeing struct                               │
│     • Handle -EBUSY return (GPIOs still in use)                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 10.2 How gpiolib Enforces Abstraction

| Mechanism | What It Enforces |
|-----------|------------------|
| `gpio_request()` | Exclusive access, tracks usage |
| `gpio_chip` callbacks | Uniform interface for all hardware |
| `can_sleep` flag | Context-appropriate API usage |
| `base` + `offset` | Location-independent GPIO access |
| `gpiochip_remove()` check | Prevents use-after-free |
| `try_module_get()` | Module lifetime safety |

### 10.3 How Ops-Based Design Enables Scalability

```c
/*
 * gpio_chip IS the ops pattern:
 * 
 * struct gpio_chip {
 *     int (*direction_input)(struct gpio_chip *, unsigned);
 *     int (*get)(struct gpio_chip *, unsigned);
 *     void (*set)(struct gpio_chip *, unsigned, int);
 *     ...
 * };
 * 
 * Each controller driver provides its own implementation.
 * Core code knows nothing about hardware.
 */

/* Core code is GENERIC: */
void __gpio_set_value(unsigned gpio, int value)
{
    struct gpio_chip *chip = gpio_to_chip(gpio);
    chip->set(chip, gpio - chip->base, value);  /* Dispatch */
}

/* Driver code is SPECIFIC: */
/* SoC GPIO: MMIO write */
static void pl061_set_value(struct gpio_chip *gc, unsigned offset, int value)
{
    writeb(!!value << offset, chip->base + (1 << (offset + 2)));
}

/* I2C expander: I2C transaction */
static void pca953x_set_value(struct gpio_chip *gc, unsigned offset, int value)
{
    i2c_smbus_write_byte_data(chip->client, REG_OUTPUT, val);
}
```

### 10.4 Generalizing to Other Subsystems

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    PATTERN: SUBSYSTEM WITH PLUGGABLE DRIVERS                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   COMPONENT           GPIO EXAMPLE           GENERAL PATTERN                │
│   ─────────────────────────────────────────────────────────────             │
│   Core library        gpiolib.c              Framework code                 │
│   Controller object   struct gpio_chip       struct xxx_controller          │
│   Ops callbacks       get, set, ...          Operation function pointers    │
│   Registration        gpiochip_add()         xxx_register()                 │
│   Consumer API        gpio_get_value()       xxx_do_operation()             │
│   Resource tracking   FLAG_REQUESTED         Refcounts, bitmaps             │
│   Private data        container_of pattern   Embedded struct or drvdata     │
│                                                                             │
│   OTHER KERNEL EXAMPLES:                                                    │
│   ──────────────────────                                                    │
│   • block_device_operations  (block layer)                                  │
│   • file_operations          (VFS)                                          │
│   • net_device_ops           (networking)                                   │
│   • tty_operations           (TTY layer)                                    │
│   • regmap                   (register abstraction)                         │
│                                                                             │
│   USER-SPACE DRIVER FRAMEWORKS:                                             │
│   ──────────────────────────────                                            │
│   • libusb: Function pointers for backend (Linux/BSD/Windows)               │
│   • FUSE: ops struct for filesystem callbacks                               │
│   • Plugin architectures: ops = vtable                                      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 10.5 Complete GPIO Driver Template

```c
/*
 * Minimal GPIO driver template following best practices
 */

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

struct my_gpio {
    struct gpio_chip    gc;
    spinlock_t          lock;       /* For RMW operations */
    void __iomem        *base;
};

static int my_direction_input(struct gpio_chip *gc, unsigned offset)
{
    struct my_gpio *chip = container_of(gc, struct my_gpio, gc);
    unsigned long flags;
    u32 reg;

    spin_lock_irqsave(&chip->lock, flags);
    reg = readl(chip->base + DIR_REG);
    reg &= ~(1 << offset);          /* Clear = input */
    writel(reg, chip->base + DIR_REG);
    spin_unlock_irqrestore(&chip->lock, flags);

    return 0;
}

static int my_direction_output(struct gpio_chip *gc, unsigned offset, int value)
{
    struct my_gpio *chip = container_of(gc, struct my_gpio, gc);
    unsigned long flags;
    u32 reg;

    spin_lock_irqsave(&chip->lock, flags);
    
    /* [KEY] Set value FIRST */
    reg = readl(chip->base + DATA_REG);
    if (value)
        reg |= (1 << offset);
    else
        reg &= ~(1 << offset);
    writel(reg, chip->base + DATA_REG);
    
    /* THEN set direction */
    reg = readl(chip->base + DIR_REG);
    reg |= (1 << offset);           /* Set = output */
    writel(reg, chip->base + DIR_REG);
    
    spin_unlock_irqrestore(&chip->lock, flags);
    return 0;
}

static int my_get_value(struct gpio_chip *gc, unsigned offset)
{
    struct my_gpio *chip = container_of(gc, struct my_gpio, gc);
    return !!(readl(chip->base + DATA_REG) & (1 << offset));
}

static void my_set_value(struct gpio_chip *gc, unsigned offset, int value)
{
    struct my_gpio *chip = container_of(gc, struct my_gpio, gc);
    unsigned long flags;
    u32 reg;

    spin_lock_irqsave(&chip->lock, flags);
    reg = readl(chip->base + DATA_REG);
    if (value)
        reg |= (1 << offset);
    else
        reg &= ~(1 << offset);
    writel(reg, chip->base + DATA_REG);
    spin_unlock_irqrestore(&chip->lock, flags);
}

static int my_gpio_probe(struct platform_device *pdev)
{
    struct my_gpio *chip;
    struct resource *res;
    int ret;

    chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
    if (!chip)
        return -ENOMEM;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    chip->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(chip->base))
        return PTR_ERR(chip->base);

    spin_lock_init(&chip->lock);

    chip->gc.label = dev_name(&pdev->dev);
    chip->gc.dev = &pdev->dev;
    chip->gc.owner = THIS_MODULE;
    chip->gc.base = -1;             /* [KEY] Dynamic allocation */
    chip->gc.ngpio = 32;
    chip->gc.can_sleep = 0;         /* [KEY] MMIO = atomic */
    chip->gc.direction_input = my_direction_input;
    chip->gc.direction_output = my_direction_output;
    chip->gc.get = my_get_value;
    chip->gc.set = my_set_value;

    platform_set_drvdata(pdev, chip);

    ret = gpiochip_add(&chip->gc);
    if (ret) {
        dev_err(&pdev->dev, "Failed to register GPIO chip\n");
        return ret;
    }

    dev_info(&pdev->dev, "Registered %d GPIOs starting at %d\n",
             chip->gc.ngpio, chip->gc.base);
    return 0;
}

static int my_gpio_remove(struct platform_device *pdev)
{
    struct my_gpio *chip = platform_get_drvdata(pdev);
    int ret;

    ret = gpiochip_remove(&chip->gc);
    if (ret) {
        dev_err(&pdev->dev, "GPIOs still in use!\n");
        return ret;
    }
    return 0;
}

static struct platform_driver my_gpio_driver = {
    .driver = {
        .name = "my-gpio",
    },
    .probe = my_gpio_probe,
    .remove = my_gpio_remove,
};
module_platform_driver(my_gpio_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Example GPIO Controller Driver");
```

**说明:**
- 使用 `container_of` 获取私有数据
- `can_sleep = 0` 因为是 MMIO 访问
- `base = -1` 使用动态分配
- `direction_output` 先设置值，后设置方向
- 使用自旋锁保护读-修改-写操作
- `gpiochip_remove()` 在释放内存前调用

---

## Appendix A — Walking Through Real GPIO Controller Drivers

This section provides a detailed walkthrough of two real GPIO controller drivers from Linux kernel v3.2, demonstrating how the architectural principles translate into actual code.

### A.1 Driver Comparison Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    TWO GPIO DRIVERS: ARCHITECTURAL COMPARISON               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   DRIVER 1: gpio-pl061.c                   DRIVER 2: gpio-pca953x.c         │
│   ──────────────────────                   ─────────────────────────        │
│   Hardware: ARM PrimeCell PL061            Hardware: PCA953x I2C Expander   │
│   Bus: Memory-mapped (AMBA)                Bus: I2C                         │
│   Access: Atomic (MMIO)                    Access: May sleep (I2C xfer)     │
│   Lock: spinlock                           Lock: mutex                       │
│   can_sleep: 0                             can_sleep: 1                     │
│   GPIOs: 8 per chip                        GPIOs: 4/8/16 per chip           │
│                                                                             │
│   COMMON PATTERNS:                                                          │
│   ────────────────                                                          │
│   ✓ Both embed gpio_chip in private struct                                 │
│   ✓ Both use container_of() for private data recovery                      │
│   ✓ Both implement same callback set                                        │
│   ✓ Both follow probe/remove lifecycle                                      │
│   ✓ Both support IRQ (GPIO as interrupt source)                             │
│                                                                             │
│   KEY DIFFERENCES:                                                          │
│   ────────────────                                                          │
│   PL061: Direct register read/write        PCA953x: I2C bus transactions   │
│   PL061: No blocking in callbacks          PCA953x: mutex + I2C may block  │
│   PL061: IRQ disabled in callbacks OK      PCA953x: Process context only   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 两个驱动展示了同一抽象(`gpio_chip`)如何适应不同硬件
- PL061 是典型的 SoC GPIO（快速，原子访问）
- PCA953x 是 I2C 扩展器（需要总线事务，可能睡眠）
- 尽管硬件差异很大，代码结构惊人地相似

---

### A.2 Driver 1: PL061 (SoC Memory-Mapped GPIO)

#### A.2.1 Hardware Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         PL061 HARDWARE ARCHITECTURE                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   CPU                                                                       │
│    │                                                                        │
│    │ AMBA Bus (APB)                                                         │
│    ▼                                                                        │
│   ┌─────────────────────────────────────────┐                               │
│   │         PL061 GPIO Controller           │                               │
│   ├─────────────────────────────────────────┤                               │
│   │  GPIODATA [0x000-0x3FC]  Data register  │ ◄── Bit-banded access!       │
│   │  GPIODIR  [0x400]        Direction reg  │                               │
│   │  GPIOIS   [0x404]        IRQ sense      │                               │
│   │  GPIOIBE  [0x408]        Both edges     │                               │
│   │  GPIOIEV  [0x40C]        Event value    │                               │
│   │  GPIOIE   [0x410]        IRQ enable     │                               │
│   │  GPIORIS  [0x414]        Raw IRQ status │                               │
│   │  GPIOMIS  [0x418]        Masked IRQ     │                               │
│   │  GPIOIC   [0x41C]        IRQ clear      │                               │
│   └─────────────────────────────────────────┘                               │
│            │ │ │ │ │ │ │ │                                                  │
│            ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼                                                  │
│           GPIO[0] ... GPIO[7]   (8 pins per controller)                     │
│                                                                             │
│   SPECIAL: Bit-banded data access                                           │
│   ─────────────────────────────                                             │
│   Address = base + (1 << (offset + 2))                                      │
│   GPIO 0: 0x004, GPIO 1: 0x008, ... GPIO 7: 0x200                           │
│   This allows atomic single-bit operations!                                 │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- PL061 是 ARM PrimeCell 系列的 GPIO 控制器
- 使用"位带"（bit-banding）技术，每个 GPIO 有独立地址
- 支持边沿和电平触发中断

#### A.2.2 Private Data Structure

```c
/* drivers/gpio/gpio-pl061.c */

/* [KEY] Hardware register offsets */
#define GPIODIR 0x400    /* Direction: 0=input, 1=output */
#define GPIOIS  0x404    /* Interrupt sense: 0=edge, 1=level */
#define GPIOIBE 0x408    /* Both edges: 1=both edges trigger */
#define GPIOIEV 0x40C    /* Event value: rising/high vs falling/low */
#define GPIOIE  0x410    /* Interrupt enable */
#define GPIORIS 0x414    /* Raw interrupt status */
#define GPIOMIS 0x418    /* Masked interrupt status */
#define GPIOIC  0x41C    /* Interrupt clear */

#define PL061_GPIO_NR   8    /* [KEY] Always 8 GPIOs per PL061 */

/* [KEY] Private driver structure */
struct pl061_gpio {
    /* For systems with multiple PL061s sharing one IRQ */
    struct list_head    list;           /* [1] IRQ handler chain */

    /* [KEY] Two separate locks for decoupling */
    spinlock_t          lock;           /* [2] Protects GPIO registers */
    spinlock_t          irq_lock;       /* [3] Protects IRQ registers */

    void __iomem        *base;          /* [4] MMIO base address */
    unsigned            irq_base;       /* [5] First IRQ number */
    struct gpio_chip    gc;             /* [6] Embedded gpio_chip */
};
```

**Memory Layout:**
```
┌─────────────────────────────────────────┐
│           struct pl061_gpio             │
├─────────────────────────────────────────┤
│  list_head list     [16 bytes]          │ ◄── For IRQ chaining
├─────────────────────────────────────────┤
│  spinlock_t lock    [4 bytes]           │ ◄── GPIO register lock
├─────────────────────────────────────────┤
│  spinlock_t irq_lock [4 bytes]          │ ◄── IRQ register lock
├─────────────────────────────────────────┤
│  void __iomem *base [4/8 bytes]         │ ◄── MMIO pointer
├─────────────────────────────────────────┤
│  unsigned irq_base  [4 bytes]           │ ◄── IRQ number base
├─────────────────────────────────────────┤
│  struct gpio_chip gc [~120 bytes]       │ ◄── EMBEDDED (container_of!)
│    ├── label                            │
│    ├── base                             │
│    ├── ngpio                            │
│    ├── direction_input  ─────┐          │
│    ├── direction_output      │          │
│    ├── get              ◄────┼── Callbacks to pl061_*
│    ├── set                   │          │
│    └── to_irq ───────────────┘          │
└─────────────────────────────────────────┘
```

**说明:**
- 驱动结构体包含所有硬件状态和锁
- `gpio_chip` 嵌入在结构体末尾
- 两个独立的自旋锁：GPIO 操作和 IRQ 操作分离

#### A.2.3 Callback Implementation Analysis

**Direction Input:**

```c
static int pl061_direction_input(struct gpio_chip *gc, unsigned offset)
{
    /* [STEP 1] Recover private data using container_of */
    struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
    unsigned long flags;
    unsigned char gpiodir;

    /* [STEP 2] Validate offset (defensive, core should already check) */
    if (offset >= gc->ngpio)
        return -EINVAL;

    /* [STEP 3] Lock for read-modify-write sequence */
    spin_lock_irqsave(&chip->lock, flags);
    
    /* [STEP 4] Read current direction register */
    gpiodir = readb(chip->base + GPIODIR);
    
    /* [STEP 5] Clear bit = input (PL061 convention) */
    gpiodir &= ~(1 << offset);
    
    /* [STEP 6] Write back */
    writeb(gpiodir, chip->base + GPIODIR);
    
    spin_unlock_irqrestore(&chip->lock, flags);

    return 0;
}
```

**说明:**
- `container_of` 从 `gpio_chip` 指针恢复完整的驱动结构体
- 必须使用锁保护读-修改-写序列
- 使用 `spin_lock_irqsave` 因为可能从中断上下文调用

**Direction Output (CRITICAL: Value-Before-Direction):**

```c
static int pl061_direction_output(struct gpio_chip *gc, unsigned offset,
        int value)
{
    struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
    unsigned long flags;
    unsigned char gpiodir;

    if (offset >= gc->ngpio)
        return -EINVAL;

    spin_lock_irqsave(&chip->lock, flags);
    
    /* [KEY STEP A] Set value FIRST via bit-banded address */
    writeb(!!value << offset, chip->base + (1 << (offset + 2)));
    
    /* [STEP B] Then set direction to output */
    gpiodir = readb(chip->base + GPIODIR);
    gpiodir |= 1 << offset;      /* Set bit = output */
    writeb(gpiodir, chip->base + GPIODIR);

    /*
     * [STEP C] Set value AGAIN!
     * PL061 quirk: value written while in input mode may be lost.
     * The datasheet notes this behavior.
     */
    writeb(!!value << offset, chip->base + (1 << (offset + 2)));
    
    spin_unlock_irqrestore(&chip->lock, flags);

    return 0;
}
```

**说明:**
- 关键：先设置值，再设置方向，避免毛刺
- PL061 硬件怪癖：值需要写两次
- 整个操作在锁内完成，保证原子性

**Get/Set Value (Bit-Banded Access):**

```c
/* [KEY] Bit-banded read - no lock needed for single-bit atomic operation */
static int pl061_get_value(struct gpio_chip *gc, unsigned offset)
{
    struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
    
    /* Address formula: base + (1 << (offset + 2))
     * This reads only the bit for 'offset', atomically.
     */
    return !!readb(chip->base + (1 << (offset + 2)));
}

/* [KEY] Bit-banded write - no lock needed! */
static void pl061_set_value(struct gpio_chip *gc, unsigned offset, int value)
{
    struct pl061_gpio *chip = container_of(gc, struct pl061_gpio, gc);
    
    /* Single atomic write to bit-specific address */
    writeb(!!value << offset, chip->base + (1 << (offset + 2)));
}
```

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    PL061 BIT-BANDED ADDRESS CALCULATION                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   FORMULA: address = base + (1 << (offset + 2))                             │
│                                                                             │
│   OFFSET    CALCULATION         ADDRESS     MASK                            │
│   ──────    ───────────         ───────     ────                            │
│   0         1 << 2 = 4          base+0x004  bit 0                           │
│   1         1 << 3 = 8          base+0x008  bit 1                           │
│   2         1 << 4 = 16         base+0x010  bit 2                           │
│   3         1 << 5 = 32         base+0x020  bit 3                           │
│   4         1 << 6 = 64         base+0x040  bit 4                           │
│   5         1 << 7 = 128        base+0x080  bit 5                           │
│   6         1 << 8 = 256        base+0x100  bit 6                           │
│   7         1 << 9 = 512        base+0x200  bit 7                           │
│                                                                             │
│   WHY THIS WORKS:                                                           │
│   ────────────────                                                          │
│   PL061 uses address bits [9:2] as a MASK for the data register.            │
│   Reading from base+0x008 returns only bit 1.                               │
│   Writing to base+0x008 only affects bit 1.                                 │
│                                                                             │
│   BENEFIT: Atomic single-bit operations without read-modify-write!          │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- PL061 使用地址位作为掩码，实现硬件级原子操作
- 因此 `get` 和 `set` 不需要锁
- 这是 ARM PrimeCell 的聪明设计

#### A.2.4 Probe Function Walkthrough

```c
static int pl061_probe(struct amba_device *dev, const struct amba_id *id)
{
    struct pl061_platform_data *pdata;
    struct pl061_gpio *chip;
    int ret;

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 1: Allocate driver private structure                           */
    /* ═══════════════════════════════════════════════════════════════════ */
    chip = kzalloc(sizeof(*chip), GFP_KERNEL);
    if (chip == NULL)
        return -ENOMEM;

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 2: Get platform data (GPIO base, IRQ base, initial config)     */
    /* ═══════════════════════════════════════════════════════════════════ */
    pdata = dev->dev.platform_data;
    if (pdata) {
        chip->gc.base = pdata->gpio_base;   /* Platform-specified base */
        chip->irq_base = pdata->irq_base;
    } else if (dev->dev.of_node) {
        chip->gc.base = -1;                  /* Dynamic allocation */
        chip->irq_base = NO_IRQ;
    } else {
        ret = -ENODEV;
        goto free_mem;
    }

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 3: Request and map hardware resources                          */
    /* ═══════════════════════════════════════════════════════════════════ */
    if (!request_mem_region(dev->res.start,
                resource_size(&dev->res), "pl061")) {
        ret = -EBUSY;
        goto free_mem;
    }

    chip->base = ioremap(dev->res.start, resource_size(&dev->res));
    if (chip->base == NULL) {
        ret = -ENOMEM;
        goto release_region;
    }

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 4: Initialize locks and lists                                  */
    /* ═══════════════════════════════════════════════════════════════════ */
    spin_lock_init(&chip->lock);
    spin_lock_init(&chip->irq_lock);
    INIT_LIST_HEAD(&chip->list);

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 5: Configure gpio_chip structure                               */
    /* ═══════════════════════════════════════════════════════════════════ */
    chip->gc.direction_input  = pl061_direction_input;   /* [KEY] Wire callbacks */
    chip->gc.direction_output = pl061_direction_output;
    chip->gc.get              = pl061_get_value;
    chip->gc.set              = pl061_set_value;
    chip->gc.to_irq           = pl061_to_irq;
    chip->gc.ngpio            = PL061_GPIO_NR;           /* Always 8 */
    chip->gc.label            = dev_name(&dev->dev);
    chip->gc.dev              = &dev->dev;
    chip->gc.owner            = THIS_MODULE;
    /* NOTE: can_sleep is NOT set, defaults to 0 (atomic) */

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 6: Register with gpiolib                                       */
    /* ═══════════════════════════════════════════════════════════════════ */
    ret = gpiochip_add(&chip->gc);
    if (ret)
        goto iounmap;

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 7: Setup IRQ handling (if enabled)                             */
    /* ═══════════════════════════════════════════════════════════════════ */
    if (chip->irq_base != NO_IRQ) {
        writeb(0, chip->base + GPIOIE);  /* Disable all IRQs initially */
        /* ... IRQ setup code ... */
    }

    return 0;

    /* Error cleanup path */
iounmap:
    iounmap(chip->base);
release_region:
    release_mem_region(dev->res.start, resource_size(&dev->res));
free_mem:
    kfree(chip);
    return ret;
}
```

**说明:**
- 清晰的分步初始化流程
- 资源获取按顺序进行
- 错误路径释放所有已获取的资源
- `gpiochip_add()` 是关键的注册步骤

---

### A.3 Driver 2: PCA953x (I2C GPIO Expander)

#### A.3.1 Hardware Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      PCA953x HARDWARE ARCHITECTURE                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   CPU                                                                       │
│    │                                                                        │
│    │                                                                        │
│    ▼                                                                        │
│   ┌──────────────┐     I2C Bus     ┌─────────────────────────────┐          │
│   │ I2C Master   │─────────────────│    PCA9535 I2C Expander     │          │
│   │ (SoC)        │   SCL / SDA     │                             │          │
│   └──────────────┘                 │  ┌──────────────────────┐   │          │
│                                    │  │ INPUT  reg (0x00/01) │   │          │
│                                    │  │ OUTPUT reg (0x02/03) │   │          │
│                                    │  │ INVERT reg (0x04/05) │   │          │
│                                    │  │ CONFIG reg (0x06/07) │   │          │
│                                    │  └──────────────────────┘   │          │
│                                    │            │                │          │
│                                    │    ┌───────┴───────┐        │          │
│                                    │    │ IO Expander   │        │          │
│                                    │    │ P0.0 ... P1.7 │        │          │
│                                    │    └───────────────┘        │          │
│                                    └─────────────────────────────┘          │
│                                              │ │ │ │ │ │ │ │                │
│                                              ▼ ▼ ▼ ▼ ▼ ▼ ▼ ▼                │
│                                             16 GPIO pins                    │
│                                                                             │
│   REGISTER ACCESS:                                                          │
│   ─────────────────                                                         │
│   Read GPIO 5:  I2C read from reg 0x00, check bit 5                        │
│   Write GPIO 5: I2C read reg 0x02, modify bit 5, I2C write back            │
│                                                                             │
│   TIMING:                                                                   │
│   ────────                                                                  │
│   I2C transaction: 100-400 kHz clock                                        │
│   Single byte: ~100-200 microseconds                                        │
│   → CANNOT be used in atomic context!                                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- PCA953x 是 I2C 总线上的 GPIO 扩展器
- 所有操作需要 I2C 事务，耗时较长
- 因此必须设置 `can_sleep = 1`

#### A.3.2 Private Data Structure

```c
/* drivers/gpio/gpio-pca953x.c */

/* Device variants (encoded in id_table) */
#define PCA_GPIO_MASK   0x00FF    /* Number of GPIOs */
#define PCA_INT         0x0100    /* Has interrupt support */
#define PCA953X_TYPE    0x1000    /* PCA953x register layout */
#define PCA957X_TYPE    0x2000    /* PCA957x register layout */

/* [KEY] Private driver structure */
struct pca953x_chip {
    unsigned gpio_start;          /* [1] GPIO base number */
    uint16_t reg_output;          /* [2] Cached output register */
    uint16_t reg_direction;       /* [3] Cached direction register */
    struct mutex i2c_lock;        /* [4] Protects I2C access */

#ifdef CONFIG_GPIO_PCA953X_IRQ
    struct mutex irq_lock;        /* IRQ configuration lock */
    uint16_t irq_mask;            /* Which GPIOs can generate IRQs */
    uint16_t irq_stat;            /* Last read input state */
    uint16_t irq_trig_raise;      /* Trigger on rising edge */
    uint16_t irq_trig_fall;       /* Trigger on falling edge */
    int irq_base;                 /* First IRQ number */
#endif

    struct i2c_client *client;    /* [5] I2C client handle */
    struct gpio_chip gpio_chip;   /* [6] Embedded gpio_chip */
    const char *const *names;     /* [7] GPIO names for debugfs */
    int chip_type;                /* [8] PCA953X_TYPE or PCA957X_TYPE */
};
```

**Key Differences from PL061:**

| Aspect | PL061 | PCA953x |
|--------|-------|---------|
| Lock type | spinlock | mutex |
| Register cache | None (MMIO is fast) | Yes (avoid I2C reads) |
| Bus handle | void __iomem *base | struct i2c_client * |
| Chip variants | One type | Multiple (953x, 957x) |

#### A.3.3 I2C Register Access

```c
/* [KEY] Write to PCA953x register via I2C */
static int pca953x_write_reg(struct pca953x_chip *chip, int reg, uint16_t val)
{
    int ret = 0;

    if (chip->gpio_chip.ngpio <= 8) {
        /* 8-bit chips: single byte write */
        ret = i2c_smbus_write_byte_data(chip->client, reg, val);
    } else {
        /* 16-bit chips: word write */
        switch (chip->chip_type) {
        case PCA953X_TYPE:
            /* PCA953x: reg*2 is port 0, reg*2+1 is port 1 */
            ret = i2c_smbus_write_word_data(chip->client, reg << 1, val);
            break;
        case PCA957X_TYPE:
            /* PCA957x: must write bytes separately */
            ret = i2c_smbus_write_byte_data(chip->client, 
                                            reg << 1, val & 0xff);
            if (ret < 0)
                break;
            ret = i2c_smbus_write_byte_data(chip->client,
                                            (reg << 1) + 1, 
                                            (val & 0xff00) >> 8);
            break;
        }
    }

    if (ret < 0) {
        dev_err(&chip->client->dev, "failed writing register\n");
        return ret;
    }
    return 0;
}

/* [KEY] Read from PCA953x register via I2C */
static int pca953x_read_reg(struct pca953x_chip *chip, int reg, uint16_t *val)
{
    int ret;

    if (chip->gpio_chip.ngpio <= 8)
        ret = i2c_smbus_read_byte_data(chip->client, reg);
    else
        ret = i2c_smbus_read_word_data(chip->client, reg << 1);

    if (ret < 0) {
        dev_err(&chip->client->dev, "failed reading register\n");
        return ret;
    }

    *val = (uint16_t)ret;
    return 0;
}
```

**说明:**
- I2C 访问需要多个函数调用
- 支持不同芯片变体的寄存器布局
- 这些函数会睡眠！

#### A.3.4 Callback Implementation (Sleeping Allowed)

**Direction Input:**

```c
static int pca953x_gpio_direction_input(struct gpio_chip *gc, unsigned off)
{
    struct pca953x_chip *chip;
    uint16_t reg_val;
    int ret, offset = 0;

    /* [STEP 1] Recover private data */
    chip = container_of(gc, struct pca953x_chip, gpio_chip);

    /* [STEP 2] Lock with MUTEX (not spinlock!) */
    mutex_lock(&chip->i2c_lock);
    
    /* [STEP 3] Use cached direction, set bit = input */
    reg_val = chip->reg_direction | (1u << off);

    /* [STEP 4] Select correct register based on chip type */
    switch (chip->chip_type) {
    case PCA953X_TYPE:
        offset = PCA953X_DIRECTION;
        break;
    case PCA957X_TYPE:
        offset = PCA957X_CFG;
        break;
    }
    
    /* [STEP 5] Write via I2C (MAY SLEEP!) */
    ret = pca953x_write_reg(chip, offset, reg_val);
    if (ret)
        goto exit;

    /* [STEP 6] Update cache on success */
    chip->reg_direction = reg_val;
    ret = 0;
    
exit:
    mutex_unlock(&chip->i2c_lock);
    return ret;
}
```

**说明:**
- 使用 `mutex_lock`，允许睡眠
- 使用寄存器缓存避免额外的 I2C 读取
- 必须处理 I2C 错误

**Direction Output (Correct Order):**

```c
static int pca953x_gpio_direction_output(struct gpio_chip *gc,
        unsigned off, int val)
{
    struct pca953x_chip *chip;
    uint16_t reg_val;
    int ret, offset = 0;

    chip = container_of(gc, struct pca953x_chip, gpio_chip);

    mutex_lock(&chip->i2c_lock);
    
    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP A: Set output level FIRST (same as PL061!)                     */
    /* ═══════════════════════════════════════════════════════════════════ */
    if (val)
        reg_val = chip->reg_output | (1u << off);
    else
        reg_val = chip->reg_output & ~(1u << off);

    switch (chip->chip_type) {
    case PCA953X_TYPE:
        offset = PCA953X_OUTPUT;
        break;
    case PCA957X_TYPE:
        offset = PCA957X_OUT;
        break;
    }
    
    ret = pca953x_write_reg(chip, offset, reg_val);
    if (ret)
        goto exit;

    chip->reg_output = reg_val;   /* Update cache */

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP B: THEN set direction to output                                */
    /* ═══════════════════════════════════════════════════════════════════ */
    reg_val = chip->reg_direction & ~(1u << off);  /* Clear = output */
    
    switch (chip->chip_type) {
    case PCA953X_TYPE:
        offset = PCA953X_DIRECTION;
        break;
    case PCA957X_TYPE:
        offset = PCA957X_CFG;
        break;
    }
    
    ret = pca953x_write_reg(chip, offset, reg_val);
    if (ret)
        goto exit;

    chip->reg_direction = reg_val;
    ret = 0;
    
exit:
    mutex_unlock(&chip->i2c_lock);
    return ret;
}
```

**说明:**
- 同样遵循"先设值，后设方向"原则
- 两次 I2C 写操作
- 缓存成功写入的值

**Get/Set Value:**

```c
static int pca953x_gpio_get_value(struct gpio_chip *gc, unsigned off)
{
    struct pca953x_chip *chip;
    uint16_t reg_val;
    int ret, offset = 0;

    chip = container_of(gc, struct pca953x_chip, gpio_chip);

    mutex_lock(&chip->i2c_lock);
    
    /* Read INPUT register (actual pin state) */
    switch (chip->chip_type) {
    case PCA953X_TYPE:
        offset = PCA953X_INPUT;
        break;
    case PCA957X_TYPE:
        offset = PCA957X_IN;
        break;
    }
    
    ret = pca953x_read_reg(chip, offset, &reg_val);
    mutex_unlock(&chip->i2c_lock);
    
    if (ret < 0) {
        /* Can't report error! Return 0 and log */
        return 0;
    }

    return (reg_val & (1u << off)) ? 1 : 0;
}

static void pca953x_gpio_set_value(struct gpio_chip *gc, unsigned off, int val)
{
    struct pca953x_chip *chip;
    uint16_t reg_val;
    int ret, offset = 0;

    chip = container_of(gc, struct pca953x_chip, gpio_chip);

    mutex_lock(&chip->i2c_lock);
    
    /* Update cached output value */
    if (val)
        reg_val = chip->reg_output | (1u << off);
    else
        reg_val = chip->reg_output & ~(1u << off);

    switch (chip->chip_type) {
    case PCA953X_TYPE:
        offset = PCA953X_OUTPUT;
        break;
    case PCA957X_TYPE:
        offset = PCA957X_OUT;
        break;
    }
    
    ret = pca953x_write_reg(chip, offset, reg_val);
    if (ret)
        goto exit;

    chip->reg_output = reg_val;
    
exit:
    mutex_unlock(&chip->i2c_lock);
}
```

**对比 PL061:**

| Operation | PL061 | PCA953x |
|-----------|-------|---------|
| `get()` | 1 MMIO read, ~10ns | 1 I2C xfer, ~100µs |
| `set()` | 1 MMIO write, ~10ns | 1 I2C xfer, ~100µs |
| Locking | None (atomic HW) | mutex |
| Cache | No | Yes (reg_output) |

#### A.3.5 gpio_chip Setup Function

```c
static void pca953x_setup_gpio(struct pca953x_chip *chip, int gpios)
{
    struct gpio_chip *gc;

    gc = &chip->gpio_chip;

    /* [KEY] Wire up all callbacks */
    gc->direction_input  = pca953x_gpio_direction_input;
    gc->direction_output = pca953x_gpio_direction_output;
    gc->get = pca953x_gpio_get_value;
    gc->set = pca953x_gpio_set_value;
    
    /* [KEY] CRITICAL: Must set can_sleep for I2C chips! */
    gc->can_sleep = 1;

    gc->base = chip->gpio_start;
    gc->ngpio = gpios;
    gc->label = chip->client->name;
    gc->dev = &chip->client->dev;
    gc->owner = THIS_MODULE;
    gc->names = chip->names;
}
```

**说明:**
- `can_sleep = 1` 是关键设置！
- 忘记设置会导致从原子上下文调用时崩溃

#### A.3.6 Probe Function Walkthrough

```c
static int __devinit pca953x_probe(struct i2c_client *client,
                   const struct i2c_device_id *id)
{
    struct pca953x_platform_data *pdata;
    struct pca953x_chip *chip;
    int irq_base = 0, invert = 0;
    int ret;

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 1: Allocate and zero driver structure                          */
    /* ═══════════════════════════════════════════════════════════════════ */
    chip = kzalloc(sizeof(struct pca953x_chip), GFP_KERNEL);
    if (chip == NULL)
        return -ENOMEM;

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 2: Get platform data or use Device Tree                        */
    /* ═══════════════════════════════════════════════════════════════════ */
    pdata = client->dev.platform_data;
    if (pdata) {
        irq_base = pdata->irq_base;
        chip->gpio_start = pdata->gpio_base;
        invert = pdata->invert;
        chip->names = pdata->names;
    } else {
        pca953x_get_alt_pdata(client, &chip->gpio_start, &invert);
    }

    chip->client = client;

    /* [KEY] Decode chip type from id_table driver_data */
    chip->chip_type = id->driver_data & (PCA953X_TYPE | PCA957X_TYPE);

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 3: Initialize I2C access lock                                  */
    /* ═══════════════════════════════════════════════════════════════════ */
    mutex_init(&chip->i2c_lock);

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 4: Setup gpio_chip with callbacks                              */
    /* ═══════════════════════════════════════════════════════════════════ */
    /* ngpio comes from id->driver_data (4, 8, or 16) */
    pca953x_setup_gpio(chip, id->driver_data & PCA_GPIO_MASK);

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 5: Initialize chip hardware and read initial register state   */
    /* ═══════════════════════════════════════════════════════════════════ */
    if (chip->chip_type == PCA953X_TYPE)
        ret = device_pca953x_init(chip, invert);
    else
        ret = device_pca957x_init(chip, invert);
    if (ret)
        goto out_failed;

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 6: Setup IRQ handling (optional)                               */
    /* ═══════════════════════════════════════════════════════════════════ */
    ret = pca953x_irq_setup(chip, id, irq_base);
    if (ret)
        goto out_failed;

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 7: Register with gpiolib                                       */
    /* ═══════════════════════════════════════════════════════════════════ */
    ret = gpiochip_add(&chip->gpio_chip);
    if (ret)
        goto out_failed_irq;

    /* ═══════════════════════════════════════════════════════════════════ */
    /* STEP 8: Call platform-specific setup hook                           */
    /* ═══════════════════════════════════════════════════════════════════ */
    if (pdata && pdata->setup) {
        ret = pdata->setup(client, chip->gpio_chip.base,
                chip->gpio_chip.ngpio, pdata->context);
        if (ret < 0)
            dev_warn(&client->dev, "setup failed, %d\n", ret);
    }

    /* Store chip pointer for remove() */
    i2c_set_clientdata(client, chip);
    return 0;

out_failed_irq:
    pca953x_irq_teardown(chip);
out_failed:
    kfree(chip);
    return ret;
}
```

**说明:**
- I2C 驱动使用 `i2c_device_id` 表识别支持的芯片
- `driver_data` 字段编码 GPIO 数量和芯片类型
- 平台数据可以提供 `setup` 回调进行额外初始化

#### A.3.7 Remove Function

```c
static int pca953x_remove(struct i2c_client *client)
{
    struct pca953x_platform_data *pdata = client->dev.platform_data;
    struct pca953x_chip *chip = i2c_get_clientdata(client);
    int ret = 0;

    /* [STEP 1] Call platform teardown hook first */
    if (pdata && pdata->teardown) {
        ret = pdata->teardown(client, chip->gpio_chip.base,
                chip->gpio_chip.ngpio, pdata->context);
        if (ret < 0) {
            dev_err(&client->dev, "%s failed, %d\n", "teardown", ret);
            return ret;  /* Can't continue if teardown fails */
        }
    }

    /* [STEP 2] Unregister from gpiolib */
    ret = gpiochip_remove(&chip->gpio_chip);
    if (ret) {
        dev_err(&client->dev, "%s failed, %d\n", "gpiochip_remove()", ret);
        return ret;  /* [KEY] Can't remove - GPIOs still in use! */
    }

    /* [STEP 3] Cleanup IRQ resources */
    pca953x_irq_teardown(chip);
    
    /* [STEP 4] Free driver structure */
    kfree(chip);
    return 0;
}
```

**说明:**
- 移除顺序与 probe 相反
- `gpiochip_remove()` 失败时不能释放内存！
- 这防止了使用后释放（use-after-free）错误

---

### A.4 Comparing the Two Drivers

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      SIDE-BY-SIDE COMPARISON TABLE                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ASPECT              │ PL061                   │ PCA953x                    │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  Bus                 │ AMBA (memory-mapped)    │ I2C                        │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  Access Latency      │ ~10-100 ns              │ ~100-500 µs                │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  can_sleep           │ 0 (atomic)              │ 1 (may sleep)              │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  Lock Type           │ spinlock_t              │ mutex                      │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  Register Cache      │ No                      │ Yes (output, direction)    │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  get() Locking       │ None (HW atomic)        │ mutex                      │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  set() Locking       │ None (HW atomic)        │ mutex                      │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  direction() Lock    │ spinlock (RMW)          │ mutex                      │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  Chip Variants       │ 1                       │ ~20                        │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  GPIOs per chip      │ Fixed 8                 │ 4, 8, or 16                │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  IRQ Context Safe    │ Yes                     │ No (threaded IRQ only)     │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  Platform Binding    │ AMBA device             │ I2C client                 │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  container_of        │ Yes                     │ Yes                        │
│  ────────────────────┼─────────────────────────┼────────────────────────    │
│  Error Handling      │ Minimal                 │ I2C errors checked         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### A.5 Lessons from Real Driver Code

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    KEY LESSONS FROM REAL GPIO DRIVERS                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   LESSON 1: The gpio_chip Abstraction Works                                 │
│   ─────────────────────────────────────────                                 │
│   • Same consumer API works for MMIO and I2C hardware                       │
│   • Drivers just implement callbacks differently                            │
│   • Core routes calls without knowing hardware details                      │
│                                                                             │
│   LESSON 2: can_sleep Flag is Critical                                      │
│   ────────────────────────────────────                                      │
│   • PL061: can_sleep=0 → usable in IRQ handlers                            │
│   • PCA953x: can_sleep=1 → process context only                            │
│   • Wrong setting → kernel crash or deadlock                               │
│                                                                             │
│   LESSON 3: Locking Strategy Follows Access Pattern                        │
│   ─────────────────────────────────────────────                             │
│   • Fast MMIO: spinlock for RMW, none for atomic ops                       │
│   • Slow I2C: mutex everywhere (sleeping allowed)                          │
│                                                                             │
│   LESSON 4: Value-Before-Direction is Universal                            │
│   ──────────────────────────────────────────────                            │
│   • Both drivers set output value before enabling output direction         │
│   • This prevents glitches on the GPIO pin                                  │
│   • Some hardware (PL061) needs extra care                                  │
│                                                                             │
│   LESSON 5: Register Caching Saves Bus Bandwidth                           │
│   ───────────────────────────────────────────────                           │
│   • PCA953x caches direction and output registers                          │
│   • Avoids I2C reads for known state                                        │
│   • Must keep cache synchronized with hardware!                             │
│                                                                             │
│   LESSON 6: container_of is the Standard Pattern                           │
│   ───────────────────────────────────────────────                           │
│   • Both drivers embed gpio_chip in private struct                         │
│   • Both use container_of() to recover full struct                         │
│   • This is THE way to attach private data in Linux                        │
│                                                                             │
│   LESSON 7: Error Path Must Release Resources in Reverse Order             │
│   ─────────────────────────────────────────────────────────────             │
│   • Both drivers have careful error cleanup in probe()                     │
│   • Resources released in reverse order of acquisition                     │
│   • gpiochip_remove() must succeed before freeing memory                   │
│                                                                             │
│   LESSON 8: Platform Data Carries Board-Specific Info                      │
│   ────────────────────────────────────────────────────                      │
│   • GPIO base, IRQ base, initial directions                                │
│   • Setup/teardown callbacks for complex boards                            │
│   • Later replaced by Device Tree properties                               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 同一个 `gpio_chip` 接口可以抽象完全不同的硬件
- `can_sleep` 标志决定了驱动可以在什么上下文中使用
- 锁策略取决于硬件访问特性
- 先设值后设方向是通用模式
- `container_of` 是标准的私有数据恢复方法
- 错误处理必须按相反顺序释放资源

---

### A.6 Complete Call Flow Trace

Let's trace a complete call from consumer to hardware for both drivers:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              COMPLETE CALL FLOW: gpio_set_value(42, 1)                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   CONSUMER (e.g., LED driver)                                               │
│   ───────────────────────────                                               │
│   gpio_set_value(42, 1);                                                    │
│          │                                                                  │
│          ▼                                                                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                         gpiolib.c                                    │   │
│   │   __gpio_set_value(42, 1) {                                         │   │
│   │       chip = gpio_to_chip(42);    // O(1) lookup: gpio_desc[42]     │   │
│   │       WARN_ON(chip->can_sleep);   // Warn if wrong context          │   │
│   │       chip->set(chip, 42 - chip->base, 1);   // Dispatch            │   │
│   │   }                                                                  │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│          │                                                                  │
│          │ chip->set()                                                      │
│          ▼                                                                  │
│   ╔═════════════════════════════╦═════════════════════════════════════╗     │
│   ║         PL061 PATH           ║          PCA953x PATH               ║     │
│   ╠═════════════════════════════╬═════════════════════════════════════╣     │
│   ║ pl061_set_value(gc, 2, 1)   ║ pca953x_gpio_set_value(gc, 2, 1)   ║     │
│   ║ {                           ║ {                                   ║     │
│   ║   chip = container_of(gc);  ║   chip = container_of(gc);         ║     │
│   ║                             ║   mutex_lock(&chip->i2c_lock);     ║     │
│   ║   // Calculate bit-band    ║   // Update cached output          ║     │
│   ║   // address for GPIO 2    ║   reg_val = chip->reg_output;      ║     │
│   ║   addr = base + (1 << 4);  ║   reg_val |= (1 << 2);             ║     │
│   ║                             ║                                     ║     │
│   ║   // Single atomic write   ║   // I2C transaction               ║     │
│   ║   writeb(1 << 2, addr);    ║   i2c_smbus_write_byte_data(       ║     │
│   ║                             ║       client, OUTPUT_REG, reg_val);║     │
│   ║   // Done! ~10ns           ║                                     ║     │
│   ║                             ║   chip->reg_output = reg_val;      ║     │
│   ║                             ║   mutex_unlock();                   ║     │
│   ║ }                           ║ }  // ~100-500µs                    ║     │
│   ╚═════════════════════════════╩═════════════════════════════════════╝     │
│          │                               │                                  │
│          ▼                               ▼                                  │
│   ┌─────────────────┐           ┌─────────────────────────┐                │
│   │  MMIO Write     │           │   I2C Bus Transaction   │                │
│   │  to PL061 reg   │           │   START-ADDR-REG-DATA-  │                │
│   │  via CPU bus    │           │   STOP on I2C wires     │                │
│   └─────────────────┘           └─────────────────────────┘                │
│          │                               │                                  │
│          ▼                               ▼                                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                         HARDWARE                                     │   │
│   │                   GPIO PIN 42 → HIGH                                 │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 相同的消费者 API 调用
- gpiolib 根据 GPIO 编号找到正确的控制器
- 调用驱动的回调函数
- 驱动执行硬件特定的操作
- 延迟差异：PL061 ~10ns，PCA953x ~100-500µs

---

### A.7 Driver Development Checklist (Based on Real Drivers)

Based on analyzing these real drivers, here's a checklist for writing your own:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   GPIO DRIVER DEVELOPMENT CHECKLIST                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   STRUCTURE DEFINITION:                                                     │
│   □ Define private structure with embedded gpio_chip                        │
│   □ Include appropriate lock (spinlock for MMIO, mutex for bus)            │
│   □ Include hardware access handle (void __iomem * or bus client)          │
│   □ Include register cache if bus access is slow                           │
│                                                                             │
│   CALLBACK IMPLEMENTATION:                                                  │
│   □ Implement direction_input()                                             │
│   □ Implement direction_output() - VALUE BEFORE DIRECTION!                  │
│   □ Implement get() - check return value semantics                         │
│   □ Implement set()                                                         │
│   □ Optional: request(), free(), to_irq(), set_debounce()                  │
│   □ Use container_of() to recover private data                             │
│   □ Use appropriate locking for read-modify-write                          │
│                                                                             │
│   GPIO_CHIP CONFIGURATION:                                                  │
│   □ Set label (device name)                                                 │
│   □ Set dev (parent device pointer)                                         │
│   □ Set owner = THIS_MODULE                                                 │
│   □ Set base (-1 for dynamic, or platform-specified)                       │
│   □ Set ngpio (number of GPIOs)                                             │
│   □ Set can_sleep = 1 if ANY callback might sleep                          │
│   □ Wire all callback function pointers                                     │
│                                                                             │
│   PROBE FUNCTION:                                                           │
│   □ Allocate private structure (kzalloc or devm_kzalloc)                   │
│   □ Get and validate platform data                                          │
│   □ Request and map hardware resources                                      │
│   □ Initialize locks                                                        │
│   □ Configure gpio_chip                                                     │
│   □ Call gpiochip_add()                                                     │
│   □ Handle errors with proper cleanup                                       │
│   □ Store driver data for remove()                                          │
│                                                                             │
│   REMOVE FUNCTION:                                                          │
│   □ Get stored driver data                                                  │
│   □ Call gpiochip_remove() and CHECK RETURN VALUE                          │
│   □ Only free resources if gpiochip_remove() succeeds                      │
│   □ Release in reverse order of probe()                                     │
│                                                                             │
│   TESTING:                                                                  │
│   □ Test from process context (normal use)                                  │
│   □ Test from atomic context if can_sleep=0                                │
│   □ Test direction changes                                                  │
│   □ Test concurrent access                                                  │
│   □ Test module load/unload with active GPIOs                              │
│   □ Check /sys/kernel/debug/gpio                                           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

*This appendix walks through real GPIO drivers from Linux kernel v3.2 to demonstrate how the architectural principles translate into practical code. The patterns shown here apply to many other kernel subsystems with similar ops-based designs.*

