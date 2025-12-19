# Linux I²C Subsystem Architecture

A deep systems apprenticeship in I²C driver design from Linux kernel v3.2.

---

## Table of Contents

- [Phase 1: I²C Subsystem Overview](#phase-1--ic-subsystem-overview)
- [Phase 2: Core Objects & Relationships](#phase-2--core-objects--relationships)
- [Phase 3: struct i2c_adapter (Bus Controller)](#phase-3--struct-i2c_adapter-bus-controller)
- [Phase 4: struct i2c_client (Devices on the Bus)](#phase-4--struct-i2c_client-devices-on-the-bus)
- [Phase 5: struct i2c_driver (Device Drivers)](#phase-5--struct-i2c_driver-device-drivers)
- [Phase 6: Ops, Callbacks, and Contracts](#phase-6--ops-callbacks-and-contracts)
- [Phase 7: Driver Registration & Matching](#phase-7--driver-registration--matching)
- [Phase 8: End-to-End Data Transfer Path](#phase-8--end-to-end-data-transfer-path)
- [Phase 9: Concurrency, Context, and Power](#phase-9--concurrency-context-and-power)
- [Phase 10: Common Bugs, Pitfalls, and Design Lessons](#phase-10--common-bugs-pitfalls-and-design-lessons)
- [Appendix A: Walking Through Real I²C Drivers](#appendix-a--walking-through-real-ic-drivers)
- [Appendix B: I²C vs SPI Architecture Comparison](#appendix-b--ic-vs-spi-architecture-comparison)
- [Appendix C: Designing a Clean Bus Subsystem in User-Space](#appendix-c--designing-a-clean-bus-subsystem-in-user-space)
- [Appendix D: Device Tree Integration](#appendix-d--device-tree-integration)

---

## Phase 1 — I²C Subsystem Overview

### 1.1 Problems the I²C Subsystem Solves

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                  WITHOUT I²C SUBSYSTEM (CHAOS)                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   EEPROM Driver ──────────► Direct I²C bit-bang on SoC pins                │
│   Sensor Driver ──────────► Different SoC, different registers (rewrite!)  │
│   RTC Driver ─────────────► USB-I²C bridge (completely different API!)     │
│   Audio Codec ────────────► PCI I²C controller (yet another API!)          │
│                                                                             │
│   Problems:                                                                 │
│   1. Each device driver contains bus access code → code duplication        │
│   2. Same chip needs different driver for each platform                    │
│   3. No unified device model → no sysfs, no udev, no power management      │
│   4. No way to share bus between drivers → conflicts                       │
│   5. No address conflict detection → system instability                    │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                    WITH I²C SUBSYSTEM (ORDER)                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│                      ┌───────────────────────────────┐                      │
│                      │        I²C CORE               │                      │
│                      │  (Unified API + Driver Model) │                      │
│                      └───────────────┬───────────────┘                      │
│                                      │                                      │
│          ┌───────────────────────────┼───────────────────────────┐          │
│          │                           │                           │          │
│          ▼                           ▼                           ▼          │
│   ┌─────────────┐             ┌─────────────┐             ┌─────────────┐   │
│   │ SoC I²C     │             │ USB-I²C     │             │ PCI I²C     │   │
│   │ Adapter     │             │ Adapter     │             │ Adapter     │   │
│   │ (i2c-imx)   │             │ (i2c-tiny)  │             │ (i2c-i801)  │   │
│   └──────┬──────┘             └──────┬──────┘             └──────┬──────┘   │
│          │                           │                           │          │
│   ┌──────┴──────┐             ┌──────┴──────┐             ┌──────┴──────┐   │
│   │ EEPROM      │             │ Temp Sensor │             │ RTC         │   │
│   │ (i2c_client)│             │ (i2c_client)│             │ (i2c_client)│   │
│   └─────────────┘             └─────────────┘             └─────────────┘   │
│                                                                             │
│   Benefits:                                                                 │
│   1. Device drivers are portable across all I²C adapters                   │
│   2. Adapter drivers are reusable for all I²C devices                      │
│   3. Full driver model integration (sysfs, power management)               │
│   4. Bus locking prevents concurrent access conflicts                      │
│   5. Address management prevents collisions                                │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- I²C 子系统提供统一的 API，隔离设备驱动和总线控制器
- 设备驱动不需要知道底层硬件（SoC、USB、PCI）
- 适配器驱动不需要知道连接的设备类型
- 核心层处理总线锁定、地址管理、电源管理

### 1.2 Why Linux Separates Adapter, Client, and Driver

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    THE THREE-WAY SEPARATION                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   1. i2c_adapter (Bus Controller)                                           │
│   ─────────────────────────────────                                         │
│   • Represents a PHYSICAL I²C bus (the wires)                               │
│   • Knows HOW to send/receive bytes on the bus                              │
│   • Knows NOTHING about what devices are connected                          │
│   • Examples: i2c-imx, i2c-omap, i2c-i801                                  │
│                                                                             │
│   2. i2c_client (Device Representation)                                     │
│   ────────────────────────────────────                                      │
│   • Represents a SPECIFIC CHIP at a specific address                        │
│   • Links a device to an adapter                                            │
│   • Created by: board files, device tree, or detection                      │
│   • Knows its address, but not how to use the chip                          │
│                                                                             │
│   3. i2c_driver (Device Driver)                                             │
│   ────────────────────────────────                                          │
│   • Knows how to USE a specific type of chip                                │
│   • Doesn't know WHICH bus the chip is on                                   │
│   • Binds to i2c_clients by name/id matching                                │
│   • Examples: eeprom, tmp102, rtc-ds1307                                    │
│                                                                             │
│   ANALOGY:                                                                  │
│   ─────────                                                                 │
│   adapter = highway system                                                  │
│   client  = address of a building                                           │
│   driver  = knowledge of what business is in the building                   │
│                                                                             │
│   WHY THIS SEPARATION?                                                      │
│   ─────────────────────                                                     │
│   • Same chip (TMP102) works on ANY I²C bus                                 │
│   • Same bus (i.MX I²C) works with ANY chip                                 │
│   • Adding new hardware doesn't require changing existing code              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 三层分离实现了完全解耦
- 适配器驱动只关心如何操作 I²C 硬件
- 设备驱动只关心如何使用特定芯片
- `i2c_client` 是连接两者的桥梁

### 1.3 I²C Code Locations in v3.2

| Component | Location | Purpose |
|-----------|----------|---------|
| **Core** | `drivers/i2c/i2c-core.c` | Bus registration, matching, transfer API |
| | `drivers/i2c/i2c-boardinfo.c` | Static device declarations |
| | `drivers/i2c/i2c-smbus.c` | SMBus protocol helpers |
| **Headers** | `include/linux/i2c.h` | Main API, struct definitions |
| | `include/linux/i2c-algo-bit.h` | Bit-bang algorithm interface |
| **Adapter Drivers** | `drivers/i2c/busses/` | Controller implementations |
| | `drivers/i2c/algos/` | Algorithm implementations |
| **Client Drivers** | `drivers/hwmon/`, `drivers/rtc/`, etc. | Device drivers |
| **Board Files** | `arch/*/mach-*/board-*.c` | Device declarations |

---

## Phase 2 — Core Objects & Relationships

### 2.1 Object Relationship Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    I²C SUBSYSTEM OBJECT GRAPH                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                         i2c_bus_type                                 │   │
│   │                    (Global Bus Subsystem)                            │   │
│   │                                                                      │   │
│   │   Contains: match(), probe(), remove(), shutdown(), pm_ops          │   │
│   └───────────────────────────────┬─────────────────────────────────────┘   │
│                                   │                                         │
│           ┌───────────────────────┼───────────────────────┐                 │
│           │                       │                       │                 │
│           ▼                       ▼                       ▼                 │
│   ┌───────────────┐       ┌───────────────┐       ┌───────────────┐         │
│   │ i2c_adapter   │       │ i2c_adapter   │       │ i2c_driver    │         │
│   │ (Bus 0)       │       │ (Bus 1)       │       │ (tmp102)      │         │
│   ├───────────────┤       ├───────────────┤       ├───────────────┤         │
│   │ name          │       │ name          │       │ driver        │◄────────┤
│   │ algo ─────────┼───┐   │ algo          │       │ id_table      │         │
│   │ dev ──────────┼─┐ │   │ dev           │       │ probe()       │         │
│   │ nr = 0        │ │ │   │ nr = 1        │       │ remove()      │         │
│   └───────────────┘ │ │   └───────┬───────┘       └───────────────┘         │
│          │          │ │           │                       │                 │
│          │          │ │   ┌───────┴───────┐               │                 │
│   ┌──────┴──────┐   │ │   │ i2c_client    │               │ matches         │
│   │ i2c_client  │   │ │   │ (0x48: RTC)   │◄──────────────┘                 │
│   │ (0x50:EEPROM│   │ │   ├───────────────┤                                 │
│   ├─────────────┤   │ │   │ adapter ──────┼───────► i2c_adapter (Bus 1)     │
│   │ adapter ────┼───┘ │   │ driver ───────┼───────► i2c_driver (rtc-ds1307) │
│   │ driver      │     │   │ addr = 0x48   │                                 │
│   │ addr = 0x50 │     │   │ dev           │                                 │
│   │ dev ────────┼─────┘   └───────────────┘                                 │
│   └─────────────┘                                                           │
│          │                                                                  │
│          ▼                                                                  │
│   ┌─────────────────────────────────────────┐                               │
│   │            struct device                 │                               │
│   │  (Generic driver model device)          │                               │
│   ├─────────────────────────────────────────┤                               │
│   │ parent = &adapter->dev                  │                               │
│   │ bus = &i2c_bus_type                     │                               │
│   │ driver_data = client private data       │                               │
│   └─────────────────────────────────────────┘                               │
│                                                                             │
│   ┌─────────────────────────────────────────┐                               │
│   │          i2c_algorithm                   │                               │
│   ├─────────────────────────────────────────┤                               │
│   │ master_xfer()  ─── Raw I²C transfer     │                               │
│   │ smbus_xfer()   ─── Native SMBus         │                               │
│   │ functionality()─── Report capabilities  │                               │
│   └─────────────────────────────────────────┘                               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- `i2c_bus_type` 是 Linux 驱动模型中的总线类型
- 每个 `i2c_adapter` 代表一条物理 I²C 总线
- 每个 `i2c_client` 代表总线上的一个设备
- `i2c_driver` 与匹配的 `i2c_client` 绑定
- 所有对象都通过嵌入的 `struct device` 集成到驱动模型

### 2.2 Key Relationships Summary

| Relationship | How Expressed |
|--------------|---------------|
| adapter → algorithm | `adapter->algo` pointer |
| client → adapter | `client->adapter` pointer |
| client → driver | `client->driver` pointer (after probe) |
| driver → bus | `driver->driver.bus = &i2c_bus_type` |
| all → driver model | Embedded `struct device` |

---

## Phase 3 — struct i2c_adapter (Bus Controller)

### 3.1 What an Adapter Represents

An `i2c_adapter` represents a **physical I²C bus controller**:
- The hardware that generates clock/data signals
- May be integrated in SoC or external (USB, PCI)
- Manages bus arbitration and timing
- Does NOT know what devices are connected

### 3.2 Key Fields

```c
/* include/linux/i2c.h */

struct i2c_adapter {
    struct module *owner;               /* [1] Module owning this adapter */
    unsigned int class;                 /* [2] Device class for probing */
    const struct i2c_algorithm *algo;   /* [3] How to do transfers */
    void *algo_data;                    /* [4] Algorithm private data */

    struct rt_mutex bus_lock;           /* [5] Serialize bus access */

    int timeout;                        /* [6] Transfer timeout (jiffies) */
    int retries;                        /* [7] Retry count on failure */
    struct device dev;                  /* [8] Driver model device */

    int nr;                             /* [9] Bus number */
    char name[48];                      /* [10] Human-readable name */
    struct completion dev_released;     /* [11] For safe removal */

    struct mutex userspace_clients_lock;
    struct list_head userspace_clients; /* [12] sysfs-created clients */
};
```

| Field | Purpose |
|-------|---------|
| `owner` | Module reference counting (prevent unload) |
| `class` | Bitmask: I2C_CLASS_HWMON, I2C_CLASS_DDC, etc. |
| `algo` | **THE KEY**: pointer to transfer functions |
| `algo_data` | Private data for algorithm implementation |
| `bus_lock` | RT mutex for exclusive bus access |
| `timeout` | Default 1 second (HZ jiffies) |
| `retries` | Retry on EAGAIN (arbitration loss) |
| `nr` | Bus number (0, 1, 2, ...) |
| `dev` | Driver model integration |

### 3.3 Private Data Storage

```c
/* Pattern: Store private data in device */
static inline void i2c_set_adapdata(struct i2c_adapter *dev, void *data)
{
    dev_set_drvdata(&dev->dev, data);  /* Uses dev->dev.driver_data */
}

static inline void *i2c_get_adapdata(const struct i2c_adapter *dev)
{
    return dev_get_drvdata(&dev->dev);
}

/* Example usage in adapter driver */
struct my_i2c_priv {
    void __iomem *base;
    int irq;
    struct clk *clk;
    struct i2c_adapter adap;  /* [KEY] Embedded adapter */
};

static int my_i2c_probe(struct platform_device *pdev)
{
    struct my_i2c_priv *priv;
    
    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    
    /* Store priv for retrieval in callbacks */
    i2c_set_adapdata(&priv->adap, priv);
    /* Or use platform_set_drvdata(pdev, priv); */
    
    return i2c_add_adapter(&priv->adap);
}
```

### 3.4 Adapter Registration

```c
/* drivers/i2c/i2c-core.c */

/* Dynamic bus number (most common) */
int i2c_add_adapter(struct i2c_adapter *adapter)
{
    int id, res = 0;

retry:
    if (idr_pre_get(&i2c_adapter_idr, GFP_KERNEL) == 0)
        return -ENOMEM;

    mutex_lock(&core_lock);
    /* Get next available bus number */
    res = idr_get_new_above(&i2c_adapter_idr, adapter,
                __i2c_first_dynamic_bus_num, &id);
    mutex_unlock(&core_lock);

    if (res < 0) {
        if (res == -EAGAIN)
            goto retry;
        return res;
    }

    adapter->nr = id;
    return i2c_register_adapter(adapter);  /* [KEY] Do actual registration */
}

/* Static bus number (for board files with predeclared devices) */
int i2c_add_numbered_adapter(struct i2c_adapter *adap)
{
    if (adap->nr == -1)
        return i2c_add_adapter(adap);  /* Dynamic if -1 */
    
    /* Otherwise, request specific bus number */
    ...
}
```

### 3.5 Adapter Lifetime and Removal

```c
int i2c_del_adapter(struct i2c_adapter *adap)
{
    /* [STEP 1] Verify adapter was registered */
    found = idr_find(&i2c_adapter_idr, adap->nr);
    if (found != adap)
        return -EINVAL;

    /* [STEP 2] Notify all drivers of removal */
    bus_for_each_drv(&i2c_bus_type, NULL, adap, __process_removed_adapter);

    /* [STEP 3] Remove sysfs-created clients */
    list_for_each_entry_safe(client, next, &adap->userspace_clients, ...) {
        i2c_unregister_device(client);
    }

    /* [STEP 4] Unregister all clients (two passes) */
    device_for_each_child(&adap->dev, NULL, __unregister_client);
    device_for_each_child(&adap->dev, NULL, __unregister_dummy);

    /* [STEP 5] Unregister from driver model */
    device_unregister(&adap->dev);

    /* [STEP 6] Wait for all references to be released */
    wait_for_completion(&adap->dev_released);

    /* [STEP 7] Free bus ID */
    idr_remove(&i2c_adapter_idr, adap->nr);

    return 0;
}
```

**Key invariant**: Adapter cannot be removed while clients exist!

---

## Phase 4 — struct i2c_client (Devices on the Bus)

### 4.1 What an i2c_client Represents

An `i2c_client` represents a **specific chip** at a **specific address** on a **specific bus**:
- NOT the driver – the driver is separate
- NOT the chip type – just THIS instance of a chip
- The "device node" in driver model terms

### 4.2 Structure Definition

```c
/* include/linux/i2c.h */

struct i2c_client {
    unsigned short flags;           /* [1] I2C_CLIENT_TEN, I2C_CLIENT_PEC */
    unsigned short addr;            /* [2] 7-bit address (in lower 7 bits) */
    char name[I2C_NAME_SIZE];       /* [3] Device type name */
    struct i2c_adapter *adapter;    /* [4] Bus this device is on */
    struct i2c_driver *driver;      /* [5] Bound driver (NULL until probe) */
    struct device dev;              /* [6] Driver model device */
    int irq;                        /* [7] IRQ number (if any) */
    struct list_head detected;      /* [8] For driver detection list */
};

#define to_i2c_client(d) container_of(d, struct i2c_client, dev)
```

### 4.3 How Clients Are Created

**Method 1: Board Info (v3.2 Style)**

```c
/* In arch/arm/mach-xxx/board-yyy.c */

static struct i2c_board_info my_i2c_devices[] __initdata = {
    {
        I2C_BOARD_INFO("tmp102", 0x48),  /* type, address */
        .irq = IRQ_TEMP,
        .platform_data = &tmp102_pdata,
    },
    {
        I2C_BOARD_INFO("24c02", 0x50),
    },
};

static void __init my_board_init(void)
{
    /* Register at arch_initcall, BEFORE adapters exist */
    i2c_register_board_info(0, my_i2c_devices, 
                            ARRAY_SIZE(my_i2c_devices));
}
```

**Method 2: Dynamic Creation**

```c
/* Create a client on a known adapter */
struct i2c_board_info info = {
    I2C_BOARD_INFO("tmp102", 0x48),
};

struct i2c_client *client = i2c_new_device(adapter, &info);

/* For multi-address chips, create "dummy" clients */
struct i2c_client *dummy = i2c_new_dummy(adapter, 0x51);
```

**Method 3: Driver Detection**

```c
/* Driver provides address_list and detect callback */
static struct i2c_driver my_driver = {
    .class = I2C_CLASS_HWMON,
    .detect = my_detect,
    .address_list = (const unsigned short[]){ 0x48, 0x49, I2C_CLIENT_END },
};

static int my_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    /* Probe hardware to see if it's really this chip */
    int id = i2c_smbus_read_byte_data(client, ID_REG);
    if (id != EXPECTED_ID)
        return -ENODEV;
    
    strlcpy(info->type, "my-sensor", I2C_NAME_SIZE);
    return 0;
}
```

### 4.4 Client Addressing Rules

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    I²C ADDRESS SPACE (7-bit)                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   Address Range    Usage                                                    │
│   ─────────────    ─────                                                    │
│   0x00             General call (broadcast)        [RESERVED]               │
│   0x01             CBUS compatibility              [RESERVED]               │
│   0x02             Different bus format            [RESERVED]               │
│   0x03             Reserved for future use         [RESERVED]               │
│   0x04 - 0x07      Hs-mode master code             [RESERVED]               │
│   0x08 - 0x77      USABLE FOR DEVICES              ← Normal range           │
│   0x78 - 0x7B      10-bit addressing               [RESERVED]               │
│   0x7C - 0x7F      Reserved for future use         [RESERVED]               │
│                                                                             │
│   VALIDATION in i2c_check_addr_validity():                                  │
│   • Reject addr < 0x08 or addr > 0x77 for probing                          │
│   • 10-bit addresses use I2C_CLIENT_TEN flag                               │
│                                                                             │
│   ADDRESS CONFLICT DETECTION:                                               │
│   • i2c_check_addr_busy() walks device tree                                 │
│   • Handles mux hierarchies (walks up and down)                            │
│   • Returns -EBUSY if address already in use                               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.5 Client Private Data

```c
/* Standard pattern: use i2c_set_clientdata */

struct my_chip_data {
    struct mutex lock;
    int cached_value;
    /* ... chip-specific state ... */
};

static int my_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct my_chip_data *data;
    
    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;
    
    /* [KEY] Store private data */
    i2c_set_clientdata(client, data);
    
    mutex_init(&data->lock);
    return 0;
}

static int my_remove(struct i2c_client *client)
{
    struct my_chip_data *data = i2c_get_clientdata(client);
    
    kfree(data);
    return 0;
}
```

---

## Phase 5 — struct i2c_driver (Device Drivers)

### 5.1 What an i2c_driver Represents

An `i2c_driver` knows how to **use a specific type of chip**:
- Contains chip-specific logic
- Binds to matching `i2c_client` instances
- Platform/bus independent

### 5.2 Structure Definition

```c
/* include/linux/i2c.h */

struct i2c_driver {
    unsigned int class;                     /* [1] For detection matching */

    /* Legacy (deprecated in v3.2) */
    int (*attach_adapter)(struct i2c_adapter *) __deprecated;
    int (*detach_adapter)(struct i2c_adapter *) __deprecated;

    /* [KEY] Standard driver model callbacks */
    int (*probe)(struct i2c_client *, const struct i2c_device_id *);
    int (*remove)(struct i2c_client *);

    /* Power management */
    void (*shutdown)(struct i2c_client *);
    int (*suspend)(struct i2c_client *, pm_message_t mesg);
    int (*resume)(struct i2c_client *);

    /* SMBus alert protocol */
    void (*alert)(struct i2c_client *, unsigned int data);

    /* Generic command interface (rarely used) */
    int (*command)(struct i2c_client *client, unsigned int cmd, void *arg);

    /* [KEY] Embedded driver structure */
    struct device_driver driver;

    /* [KEY] Device matching table */
    const struct i2c_device_id *id_table;

    /* Detection support */
    int (*detect)(struct i2c_client *, struct i2c_board_info *);
    const unsigned short *address_list;
    struct list_head clients;   /* Detected clients (core use only) */
};

#define to_i2c_driver(d) container_of(d, struct i2c_driver, driver)
```

### 5.3 Complete Driver Example

```c
/* drivers/hwmon/tmp102.c (simplified) */

#define DRIVER_NAME "tmp102"

/* [1] Device ID table for matching */
static const struct i2c_device_id tmp102_id[] = {
    { "tmp102", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, tmp102_id);

/* [2] Probe: called when client matches */
static int __devinit tmp102_probe(struct i2c_client *client,
                  const struct i2c_device_id *id)
{
    struct tmp102 *tmp102;
    int status;

    /* Check functionality */
    if (!i2c_check_functionality(client->adapter,
            I2C_FUNC_SMBUS_WORD_DATA)) {
        dev_err(&client->dev, "adapter doesn't support SMBus word\n");
        return -EIO;
    }

    /* Allocate private data */
    tmp102 = kzalloc(sizeof(*tmp102), GFP_KERNEL);
    if (!tmp102)
        return -ENOMEM;

    i2c_set_clientdata(client, tmp102);
    mutex_init(&tmp102->lock);

    /* Initialize hardware */
    status = i2c_smbus_read_word_swapped(client, TMP102_CONF_REG);
    if (status < 0)
        goto fail;
    
    /* ... rest of initialization ... */
    
    return 0;

fail:
    kfree(tmp102);
    return status;
}

/* [3] Remove: cleanup */
static int __devexit tmp102_remove(struct i2c_client *client)
{
    struct tmp102 *tmp102 = i2c_get_clientdata(client);
    
    /* ... cleanup ... */
    
    kfree(tmp102);
    return 0;
}

/* [4] Driver structure */
static struct i2c_driver tmp102_driver = {
    .driver.name    = DRIVER_NAME,
    .probe          = tmp102_probe,
    .remove         = __devexit_p(tmp102_remove),
    .id_table       = tmp102_id,
};

/* [5] Module init/exit */
static int __init tmp102_init(void)
{
    return i2c_add_driver(&tmp102_driver);
}

static void __exit tmp102_exit(void)
{
    i2c_del_driver(&tmp102_driver);
}

module_init(tmp102_init);
module_exit(tmp102_exit);
```

### 5.4 Relationship to Generic Driver Model

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   i2c_driver vs device_driver                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   struct i2c_driver {                                                       │
│       ...                                                                   │
│       int (*probe)(struct i2c_client *, ...);   ◄── I²C-specific callback  │
│       int (*remove)(struct i2c_client *);                                   │
│       ...                                                                   │
│       struct device_driver driver;               ◄── Embedded generic drv  │
│       ...                                                                   │
│   };                                                                        │
│                                                                             │
│   WHEN REGISTERING:                                                         │
│   ─────────────────                                                         │
│   i2c_add_driver(&my_driver)                                                │
│       │                                                                     │
│       ├── my_driver.driver.bus = &i2c_bus_type;                            │
│       │                                                                     │
│       └── driver_register(&my_driver.driver);   ◄── Generic registration   │
│                                                                             │
│   WHEN PROBING:                                                             │
│   ──────────────                                                            │
│   i2c_bus_type.probe(dev)                        ◄── Called by driver core │
│       │                                                                     │
│       └── i2c_device_probe(dev)                                            │
│               │                                                             │
│               └── driver->probe(client, id);    ◄── Calls I²C probe        │
│                                                                             │
│   This pattern allows I²C to add its own logic while using                  │
│   the standard driver model infrastructure.                                 │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Phase 6 — Ops, Callbacks, and Contracts

### 6.1 The i2c_algorithm Callbacks

```c
/* include/linux/i2c.h */

struct i2c_algorithm {
    /* [KEY] Raw I²C message transfer */
    int (*master_xfer)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
    
    /* [KEY] Native SMBus transfer (optional) */
    int (*smbus_xfer)(struct i2c_adapter *adap, u16 addr,
              unsigned short flags, char read_write,
              u8 command, int size, union i2c_smbus_data *data);

    /* [KEY] Report supported features */
    u32 (*functionality)(struct i2c_adapter *);
};
```

### 6.2 Callback Contracts

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    CALLBACK CONTRACT TABLE                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  CALLBACK              WHEN CALLED           CONTEXT        CONTRACT        │
│  ────────────────────────────────────────────────────────────────────────   │
│                                                                             │
│  master_xfer()         i2c_transfer()        process        • MAY sleep     │
│                                              (usually)      • Lock held     │
│                                                             • Return >0=OK  │
│                                                             • Return <0=err │
│                                                                             │
│  smbus_xfer()          i2c_smbus_xfer()      process        • MAY sleep     │
│                        (if provided)                        • Lock held     │
│                                                             • Can be NULL   │
│                                                             • Core emulates │
│                                                                             │
│  functionality()       i2c_check_functionality process      • MUST NOT sleep│
│                        (and others)                         • Return bitmask│
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  DRIVER CALLBACKS                                                           │
│  ────────────────────────────────────────────────────────────────────────   │
│                                                                             │
│  probe()               device_register()     process        • MAY sleep     │
│                        or driver_register()                 • 0=success     │
│                                                             • <0 = no bind  │
│                                                                             │
│  remove()              device_unregister()   process        • MAY sleep     │
│                        or driver_unregister()               • Free resources│
│                                                                             │
│  suspend()             System suspend        process        • MAY sleep     │
│                                                             • Save state    │
│                                                                             │
│  resume()              System resume         process        • MAY sleep     │
│                                                             • Restore state │
│                                                                             │
│  shutdown()            System shutdown       process        • Quick cleanup │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 所有 I²C 回调都可以睡眠（非原子上下文）
- `master_xfer` 返回成功传输的消息数，不是字节数
- `functionality` 返回能力位掩码，如 `I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE`

### 6.3 The xxx->ops->yyy() Pattern

```c
/*
 * i2c_algorithm follows the kernel's ops pattern:
 *
 *   Consumer calls:           Core routes to:              Driver implements:
 *   ──────────────────        ─────────────────           ──────────────────
 *   i2c_transfer(adap, ...) → adap->algo->master_xfer() → my_master_xfer()
 *   i2c_smbus_xfer(...)     → adap->algo->smbus_xfer()  → my_smbus_xfer()
 *                               or
 *                             → i2c_smbus_xfer_emulated()  [fallback]
 */

/* Example algorithm implementation */
static int my_master_xfer(struct i2c_adapter *adap, 
                          struct i2c_msg *msgs, int num)
{
    struct my_i2c_priv *priv = i2c_get_adapdata(adap);
    int i, ret;

    for (i = 0; i < num; i++) {
        if (msgs[i].flags & I2C_M_RD)
            ret = my_i2c_read(priv, msgs[i].addr, 
                              msgs[i].buf, msgs[i].len);
        else
            ret = my_i2c_write(priv, msgs[i].addr,
                               msgs[i].buf, msgs[i].len);
        if (ret < 0)
            return ret;
    }
    return num;  /* [KEY] Return number of messages transferred */
}

static u32 my_functionality(struct i2c_adapter *adap)
{
    return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm my_algo = {
    .master_xfer   = my_master_xfer,
    .functionality = my_functionality,
    /* smbus_xfer = NULL → core will emulate SMBus via I2C */
};
```

### 6.4 What Core Guarantees vs Driver Must Guarantee

| Core Guarantees | Driver Must Guarantee |
|-----------------|----------------------|
| Bus is locked during transfer | Correct timing for I²C protocol |
| Retry on EAGAIN (arbitration loss) | Return correct error codes |
| Timeout handling | Handle NACK appropriately |
| SMBus emulation if no smbus_xfer | Report correct functionality |
| client->adapter is valid in callbacks | Not access freed resources |

---

## Phase 7 — Driver Registration & Matching

### 7.1 How i2c_add_driver() Works

```c
/* drivers/i2c/i2c-core.c */

#define i2c_add_driver(driver) \
    i2c_register_driver(THIS_MODULE, driver)

int i2c_register_driver(struct module *owner, struct i2c_driver *driver)
{
    int res;

    /* [STEP 1] Wire up to I²C bus */
    driver->driver.owner = owner;
    driver->driver.bus = &i2c_bus_type;

    /* [STEP 2] Register with driver core */
    res = driver_register(&driver->driver);
    if (res)
        return res;
    
    /* At this point, driver_register() has already:
     * - Called probe() for any matching existing clients
     */

    /* [STEP 3] Initialize detection list */
    INIT_LIST_HEAD(&driver->clients);
    
    /* [STEP 4] Walk all adapters for detection */
    i2c_for_each_dev(driver, __process_new_driver);

    return 0;
}

static int __process_new_driver(struct device *dev, void *data)
{
    if (dev->type != &i2c_adapter_type)
        return 0;
    return i2c_do_add_adapter(data, to_i2c_adapter(dev));
}

static int i2c_do_add_adapter(struct i2c_driver *driver,
                  struct i2c_adapter *adap)
{
    /* Try to detect devices on this adapter */
    i2c_detect(adap, driver);
    return 0;
}
```

### 7.2 Matching Logic

```c
/* drivers/i2c/i2c-core.c */

static int i2c_device_match(struct device *dev, struct device_driver *drv)
{
    struct i2c_client *client = i2c_verify_client(dev);
    struct i2c_driver *driver;

    if (!client)
        return 0;

    /* [OPTION 1] Device Tree matching */
    if (of_driver_match_device(dev, drv))
        return 1;

    driver = to_i2c_driver(drv);
    
    /* [OPTION 2] ID table matching (most common) */
    if (driver->id_table)
        return i2c_match_id(driver->id_table, client) != NULL;

    return 0;
}

static const struct i2c_device_id *i2c_match_id(
                const struct i2c_device_id *id,
                const struct i2c_client *client)
{
    while (id->name[0]) {
        /* [KEY] Match by NAME, not by address! */
        if (strcmp(client->name, id->name) == 0)
            return id;
        id++;
    }
    return NULL;
}
```

**说明:**
- 匹配基于设备名称（`client->name`），而非地址
- 地址在 `i2c_client` 创建时已确定
- `i2c_device_id.driver_data` 可传递变体信息

### 7.3 Probe/Remove Lifecycle

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    PROBE/REMOVE LIFECYCLE                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   SCENARIO 1: Driver loads AFTER device exists                              │
│   ─────────────────────────────────────────────                             │
│                                                                             │
│   1. Board file registers i2c_board_info                                    │
│   2. Adapter driver loads, calls i2c_add_adapter()                          │
│   3. i2c_scan_static_board_info() creates i2c_client                        │
│   4. Later, i2c_add_driver(&my_driver)                                      │
│   5. driver_register() → bus matches → i2c_device_probe()                   │
│   6. my_driver.probe(client, id) called                                     │
│                                                                             │
│   SCENARIO 2: Device appears AFTER driver loaded                            │
│   ───────────────────────────────────────────────                           │
│                                                                             │
│   1. i2c_add_driver(&my_driver) → driver registered                         │
│   2. Later, adapter loads, calls i2c_add_adapter()                          │
│   3. i2c_scan_static_board_info() creates i2c_client                        │
│   4. device_register(&client->dev) → bus matches                            │
│   5. my_driver.probe(client, id) called                                     │
│                                                                             │
│   REMOVAL:                                                                  │
│   ─────────                                                                 │
│   i2c_unregister_device(client)                                             │
│       → device_unregister(&client->dev)                                     │
│           → i2c_device_remove()                                             │
│               → driver->remove(client)                                      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 7.4 Handling Probe Failure

```c
static int i2c_device_probe(struct device *dev)
{
    struct i2c_client *client = i2c_verify_client(dev);
    struct i2c_driver *driver;
    int status;

    driver = to_i2c_driver(dev->driver);
    
    /* Check requirements */
    if (!driver->probe || !driver->id_table)
        return -ENODEV;
    
    /* Link driver to client (optimistic) */
    client->driver = driver;
    
    /* Call driver probe */
    status = driver->probe(client, i2c_match_id(driver->id_table, client));
    
    if (status) {
        /* [KEY] Probe failed: clean up linkage */
        client->driver = NULL;
        i2c_set_clientdata(client, NULL);
    }
    
    return status;
}
```

**Key rule**: If `probe()` returns non-zero, the device is NOT bound to the driver.

---

## Phase 8 — End-to-End Data Transfer Path

### 8.1 Complete Transfer Path Trace

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              COMPLETE I²C TRANSFER: i2c_smbus_read_byte_data()               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   CLIENT DRIVER (e.g., tmp102)                                              │
│   ────────────────────────────                                              │
│                                                                             │
│   val = i2c_smbus_read_byte_data(client, TMP102_TEMP_REG);                  │
│          │                                                                  │
│          ▼                                                                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ i2c_smbus_read_byte_data() [i2c-core.c]                             │   │
│   │   return i2c_smbus_xfer(client->adapter, client->addr, ...)         │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│          │                                                                  │
│          ▼                                                                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ i2c_smbus_xfer() [i2c-core.c]                                       │   │
│   │                                                                      │   │
│   │   if (adapter->algo->smbus_xfer) {                                  │   │
│   │       i2c_lock_adapter(adapter);        ◄── [1] Acquire bus lock   │   │
│   │       for (try = 0; try <= retries; try++) {                        │   │
│   │           res = adapter->algo->smbus_xfer(...);                     │   │
│   │           if (res != -EAGAIN) break;    ◄── [2] Retry on arb loss  │   │
│   │       }                                                              │   │
│   │       i2c_unlock_adapter(adapter);      ◄── [3] Release lock       │   │
│   │   } else {                                                           │   │
│   │       res = i2c_smbus_xfer_emulated(...);◄── [4] Emulate via I2C   │   │
│   │   }                                                                  │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│          │                                                                  │
│          │ If smbus_xfer is NULL, emulate:                                 │
│          ▼                                                                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ i2c_smbus_xfer_emulated() [i2c-core.c]                              │   │
│   │                                                                      │   │
│   │   /* Build i2c_msg array from SMBus parameters */                   │   │
│   │   msg[0].addr = addr;                                               │   │
│   │   msg[0].flags = 0;               /* Write register address */      │   │
│   │   msg[0].len = 1;                                                   │   │
│   │   msg[0].buf = &command;                                            │   │
│   │                                                                      │   │
│   │   msg[1].addr = addr;                                               │   │
│   │   msg[1].flags = I2C_M_RD;        /* Read data byte */              │   │
│   │   msg[1].len = 1;                                                   │   │
│   │   msg[1].buf = &data->byte;                                         │   │
│   │                                                                      │   │
│   │   return i2c_transfer(adapter, msg, 2);                             │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│          │                                                                  │
│          ▼                                                                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ i2c_transfer() [i2c-core.c]                                         │   │
│   │                                                                      │   │
│   │   if (in_atomic() || irqs_disabled()) {                             │   │
│   │       ret = i2c_trylock_adapter(adap);  ◄── Non-blocking try       │   │
│   │       if (!ret) return -EAGAIN;                                     │   │
│   │   } else {                                                           │   │
│   │       i2c_lock_adapter(adap);           ◄── Blocking lock          │   │
│   │   }                                                                  │   │
│   │                                                                      │   │
│   │   for (try = 0; try <= adap->retries; try++) {                      │   │
│   │       ret = adap->algo->master_xfer(adap, msgs, num);               │   │
│   │       if (ret != -EAGAIN) break;                                    │   │
│   │       if (time_after(jiffies, orig + adap->timeout)) break;         │   │
│   │   }                                                                  │   │
│   │                                                                      │   │
│   │   i2c_unlock_adapter(adap);                                         │   │
│   │   return ret;                                                       │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│          │                                                                  │
│          │ adap->algo->master_xfer()                                       │
│          ▼                                                                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ ADAPTER DRIVER (e.g., i2c-imx.c)                                    │   │
│   │                                                                      │   │
│   │ static int i2c_imx_xfer(struct i2c_adapter *adapter,                │   │
│   │                         struct i2c_msg *msgs, int num) {            │   │
│   │     struct imx_i2c_struct *i2c_imx = i2c_get_adapdata(adapter);     │   │
│   │                                                                      │   │
│   │     /* Start condition */                                           │   │
│   │     i2c_imx_start(i2c_imx);                                         │   │
│   │                                                                      │   │
│   │     for (i = 0; i < num; i++) {                                     │   │
│   │         if (msgs[i].flags & I2C_M_RD)                               │   │
│   │             result = i2c_imx_read(i2c_imx, &msgs[i]);               │   │
│   │         else                                                        │   │
│   │             result = i2c_imx_write(i2c_imx, &msgs[i]);              │   │
│   │     }                                                                │   │
│   │                                                                      │   │
│   │     /* Stop condition */                                            │   │
│   │     i2c_imx_stop(i2c_imx);                                          │   │
│   │                                                                      │   │
│   │     return (result < 0) ? result : num;                             │   │
│   │ }                                                                    │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│          │                                                                  │
│          ▼                                                                  │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                         HARDWARE                                     │   │
│   │   SDA: ─┐     ┌─────────────┐     ┌─────────┐                       │   │
│   │         └─────┘ addr + R/W  └─────┘  data   └───                    │   │
│   │   SCL: ─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─                    │   │
│   │         └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └                    │   │
│   │       START      BYTE 1       ACK    BYTE 2   ACK STOP              │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- 高层 SMBus 调用自动转换为底层 I²C 消息
- 如果适配器不支持原生 SMBus，核心层使用 I²C 模拟
- 总线锁在整个传输过程中保持
- 重试机制处理总线仲裁失败

### 8.2 Locking in Detail

```c
/* Recursive locking for mux support */
void i2c_lock_adapter(struct i2c_adapter *adapter)
{
    struct i2c_adapter *parent = i2c_parent_is_i2c_adapter(adapter);

    if (parent)
        i2c_lock_adapter(parent);   /* Lock parent first */
    else
        rt_mutex_lock(&adapter->bus_lock);
}

void i2c_unlock_adapter(struct i2c_adapter *adapter)
{
    struct i2c_adapter *parent = i2c_parent_is_i2c_adapter(adapter);

    if (parent)
        i2c_unlock_adapter(parent); /* Unlock parent */
    else
        rt_mutex_unlock(&adapter->bus_lock);
}
```

---

## Phase 9 — Concurrency, Context, and Power

### 9.1 What I²C Operations May Sleep

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   SLEEPING IN I²C OPERATIONS                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ALL I²C transfer operations MAY SLEEP because:                            │
│   ─────────────────────────────────────────────                             │
│   1. Bus locking uses rt_mutex (can sleep)                                  │
│   2. Hardware may use wait_for_completion()                                 │
│   3. Clock stretching requires waiting                                      │
│                                                                             │
│   CONSEQUENCE:                                                              │
│   ─────────────                                                             │
│   • CANNOT call from interrupt context                                      │
│   • CANNOT call with spinlock held                                          │
│   • CANNOT call with interrupts disabled                                    │
│                                                                             │
│   CHECK in i2c_transfer():                                                  │
│   ────────────────────────                                                  │
│   if (in_atomic() || irqs_disabled()) {                                     │
│       ret = i2c_trylock_adapter(adap);                                      │
│       if (!ret)                                                             │
│           return -EAGAIN;  /* Can't get lock, don't block */               │
│   } else {                                                                  │
│       i2c_lock_adapter(adap);  /* OK to block */                           │
│   }                                                                         │
│                                                                             │
│   VALID CONTEXTS:                                                           │
│   ───────────────                                                           │
│   ✓ Process context (normal driver code)                                   │
│   ✓ Workqueue (work_struct handlers)                                       │
│   ✓ Kernel threads                                                          │
│   ✓ Probe/remove callbacks                                                  │
│   ✓ Sysfs callbacks                                                         │
│                                                                             │
│   INVALID CONTEXTS:                                                         │
│   ─────────────────                                                         │
│   ✗ Interrupt handlers (hardirq, softirq)                                  │
│   ✗ Timer callbacks                                                         │
│   ✗ While holding spinlock                                                  │
│   ✗ With preemption disabled                                                │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 9.2 Locking Inside I²C Core

| Lock | Type | Purpose |
|------|------|---------|
| `core_lock` | mutex | Protect adapter IDR, driver lists |
| `adapter->bus_lock` | rt_mutex | Serialize bus access |
| `adapter->userspace_clients_lock` | mutex | Protect sysfs client list |
| `__i2c_board_lock` | rwsem | Protect board info list |

### 9.3 Interrupt and Workqueue Integration

```c
/* Pattern: Defer I²C work from interrupt to workqueue */

struct my_chip {
    struct i2c_client *client;
    struct work_struct irq_work;
    int irq;
};

static irqreturn_t my_chip_irq(int irq, void *dev_id)
{
    struct my_chip *chip = dev_id;
    
    /* [KEY] Cannot do I²C here - schedule work instead */
    schedule_work(&chip->irq_work);
    
    return IRQ_HANDLED;
}

static void my_chip_work(struct work_struct *work)
{
    struct my_chip *chip = container_of(work, struct my_chip, irq_work);
    
    /* [KEY] Workqueue context - I²C is safe here */
    int status = i2c_smbus_read_byte_data(chip->client, STATUS_REG);
    
    /* Handle the interrupt... */
}

static int my_probe(struct i2c_client *client, ...)
{
    struct my_chip *chip;
    
    chip = kzalloc(sizeof(*chip), GFP_KERNEL);
    chip->client = client;
    INIT_WORK(&chip->irq_work, my_chip_work);
    
    request_irq(chip->irq, my_chip_irq, 0, "my-chip", chip);
    
    return 0;
}
```

### 9.4 Suspend/Resume for I²C Drivers

```c
/* Using legacy callbacks (v3.2) */
static int my_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct my_chip_data *data = i2c_get_clientdata(client);
    
    /* Save any volatile state */
    data->saved_config = i2c_smbus_read_byte_data(client, CONFIG_REG);
    
    /* Put chip in low-power mode */
    i2c_smbus_write_byte_data(client, CONFIG_REG, SLEEP_MODE);
    
    return 0;
}

static int my_resume(struct i2c_client *client)
{
    struct my_chip_data *data = i2c_get_clientdata(client);
    
    /* Restore saved state */
    i2c_smbus_write_byte_data(client, CONFIG_REG, data->saved_config);
    
    return 0;
}

static struct i2c_driver my_driver = {
    .driver.name = "my-chip",
    .probe    = my_probe,
    .remove   = my_remove,
    .suspend  = my_suspend,    /* Legacy PM */
    .resume   = my_resume,
    .id_table = my_id_table,
};
```

---

## Phase 10 — Common Bugs, Pitfalls, and Design Lessons

### 10.1 Bug: Sleeping in Atomic Context

```c
/* BUG: I²C from interrupt handler */
static irqreturn_t bad_irq_handler(int irq, void *data)
{
    struct my_chip *chip = data;
    
    /* [BUG] This will crash or hang! */
    int val = i2c_smbus_read_byte_data(chip->client, STATUS_REG);
    
    return IRQ_HANDLED;
}

/* FIX: Use threaded IRQ or workqueue */
static irqreturn_t good_irq_handler(int irq, void *data)
{
    struct my_chip *chip = data;
    schedule_work(&chip->work);  /* Defer to workqueue */
    return IRQ_HANDLED;
}

/* Or use threaded interrupt */
request_threaded_irq(irq, NULL, my_threaded_handler, 
                     IRQF_ONESHOT, "my-chip", chip);
```

### 10.2 Bug: Adapter Lock Misuse

```c
/* BUG: Manually locking without proper protocol */
static int bad_transfer(struct i2c_client *client)
{
    struct i2c_adapter *adap = client->adapter;
    
    i2c_lock_adapter(adap);
    
    /* Do multiple transfers atomically... */
    i2c_smbus_write_byte(client, 0x01);
    /* [BUG] i2c_smbus_write_byte will try to lock again! */
    
    i2c_unlock_adapter(adap);
}

/* FIX: Use i2c_transfer with multiple messages */
static int good_transfer(struct i2c_client *client)
{
    struct i2c_msg msgs[2] = {
        { .addr = client->addr, .flags = 0, .len = 1, .buf = buf1 },
        { .addr = client->addr, .flags = 0, .len = 1, .buf = buf2 },
    };
    
    /* Single locked transaction with multiple messages */
    return i2c_transfer(client->adapter, msgs, 2);
}
```

### 10.3 Bug: Incorrect Client Lifetime

```c
/* BUG: Using client after it may be freed */
static struct i2c_client *saved_client;  /* [BUG] Global pointer! */

static int bad_probe(struct i2c_client *client, ...)
{
    saved_client = client;  /* [BUG] Dangling pointer after remove */
    return 0;
}

static void bad_timer_callback(unsigned long data)
{
    /* [BUG] Client may be gone by now! */
    i2c_smbus_read_byte(saved_client, REG);
}

/* FIX: Proper reference counting and cleanup */
static int good_remove(struct i2c_client *client)
{
    struct my_data *data = i2c_get_clientdata(client);
    
    /* Cancel pending work BEFORE device goes away */
    cancel_delayed_work_sync(&data->work);
    
    /* Now safe to free */
    kfree(data);
    return 0;
}
```

### 10.4 Bug: Broken Probe Error Path

```c
/* BUG: Resources leaked on probe failure */
static int bad_probe(struct i2c_client *client, ...)
{
    struct my_data *data;
    int ret;
    
    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;
    
    data->workqueue = create_singlethread_workqueue("my-wq");
    if (!data->workqueue)
        return -ENOMEM;  /* [BUG] data leaked! */
    
    ret = request_irq(client->irq, my_handler, 0, "my", data);
    if (ret)
        return ret;  /* [BUG] data and workqueue leaked! */
    
    return 0;
}

/* FIX: Proper error unwinding */
static int good_probe(struct i2c_client *client, ...)
{
    struct my_data *data;
    int ret;
    
    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;
    
    data->workqueue = create_singlethread_workqueue("my-wq");
    if (!data->workqueue) {
        ret = -ENOMEM;
        goto err_free_data;
    }
    
    ret = request_irq(client->irq, my_handler, 0, "my", data);
    if (ret)
        goto err_destroy_wq;
    
    i2c_set_clientdata(client, data);
    return 0;

err_destroy_wq:
    destroy_workqueue(data->workqueue);
err_free_data:
    kfree(data);
    return ret;
}
```

### 10.5 Bug: Address Conflicts

```c
/* BUG: Not checking if address is already used */
static int bad_secondary_probe(struct i2c_client *primary, ...)
{
    /* Create secondary client without checking */
    secondary = i2c_new_dummy(primary->adapter, 0x51);
    /* [BUG] May fail silently if 0x51 is already used */
}

/* FIX: Check return value */
static int good_secondary_probe(struct i2c_client *primary, ...)
{
    secondary = i2c_new_dummy(primary->adapter, 0x51);
    if (!secondary) {
        dev_err(&primary->dev, "Failed to create secondary at 0x51\n");
        return -EBUSY;
    }
    /* ... */
}
```

### 10.6 Summary: What Makes a Good I²C Driver

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      GOOD I²C DRIVER CHECKLIST                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ✓ FUNCTIONALITY CHECK                                                     │
│     • Check adapter capabilities before use                                 │
│     • Don't assume SMBus word support                                       │
│                                                                             │
│   ✓ PROPER ERROR HANDLING                                                   │
│     • Check all I²C transfer return values                                  │
│     • Unwind resources on probe failure                                     │
│     • Return appropriate error codes                                        │
│                                                                             │
│   ✓ CONTEXT AWARENESS                                                       │
│     • Never call I²C from atomic context                                    │
│     • Use workqueues for interrupt-triggered I²C                            │
│     • Be aware that transfers may sleep                                     │
│                                                                             │
│   ✓ LIFETIME MANAGEMENT                                                     │
│     • Cancel pending work before remove                                     │
│     • Don't store global client pointers                                    │
│     • Free all resources in remove                                          │
│                                                                             │
│   ✓ POWER MANAGEMENT                                                        │
│     • Implement suspend/resume if chip has PM features                      │
│     • Save/restore volatile registers                                       │
│                                                                             │
│   ✓ DEVICE MODEL INTEGRATION                                                │
│     • Use i2c_set_clientdata for private data                              │
│     • Provide sysfs attributes where useful                                 │
│     • Use devm_* for automatic cleanup                                      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 10.7 How the Layered Architecture Prevents Chaos

| Problem | How I²C Subsystem Solves It |
|---------|---------------------------|
| Bus access conflicts | Single rt_mutex per adapter |
| Address conflicts | i2c_check_addr_busy() |
| Driver portability | Unified i2c_transfer() API |
| Device discovery | Board info + detection + DT |
| Power management | PM callbacks in driver model |
| Hot-plug | Dynamic registration/unregistration |

### 10.8 Lessons for Other Bus Subsystems

The I²C subsystem architecture generalizes to:
- **SPI**: Similar adapter/device/driver separation
- **USB**: Host controller / device / driver
- **PCIe**: Root complex / device / driver
- **Platform bus**: Pseudo-bus for non-discoverable devices

Key pattern: **Separate the "how to access" from "what to do"**

---

## Appendix A — Walking Through Real I²C Drivers

### A.1 Adapter Driver: i2c-versatile.c

A minimal but complete adapter driver for ARM Versatile:

```c
/* drivers/i2c/busses/i2c-versatile.c */

/* [1] Private data structure */
struct i2c_versatile {
    struct i2c_adapter   adap;      /* [KEY] Embedded adapter */
    struct i2c_algo_bit_data algo;  /* Bit-bang algorithm data */
    void __iomem         *base;     /* Register base */
};

/* [2] Bit-bang callbacks */
static void i2c_versatile_setsda(void *data, int state)
{
    struct i2c_versatile *i2c = data;
    writel(SDA, i2c->base + (state ? I2C_CONTROLS : I2C_CONTROLC));
}

static int i2c_versatile_getsda(void *data)
{
    struct i2c_versatile *i2c = data;
    return !!(readl(i2c->base + I2C_CONTROL) & SDA);
}

/* [3] Probe function */
static int i2c_versatile_probe(struct platform_device *dev)
{
    struct i2c_versatile *i2c;
    struct resource *r;
    int ret;

    /* Get memory resource */
    r = platform_get_resource(dev, IORESOURCE_MEM, 0);
    
    /* Allocate private data */
    i2c = kzalloc(sizeof(struct i2c_versatile), GFP_KERNEL);
    
    /* Map registers */
    i2c->base = ioremap(r->start, resource_size(r));

    /* Configure adapter */
    i2c->adap.owner = THIS_MODULE;
    strlcpy(i2c->adap.name, "Versatile I2C", sizeof(i2c->adap.name));
    i2c->adap.algo_data = &i2c->algo;  /* [KEY] Link to algorithm */
    i2c->adap.dev.parent = &dev->dev;
    
    /* Configure bit-bang algorithm */
    i2c->algo.setsda = i2c_versatile_setsda;
    i2c->algo.setscl = i2c_versatile_setscl;
    i2c->algo.getsda = i2c_versatile_getsda;
    i2c->algo.getscl = i2c_versatile_getscl;
    i2c->algo.udelay = 30;
    i2c->algo.timeout = HZ;
    i2c->algo.data = i2c;  /* [KEY] Private data for callbacks */

    /* Register adapter */
    ret = i2c_bit_add_bus(&i2c->adap);
    
    platform_set_drvdata(dev, i2c);
    return 0;
}

/* [4] Remove function */
static int i2c_versatile_remove(struct platform_device *dev)
{
    struct i2c_versatile *i2c = platform_get_drvdata(dev);
    
    i2c_del_adapter(&i2c->adap);
    return 0;
}
```

**说明:**
- 使用 `i2c_algo_bit_data` 实现位操作接口
- `i2c_bit_add_bus()` 提供完整的 `i2c_algorithm`
- 硬件寄存器通过 MMIO 访问

### A.2 Client Driver: tmp102.c

A typical temperature sensor driver:

```c
/* drivers/hwmon/tmp102.c (simplified) */

/* [1] Private data */
struct tmp102 {
    struct mutex lock;
    int temp[3];  /* Cached temperatures */
};

/* [2] ID table for matching */
static const struct i2c_device_id tmp102_id[] = {
    { "tmp102", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, tmp102_id);

/* [3] Read from chip */
static struct tmp102 *tmp102_update_device(struct i2c_client *client)
{
    struct tmp102 *tmp102 = i2c_get_clientdata(client);

    mutex_lock(&tmp102->lock);
    
    /* Read temperature register */
    int status = i2c_smbus_read_word_swapped(client, TMP102_TEMP_REG);
    if (status > -1)
        tmp102->temp[0] = tmp102_reg_to_mC(status);
    
    mutex_unlock(&tmp102->lock);
    return tmp102;
}

/* [4] Probe function */
static int tmp102_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
    struct tmp102 *tmp102;
    int status;

    /* [KEY] Check adapter functionality */
    if (!i2c_check_functionality(client->adapter,
            I2C_FUNC_SMBUS_WORD_DATA)) {
        dev_err(&client->dev, "SMBus word not supported\n");
        return -EIO;
    }

    /* Allocate and init private data */
    tmp102 = kzalloc(sizeof(*tmp102), GFP_KERNEL);
    if (!tmp102)
        return -ENOMEM;

    i2c_set_clientdata(client, tmp102);
    mutex_init(&tmp102->lock);

    /* Initialize hardware */
    status = i2c_smbus_read_word_swapped(client, TMP102_CONF_REG);
    if (status < 0)
        goto fail;

    /* ... configure chip ... */
    
    return 0;

fail:
    kfree(tmp102);
    return status;
}

/* [5] Remove function */
static int tmp102_remove(struct i2c_client *client)
{
    struct tmp102 *tmp102 = i2c_get_clientdata(client);
    
    /* ... cleanup ... */
    
    kfree(tmp102);
    return 0;
}

/* [6] Driver structure */
static struct i2c_driver tmp102_driver = {
    .driver.name = "tmp102",
    .probe       = tmp102_probe,
    .remove      = tmp102_remove,
    .id_table    = tmp102_id,
};

module_i2c_driver(tmp102_driver);  /* Convenience macro */
```

**说明:**
- 使用 `i2c_smbus_*` 便捷函数进行通信
- 始终检查适配器功能
- 使用互斥锁保护共享数据
- 正确处理探测失败

### A.3 Complete Data Flow Summary

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    COMPLETE I²C ARCHITECTURE SUMMARY                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   USER SPACE                                                                │
│       │                                                                     │
│       │ /dev/i2c-N (i2c-dev.c)                                             │
│       ▼                                                                     │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                    I²C CORE (i2c-core.c)                             │   │
│   │  • i2c_transfer(), i2c_smbus_xfer()                                 │   │
│   │  • Driver matching and binding                                       │   │
│   │  • Bus locking and retry logic                                       │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│       │                                                                     │
│   ┌───┴───────────────────────────────────────────────────────────┐         │
│   │                                                               │         │
│   ▼                                                               ▼         │
│   ┌───────────────────────┐                       ┌───────────────────────┐ │
│   │   CLIENT DRIVERS      │                       │   ADAPTER DRIVERS     │ │
│   │   (tmp102, eeprom)    │                       │   (i2c-imx, i2c-omap) │ │
│   │                       │                       │                       │ │
│   │   Use i2c_client to   │                       │   Implement           │ │
│   │   access chips        │                       │   i2c_algorithm       │ │
│   └───────────────────────┘                       └───────────────────────┘ │
│                                                               │             │
│                                                               ▼             │
│                                               ┌───────────────────────────┐ │
│                                               │      HARDWARE             │ │
│                                               │  SCL/SDA lines, I²C bus   │ │
│                                               └───────────────────────────┘ │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Appendix B — I²C vs SPI Architecture Comparison

### B.1 High-Level Comparison

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    I²C vs SPI: PHYSICAL LAYER DIFFERENCES                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   I²C (Inter-Integrated Circuit)           SPI (Serial Peripheral Interface)│
│   ────────────────────────────────        ─────────────────────────────────│
│                                                                             │
│   Wires: 2 (SDA + SCL)                    Wires: 4+ (MOSI + MISO + SCK + CS)│
│                                                                             │
│        ┌──────┐     ┌──────┐                   ┌──────┐     ┌──────┐        │
│        │Master│     │Slave │                   │Master│     │Slave │        │
│        └──┬───┘     └──┬───┘                   └──┬───┘     └──┬───┘        │
│           │            │                          │            │            │
│     SDA ──┼────────────┼─── (bidirectional)       │◄──MISO─────│            │
│     SCL ──┼────────────┼─── (clock)               │───MOSI────►│            │
│           │            │                          │───SCK─────►│            │
│        ┌──┴───┐     ┌──┴───┐                      │───CS──────►│            │
│        │Slave2│     │Slave3│                   ┌──┴───┐                     │
│        └──────┘     └──────┘                   │Slave2│◄──CS2               │
│                                                └──────┘                     │
│                                                                             │
│   Addressing: In-band (7/10-bit address)  Addressing: Out-of-band (CS line)│
│   Duplex: Half-duplex only                Duplex: Full-duplex possible     │
│   Speed: 100KHz - 3.4MHz                  Speed: 10MHz - 100MHz+           │
│   Distance: Short (30cm typical)          Distance: Very short (PCB traces)│
│   Multi-master: Supported                 Multi-master: Complex/rare       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- I²C 使用地址区分设备，SPI 使用独立的片选信号
- I²C 半双工（共享数据线），SPI 全双工（独立收发线）
- I²C 速度慢但布线简单，SPI 速度快但需要更多引脚

### B.2 Software Architecture Comparison

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    KERNEL OBJECT MAPPING: I²C vs SPI                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│           I²C SUBSYSTEM                       SPI SUBSYSTEM                 │
│   ─────────────────────────────       ─────────────────────────────         │
│                                                                             │
│   i2c_adapter                         spi_master                            │
│   (Bus controller)                    (Bus controller)                      │
│   ├─ name, nr                         ├─ bus_num, num_chipselect           │
│   ├─ algo (i2c_algorithm)             ├─ setup(), transfer()               │
│   ├─ bus_lock (rt_mutex)              ├─ bus_lock_mutex                    │
│   └─ dev (struct device)              └─ dev (struct device)               │
│                                                                             │
│   i2c_client                          spi_device                            │
│   (Device on bus)                     (Device on bus)                       │
│   ├─ addr (7/10-bit)                  ├─ chip_select (CS line number)      │
│   ├─ adapter (→ controller)           ├─ master (→ controller)             │
│   ├─ name                             ├─ modalias                          │
│   └─ dev (struct device)              ├─ mode (CPOL, CPHA, etc.)           │
│                                       ├─ max_speed_hz                       │
│                                       └─ dev (struct device)               │
│                                                                             │
│   i2c_driver                          spi_driver                            │
│   (Device driver)                     (Device driver)                       │
│   ├─ probe(client, id)                ├─ probe(spi)                        │
│   ├─ remove(client)                   ├─ remove(spi)                       │
│   ├─ id_table                         ├─ id_table                          │
│   └─ driver (device_driver)           └─ driver (device_driver)            │
│                                                                             │
│   i2c_algorithm                       (embedded in spi_master)              │
│   ├─ master_xfer()                    └─ transfer()                        │
│   ├─ smbus_xfer()                        setup()                           │
│   └─ functionality()                     cleanup()                          │
│                                                                             │
│   i2c_msg                             spi_transfer + spi_message            │
│   ├─ addr                             ├─ tx_buf, rx_buf                    │
│   ├─ flags (RD/WR)                    ├─ len                               │
│   ├─ len                              ├─ cs_change                         │
│   └─ buf                              └─ speed_hz, bits_per_word           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### B.3 Key Architectural Differences

| Aspect | I²C | SPI |
|--------|-----|-----|
| **Algorithm separation** | `i2c_algorithm` is separate struct | Callbacks embedded in `spi_master` |
| **Transfer API** | `i2c_transfer(adapter, msgs, num)` | `spi_sync(spi, message)` or `spi_async()` |
| **Async support** | No native async (blocking only) | First-class async with completion callback |
| **Device addressing** | Address in message (`msg.addr`) | CS line selected per-device (`spi->chip_select`) |
| **Per-transfer config** | Minimal (just R/W flag) | Rich: speed, bits_per_word, CS changes |
| **Memory allocation** | Core allocates `i2c_client` | Core allocates `spi_device` |
| **Master allocation** | Driver allocates `i2c_adapter` | Core provides `spi_alloc_master()` |

### B.4 Transfer Flow Comparison

```c
/* ═══════════════════════════════════════════════════════════════════════════ */
/*                              I²C TRANSFER                                    */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* Simple: Build message array, call i2c_transfer */
struct i2c_msg msgs[2] = {
    { .addr = 0x50, .flags = 0,        .len = 1, .buf = &reg },
    { .addr = 0x50, .flags = I2C_M_RD, .len = 2, .buf = data },
};
ret = i2c_transfer(client->adapter, msgs, 2);  /* Blocking */

/* Or use SMBus helpers (preferred for simple ops) */
val = i2c_smbus_read_word_data(client, reg);   /* Blocking */


/* ═══════════════════════════════════════════════════════════════════════════ */
/*                              SPI TRANSFER                                    */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* More complex: Build transfer list, add to message, submit */
struct spi_transfer xfers[2] = {
    { .tx_buf = &cmd, .len = 1 },                    /* Write command */
    { .rx_buf = data, .len = 2, .speed_hz = 1000000 }, /* Read response */
};

struct spi_message msg;
spi_message_init(&msg);
spi_message_add_tail(&xfers[0], &msg);
spi_message_add_tail(&xfers[1], &msg);

/* Synchronous (blocking) */
ret = spi_sync(spi, &msg);

/* Or asynchronous (non-blocking) */
msg.complete = my_callback;
msg.context = my_data;
ret = spi_async(spi, &msg);  /* Returns immediately, callback later */

/* Or use helpers for simple cases */
ret = spi_write_then_read(spi, cmd, 1, data, 2);
```

### B.5 Why the Differences Exist

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    WHY I²C AND SPI DIFFER IN DESIGN                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   I²C DESIGN RATIONALE:                                                     │
│   ─────────────────────                                                     │
│   • Half-duplex → simpler message model (one buffer per operation)          │
│   • In-band addressing → address in every message                           │
│   • Low speed → blocking transfers acceptable                               │
│   • SMBus legacy → helper functions for common patterns                     │
│   • Multi-master → complex arbitration, simpler API compensates             │
│                                                                             │
│   SPI DESIGN RATIONALE:                                                     │
│   ─────────────────────                                                     │
│   • Full-duplex → tx_buf + rx_buf in every transfer                         │
│   • High speed → async transfers essential for performance                  │
│   • Per-device config → mode/speed stored in spi_device                     │
│   • DMA common → is_dma_mapped flag, alignment requirements                 │
│   • Complex protocols → linked list of transfers, cs_change control         │
│                                                                             │
│   COMMON PATTERNS (shared by both):                                         │
│   ───────────────────────────────────                                       │
│   • Three-way separation: controller / device / driver                      │
│   • Bus type integration (spi_bus_type, i2c_bus_type)                       │
│   • ID table matching for driver binding                                    │
│   • Private data via set/get_drvdata                                        │
│   • Platform device + board files / device tree                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**说明:**
- I²C 的简单性来自其低速半双工特性
- SPI 的复杂性是为了支持高速 DMA 和全双工传输
- 两者都使用三层分离架构（控制器/设备/驱动）

### B.6 Summary Table

| Feature | I²C | SPI |
|---------|-----|-----|
| Bus type | `i2c_bus_type` | `spi_bus_type` |
| Controller | `struct i2c_adapter` | `struct spi_master` |
| Device | `struct i2c_client` | `struct spi_device` |
| Driver | `struct i2c_driver` | `struct spi_driver` |
| Ops table | `struct i2c_algorithm` | Callbacks in `spi_master` |
| Message | `struct i2c_msg[]` | `struct spi_message` + `spi_transfer[]` |
| Sync API | `i2c_transfer()` | `spi_sync()` |
| Async API | None | `spi_async()` |
| Helpers | `i2c_smbus_*()` | `spi_write()`, `spi_read()`, etc. |
| Device addressing | 7/10-bit address | Chip select line |
| Locking | `rt_mutex bus_lock` | `mutex bus_lock_mutex` |

---

## Appendix C — Designing a Clean Bus Subsystem in User-Space

### C.1 Architecture Overview

Applying Linux kernel I²C patterns to user-space design:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│               USER-SPACE BUS FRAMEWORK ARCHITECTURE                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   APPLICATION LAYER                                                         │
│   ─────────────────                                                         │
│   ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐             │
│   │ Temperature App │  │  EEPROM Tool    │  │  Sensor Logger  │             │
│   └────────┬────────┘  └────────┬────────┘  └────────┬────────┘             │
│            │                    │                    │                      │
│            └────────────────────┼────────────────────┘                      │
│                                 │                                           │
│   ──────────────────────────────┼───────────────────────────────────────    │
│                                 │                                           │
│   DEVICE DRIVER LAYER           ▼                                           │
│   ───────────────────  ┌────────────────────────────────────────────────┐   │
│                        │            Bus Manager (Core)                  │   │
│                        │  • Driver registration                         │   │
│                        │  • Device-driver matching                      │   │
│                        │  • Transfer routing                            │   │
│                        └────────────────────────────────────────────────┘   │
│                                 │                                           │
│            ┌────────────────────┼────────────────────┐                      │
│            │                    │                    │                      │
│            ▼                    ▼                    ▼                      │
│   ┌────────────────┐   ┌────────────────┐   ┌────────────────┐              │
│   │  tmp102_driver │   │  eeprom_driver │   │  sensor_driver │              │
│   │  .probe()      │   │  .probe()      │   │  .probe()      │              │
│   │  .remove()     │   │  .remove()     │   │  .remove()     │              │
│   │  .read_temp()  │   │  .read/write() │   │  .read_data()  │              │
│   └────────┬───────┘   └────────┬───────┘   └────────┬───────┘              │
│            │                    │                    │                      │
│   ──────────────────────────────┼───────────────────────────────────────    │
│                                 │                                           │
│   ADAPTER LAYER                 ▼                                           │
│   ─────────────────  ┌────────────────────────────────────────────────┐     │
│                      │            Adapter Registry                     │     │
│                      │  • Bus locking                                  │     │
│                      │  • Transfer dispatch                            │     │
│                      └────────────────────────────────────────────────┘     │
│                                 │                                           │
│            ┌────────────────────┼────────────────────┐                      │
│            │                    │                    │                      │
│            ▼                    ▼                    ▼                      │
│   ┌────────────────┐   ┌────────────────┐   ┌────────────────┐              │
│   │ Linux I2C Dev  │   │  FTDI USB-I2C  │   │  Bit-bang GPIO │              │
│   │ /dev/i2c-N     │   │  libftdi       │   │  /dev/gpiochip │              │
│   └────────────────┘   └────────────────┘   └────────────────┘              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### C.2 Core Data Structures

```c
/* ═══════════════════════════════════════════════════════════════════════════ */
/*                    USER-SPACE BUS FRAMEWORK HEADER                          */
/* ═══════════════════════════════════════════════════════════════════════════ */

#ifndef USERSPACE_BUS_H
#define USERSPACE_BUS_H

#include <pthread.h>
#include <stdint.h>

/* Forward declarations */
struct bus_adapter;
struct bus_device;
struct bus_driver;

/* ─────────────────────────────────────────────────────────────────────────── */
/*                         ADAPTER (Controller)                                 */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
 * struct bus_algorithm - how to perform transfers
 * @transfer: Execute a transfer (may sleep)
 * @functionality: Report what this adapter can do
 *
 * Mirrors kernel's i2c_algorithm concept
 */
struct bus_algorithm {
    int (*transfer)(struct bus_adapter *adap, 
                    struct bus_msg *msgs, int num);
    uint32_t (*functionality)(struct bus_adapter *adap);
};

/**
 * struct bus_adapter - represents a physical bus controller
 * @name: Human-readable name
 * @nr: Bus number (0, 1, 2, ...)
 * @algo: Pointer to algorithm implementation
 * @algo_data: Private data for algorithm
 * @lock: Mutex for exclusive bus access
 * @priv: Adapter-specific private data
 */
struct bus_adapter {
    char name[64];
    int nr;
    
    const struct bus_algorithm *algo;    /* [KEY] How to transfer */
    void *algo_data;
    
    pthread_mutex_t lock;                /* [KEY] Bus serialization */
    
    void *priv;                          /* Driver private data */
    struct bus_adapter *next;            /* Linked list */
};

/* ─────────────────────────────────────────────────────────────────────────── */
/*                           DEVICE (Client)                                    */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
 * struct bus_device - represents a device on the bus
 * @name: Device type name (for driver matching)
 * @addr: Device address on the bus
 * @adapter: Which bus this device is on
 * @driver: Bound driver (NULL until probe succeeds)
 * @driver_data: Private data for driver
 */
struct bus_device {
    char name[32];                       /* [KEY] For matching */
    uint16_t addr;
    
    struct bus_adapter *adapter;         /* [KEY] Link to controller */
    struct bus_driver *driver;           /* [KEY] Link to driver */
    
    void *driver_data;                   /* Driver's private data */
    struct bus_device *next;
};

/* ─────────────────────────────────────────────────────────────────────────── */
/*                           DRIVER                                             */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
 * struct bus_device_id - device identification entry
 * @name: Device name to match
 * @driver_data: Private data passed to probe
 */
struct bus_device_id {
    char name[32];
    unsigned long driver_data;
};

/**
 * struct bus_driver - device driver
 * @name: Driver name
 * @id_table: NULL-terminated list of supported devices
 * @probe: Called when device matches
 * @remove: Called on unbind
 */
struct bus_driver {
    const char *name;
    const struct bus_device_id *id_table;  /* [KEY] For matching */
    
    int (*probe)(struct bus_device *dev, 
                 const struct bus_device_id *id);   /* [KEY] Binding */
    void (*remove)(struct bus_device *dev);
    
    struct bus_driver *next;
};

/* ─────────────────────────────────────────────────────────────────────────── */
/*                           MESSAGE                                            */
/* ─────────────────────────────────────────────────────────────────────────── */

#define BUS_MSG_READ  0x01

struct bus_msg {
    uint16_t addr;      /* Device address */
    uint16_t flags;     /* BUS_MSG_READ, etc. */
    uint16_t len;       /* Buffer length */
    uint8_t *buf;       /* Data buffer */
};

/* ─────────────────────────────────────────────────────────────────────────── */
/*                           API FUNCTIONS                                      */
/* ─────────────────────────────────────────────────────────────────────────── */

/* Adapter management */
int bus_add_adapter(struct bus_adapter *adap);
void bus_del_adapter(struct bus_adapter *adap);

/* Device management */
struct bus_device *bus_new_device(struct bus_adapter *adap,
                                  const char *name, uint16_t addr);
void bus_unregister_device(struct bus_device *dev);

/* Driver management */
int bus_register_driver(struct bus_driver *drv);
void bus_unregister_driver(struct bus_driver *drv);

/* Transfer API */
int bus_transfer(struct bus_adapter *adap, struct bus_msg *msgs, int num);

/* Helpers */
int bus_read_byte(struct bus_device *dev, uint8_t reg);
int bus_write_byte(struct bus_device *dev, uint8_t reg, uint8_t val);

/* Private data accessors */
static inline void bus_set_drvdata(struct bus_device *dev, void *data) {
    dev->driver_data = data;
}
static inline void *bus_get_drvdata(struct bus_device *dev) {
    return dev->driver_data;
}

#endif /* USERSPACE_BUS_H */
```

### C.3 Core Implementation

```c
/* ═══════════════════════════════════════════════════════════════════════════ */
/*                    USER-SPACE BUS FRAMEWORK IMPLEMENTATION                   */
/* ═══════════════════════════════════════════════════════════════════════════ */

#include "bus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Global registries (protected by mutexes) */
static struct bus_adapter *adapter_list = NULL;
static struct bus_driver *driver_list = NULL;
static struct bus_device *device_list = NULL;
static pthread_mutex_t core_lock = PTHREAD_MUTEX_INITIALIZER;

/* ─────────────────────────────────────────────────────────────────────────── */
/*                           MATCHING LOGIC                                     */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
 * [KEY] Match device to driver by name
 * Mirrors kernel's i2c_match_id()
 */
static const struct bus_device_id *
bus_match_id(const struct bus_device_id *id, struct bus_device *dev)
{
    while (id->name[0]) {
        if (strcmp(dev->name, id->name) == 0)
            return id;
        id++;
    }
    return NULL;
}

/**
 * Try to bind a device to a driver
 */
static int bus_try_bind(struct bus_device *dev, struct bus_driver *drv)
{
    const struct bus_device_id *id;
    int ret;
    
    /* Check if driver supports this device */
    id = bus_match_id(drv->id_table, dev);
    if (!id)
        return -1;  /* No match */
    
    /* [KEY] Call driver's probe function */
    ret = drv->probe(dev, id);
    if (ret == 0) {
        /* Binding successful */
        dev->driver = drv;
        printf("[bus] Bound device '%s' at 0x%02x to driver '%s'\n",
               dev->name, dev->addr, drv->name);
    }
    return ret;
}

/**
 * When a new driver is registered, try to match existing devices
 */
static void bus_attach_driver(struct bus_driver *drv)
{
    struct bus_device *dev;
    
    for (dev = device_list; dev; dev = dev->next) {
        if (dev->driver == NULL) {
            bus_try_bind(dev, drv);
        }
    }
}

/**
 * When a new device is registered, try to find a matching driver
 */
static void bus_attach_device(struct bus_device *dev)
{
    struct bus_driver *drv;
    
    for (drv = driver_list; drv; drv = drv->next) {
        if (bus_try_bind(dev, drv) == 0)
            return;  /* Found a driver */
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*                        ADAPTER MANAGEMENT                                    */
/* ─────────────────────────────────────────────────────────────────────────── */

int bus_add_adapter(struct bus_adapter *adap)
{
    /* Initialize lock */
    pthread_mutex_init(&adap->lock, NULL);
    
    /* Add to list */
    pthread_mutex_lock(&core_lock);
    adap->next = adapter_list;
    adapter_list = adap;
    pthread_mutex_unlock(&core_lock);
    
    printf("[bus] Registered adapter '%s' as bus %d\n", adap->name, adap->nr);
    return 0;
}

void bus_del_adapter(struct bus_adapter *adap)
{
    struct bus_device *dev, *next;
    
    pthread_mutex_lock(&core_lock);
    
    /* [KEY] Unregister all devices on this adapter */
    for (dev = device_list; dev; dev = next) {
        next = dev->next;
        if (dev->adapter == adap) {
            bus_unregister_device(dev);
        }
    }
    
    /* Remove from list */
    /* ... (list removal logic) ... */
    
    pthread_mutex_unlock(&core_lock);
    pthread_mutex_destroy(&adap->lock);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*                         DEVICE MANAGEMENT                                    */
/* ─────────────────────────────────────────────────────────────────────────── */

struct bus_device *bus_new_device(struct bus_adapter *adap,
                                  const char *name, uint16_t addr)
{
    struct bus_device *dev;
    
    dev = calloc(1, sizeof(*dev));
    if (!dev)
        return NULL;
    
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->addr = addr;
    dev->adapter = adap;
    dev->driver = NULL;
    
    /* Add to global list */
    pthread_mutex_lock(&core_lock);
    dev->next = device_list;
    device_list = dev;
    pthread_mutex_unlock(&core_lock);
    
    printf("[bus] Created device '%s' at 0x%02x on bus %d\n",
           name, addr, adap->nr);
    
    /* [KEY] Try to find a matching driver */
    bus_attach_device(dev);
    
    return dev;
}

void bus_unregister_device(struct bus_device *dev)
{
    if (dev->driver && dev->driver->remove) {
        dev->driver->remove(dev);
    }
    /* ... (list removal, free) ... */
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*                         DRIVER MANAGEMENT                                    */
/* ─────────────────────────────────────────────────────────────────────────── */

int bus_register_driver(struct bus_driver *drv)
{
    pthread_mutex_lock(&core_lock);
    drv->next = driver_list;
    driver_list = drv;
    pthread_mutex_unlock(&core_lock);
    
    printf("[bus] Registered driver '%s'\n", drv->name);
    
    /* [KEY] Try to attach to existing devices */
    bus_attach_driver(drv);
    
    return 0;
}

void bus_unregister_driver(struct bus_driver *drv)
{
    struct bus_device *dev;
    
    pthread_mutex_lock(&core_lock);
    
    /* Unbind from all devices */
    for (dev = device_list; dev; dev = dev->next) {
        if (dev->driver == drv) {
            if (drv->remove)
                drv->remove(dev);
            dev->driver = NULL;
        }
    }
    
    /* Remove from list */
    /* ... */
    
    pthread_mutex_unlock(&core_lock);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*                            TRANSFER API                                      */
/* ─────────────────────────────────────────────────────────────────────────── */

/**
 * [KEY] Locked transfer - mirrors kernel's i2c_transfer()
 */
int bus_transfer(struct bus_adapter *adap, struct bus_msg *msgs, int num)
{
    int ret;
    
    if (!adap->algo || !adap->algo->transfer)
        return -EOPNOTSUPP;
    
    /* [KEY] Serialize bus access */
    pthread_mutex_lock(&adap->lock);
    ret = adap->algo->transfer(adap, msgs, num);
    pthread_mutex_unlock(&adap->lock);
    
    return ret;
}

/* Helper functions */
int bus_read_byte(struct bus_device *dev, uint8_t reg)
{
    uint8_t val;
    struct bus_msg msgs[2] = {
        { .addr = dev->addr, .flags = 0,            .len = 1, .buf = &reg },
        { .addr = dev->addr, .flags = BUS_MSG_READ, .len = 1, .buf = &val },
    };
    
    if (bus_transfer(dev->adapter, msgs, 2) < 0)
        return -1;
    
    return val;
}

int bus_write_byte(struct bus_device *dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    struct bus_msg msg = {
        .addr = dev->addr, .flags = 0, .len = 2, .buf = buf
    };
    
    return bus_transfer(dev->adapter, &msg, 1);
}
```

### C.4 Example: Linux I2C Backend Adapter

```c
/* ═══════════════════════════════════════════════════════════════════════════ */
/*               ADAPTER BACKEND: Linux /dev/i2c-N                             */
/* ═══════════════════════════════════════════════════════════════════════════ */

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

struct linux_i2c_data {
    int fd;            /* File descriptor for /dev/i2c-N */
};

static int linux_i2c_transfer(struct bus_adapter *adap, 
                              struct bus_msg *msgs, int num)
{
    struct linux_i2c_data *priv = adap->algo_data;
    struct i2c_rdwr_ioctl_data data;
    struct i2c_msg *kernel_msgs;
    int i, ret;
    
    /* Convert our messages to kernel format */
    kernel_msgs = malloc(num * sizeof(struct i2c_msg));
    for (i = 0; i < num; i++) {
        kernel_msgs[i].addr = msgs[i].addr;
        kernel_msgs[i].flags = (msgs[i].flags & BUS_MSG_READ) ? I2C_M_RD : 0;
        kernel_msgs[i].len = msgs[i].len;
        kernel_msgs[i].buf = msgs[i].buf;
    }
    
    data.msgs = kernel_msgs;
    data.nmsgs = num;
    
    ret = ioctl(priv->fd, I2C_RDWR, &data);
    
    free(kernel_msgs);
    return (ret < 0) ? -errno : num;
}

static uint32_t linux_i2c_functionality(struct bus_adapter *adap)
{
    struct linux_i2c_data *priv = adap->algo_data;
    unsigned long funcs;
    
    if (ioctl(priv->fd, I2C_FUNCS, &funcs) < 0)
        return 0;
    
    return (uint32_t)funcs;
}

static const struct bus_algorithm linux_i2c_algo = {
    .transfer      = linux_i2c_transfer,
    .functionality = linux_i2c_functionality,
};

/**
 * Create a Linux I²C adapter backend
 */
struct bus_adapter *linux_i2c_adapter_create(int bus_nr)
{
    struct bus_adapter *adap;
    struct linux_i2c_data *priv;
    char path[32];
    
    adap = calloc(1, sizeof(*adap));
    priv = calloc(1, sizeof(*priv));
    
    snprintf(path, sizeof(path), "/dev/i2c-%d", bus_nr);
    priv->fd = open(path, O_RDWR);
    if (priv->fd < 0) {
        free(adap);
        free(priv);
        return NULL;
    }
    
    snprintf(adap->name, sizeof(adap->name), "Linux I2C Bus %d", bus_nr);
    adap->nr = bus_nr;
    adap->algo = &linux_i2c_algo;
    adap->algo_data = priv;
    
    return adap;
}
```

### C.5 Example: Device Driver

```c
/* ═══════════════════════════════════════════════════════════════════════════ */
/*                    EXAMPLE: TMP102 TEMPERATURE SENSOR DRIVER                */
/* ═══════════════════════════════════════════════════════════════════════════ */

#include "bus.h"

#define TMP102_TEMP_REG    0x00
#define TMP102_CONF_REG    0x01

struct tmp102_data {
    int last_temp_mc;  /* millicelsius */
};

static int tmp102_probe(struct bus_device *dev, const struct bus_device_id *id)
{
    struct tmp102_data *data;
    int conf;
    
    printf("[tmp102] Probing device at 0x%02x\n", dev->addr);
    
    /* Verify chip is present by reading config */
    conf = bus_read_byte(dev, TMP102_CONF_REG);
    if (conf < 0) {
        printf("[tmp102] Failed to read config\n");
        return -1;
    }
    
    /* Allocate private data */
    data = calloc(1, sizeof(*data));
    if (!data)
        return -ENOMEM;
    
    /* [KEY] Store private data */
    bus_set_drvdata(dev, data);
    
    printf("[tmp102] Device initialized (config=0x%02x)\n", conf);
    return 0;
}

static void tmp102_remove(struct bus_device *dev)
{
    struct tmp102_data *data = bus_get_drvdata(dev);
    printf("[tmp102] Removing device\n");
    free(data);
}

/* Public API */
int tmp102_read_temperature(struct bus_device *dev)
{
    struct tmp102_data *data = bus_get_drvdata(dev);
    int msb, lsb, raw;
    
    msb = bus_read_byte(dev, TMP102_TEMP_REG);
    lsb = bus_read_byte(dev, TMP102_TEMP_REG + 1);  /* Auto-increment */
    
    if (msb < 0 || lsb < 0)
        return -1;
    
    raw = (msb << 4) | (lsb >> 4);
    data->last_temp_mc = raw * 625 / 10;  /* 0.0625°C per LSB */
    
    return data->last_temp_mc;
}

/* Device ID table */
static const struct bus_device_id tmp102_ids[] = {
    { "tmp102", 0 },
    { "tmp112", 1 },  /* Compatible variant */
    { }               /* Terminator */
};

/* Driver structure */
static struct bus_driver tmp102_driver = {
    .name     = "tmp102",
    .id_table = tmp102_ids,
    .probe    = tmp102_probe,
    .remove   = tmp102_remove,
};

/* Convenience registration */
void tmp102_driver_register(void)
{
    bus_register_driver(&tmp102_driver);
}
```

### C.6 Complete Example: Putting It Together

```c
/* ═══════════════════════════════════════════════════════════════════════════ */
/*                         COMPLETE APPLICATION EXAMPLE                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

#include "bus.h"

/* External driver registration */
extern void tmp102_driver_register(void);
extern int tmp102_read_temperature(struct bus_device *dev);

int main(void)
{
    struct bus_adapter *adap;
    struct bus_device *dev;
    
    printf("=== User-Space Bus Framework Demo ===\n\n");
    
    /* [STEP 1] Register driver first (like module_init) */
    tmp102_driver_register();
    
    /* [STEP 2] Create adapter backend */
    adap = linux_i2c_adapter_create(1);  /* /dev/i2c-1 */
    if (!adap) {
        fprintf(stderr, "Failed to open I2C bus\n");
        return 1;
    }
    bus_add_adapter(adap);
    
    /* [STEP 3] Declare device (like board file) */
    /* This will auto-match with tmp102_driver and call probe() */
    dev = bus_new_device(adap, "tmp102", 0x48);
    if (!dev) {
        fprintf(stderr, "Failed to create device\n");
        return 1;
    }
    
    /* [STEP 4] Use driver API */
    if (dev->driver) {
        int temp_mc = tmp102_read_temperature(dev);
        printf("Temperature: %d.%03d °C\n", 
               temp_mc / 1000, abs(temp_mc % 1000));
    } else {
        fprintf(stderr, "No driver bound!\n");
    }
    
    /* [STEP 5] Cleanup */
    bus_unregister_device(dev);
    bus_del_adapter(adap);
    
    return 0;
}

/*
 * Expected output:
 *
 *   === User-Space Bus Framework Demo ===
 *
 *   [bus] Registered driver 'tmp102'
 *   [bus] Registered adapter 'Linux I2C Bus 1' as bus 1
 *   [bus] Created device 'tmp102' at 0x48 on bus 1
 *   [tmp102] Probing device at 0x48
 *   [tmp102] Device initialized (config=0x60)
 *   [bus] Bound device 'tmp102' at 0x48 to driver 'tmp102'
 *   Temperature: 25.250 °C
 *   [tmp102] Removing device
 */
```

**说明:**
- 用户空间框架完全模仿内核 I²C 子系统的三层架构
- 使用互斥锁替代内核的 rt_mutex
- 通过 `/dev/i2c-N` 与内核 I²C 驱动通信
- 驱动匹配、探测、私有数据存储模式与内核一致

---

## Appendix D — Device Tree Integration

### D.1 From Board Files to Device Tree

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              EVOLUTION: BOARD FILES → DEVICE TREE                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   TRADITIONAL (v3.2 and earlier):                                           │
│   ───────────────────────────────                                           │
│                                                                             │
│   arch/arm/mach-xxx/board-yyy.c:                                            │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ static struct i2c_board_info my_devices[] __initdata = {            │   │
│   │     { I2C_BOARD_INFO("tmp102", 0x48), .irq = IRQ_TEMP },            │   │
│   │     { I2C_BOARD_INFO("24c02", 0x50) },                               │   │
│   │ };                                                                   │   │
│   │                                                                      │   │
│   │ static void __init my_board_init(void) {                            │   │
│   │     i2c_register_board_info(0, my_devices, ARRAY_SIZE(my_devices)); │   │
│   │ }                                                                    │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│   PROBLEMS:                                                                 │
│   • Every board needs C code changes → recompile kernel                    │
│   • Device info scattered across hundreds of board files                   │
│   • Impossible to support many boards with one kernel binary              │
│                                                                             │
│   ─────────────────────────────────────────────────────────────────────    │
│                                                                             │
│   MODERN (v3.7+ dominant):                                                  │
│   ────────────────────────                                                  │
│                                                                             │
│   arch/arm/boot/dts/xxx-board.dts:                                          │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ &i2c0 {                                                              │   │
│   │     status = "okay";                                                 │   │
│   │     clock-frequency = <400000>;                                      │   │
│   │                                                                      │   │
│   │     tmp102@48 {                                                      │   │
│   │         compatible = "ti,tmp102";                                    │   │
│   │         reg = <0x48>;                                                │   │
│   │         interrupt-parent = <&gpio>;                                  │   │
│   │         interrupts = <7 IRQ_TYPE_LEVEL_LOW>;                         │   │
│   │     };                                                               │   │
│   │                                                                      │   │
│   │     eeprom@50 {                                                      │   │
│   │         compatible = "atmel,24c02";                                  │   │
│   │         reg = <0x50>;                                                │   │
│   │         pagesize = <8>;                                              │   │
│   │     };                                                               │   │
│   │ };                                                                   │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│   BENEFITS:                                                                 │
│   • Hardware description separate from kernel code                         │
│   • One kernel binary supports many boards (just swap DTB)                 │
│   • Declarative format, easier to maintain                                 │
│   • Runtime device instantiation                                           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### D.2 How Device Tree I²C Binding Works

```c
/* drivers/of/of_i2c.c (v3.2) */

/**
 * of_i2c_register_devices - Create i2c_clients from device tree
 * @adap: The I²C adapter
 *
 * Called by adapter drivers after registration.
 * Scans DT children and creates i2c_client for each.
 */
void of_i2c_register_devices(struct i2c_adapter *adap)
{
    struct device_node *node;

    /* [KEY] Only if adapter has DT node */
    if (!adap->dev.of_node)
        return;

    /* [KEY] Walk all child nodes of this adapter */
    for_each_child_of_node(adap->dev.of_node, node) {
        struct i2c_board_info info = {};
        const __be32 *addr;

        /* Get device name from "compatible" property */
        if (of_modalias_node(node, info.type, sizeof(info.type)) < 0) {
            dev_err(...);
            continue;
        }

        /* [KEY] Get address from "reg" property */
        addr = of_get_property(node, "reg", &len);
        if (!addr) {
            dev_err(...);
            continue;
        }
        info.addr = be32_to_cpup(addr);

        /* Get IRQ if specified */
        info.irq = irq_of_parse_and_map(node, 0);
        
        /* Store DT node reference */
        info.of_node = of_node_get(node);

        /* [KEY] Create the i2c_client (same as board file path!) */
        i2c_new_device(adap, &info);
    }
}
```

**说明:**
- DT 节点的 `compatible` 属性变成 `i2c_client.name`
- `reg` 属性变成 `i2c_client.addr`
- 最终调用的还是 `i2c_new_device()`，与 board file 路径相同
- 驱动匹配逻辑不变（比对名称）

### D.3 Device Tree Binding Anatomy

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              DEVICE TREE I²C BINDING FORMAT                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   /* I²C controller node */                                                 │
│   i2c0: i2c@44e0b000 {                                                      │
│       compatible = "ti,omap4-i2c";        ← [1] Controller driver match    │
│       reg = <0x44e0b000 0x1000>;          ← [2] Register address/size      │
│       interrupts = <70>;                  ← [3] Controller IRQ             │
│       #address-cells = <1>;               ← [4] Child addr is 1 cell       │
│       #size-cells = <0>;                  ← [5] Children have no size      │
│       clock-frequency = <400000>;         ← [6] Bus speed (Hz)             │
│       status = "okay";                    ← [7] Enable this controller     │
│                                                                             │
│       /* Child device nodes */                                              │
│                                                                             │
│       tmp102@48 {                         ← Node name (for humans)          │
│           compatible = "ti,tmp102";       ← [A] CRITICAL: driver matching  │
│           reg = <0x48>;                   ← [B] I²C address                 │
│           #thermal-sensor-cells = <1>;    ← Device-specific binding        │
│       };                                                                    │
│                                                                             │
│       eeprom@50 {                                                           │
│           compatible = "atmel,24c02";     ← [A] Different device type      │
│           reg = <0x50>;                   ← [B] Different address          │
│           pagesize = <8>;                 ← Device-specific property       │
│           read-only;                      ← Boolean property               │
│       };                                                                    │
│                                                                             │
│       /* Multi-address device example */                                    │
│       codec@1a {                                                            │
│           compatible = "cirrus,cs42l51";                                    │
│           reg = <0x1a>;                   ← Primary address                │
│           clocks = <&clk_ext>;                                              │
│       };                                                                    │
│   };                                                                        │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   KEY PROPERTIES:                                                           │
│   ───────────────                                                           │
│   compatible  → Maps to driver's OF match table (preferred) or id_table    │
│   reg         → I²C slave address (7-bit)                                   │
│   interrupts  → Optional: device interrupt                                  │
│   status      → "okay" (enabled) or "disabled"                             │
│                                                                             │
│   The "compatible" property uses vendor,device format:                      │
│   • "ti,tmp102" → Texas Instruments TMP102                                  │
│   • "atmel,24c02" → Atmel 24C02 EEPROM                                      │
│   • Multiple strings allowed for fallback: "ti,tmp112", "ti,tmp102"        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### D.4 Driver Support for Device Tree

```c
/* Modern driver with both ID table and OF table */

/* Traditional I²C ID table (still needed for non-DT platforms) */
static const struct i2c_device_id tmp102_id[] = {
    { "tmp102", 0 },
    { "tmp112", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, tmp102_id);

/* [KEY] Device Tree match table */
static const struct of_device_id tmp102_of_match[] = {
    { .compatible = "ti,tmp102" },
    { .compatible = "ti,tmp112" },
    { }
};
MODULE_DEVICE_TABLE(of, tmp102_of_match);

static struct i2c_driver tmp102_driver = {
    .driver = {
        .name           = "tmp102",
        .owner          = THIS_MODULE,
        .of_match_table = of_match_ptr(tmp102_of_match),  /* [KEY] DT */
    },
    .probe    = tmp102_probe,
    .remove   = tmp102_remove,
    .id_table = tmp102_id,  /* For non-DT platforms */
};

/* In probe, get DT properties */
static int tmp102_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
    struct device_node *np = client->dev.of_node;
    u32 val;
    
    /* Example: read device-specific DT property */
    if (np) {
        if (of_property_read_u32(np, "ti,conversion-rate", &val) == 0) {
            /* Use val... */
        }
        
        if (of_property_read_bool(np, "ti,extended-mode")) {
            /* Enable extended mode... */
        }
    }
    
    /* ... rest of probe ... */
}
```

### D.5 Matching Priority

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    DRIVER MATCHING PRIORITY                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   When i2c_device_match() is called:                                        │
│                                                                             │
│   [1] OF (Device Tree) matching       ← Checked FIRST                      │
│       ─────────────────────────                                             │
│       if (of_driver_match_device(dev, drv))                                 │
│           return 1;                                                         │
│                                                                             │
│       Compares client->dev.of_node's "compatible" property                  │
│       against driver->driver.of_match_table                                 │
│                                                                             │
│   [2] ID table matching               ← Fallback                           │
│       ────────────────────                                                  │
│       if (driver->id_table)                                                 │
│           return i2c_match_id(driver->id_table, client) != NULL;           │
│                                                                             │
│       Compares client->name against id_table[].name                         │
│       Used when: no DT, or DT but no of_match_table in driver              │
│                                                                             │
│   [3] ACPI matching (not shown)       ← For x86 platforms                  │
│                                                                             │
│   PRACTICAL IMPLICATION:                                                    │
│   ──────────────────────                                                    │
│   • DT platforms: "compatible" property is king                             │
│   • Non-DT: i2c_board_info.type → client->name → id_table match            │
│   • Drivers should provide BOTH tables for maximum compatibility           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### D.6 Complete Device Tree Example

```dts
/* arch/arm/boot/dts/my-board.dts */

/dts-v1/;
#include "soc-base.dtsi"

/ {
    model = "My Custom Board";
    compatible = "myvendor,myboard", "ti,am335x";

    /* Override I²C controller from base */
    &i2c0 {
        pinctrl-names = "default";
        pinctrl-0 = <&i2c0_pins>;
        status = "okay";
        clock-frequency = <400000>;

        /* Temperature sensor */
        tmp102: temperature-sensor@48 {
            compatible = "ti,tmp102";
            reg = <0x48>;
            interrupt-parent = <&gpio1>;
            interrupts = <7 IRQ_TYPE_LEVEL_LOW>;
            #thermal-sensor-cells = <1>;
        };

        /* EEPROM for board ID */
        eeprom@50 {
            compatible = "atmel,24c256";
            reg = <0x50>;
            pagesize = <64>;
        };

        /* Real-time clock */
        rtc@68 {
            compatible = "dallas,ds1307";
            reg = <0x68>;
        };

        /* Power management IC */
        pmic@24 {
            compatible = "ti,tps65217";
            reg = <0x24>;
            
            regulators {
                dcdc1: dcdc1 {
                    regulator-name = "vdd_mpu";
                    regulator-min-microvolt = <925000>;
                    regulator-max-microvolt = <1325000>;
                    regulator-boot-on;
                    regulator-always-on;
                };
                /* ... more regulators ... */
            };
        };
    };

    /* Thermal zone using tmp102 */
    thermal-zones {
        board-thermal {
            polling-delay = <1000>;
            polling-delay-passive = <250>;
            thermal-sensors = <&tmp102 0>;
            
            trips {
                board_alert: board-alert {
                    temperature = <70000>; /* millicelsius */
                    hysteresis = <2000>;
                    type = "passive";
                };
            };
        };
    };
};
```

### D.7 v3.2 vs Modern Kernel Comparison

| Aspect | v3.2 (Early DT) | Modern (v5.x+) |
|--------|-----------------|----------------|
| DT support | Optional, parallel to board files | Mandatory for ARM/ARM64 |
| Matching | `of_match_table` separate | Unified matching framework |
| Properties | `of_property_read_*()` | `device_property_read_*()` |
| Regmap | Manual register access | DT-driven regmap config |
| GPIO | `of_get_gpio()` | GPIO descriptors (`gpiod_*`) |
| Clocks | Platform-specific | Common clock framework |
| Pinmux | Scattered | Unified pinctrl subsystem |

**说明:**
- v3.2 是 Device Tree 过渡期，许多驱动仍需同时支持 board file
- 现代内核中，ARM 平台几乎完全依赖 Device Tree
- 属性访问从 OF 特定 API 迁移到设备无关 API（`device_property_*`）
- 驱动应同时提供 `of_match_table` 和 `id_table` 以保持兼容性

---

*This document analyzes the I²C subsystem from Linux kernel v3.2. Later kernels added features like Device Tree integration, regmap, and runtime PM, but the core architectural patterns remain the same.*

