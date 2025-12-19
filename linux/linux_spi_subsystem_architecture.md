# Linux Kernel SPI Subsystem Architecture (v3.2)

## Table of Contents

1. [Phase 1 — SPI Subsystem Overview](#phase-1--spi-subsystem-overview)
2. [Phase 2 — Core Objects & Relationships](#phase-2--core-objects--relationships)
3. [Phase 3 — struct spi_master (Controller)](#phase-3--struct-spi_master-controller)
4. [Phase 4 — struct spi_device (Slave Device)](#phase-4--struct-spi_device-slave-device)
5. [Phase 5 — struct spi_driver (Device Driver)](#phase-5--struct-spi_driver-device-driver)
6. [Phase 6 — Ops, Callbacks, and Contracts](#phase-6--ops-callbacks-and-contracts)
7. [Phase 7 — Driver Registration & Matching](#phase-7--driver-registration--matching)
8. [Phase 8 — End-to-End Transfer Path](#phase-8--end-to-end-transfer-path)
9. [Phase 9 — Concurrency, Context, and Performance](#phase-9--concurrency-context-and-performance)
10. [Phase 10 — Common Bugs, Pitfalls, and Design Lessons](#phase-10--common-bugs-pitfalls-and-design-lessons)
11. [Appendix A — Walking Through Real SPI Drivers](#appendix-a--walking-through-real-spi-drivers)
12. [Appendix B — SPI vs I²C vs UART Architecture Comparison](#appendix-b--spi-vs-ic-vs-uart-architecture-comparison)
13. [Appendix C — Designing a Bus Subsystem in User-Space](#appendix-c--designing-a-bus-subsystem-in-user-space)
14. [Appendix D — SPI and Async I/O Patterns](#appendix-d--spi-and-async-io-patterns)

---

## Phase 1 — SPI Subsystem Overview

### 1.1 What Problems Does the SPI Subsystem Solve?

The SPI (Serial Peripheral Interface) subsystem addresses several fundamental challenges:

| Problem | Solution |
|---------|----------|
| **Hardware Diversity** | Abstract different SPI controllers behind a uniform API |
| **Protocol Uniformity** | Provide consistent message-based interface for all SPI devices |
| **Driver Portability** | Device drivers work unchanged across different SPI controllers |
| **Resource Management** | Centralized bus arbitration, chip select management, clocking |
| **Concurrency Control** | Serialize access to shared bus, handle multiple devices |
| **Power Management** | Coordinate suspend/resume across the bus hierarchy |

### 1.2 Why Linux Models SPI as Controller-Device-Driver

Linux separates SPI into three distinct layers:

```
+------------------------------------------+
|          SPI Device Drivers              |  <-- Protocol handlers
|   (spi-tle62x0.c, spi-flash, etc.)       |      (know device protocols)
+------------------------------------------+
                    |
                    | spi_transfer/spi_message
                    v
+------------------------------------------+
|          SPI Core (spi.c)                |  <-- Bus abstraction
|   + spi_master management                |      (matches drivers to devices)
|   + spi_device representation            |
|   + Registration & matching              |
+------------------------------------------+
                    |
                    | master->transfer()
                    v
+------------------------------------------+
|       SPI Controller Drivers             |  <-- Hardware access
|   (spi-pl022.c, spi-gpio.c, etc.)        |      (talk to actual hardware)
+------------------------------------------+
                    |
                    v
            [ SPI Hardware ]
```

**中文解释：**
- **SPI设备驱动**：理解特定设备的协议（如Flash、传感器）
- **SPI核心**：管理控制器和设备，负责匹配驱动程序
- **SPI控制器驱动**：直接操作硬件寄存器或GPIO

**Why This Separation Exists:**

1. **Controller driver**: Knows HOW to clock data (bit-bang, DMA, FIFO)
2. **Device representation**: Describes WHICH device (chip select, speed, mode)
3. **Device driver**: Knows WHAT to send (protocol, commands, data format)

### 1.3 SPI vs I²C: Architectural Comparison

| Aspect | SPI | I²C |
|--------|-----|-----|
| **Addressing** | Hardware chip select lines | Software addresses (7/10-bit) |
| **Speed** | Higher (10+ MHz common) | Lower (100/400 kHz standard) |
| **Wiring** | MOSI, MISO, SCK, CS (per device) | SDA, SCL (shared) |
| **Transfer Unit** | Messages with multiple transfers | Individual transactions |
| **Full Duplex** | Yes (simultaneous read/write) | No (half-duplex) |
| **Discovery** | Not possible (static config) | Possible but uncommon |

**Key Architectural Difference:**
- I²C: Addressing is embedded in the protocol
- SPI: Chip select is physical hardware; device discovery requires board configuration

### 1.4 Where SPI Code Lives in v3.2

```
linux/
├── drivers/spi/
│   ├── spi.c                    # [CORE] SPI core implementation
│   ├── spi-bitbang.c            # [LIB]  Bitbang helper library
│   ├── spi-bitbang-txrx.h       # [LIB]  Bitbang transfer macros
│   │
│   ├── spi-pl022.c              # [CTRL] ARM PL022 SSP controller
│   ├── spi-gpio.c               # [CTRL] GPIO bitbang controller
│   ├── spi-omap2-mcspi.c        # [CTRL] OMAP2/3 SPI controller
│   ├── spi-imx.c                # [CTRL] i.MX SPI controller
│   ├── ... (40+ controller drivers)
│   │
│   ├── spi-tle62x0.c            # [DEV]  TLE62x0 SPI GPIO expander
│   └── spidev.c                 # [DEV]  Userspace SPI access
│
├── include/linux/spi/
│   ├── spi.h                    # [API]  Main SPI API header
│   ├── spi_bitbang.h            # [API]  Bitbang helper API
│   ├── spi_gpio.h               # [PLAT] GPIO-SPI platform data
│   └── ... (device-specific headers)
```

**File Categories:**
- `[CORE]`: Framework implementation
- `[LIB]`: Reusable helper code
- `[CTRL]`: Controller (master) drivers
- `[DEV]`: Device (slave) drivers
- `[API]`: Public headers
- `[PLAT]`: Platform data structures

---

## Phase 2 — Core Objects & Relationships

### 2.1 Object Relationship Diagram

```
                    +------------------+
                    |   struct device  |  <-- Generic driver model
                    +------------------+
                           ^    ^
              embedded     |    |     embedded
        +------------------+    +------------------+
        |                                          |
+-------v---------+                    +-----------v------+
| struct spi_master |                    | struct spi_device |
| (Controller)      |<-------------------| (Slave Device)    |
|                   |    spi->master     |                   |
| - dev             |                    | - dev             |
| - bus_num         |                    | - master          |
| - num_chipselect  |                    | - chip_select     |
| - transfer()      |                    | - max_speed_hz    |
| - setup()         |                    | - mode            |
| - cleanup()       |                    | - modalias        |
+---------+---------+                    +--------+----------+
          |                                       |
          | spi_master_get_devdata()              | driver attachment
          v                                       v
+-------------------+                    +-----------------+
| Controller Private|                    | struct spi_driver|
| Data (pl022, etc.)|                    | (Device Driver)  |
| - hardware regs   |                    | - probe()        |
| - DMA channels    |                    | - remove()       |
| - message queue   |                    | - id_table       |
+-------------------+                    | - driver         |
                                         +-----------------+
```

**中文解释：**
- `spi_master`：代表SPI控制器（主设备），负责发送时钟和数据
- `spi_device`：代表SPI从设备，描述设备参数（速度、模式、片选）
- `spi_driver`：设备驱动程序，实现设备协议
- 私有数据通过`spi_master_get_devdata()`或`spi_set_drvdata()`访问

### 2.2 Ownership and Reference Rules

| Object | Allocated By | Registered By | Reference Count |
|--------|--------------|---------------|-----------------|
| `spi_master` | Controller driver via `spi_alloc_master()` | Controller driver via `spi_register_master()` | `spi_master_get/put()` |
| `spi_device` | SPI core via `spi_alloc_device()` | SPI core via `spi_add_device()` | `spi_dev_get/put()` |
| `spi_driver` | Device driver (static) | Device driver via `spi_register_driver()` | None (driver model) |

### 2.3 Key Data Flow

```
Device Driver                SPI Core                 Controller Driver
     |                          |                           |
     | spi_sync(spi, msg)       |                           |
     |------------------------->|                           |
     |                          | msg->spi = spi            |
     |                          | master->transfer(spi,msg) |
     |                          |-------------------------->|
     |                          |                           | [queue message]
     |                          |                           | [process queue]
     |                          |                           | [hardware I/O]
     |                          |                           | msg->complete()
     |                          |<--------------------------|
     | [wake up]                |                           |
     |<-------------------------|                           |
     | return msg->status       |                           |
```

**中文解释：**
- 设备驱动调用`spi_sync()`发送消息
- SPI核心将消息传递给控制器驱动的`transfer()`回调
- 控制器驱动将消息入队并处理
- 完成后通过回调通知，`spi_sync()`返回

---

## Phase 3 — struct spi_master (Controller)

### 3.1 What a spi_master Represents

`struct spi_master` represents an SPI bus controller (master device). It:
- Provides the physical SPI interface (clock, data lines)
- Manages multiple slave devices through chip select signals
- Implements the actual data transfer mechanism
- Owns the bus and arbitrates access

### 3.2 Important Fields

```c
/* include/linux/spi/spi.h */
struct spi_master {
    struct device   dev;              /* [KEY] Embedded device (driver model) */

    struct list_head list;            /* [INTERNAL] Global master list */

    s16         bus_num;              /* [KEY] Board-specific bus number */
    u16         num_chipselect;       /* [KEY] Number of CS lines available */
    u16         dma_alignment;        /* DMA buffer alignment requirement */
    u16         mode_bits;            /* [KEY] Supported SPI modes (CPOL, CPHA, etc.) */
    u16         flags;                /* Constraints (half-duplex, no RX/TX) */
#define SPI_MASTER_HALF_DUPLEX  BIT(0)
#define SPI_MASTER_NO_RX        BIT(1)
#define SPI_MASTER_NO_TX        BIT(2)

    /* [LOCK] Bus locking for exclusive access */
    spinlock_t      bus_lock_spinlock;
    struct mutex    bus_lock_mutex;
    bool            bus_lock_flag;

    /* [OPS] Controller callbacks */
    int  (*setup)(struct spi_device *spi);      /* Configure device parameters */
    int  (*transfer)(struct spi_device *spi,    /* [KEY] Queue a message */
                     struct spi_message *mesg);
    void (*cleanup)(struct spi_device *spi);    /* Free per-device resources */
};
```

**Field Purposes:**

| Field | Purpose | Set By |
|-------|---------|--------|
| `bus_num` | Identifies this controller (e.g., SPI0, SPI1) | Controller driver or platform |
| `num_chipselect` | How many devices can be connected | Controller driver |
| `mode_bits` | Which SPI modes are supported (CPOL, CPHA, etc.) | Controller driver |
| `transfer` | **Critical**: How to send messages | Controller driver |
| `setup` | Configure a device before use | Controller driver |
| `cleanup` | Release per-device resources | Controller driver |

### 3.3 Controller Private Data Storage

```c
/* Allocation: driver-private data follows the spi_master struct */
struct spi_master *spi_alloc_master(struct device *dev, unsigned size)
{
    struct spi_master *master;

    master = kzalloc(size + sizeof *master, GFP_KERNEL);  /* [KEY] Extra space for private data */
    if (!master)
        return NULL;

    device_initialize(&master->dev);
    master->dev.class = &spi_master_class;
    master->dev.parent = get_device(dev);
    spi_master_set_devdata(master, &master[1]);  /* [KEY] Point to extra space */

    return master;
}

/* Access: retrieve private data */
static inline void *spi_master_get_devdata(struct spi_master *master)
{
    return dev_get_drvdata(&master->dev);
}
```

**Memory Layout:**

```
+---------------------+
|   spi_master        |  <-- sizeof(struct spi_master)
|   - dev             |
|   - bus_num         |
|   - transfer        |
|   - ...             |
+---------------------+
|   Driver Private    |  <-- 'size' bytes passed to spi_alloc_master()
|   Data              |
|   - hardware regs   |
|   - DMA state       |
|   - work queue      |
+---------------------+
```

**中文解释：**
- `spi_alloc_master()`分配`spi_master`结构体加上驱动私有数据的空间
- 私有数据紧跟在`spi_master`结构体之后
- 通过`spi_master_get_devdata()`获取私有数据指针

### 3.4 Master Registration and Removal

```c
/* Registration (from controller driver's probe) */
int spi_register_master(struct spi_master *master)
{
    /* Validate */
    if (master->num_chipselect == 0)
        return -EINVAL;

    /* Assign dynamic bus number if needed */
    if (master->bus_num < 0)
        master->bus_num = atomic_dec_return(&dyn_bus_id);

    /* Initialize locking */
    spin_lock_init(&master->bus_lock_spinlock);
    mutex_init(&master->bus_lock_mutex);

    /* Register device */
    dev_set_name(&master->dev, "spi%u", master->bus_num);
    status = device_add(&master->dev);  /* [KEY] Visible in sysfs */

    /* Add to global list and match board info */
    mutex_lock(&board_lock);
    list_add_tail(&master->list, &spi_master_list);
    list_for_each_entry(bi, &board_list, list)
        spi_match_master_to_boardinfo(master, &bi->board_info);
    mutex_unlock(&board_lock);

    /* Register devices from device tree */
    of_register_spi_devices(master);

    return 0;
}

/* Removal (from controller driver's remove) */
void spi_unregister_master(struct spi_master *master)
{
    /* Remove from global list */
    mutex_lock(&board_lock);
    list_del(&master->list);
    mutex_unlock(&board_lock);

    /* Unregister all child devices */
    device_for_each_child(&master->dev, NULL, __unregister);

    device_unregister(&master->dev);
}
```

### 3.5 Lifetime and Ownership Rules

1. **Allocation**: Controller driver allocates via `spi_alloc_master()`
2. **Registration**: Controller driver registers via `spi_register_master()`
3. **Reference Counting**: Use `spi_master_get()`/`spi_master_put()`
4. **Removal**: Controller driver calls `spi_unregister_master()`
5. **Release**: Called automatically when refcount reaches zero

**Contract:**
- Master must remain valid while any `spi_device` references it
- `spi_device` holds a reference to its master
- Controller driver must NOT free master directly; use `spi_master_put()`

---

## Phase 4 — struct spi_device (Slave Device)

### 4.1 What a spi_device Represents

`struct spi_device` is a **proxy object** that represents an SPI slave device attached to a specific master's chip select line. It describes:
- Which controller it's connected to
- Its electrical parameters (speed, polarity, phase)
- Its chip select position
- Its driver association

### 4.2 Structure Definition

```c
/* include/linux/spi/spi.h */
struct spi_device {
    struct device       dev;              /* [KEY] Embedded device for driver model */
    struct spi_master   *master;          /* [KEY] Controller this device is on */
    u32                 max_speed_hz;     /* [KEY] Maximum clock rate */
    u8                  chip_select;      /* [KEY] Chip select line (0..num_chipselect-1) */
    u8                  mode;             /* [KEY] SPI mode flags */
#define SPI_CPHA        0x01              /* Clock phase */
#define SPI_CPOL        0x02              /* Clock polarity */
#define SPI_MODE_0      (0|0)
#define SPI_MODE_1      (0|SPI_CPHA)
#define SPI_MODE_2      (SPI_CPOL|0)
#define SPI_MODE_3      (SPI_CPOL|SPI_CPHA)
#define SPI_CS_HIGH     0x04              /* Chip select active high */
#define SPI_LSB_FIRST   0x08              /* LSB first */
#define SPI_3WIRE       0x10              /* SI/SO shared */
#define SPI_LOOP        0x20              /* Loopback mode */
#define SPI_NO_CS       0x40              /* No chip select */
#define SPI_READY       0x80              /* Slave pulls low to pause */

    u8                  bits_per_word;    /* Word size (default 8) */
    int                 irq;              /* Optional interrupt */
    void                *controller_state;/* [KEY] Controller-specific runtime state */
    void                *controller_data; /* Board-specific controller hints */
    char                modalias[SPI_NAME_SIZE]; /* [KEY] Driver name to bind */
};
```

### 4.3 How spi_devices Are Created (v3.2 Style)

**Method 1: Static Board Info (Most Common in v3.2)**

```c
/* In board file (arch/arm/mach-xxx/board-yyy.c) */
static struct spi_board_info my_spi_devices[] __initdata = {
    {
        .modalias       = "tle62x0",          /* [KEY] Driver to bind */
        .platform_data  = &tle62x0_info,      /* Device-specific config */
        .controller_data = NULL,               /* Controller hints */
        .irq            = IRQ_GPIO_PIN,
        .max_speed_hz   = 1000000,             /* 1 MHz */
        .bus_num        = 0,                   /* SPI0 */
        .chip_select    = 1,                   /* CS1 */
        .mode           = SPI_MODE_0,
    },
    /* ... more devices ... */
};

static void __init board_init(void)
{
    spi_register_board_info(my_spi_devices,
                            ARRAY_SIZE(my_spi_devices));
}
```

**When Master Registers:**

```c
/* drivers/spi/spi.c - called from spi_register_master() */
static void spi_match_master_to_boardinfo(struct spi_master *master,
                                          struct spi_board_info *bi)
{
    if (master->bus_num != bi->bus_num)
        return;  /* Wrong bus */

    /* Create the spi_device */
    dev = spi_new_device(master, bi);  /* [KEY] Allocate and register */
}
```

**Method 2: Dynamic Creation**

```c
/* For hotplug scenarios */
struct spi_device *spi_new_device(struct spi_master *master,
                                  struct spi_board_info *chip)
{
    struct spi_device *proxy;

    proxy = spi_alloc_device(master);  /* [KEY] Allocate */
    if (!proxy)
        return NULL;

    /* Copy board info to device */
    proxy->chip_select = chip->chip_select;
    proxy->max_speed_hz = chip->max_speed_hz;
    proxy->mode = chip->mode;
    proxy->irq = chip->irq;
    strlcpy(proxy->modalias, chip->modalias, sizeof(proxy->modalias));
    proxy->dev.platform_data = (void *) chip->platform_data;
    proxy->controller_data = chip->controller_data;

    status = spi_add_device(proxy);  /* [KEY] Register and setup */
    if (status < 0) {
        spi_dev_put(proxy);
        return NULL;
    }

    return proxy;
}
```

### 4.4 Chip Select Handling

```c
/* Validation during registration */
int spi_add_device(struct spi_device *spi)
{
    /* Validate chip select */
    if (spi->chip_select >= spi->master->num_chipselect) {
        dev_err(dev, "cs%d >= max %d\n",
                spi->chip_select, spi->master->num_chipselect);
        return -EINVAL;  /* [KEY] Reject invalid CS */
    }

    /* Check for duplicate CS */
    d = bus_find_device_by_name(&spi_bus_type, NULL, dev_name(&spi->dev));
    if (d != NULL) {
        dev_err(dev, "chipselect %d already in use\n", spi->chip_select);
        return -EBUSY;  /* [KEY] Prevent conflicts */
    }

    /* Call master's setup to configure hardware */
    status = spi_setup(spi);  /* [KEY] Apply device settings */
    if (status < 0)
        return status;

    /* Register with driver model */
    status = device_add(&spi->dev);
    return status;
}
```

### 4.5 Driver-Private Data Attachment

```c
/* In device driver's probe() */
static int my_spi_probe(struct spi_device *spi)
{
    struct my_device_data *data;

    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->spi = spi;  /* Back-reference */

    spi_set_drvdata(spi, data);  /* [KEY] Attach private data */

    /* ... rest of initialization ... */
    return 0;
}

/* In other driver functions */
static ssize_t my_read(...)
{
    struct my_device_data *data = spi_get_drvdata(spi);  /* [KEY] Retrieve */
    /* ... use data ... */
}

/* In remove() */
static int my_spi_remove(struct spi_device *spi)
{
    struct my_device_data *data = spi_get_drvdata(spi);

    /* ... cleanup ... */
    kfree(data);
    return 0;
}
```

---

## Phase 5 — struct spi_driver (Device Driver)

### 5.1 What a spi_driver Represents

`struct spi_driver` is a **protocol driver** that knows how to communicate with a specific type of SPI device. It doesn't touch hardware directly; it uses the SPI message API to exchange data.

### 5.2 Structure Definition

```c
/* include/linux/spi/spi.h */
struct spi_driver {
    const struct spi_device_id *id_table;  /* [KEY] Supported devices */
    int  (*probe)(struct spi_device *spi);  /* [KEY] Device found */
    int  (*remove)(struct spi_device *spi); /* [KEY] Device going away */
    void (*shutdown)(struct spi_device *spi);/* System shutdown */
    int  (*suspend)(struct spi_device *spi, pm_message_t mesg);
    int  (*resume)(struct spi_device *spi);
    struct device_driver driver;            /* [KEY] Generic driver (name, owner) */
};
```

### 5.3 Integration with Linux Driver Model

```c
/* drivers/spi/spi.c */

/* Wrapper to call spi_driver's probe with correct type */
static int spi_drv_probe(struct device *dev)
{
    const struct spi_driver *sdrv = to_spi_driver(dev->driver);
    return sdrv->probe(to_spi_device(dev));  /* [KEY] Type-safe wrapper */
}

static int spi_drv_remove(struct device *dev)
{
    const struct spi_driver *sdrv = to_spi_driver(dev->driver);
    return sdrv->remove(to_spi_device(dev));
}

/* Registration hooks these wrappers */
int spi_register_driver(struct spi_driver *sdrv)
{
    sdrv->driver.bus = &spi_bus_type;  /* [KEY] Associate with SPI bus */
    if (sdrv->probe)
        sdrv->driver.probe = spi_drv_probe;
    if (sdrv->remove)
        sdrv->driver.remove = spi_drv_remove;
    if (sdrv->shutdown)
        sdrv->driver.shutdown = spi_drv_shutdown;
    return driver_register(&sdrv->driver);  /* [KEY] Generic registration */
}
```

### 5.4 Complete Driver Example

```c
/* drivers/spi/spi-tle62x0.c - Simplified */

/* Private data structure */
struct tle62x0_state {
    struct spi_device   *us;
    struct mutex        lock;
    unsigned int        nr_gpio;
    unsigned int        gpio_state;
    unsigned char       tx_buff[4];
    unsigned char       rx_buff[4];
};

/* probe: called when device matches */
static int __devinit tle62x0_probe(struct spi_device *spi)
{
    struct tle62x0_state *st;
    struct tle62x0_pdata *pdata;

    pdata = spi->dev.platform_data;  /* [KEY] Get board config */
    if (!pdata)
        return -EINVAL;

    st = kzalloc(sizeof(*st), GFP_KERNEL);  /* Allocate private data */
    if (!st)
        return -ENOMEM;

    st->us = spi;  /* [KEY] Store spi_device reference */
    st->nr_gpio = pdata->gpio_count;
    st->gpio_state = pdata->init_state;
    mutex_init(&st->lock);

    spi_set_drvdata(spi, st);  /* [KEY] Attach to spi_device */

    /* Create sysfs attributes, etc. */
    return 0;
}

/* remove: called when device is unbound */
static int __devexit tle62x0_remove(struct spi_device *spi)
{
    struct tle62x0_state *st = spi_get_drvdata(spi);

    /* Remove sysfs attributes */
    kfree(st);
    return 0;
}

/* Driver definition */
static struct spi_driver tle62x0_driver = {
    .driver = {
        .name   = "tle62x0",       /* [KEY] Must match modalias */
        .owner  = THIS_MODULE,
    },
    .probe      = tle62x0_probe,
    .remove     = __devexit_p(tle62x0_remove),
};

/* Module init/exit */
static __init int tle62x0_init(void)
{
    return spi_register_driver(&tle62x0_driver);
}

static __exit void tle62x0_exit(void)
{
    spi_unregister_driver(&tle62x0_driver);
}

module_init(tle62x0_init);
module_exit(tle62x0_exit);

MODULE_ALIAS("spi:tle62x0");  /* [KEY] Enables modprobe */
```

### 5.5 Probe/Remove Lifecycle and Error Handling

```
         spi_register_driver()
                 |
                 v
         driver_register()
                 |
      (driver model scans for matches)
                 |
    +------------+------------+
    |                         |
    v                         v
[Match Found]           [No Match]
    |                         |
    v                         v
spi_drv_probe()          (nothing)
    |
    v
sdrv->probe(spi)
    |
    +---> success (0): device bound
    |
    +---> failure (-Exxx): device NOT bound
                          driver model handles cleanup
```

**Error Handling in probe():**

```c
static int my_probe(struct spi_device *spi)
{
    struct my_data *data;
    int ret;

    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;  /* [KEY] Early return on alloc failure */

    ret = some_init_function();
    if (ret)
        goto err_free_data;  /* [KEY] Clean up on partial failure */

    ret = another_init();
    if (ret)
        goto err_undo_init;

    spi_set_drvdata(spi, data);
    return 0;  /* Success */

err_undo_init:
    undo_some_init();
err_free_data:
    kfree(data);
    return ret;  /* [KEY] Return error code */
}
```

---

## Phase 6 — Ops, Callbacks, and Contracts

### 6.1 Key Callback Sets

SPI has two distinct callback sets:

| Callback Set | Location | Purpose |
|--------------|----------|---------|
| Master ops | `struct spi_master` | Controller hardware access |
| Driver callbacks | `struct spi_driver` | Device protocol handling |

### 6.2 Master Callbacks (Controller Ops)

```c
struct spi_master {
    /* [OP1] Setup: configure device-specific parameters */
    int (*setup)(struct spi_device *spi);

    /* [OP2] Transfer: queue a message for transmission */
    int (*transfer)(struct spi_device *spi, struct spi_message *mesg);

    /* [OP3] Cleanup: release per-device controller resources */
    void (*cleanup)(struct spi_device *spi);
};
```

**Callback Contract Table:**

| Callback | When Called | Context | Core Guarantees | Driver Must |
|----------|-------------|---------|-----------------|-------------|
| `setup` | Before first transfer, after mode changes | May sleep | Device registered, valid params | Configure hardware for this device |
| `transfer` | For every message submission | Any (IRQ, process) | Valid message, valid device | Queue message, return immediately |
| `cleanup` | When device is unregistered | May sleep | Device won't be used again | Free per-device resources |

### 6.3 The xxx->ops->yyy() Pattern in SPI

SPI uses a variation of the ops pattern:

```c
/* The pattern: context object contains ops directly (not via pointer) */
int spi_setup(struct spi_device *spi)
{
    /* Validation */
    bad_bits = spi->mode & ~spi->master->mode_bits;
    if (bad_bits)
        return -EINVAL;

    /* [KEY] Invoke master's setup callback */
    return spi->master->setup(spi);
}

static int __spi_async(struct spi_device *spi, struct spi_message *message)
{
    struct spi_master *master = spi->master;

    /* Validation */
    if ((master->flags & SPI_MASTER_HALF_DUPLEX) ...) {
        /* Check for invalid configurations */
    }

    message->spi = spi;
    message->status = -EINPROGRESS;

    /* [KEY] Invoke master's transfer callback */
    return master->transfer(spi, message);
}
```

**中文解释：**
- `spi_master`直接包含回调函数指针（不是通过独立的ops结构）
- 这与I²C的`i2c_algorithm`模式略有不同
- 每个控制器驱动在注册时设置这些回调

### 6.4 Why Direct Hardware Access Is Avoided

Device drivers use the message API instead of direct hardware access because:

1. **Hardware Independence**: Same driver works across all controllers
2. **Concurrency Control**: Core handles bus locking
3. **Resource Management**: Core manages chip select
4. **Error Handling**: Uniform error propagation

```c
/* WRONG: Direct hardware access in device driver */
static int bad_driver_read(struct spi_device *spi)
{
    void __iomem *base = ???;  /* No way to get controller registers! */
    writel(0x01, base + SPI_CMD);  /* Violates abstraction */
}

/* CORRECT: Use message API */
static int good_driver_read(struct spi_device *spi)
{
    unsigned char cmd = 0x01;
    unsigned char response;
    struct spi_transfer t = {
        .tx_buf = &cmd,
        .rx_buf = &response,
        .len = 1,
    };
    struct spi_message m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);

    return spi_sync(spi, &m);  /* [KEY] Let core handle it */
}
```

### 6.5 spi_driver Callbacks

```c
struct spi_driver {
    int  (*probe)(struct spi_device *spi);   /* Device bound */
    int  (*remove)(struct spi_device *spi);  /* Device unbound */
    void (*shutdown)(struct spi_device *spi);/* System shutdown */
    int  (*suspend)(struct spi_device *spi, pm_message_t mesg);
    int  (*resume)(struct spi_device *spi);
};
```

| Callback | When Called | Context | Core Guarantees | Driver Must |
|----------|-------------|---------|-----------------|-------------|
| `probe` | Match found | Process | Device configured | Initialize, return 0 or error |
| `remove` | Device going away | Process | No pending transfers | Clean up, free resources |
| `shutdown` | System shutdown | Process | - | Quiesce device |
| `suspend` | Power management | Process | - | Stop I/O, save state |
| `resume` | Power management | Process | - | Restore state, restart I/O |

---

## Phase 7 — Driver Registration & Matching

### 7.1 How spi_register_master() Works

```c
int spi_register_master(struct spi_master *master)
{
    /* Step 1: Validate */
    if (master->num_chipselect == 0)
        return -EINVAL;

    /* Step 2: Assign bus number */
    if (master->bus_num < 0)
        master->bus_num = atomic_dec_return(&dyn_bus_id);  /* Dynamic */

    /* Step 3: Initialize locks */
    spin_lock_init(&master->bus_lock_spinlock);
    mutex_init(&master->bus_lock_mutex);

    /* Step 4: Register with device model */
    dev_set_name(&master->dev, "spi%u", master->bus_num);
    device_add(&master->dev);

    /* Step 5: Match pending board info */
    mutex_lock(&board_lock);
    list_add_tail(&master->list, &spi_master_list);
    list_for_each_entry(bi, &board_list, list)
        spi_match_master_to_boardinfo(master, &bi->board_info);
    mutex_unlock(&board_lock);

    /* Step 6: Handle device tree */
    of_register_spi_devices(master);

    return 0;
}
```

### 7.2 How spi_register_driver() Works

```c
int spi_register_driver(struct spi_driver *sdrv)
{
    /* Associate with SPI bus */
    sdrv->driver.bus = &spi_bus_type;

    /* Set up wrapper callbacks */
    if (sdrv->probe)
        sdrv->driver.probe = spi_drv_probe;
    if (sdrv->remove)
        sdrv->driver.remove = spi_drv_remove;
    if (sdrv->shutdown)
        sdrv->driver.shutdown = spi_drv_shutdown;

    /* Generic driver registration */
    return driver_register(&sdrv->driver);
}
```

### 7.3 How Matching Occurs

```c
/* drivers/spi/spi.c */
static int spi_match_device(struct device *dev, struct device_driver *drv)
{
    const struct spi_device *spi = to_spi_device(dev);
    const struct spi_driver *sdrv = to_spi_driver(drv);

    /* Method 1: Device Tree matching */
    if (of_driver_match_device(dev, drv))
        return 1;

    /* Method 2: ID table matching */
    if (sdrv->id_table)
        return !!spi_match_id(sdrv->id_table, spi);

    /* Method 3: Name matching (modalias vs driver name) */
    return strcmp(spi->modalias, drv->name) == 0;
}

static const struct spi_device_id *spi_match_id(const struct spi_device_id *id,
                                                const struct spi_device *sdev)
{
    while (id->name[0]) {
        if (!strcmp(sdev->modalias, id->name))
            return id;  /* [KEY] Match found */
        id++;
    }
    return NULL;  /* No match */
}
```

**Matching Priority:**
1. Device Tree (`of_match_table`)
2. ID table (`id_table`)
3. Direct name match (`modalias` == `driver.name`)

### 7.4 Registration Flow Diagram

```
             spi_register_board_info()           spi_register_master()
                      |                                  |
                      v                                  v
              +---------------+                  +---------------+
              |  board_list   |                  | spi_master_list|
              +-------+-------+                  +-------+-------+
                      |                                  |
                      +-------------+--------------------+
                                    |
                                    v
                         spi_match_master_to_boardinfo()
                                    |
                                    v
                            spi_new_device()
                                    |
                                    v
                            spi_add_device()
                                    |
                                    v
                            device_add()
                                    |
            spi_register_driver()   |
                    |               |
                    v               v
              +-----+---------------+-----+
              |    spi_match_device()     |
              +---------------------------+
                          |
                          v
                   [Match Found?]
                      |     |
                     Yes    No
                      |     |
                      v     v
               spi_drv_probe()  (nothing)
                      |
                      v
               sdrv->probe(spi)
```

**中文解释：**
- 板级信息和控制器分别注册到全局列表
- 当控制器注册时，匹配板级信息创建`spi_device`
- 当驱动注册时，与现有设备匹配并调用`probe()`
- 匹配通过三种方式：设备树、ID表、名称

### 7.5 Probe Failure Handling

```c
int spi_add_device(struct spi_device *spi)
{
    /* ... validation ... */

    /* Call master's setup */
    status = spi_setup(spi);
    if (status < 0) {
        dev_err(dev, "can't setup %s, status %d\n",
                dev_name(&spi->dev), status);
        return status;  /* [KEY] Fail gracefully */
    }

    /* Register device - triggers probe if driver present */
    status = device_add(&spi->dev);
    if (status < 0)
        dev_err(dev, "can't add %s, status %d\n",
                dev_name(&spi->dev), status);

    return status;
}
```

If `probe()` fails:
1. Error code propagated back
2. Device model handles cleanup
3. `spi_device` remains registered (can retry with different driver)
4. Driver's `remove()` is NOT called

---

## Phase 8 — End-to-End Transfer Path

### 8.1 SPI Message Structure

```c
/* A complete SPI transaction */
struct spi_message {
    struct list_head    transfers;     /* [KEY] List of spi_transfer */
    struct spi_device   *spi;          /* Device for this message */
    unsigned            is_dma_mapped:1;

    /* Completion handling */
    void    (*complete)(void *context);/* [KEY] Callback when done */
    void    *context;                  /* Argument to complete() */

    unsigned    actual_length;         /* Bytes actually transferred */
    int         status;                /* [KEY] Result: 0 or -errno */

    /* For driver use */
    struct list_head    queue;         /* Controller's queue position */
    void                *state;        /* Controller state tracking */
};

/* A single transfer within a message */
struct spi_transfer {
    const void  *tx_buf;    /* Data to transmit (or NULL) */
    void        *rx_buf;    /* Data to receive (or NULL) */
    unsigned    len;        /* [KEY] Number of bytes */

    dma_addr_t  tx_dma;     /* DMA address (if is_dma_mapped) */
    dma_addr_t  rx_dma;

    unsigned    cs_change:1;    /* Deselect after this transfer? */
    u8          bits_per_word;  /* Override device default */
    u16         delay_usecs;    /* Delay before next transfer */
    u32         speed_hz;       /* Override device clock */

    struct list_head transfer_list;  /* Link in message */
};
```

### 8.2 Message Construction

```c
/* Option 1: Stack-allocated (common) */
int simple_transfer(struct spi_device *spi)
{
    struct spi_transfer t = {
        .tx_buf = tx_data,
        .rx_buf = rx_data,
        .len = 4,
    };
    struct spi_message m;

    spi_message_init(&m);           /* Initialize */
    spi_message_add_tail(&t, &m);   /* Add transfer */

    return spi_sync(spi, &m);       /* Execute */
}

/* Option 2: Dynamic allocation (for complex messages) */
struct spi_message *m = spi_message_alloc(3, GFP_KERNEL);  /* 3 transfers */
/* ... fill in transfers ... */
spi_async(spi, m);
/* ... later in complete callback ... */
spi_message_free(m);
```

### 8.3 spi_sync() - Synchronous Transfer

```c
/* Public API */
int spi_sync(struct spi_device *spi, struct spi_message *message)
{
    return __spi_sync(spi, message, 0);
}

/* Internal implementation */
static int __spi_sync(struct spi_device *spi, struct spi_message *message,
                      int bus_locked)
{
    DECLARE_COMPLETION_ONSTACK(done);  /* [KEY] On-stack completion */
    int status;
    struct spi_master *master = spi->master;

    /* Set up completion callback */
    message->complete = spi_complete;  /* [KEY] Generic complete function */
    message->context = &done;

    /* Lock bus if not already locked */
    if (!bus_locked)
        mutex_lock(&master->bus_lock_mutex);

    /* Submit asynchronously */
    status = spi_async_locked(spi, message);  /* [KEY] Queue message */

    if (!bus_locked)
        mutex_unlock(&master->bus_lock_mutex);

    /* Wait for completion */
    if (status == 0) {
        wait_for_completion(&done);  /* [KEY] Block until done */
        status = message->status;
    }
    message->context = NULL;
    return status;
}

static void spi_complete(void *arg)
{
    complete(arg);  /* Wake up waiter */
}
```

### 8.4 spi_async() - Asynchronous Transfer

```c
int spi_async(struct spi_device *spi, struct spi_message *message)
{
    struct spi_master *master = spi->master;
    int ret;
    unsigned long flags;

    /* Check if bus is locked for exclusive use */
    spin_lock_irqsave(&master->bus_lock_spinlock, flags);
    if (master->bus_lock_flag)
        ret = -EBUSY;  /* [KEY] Bus locked, reject */
    else
        ret = __spi_async(spi, message);
    spin_unlock_irqrestore(&master->bus_lock_spinlock, flags);

    return ret;
}

static int __spi_async(struct spi_device *spi, struct spi_message *message)
{
    struct spi_master *master = spi->master;

    /* Validate half-duplex constraints */
    if ((master->flags & SPI_MASTER_HALF_DUPLEX) ...) {
        struct spi_transfer *xfer;
        list_for_each_entry(xfer, &message->transfers, transfer_list) {
            if (xfer->rx_buf && xfer->tx_buf)
                return -EINVAL;  /* Full duplex not allowed */
        }
    }

    /* Mark message as in-progress */
    message->spi = spi;
    message->status = -EINPROGRESS;

    /* [KEY] Hand off to controller driver */
    return master->transfer(spi, message);
}
```

### 8.5 Controller Driver Message Processing

**Example: PL022 Controller**

```c
/* drivers/spi/spi-pl022.c */

/* transfer() callback - just queues the message */
static int pl022_transfer(struct spi_device *spi, struct spi_message *msg)
{
    struct pl022 *pl022 = spi_master_get_devdata(spi->master);
    unsigned long flags;

    spin_lock_irqsave(&pl022->queue_lock, flags);

    if (!pl022->running) {
        spin_unlock_irqrestore(&pl022->queue_lock, flags);
        return -ESHUTDOWN;
    }

    /* Initialize message state */
    msg->actual_length = 0;
    msg->status = -EINPROGRESS;
    msg->state = STATE_START;

    /* [KEY] Add to queue */
    list_add_tail(&msg->queue, &pl022->queue);

    /* Wake up the work queue if not busy */
    if (pl022->running && !pl022->busy)
        queue_work(pl022->workqueue, &pl022->pump_messages);

    spin_unlock_irqrestore(&pl022->queue_lock, flags);
    return 0;  /* [KEY] Return immediately */
}

/* Work function - actually processes messages */
static void pump_messages(struct work_struct *work)
{
    struct pl022 *pl022 = container_of(work, struct pl022, pump_messages);

    /* Get next message from queue */
    /* Select chip */
    /* For each transfer:
     *   - Configure hardware
     *   - Start DMA or poll
     *   - Wait for completion
     */
    /* Deselect chip */
    /* Call message->complete() */
}

/* Completion - called when message is done */
static void giveback(struct pl022 *pl022)
{
    struct spi_message *msg = pl022->cur_msg;

    /* ... cleanup ... */

    msg->state = NULL;
    if (msg->complete)
        msg->complete(msg->context);  /* [KEY] Notify caller */
}
```

### 8.6 Complete Transfer Path Diagram

```
Device Driver                 SPI Core                   Controller Driver
     |                           |                              |
     | spi_sync(spi, msg)        |                              |
     |-------------------------->|                              |
     |                           | message->complete = spi_complete
     |                           | message->context = &done
     |                           |                              |
     |                           | mutex_lock(bus_lock_mutex)   |
     |                           |                              |
     |                           | spi_async_locked()           |
     |                           |------------------------->|   |
     |                           |                          |   |
     |                           |     __spi_async()        |   |
     |                           |     - validate           |   |
     |                           |     - msg->spi = spi     |   |
     |                           |     - msg->status = -EINPROGRESS
     |                           |                          |   |
     |                           |     master->transfer()   |   |
     |                           |------------------------->|   |
     |                           |                          |   v
     |                           |                    pl022_transfer()
     |                           |                          |
     |                           |                    [add to queue]
     |                           |                    [queue_work]
     |                           |                          |
     |                           |<-------------------------|
     |                           | return 0                 |
     |                           |                          |
     |                           | mutex_unlock()           |
     |                           |                          |
     | wait_for_completion(&done)|                          |
     | <BLOCKED>                 |                          |
     |                           |                   [workqueue runs]
     |                           |                   pump_messages()
     |                           |                          |
     |                           |                   [select chip]
     |                           |                   [configure hw]
     |                           |                   [DMA/poll xfer]
     |                           |                   [deselect chip]
     |                           |                          |
     |                           |                   msg->status = 0
     |                           |                   msg->complete(context)
     |                           |<-------------------------|
     |                           |                          |
     | <WOKEN UP>                |                          |
     |<--------------------------|                          |
     | return msg->status        |                          |
```

**中文解释：**
1. 设备驱动调用`spi_sync()`
2. 核心设置完成回调并获取总线锁
3. 消息通过`master->transfer()`传递给控制器驱动
4. 控制器驱动将消息加入队列并立即返回
5. 设备驱动阻塞等待完成
6. 工作队列处理消息：选择芯片、传输数据、取消选择
7. 完成后调用回调，唤醒等待的驱动

---

## Phase 9 — Concurrency, Context, and Performance

### 9.1 Which SPI APIs May Sleep

| API | May Sleep? | Context |
|-----|------------|---------|
| `spi_sync()` | Yes | Process only |
| `spi_async()` | No | Any (IRQ, softirq, process) |
| `spi_write()` | Yes | Process only |
| `spi_read()` | Yes | Process only |
| `spi_write_then_read()` | Yes | Process only |
| `spi_setup()` | Yes | Process only |
| `spi_register_driver()` | Yes | Process only |

### 9.2 Locking in the SPI Core

```c
struct spi_master {
    spinlock_t      bus_lock_spinlock;  /* Protects bus_lock_flag */
    struct mutex    bus_lock_mutex;     /* Serializes bus access */
    bool            bus_lock_flag;      /* Exclusive access active? */
};
```

**Locking Hierarchy:**

```
                    +-------------------+
                    | board_lock mutex  |  <-- Protects global lists
                    +-------------------+
                             |
            +----------------+----------------+
            |                                 |
    +-------v--------+               +--------v-------+
    | spi_add_lock   |               | bus_lock_mutex |
    | (per-device)   |               | (per-master)   |
    +----------------+               +----------------+
                                              |
                                     +--------v--------+
                                     |bus_lock_spinlock|
                                     | (protects flag) |
                                     +-----------------+
```

**Bus Locking for Exclusive Access:**

```c
/* Lock bus for multiple operations */
spi_bus_lock(master);

/* Now use _locked variants */
spi_async_locked(spi, msg1);
spi_sync_locked(spi, msg2);

/* Release bus */
spi_bus_unlock(master);
```

### 9.3 Interrupt vs Polling Transfer Models

**Polling (Bitbang):**
```c
/* drivers/spi/spi-bitbang.c */
static void bitbang_work(struct work_struct *work)
{
    while (!list_empty(&bitbang->queue)) {
        m = list_first_entry(&bitbang->queue, ...);

        list_for_each_entry(t, &m->transfers, transfer_list) {
            /* Directly transfer data, word by word */
            status = bitbang->txrx_bufs(spi, t);  /* [KEY] Blocking I/O */
        }

        m->complete(m->context);
    }
}
```
- Simple implementation
- CPU busy during transfer
- Good for slow devices, low data rates

**Interrupt-driven (PL022):**
```c
/* drivers/spi/spi-pl022.c */
static irqreturn_t pl022_interrupt_handler(int irq, void *dev_id)
{
    /* Check what caused interrupt */
    if (readw(SSP_MIS(pl022->virtbase)) & SSP_MIS_MASK_RXMIS) {
        /* RX FIFO has data - read it */
        readfifo(pl022);
    }

    if (/* TX FIFO needs data */) {
        writeFIFO(pl022);
    }

    if (/* Transfer complete */) {
        /* Move to next transfer or complete message */
        tasklet_schedule(&pl022->pump_transfers);
    }

    return IRQ_HANDLED;
}
```
- More complex
- CPU free during transfer
- Essential for high-speed, DMA operations

### 9.4 Sync vs Async Performance Trade-offs

| Aspect | spi_sync() | spi_async() |
|--------|------------|-------------|
| **Latency** | Higher (completion wait) | Lower (immediate return) |
| **Throughput** | Lower (serialized) | Higher (can pipeline) |
| **Complexity** | Simple (blocking) | Complex (callbacks) |
| **Context** | Process only | Any |
| **Error Handling** | Immediate return value | Check in callback |
| **Memory** | Stack or heap | Heap only (must persist) |

**When to Use Each:**

```c
/* Use spi_sync() for: */
/* - Simple, infrequent transfers */
/* - When blocking is acceptable */
/* - When you need the result immediately */
ret = spi_sync(spi, &msg);
if (ret)
    handle_error();

/* Use spi_async() for: */
/* - High-throughput scenarios */
/* - When called from IRQ context */
/* - When pipelining multiple transfers */
msg->complete = my_callback;
ret = spi_async(spi, msg);
/* ... result comes later via callback ... */
```

### 9.5 DMA Interaction (Conceptual)

```c
/* DMA-enabled transfer */
struct spi_transfer t = {
    .tx_buf = dma_buffer,
    .rx_buf = dma_buffer,
    .len = 4096,
};
struct spi_message m;

spi_message_init(&m);

/* Option 1: Let controller handle DMA mapping */
spi_message_add_tail(&t, &m);
spi_sync(spi, &m);

/* Option 2: Pre-map DMA (for reuse) */
t.tx_dma = dma_map_single(...);
t.rx_dma = dma_map_single(...);
m.is_dma_mapped = 1;  /* [KEY] Tell controller buffers are pre-mapped */
spi_sync(spi, &m);
dma_unmap_single(...);
```

**Controller DMA Implementation (PL022):**

```c
static int configure_dma(struct pl022 *pl022)
{
    /* Set up DMA descriptors */
    desc_tx = dmaengine_prep_slave_sg(pl022->dma_tx_channel, ...);
    desc_rx = dmaengine_prep_slave_sg(pl022->dma_rx_channel, ...);

    /* Enable DMA in SSP */
    SSP_WRITE_BITS(chip->dmacr, SSP_DMA_ENABLED, SSP_DMACR_MASK_RXDMAE, 0);
    SSP_WRITE_BITS(chip->dmacr, SSP_DMA_ENABLED, SSP_DMACR_MASK_TXDMAE, 1);

    /* Start DMA */
    dmaengine_submit(desc_tx);
    dmaengine_submit(desc_rx);
    dma_async_issue_pending(pl022->dma_tx_channel);
    dma_async_issue_pending(pl022->dma_rx_channel);
}
```

---

## Phase 10 — Common Bugs, Pitfalls, and Design Lessons

### 10.1 Common SPI Driver Bugs

#### Bug 1: Sleeping in Atomic Context

```c
/* WRONG: spi_sync() called from IRQ handler */
static irqreturn_t my_irq_handler(int irq, void *data)
{
    struct my_device *dev = data;
    u8 cmd = 0x01;

    spi_write(dev->spi, &cmd, 1);  /* [BUG] spi_write calls spi_sync! */
    return IRQ_HANDLED;
}

/* CORRECT: Use async or defer to workqueue */
static irqreturn_t my_irq_handler(int irq, void *data)
{
    struct my_device *dev = data;
    schedule_work(&dev->work);  /* Defer to process context */
    return IRQ_HANDLED;
}

static void my_work_handler(struct work_struct *work)
{
    struct my_device *dev = container_of(work, struct my_device, work);
    u8 cmd = 0x01;
    spi_write(dev->spi, &cmd, 1);  /* OK in process context */
}
```

#### Bug 2: Incorrect Chip Select Handling

```c
/* WRONG: Assuming chip select persists between messages */
static int bad_sequence(struct spi_device *spi)
{
    spi_write(spi, cmd1, 2);   /* CS asserted, then deasserted */
    /* [BUG] Device may reset between messages! */
    spi_write(spi, cmd2, 2);   /* CS asserted again */
}

/* CORRECT: Use single message with multiple transfers */
static int good_sequence(struct spi_device *spi)
{
    struct spi_transfer t[] = {
        { .tx_buf = cmd1, .len = 2, .cs_change = 0 },  /* Keep CS */
        { .tx_buf = cmd2, .len = 2 },
    };
    struct spi_message m;

    spi_message_init(&m);
    spi_message_add_tail(&t[0], &m);
    spi_message_add_tail(&t[1], &m);
    return spi_sync(spi, &m);  /* [KEY] CS held throughout */
}
```

#### Bug 3: spi_message Lifetime Misuse

```c
/* WRONG: Stack-allocated message with async */
static int bad_async(struct spi_device *spi)
{
    struct spi_message m;  /* [BUG] On stack! */
    struct spi_transfer t = { ... };

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);

    return spi_async(spi, &m);  /* Returns immediately */
}   /* Stack frame destroyed - m and t invalid! */

/* CORRECT: Heap-allocated for async */
static int good_async(struct spi_device *spi)
{
    struct my_async_data *data;

    data = kmalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->t.tx_buf = data->buf;
    data->t.len = 4;

    spi_message_init(&data->m);
    spi_message_add_tail(&data->t, &data->m);
    data->m.complete = my_complete;
    data->m.context = data;

    return spi_async(spi, &data->m);  /* data lives until callback */
}

static void my_complete(void *context)
{
    struct my_async_data *data = context;
    /* ... process result ... */
    kfree(data);  /* Now safe to free */
}
```

#### Bug 4: Missing Error Checks

```c
/* WRONG: Ignoring return values */
static void bad_init(struct spi_device *spi)
{
    spi_setup(spi);  /* [BUG] Might fail! */
    spi_write(spi, cmd, 1);
}

/* CORRECT: Check all returns */
static int good_init(struct spi_device *spi)
{
    int ret;

    ret = spi_setup(spi);
    if (ret) {
        dev_err(&spi->dev, "setup failed: %d\n", ret);
        return ret;
    }

    ret = spi_write(spi, cmd, 1);
    if (ret) {
        dev_err(&spi->dev, "write failed: %d\n", ret);
        return ret;
    }

    return 0;
}
```

#### Bug 5: Concurrency Violations

```c
/* WRONG: Unprotected shared state */
struct my_device {
    struct spi_device *spi;
    u8 current_mode;  /* Accessed by multiple threads */
};

static int change_mode(struct my_device *dev, u8 mode)
{
    dev->current_mode = mode;  /* [BUG] Race condition! */
    return spi_write(dev->spi, &mode, 1);
}

/* CORRECT: Proper locking */
struct my_device {
    struct spi_device *spi;
    struct mutex lock;
    u8 current_mode;
};

static int change_mode(struct my_device *dev, u8 mode)
{
    int ret;

    mutex_lock(&dev->lock);
    dev->current_mode = mode;
    ret = spi_write(dev->spi, &mode, 1);
    mutex_unlock(&dev->lock);

    return ret;
}
```

### 10.2 What Makes a Good SPI Controller Driver

1. **Correct Callback Implementation**
   - `transfer()` must not block
   - `setup()` validates all parameters
   - `cleanup()` frees all per-device resources

2. **Proper Message Processing**
   - Handle all edge cases (empty transfers, zero length)
   - Respect `cs_change` flags
   - Maintain accurate `actual_length`

3. **Efficient Resource Usage**
   - Use DMA for large transfers
   - Minimize interrupt overhead
   - Support runtime power management

4. **Robust Error Handling**
   - Timeout detection
   - FIFO overrun/underrun handling
   - Complete messages even on error

### 10.3 What Makes a Good SPI Device Driver

1. **Clean Probe/Remove**
   - Initialize everything in probe
   - Clean up everything in remove (reverse order)
   - Handle partial initialization failures

2. **Correct API Usage**
   - Use `spi_sync()` from process context only
   - Use `spi_async()` with proper lifetime management
   - Combine related operations into single messages

3. **Proper Private Data**
   - Use `spi_set_drvdata()`/`spi_get_drvdata()`
   - Protect shared state with locks
   - Store back-reference to `spi_device`

4. **Defensive Programming**
   - Check return values
   - Validate platform data
   - Handle hardware quirks

### 10.4 How SPI Architecture Generalizes

The SPI subsystem patterns apply to other bus subsystems:

| Pattern | SPI | I²C | Platform |
|---------|-----|-----|----------|
| Controller abstraction | `spi_master` | `i2c_adapter` | `platform_device` |
| Device representation | `spi_device` | `i2c_client` | N/A |
| Protocol driver | `spi_driver` | `i2c_driver` | `platform_driver` |
| Transfer mechanism | `spi_message` | `i2c_msg` | N/A |
| Matching | modalias/id_table | name/id_table | name/id_table |

**Key Design Principles:**

1. **Separation of Concerns**
   - Controller knows HOW (hardware access)
   - Device knows WHAT (protocol)
   - Core knows WHEN (scheduling, matching)

2. **Uniform API**
   - Device drivers are portable
   - Controller drivers are replaceable
   - Board code is isolated

3. **Layered Abstraction**
   - Each layer has clear responsibilities
   - Contracts enforced by API design
   - Extension without modification

---

## Appendix A — Walking Through Real SPI Drivers

### A.1 Driver Comparison Overview

| Aspect | PL022 (Controller) | spi-gpio (Controller) | TLE62x0 (Device) |
|--------|--------------------|-----------------------|------------------|
| Type | Hardware SPI | Bitbang GPIO | Protocol driver |
| Complexity | High (IRQ, DMA, FIFO) | Medium (bitbang) | Low |
| Transfer | Async workqueue | Sync bitbang | Uses spi_sync |
| Context | IRQ + process | Process only | Process only |

### A.2 Controller Driver: spi-gpio.c (Bitbang)

**Architecture:**

```
+-------------------+
|   spi_gpio        |
| +---------------+ |
| | spi_bitbang   | |  <-- Embeds bitbang framework
| | - workqueue   | |
| | - queue       | |
| | - chipselect  | |
| | - txrx_word[] | |
| +---------------+ |
| - pdata           |  <-- GPIO numbers
| - pdev            |
+-------------------+
```

**Probe Function Analysis:**

```c
static int __devinit spi_gpio_probe(struct platform_device *pdev)
{
    struct spi_master       *master;
    struct spi_gpio         *spi_gpio;
    struct spi_gpio_platform_data *pdata;
    u16 master_flags = 0;

    pdata = pdev->dev.platform_data;

    /* [STEP 1] Request GPIOs */
    status = spi_gpio_request(pdata, dev_name(&pdev->dev), &master_flags);
    if (status < 0)
        return status;

    /* [STEP 2] Allocate master with extra space for spi_gpio */
    master = spi_alloc_master(&pdev->dev, sizeof *spi_gpio);
    if (!master) {
        status = -ENOMEM;
        goto gpio_free;
    }

    /* [STEP 3] Get private data pointer */
    spi_gpio = spi_master_get_devdata(master);
    platform_set_drvdata(pdev, spi_gpio);

    /* [STEP 4] Initialize platform data */
    spi_gpio->pdev = pdev;
    if (pdata)
        spi_gpio->pdata = *pdata;

    /* [STEP 5] Configure master */
    master->flags = master_flags;
    master->bus_num = pdev->id;
    master->num_chipselect = SPI_N_CHIPSEL;
    master->setup = spi_gpio_setup;      /* [KEY] Per-device setup */
    master->cleanup = spi_gpio_cleanup;

    /* [STEP 6] Configure bitbang framework */
    spi_gpio->bitbang.master = spi_master_get(master);
    spi_gpio->bitbang.chipselect = spi_gpio_chipselect;  /* [KEY] CS control */

    /* [KEY] Word-level transfer functions for each SPI mode */
    spi_gpio->bitbang.txrx_word[SPI_MODE_0] = spi_gpio_txrx_word_mode0;
    spi_gpio->bitbang.txrx_word[SPI_MODE_1] = spi_gpio_txrx_word_mode1;
    spi_gpio->bitbang.txrx_word[SPI_MODE_2] = spi_gpio_txrx_word_mode2;
    spi_gpio->bitbang.txrx_word[SPI_MODE_3] = spi_gpio_txrx_word_mode3;

    spi_gpio->bitbang.setup_transfer = spi_bitbang_setup_transfer;

    /* [STEP 7] Start bitbang and register master */
    status = spi_bitbang_start(&spi_gpio->bitbang);
    if (status < 0) {
        spi_master_put(spi_gpio->bitbang.master);
        /* cleanup... */
    }

    return status;
}
```

**Bitbang Transfer:**

```c
/* spi-bitbang-txrx.h - Mode 0 transfer */
static inline u32
bitbang_txrx_be_cpha0(struct spi_device *spi,
                      unsigned nsecs, unsigned cpol, unsigned flags,
                      u32 word, u8 bits)
{
    /* Sample on leading edge, setup on trailing edge */
    for (word <<= (32 - bits); likely(bits); bits--) {

        /* setup: write MOSI */
        if ((flags & SPI_MASTER_NO_TX) == 0)
            setmosi(spi, word & (1 << 31));  /* [KEY] Set output bit */
        spidelay(nsecs);

        /* clock: rising edge */
        setsck(spi, !cpol);
        spidelay(nsecs);

        /* sample: read MISO */
        if ((flags & SPI_MASTER_NO_RX) == 0)
            word |= (getmiso(spi) << 31);    /* [KEY] Read input bit */
        word <<= 1;

        /* clock: falling edge */
        setsck(spi, cpol);
    }
    return word;
}
```

### A.3 Controller Driver: spi-pl022.c (Hardware SPI)

**Architecture:**

```
+---------------------+
|   struct pl022      |
| +-------------+     |
| | Hardware    |     |
| | - virtbase  |---->[ SPI Registers ]
| | - clk       |     |
| | - irq       |     |
| +-------------+     |
| +-------------+     |
| | Workqueue   |     |
| | - queue     |<--->[ Message list ]
| | - busy      |     |
| +-------------+     |
| +-------------+     |
| | Current     |     |
| | - cur_msg   |---->[ spi_message ]
| | - cur_chip  |---->[ chip_data ]
| +-------------+     |
| +-------------+     |
| | DMA         |     |
| | - dma_rx    |     |
| | - dma_tx    |     |
| +-------------+     |
+---------------------+
```

**Probe Function (Key Parts):**

```c
static int __devinit pl022_probe(struct amba_device *adev, ...)
{
    /* [STEP 1] Allocate master + private data */
    master = spi_alloc_master(dev, sizeof(struct pl022));
    pl022 = spi_master_get_devdata(master);
    pl022->master = master;

    /* [STEP 2] Configure master callbacks */
    master->bus_num = platform_info->bus_id;
    master->num_chipselect = platform_info->num_chipselect;
    master->cleanup = pl022_cleanup;
    master->setup = pl022_setup;
    master->transfer = pl022_transfer;  /* [KEY] Message queuing */

    /* [STEP 3] Map hardware registers */
    pl022->virtbase = ioremap(adev->res.start, resource_size(&adev->res));

    /* [STEP 4] Get clock and enable */
    pl022->clk = clk_get(&adev->dev, NULL);
    clk_enable(pl022->clk);

    /* [STEP 5] Register IRQ handler */
    request_irq(adev->irq[0], pl022_interrupt_handler, 0, "pl022", pl022);

    /* [STEP 6] Setup DMA if enabled */
    if (platform_info->enable_dma)
        pl022_dma_probe(pl022);

    /* [STEP 7] Initialize message queue */
    init_queue(pl022);
    start_queue(pl022);

    /* [STEP 8] Register with SPI framework */
    spi_register_master(master);

    return 0;
}
```

**Message Queuing:**

```c
static int pl022_transfer(struct spi_device *spi, struct spi_message *msg)
{
    struct pl022 *pl022 = spi_master_get_devdata(spi->master);
    unsigned long flags;

    spin_lock_irqsave(&pl022->queue_lock, flags);

    if (!pl022->running) {
        spin_unlock_irqrestore(&pl022->queue_lock, flags);
        return -ESHUTDOWN;
    }

    /* [KEY] Initialize message state */
    msg->actual_length = 0;
    msg->status = -EINPROGRESS;
    msg->state = STATE_START;

    /* [KEY] Add to tail of queue */
    list_add_tail(&msg->queue, &pl022->queue);

    /* [KEY] Wake up worker if idle */
    if (pl022->running && !pl022->busy)
        queue_work(pl022->workqueue, &pl022->pump_messages);

    spin_unlock_irqrestore(&pl022->queue_lock, flags);
    return 0;  /* [KEY] Return immediately */
}
```

### A.4 Device Driver: spi-tle62x0.c

**Simple Read/Write Operations:**

```c
/* Write to device */
static inline int tle62x0_write(struct tle62x0_state *st)
{
    unsigned char *buff = st->tx_buff;
    unsigned int gpio_state = st->gpio_state;

    buff[0] = CMD_SET;

    if (st->nr_gpio == 16) {
        buff[1] = gpio_state >> 8;
        buff[2] = gpio_state;
    } else {
        buff[1] = gpio_state;
    }

    /* [KEY] Use simple spi_write helper */
    return spi_write(st->us, buff, (st->nr_gpio == 16) ? 3 : 2);
}

/* Read from device */
static inline int tle62x0_read(struct tle62x0_state *st)
{
    unsigned char *txbuff = st->tx_buff;

    /* [KEY] Prepare transfer with both TX and RX */
    struct spi_transfer xfer = {
        .tx_buf     = txbuff,
        .rx_buf     = st->rx_buff,
        .len        = (st->nr_gpio * 2) / 8,
    };
    struct spi_message msg;

    txbuff[0] = CMD_READ;
    txbuff[1] = 0x00;
    txbuff[2] = 0x00;
    txbuff[3] = 0x00;

    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);

    /* [KEY] Synchronous full-duplex transfer */
    return spi_sync(st->us, &msg);
}
```

**Probe Function:**

```c
static int __devinit tle62x0_probe(struct spi_device *spi)
{
    struct tle62x0_state *st;
    struct tle62x0_pdata *pdata;

    /* [STEP 1] Get platform data */
    pdata = spi->dev.platform_data;
    if (!pdata) {
        dev_err(&spi->dev, "no device data specified\n");
        return -EINVAL;
    }

    /* [STEP 2] Allocate private state */
    st = kzalloc(sizeof(struct tle62x0_state), GFP_KERNEL);
    if (!st) {
        dev_err(&spi->dev, "no memory for device state\n");
        return -ENOMEM;
    }

    /* [STEP 3] Initialize state */
    st->us = spi;  /* [KEY] Store spi_device reference */
    st->nr_gpio = pdata->gpio_count;
    st->gpio_state = pdata->init_state;
    mutex_init(&st->lock);

    /* [STEP 4] Create sysfs interface */
    ret = device_create_file(&spi->dev, &dev_attr_status_show);
    if (ret)
        goto err_status;

    for (ptr = 0; ptr < pdata->gpio_count; ptr++) {
        ret = device_create_file(&spi->dev, gpio_attrs[ptr]);
        if (ret)
            goto err_gpios;
    }

    /* [STEP 5] Store private data */
    spi_set_drvdata(spi, st);  /* [KEY] Attach to spi_device */

    return 0;

err_gpios:
    /* ... cleanup ... */
err_status:
    kfree(st);
    return ret;
}
```

### A.5 Complete Call Flow: GPIO Write via TLE62x0

```
User writes to /sys/devices/.../gpio1
              |
              v
tle62x0_gpio_store()
              |
              | mutex_lock(&st->lock)
              | st->gpio_state |= (1 << gpio_num)
              |
              v
tle62x0_write(st)
              |
              | buff[0] = CMD_SET
              | buff[1] = gpio_state
              |
              v
spi_write(st->us, buff, 2)
              |
              v
+-------------+------------------+
| spi_sync()                     |
|   - spi_message_init(&m)       |
|   - t.tx_buf = buff            |
|   - spi_message_add_tail(&t)   |
|   - __spi_sync()               |
+------|-------------------------+
       |
       v
+------+--------------------------+
| spi_async_locked()              |
|   - message->spi = spi          |
|   - master->transfer(spi, msg)  |
+------|-------------------------+
       |
       v (to spi-gpio)
+------+--------------------------+
| spi_bitbang_transfer()          |
|   - list_add_tail(&m->queue)    |
|   - queue_work(workqueue)       |
+------|-------------------------+
       |
       v
+------+--------------------------+
| bitbang_work() [workqueue]      |
|   - m = list_first_entry()      |
|   - spi_gpio_chipselect(ACTIVE) |
|   - bitbang_txrx_8()            |
|       for each byte:            |
|         for each bit:           |
|           setmosi()  → GPIO     |
|           setsck(1)  → GPIO     |
|           setsck(0)  → GPIO     |
|   - spi_gpio_chipselect(INACT)  |
|   - m->complete(m->context)     |
+------|-------------------------+
       |
       v
spi_complete() wakes spi_sync()
              |
              v
spi_sync() returns 0
              |
              v
tle62x0_gpio_store() returns len
              |
              | mutex_unlock(&st->lock)
              v
User sees write success
```

**中文解释：**
1. 用户通过sysfs写入GPIO值
2. TLE62x0驱动构造命令并调用`spi_write()`
3. SPI核心将消息传递给spi-gpio控制器驱动
4. Bitbang工作队列逐位传输数据
5. 完成后回调唤醒等待的`spi_sync()`
6. 返回成功，用户看到写入完成

### A.6 Driver Development Checklist

#### Controller Driver Checklist

- [ ] `spi_alloc_master()` with correct private data size
- [ ] Initialize all `spi_master` fields:
  - [ ] `bus_num`
  - [ ] `num_chipselect`
  - [ ] `mode_bits`
  - [ ] `setup`
  - [ ] `transfer`
  - [ ] `cleanup`
- [ ] Handle all SPI modes if claimed in `mode_bits`
- [ ] Implement proper chip select timing
- [ ] Handle message queue correctly
- [ ] Call `message->complete()` for ALL messages (even errors)
- [ ] Track `message->actual_length` accurately
- [ ] Set `message->status` before calling complete
- [ ] Proper locking for queue access
- [ ] Handle controller unload (drain queue, unregister)
- [ ] Power management (suspend/resume)

#### Device Driver Checklist

- [ ] Define `spi_driver` with all needed callbacks
- [ ] `probe()`:
  - [ ] Validate platform data
  - [ ] Allocate private data
  - [ ] Initialize `spi_device` if needed (`spi_setup`)
  - [ ] Attach private data (`spi_set_drvdata`)
  - [ ] Handle errors with proper cleanup
- [ ] `remove()`:
  - [ ] Get private data (`spi_get_drvdata`)
  - [ ] Free all resources (reverse order of probe)
- [ ] SPI transfers:
  - [ ] Use `spi_sync()` only from process context
  - [ ] Proper buffer lifetime for `spi_async()`
  - [ ] Check return values
  - [ ] Handle partial transfers
- [ ] Locking for shared state
- [ ] Module alias for modprobe (`MODULE_ALIAS("spi:xxx")`)

---

## Summary: SPI Subsystem Design Principles

1. **Layered Architecture**
   - Controller drivers: hardware access
   - SPI core: routing, matching, lifecycle
   - Device drivers: protocol implementation

2. **Message-Based Transfer**
   - Atomic message sequences
   - Flexible transfer chaining
   - Async-first design (sync built on async)

3. **Uniform API**
   - Device drivers portable across controllers
   - Clear contracts at each layer
   - Extension through callbacks

4. **Explicit Configuration**
   - No device discovery (static board info)
   - Platform data carries device parameters
   - Controller capabilities exposed via `mode_bits`

5. **Ownership Clarity**
   - Master owns hardware resources
   - Core owns device matching
   - Driver owns protocol logic

These principles make the SPI subsystem scalable, maintainable, and adaptable to diverse hardware while providing a consistent programming model for device drivers.

---

## Appendix B — SPI vs I²C vs UART Architecture Comparison

### B.1 Fundamental Protocol Differences

```
+------------------------------------------------------------------+
|                    PHYSICAL LAYER COMPARISON                       |
+------------------------------------------------------------------+
|                                                                    |
|  SPI (Serial Peripheral Interface)                                 |
|  +---------+      SCLK ─────────────>      +---------+            |
|  |         |      MOSI ─────────────>      |         |            |
|  | Master  |      MISO <─────────────      | Slave   |            |
|  |         |      CS   ─────────────>      |         |            |
|  +---------+                               +---------+            |
|  4-wire, full-duplex, hardware addressing via CS                   |
|                                                                    |
+------------------------------------------------------------------+
|                                                                    |
|  I²C (Inter-Integrated Circuit)                                    |
|  +---------+      SDA <────────────>       +---------+            |
|  |         |         (bidirectional)       |         |            |
|  | Master  |      SCL ─────────────>       | Slave   |            |
|  |         |                               | (addr)  |            |
|  +---------+                               +---------+            |
|  2-wire, half-duplex, software addressing (7/10-bit)               |
|                                                                    |
+------------------------------------------------------------------+
|                                                                    |
|  UART (Universal Asynchronous Receiver/Transmitter)                |
|  +---------+      TX  ─────────────>       +---------+            |
|  |         |      RX  <─────────────       |         |            |
|  | Device  |     (GND) ─────────────       | Device  |            |
|  |         |                               |         |            |
|  +---------+                               +---------+            |
|  2-wire (+ GND), full-duplex, point-to-point, no clock             |
|                                                                    |
+------------------------------------------------------------------+
```

**中文解释：**
- **SPI**：4线制，全双工，使用硬件片选(CS)寻址，需要主设备提供时钟
- **I²C**：2线制，半双工，使用软件地址寻址，支持多主设备
- **UART**：2线制(+地线)，全双工，点对点连接，无时钟线(异步)

### B.2 Architectural Comparison Table

| Aspect | SPI | I²C | UART |
|--------|-----|-----|------|
| **Topology** | Star (one master, many slaves) | Multi-master bus | Point-to-point |
| **Addressing** | Hardware (CS lines) | Software (7/10-bit addr) | None (dedicated link) |
| **Duplex** | Full | Half | Full |
| **Clock** | Master provides | Master provides | No shared clock |
| **Speed** | 10-100+ MHz | 100-3400 kHz | 9600-921600 baud typical |
| **Wire Count** | 3 + N (N = slaves) | 2 | 2 (+flow control) |
| **Discovery** | Not possible | Possible (limited) | Not applicable |
| **Arbitration** | CS-based (no conflict) | Clock stretching, NACK | N/A (point-to-point) |
| **Error Detection** | None (protocol level) | ACK/NACK | Parity (optional) |
| **Max Distance** | Short (PCB) | Short (PCB, ~1m) | Long (RS-232: 15m+) |

### B.3 Linux Kernel Architecture Comparison

```
+-------------------------------------------------------------------------+
|                    KERNEL SUBSYSTEM ARCHITECTURE                         |
+-------------------------------------------------------------------------+

                SPI                    I²C                    UART/TTY
           +----------+           +----------+           +----------+
           | spi.c    |           | i2c-core |           | tty_io.c |
           | (core)   |           | .c       |           | (core)   |
           +----+-----+           +----+-----+           +----+-----+
                |                      |                      |
    +-----------+-----------+    +-----+-----+    +-----------+-----------+
    |           |           |    |           |    |           |           |
+---v---+   +---v---+   +---v---+  +---v---+   +---v---+   +---v---+
|spi_   |   |spi_   |   |spi_   |  |i2c_   |   |i2c_   |   |tty_   |   |tty_   |
|master |   |device |   |driver |  |adapter|   |client |   |driver |   |ldisc  |
+-------+   +-------+   +-------+  +-------+   +-------+   +-------+   +-------+
   |           |           |          |           |           |           |
   v           |           |          v           |           v           v
[Controller]   |           |    [Controller]      |     [UART HW]    [Line
               |           |                      |                   Discipline]
               v           v                      v
          [SPI Slave]  [Protocol]            [I²C Device]
                       [Handler]             [+ Driver]

```

**Key Structural Differences:**

| Component | SPI | I²C | UART |
|-----------|-----|-----|------|
| Controller | `spi_master` | `i2c_adapter` | `uart_port` + `uart_driver` |
| Device | `spi_device` | `i2c_client` | N/A (point-to-point) |
| Driver | `spi_driver` | `i2c_driver` | `tty_driver` + `tty_ldisc` |
| Algorithm | Direct in master | `i2c_algorithm` | N/A |
| Transfer | `spi_message` | `i2c_msg` | Byte stream |
| Matching | modalias | name/addr | Fixed binding |

### B.4 Transfer Model Comparison

**SPI: Message-Based Atomic Transfers**

```c
/* SPI: Multiple transfers in one atomic message */
struct spi_transfer t[] = {
    { .tx_buf = cmd,  .len = 1 },
    { .tx_buf = addr, .len = 2 },
    { .rx_buf = data, .len = 256 },
};
struct spi_message m;
spi_message_init(&m);
spi_message_add_tail(&t[0], &m);
spi_message_add_tail(&t[1], &m);
spi_message_add_tail(&t[2], &m);
spi_sync(spi, &m);  /* CS held throughout all transfers */
```

**I²C: Transaction-Based Transfers**

```c
/* I²C: Multiple messages in one transaction */
struct i2c_msg msgs[] = {
    { .addr = 0x50, .flags = 0,        .len = 2, .buf = addr_buf },
    { .addr = 0x50, .flags = I2C_M_RD, .len = 4, .buf = data_buf },
};
i2c_transfer(adapter, msgs, 2);  /* REPEATED START between msgs */
```

**UART: Stream-Based I/O**

```c
/* UART: Byte stream, no message boundaries */
write(fd, data, len);     /* May block, may write partial */
read(fd, buf, sizeof(buf)); /* Returns available data */
/* Or use termios for line-based processing */
```

### B.5 Addressing and Discovery

```
+-----------------------------------------------------------------------+
|                      ADDRESSING MECHANISMS                             |
+-----------------------------------------------------------------------+

SPI: Hardware Chip Select
+--------+
| Master |---CS0---> [Device A]
|        |---CS1---> [Device B]  
|        |---CS2---> [Device C]
+--------+
  - Each device needs dedicated CS line
  - No protocol-level discovery
  - Board code must declare all devices

I²C: Software Addresses
+--------+
| Master |===SDA/SCL===+---[0x48 Temp Sensor]
+--------+             +---[0x50 EEPROM]
                       +---[0x68 RTC]
  - Devices share bus, addressed by 7/10-bit address
  - Limited discovery via probing (dangerous)
  - Address conflicts possible (fixed by hardware)

UART: Point-to-Point (No Addressing)
+--------+            +--------+
| Device |---TX/RX--->| Device |
+--------+            +--------+
  - Dedicated connection, no addressing needed
  - Higher-level protocols may add addressing (e.g., Modbus)
```

### B.6 Concurrency and Locking

| Aspect | SPI | I²C | UART |
|--------|-----|-----|------|
| **Bus Arbitration** | CS ensures exclusivity | Clock stretching, arbitration | N/A |
| **Kernel Lock** | `bus_lock_mutex` per master | `bus_lock` (rt_mutex) per adapter | Per-port spinlock |
| **Atomic Unit** | `spi_message` | `i2c_transfer()` call | None (byte stream) |
| **Interruptible** | Yes (between messages) | Yes (between transactions) | Yes (any time) |
| **Multi-master** | No (single master) | Yes (with arbitration) | N/A |

### B.7 Error Handling Comparison

```c
/* SPI: Error in message status */
ret = spi_sync(spi, &msg);
if (ret < 0) {
    /* Transfer failed (timeout, bus error) */
    /* No ACK/NACK - must use higher-level protocol */
}

/* I²C: NAK indicates device not responding */
ret = i2c_transfer(adapter, msgs, 2);
if (ret < 0) {
    /* -ENXIO: no ACK from device */
    /* -ETIMEDOUT: bus stuck */
    /* -EAGAIN: lost arbitration */
}

/* UART: Stream errors */
/* Framing error, parity error, overrun detected via UART status */
/* No automatic retry - application must handle */
```

### B.8 When to Use Which Bus

| Use Case | Best Choice | Reason |
|----------|-------------|--------|
| High-speed flash access | SPI | 50+ MHz speeds, full duplex |
| Multiple sensors | I²C | Single bus, software addressing |
| Debug console | UART | Simple, human-readable, long distance |
| Display (fast refresh) | SPI | High bandwidth needed |
| Battery monitor | I²C | Low pin count, low power |
| GPS module | UART | Standard interface, self-contained |
| Audio codec | I²C + I²S | Control via I²C, data via I²S |
| WiFi/BT module | UART/SDIO | Complex protocol, abstracted interface |

---

## Appendix C — Designing a Bus Subsystem in User-Space

### C.1 Architecture Overview

This section shows how to apply kernel SPI patterns to user-space bus design.

```
+--------------------------------------------------------------------+
|                    USER-SPACE BUS FRAMEWORK                         |
+--------------------------------------------------------------------+

Application Layer
+------------------+  +------------------+  +------------------+
| Protocol Driver A|  | Protocol Driver B|  | Protocol Driver C|
| (knows device    |  | (knows device    |  | (knows device    |
|  protocol)       |  |  protocol)       |  |  protocol)       |
+--------+---------+  +--------+---------+  +--------+---------+
         |                     |                     |
         | bus_transfer()      | bus_transfer()      |
         +----------+----------+----------+----------+
                    |
                    v
         +---------------------+
         |   Bus Core          |  <-- Abstraction layer
         | - Device registry   |      (like spi.c)
         | - Driver matching   |
         | - Message routing   |
         +----------+----------+
                    |
         +----------+----------+----------+
         |                     |          |
         v                     v          v
+--------+--------+  +--------+--------+  +--------+--------+
| Backend: GPIO   |  | Backend: FTDI   |  | Backend: Socket |
| (bitbang SPI)   |  | (USB-SPI)       |  | (remote SPI)    |
+-----------------+  +-----------------+  +-----------------+
         |                     |                    |
         v                     v                    v
    [GPIO pins]          [USB device]         [Network]
```

**中文解释：**
- **协议驱动层**：理解特定设备协议的应用逻辑
- **总线核心层**：管理设备注册、驱动匹配、消息路由
- **后端层**：实际硬件访问（GPIO比特流、USB适配器、网络等）

### C.2 Core Data Structures

```c
/* user_bus.h - Core definitions */

#include <pthread.h>
#include <stdint.h>

/* Forward declarations */
struct bus_controller;
struct bus_device;
struct bus_driver;
struct bus_message;

/*
 * Bus Controller (analogous to spi_master)
 * Represents the physical bus interface
 */
struct bus_controller {
    char name[32];
    int bus_num;
    int max_devices;
    
    /* [OPS] Controller callbacks - set by backend */
    int (*transfer)(struct bus_controller *ctrl,
                    struct bus_device *dev,
                    struct bus_message *msg);
    int (*setup)(struct bus_controller *ctrl,
                 struct bus_device *dev);
    void (*cleanup)(struct bus_controller *ctrl,
                    struct bus_device *dev);
    
    /* [LOCK] Serializes bus access */
    pthread_mutex_t bus_lock;
    
    /* [PRIVATE] Backend-specific data */
    void *private_data;
    
    /* [INTERNAL] Linked list of devices */
    struct bus_device *devices;
    struct bus_controller *next;
};

/*
 * Bus Device (analogous to spi_device)
 * Represents a device attached to the bus
 */
struct bus_device {
    char name[32];
    int address;              /* Device address or chip select */
    uint32_t max_speed_hz;
    uint32_t mode;
    
    struct bus_controller *controller;
    struct bus_driver *driver;
    
    /* [PRIVATE] Driver-specific data */
    void *driver_data;
    
    /* [INTERNAL] Device list link */
    struct bus_device *next;
};

/*
 * Bus Driver (analogous to spi_driver)
 * Protocol handler for specific device type
 */
struct bus_driver {
    char name[32];
    const char **compatible;  /* List of compatible device names */
    
    /* [OPS] Driver callbacks */
    int (*probe)(struct bus_device *dev);
    void (*remove)(struct bus_device *dev);
    
    struct bus_driver *next;
};

/*
 * Bus Transfer (analogous to spi_transfer)
 * Single transfer segment
 */
struct bus_transfer {
    const void *tx_buf;
    void *rx_buf;
    size_t len;
    
    uint32_t speed_hz;        /* Override device speed */
    uint16_t delay_usecs;     /* Delay after transfer */
    uint8_t cs_change;        /* Deselect after transfer? */
    
    struct bus_transfer *next;
};

/*
 * Bus Message (analogous to spi_message)
 * Complete transaction with multiple transfers
 */
struct bus_message {
    struct bus_transfer *transfers;
    
    /* [COMPLETION] Async support */
    void (*complete)(struct bus_message *msg, void *context);
    void *context;
    
    size_t actual_length;
    int status;               /* 0 = success, negative = error */
};

/* Core API */
struct bus_controller *bus_controller_alloc(const char *name, size_t priv_size);
int bus_controller_register(struct bus_controller *ctrl);
void bus_controller_unregister(struct bus_controller *ctrl);

struct bus_device *bus_device_alloc(struct bus_controller *ctrl);
int bus_device_register(struct bus_device *dev);
void bus_device_unregister(struct bus_device *dev);

int bus_driver_register(struct bus_driver *drv);
void bus_driver_unregister(struct bus_driver *drv);

int bus_transfer_sync(struct bus_device *dev, struct bus_message *msg);
int bus_transfer_async(struct bus_device *dev, struct bus_message *msg);
```

### C.3 Core Implementation

```c
/* user_bus_core.c - Core implementation */

#include "user_bus.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Global state */
static struct bus_controller *controllers = NULL;
static struct bus_driver *drivers = NULL;
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Controller allocation (like spi_alloc_master)
 */
struct bus_controller *bus_controller_alloc(const char *name, size_t priv_size)
{
    struct bus_controller *ctrl;
    
    /* [KEY] Allocate controller + private data in one block */
    ctrl = calloc(1, sizeof(*ctrl) + priv_size);
    if (!ctrl)
        return NULL;
    
    strncpy(ctrl->name, name, sizeof(ctrl->name) - 1);
    pthread_mutex_init(&ctrl->bus_lock, NULL);
    
    /* [KEY] Private data follows the struct */
    if (priv_size > 0)
        ctrl->private_data = ctrl + 1;
    
    return ctrl;
}

/*
 * Device-driver matching (like spi_match_device)
 */
static int bus_match_device(struct bus_device *dev, struct bus_driver *drv)
{
    const char **compat;
    
    if (!drv->compatible)
        return strcmp(dev->name, drv->name) == 0;
    
    for (compat = drv->compatible; *compat; compat++) {
        if (strcmp(dev->name, *compat) == 0)
            return 1;  /* Match! */
    }
    return 0;
}

/*
 * Try to bind a driver to a device
 */
static void bus_try_bind(struct bus_device *dev)
{
    struct bus_driver *drv;
    
    if (dev->driver)
        return;  /* Already bound */
    
    for (drv = drivers; drv; drv = drv->next) {
        if (bus_match_device(dev, drv)) {
            /* [KEY] Call driver's probe */
            if (drv->probe(dev) == 0) {
                dev->driver = drv;
                printf("bus: bound %s to driver %s\n", dev->name, drv->name);
                return;
            }
        }
    }
}

/*
 * Controller registration (like spi_register_master)
 */
int bus_controller_register(struct bus_controller *ctrl)
{
    static int next_bus_num = 0;
    
    pthread_mutex_lock(&global_lock);
    
    ctrl->bus_num = next_bus_num++;
    ctrl->next = controllers;
    controllers = ctrl;
    
    printf("bus: registered controller %s as bus%d\n", ctrl->name, ctrl->bus_num);
    
    pthread_mutex_unlock(&global_lock);
    return 0;
}

/*
 * Device registration (like spi_add_device)
 */
int bus_device_register(struct bus_device *dev)
{
    struct bus_controller *ctrl = dev->controller;
    
    if (!ctrl)
        return -EINVAL;
    
    pthread_mutex_lock(&global_lock);
    
    /* Add to controller's device list */
    dev->next = ctrl->devices;
    ctrl->devices = dev;
    
    /* [KEY] Call controller's setup if provided */
    if (ctrl->setup) {
        int ret = ctrl->setup(ctrl, dev);
        if (ret < 0) {
            pthread_mutex_unlock(&global_lock);
            return ret;
        }
    }
    
    /* [KEY] Try to find and bind a driver */
    bus_try_bind(dev);
    
    pthread_mutex_unlock(&global_lock);
    return 0;
}

/*
 * Driver registration (like spi_register_driver)
 */
int bus_driver_register(struct bus_driver *drv)
{
    struct bus_controller *ctrl;
    struct bus_device *dev;
    
    pthread_mutex_lock(&global_lock);
    
    drv->next = drivers;
    drivers = drv;
    
    /* [KEY] Try to bind to existing devices */
    for (ctrl = controllers; ctrl; ctrl = ctrl->next) {
        for (dev = ctrl->devices; dev; dev = dev->next) {
            if (!dev->driver)
                bus_try_bind(dev);
        }
    }
    
    pthread_mutex_unlock(&global_lock);
    return 0;
}

/*
 * Synchronous transfer (like spi_sync)
 */
int bus_transfer_sync(struct bus_device *dev, struct bus_message *msg)
{
    struct bus_controller *ctrl = dev->controller;
    int ret;
    
    if (!ctrl || !ctrl->transfer)
        return -ENODEV;
    
    /* [KEY] Lock the bus for exclusive access */
    pthread_mutex_lock(&ctrl->bus_lock);
    
    msg->status = -EINPROGRESS;
    msg->actual_length = 0;
    
    /* [KEY] Call controller's transfer callback */
    ret = ctrl->transfer(ctrl, dev, msg);
    
    pthread_mutex_unlock(&ctrl->bus_lock);
    
    return ret;
}
```

### C.4 Backend Implementation Example: GPIO Bitbang

```c
/* user_bus_gpio.c - GPIO bitbang backend */

#include "user_bus.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

struct gpio_backend {
    int chip_fd;
    int sclk_line;
    int mosi_line;
    int miso_line;
    int *cs_lines;
    int num_cs;
};

static void gpio_set(struct gpio_backend *gb, int line, int value)
{
    struct gpiohandle_data data = { .values[0] = value };
    /* Simplified - real code would use proper GPIO API */
}

static int gpio_get(struct gpio_backend *gb, int line)
{
    struct gpiohandle_data data;
    /* Read GPIO value */
    return data.values[0];
}

/*
 * Bitbang one byte (SPI mode 0)
 */
static uint8_t gpio_txrx_byte(struct gpio_backend *gb, uint8_t tx)
{
    uint8_t rx = 0;
    int i;
    
    for (i = 7; i >= 0; i--) {
        /* [KEY] Set MOSI on falling edge */
        gpio_set(gb, gb->mosi_line, (tx >> i) & 1);
        usleep(1);  /* Half clock period */
        
        /* [KEY] Rising edge - sample MISO */
        gpio_set(gb, gb->sclk_line, 1);
        rx |= (gpio_get(gb, gb->miso_line) << i);
        usleep(1);
        
        /* Falling edge */
        gpio_set(gb, gb->sclk_line, 0);
    }
    
    return rx;
}

/*
 * Transfer callback for GPIO backend
 */
static int gpio_transfer(struct bus_controller *ctrl,
                         struct bus_device *dev,
                         struct bus_message *msg)
{
    struct gpio_backend *gb = ctrl->private_data;
    struct bus_transfer *t;
    size_t i;
    
    /* [KEY] Assert chip select */
    gpio_set(gb, gb->cs_lines[dev->address], 0);
    
    for (t = msg->transfers; t; t = t->next) {
        const uint8_t *tx = t->tx_buf;
        uint8_t *rx = t->rx_buf;
        
        for (i = 0; i < t->len; i++) {
            uint8_t tx_byte = tx ? tx[i] : 0;
            uint8_t rx_byte = gpio_txrx_byte(gb, tx_byte);
            if (rx)
                rx[i] = rx_byte;
        }
        
        msg->actual_length += t->len;
        
        if (t->delay_usecs)
            usleep(t->delay_usecs);
        
        if (t->cs_change && t->next) {
            gpio_set(gb, gb->cs_lines[dev->address], 1);
            usleep(1);
            gpio_set(gb, gb->cs_lines[dev->address], 0);
        }
    }
    
    /* [KEY] Deassert chip select */
    gpio_set(gb, gb->cs_lines[dev->address], 1);
    
    msg->status = 0;
    return 0;
}

/*
 * Create GPIO backend controller
 */
struct bus_controller *gpio_controller_create(const char *gpio_chip,
                                               int sclk, int mosi, int miso,
                                               int *cs_lines, int num_cs)
{
    struct bus_controller *ctrl;
    struct gpio_backend *gb;
    
    ctrl = bus_controller_alloc("gpio-spi", sizeof(*gb));
    if (!ctrl)
        return NULL;
    
    gb = ctrl->private_data;
    gb->sclk_line = sclk;
    gb->mosi_line = mosi;
    gb->miso_line = miso;
    gb->cs_lines = cs_lines;
    gb->num_cs = num_cs;
    
    ctrl->max_devices = num_cs;
    ctrl->transfer = gpio_transfer;
    
    /* Initialize GPIOs... */
    
    return ctrl;
}
```

### C.5 Protocol Driver Example

```c
/* example_sensor_driver.c - Example device driver */

#include "user_bus.h"
#include <stdio.h>
#include <stdlib.h>

#define SENSOR_REG_ID       0x00
#define SENSOR_REG_DATA     0x01
#define SENSOR_EXPECTED_ID  0x42

struct sensor_data {
    struct bus_device *dev;
    uint8_t device_id;
};

/*
 * Read a register from the sensor
 */
static int sensor_read_reg(struct bus_device *dev, uint8_t reg, uint8_t *value)
{
    uint8_t tx_buf[2] = { reg | 0x80, 0x00 };  /* Read flag */
    uint8_t rx_buf[2];
    
    struct bus_transfer t = {
        .tx_buf = tx_buf,
        .rx_buf = rx_buf,
        .len = 2,
    };
    struct bus_message msg = {
        .transfers = &t,
    };
    
    int ret = bus_transfer_sync(dev, &msg);
    if (ret == 0)
        *value = rx_buf[1];
    
    return ret;
}

/*
 * Probe callback - called when device matches
 */
static int sensor_probe(struct bus_device *dev)
{
    struct sensor_data *data;
    uint8_t id;
    int ret;
    
    /* [KEY] Read and verify device ID */
    ret = sensor_read_reg(dev, SENSOR_REG_ID, &id);
    if (ret < 0) {
        fprintf(stderr, "sensor: failed to read ID\n");
        return ret;
    }
    
    if (id != SENSOR_EXPECTED_ID) {
        fprintf(stderr, "sensor: unexpected ID 0x%02x\n", id);
        return -ENODEV;
    }
    
    /* [KEY] Allocate driver-private data */
    data = calloc(1, sizeof(*data));
    if (!data)
        return -ENOMEM;
    
    data->dev = dev;
    data->device_id = id;
    
    /* [KEY] Attach private data to device */
    dev->driver_data = data;
    
    printf("sensor: probed device with ID 0x%02x\n", id);
    return 0;
}

static void sensor_remove(struct bus_device *dev)
{
    struct sensor_data *data = dev->driver_data;
    free(data);
    dev->driver_data = NULL;
}

/* Driver definition */
static const char *sensor_compatible[] = {
    "acme,temp-sensor",
    "generic,spi-sensor",
    NULL,
};

static struct bus_driver sensor_driver = {
    .name = "temp-sensor",
    .compatible = sensor_compatible,
    .probe = sensor_probe,
    .remove = sensor_remove,
};

/* Registration function */
void sensor_driver_init(void)
{
    bus_driver_register(&sensor_driver);
}
```

### C.6 Complete Usage Example

```c
/* main.c - Example application */

#include "user_bus.h"
#include <stdio.h>

int main(void)
{
    struct bus_controller *ctrl;
    struct bus_device *dev;
    int cs_lines[] = { 17, 18 };  /* GPIO17, GPIO18 as CS */
    
    /* [STEP 1] Create and register controller */
    ctrl = gpio_controller_create("/dev/gpiochip0",
                                  11,  /* SCLK = GPIO11 */
                                  10,  /* MOSI = GPIO10 */
                                  9,   /* MISO = GPIO9 */
                                  cs_lines, 2);
    if (!ctrl) {
        fprintf(stderr, "Failed to create controller\n");
        return 1;
    }
    bus_controller_register(ctrl);
    
    /* [STEP 2] Register driver */
    sensor_driver_init();
    
    /* [STEP 3] Create and register device */
    dev = bus_device_alloc(ctrl);
    if (!dev) {
        fprintf(stderr, "Failed to allocate device\n");
        return 1;
    }
    
    strncpy(dev->name, "acme,temp-sensor", sizeof(dev->name) - 1);
    dev->address = 0;  /* CS0 */
    dev->max_speed_hz = 1000000;
    dev->mode = 0;
    
    bus_device_register(dev);  /* Triggers probe if driver matches */
    
    /* [STEP 4] Now driver can be used... */
    /* In real code, driver would expose its own API */
    
    return 0;
}
```

### C.7 Design Lessons from Kernel to User-Space

| Kernel Pattern | User-Space Adaptation |
|----------------|----------------------|
| `kzalloc()` | `calloc()` |
| `spinlock_t` | `pthread_mutex_t` (fine for user-space) |
| `container_of()` | Explicit `private_data` pointer |
| `EXPORT_SYMBOL()` | Public header + shared library |
| `module_init()` | Explicit init function call |
| `dev_err()` | `fprintf(stderr, ...)` |
| Interrupt context | Separate thread (if needed) |
| Work queues | Thread pool or event loop |

---

## Appendix D — SPI and Async I/O Patterns

### D.1 Async I/O Architecture in Kernel SPI

```
+--------------------------------------------------------------------+
|                    SPI ASYNC I/O FLOW                               |
+--------------------------------------------------------------------+

   Caller                  SPI Core                 Controller
     |                        |                         |
     | spi_async(msg)         |                         |
     |----------------------->|                         |
     |                        | msg->status = -EINPROGRESS
     |                        | master->transfer(msg)   |
     |                        |------------------------>|
     |                        |                         |
     | [returns immediately]  |                    [queue msg]
     |                        |                    [start DMA]
     |                        |                    [return 0]
     |                        |<------------------------|
     |                        |                         |
     |  ...caller continues.. |                         |
     |                        |                    [IRQ: done]
     |                        |                    msg->status = 0
     |                        |                    msg->complete()
     |                        |<------------------------|
     |                        |                         |
     | [complete() callback]  |                         |
     |<-----------------------|                         |
     |                        |                         |
     | [process result]       |                         |
```

**中文解释：**
- `spi_async()`立即返回，不等待传输完成
- 控制器驱动将消息加入队列并启动DMA
- 传输完成后通过中断触发回调
- 回调函数处理结果（可能在中断上下文）

### D.2 Async Patterns Comparison

```
+--------------------------------------------------------------------+
|              ASYNC I/O PATTERNS IN DIFFERENT SYSTEMS                |
+--------------------------------------------------------------------+

KERNEL SPI (Callback-based)
+-----------+     +-----------+     +-----------+
| Initiate  |---->| Complete  |---->| Callback  |
| Transfer  |     | (async)   |     | Invoked   |
+-----------+     +-----------+     +-----------+
  spi_async()      [hardware]       msg->complete()

POSIX AIO (Signal/Callback)
+-----------+     +-----------+     +-----------+
| aio_read  |---->| Kernel    |---->| Signal or |
| aio_write |     | Completes |     | Callback  |
+-----------+     +-----------+     +-----------+

io_uring (Completion Queue)
+-----------+     +-----------+     +-----------+
| Submit to |---->| Kernel    |---->| Poll CQ   |
| SQ ring   |     | Completes |     | for result|
+-----------+     +-----------+     +-----------+

libuv/epoll (Event Loop)
+-----------+     +-----------+     +-----------+
| Register  |---->| epoll_wait|---->| Dispatch  |
| Interest  |     | (blocks)  |     | Callback  |
+-----------+     +-----------+     +-----------+
```

### D.3 Kernel SPI Async Implementation Details

```c
/* Key structures for async support */

struct spi_message {
    /* [COMPLETION] The async mechanism */
    void (*complete)(void *context);  /* Callback function */
    void *context;                    /* Callback argument */
    
    int status;                       /* Result when complete */
    size_t actual_length;             /* Bytes transferred */
    
    /* [QUEUE] For controller's internal queue */
    struct list_head queue;
    void *state;
};

/* How spi_sync() builds on spi_async() */
static int __spi_sync(struct spi_device *spi, struct spi_message *msg, ...)
{
    DECLARE_COMPLETION_ONSTACK(done);  /* [KEY] Stack completion */
    
    msg->complete = spi_complete;      /* [KEY] Generic wakeup */
    msg->context = &done;
    
    status = spi_async_locked(spi, msg);  /* Submit async */
    
    if (status == 0)
        wait_for_completion(&done);    /* [KEY] Block until done */
    
    return msg->status;
}

/* The simple completion function */
static void spi_complete(void *arg)
{
    complete(arg);  /* Wake up waiter */
}
```

### D.4 Async Pattern: Overlapping Transfers

```c
/*
 * Example: Pipelining multiple async transfers
 * Useful for high-throughput scenarios
 */

#define NUM_BUFFERS 4

struct async_context {
    struct spi_message msg;
    struct spi_transfer xfer;
    uint8_t tx_buf[256];
    uint8_t rx_buf[256];
    int sequence;
    struct completion done;
};

static struct async_context contexts[NUM_BUFFERS];
static atomic_t pending_count = ATOMIC_INIT(0);

/* Completion callback - called for each finished transfer */
static void pipeline_complete(void *arg)
{
    struct async_context *ctx = arg;
    
    /* Process received data */
    process_data(ctx->rx_buf, ctx->xfer.len, ctx->sequence);
    
    if (atomic_dec_and_test(&pending_count))
        complete(&contexts[0].done);  /* All done */
}

/* Submit multiple overlapping transfers */
static int pipeline_transfers(struct spi_device *spi, int count)
{
    int i, ret;
    
    init_completion(&contexts[0].done);
    atomic_set(&pending_count, count);
    
    /* [KEY] Submit all transfers without waiting */
    for (i = 0; i < count; i++) {
        struct async_context *ctx = &contexts[i % NUM_BUFFERS];
        
        /* Wait if this buffer is still in use */
        if (i >= NUM_BUFFERS) {
            /* In real code: wait for buffer to be free */
        }
        
        /* Prepare transfer */
        prepare_tx_data(ctx->tx_buf, i);
        ctx->sequence = i;
        
        ctx->xfer.tx_buf = ctx->tx_buf;
        ctx->xfer.rx_buf = ctx->rx_buf;
        ctx->xfer.len = 256;
        
        spi_message_init(&ctx->msg);
        spi_message_add_tail(&ctx->xfer, &ctx->msg);
        ctx->msg.complete = pipeline_complete;
        ctx->msg.context = ctx;
        
        /* [KEY] Submit async - returns immediately */
        ret = spi_async(spi, &ctx->msg);
        if (ret < 0) {
            /* Handle error */
            break;
        }
    }
    
    /* Wait for all to complete */
    wait_for_completion(&contexts[0].done);
    return 0;
}
```

### D.5 User-Space Async SPI with io_uring

```c
/*
 * Modern async SPI access using io_uring
 * (Requires kernel 5.6+ with spidev io_uring support)
 */

#include <liburing.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>

struct spi_async_op {
    struct spi_ioc_transfer xfer;
    uint8_t tx_buf[256];
    uint8_t rx_buf[256];
    int id;
};

static int spi_async_io_uring(int spi_fd, int num_ops)
{
    struct io_uring ring;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    struct spi_async_op *ops;
    int i, ret;
    
    /* Initialize io_uring */
    ret = io_uring_queue_init(32, &ring, 0);
    if (ret < 0)
        return ret;
    
    ops = calloc(num_ops, sizeof(*ops));
    
    /* [KEY] Submit all operations to submission queue */
    for (i = 0; i < num_ops; i++) {
        ops[i].id = i;
        ops[i].xfer.tx_buf = (unsigned long)ops[i].tx_buf;
        ops[i].xfer.rx_buf = (unsigned long)ops[i].rx_buf;
        ops[i].xfer.len = 256;
        ops[i].xfer.speed_hz = 1000000;
        
        /* Prepare data */
        prepare_data(ops[i].tx_buf, i);
        
        sqe = io_uring_get_sqe(&ring);
        /* Note: Real spidev doesn't support io_uring directly yet,
         * this is conceptual. Would need kernel support. */
        io_uring_prep_rw(IORING_OP_URING_CMD, sqe, spi_fd, 
                         &ops[i].xfer, 1, 0);
        io_uring_sqe_set_data(sqe, &ops[i]);
    }
    
    /* [KEY] Submit all at once */
    io_uring_submit(&ring);
    
    /* [KEY] Collect completions */
    for (i = 0; i < num_ops; i++) {
        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0)
            break;
        
        struct spi_async_op *op = io_uring_cqe_get_data(cqe);
        printf("Op %d completed with result %d\n", op->id, cqe->res);
        
        /* Process result */
        process_result(op->rx_buf, op->xfer.len);
        
        io_uring_cqe_seen(&ring, cqe);
    }
    
    free(ops);
    io_uring_queue_exit(&ring);
    return 0;
}
```

### D.6 Event Loop Integration Pattern

```c
/*
 * Integrating async SPI with an event loop (e.g., libuv, libevent)
 * Common pattern for user-space daemons
 */

#include <pthread.h>
#include <poll.h>

/* Event-driven SPI context */
struct spi_event_ctx {
    int spi_fd;
    
    /* Message queue */
    pthread_mutex_t queue_lock;
    struct spi_request *queue_head;
    struct spi_request *queue_tail;
    
    /* Worker thread */
    pthread_t worker;
    int running;
    
    /* Event notification */
    int event_fd;  /* For signaling main loop */
};

struct spi_request {
    struct spi_ioc_transfer xfer;
    void (*callback)(struct spi_request *req, int status);
    void *user_data;
    struct spi_request *next;
};

/* Worker thread - processes SPI transfers */
static void *spi_worker(void *arg)
{
    struct spi_event_ctx *ctx = arg;
    struct spi_request *req;
    
    while (ctx->running) {
        pthread_mutex_lock(&ctx->queue_lock);
        
        if (ctx->queue_head) {
            /* [KEY] Dequeue request */
            req = ctx->queue_head;
            ctx->queue_head = req->next;
            if (!ctx->queue_head)
                ctx->queue_tail = NULL;
        } else {
            req = NULL;
        }
        
        pthread_mutex_unlock(&ctx->queue_lock);
        
        if (req) {
            /* [KEY] Execute transfer (blocking in worker thread) */
            int ret = ioctl(ctx->spi_fd, SPI_IOC_MESSAGE(1), &req->xfer);
            
            /* [KEY] Invoke callback */
            req->callback(req, ret < 0 ? ret : 0);
            
            /* Signal event loop if needed */
            uint64_t val = 1;
            write(ctx->event_fd, &val, sizeof(val));
        } else {
            /* Wait for work */
            usleep(1000);
        }
    }
    
    return NULL;
}

/* Submit async request from main thread */
int spi_submit_async(struct spi_event_ctx *ctx, struct spi_request *req)
{
    pthread_mutex_lock(&ctx->queue_lock);
    
    req->next = NULL;
    if (ctx->queue_tail) {
        ctx->queue_tail->next = req;
    } else {
        ctx->queue_head = req;
    }
    ctx->queue_tail = req;
    
    pthread_mutex_unlock(&ctx->queue_lock);
    return 0;
}

/* Main event loop integration */
void event_loop_example(struct spi_event_ctx *ctx)
{
    struct pollfd fds[2];
    
    fds[0].fd = ctx->event_fd;
    fds[0].events = POLLIN;
    fds[1].fd = /* other fd */;
    
    while (1) {
        int ret = poll(fds, 2, -1);
        
        if (fds[0].revents & POLLIN) {
            /* SPI completion signaled */
            uint64_t val;
            read(ctx->event_fd, &val, sizeof(val));
            /* Handle completed SPI operations */
        }
        
        /* Handle other events... */
    }
}
```

### D.7 Async Pattern Summary

```
+--------------------------------------------------------------------+
|                    ASYNC PATTERN DECISION MATRIX                    |
+--------------------------------------------------------------------+

Use Case                    | Pattern               | Complexity
----------------------------|-----------------------|------------
Simple kernel driver        | spi_sync()            | Low
High-throughput kernel      | spi_async() + pipeline| Medium
User-space, simple          | blocking ioctl()      | Low
User-space, responsive      | Thread + event loop   | Medium
User-space, high-perf       | io_uring (future)     | High
Real-time constraints       | spi_async() + RT prio | High

+--------------------------------------------------------------------+
|                    KEY ASYNC DESIGN RULES                           |
+--------------------------------------------------------------------+

1. BUFFER LIFETIME
   - Stack buffers: ONLY with spi_sync()
   - Heap buffers: REQUIRED for spi_async()
   - Rule: Buffer must live until complete() is called

2. COMPLETION CONTEXT
   - Kernel: complete() may run in IRQ context
   - User-space: callback runs in worker thread
   - Rule: Don't block in completion handlers

3. ERROR HANDLING
   - Check msg->status in completion, not submit return
   - Submit may succeed but transfer may fail later
   - Always handle partial transfers (actual_length)

4. ORDERING GUARANTEES
   - Messages to SAME device: FIFO order guaranteed
   - Messages to DIFFERENT devices: No ordering
   - Use spi_bus_lock() for multi-device sequences

5. CANCELLATION
   - Kernel SPI: No standard cancellation API
   - Must wait for in-flight transfers to complete
   - Design with graceful shutdown in mind
```

**中文解释：**
- **缓冲区生命周期**：异步传输时，缓冲区必须保持有效直到回调被调用
- **完成上下文**：内核中回调可能在中断上下文运行，不要阻塞
- **错误处理**：在回调中检查`msg->status`，而不是提交返回值
- **顺序保证**：同一设备的消息保证FIFO顺序
- **取消操作**：内核SPI没有标准取消API，需优雅关闭设计

### D.8 Connecting It All: Full Async Example

```c
/*
 * Complete async SPI example with proper patterns
 */

struct flash_device {
    struct spi_device *spi;
    struct mutex lock;
    
    /* Async state */
    struct spi_message msg;
    struct spi_transfer xfer[2];
    uint8_t cmd_buf[4];
    uint8_t *data_buf;
    struct completion done;
    int result;
};

/* Completion callback */
static void flash_complete(void *context)
{
    struct flash_device *flash = context;
    
    /* [KEY] Called in interrupt context! Keep it short. */
    flash->result = flash->msg.status;
    complete(&flash->done);
}

/* Async read operation */
int flash_read_async(struct flash_device *flash, 
                     uint32_t addr, void *buf, size_t len,
                     void (*done_cb)(int status, void *ctx), void *ctx)
{
    int ret;
    
    mutex_lock(&flash->lock);
    
    /* Prepare command */
    flash->cmd_buf[0] = 0x03;  /* READ command */
    flash->cmd_buf[1] = (addr >> 16) & 0xFF;
    flash->cmd_buf[2] = (addr >> 8) & 0xFF;
    flash->cmd_buf[3] = addr & 0xFF;
    
    /* [KEY] Setup transfers - buffers must remain valid! */
    flash->xfer[0].tx_buf = flash->cmd_buf;
    flash->xfer[0].len = 4;
    flash->xfer[1].rx_buf = buf;  /* Caller's buffer */
    flash->xfer[1].len = len;
    
    spi_message_init(&flash->msg);
    spi_message_add_tail(&flash->xfer[0], &flash->msg);
    spi_message_add_tail(&flash->xfer[1], &flash->msg);
    
    flash->msg.complete = flash_complete;
    flash->msg.context = flash;
    
    init_completion(&flash->done);
    
    /* [KEY] Submit async */
    ret = spi_async(flash->spi, &flash->msg);
    if (ret < 0) {
        mutex_unlock(&flash->lock);
        return ret;
    }
    
    /* Return immediately - caller should wait or check later */
    return 0;
}

/* Wait for async operation to complete */
int flash_wait(struct flash_device *flash)
{
    wait_for_completion(&flash->done);
    mutex_unlock(&flash->lock);
    return flash->result;
}

/* Combined async+wait for simple usage */
int flash_read(struct flash_device *flash, uint32_t addr, void *buf, size_t len)
{
    int ret = flash_read_async(flash, addr, buf, len, NULL, NULL);
    if (ret < 0)
        return ret;
    return flash_wait(flash);
}
```

This completes the SPI subsystem architecture document with comprehensive comparisons to I²C and UART, a complete user-space bus framework design, and detailed async I/O patterns.
