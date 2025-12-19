# Linux Kernel UART/TTY Subsystem Architecture (v3.2)

## Table of Contents

1. [Phase 1 — UART & TTY Subsystem Overview](#phase-1--uart--tty-subsystem-overview)
2. [Phase 2 — Layering: TTY ↔ Serial Core ↔ UART Driver](#phase-2--layering-tty--serial-core--uart-driver)
3. [Phase 3 — struct uart_driver (Serial Core Registration)](#phase-3--struct-uart_driver-serial-core-registration)
4. [Phase 4 — struct uart_port (Per-Port State)](#phase-4--struct-uart_port-per-port-state)
5. [Phase 5 — struct uart_ops (Hardware Abstraction)](#phase-5--struct-uart_ops-hardware-abstraction)
6. [Phase 6 — Driver Registration & Port Lifecycle](#phase-6--driver-registration--port-lifecycle)
7. [Phase 7 — Data Path: TX & RX Flow](#phase-7--data-path-tx--rx-flow)
8. [Phase 8 — Interrupt Handling & Concurrency](#phase-8--interrupt-handling--concurrency)
9. [Phase 9 — Power Management & Console Support](#phase-9--power-management--console-support)
10. [Phase 10 — Common Bugs & Architecture Lessons](#phase-10--common-bugs--architecture-lessons)
11. [Appendix A — Walking Through a Real UART Driver (PL011)](#appendix-a--walking-through-a-real-uart-driver-pl011)
12. [Appendix B — Comparing UART vs SPI vs I²C Architectures](#appendix-b--comparing-uart-vs-spi-vs-ic-architectures)
13. [Appendix C — Writing a Minimal UART Driver Safely](#appendix-c--writing-a-minimal-uart-driver-safely)
14. [Appendix D — Why TTY Is Structured the Way It Is](#appendix-d--why-tty-is-structured-the-way-it-is)

---

## Phase 1 — UART & TTY Subsystem Overview

### 1.1 The Role of UART in Linux

UART (Universal Asynchronous Receiver/Transmitter) is the fundamental hardware interface for serial communication. In Linux, UARTs are abstracted through multiple layers:

| Role | Description |
|------|-------------|
| **Hardware Interface** | Physical serial ports, RS-232, console ports |
| **System Console** | Boot messages, kernel debugging, emergency output |
| **Device Communication** | GPS, Bluetooth, modems, sensors |
| **User Terminal** | Login shells, serial terminals |

### 1.2 How UART Fits into the TTY Subsystem

```
+---------------------------------------------------------------------+
|                      USER SPACE                                      |
|  +----------+  +----------+  +----------+  +----------+             |
|  | Terminal |  | minicom  |  | stty     |  | screen   |             |
|  +----+-----+  +----+-----+  +----+-----+  +----+-----+             |
|       |             |             |             |                    |
|       +------+------+------+------+             |                    |
|              |                                  |                    |
+==============|==================================|====================+
               | read()/write()/ioctl()           |
               v                                  v
+---------------------------------------------------------------------+
|                      TTY LAYER                                       |
|  +------------------+                                                |
|  | Line Discipline  |  <-- N_TTY (cooked mode), N_SLIP, N_PPP, etc. |
|  |  (n_tty.c)       |      Processes input/output (echo, editing)   |
|  +--------+---------+                                                |
|           |                                                          |
|           v                                                          |
|  +------------------+                                                |
|  | TTY Core         |  <-- tty_io.c                                 |
|  | (tty_struct)     |      Manages TTY devices, handles open/close  |
|  +--------+---------+                                                |
|           |                                                          |
+===========|==========================================================+
            | tty_operations callbacks
            v
+---------------------------------------------------------------------+
|                    SERIAL CORE                                       |
|  +------------------+                                                |
|  | serial_core.c    |  <-- Implements tty_operations for serial     |
|  | (uart_driver,    |      Manages uart_port instances              |
|  |  uart_state)     |      Provides uniform API for UART drivers    |
|  +--------+---------+                                                |
|           |                                                          |
+===========|==========================================================+
            | uart_ops callbacks
            v
+---------------------------------------------------------------------+
|                    UART DRIVER                                       |
|  +------------------+                                                |
|  | amba-pl011.c     |  <-- Hardware-specific driver                 |
|  | 8250.c           |      Implements uart_ops                      |
|  | (uart_port)      |      Accesses hardware registers              |
|  +--------+---------+                                                |
|           |                                                          |
+===========|==========================================================+
            | register read/write
            v
+---------------------------------------------------------------------+
|                    HARDWARE                                          |
|  [ UART Controller ]  <-- TX/RX FIFOs, Control/Status registers     |
|  [ Physical Pins    ]  <-- TX, RX, RTS, CTS, DTR, DSR, etc.         |
+---------------------------------------------------------------------+
```

**中文解释：**
- **用户空间**：终端应用程序通过标准系统调用(`read`/`write`/`ioctl`)访问串口
- **TTY层**：包括线路规程(处理回显、编辑等)和TTY核心(管理设备)
- **串口核心**：为UART驱动提供统一API，实现TTY操作
- **UART驱动**：硬件特定的驱动程序，直接访问硬件寄存器
- **硬件**：实际的UART控制器和物理引脚

### 1.3 Why Linux Separates Line Discipline, TTY Core, Serial Core, and Hardware Drivers

| Layer | Responsibility | Why Separated? |
|-------|---------------|----------------|
| **Line Discipline** | Character processing (echo, line editing, special chars) | Allows different protocols (TTY, SLIP, PPP, Bluetooth HCI) over same hardware |
| **TTY Core** | Device management, open/close/ioctl, driver model integration | Uniform interface for all terminal-like devices (console, pty, serial) |
| **Serial Core** | UART-specific abstraction | Handles common serial logic (baud rate, flow control) so drivers only implement hardware access |
| **Hardware Driver** | Register-level hardware access | Isolates hardware-specific code; enables portability |

**Key Insight:** Each layer provides abstraction that reduces code duplication and increases flexibility.

### 1.4 Where Relevant Code Lives in v3.2

```
linux/
├── drivers/tty/
│   ├── tty_io.c              # [CORE] TTY core implementation
│   ├── tty_ioctl.c           # [CORE] TTY ioctl handling
│   ├── tty_ldisc.c           # [CORE] Line discipline management
│   ├── tty_buffer.c          # [CORE] TTY flip buffer implementation
│   ├── tty_port.c            # [CORE] tty_port helpers
│   ├── n_tty.c               # [LDISC] Default line discipline
│   ├── n_hdlc.c              # [LDISC] HDLC line discipline
│   ├── n_ppp.c               # [LDISC] PPP line discipline
│   │
│   └── serial/
│       ├── serial_core.c     # [SERIAL] Serial core implementation
│       ├── 8250.c            # [DRIVER] 8250/16550 UART driver
│       ├── amba-pl011.c      # [DRIVER] ARM PL011 UART driver
│       ├── imx.c             # [DRIVER] i.MX UART driver
│       ├── samsung.c         # [DRIVER] Samsung UART driver
│       └── ... (80+ drivers)
│
├── include/linux/
│   ├── tty.h                 # TTY structure definitions
│   ├── tty_driver.h          # tty_driver and tty_operations
│   ├── tty_ldisc.h           # Line discipline interface
│   ├── tty_flip.h            # Flip buffer API
│   └── serial_core.h         # [KEY] uart_driver, uart_port, uart_ops
```

**File Categories:**
- `[CORE]`: TTY subsystem core
- `[LDISC]`: Line discipline implementations
- `[SERIAL]`: Serial core (UART abstraction)
- `[DRIVER]`: Hardware-specific UART drivers

---

## Phase 2 — Layering: TTY ↔ Serial Core ↔ UART Driver

### 2.1 Complete Layering from User Space to Hardware

```
+------------------------------------------------------------------------+
|                    COMPLETE CALL CHAIN                                  |
+------------------------------------------------------------------------+

USER SPACE
    |
    | write(fd, buf, count)
    v
+-------------------+
| VFS               |
| sys_write()       |
+-------------------+
    |
    | file->f_op->write()
    v
+-------------------+
| TTY Core          |  <-- tty_write()
| (tty_io.c)        |      - Calls line discipline's write
+-------------------+
    |
    | ld->ops->write()
    v
+-------------------+
| Line Discipline   |  <-- n_tty_write()
| (n_tty.c)         |      - Output processing (OPOST)
|                   |      - Calls tty->ops->write()
+-------------------+
    |
    | tty->ops->write()
    v
+-------------------+
| TTY Operations    |  <-- uart_write() [serial_core.c]
| (serial_core.c)   |      - Copies data to circular buffer
|                   |      - Calls uart_start()
+-------------------+
    |
    | port->ops->start_tx()
    v
+-------------------+
| UART Ops          |  <-- pl011_start_tx() [amba-pl011.c]
| (driver)          |      - Enables TX interrupt
|                   |      - Hardware starts transmission
+-------------------+
    |
    | writew(data, UART_DR)
    v
+-------------------+
| HARDWARE          |  <-- TX FIFO → Physical TX pin
+-------------------+
```

### 2.2 Control Flow vs Data Flow

```
+------------------------------------------------------------------------+
|                    FLOW DIRECTIONS                                      |
+------------------------------------------------------------------------+

                CONTROL FLOW                    DATA FLOW
                (Configuration)                 (Actual bytes)
                
User Space      stty, ioctl()                   write() → TX
    |               |                               |
    v               v                               v
TTY Core        termios setup                   tty_write()
    |               |                               |
    v               v                               v
Line Disc       Process termios                 Process output
    |               |                               |
    v               v                               v
Serial Core     uart_set_termios()              uart_write()
    |               |                               |
    v               v                               v
UART Driver     ops->set_termios()              ops->start_tx()
    |               |                               |
    v               v                               v
Hardware        Configure baud/parity           Write to TX FIFO


                                                RX → read()
                                                    ^
                                                    |
                                                ldisc->receive_buf()
                                                    ^
                                                    |
                                                tty_flip_buffer_push()
                                                    ^
                                                    |
                                                ISR: uart_insert_char()
                                                    ^
                                                    |
                                                Hardware RX FIFO
```

**中文解释：**
- **控制流**：配置操作从用户空间向下传递（如波特率、校验位设置）
- **数据流-TX**：用户数据从用户空间写入，经过各层处理后到达硬件发送
- **数据流-RX**：硬件接收数据，通过中断上传到用户空间读取

### 2.3 Context and Sleeping Rules

| Layer | May Sleep? | Runs in IRQ Context? | Notes |
|-------|------------|---------------------|-------|
| User Space | Yes | No | Normal process context |
| TTY Core (open/close) | Yes | No | Process context |
| TTY Core (write) | Depends | No | May block if buffer full |
| Line Discipline | Yes | Sometimes | `receive_buf` may be in softirq |
| Serial Core | Depends | Depends | `uart_write`: No sleep; `set_termios`: May sleep |
| UART Driver (most ops) | **NO** | Often | `start_tx`, `stop_tx`, `get_mctrl` called with spinlock |
| UART Driver (startup/shutdown) | Yes | No | May sleep for resource allocation |

**Critical Contract:**
- `uart_ops->start_tx()`, `stop_tx()`, `stop_rx()`, `get_mctrl()`, `set_mctrl()`: Called with port lock held, **MUST NOT SLEEP**
- `uart_ops->startup()`, `shutdown()`, `set_termios()`: May sleep

---

## Phase 3 — struct uart_driver (Serial Core Registration)

### 3.1 What uart_driver Represents

`struct uart_driver` represents a **serial driver family**—a collection of UART ports of the same type. It's the connection point between the serial core and the TTY layer.

### 3.2 Structure Definition

```c
/* include/linux/serial_core.h */
struct uart_driver {
    struct module       *owner;         /* [KEY] THIS_MODULE */
    const char          *driver_name;   /* [KEY] Internal driver name (e.g., "ttyAMA") */
    const char          *dev_name;      /* [KEY] Device name prefix (e.g., "ttyAMA") */
    int                  major;         /* [KEY] Major device number */
    int                  minor;         /* [KEY] Starting minor number */
    int                  nr;            /* [KEY] Maximum number of ports */
    struct console      *cons;          /* [OPT] Console associated with this driver */

    /* Private - managed by serial core */
    struct uart_state   *state;         /* Per-port state array */
    struct tty_driver   *tty_driver;    /* TTY driver allocated by serial core */
};
```

**Field Purposes:**

| Field | Purpose | Example |
|-------|---------|---------|
| `driver_name` | Identifies driver in /proc/tty/driver | "ttyAMA" |
| `dev_name` | Device node name in /dev | "ttyAMA" → /dev/ttyAMA0 |
| `major` | Major device number | 204 |
| `minor` | Starting minor number | 64 → ttyAMA0=64, ttyAMA1=65 |
| `nr` | How many ports this driver supports | 14 (UART_NR) |
| `cons` | Console structure for boot console | &amba_console |

### 3.3 Example: PL011 uart_driver

```c
/* drivers/tty/serial/amba-pl011.c */
static struct uart_driver amba_reg = {
    .owner          = THIS_MODULE,
    .driver_name    = "ttyAMA",           /* [KEY] Internal name */
    .dev_name       = "ttyAMA",           /* [KEY] Creates /dev/ttyAMA0, ttyAMA1, ... */
    .major          = SERIAL_AMBA_MAJOR,  /* 204 */
    .minor          = SERIAL_AMBA_MINOR,  /* 64 */
    .nr             = UART_NR,            /* 14 ports max */
    .cons           = AMBA_CONSOLE,       /* Boot console */
};
```

### 3.4 How uart_driver Is Registered

```c
/* drivers/tty/serial/serial_core.c */
int uart_register_driver(struct uart_driver *drv)
{
    struct tty_driver *normal;
    int i, retval;

    BUG_ON(drv->state);  /* [CHECK] Must not be registered already */

    /* [STEP 1] Allocate per-port state array */
    drv->state = kzalloc(sizeof(struct uart_state) * drv->nr, GFP_KERNEL);
    if (!drv->state)
        goto out;

    /* [STEP 2] Allocate TTY driver */
    normal = alloc_tty_driver(drv->nr);
    if (!normal)
        goto out_kfree;

    drv->tty_driver = normal;

    /* [STEP 3] Configure TTY driver */
    normal->owner       = drv->owner;
    normal->driver_name = drv->driver_name;
    normal->name        = drv->dev_name;       /* Device name */
    normal->major       = drv->major;
    normal->minor_start = drv->minor;
    normal->type        = TTY_DRIVER_TYPE_SERIAL;
    normal->subtype     = SERIAL_TYPE_NORMAL;
    normal->init_termios    = tty_std_termios;
    normal->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    normal->flags       = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
    normal->driver_state = drv;
    
    /* [KEY] Set serial core's tty_operations */
    tty_set_operations(normal, &uart_ops);

    /* [STEP 4] Initialize per-port tty_port */
    for (i = 0; i < drv->nr; i++) {
        struct uart_state *state = drv->state + i;
        struct tty_port *port = &state->port;

        tty_port_init(port);
        port->ops = &uart_port_ops;   /* [KEY] TTY port ops */
        port->close_delay     = 500;  /* .5 seconds */
        port->closing_wait    = 30000;/* 30 seconds */
    }

    /* [STEP 5] Register with TTY layer */
    retval = tty_register_driver(normal);
    if (retval >= 0)
        return retval;

    /* Error cleanup */
    put_tty_driver(normal);
out_kfree:
    kfree(drv->state);
out:
    return -ENOMEM;
}
```

### 3.5 Lifetime and Ownership Rules

```
+-----------------------------------------------------------------------+
|                    uart_driver LIFETIME                                |
+-----------------------------------------------------------------------+

Module Load:
    1. uart_register_driver(&amba_reg)    → Creates tty_driver, uart_state[]
    2. uart_add_one_port(&amba_reg, &port) → Adds each port

Module Running:
    - uart_driver and tty_driver remain alive
    - Ports can be added/removed dynamically

Module Unload:
    1. uart_remove_one_port(&amba_reg, &port) → Remove each port
    2. uart_unregister_driver(&amba_reg)      → Frees tty_driver, state[]
```

**Ownership Rules:**
1. `uart_driver` is statically allocated by the driver module
2. `uart_state[]` is allocated by `uart_register_driver()`, freed by `uart_unregister_driver()`
3. `tty_driver` is allocated by serial core, freed by serial core
4. All ports must be removed before unregistering the driver

---

## Phase 4 — struct uart_port (Per-Port State)

### 4.1 What uart_port Represents

`struct uart_port` represents a **single UART port instance**—one physical (or virtual) serial port with its own registers, IRQ, and state.

### 4.2 Structure Definition (Key Fields)

```c
/* include/linux/serial_core.h */
struct uart_port {
    spinlock_t          lock;           /* [LOCK] Port lock for IRQ safety */
    
    /* I/O Access */
    unsigned long       iobase;         /* [IO] I/O port base (inb/outb) */
    unsigned char __iomem *membase;     /* [MMIO] Memory-mapped base */
    unsigned int        (*serial_in)(struct uart_port *, int);
    void                (*serial_out)(struct uart_port *, int, int);
    
    /* Hardware Config */
    unsigned int        irq;            /* [IRQ] IRQ number */
    unsigned long       irqflags;       /* IRQ flags (shared, etc.) */
    unsigned int        uartclk;        /* [CLK] UART input clock rate */
    unsigned int        fifosize;       /* [HW] TX FIFO size */
    unsigned char       x_char;         /* [FLOW] XON/XOFF char to send */
    unsigned char       regshift;       /* [HW] Register address shift */
    unsigned char       iotype;         /* [HW] I/O access type */
#define UPIO_PORT       (0)             /* I/O port access */
#define UPIO_MEM        (2)             /* Memory-mapped I/O */
#define UPIO_MEM32      (3)             /* 32-bit memory-mapped */

    /* Status Masks */
    unsigned int        read_status_mask;   /* Which errors to report */
    unsigned int        ignore_status_mask; /* Which errors to ignore */
    
    /* Linkage */
    struct uart_state   *state;         /* [KEY] Link to uart_state */
    struct uart_icount  icount;         /* [STATS] TX/RX/error counters */
    struct console      *cons;          /* Console, if any */
    
    /* Flags and Type */
    upf_t               flags;          /* [FLAGS] Port flags */
#define UPF_BOOT_AUTOCONF   (1 << 28)   /* Auto-configure at boot */
#define UPF_FIXED_PORT      (1 << 29)   /* Cannot change I/O base */
#define UPF_DEAD            (1 << 30)   /* Port is being removed */

    unsigned int        mctrl;          /* [MODEM] Current modem control */
    unsigned int        timeout;        /* [TIMEOUT] Character timeout */
    unsigned int        type;           /* [TYPE] Port type (PORT_PL011, etc.) */
    const struct uart_ops *ops;         /* [OPS] Hardware operations */
    unsigned int        line;           /* [INDEX] Port index (0, 1, 2...) */
    resource_size_t     mapbase;        /* [MMIO] Physical address for ioremap */
    struct device       *dev;           /* [DEV] Parent device */
    unsigned char       suspended;      /* [PM] Suspended state */
    void                *private_data;  /* [PRIVATE] Driver-specific data */
};
```

### 4.3 Memory Layout and Private Data

```
+-----------------------------------------------------------------------+
|                    PRIVATE DATA PATTERN                                |
+-----------------------------------------------------------------------+

Pattern 1: Embedding (Most Common)
+---------------------------+
| struct uart_amba_port     |
| +---------------------+   |
| | struct uart_port    |   |  <-- First field
| | - lock              |   |
| | - membase           |   |
| | - ops               |   |
| +---------------------+   |
| struct clk *clk           |  <-- Driver-specific fields
| unsigned int im           |
| unsigned int old_status   |
| bool using_dma            |
+---------------------------+

Recovery: container_of(port, struct uart_amba_port, port)

Pattern 2: private_data pointer
+------------------+           +------------------+
| struct uart_port |---------->| Driver Data      |
| - private_data   |           | - custom fields  |
+------------------+           +------------------+
```

**Example: PL011 Embedding Pattern**

```c
/* drivers/tty/serial/amba-pl011.c */
struct uart_amba_port {
    struct uart_port    port;           /* [KEY] Embedded uart_port FIRST */
    struct clk          *clk;           /* Clock control */
    const struct vendor_data *vendor;   /* Vendor-specific quirks */
    unsigned int        dmacr;          /* DMA control register */
    unsigned int        im;             /* Interrupt mask */
    unsigned int        old_status;     /* Previous modem status */
    unsigned int        fifosize;       /* FIFO size */
    unsigned int        lcrh_tx;        /* TX line control register offset */
    unsigned int        lcrh_rx;        /* RX line control register offset */
    bool                autorts;        /* Auto RTS enabled */
    char                type[12];       /* Type string */
    bool                interrupt_may_hang;
#ifdef CONFIG_DMA_ENGINE
    bool                using_tx_dma;
    bool                using_rx_dma;
    struct pl011_dmarx_data dmarx;
    struct pl011_dmatx_data dmatx;
#endif
};

/* Recovery in ops callbacks */
static void pl011_start_tx(struct uart_port *port)
{
    /* [KEY] Recover full structure from embedded uart_port */
    struct uart_amba_port *uap = (struct uart_amba_port *)port;
    
    uap->im |= UART011_TXIM;  /* Access driver-specific field */
    writew(uap->im, uap->port.membase + UART011_IMSC);
}
```

### 4.4 How Ports Are Added and Removed

```c
/* Adding a port (in driver's probe) */
static int pl011_probe(struct amba_device *dev, const struct amba_id *id)
{
    struct uart_amba_port *uap;
    
    /* [STEP 1] Allocate driver structure (includes uart_port) */
    uap = kzalloc(sizeof(*uap), GFP_KERNEL);
    
    /* [STEP 2] Configure uart_port fields */
    uap->port.dev = &dev->dev;
    uap->port.mapbase = dev->res.start;
    uap->port.membase = ioremap(dev->res.start, ...);
    uap->port.iotype = UPIO_MEM;
    uap->port.irq = dev->irq[0];
    uap->port.fifosize = uap->fifosize;
    uap->port.ops = &amba_pl011_pops;    /* [KEY] Assign ops */
    uap->port.flags = UPF_BOOT_AUTOCONF;
    uap->port.line = i;                  /* Port index */
    
    /* [STEP 3] Register with serial core */
    ret = uart_add_one_port(&amba_reg, &uap->port);
    
    return ret;
}

/* Removing a port (in driver's remove) */
static int pl011_remove(struct amba_device *dev)
{
    struct uart_amba_port *uap = amba_get_drvdata(dev);
    
    /* [STEP 1] Unregister from serial core */
    uart_remove_one_port(&amba_reg, &uap->port);
    
    /* [STEP 2] Free resources */
    iounmap(uap->port.membase);
    clk_put(uap->clk);
    kfree(uap);
    
    return 0;
}
```

---

## Phase 5 — struct uart_ops (Hardware Abstraction)

### 5.1 What uart_ops Represents

`struct uart_ops` is the **hardware abstraction layer**—a table of function pointers that the serial core calls to perform hardware operations. Each UART driver implements these callbacks for its specific hardware.

### 5.2 Complete Callback Enumeration

```c
/* include/linux/serial_core.h */
struct uart_ops {
    /* Status Callbacks */
    unsigned int (*tx_empty)(struct uart_port *);
    void         (*set_mctrl)(struct uart_port *, unsigned int mctrl);
    unsigned int (*get_mctrl)(struct uart_port *);
    
    /* TX Control */
    void (*stop_tx)(struct uart_port *);
    void (*start_tx)(struct uart_port *);
    void (*send_xchar)(struct uart_port *, char ch);
    
    /* RX Control */
    void (*stop_rx)(struct uart_port *);
    void (*enable_ms)(struct uart_port *);  /* Enable modem status IRQ */
    
    /* Line Control */
    void (*break_ctl)(struct uart_port *, int ctl);
    
    /* Port Lifecycle */
    int  (*startup)(struct uart_port *);
    void (*shutdown)(struct uart_port *);
    void (*flush_buffer)(struct uart_port *);
    
    /* Configuration */
    void (*set_termios)(struct uart_port *, struct ktermios *new,
                        struct ktermios *old);
    void (*set_ldisc)(struct uart_port *, int new);
    
    /* Power Management */
    void (*pm)(struct uart_port *, unsigned int state, unsigned int oldstate);
    int  (*set_wake)(struct uart_port *, unsigned int state);
    
    /* Port Information */
    const char *(*type)(struct uart_port *);
    
    /* Resource Management */
    void (*release_port)(struct uart_port *);
    int  (*request_port)(struct uart_port *);
    void (*config_port)(struct uart_port *, int);
    int  (*verify_port)(struct uart_port *, struct serial_struct *);
    
    /* Misc */
    int  (*ioctl)(struct uart_port *, unsigned int, unsigned long);
    
#ifdef CONFIG_CONSOLE_POLL
    void (*poll_put_char)(struct uart_port *, unsigned char);
    int  (*poll_get_char)(struct uart_port *);
#endif
};
```

### 5.3 Callback Contract Table

| Callback | When Called | Context | Core Guarantees | Driver Must |
|----------|-------------|---------|-----------------|-------------|
| `tx_empty` | Check if TX complete | Any (spinlock held) | Port lock held | Return TIOCSER_TEMT if empty |
| `set_mctrl` | Set modem control lines | Any (spinlock held) | Port lock held | Set RTS/DTR as requested |
| `get_mctrl` | Read modem status | Any (spinlock held) | Port lock held | Return CTS/DCD/DSR/RI state |
| `stop_tx` | Stop transmission | IRQ or process | Port lock held | Disable TX interrupt |
| `start_tx` | Start transmission | IRQ or process | Port lock held | Enable TX interrupt |
| `send_xchar` | Send XON/XOFF urgently | Process | May hold port lock | Queue x_char for immediate TX |
| `stop_rx` | Stop receiving | IRQ or process | Port lock held | Disable RX interrupt |
| `enable_ms` | Enable modem status IRQ | Process | Port lock held | Enable CTS/DCD/DSR/RI IRQs |
| `break_ctl` | Send break signal | Process | - | Set/clear break on TX line |
| `startup` | Port opened | Process | May sleep | Request IRQ, enable hardware |
| `shutdown` | Port closed | Process | May sleep | Free IRQ, disable hardware |
| `flush_buffer` | Discard TX buffer | IRQ or process | Port lock held | Cancel pending TX, clear FIFO |
| `set_termios` | Settings changed | Process | - | Configure baud, parity, etc. |
| `type` | Get port type string | Process | - | Return type name |
| `release_port` | Free I/O resources | Process | May sleep | iounmap, release_region |
| `request_port` | Acquire I/O resources | Process | May sleep | ioremap, request_region |
| `config_port` | Auto-configure port | Process | May sleep | Detect and configure hardware |

### 5.4 Example: PL011 uart_ops Implementation

```c
/* drivers/tty/serial/amba-pl011.c */
static struct uart_ops amba_pl011_pops = {
    .tx_empty       = pl01x_tx_empty,
    .set_mctrl      = pl011_set_mctrl,
    .get_mctrl      = pl01x_get_mctrl,
    .stop_tx        = pl011_stop_tx,
    .start_tx       = pl011_start_tx,
    .stop_rx        = pl011_stop_rx,
    .enable_ms      = pl011_enable_ms,
    .break_ctl      = pl011_break_ctl,
    .startup        = pl011_startup,
    .shutdown       = pl011_shutdown,
    .flush_buffer   = pl011_dma_flush_buffer,
    .set_termios    = pl011_set_termios,
    .type           = pl011_type,
    .release_port   = pl010_release_port,
    .request_port   = pl010_request_port,
    .config_port    = pl010_config_port,
    .verify_port    = pl010_verify_port,
#ifdef CONFIG_CONSOLE_POLL
    .poll_get_char  = pl010_get_poll_char,
    .poll_put_char  = pl010_put_poll_char,
#endif
};
```

### 5.5 Key Callback Implementations

**start_tx() - Enable Transmission**

```c
static void pl011_start_tx(struct uart_port *port)
{
    struct uart_amba_port *uap = (struct uart_amba_port *)port;

    /* [KEY] Enable TX interrupt - hardware will call ISR when FIFO has space */
    if (!pl011_dma_tx_start(uap)) {
        uap->im |= UART011_TXIM;
        writew(uap->im, uap->port.membase + UART011_IMSC);
    }
}
```

**startup() - Port Initialization**

```c
static int pl011_startup(struct uart_port *port)
{
    struct uart_amba_port *uap = (struct uart_amba_port *)port;
    int retval;

    /* [STEP 1] Enable clock */
    retval = clk_enable(uap->clk);
    if (retval)
        goto out;

    /* [STEP 2] Request IRQ */
    retval = request_irq(uap->port.irq, pl011_int, 0, "uart-pl011", uap);
    if (retval)
        goto clk_dis;

    /* [STEP 3] Configure hardware */
    writew(uap->vendor->ifls, uap->port.membase + UART011_IFLS);

    /* [STEP 4] Enable RX interrupts */
    uap->im = UART011_RXIM | UART011_RTIM;
    writew(uap->im, uap->port.membase + UART011_IMSC);

    /* [STEP 5] Enable UART */
    writew(UART01x_CR_UARTEN | UART011_CR_RXE | UART011_CR_TXE,
           uap->port.membase + UART011_CR);

    return 0;

clk_dis:
    clk_disable(uap->clk);
out:
    return retval;
}
```

### 5.6 How This Follows the xxx->ops->yyy() Pattern

```
+-----------------------------------------------------------------------+
|                    OPS PATTERN IN SERIAL SUBSYSTEM                     |
+-----------------------------------------------------------------------+

Serial Core                              UART Driver
     |                                        |
     | uart_start(tty)                        |
     |     |                                  |
     |     v                                  |
     | port->ops->start_tx(port)              |
     |-------------------------->             |
     |                          | pl011_start_tx(port)
     |                          |     |
     |                          |     v
     |                          | uap->im |= UART011_TXIM
     |                          | writew(uap->im, ...)
     |                          |
     |<--------------------------
     |
```

**This is the `xxx->ops->yyy(xxx, ...)` pattern:**
- `xxx` = `uart_port`
- `ops` = `uart_ops` (table of function pointers)
- `yyy` = `start_tx`, `stop_tx`, etc.

**中文解释：**
- 串口核心不知道具体硬件如何工作
- 驱动程序将硬件操作封装在`uart_ops`回调中
- 核心通过`port->ops->xxx(port)`调用驱动的实现
- 驱动从`uart_port`恢复完整的私有结构并访问硬件

---

## Phase 6 — Driver Registration & Port Lifecycle

### 6.1 How uart_register_driver() Works

```c
/* drivers/tty/serial/serial_core.c */
int uart_register_driver(struct uart_driver *drv)
{
    struct tty_driver *normal;
    int i, retval;

    BUG_ON(drv->state);

    /* [STEP 1] Allocate uart_state array for all possible ports */
    drv->state = kzalloc(sizeof(struct uart_state) * drv->nr, GFP_KERNEL);

    /* [STEP 2] Allocate and configure TTY driver */
    normal = alloc_tty_driver(drv->nr);
    drv->tty_driver = normal;
    
    normal->owner       = drv->owner;
    normal->driver_name = drv->driver_name;
    normal->name        = drv->dev_name;
    normal->major       = drv->major;
    normal->minor_start = drv->minor;
    normal->type        = TTY_DRIVER_TYPE_SERIAL;
    normal->subtype     = SERIAL_TYPE_NORMAL;
    normal->init_termios = tty_std_termios;
    normal->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    normal->flags       = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
    normal->driver_state = drv;
    
    /* [KEY] Hook serial core operations to TTY layer */
    tty_set_operations(normal, &uart_ops);

    /* [STEP 3] Initialize each uart_state's tty_port */
    for (i = 0; i < drv->nr; i++) {
        struct uart_state *state = drv->state + i;
        struct tty_port *port = &state->port;
        
        tty_port_init(port);
        port->ops = &uart_port_ops;
    }

    /* [STEP 4] Register with TTY layer */
    retval = tty_register_driver(normal);
    
    return retval;
}
```

### 6.2 How uart_add_one_port() Works

```c
int uart_add_one_port(struct uart_driver *drv, struct uart_port *uport)
{
    struct uart_state *state;
    struct tty_port *port;
    int ret = 0;
    struct device *tty_dev;

    BUG_ON(in_interrupt());  /* [CHECK] Must not be called from IRQ */

    if (uport->line >= drv->nr)
        return -EINVAL;

    /* [STEP 1] Get state slot for this port index */
    state = drv->state + uport->line;
    port = &state->port;

    mutex_lock(&port_mutex);
    mutex_lock(&port->mutex);
    
    if (state->uart_port) {
        ret = -EINVAL;  /* Already registered */
        goto out;
    }

    /* [STEP 2] Link port to state */
    state->uart_port = uport;
    state->pm_state = -1;
    uport->cons = drv->cons;
    uport->state = state;

    /* [STEP 3] Initialize spinlock (unless console already did) */
    if (!(uart_console(uport) && (uport->cons->flags & CON_ENABLED))) {
        spin_lock_init(&uport->lock);
        lockdep_set_class(&uport->lock, &port_lock_key);
    }

    /* [STEP 4] Configure the port (calls ops->config_port) */
    uart_configure_port(drv, state, uport);

    /* [STEP 5] Register TTY device */
    tty_dev = tty_register_device(drv->tty_driver, uport->line, uport->dev);
    if (likely(!IS_ERR(tty_dev))) {
        device_init_wakeup(tty_dev, 1);
        device_set_wakeup_enable(tty_dev, 0);
    }

    /* [STEP 6] Clear dead flag */
    uport->flags &= ~UPF_DEAD;

out:
    mutex_unlock(&port->mutex);
    mutex_unlock(&port_mutex);
    return ret;
}
```

### 6.3 Registration Flow Diagram

```
+-----------------------------------------------------------------------+
|                    DRIVER REGISTRATION FLOW                            |
+-----------------------------------------------------------------------+

Module Init:
    |
    | uart_register_driver(&amba_reg)
    |
    +---> Allocates uart_state[nr]
    +---> Allocates tty_driver
    +---> Hooks uart_ops to TTY
    +---> tty_register_driver()
    |
    | amba_driver_register(&pl011_driver)
    |
    +---> Register with AMBA bus
          |
          | (for each matching device)
          |
          +---> pl011_probe()
                |
                +---> Allocate uart_amba_port
                +---> Configure uart_port
                +---> uart_add_one_port()
                      |
                      +---> Link to uart_state[line]
                      +---> uart_configure_port()
                      +---> tty_register_device()
                            |
                            +---> Creates /dev/ttyAMAx
```

### 6.4 Probe/Remove Ordering

```c
/* Correct probe sequence */
static int pl011_probe(...)
{
    /* 1. Allocate private structure */
    uap = kzalloc(sizeof(*uap), GFP_KERNEL);
    
    /* 2. Map hardware resources */
    base = ioremap(dev->res.start, ...);
    
    /* 3. Get clock */
    uap->clk = clk_get(&dev->dev, NULL);
    
    /* 4. Configure uart_port */
    uap->port.membase = base;
    uap->port.ops = &amba_pl011_pops;
    ...
    
    /* 5. Register with serial core (LAST) */
    ret = uart_add_one_port(&amba_reg, &uap->port);
    if (ret) {
        /* Cleanup in reverse order */
        clk_put(uap->clk);
        iounmap(base);
        kfree(uap);
    }
    
    return ret;
}

/* Correct remove sequence */
static int pl011_remove(...)
{
    /* 1. Unregister from serial core (FIRST) */
    uart_remove_one_port(&amba_reg, &uap->port);
    
    /* 2. Free resources in reverse order of allocation */
    iounmap(uap->port.membase);
    clk_put(uap->clk);
    kfree(uap);
    
    return 0;
}
```

### 6.5 Cleanup Rules and Failure Handling

| Scenario | Correct Handling |
|----------|------------------|
| Allocation failure | Return error, free nothing |
| ioremap failure | Free allocation, return error |
| uart_add_one_port failure | Free all resources in reverse order |
| Module unload | Remove all ports, then unregister driver |

**Key Contract:**
- Resources must be freed in **reverse order** of acquisition
- `uart_remove_one_port()` must be called **before** freeing uart_port memory
- All ports must be removed **before** `uart_unregister_driver()`

---

## Phase 7 — Data Path: TX & RX Flow

### 7.1 TX Path: User write() to Hardware

```
+-----------------------------------------------------------------------+
|                    TX DATA PATH                                        |
+-----------------------------------------------------------------------+

User Space:
    write(fd, "Hello", 5)
           |
           v
VFS Layer:
    sys_write() → file->f_op->write()
           |
           v
TTY Core (tty_io.c):
    tty_write()
           |
           v
Line Discipline (n_tty.c):
    n_tty_write()
    - Process output (OPOST: NL→CRNL, etc.)
    - Call tty->ops->write()
           |
           v
Serial Core (serial_core.c):
    uart_write()
    - Acquire port->lock
    - Copy data to circular buffer (state->xmit)
    - Release lock
    - Call uart_start()
           |
           v
    uart_start()
    - Acquire port->lock
    - Call port->ops->start_tx(port)
           |
           v
UART Driver (amba-pl011.c):
    pl011_start_tx()
    - Enable TX interrupt: uap->im |= UART011_TXIM
    - Write to IMSC register
           |
           v
[TX Interrupt fires when FIFO has space]
           |
           v
    pl011_int() → pl011_tx_chars()
    - Read from xmit circular buffer
    - Write to TX FIFO: writew(ch, UART01x_DR)
    - If buffer empty: disable TX interrupt
    - If buffer low: call uart_write_wakeup()
           |
           v
Hardware:
    TX FIFO → Shift Register → TX Pin
```

**Key Code: uart_write()**

```c
/* drivers/tty/serial/serial_core.c */
static int uart_write(struct tty_struct *tty,
                      const unsigned char *buf, int count)
{
    struct uart_state *state = tty->driver_data;
    struct uart_port *port;
    struct circ_buf *circ;
    unsigned long flags;
    int c, ret = 0;

    port = state->uart_port;
    circ = &state->xmit;

    if (!circ->buf)
        return 0;

    /* [KEY] Copy to circular buffer under lock */
    spin_lock_irqsave(&port->lock, flags);
    while (1) {
        c = CIRC_SPACE_TO_END(circ->head, circ->tail, UART_XMIT_SIZE);
        if (count < c)
            c = count;
        if (c <= 0)
            break;
        memcpy(circ->buf + circ->head, buf, c);
        circ->head = (circ->head + c) & (UART_XMIT_SIZE - 1);
        buf += c;
        count -= c;
        ret += c;
    }
    spin_unlock_irqrestore(&port->lock, flags);

    /* [KEY] Trigger transmission */
    uart_start(tty);
    return ret;
}
```

**Key Code: pl011_tx_chars() - ISR TX Handler**

```c
static void pl011_tx_chars(struct uart_amba_port *uap)
{
    struct circ_buf *xmit = &uap->port.state->xmit;
    int count;

    /* Handle x_char (XON/XOFF) first */
    if (uap->port.x_char) {
        writew(uap->port.x_char, uap->port.membase + UART01x_DR);
        uap->port.icount.tx++;
        uap->port.x_char = 0;
        return;
    }

    /* Check if we should stop */
    if (uart_circ_empty(xmit) || uart_tx_stopped(&uap->port)) {
        pl011_stop_tx(&uap->port);
        return;
    }

    /* [KEY] Fill TX FIFO from circular buffer */
    count = uap->fifosize >> 1;  /* Half FIFO */
    do {
        writew(xmit->buf[xmit->tail], uap->port.membase + UART01x_DR);
        xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
        uap->port.icount.tx++;
        if (uart_circ_empty(xmit))
            break;
    } while (--count > 0);

    /* [KEY] Wake up writers if buffer getting empty */
    if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
        uart_write_wakeup(&uap->port);

    /* Stop TX if buffer empty */
    if (uart_circ_empty(xmit))
        pl011_stop_tx(&uap->port);
}
```

### 7.2 RX Path: Hardware to User read()

```
+-----------------------------------------------------------------------+
|                    RX DATA PATH                                        |
+-----------------------------------------------------------------------+

Hardware:
    RX Pin → Shift Register → RX FIFO
           |
           v
[RX Interrupt fires when FIFO threshold reached]
           |
           v
UART Driver (amba-pl011.c):
    pl011_int()
    - Read interrupt status
    - If RXIS/RTIS set: call pl011_rx_chars()
           |
           v
    pl011_rx_chars() → pl011_fifo_to_tty()
    - Read from RX FIFO: ch = readw(UART01x_DR)
    - Check for errors (parity, frame, overrun)
    - Handle sysrq: uart_handle_sysrq_char()
    - Insert char: uart_insert_char()
           |
           v
Serial Core Helper:
    uart_insert_char()
    - tty_insert_flip_char(tty, ch, flag)
           |
           v
TTY Buffer (tty_buffer.c):
    tty_insert_flip_char()
    - Add to flip buffer
           |
           v
    tty_flip_buffer_push()
    - Schedule work to push to ldisc
           |
           v
[Workqueue runs]
           |
           v
Line Discipline (n_tty.c):
    n_tty_receive_buf()
    - Process input (echo, line editing)
    - Add to read buffer
           |
           v
User Space:
    read(fd, buf, count)
    - n_tty_read() returns data
```

**Key Code: pl011_fifo_to_tty()**

```c
static int pl011_fifo_to_tty(struct uart_amba_port *uap)
{
    u16 status, ch;
    unsigned int flag, max_count = 256;
    int fifotaken = 0;

    while (max_count--) {
        /* [KEY] Check if FIFO empty */
        status = readw(uap->port.membase + UART01x_FR);
        if (status & UART01x_FR_RXFE)
            break;

        /* [KEY] Read from RX FIFO */
        ch = readw(uap->port.membase + UART01x_DR) | UART_DUMMY_DR_RX;
        flag = TTY_NORMAL;
        uap->port.icount.rx++;
        fifotaken++;

        /* [KEY] Handle errors */
        if (unlikely(ch & UART_DR_ERROR)) {
            if (ch & UART011_DR_BE) {
                ch &= ~(UART011_DR_FE | UART011_DR_PE);
                uap->port.icount.brk++;
                if (uart_handle_break(&uap->port))
                    continue;
            } else if (ch & UART011_DR_PE)
                uap->port.icount.parity++;
            else if (ch & UART011_DR_FE)
                uap->port.icount.frame++;
            if (ch & UART011_DR_OE)
                uap->port.icount.overrun++;

            ch &= uap->port.read_status_mask;

            if (ch & UART011_DR_BE)
                flag = TTY_BREAK;
            else if (ch & UART011_DR_PE)
                flag = TTY_PARITY;
            else if (ch & UART011_DR_FE)
                flag = TTY_FRAME;
        }

        /* [KEY] Handle sysrq */
        if (uart_handle_sysrq_char(&uap->port, ch & 255))
            continue;

        /* [KEY] Insert into TTY layer */
        uart_insert_char(&uap->port, ch, UART011_DR_OE, ch, flag);
    }

    return fifotaken;
}
```

### 7.3 Buffering and Flow Control

```
+-----------------------------------------------------------------------+
|                    BUFFER STRUCTURE                                    |
+-----------------------------------------------------------------------+

TX Buffering:
+----------------+      +----------------+      +----------------+
| User Buffer    | ---> | xmit circ_buf  | ---> | TX FIFO        |
| (user space)   |      | (PAGE_SIZE)    |      | (16-64 bytes)  |
+----------------+      +----------------+      +----------------+
                        state->xmit              Hardware

RX Buffering:
+----------------+      +----------------+      +----------------+
| RX FIFO        | ---> | Flip Buffer    | ---> | n_tty buffer   |
| (16-64 bytes)  |      | (tty_buffer)   |      | (4K)           |
+----------------+      +----------------+      +----------------+
Hardware                 Kernel                  User read()


Flow Control:
+-------------------+
| Software (XON/XOFF)|  uart_port->x_char = XOFF to stop sender
+-------------------+
| Hardware (RTS/CTS) |  uart_handle_cts_change() starts/stops TX
+-------------------+
```

**中文解释：**
- **TX缓冲**：用户数据先复制到循环缓冲区，再由中断处理程序移到硬件FIFO
- **RX缓冲**：硬件FIFO数据通过翻转缓冲区传递给线路规程，用户通过read()读取
- **流控制**：软件流控使用XON/XOFF字符，硬件流控使用RTS/CTS信号线

---

## Phase 8 — Interrupt Handling & Concurrency

### 8.1 Which Code Runs in IRQ Context

| Function | Context | Lock Held |
|----------|---------|-----------|
| `pl011_int()` (ISR) | IRQ (hardirq) | None initially |
| `pl011_tx_chars()` | IRQ | port->lock |
| `pl011_rx_chars()` | IRQ | port->lock |
| `uart_write()` | Process | Acquires port->lock |
| `uart_start()` | Process or IRQ | Acquires port->lock |
| `set_termios()` | Process | May acquire port->lock |
| `startup()` | Process | May sleep |
| `shutdown()` | Process | May sleep |

### 8.2 Locking: Spinlocks vs Mutexes

```c
struct uart_port {
    spinlock_t  lock;  /* [IRQ-SAFE] For TX/RX buffer and register access */
};

struct uart_state {
    struct tty_port port;  /* Contains mutex for port open/close */
};

/* Global mutex */
static DEFINE_MUTEX(port_mutex);  /* Serializes add/remove port */
```

**Locking Hierarchy:**

```
+-----------------------------------------------------------------------+
|                    LOCKING HIERARCHY                                   |
+-----------------------------------------------------------------------+

port_mutex (global)           <-- Coarse: add/remove port
    |
    v
tty_port->mutex (per-port)    <-- Medium: open/close serialization
    |
    v
uart_port->lock (per-port)    <-- Fine: IRQ-safe data access
                                   - TX/RX buffer manipulation
                                   - Register read/write
                                   - Modem control
```

**Usage Patterns:**

```c
/* Pattern 1: Process context with lock */
static int uart_write(struct tty_struct *tty, ...)
{
    spin_lock_irqsave(&port->lock, flags);
    /* Manipulate xmit buffer */
    spin_unlock_irqrestore(&port->lock, flags);
}

/* Pattern 2: IRQ context already has lock */
static void uart_start(struct tty_struct *tty)
{
    spin_lock_irqsave(&port->lock, flags);
    __uart_start(tty);  /* Calls port->ops->start_tx() */
    spin_unlock_irqrestore(&port->lock, flags);
}

/* Pattern 3: ISR acquires lock */
static irqreturn_t pl011_int(int irq, void *dev_id)
{
    struct uart_amba_port *uap = dev_id;
    
    spin_lock_irqsave(&uap->port.lock, flags);
    
    /* Handle TX/RX */
    if (status & UART011_TXIS)
        pl011_tx_chars(uap);  /* Called with lock held */
    
    spin_unlock_irqrestore(&uap->port.lock, flags);
}
```

### 8.3 Preventing Races

**Race 1: TX Buffer Access**

```
Producer (write)                Consumer (ISR)
--------------                  --------------
spin_lock(&port->lock)          spin_lock(&port->lock)
memcpy(circ->buf, data)         ch = circ->buf[tail]
circ->head = new_head           circ->tail = new_tail
spin_unlock(&port->lock)        spin_unlock(&port->lock)
```

**Race 2: Termios Changes During I/O**

```c
static void uart_change_speed(struct tty_struct *tty, ...)
{
    /* Called from set_termios() in process context */
    
    /* Note: port->ops->set_termios() should handle its own locking
     * if it accesses registers that ISR also touches */
}

static void pl011_set_termios(struct uart_port *port, ...)
{
    unsigned long flags;
    
    /* [KEY] Disable interrupts while reconfiguring */
    spin_lock_irqsave(&port->lock, flags);
    
    /* Reconfigure baud rate, data bits, etc. */
    
    spin_unlock_irqrestore(&port->lock, flags);
}
```

**Race 3: Shutdown During TX**

```c
static void pl011_shutdown(struct uart_port *port)
{
    struct uart_amba_port *uap = (struct uart_amba_port *)port;
    unsigned long flags;

    /* [STEP 1] Disable interrupts under lock */
    spin_lock_irqsave(&uap->port.lock, flags);
    uap->im = 0;
    writew(uap->im, uap->port.membase + UART011_IMSC);
    spin_unlock_irqrestore(&uap->port.lock, flags);

    /* [STEP 2] Now safe to free IRQ (no new interrupts) */
    free_irq(uap->port.irq, uap);

    /* [STEP 3] Disable UART hardware */
    writew(0, uap->port.membase + UART011_CR);
}
```

### 8.4 Why Sleeping Is Forbidden in Most Callbacks

| Callback | Called With | Why No Sleep |
|----------|-------------|--------------|
| `start_tx` | port->lock held | Spinlock held |
| `stop_tx` | port->lock held | Spinlock held |
| `get_mctrl` | port->lock held | Spinlock held |
| `set_mctrl` | port->lock held | Spinlock held |
| `tx_empty` | port->lock held | Spinlock held |

**These callbacks CAN sleep:**
- `startup()`: May call `request_irq()`, `clk_enable()`
- `shutdown()`: May call `free_irq()`
- `set_termios()`: May configure clocks (implementation dependent)
- `request_port()`: May call `ioremap()`
- `release_port()`: May call `iounmap()`

---

## Phase 9 — Power Management & Console Support

### 9.1 Suspend/Resume Handling

```c
/* drivers/tty/serial/serial_core.c */
int uart_suspend_port(struct uart_driver *drv, struct uart_port *uport)
{
    struct uart_state *state = drv->state + uport->line;
    struct tty_port *port = &state->port;
    struct device *tty_dev;
    
    mutex_lock(&port->mutex);

    /* [CHECK] Can this port wake the system? */
    tty_dev = device_find_child(uport->dev, &match, serial_match_port);
    if (device_may_wakeup(tty_dev)) {
        if (uport->irq_wake) {
            uport->irq_wake = 0;
            /* Keep IRQ enabled for wake */
        }
        mutex_unlock(&port->mutex);
        return 0;
    }
    
    /* [STEP 1] Wait for TX to complete */
    if (port->flags & ASYNC_INITIALIZED) {
        const struct uart_ops *ops = uport->ops;
        int tries;
        
        /* Wait for transmitter to empty */
        for (tries = 3; !ops->tx_empty(uport) && tries; tries--)
            msleep(10);
    }

    /* [STEP 2] Disable port */
    if (port->flags & ASYNC_INITIALIZED) {
        uport->ops->stop_tx(uport);
        uport->ops->stop_rx(uport);
        uport->ops->shutdown(uport);
    }

    /* [STEP 3] Save termios for restore */
    if (port->tty)
        uport->cons_cflag = port->tty->termios->c_cflag;

    /* [STEP 4] Call driver's PM callback */
    if (uport->ops->pm)
        uport->ops->pm(uport, 3, 0);  /* State 3 = D3 */

    uport->suspended = 1;
    mutex_unlock(&port->mutex);
    return 0;
}

int uart_resume_port(struct uart_driver *drv, struct uart_port *uport)
{
    struct uart_state *state = drv->state + uport->line;
    struct tty_port *port = &state->port;
    
    mutex_lock(&port->mutex);

    uport->suspended = 0;

    /* [STEP 1] Power on */
    if (uport->ops->pm)
        uport->ops->pm(uport, 0, 3);  /* State 0 = D0 */

    /* [STEP 2] Re-enable if was active */
    if (port->flags & ASYNC_INITIALIZED) {
        uport->ops->set_termios(uport, &termios, NULL);
        uport->ops->startup(uport);
    }

    mutex_unlock(&port->mutex);
    return 0;
}
```

**Driver Usage:**

```c
/* drivers/tty/serial/amba-pl011.c */
#ifdef CONFIG_PM
static int pl011_suspend(struct amba_device *dev, pm_message_t state)
{
    struct uart_amba_port *uap = amba_get_drvdata(dev);
    
    if (!uap)
        return -EINVAL;

    return uart_suspend_port(&amba_reg, &uap->port);
}

static int pl011_resume(struct amba_device *dev)
{
    struct uart_amba_port *uap = amba_get_drvdata(dev);
    
    if (!uap)
        return -EINVAL;

    return uart_resume_port(&amba_reg, &uap->port);
}
#endif
```

### 9.2 Console Registration and Early printk

```
+-----------------------------------------------------------------------+
|                    CONSOLE ARCHITECTURE                                |
+-----------------------------------------------------------------------+

                    +-------------------+
                    | Console Subsystem |
                    | (kernel/printk.c) |
                    +--------+----------+
                             |
           +-----------------+-----------------+
           |                 |                 |
    +------v------+   +------v------+   +------v------+
    | VGA Console |   | Netconsole  |   | UART Console|
    |             |   |             |   | (serial)    |
    +-------------+   +-------------+   +------+------+
                                               |
                                    +----------+----------+
                                    |                     |
                              +-----v-----+         +-----v-----+
                              | Early     |         | Normal    |
                              | Console   |         | Console   |
                              | (earlyprintk) |     | (full TTY)|
                              +-----------+         +-----------+
```

**Console Structure:**

```c
/* drivers/tty/serial/amba-pl011.c */
static struct console amba_console = {
    .name       = "ttyAMA",             /* Console name prefix */
    .write      = pl011_console_write,  /* [KEY] Output function */
    .device     = uart_console_device,  /* Get TTY driver */
    .setup      = pl011_console_setup,  /* [KEY] Parse console= options */
    .flags      = CON_PRINTBUFFER,      /* Print buffered messages */
    .index      = -1,                   /* Any port */
    .data       = &amba_reg,            /* Link to uart_driver */
};

#define AMBA_CONSOLE    (&amba_console)

static struct uart_driver amba_reg = {
    ...
    .cons       = AMBA_CONSOLE,         /* [KEY] Link console to driver */
};
```

**Console Write (Polling Mode):**

```c
static void pl011_console_putchar(struct uart_port *port, int ch)
{
    struct uart_amba_port *uap = (struct uart_amba_port *)port;

    /* [KEY] Busy-wait for space in TX FIFO */
    while (readw(uap->port.membase + UART01x_FR) & UART01x_FR_TXFF)
        barrier();
    
    /* Write character */
    writew(ch, uap->port.membase + UART01x_DR);
}

static void pl011_console_write(struct console *co, const char *s,
                                unsigned int count)
{
    struct uart_amba_port *uap = amba_ports[co->index];
    unsigned int old_cr, new_cr;

    clk_enable(uap->clk);

    /* Save and disable interrupts */
    old_cr = readw(uap->port.membase + UART011_CR);
    new_cr = old_cr & ~UART011_CR_CTSEN;
    new_cr |= UART01x_CR_UARTEN | UART011_CR_TXE;
    writew(new_cr, uap->port.membase + UART011_CR);

    /* [KEY] Output each character (polling) */
    uart_console_write(&uap->port, s, count, pl011_console_putchar);

    /* Wait for TX to complete, restore */
    do {
        status = readw(uap->port.membase + UART01x_FR);
    } while (status & UART01x_FR_BUSY);
    writew(old_cr, uap->port.membase + UART011_CR);

    clk_disable(uap->clk);
}
```

### 9.3 Why Console UARTs Are Special

| Aspect | Normal UART | Console UART |
|--------|-------------|--------------|
| **Initialization** | At module load | Very early in boot |
| **Output method** | Interrupt-driven | Polling (blocking) |
| **Lock behavior** | Uses spinlock | May bypass locks (panic) |
| **Clock handling** | Can be gated | Must stay on for output |
| **Spinlock init** | By uart_add_one_port | By console_setup (early) |

**Boot-Time Constraints:**

```c
/* From uart_add_one_port() */
if (!(uart_console(uport) && (uport->cons->flags & CON_ENABLED))) {
    /* Normal port: initialize spinlock here */
    spin_lock_init(&uport->lock);
} else {
    /* Console port: spinlock already initialized by console code */
    /* before interrupts are enabled */
}
```

### 9.4 Console Boot Parameter

```
Kernel command line:
    console=ttyAMA0,115200n8

Parsing:
    - ttyAMA0: Use ttyAMA port 0
    - 115200: Baud rate
    - n: No parity
    - 8: 8 data bits

Handled by:
    uart_parse_options() → baud=115200, parity='n', bits=8
    uart_set_options()   → Configure hardware
```

---

## Phase 10 — Common Bugs & Architecture Lessons

### 10.1 Common UART Driver Bugs

#### Bug 1: Sleeping in Interrupt Context

```c
/* WRONG: Sleeping in start_tx (called with spinlock held) */
static void bad_start_tx(struct uart_port *port)
{
    clk_enable(my_clk);  /* [BUG] clk_enable may sleep! */
    writew(TX_EN, port->membase + CTRL);
}

/* CORRECT: Enable clock in startup() */
static int good_startup(struct uart_port *port)
{
    clk_enable(my_clk);  /* OK: startup may sleep */
    return 0;
}

static void good_start_tx(struct uart_port *port)
{
    writew(TX_EN, port->membase + CTRL);  /* Just register access */
}
```

#### Bug 2: Incorrect Locking

```c
/* WRONG: Missing lock in modem status handler */
static void bad_modem_status(struct uart_amba_port *uap)
{
    unsigned int status = readw(uap->port.membase + FR);
    
    /* [BUG] No lock! May race with termios change */
    if (status & CTS_CHANGED)
        uart_handle_cts_change(&uap->port, status & CTS);
}

/* CORRECT: Called from ISR which holds lock */
static irqreturn_t good_isr(int irq, void *dev_id)
{
    struct uart_amba_port *uap = dev_id;
    
    spin_lock(&uap->port.lock);
    
    if (status & MODEM_STATUS)
        handle_modem_status(uap);  /* Called with lock held */
    
    spin_unlock(&uap->port.lock);
}
```

#### Bug 3: Broken FIFO Handling

```c
/* WRONG: Reading FIFO without checking status */
static void bad_rx_chars(struct uart_amba_port *uap)
{
    int i;
    for (i = 0; i < 16; i++) {
        /* [BUG] Reading empty FIFO returns garbage! */
        ch = readw(uap->port.membase + DR);
        tty_insert_flip_char(tty, ch, TTY_NORMAL);
    }
}

/* CORRECT: Check FIFO status before reading */
static void good_rx_chars(struct uart_amba_port *uap)
{
    while (!(readw(uap->port.membase + FR) & RXFE)) {
        ch = readw(uap->port.membase + DR);  /* FIFO has data */
        tty_insert_flip_char(tty, ch, TTY_NORMAL);
    }
}
```

#### Bug 4: Termios Misconfiguration

```c
/* WRONG: Ignoring termios fields */
static void bad_set_termios(struct uart_port *port,
                            struct ktermios *termios,
                            struct ktermios *old)
{
    unsigned int baud = uart_get_baud_rate(port, termios, old, 0, 460800);
    
    /* [BUG] Ignoring CSTOPB (stop bits) and PARENB (parity) */
    set_baud_rate(port, baud);
}

/* CORRECT: Handle all relevant flags */
static void good_set_termios(struct uart_port *port, ...)
{
    unsigned int lcr = 0;
    unsigned int baud = uart_get_baud_rate(port, termios, old, 0, 460800);
    
    /* Handle data bits */
    switch (termios->c_cflag & CSIZE) {
    case CS5: lcr |= WLEN_5; break;
    case CS6: lcr |= WLEN_6; break;
    case CS7: lcr |= WLEN_7; break;
    default:  lcr |= WLEN_8; break;
    }
    
    /* Handle stop bits */
    if (termios->c_cflag & CSTOPB)
        lcr |= STOP_2;
    
    /* Handle parity */
    if (termios->c_cflag & PARENB) {
        lcr |= PARITY_EN;
        if (!(termios->c_cflag & PARODD))
            lcr |= PARITY_EVEN;
    }
    
    set_baud_rate(port, baud);
    writew(lcr, port->membase + LCR);
}
```

#### Bug 5: Race Conditions During Suspend/Resume

```c
/* WRONG: Not waiting for TX to complete before suspend */
static int bad_suspend(struct device *dev)
{
    struct my_port *port = dev_get_drvdata(dev);
    
    /* [BUG] May lose data in TX FIFO! */
    clk_disable(port->clk);
    return 0;
}

/* CORRECT: Wait for TX to drain */
static int good_suspend(struct device *dev)
{
    struct my_port *port = dev_get_drvdata(dev);
    
    /* Wait for transmitter to empty */
    while (!port->ops->tx_empty(&port->uart_port))
        msleep(10);
    
    clk_disable(port->clk);
    return 0;
}
```

### 10.2 What Makes a GOOD UART Driver

| Quality | Implementation |
|---------|---------------|
| **Correct locking** | Use port->lock for all register access from IRQ |
| **Complete termios** | Handle all relevant c_cflag bits |
| **Proper error handling** | Report parity, frame, overrun errors to TTY |
| **Flow control** | Implement hardware RTS/CTS correctly |
| **Resource cleanup** | Free IRQ, unmap in correct order in shutdown/remove |
| **Power awareness** | Disable clocks when port closed |
| **Console support** | Provide polling write for early boot |
| **Statistics** | Maintain icount for debugging |

### 10.3 How Serial Core Enforces Discipline

| Contract | Enforcement |
|----------|-------------|
| **Callback context rules** | Documentation + BUG_ON(in_interrupt()) |
| **Port indexing** | Validates line < nr in uart_add_one_port |
| **Duplicate port** | Checks state->uart_port == NULL |
| **Resource ordering** | register_driver before add_port |
| **Termios sanitization** | uart_get_baud_rate() clamps values |
| **Lifecycle management** | uart_state tracks open/close count |

### 10.4 Architecture Lessons for Other Subsystems

**Lesson 1: Layered Abstraction**
```
User API → Core Logic → Hardware Ops
(uniform)   (shared)    (per-device)
```
- Same pattern used in: Block layer, Network, Input subsystem

**Lesson 2: Ops Table Pattern**
```c
struct xxx_ops {
    int (*operation1)(...);
    int (*operation2)(...);
};

container->ops->operation1(container);
```
- Enables hot-pluggable implementations
- Clear contract between layers

**Lesson 3: State Embedding**
```c
struct my_device {
    struct generic_object obj;  /* Embed first */
    /* Private fields */
};

/* Recovery */
my_dev = container_of(obj, struct my_device, obj);
```
- Zero overhead for private data access
- No extra allocations

**Lesson 4: IRQ-Safe vs Sleepable Split**
```
startup()   ← May sleep: allocate, request_irq
start_tx()  ← Must not sleep: just enable interrupt
shutdown()  ← May sleep: free_irq, cleanup
```
- Clear documentation of context requirements

**Lesson 5: Circular Buffer for High-Throughput I/O**
```c
struct circ_buf {
    char *buf;
    int head;  /* Producer writes here */
    int tail;  /* Consumer reads here */
};
```
- Lock-free (almost) with single producer/consumer
- Used in: serial, audio, network drivers

---

## Appendix A — Walking Through a Real UART Driver (PL011)

### A.1 Driver Overview

The PL011 is ARM's AMBA-connected UART, found in many ARM SoCs. The Linux driver (`amba-pl011.c`) demonstrates:

- Embedded `uart_port` pattern
- DMA support (optional)
- Console support for boot messages
- Power management

### A.2 Key Data Structures

```c
/* Private device structure */
struct uart_amba_port {
    struct uart_port    port;           /* [EMBEDDED] Generic port */
    struct clk          *clk;           /* Clock control */
    const struct vendor_data *vendor;   /* Vendor quirks */
    unsigned int        dmacr;          /* DMA control shadow */
    unsigned int        im;             /* Interrupt mask shadow */
    unsigned int        old_status;     /* Previous modem status */
    unsigned int        fifosize;       /* FIFO depth */
    unsigned int        lcrh_tx;        /* TX control register offset */
    unsigned int        lcrh_rx;        /* RX control register offset */
    bool                autorts;        /* Auto RTS enabled */
    char                type[12];       /* Type name for /proc */
#ifdef CONFIG_DMA_ENGINE
    bool                using_tx_dma;
    bool                using_rx_dma;
    struct pl011_dmarx_data dmarx;
    struct pl011_dmatx_data dmatx;
#endif
};

/* uart_ops implementation */
static struct uart_ops amba_pl011_pops = {
    .tx_empty       = pl01x_tx_empty,
    .set_mctrl      = pl011_set_mctrl,
    .get_mctrl      = pl01x_get_mctrl,
    .stop_tx        = pl011_stop_tx,
    .start_tx       = pl011_start_tx,
    .stop_rx        = pl011_stop_rx,
    .enable_ms      = pl011_enable_ms,
    .break_ctl      = pl011_break_ctl,
    .startup        = pl011_startup,
    .shutdown       = pl011_shutdown,
    .flush_buffer   = pl011_dma_flush_buffer,
    .set_termios    = pl011_set_termios,
    .type           = pl011_type,
    .release_port   = pl010_release_port,
    .request_port   = pl010_request_port,
    .config_port    = pl010_config_port,
    .verify_port    = pl010_verify_port,
};
```

### A.3 Interrupt Handler

```c
static irqreturn_t pl011_int(int irq, void *dev_id)
{
    struct uart_amba_port *uap = dev_id;
    unsigned long flags;
    unsigned int status, pass_counter = AMBA_ISR_PASS_LIMIT;
    int handled = 0;

    /* [LOCK] Acquire port lock for entire ISR */
    spin_lock_irqsave(&uap->port.lock, flags);

    /* Read masked interrupt status */
    status = readw(uap->port.membase + UART011_MIS);
    
    if (status) {
        do {
            /* [ACK] Clear interrupts (except TX/RX which clear on FIFO access) */
            writew(status & ~(UART011_TXIS|UART011_RTIS|UART011_RXIS),
                   uap->port.membase + UART011_ICR);

            /* [RX] Handle receive interrupt or timeout */
            if (status & (UART011_RTIS|UART011_RXIS)) {
                if (pl011_dma_rx_running(uap))
                    pl011_dma_rx_irq(uap);
                else
                    pl011_rx_chars(uap);
            }
            
            /* [MODEM] Handle modem status change */
            if (status & (UART011_DSRMIS|UART011_DCDMIS|
                          UART011_CTSMIS|UART011_RIMIS))
                pl011_modem_status(uap);
            
            /* [TX] Handle transmit interrupt */
            if (status & UART011_TXIS)
                pl011_tx_chars(uap);

            /* [WATCHDOG] Prevent infinite loop */
            if (pass_counter-- == 0)
                break;

            /* Check for more interrupts */
            status = readw(uap->port.membase + UART011_MIS);
        } while (status != 0);
        handled = 1;
    }

    spin_unlock_irqrestore(&uap->port.lock, flags);

    return IRQ_RETVAL(handled);
}
```

### A.4 Complete Driver Initialization

```c
/* Module init */
static int __init pl011_init(void)
{
    int ret;
    printk(KERN_INFO "Serial: AMBA PL011 UART driver\n");

    /* [STEP 1] Register uart_driver (creates tty_driver) */
    ret = uart_register_driver(&amba_reg);
    if (ret == 0) {
        /* [STEP 2] Register with AMBA bus (triggers probe for each device) */
        ret = amba_driver_register(&pl011_driver);
        if (ret)
            uart_unregister_driver(&amba_reg);
    }
    return ret;
}

/* Probe for each matching AMBA device */
static int pl011_probe(struct amba_device *dev, const struct amba_id *id)
{
    struct uart_amba_port *uap;
    struct vendor_data *vendor = id->data;
    void __iomem *base;
    int i, ret;

    /* [STEP 1] Find free port slot */
    for (i = 0; i < ARRAY_SIZE(amba_ports); i++)
        if (amba_ports[i] == NULL)
            break;
    if (i == ARRAY_SIZE(amba_ports))
        return -EBUSY;

    /* [STEP 2] Allocate private structure */
    uap = kzalloc(sizeof(struct uart_amba_port), GFP_KERNEL);
    if (uap == NULL)
        return -ENOMEM;

    /* [STEP 3] Map registers */
    base = ioremap(dev->res.start, resource_size(&dev->res));
    if (!base) {
        ret = -ENOMEM;
        goto free;
    }

    /* [STEP 4] Get clock */
    uap->clk = clk_get(&dev->dev, NULL);
    if (IS_ERR(uap->clk)) {
        ret = PTR_ERR(uap->clk);
        goto unmap;
    }

    /* [STEP 5] Configure uart_port */
    uap->vendor = vendor;
    uap->lcrh_rx = vendor->lcrh_rx;
    uap->lcrh_tx = vendor->lcrh_tx;
    uap->fifosize = vendor->fifosize;
    uap->port.dev = &dev->dev;
    uap->port.mapbase = dev->res.start;
    uap->port.membase = base;
    uap->port.iotype = UPIO_MEM;
    uap->port.irq = dev->irq[0];
    uap->port.fifosize = uap->fifosize;
    uap->port.ops = &amba_pl011_pops;       /* [KEY] Assign ops */
    uap->port.flags = UPF_BOOT_AUTOCONF;
    uap->port.line = i;

    /* [STEP 6] Setup DMA if available */
    pl011_dma_probe(uap);

    snprintf(uap->type, sizeof(uap->type), "PL011 rev%u", amba_rev(dev));

    /* [STEP 7] Store in global array */
    amba_ports[i] = uap;

    /* [STEP 8] Link to AMBA device */
    amba_set_drvdata(dev, uap);
    
    /* [STEP 9] Register with serial core */
    ret = uart_add_one_port(&amba_reg, &uap->port);
    if (ret) {
        amba_set_drvdata(dev, NULL);
        amba_ports[i] = NULL;
        pl011_dma_remove(uap);
        clk_put(uap->clk);
unmap:
        iounmap(base);
free:
        kfree(uap);
    }
    return ret;
}
```

### A.5 Summary Diagram

```
+-----------------------------------------------------------------------+
|                    PL011 DRIVER ARCHITECTURE                           |
+-----------------------------------------------------------------------+

                            uart_register_driver(&amba_reg)
                                        |
                                        v
+-------------------+          +-------------------+
|   amba_reg        |--------->|   tty_driver      |
|   (uart_driver)   |          | (allocated by     |
|   .driver_name    |          |  serial core)     |
|   .dev_name       |          +-------------------+
|   .nr = 14        |
+-------------------+
         |
         | amba_driver_register(&pl011_driver)
         v
+-------------------+          +-------------------+
|   pl011_driver    |          | AMBA Bus          |
|   (amba_driver)   |--------->| (device matching) |
|   .probe          |          +-------------------+
|   .remove         |                   |
+-------------------+                   |
                                        | For each matching device
                                        v
                            +-------------------+
                            |   pl011_probe()   |
                            +-------------------+
                                        |
         +------------------------------+
         |
         v
+-------------------+          +-------------------+
| uart_amba_port    |          | amba_pl011_pops   |
| +---------------+ |          | (uart_ops)        |
| | uart_port     |<---------->| .startup          |
| | .ops          | |          | .shutdown         |
| | .membase      | |          | .start_tx         |
| +---------------+ |          | .stop_tx          |
| .clk              |          | .set_termios      |
| .im (irq mask)    |          +-------------------+
| .fifosize         |
+-------------------+
         |
         | uart_add_one_port()
         v
+-------------------+
| /dev/ttyAMA0      |  <-- TTY device node
+-------------------+
```

**中文解释：**
- **uart_driver (amba_reg)**：定义驱动家族，包含最大端口数、设备名等
- **amba_driver (pl011_driver)**：AMBA总线驱动，处理探测和移除
- **uart_amba_port**：嵌入uart_port的私有结构，包含硬件特定字段
- **uart_ops (amba_pl011_pops)**：硬件操作函数表
- **注册流程**：先注册uart_driver创建tty_driver，再注册总线驱动，总线匹配时调用probe添加端口

---

## Summary: UART/TTY Subsystem Design Principles

1. **Multi-Layer Abstraction**
   - Line Discipline: Character processing
   - TTY Core: Device management
   - Serial Core: UART-specific abstraction
   - Hardware Driver: Register-level access

2. **Clear Context Boundaries**
   - Sleepable: startup, shutdown, set_termios (optional)
   - IRQ-safe: start_tx, stop_tx, get_mctrl, tx_empty

3. **Ops Pattern for Hardware Abstraction**
   - `uart_ops` defines the hardware contract
   - Serial core calls ops without knowing hardware details
   - Drivers implement ops for their specific hardware

4. **Consistent State Management**
   - `uart_state` links uart_port to tty_port
   - Circular buffer for TX data
   - Flip buffers for RX data

5. **Power-Aware Design**
   - Console polling mode for early boot
   - Proper suspend/resume with TX drain
   - Clock gating when port idle

These principles enable the Linux serial subsystem to support hundreds of different UART implementations with a consistent API and reliable behavior.

---

## Appendix B — Comparing UART vs SPI vs I²C Architectures

### B.1 Fundamental Differences

```
+------------------------------------------------------------------------+
|                    BUS ARCHITECTURE COMPARISON                          |
+------------------------------------------------------------------------+

UART (Point-to-Point)
+----------+                              +----------+
|  Device  |----TX----------------------->|  Device  |
|    A     |<---RX------------------------|    B     |
+----------+                              +----------+
     Only 2 devices, full duplex, no addressing

SPI (Master-Slave with Chip Select)
                    +----------+
                    |  Master  |
                    +----+-----+
                         |
         +-------+-------+-------+
         |       |       |       |
        CS0     CS1     CS2     CS3
         |       |       |       |
    +----v--+ +--v----+ +v-----+ +v-----+
    |Slave 0| |Slave 1| |Slave 2| |Slave 3|
    +-------+ +-------+ +-------+ +-------+
     MOSI/MISO shared, chip select per device, full duplex

I²C (Multi-Master, Shared Bus)
+----------+     +----------+     +----------+
|  Master  |     |  Slave   |     |  Slave   |
|  (0x10)  |     |  (0x50)  |     |  (0x68)  |
+----+-----+     +----+-----+     +----+-----+
     |                |                |
=====+================+================+===== SDA (bidirectional)
=====+================+================+===== SCL (clock)
     2-wire shared bus, 7/10-bit addressing, half duplex
```

**中文解释：**
- **UART**：点对点连接，只有两个设备直接通信，全双工
- **SPI**：主从结构，主机通过片选信号选择从机，支持全双工
- **I²C**：共享总线，设备通过地址区分，半双工

### B.2 Protocol Characteristics

| Characteristic | UART | SPI | I²C |
|---------------|------|-----|-----|
| **Topology** | Point-to-Point | Star (Master + N Slaves) | Multi-drop Bus |
| **Wires** | 2 (TX, RX) + GND | 4 (MOSI, MISO, SCLK, CS) + GND | 2 (SDA, SCL) + GND |
| **Speed** | 9600 - 4 Mbps | 1 - 100+ MHz | 100 kHz - 3.4 MHz |
| **Duplex** | Full | Full | Half |
| **Addressing** | None (point-to-point) | Chip Select lines | 7/10-bit address |
| **Master/Slave** | Peer-to-peer | Single Master | Multi-Master |
| **Distance** | Long (RS-232: 15m+) | Short (< 1m typical) | Short (< 1m) |
| **Clock** | Asynchronous | Synchronous (Master) | Synchronous (Master) |
| **Flow Control** | Hardware (RTS/CTS) or Software (XON/XOFF) | None (synchronous) | Clock stretching |

### B.3 Linux Kernel Architecture Comparison

```
+------------------------------------------------------------------------+
|                    LINUX SUBSYSTEM ARCHITECTURE COMPARISON              |
+------------------------------------------------------------------------+

UART/Serial:
+------------------+     +------------------+     +------------------+
|   TTY Core       |     |   Serial Core    |     |   UART Driver    |
|   tty_io.c       |---->|   serial_core.c  |---->|   amba-pl011.c   |
|   tty_operations |     |   uart_ops       |     |   registers      |
+------------------+     +------------------+     +------------------+
        |
+------------------+
| Line Discipline  |  <-- Extra layer for protocol processing
| n_tty.c, n_ppp.c |
+------------------+

SPI:
+------------------+     +------------------+     +------------------+
|   SPI Core       |     |   SPI Master     |     |   SPI Device     |
|   spi.c          |---->|   spi-pl022.c    |     |   Driver         |
|   spi_transfer   |     |   master->transfer|     |   (e.g., flash)  |
+------------------+     +------------------+     +------------------+
        No line discipline, message-based transfer

I²C:
+------------------+     +------------------+     +------------------+
|   I²C Core       |     |   I²C Adapter    |     |   I²C Client     |
|   i2c-core.c     |---->|   i2c-xxx.c      |     |   Driver         |
|   i2c_algorithm  |     |   algo->xfer     |     |   (e.g., sensor) |
+------------------+     +------------------+     +------------------+
        No line discipline, transaction-based transfer
```

### B.4 Key Structural Differences

| Aspect | UART (Serial) | SPI | I²C |
|--------|---------------|-----|-----|
| **Core Object** | `uart_port` | `spi_master` | `i2c_adapter` |
| **Device Object** | N/A (point-to-point) | `spi_device` | `i2c_client` |
| **Driver Object** | `uart_driver` | `spi_driver` | `i2c_driver` |
| **Ops Table** | `uart_ops` | `spi_master.transfer` | `i2c_algorithm` |
| **Transfer Unit** | Character/Byte | `spi_message` + `spi_transfer` | `i2c_msg` |
| **User Interface** | `/dev/ttyXXX` (char device) | `/dev/spidevX.Y` (optional) | `/dev/i2c-X` (optional) |
| **TTY Layer** | **Yes** (full integration) | No | No |
| **Line Discipline** | **Yes** (N_TTY, N_PPP, etc.) | No | No |
| **Async API** | Interrupt-driven TX/RX | `spi_async()` | N/A (synchronous) |

### B.5 Why UART Has TTY But SPI/I²C Don't

```
+------------------------------------------------------------------------+
|                    WHY UART NEEDS TTY LAYER                             |
+------------------------------------------------------------------------+

UART's Historical Purpose:
    - Terminal communication (login shells)
    - Human-readable character streams
    - Line editing, echo, special characters (Ctrl+C)
    
    User types "ls\n"
         |
         v
    Line Discipline (N_TTY)
    - Echo characters back
    - Handle backspace
    - Buffer until newline
    - Process Ctrl+C (SIGINT)
         |
         v
    Serial Core → UART Driver → Hardware

SPI/I²C's Purpose:
    - Machine-to-machine communication
    - Binary data transfer (sensor readings, flash data)
    - No character processing needed
    
    Kernel driver calls i2c_transfer()
         |
         v
    I²C Core → Adapter → Hardware
    
    No user interaction, no line editing, no TTY!
```

**Key Insight:** TTY exists because UART historically served **human users** at terminals. SPI and I²C are purely **machine interfaces** that don't need character processing.

### B.6 Transfer Model Comparison

```c
/* UART: Character Stream */
write(fd, "Hello\n", 6);  /* User space */
    → uart_write()         /* Copy to circular buffer */
    → start_tx()           /* Enable TX interrupt */
    → ISR: tx_chars()      /* Drain buffer to FIFO */

/* SPI: Message-based */
struct spi_transfer xfer = {
    .tx_buf = tx_data,
    .rx_buf = rx_data,
    .len = 16,
};
spi_sync(spi, &msg);  /* Atomic transfer */

/* I²C: Transaction-based */
struct i2c_msg msgs[2] = {
    { .addr = 0x50, .flags = 0, .len = 1, .buf = &reg },
    { .addr = 0x50, .flags = I2C_M_RD, .len = 4, .buf = data },
};
i2c_transfer(adapter, msgs, 2);  /* Write reg, read data */
```

### B.7 When to Use Each Bus

| Use Case | Recommended Bus | Why |
|----------|-----------------|-----|
| Console/Terminal | UART | Human interaction, line editing |
| GPS module | UART | NMEA sentences, character stream |
| Bluetooth/WiFi | UART (HCI) | Serial protocol, line discipline |
| High-speed flash | SPI | Full duplex, 50+ MHz |
| Display controller | SPI | Fast pixel data transfer |
| Temperature sensor | I²C | Simple, few wires, slow OK |
| EEPROM | I²C | Small data, shared bus |
| Multiple sensors | I²C | One bus, many devices |
| Real-time control | SPI | Deterministic timing |

---

## Appendix C — Writing a Minimal UART Driver Safely

### C.1 Driver Structure Overview

```
+------------------------------------------------------------------------+
|                    MINIMAL UART DRIVER STRUCTURE                        |
+------------------------------------------------------------------------+

my_uart.c
├── Module init/exit
│   ├── uart_register_driver()
│   └── platform_driver_register()
│
├── Platform probe/remove
│   ├── Allocate private structure
│   ├── Configure uart_port
│   └── uart_add_one_port()
│
├── uart_ops implementations
│   ├── startup() / shutdown()      [May sleep]
│   ├── start_tx() / stop_tx()      [IRQ-safe, NO sleep]
│   ├── set_termios()               [Configure hardware]
│   └── tx_empty() / get_mctrl()    [Status queries]
│
└── Interrupt handler
    ├── TX: Drain circular buffer to FIFO
    └── RX: Read FIFO to flip buffer
```

### C.2 Complete Minimal Driver Example

```c
/*
 * Minimal UART Driver Template
 * 
 * This is a safe, minimal implementation that demonstrates:
 * - Correct locking patterns
 * - Proper resource management
 * - Context-aware callback implementation
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/io.h>
#include <linux/clk.h>

#define DRIVER_NAME     "my_uart"
#define DEV_NAME        "ttyMY"
#define UART_NR         4

/* Register offsets (example) */
#define UART_DATA       0x00
#define UART_STATUS     0x04
#define UART_CTRL       0x08
#define UART_BAUD       0x0C

/* Status register bits */
#define STAT_TX_EMPTY   (1 << 0)
#define STAT_RX_READY   (1 << 1)
#define STAT_TX_FULL    (1 << 2)

/* Control register bits */
#define CTRL_TX_EN      (1 << 0)
#define CTRL_RX_EN      (1 << 1)
#define CTRL_TX_IRQ     (1 << 2)
#define CTRL_RX_IRQ     (1 << 3)

/*
 * Private driver structure
 * [KEY] Embed uart_port as FIRST field for safe casting
 */
struct my_uart_port {
    struct uart_port    port;       /* [MUST BE FIRST] */
    struct clk          *clk;       /* Clock control */
    unsigned int        ctrl_reg;   /* Shadow of control register */
};

/* Forward declarations */
static struct uart_driver my_uart_driver;

/*
 * Register access helpers
 * [KEY] Use these consistently for maintainability
 */
static inline u32 my_uart_read(struct uart_port *port, unsigned int reg)
{
    return readl(port->membase + reg);
}

static inline void my_uart_write(struct uart_port *port, unsigned int reg, u32 val)
{
    writel(val, port->membase + reg);
}

/*
 * [CALLBACK] Check if TX FIFO is empty
 * Context: Called with port->lock held
 * Contract: MUST NOT sleep
 */
static unsigned int my_uart_tx_empty(struct uart_port *port)
{
    u32 status = my_uart_read(port, UART_STATUS);
    return (status & STAT_TX_EMPTY) ? TIOCSER_TEMT : 0;
}

/*
 * [CALLBACK] Set modem control lines (RTS, DTR)
 * Context: Called with port->lock held
 * Contract: MUST NOT sleep
 */
static void my_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
    /* 
     * [EXAMPLE] If hardware supports RTS/DTR:
     * u32 ctrl = my_uart_read(port, UART_CTRL);
     * if (mctrl & TIOCM_RTS) ctrl |= CTRL_RTS;
     * else ctrl &= ~CTRL_RTS;
     * my_uart_write(port, UART_CTRL, ctrl);
     */
}

/*
 * [CALLBACK] Get modem status (CTS, DCD, DSR, RI)
 * Context: Called with port->lock held
 * Contract: MUST NOT sleep
 */
static unsigned int my_uart_get_mctrl(struct uart_port *port)
{
    unsigned int mctrl = 0;
    
    /* [EXAMPLE] Read hardware status and translate */
    /* u32 status = my_uart_read(port, UART_STATUS);
     * if (status & STAT_CTS) mctrl |= TIOCM_CTS;
     * if (status & STAT_DCD) mctrl |= TIOCM_CAR;
     */
    
    /* [SAFE DEFAULT] Report all lines asserted if not implemented */
    mctrl = TIOCM_CTS | TIOCM_CAR | TIOCM_DSR;
    return mctrl;
}

/*
 * [CALLBACK] Stop transmission
 * Context: Called with port->lock held (often from ISR)
 * Contract: MUST NOT sleep, just disable TX interrupt
 */
static void my_uart_stop_tx(struct uart_port *port)
{
    struct my_uart_port *up = (struct my_uart_port *)port;
    
    /* [KEY] Disable TX interrupt */
    up->ctrl_reg &= ~CTRL_TX_IRQ;
    my_uart_write(port, UART_CTRL, up->ctrl_reg);
}

/*
 * [CALLBACK] Start transmission
 * Context: Called with port->lock held
 * Contract: MUST NOT sleep, just enable TX interrupt
 */
static void my_uart_start_tx(struct uart_port *port)
{
    struct my_uart_port *up = (struct my_uart_port *)port;
    
    /* [KEY] Enable TX interrupt - ISR will handle actual TX */
    up->ctrl_reg |= CTRL_TX_IRQ;
    my_uart_write(port, UART_CTRL, up->ctrl_reg);
}

/*
 * [CALLBACK] Stop reception
 * Context: Called with port->lock held
 * Contract: MUST NOT sleep
 */
static void my_uart_stop_rx(struct uart_port *port)
{
    struct my_uart_port *up = (struct my_uart_port *)port;
    
    up->ctrl_reg &= ~CTRL_RX_IRQ;
    my_uart_write(port, UART_CTRL, up->ctrl_reg);
}

/*
 * [ISR HELPER] Handle TX - drain circular buffer to hardware FIFO
 * Context: IRQ, port->lock held
 */
static void my_uart_tx_chars(struct my_uart_port *up)
{
    struct uart_port *port = &up->port;
    struct circ_buf *xmit = &port->state->xmit;
    int count = 16;  /* FIFO size or reasonable limit */
    
    /* [PRIORITY] Handle x_char (XON/XOFF) first */
    if (port->x_char) {
        my_uart_write(port, UART_DATA, port->x_char);
        port->icount.tx++;
        port->x_char = 0;
        return;
    }
    
    /* [CHECK] Stop if nothing to send or TX stopped */
    if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
        my_uart_stop_tx(port);
        return;
    }
    
    /* [TRANSFER] Move data from circular buffer to FIFO */
    while (count-- > 0) {
        /* Check FIFO not full */
        if (my_uart_read(port, UART_STATUS) & STAT_TX_FULL)
            break;
            
        my_uart_write(port, UART_DATA, xmit->buf[xmit->tail]);
        xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
        port->icount.tx++;
        
        if (uart_circ_empty(xmit))
            break;
    }
    
    /* [WAKEUP] Notify writers if buffer low */
    if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
        uart_write_wakeup(port);
    
    /* [STOP] Disable TX IRQ if buffer empty */
    if (uart_circ_empty(xmit))
        my_uart_stop_tx(port);
}

/*
 * [ISR HELPER] Handle RX - read hardware FIFO to flip buffer
 * Context: IRQ, port->lock held
 */
static void my_uart_rx_chars(struct my_uart_port *up)
{
    struct uart_port *port = &up->port;
    struct tty_struct *tty = port->state->port.tty;
    int max_count = 256;  /* Prevent infinite loop */
    
    while (max_count-- > 0) {
        u32 status = my_uart_read(port, UART_STATUS);
        unsigned int ch, flag;
        
        /* [CHECK] Exit if FIFO empty */
        if (!(status & STAT_RX_READY))
            break;
        
        /* [READ] Get character from FIFO */
        ch = my_uart_read(port, UART_DATA) & 0xFF;
        flag = TTY_NORMAL;
        port->icount.rx++;
        
        /* [SYSRQ] Handle magic sysrq */
        if (uart_handle_sysrq_char(port, ch))
            continue;
        
        /* [INSERT] Add to flip buffer */
        uart_insert_char(port, 0, 0, ch, flag);
    }
    
    /* [PUSH] Trigger flip buffer processing */
    tty_flip_buffer_push(tty);
}

/*
 * [ISR] Main interrupt handler
 * Context: Hardirq
 */
static irqreturn_t my_uart_interrupt(int irq, void *dev_id)
{
    struct my_uart_port *up = dev_id;
    struct uart_port *port = &up->port;
    u32 status;
    int handled = 0;
    
    /* [LOCK] Acquire port lock for entire ISR */
    spin_lock(&port->lock);
    
    status = my_uart_read(port, UART_STATUS);
    
    /* [RX] Handle receive */
    if (status & STAT_RX_READY) {
        my_uart_rx_chars(up);
        handled = 1;
    }
    
    /* [TX] Handle transmit */
    if (status & STAT_TX_EMPTY) {
        my_uart_tx_chars(up);
        handled = 1;
    }
    
    spin_unlock(&port->lock);
    
    return IRQ_RETVAL(handled);
}

/*
 * [CALLBACK] Port startup - called when port is opened
 * Context: Process, MAY SLEEP
 * Contract: Allocate resources, enable hardware
 */
static int my_uart_startup(struct uart_port *port)
{
    struct my_uart_port *up = (struct my_uart_port *)port;
    int ret;
    
    /* [STEP 1] Enable clock (may sleep) */
    ret = clk_prepare_enable(up->clk);
    if (ret)
        return ret;
    
    /* [STEP 2] Request IRQ (may sleep) */
    ret = request_irq(port->irq, my_uart_interrupt, 0, 
                      DRIVER_NAME, up);
    if (ret) {
        clk_disable_unprepare(up->clk);
        return ret;
    }
    
    /* [STEP 3] Enable RX interrupt */
    up->ctrl_reg = CTRL_TX_EN | CTRL_RX_EN | CTRL_RX_IRQ;
    my_uart_write(port, UART_CTRL, up->ctrl_reg);
    
    return 0;
}

/*
 * [CALLBACK] Port shutdown - called when port is closed
 * Context: Process, MAY SLEEP
 * Contract: Disable hardware, free resources
 */
static void my_uart_shutdown(struct uart_port *port)
{
    struct my_uart_port *up = (struct my_uart_port *)port;
    unsigned long flags;
    
    /* [STEP 1] Disable interrupts under lock */
    spin_lock_irqsave(&port->lock, flags);
    up->ctrl_reg = 0;
    my_uart_write(port, UART_CTRL, up->ctrl_reg);
    spin_unlock_irqrestore(&port->lock, flags);
    
    /* [STEP 2] Free IRQ (may sleep) */
    free_irq(port->irq, up);
    
    /* [STEP 3] Disable clock */
    clk_disable_unprepare(up->clk);
}

/*
 * [CALLBACK] Configure termios settings
 * Context: Process
 * Contract: Configure baud rate, data bits, parity, etc.
 */
static void my_uart_set_termios(struct uart_port *port,
                                struct ktermios *termios,
                                struct ktermios *old)
{
    unsigned int baud, quot;
    unsigned long flags;
    
    /* [STEP 1] Calculate baud rate */
    baud = uart_get_baud_rate(port, termios, old, 1200, 115200);
    quot = uart_get_divisor(port, baud);
    
    /* [STEP 2] Update under lock */
    spin_lock_irqsave(&port->lock, flags);
    
    /* [KEY] Update timeout for uart_wait_until_sent() */
    uart_update_timeout(port, termios->c_cflag, baud);
    
    /* Write baud divisor */
    my_uart_write(port, UART_BAUD, quot);
    
    /* [EXAMPLE] Handle data bits, parity, stop bits */
    /* u32 lcr = 0;
     * switch (termios->c_cflag & CSIZE) {
     *     case CS5: lcr |= LCR_5BIT; break;
     *     case CS6: lcr |= LCR_6BIT; break;
     *     case CS7: lcr |= LCR_7BIT; break;
     *     default:  lcr |= LCR_8BIT; break;
     * }
     * if (termios->c_cflag & CSTOPB) lcr |= LCR_STOP2;
     * if (termios->c_cflag & PARENB) {
     *     lcr |= LCR_PARITY;
     *     if (!(termios->c_cflag & PARODD)) lcr |= LCR_EVEN;
     * }
     */
    
    spin_unlock_irqrestore(&port->lock, flags);
}

/*
 * [CALLBACK] Return port type string
 * Context: Any
 */
static const char *my_uart_type(struct uart_port *port)
{
    return "MY_UART";
}

/*
 * [CALLBACK] Release I/O resources
 * Context: Process, MAY SLEEP
 */
static void my_uart_release_port(struct uart_port *port)
{
    /* [KEY] iounmap is handled in platform remove */
}

/*
 * [CALLBACK] Request I/O resources
 * Context: Process, MAY SLEEP
 */
static int my_uart_request_port(struct uart_port *port)
{
    /* [KEY] ioremap is handled in platform probe */
    return 0;
}

/*
 * [CALLBACK] Configure port type
 * Context: Process, MAY SLEEP
 */
static void my_uart_config_port(struct uart_port *port, int flags)
{
    if (flags & UART_CONFIG_TYPE)
        port->type = PORT_16550A;  /* Use appropriate type */
}

/*
 * [CALLBACK] Verify port settings
 * Context: Process
 */
static int my_uart_verify_port(struct uart_port *port,
                               struct serial_struct *ser)
{
    if (ser->type != PORT_UNKNOWN && ser->type != PORT_16550A)
        return -EINVAL;
    return 0;
}

/*
 * [UART OPS] The operations table
 */
static struct uart_ops my_uart_ops = {
    .tx_empty       = my_uart_tx_empty,
    .set_mctrl      = my_uart_set_mctrl,
    .get_mctrl      = my_uart_get_mctrl,
    .stop_tx        = my_uart_stop_tx,
    .start_tx       = my_uart_start_tx,
    .stop_rx        = my_uart_stop_rx,
    .startup        = my_uart_startup,
    .shutdown       = my_uart_shutdown,
    .set_termios    = my_uart_set_termios,
    .type           = my_uart_type,
    .release_port   = my_uart_release_port,
    .request_port   = my_uart_request_port,
    .config_port    = my_uart_config_port,
    .verify_port    = my_uart_verify_port,
};

/*
 * [UART DRIVER] The driver registration structure
 */
static struct uart_driver my_uart_driver = {
    .owner          = THIS_MODULE,
    .driver_name    = DRIVER_NAME,
    .dev_name       = DEV_NAME,
    .major          = 0,            /* [KEY] Dynamic major number */
    .minor          = 0,
    .nr             = UART_NR,
    .cons           = NULL,         /* No console support in minimal driver */
};

/*
 * [PLATFORM PROBE] Called when device is found
 */
static int my_uart_probe(struct platform_device *pdev)
{
    struct my_uart_port *up;
    struct resource *res;
    int ret;
    
    /* [STEP 1] Allocate private structure */
    up = devm_kzalloc(&pdev->dev, sizeof(*up), GFP_KERNEL);
    if (!up)
        return -ENOMEM;
    
    /* [STEP 2] Get memory resource */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res)
        return -ENODEV;
    
    /* [STEP 3] Map registers */
    up->port.membase = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(up->port.membase))
        return PTR_ERR(up->port.membase);
    
    /* [STEP 4] Get IRQ */
    up->port.irq = platform_get_irq(pdev, 0);
    if (up->port.irq < 0)
        return up->port.irq;
    
    /* [STEP 5] Get clock (optional) */
    up->clk = devm_clk_get(&pdev->dev, NULL);
    if (IS_ERR(up->clk))
        up->clk = NULL;  /* Clock may be optional */
    
    /* [STEP 6] Configure uart_port */
    up->port.dev = &pdev->dev;
    up->port.mapbase = res->start;
    up->port.iotype = UPIO_MEM;
    up->port.ops = &my_uart_ops;
    up->port.flags = UPF_BOOT_AUTOCONF;
    up->port.line = pdev->id;           /* Port index */
    up->port.fifosize = 16;
    up->port.uartclk = 48000000;        /* 48 MHz example */
    
    /* [STEP 7] Link to platform device */
    platform_set_drvdata(pdev, up);
    
    /* [STEP 8] Register with serial core (LAST) */
    ret = uart_add_one_port(&my_uart_driver, &up->port);
    if (ret) {
        dev_err(&pdev->dev, "Failed to add port: %d\n", ret);
        return ret;
    }
    
    dev_info(&pdev->dev, "Registered port %d\n", up->port.line);
    return 0;
}

/*
 * [PLATFORM REMOVE] Called when device is removed
 */
static int my_uart_remove(struct platform_device *pdev)
{
    struct my_uart_port *up = platform_get_drvdata(pdev);
    
    /* [KEY] Unregister from serial core FIRST */
    uart_remove_one_port(&my_uart_driver, &up->port);
    
    /* [KEY] devm_ resources are freed automatically */
    
    return 0;
}

/*
 * [PLATFORM DRIVER] Device matching and callbacks
 */
static const struct of_device_id my_uart_of_match[] = {
    { .compatible = "vendor,my-uart" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_uart_of_match);

static struct platform_driver my_uart_platform_driver = {
    .probe  = my_uart_probe,
    .remove = my_uart_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = my_uart_of_match,
    },
};

/*
 * [MODULE INIT]
 */
static int __init my_uart_init(void)
{
    int ret;
    
    /* [STEP 1] Register uart_driver FIRST */
    ret = uart_register_driver(&my_uart_driver);
    if (ret)
        return ret;
    
    /* [STEP 2] Register platform driver */
    ret = platform_driver_register(&my_uart_platform_driver);
    if (ret) {
        uart_unregister_driver(&my_uart_driver);
        return ret;
    }
    
    pr_info("my_uart: driver loaded\n");
    return 0;
}

/*
 * [MODULE EXIT]
 */
static void __exit my_uart_exit(void)
{
    /* [KEY] Unregister in REVERSE order */
    platform_driver_unregister(&my_uart_platform_driver);
    uart_unregister_driver(&my_uart_driver);
    
    pr_info("my_uart: driver unloaded\n");
}

module_init(my_uart_init);
module_exit(my_uart_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Minimal UART Driver Template");
```

### C.3 Safety Checklist

| Checkpoint | Verification |
|------------|--------------|
| **Embedding** | `uart_port` is first field in private struct |
| **Locking** | `start_tx`/`stop_tx`/`get_mctrl` never sleep |
| **Resources** | `devm_*` functions used for auto-cleanup |
| **Order** | `uart_register_driver` before `platform_driver_register` |
| **Cleanup** | `uart_remove_one_port` before freeing memory |
| **ISR** | Always acquires `port->lock`, handles spurious IRQs |
| **TX** | Checks `uart_circ_empty()` and `uart_tx_stopped()` |
| **RX** | Uses `uart_insert_char()` and `tty_flip_buffer_push()` |
| **Termios** | Calls `uart_update_timeout()` |

### C.4 Common Pitfalls to Avoid

```c
/* PITFALL 1: Sleeping in start_tx */
static void bad_start_tx(struct uart_port *port)
{
    mutex_lock(&my_mutex);  /* WRONG: start_tx called with spinlock held! */
    /* ... */
}

/* PITFALL 2: Forgetting to update xmit.tail in TX */
static void bad_tx_chars(struct uart_port *port)
{
    struct circ_buf *xmit = &port->state->xmit;
    my_uart_write(port, UART_DATA, xmit->buf[xmit->tail]);
    /* WRONG: Forgot to advance tail! Infinite TX of same byte */
}

/* PITFALL 3: Reading FIFO without checking status */
static void bad_rx_chars(struct uart_port *port)
{
    while (1) {  /* WRONG: No termination condition! */
        ch = my_uart_read(port, UART_DATA);
        uart_insert_char(port, 0, 0, ch, TTY_NORMAL);
    }
}

/* PITFALL 4: Wrong unregister order */
static void __exit bad_exit(void)
{
    uart_unregister_driver(&my_uart_driver);  /* WRONG: Ports still exist! */
    platform_driver_unregister(&my_uart_platform_driver);
}
```

---

## Appendix D — Why TTY Is Structured the Way It Is

### D.1 Historical Context

```
+------------------------------------------------------------------------+
|                    EVOLUTION OF TERMINALS                               |
+------------------------------------------------------------------------+

1960s: Teletype Machines (TTY)
+-------------+                           +-------------+
| Mainframe   |===== Serial Line ========>| Teletype    |
| Computer    |<==== Serial Line =========| Machine     |
+-------------+                           +-------------+
                                          - Mechanical printer
                                          - Keyboard input
                                          - 10 characters/second

1970s: Video Display Terminals (VDT)
+-------------+                           +-------------+
| Unix System |===== RS-232 =============>| VT100       |
|             |<==== RS-232 ==============| Terminal    |
+-------------+                           +-------------+
                                          - CRT display
                                          - 9600 baud
                                          - Escape sequences

1980s-Today: Terminal Emulators
+-------------+     +-------------+     +-------------+
| Application |<--->| Pseudo-TTY  |<--->| xterm/gnome |
| (bash)      |     | (pty)       |     | -terminal   |
+-------------+     +-------------+     +-------------+
                    Same interface!      Software terminal
```

**Key Insight:** The TTY subsystem preserved the **same interface** as physical terminals evolved into software emulators. This is why `/dev/ttyXXX` looks the same whether it's a hardware UART, a pseudo-terminal, or a USB serial adapter.

### D.2 Why Three Layers (Line Discipline, TTY Core, Serial Core)?

```
+------------------------------------------------------------------------+
|                    THE THREE-LAYER RATIONALE                            |
+------------------------------------------------------------------------+

Q: Why not just have drivers directly expose /dev/ttyXXX?

PROBLEM 1: Every driver would duplicate line editing
    +------------+     +------------+     +------------+
    | 8250       |     | PL011      |     | USB Serial |
    | - echo     |     | - echo     |     | - echo     |
    | - backspace|     | - backspace|     | - backspace|
    | - Ctrl+C   |     | - Ctrl+C   |     | - Ctrl+C   |
    +------------+     +------------+     +------------+
    DUPLICATION!

SOLUTION: Extract common code into Line Discipline
    +------------+     +------------+     +------------+
    | 8250       |     | PL011      |     | USB Serial |
    | (HW only)  |     | (HW only)  |     | (HW only)  |
    +------------+     +------------+     +------------+
           |                 |                 |
           +-----------------+-----------------+
                             |
                    +--------v--------+
                    | Line Discipline |
                    | (N_TTY)         |
                    | - echo          |
                    | - line editing  |
                    | - signals       |
                    +-----------------+
    SHARED!

PROBLEM 2: Different protocols over same hardware
    UART can carry: plain text, PPP, SLIP, Bluetooth HCI, GPS NMEA...
    
    If line editing is hardcoded, we can't switch protocols!

SOLUTION: Pluggable Line Disciplines
    +------------------+
    | N_TTY            | <-- Normal terminal
    +------------------+
    | N_PPP            | <-- PPP daemon
    +------------------+
    | N_SLIP           | <-- SLIP networking
    +------------------+
    | N_HCI            | <-- Bluetooth stack
    +------------------+
           |
    User switches via: ioctl(TIOCSETD, &ldisc)

PROBLEM 3: UARTs share common logic
    - Baud rate calculation
    - Modem control handling
    - Suspend/resume
    - Console integration

SOLUTION: Serial Core layer
    +-------------------+
    | serial_core.c     | <-- Common UART logic
    +-------------------+
    | - uart_set_termios|
    | - uart_suspend    |
    | - uart_console    |
    +-------------------+
           |
    +------+------+
    |             |
+-------+    +-------+
| 8250  |    | PL011 |
+-------+    +-------+
(register)   (register)
```

### D.3 The Layered Responsibilities

| Layer | Primary Responsibility | Example Functions |
|-------|----------------------|-------------------|
| **Line Discipline** | Protocol processing | `n_tty_read()`, `n_tty_receive_buf()` |
| **TTY Core** | Device management | `tty_open()`, `tty_write()`, `tty_ioctl()` |
| **Serial Core** | UART abstraction | `uart_write()`, `uart_set_termios()` |
| **UART Driver** | Hardware access | `pl011_start_tx()`, `pl011_set_termios()` |

### D.4 Why Line Disciplines Are Brilliant

```
+------------------------------------------------------------------------+
|                    LINE DISCIPLINE FLEXIBILITY                          |
+------------------------------------------------------------------------+

Same Hardware, Different Protocols:

CASE 1: Interactive Shell
    User → bash → pty → getty → UART → Physical Terminal
                        |
                  +-----v-----+
                  | N_TTY     |  Echo, line edit, Ctrl+C
                  +-----------+

CASE 2: PPP Connection (dial-up)
    IP Packets → pppd → UART
                   |
             +-----v-----+
             | N_PPP     |  HDLC framing, no echo
             +-----------+

CASE 3: Bluetooth HCI
    Bluetooth Stack → hciattach → UART
                          |
                    +-----v-----+
                    | N_HCI     |  HCI packets, binary
                    +-----------+

CASE 4: GPS Receiver
    gpsd → UART
           |
     +-----v-----+
     | N_TTY     |  Raw NMEA sentences (or custom ldisc)
     +-----------+

ALL USE THE SAME UART HARDWARE!
```

**Key Insight:** Without line disciplines, you'd need separate device nodes (`/dev/ppp0`, `/dev/gps0`, etc.) and duplicate buffering code. With line disciplines, you get `/dev/ttyUSB0` that can do *anything*.

### D.5 Why TTY Core Exists Separately

```
+------------------------------------------------------------------------+
|                    TTY CORE'S UNIVERSAL INTERFACE                       |
+------------------------------------------------------------------------+

TTY Core provides uniform interface for ALL terminal-like devices:

+-------------------+     +-------------------+     +-------------------+
| Physical UART     |     | USB Serial        |     | Pseudo-Terminal   |
| (/dev/ttyS0)      |     | (/dev/ttyUSB0)    |     | (/dev/pts/0)      |
+-------------------+     +-------------------+     +-------------------+
         |                         |                         |
         +-----------+-------------+-----------+-------------+
                     |                         |
              +------v------+           +------v------+
              | TTY Core    |           | TTY Core    |
              | (tty_io.c)  |           | (pty.c)     |
              +-------------+           +-------------+
                     |                         |
              +------v------+           +------v------+
              |Serial Core  |           | (none)      |
              |(optional)   |           |             |
              +-------------+           +-------------+

User space sees identical interface:
    open("/dev/ttyS0", ...)     open("/dev/pts/0", ...)
    tcsetattr(fd, ...)          tcsetattr(fd, ...)
    read(fd, ...)               read(fd, ...)
    write(fd, ...)              write(fd, ...)

SAME API regardless of underlying device!
```

### D.6 Why Not Merge Serial Core into TTY Core?

| Concern | Keeping Separate | If Merged |
|---------|-----------------|-----------|
| **Pseudo-terminals** | No serial code needed | Would load unnecessary code |
| **USB Serial** | Uses different stack | Would need special cases |
| **Baud rate logic** | UART-specific | Pollutes generic TTY |
| **Modem control** | RS-232 concept | Meaningless for pty |
| **Console handling** | Early boot UART | Pty never a console |

**Conclusion:** Serial Core isolates UART-specific concepts that don't apply to pseudo-terminals, virtual consoles, or USB serial (which has its own stack).

### D.7 The Full Picture

```
+------------------------------------------------------------------------+
|                    COMPLETE TTY ARCHITECTURE RATIONALE                  |
+------------------------------------------------------------------------+

                         USER SPACE
                              |
                         open("/dev/ttyS0")
                              |
                              v
+------------------------------------------------------------------------+
|                         VFS LAYER                                       |
|                    (file operations)                                    |
+------------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------------+
|                        TTY CORE                                         |
|  WHY: Unified device model for all terminal-like devices                |
|  - Manages tty_struct lifecycle                                         |
|  - Handles open/close/ioctl uniformly                                   |
|  - Provides hangup detection, job control                               |
+------------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------------+
|                     LINE DISCIPLINE                                     |
|  WHY: Separates protocol from transport                                 |
|  - N_TTY: Line editing, echo, signals for interactive shells            |
|  - N_PPP: HDLC framing for network protocol                             |
|  - N_SLIP: Simple IP encapsulation                                      |
|  - Swappable at runtime!                                                |
+------------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------------+
|                      SERIAL CORE                                        |
|  WHY: Common UART logic shared across 80+ drivers                       |
|  - Baud rate calculation (uart_get_baud_rate)                           |
|  - Modem signal handling (uart_handle_cts_change)                       |
|  - Console infrastructure (uart_console_write)                          |
|  - Power management (uart_suspend_port)                                 |
+------------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------------+
|                      UART DRIVER                                        |
|  WHY: Hardware-specific register access                                 |
|  - 8250.c: x86 PC standard UARTs                                        |
|  - amba-pl011.c: ARM SoC UARTs                                          |
|  - Only knows about THIS hardware's registers                           |
+------------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------------+
|                       HARDWARE                                          |
+------------------------------------------------------------------------+
```

**中文解释：**
- **VFS层**：提供文件操作抽象（open、read、write）
- **TTY核心**：统一管理所有终端类设备，处理设备生命周期
- **线路规程**：分离协议处理和底层传输，可运行时切换
- **串口核心**：共享UART通用逻辑，避免80多个驱动重复代码
- **UART驱动**：只关心特定硬件的寄存器访问

### D.8 Design Lessons from TTY Architecture

| Principle | TTY Implementation | General Lesson |
|-----------|-------------------|----------------|
| **Separation of Concerns** | Line discipline vs Serial core | Separate policy from mechanism |
| **Runtime Configurability** | Pluggable line disciplines | Use strategy pattern for variants |
| **Backward Compatibility** | 50+ year old API still works | Stable interfaces enable longevity |
| **Layered Abstraction** | 4 distinct layers | Each layer hides complexity below |
| **Common Code Extraction** | Serial core's 2500 lines | DRY principle at subsystem level |

### D.9 What Would Break Without This Structure?

```
IF NO LINE DISCIPLINE:
    - Every driver implements echo, line editing
    - Can't use same UART for different protocols
    - 80+ drivers × N protocols = chaos

IF NO TTY CORE:
    - Pseudo-terminals would be different from UARTs
    - Applications would need to know device type
    - No unified job control, session management

IF NO SERIAL CORE:
    - Each driver implements baud calculation
    - Duplicated suspend/resume logic
    - Console support reimplemented 80+ times

THE STRUCTURE EXISTS BECAUSE IT PREVENTS MASSIVE DUPLICATION
AND ENABLES FLEXIBILITY THAT HAS LASTED 50+ YEARS.
```

---

## Summary: Supplementary Appendices

### B. UART vs SPI vs I²C
- UART is point-to-point, asynchronous, for human/machine streams
- SPI is master-slave, synchronous, for high-speed transfers
- I²C is shared bus, synchronous, for simple sensor communication
- UART needs TTY because of terminal history; SPI/I²C don't

### C. Minimal UART Driver
- Embed `uart_port` as first field for safe casting
- `start_tx`/`stop_tx` MUST NOT sleep (spinlock held)
- Use `devm_*` for automatic cleanup
- Register order: `uart_register_driver` → `platform_driver_register`

### D. Why TTY Structure
- **Line Discipline**: Separates protocol from hardware (N_TTY, N_PPP)
- **TTY Core**: Unified interface for all terminal devices
- **Serial Core**: Common UART logic shared by 80+ drivers
- **Result**: 50+ years of backward compatibility
