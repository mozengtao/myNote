# Console 子系统中的依赖注入 (IoC) 模式

## 概述

Linux 控制台子系统是另一个经典的 IoC 实现。内核的 `printk` 输出到哪里完全由注册的 console 驱动决定，且 console 驱动可以在运行时动态添加/移除。

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     Console 子系统 IoC 架构                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   内核各模块                                                                 │
│   ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐                          │
│   │ 调度器   │ │ 网络栈  │ │ 文件系统│  │  驱动   │                          │
│   └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘                          │
│        │           │           │           │                                 │
│        └───────────┴───────────┴───────────┘                                 │
│                          │                                                   │
│                          ▼                                                   │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                         printk()                                     │   │
│   │                                                                      │   │
│   │   日志缓冲区 (ring buffer)                                           │   │
│   │   ┌──────────────────────────────────────────────────────────────┐  │   │
│   │   │ msg1 | msg2 | msg3 | msg4 | msg5 | ...                      │  │   │
│   │   └──────────────────────────────────────────────────────────────┘  │   │
│   │                          │                                           │   │
│   │                          ▼                                           │   │
│   │   ┌──────────────────────────────────────────────────────────────┐  │   │
│   │   │              遍历 console_drivers 链表                        │  │   │
│   │   │                                                               │  │   │
│   │   │   for_each_console(con):                                      │  │   │
│   │   │       con->write(con, msg, len)   ◄── 调用注入的 write 函数  │  │   │
│   │   │                                                               │  │   │
│   │   └──────────────────────────────────────────────────────────────┘  │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│                                   │                                          │
│           ┌───────────────────────┼───────────────────────┐                 │
│           │                       │                       │                  │
│           ▼                       ▼                       ▼                  │
│   ┌───────────────┐       ┌───────────────┐       ┌───────────────┐         │
│   │  串口 Console │       │  VGA Console  │       │  网络 Console │         │
│   │               │       │               │       │  (netconsole) │         │
│   │ .write = xxx  │       │ .write = xxx  │       │ .write = xxx  │         │
│   │ .setup = xxx  │       │ .setup = xxx  │       │ .setup = xxx  │         │
│   └───────────────┘       └───────────────┘       └───────────────┘         │
│                                                                              │
│   控制反转体现:                                                              │
│   - printk 不知道消息输出到哪里                                              │
│   - 具体输出设备通过 register_console() 动态注入                            │
│   - 可以同时存在多个 console，由框架依次调用                                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 核心代码片段

### 1. console 结构 - 控制台的"接口契约"

```c
// include/linux/console.h

#define CON_PRINTBUFFER (1)     // 注册时打印缓冲区内容
#define CON_CONSDEV     (2)     // 首选控制台
#define CON_ENABLED     (4)     // 已启用
#define CON_BOOT        (8)     // 早期启动控制台
#define CON_ANYTIME     (16)    // 可在 CPU 离线时使用
#define CON_BRL         (32)    // 盲文设备

struct console {
    char    name[16];               // 控制台名称 (如 "ttyS", "tty")
    
    // 核心回调 - 输出函数 (必须实现)
    void    (*write)(struct console *, const char *, unsigned);
    
    // 可选回调 - 读取函数
    int     (*read)(struct console *, char *, unsigned);
    
    // 获取关联的 tty 驱动
    struct tty_driver *(*device)(struct console *, int *);
    
    // 解除屏幕保护
    void    (*unblank)(void);
    
    // 初始化设置 (注册时调用)
    int     (*setup)(struct console *, char *);
    
    // 早期设置
    int     (*early_setup)(void);
    
    short   flags;                  // 控制台标志
    short   index;                  // 控制台索引 (如 ttyS0 的 0)
    int     cflag;                  // 终端配置
    void    *data;                  // 私有数据
    struct  console *next;          // 链表指针
};
```

**说明**: `console` 结构定义了输出接口，具体的控制台驱动通过实现 `write` 等函数来"注入"自己的输出方式。

---

### 2. register_console() - 依赖注入的入口

```c
// kernel/printk.c

// 全局控制台驱动链表头
struct console *console_drivers;
EXPORT_SYMBOL_GPL(console_drivers);

void register_console(struct console *newcon)
{
    int i;
    unsigned long flags;
    struct console *bcon = NULL;

    // 如果已有真正的控制台，拒绝新的 boot console
    if (console_drivers && newcon->flags & CON_BOOT) {
        for_each_console(bcon) {
            if (!(bcon->flags & CON_BOOT)) {
                printk(KERN_INFO "Too late to register bootconsole %s%d\n",
                    newcon->name, newcon->index);
                return;
            }
        }
    }

    if (console_drivers && console_drivers->flags & CON_BOOT)
        bcon = console_drivers;

    // 调用早期设置回调
    if (newcon->early_setup)
        newcon->early_setup();

    // 匹配命令行参数，调用 setup 回调
    for (i = 0; i < MAX_CMDLINECONSOLES && console_cmdline[i].name[0]; i++) {
        if (strcmp(console_cmdline[i].name, newcon->name) != 0)
            continue;
        if (newcon->index >= 0 &&
            newcon->index != console_cmdline[i].index)
            continue;
        if (newcon->index < 0)
            newcon->index = console_cmdline[i].index;

        // 调用驱动注入的 setup 函数
        if (newcon->setup &&
            newcon->setup(newcon, console_cmdline[i].options) != 0)
            break;
            
        newcon->flags |= CON_ENABLED;
        newcon->index = console_cmdline[i].index;
        if (i == selected_console) {
            newcon->flags |= CON_CONSDEV;
            preferred_console = selected_console;
        }
        break;
    }

    if (!(newcon->flags & CON_ENABLED))
        return;

    // 将新控制台添加到链表
    console_lock();
    if ((newcon->flags & CON_CONSDEV) || console_drivers == NULL) {
        newcon->next = console_drivers;
        console_drivers = newcon;
    } else {
        newcon->next = console_drivers->next;
        console_drivers->next = newcon;
    }
    
    // 如果设置了 CON_PRINTBUFFER，打印缓冲区中的历史消息
    if (newcon->flags & CON_PRINTBUFFER) {
        // 调用 newcon->write() 输出历史消息
        // ...
    }
    console_unlock();

    printk(KERN_INFO "%sconsole [%s%d] enabled\n",
        (newcon->flags & CON_BOOT) ? "boot" : "",
        newcon->name, newcon->index);
}
EXPORT_SYMBOL(register_console);
```

---

### 3. printk 如何使用注入的 console

```c
// kernel/printk.c (简化版)

static void call_console_drivers(unsigned start, unsigned end)
{
    struct console *con;

    // 遍历所有注册的 console
    for_each_console(con) {
        if ((con->flags & CON_ENABLED) && con->write) {
            // 调用每个 console 注入的 write 函数
            con->write(con, &LOG_BUF(start), end - start);
        }
    }
}
```

---

### 4. 串口 Console 示例 - 注入实现

```c
// drivers/tty/serial/8250/8250.c

static void serial8250_console_write(struct console *co, 
                                     const char *s, 
                                     unsigned int count)
{
    struct uart_8250_port *up = &serial8250_ports[co->index];
    unsigned long flags;
    unsigned int ier;

    // 禁用中断
    spin_lock_irqsave(&up->port.lock, flags);
    ier = serial_in(up, UART_IER);
    serial_out(up, UART_IER, 0);

    // 逐字符输出
    uart_console_write(&up->port, s, count, serial8250_console_putchar);

    // 恢复中断
    serial_out(up, UART_IER, ier);
    spin_unlock_irqrestore(&up->port.lock, flags);
}

static int __init serial8250_console_setup(struct console *co, char *options)
{
    struct uart_port *port;
    int baud = 9600;
    int bits = 8;
    int parity = 'n';
    int flow = 'n';

    // 解析命令行选项 (如 "console=ttyS0,115200n8")
    if (options)
        uart_parse_options(options, &baud, &parity, &bits, &flow);

    return uart_set_options(port, co, baud, parity, bits, flow);
}

// 注入 write 和 setup 实现
static struct console serial8250_console = {
    .name       = "ttyS",
    .write      = serial8250_console_write,     // 注入: 输出函数
    .device     = uart_console_device,
    .setup      = serial8250_console_setup,     // 注入: 初始化函数
    .flags      = CON_PRINTBUFFER,
    .index      = -1,
    .data       = &serial8250_reg,
};

// 注册到框架
static int __init serial8250_console_init(void)
{
    register_console(&serial8250_console);
    return 0;
}
console_initcall(serial8250_console_init);
```

---

### 5. Early Console (earlycon) - 早期启动控制台

```c
// 早期控制台用于 printk 子系统初始化前的输出

// arch/x86/kernel/early_printk.c
static struct console early_serial_console = {
    .name       = "earlyser",
    .write      = early_serial_write,
    .flags      = CON_PRINTBUFFER | CON_BOOT,
    .index      = -1,
};

void __init setup_early_printk(const char *cmdline)
{
    // 解析 earlyprintk= 参数
    if (!strncmp(buf, "serial", 6)) {
        early_serial_init(buf);
        early_console = &early_serial_console;
    }
    
    register_console(early_console);
}

// 早期输出函数 - 直接操作硬件
static void early_serial_write(struct console *con, 
                               const char *s, 
                               unsigned n)
{
    while (*s && n-- > 0) {
        // 直接写串口寄存器
        if (*s == '\n')
            early_serial_putc('\r');
        early_serial_putc(*s);
        s++;
    }
}
```

---

## 这样做的好处

### 1. 运行时动态切换输出目标

```
启动早期:
    printk ──► earlycon (直接操作硬件)

内核初始化后:
    printk ──► serial8250_console (串口驱动)
           └─► vga_console (VGA 显示)

调试时:
    printk ──► netconsole (网络输出)
```

### 2. 多输出并行

```c
// 同时输出到多个设备
for_each_console(con) {
    con->write(con, msg, len);  // 每个 console 都收到消息
}
```

### 3. 无需修改 printk 代码

| 新增控制台类型 | 需要的工作 |
|----------------|------------|
| USB 串口控制台 | 实现 write/setup，调用 register_console |
| 网络控制台 | 实现 write/setup，调用 register_console |
| 蓝牙控制台 | 实现 write/setup，调用 register_console |
| LCD 显示控制台 | 实现 write/setup，调用 register_console |

printk 代码完全不需要修改！

### 4. 分阶段启用

```
┌────────────────────────────────────────────────────────────────────────┐
│  Boot 阶段                    Kernel 初始化                  正常运行  │
├────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  earlyprintk ─────────────►│                                           │
│  (CON_BOOT)                │                                           │
│                            │                                           │
│                            │  8250 console ────────────────────────►   │
│                            │  (注册后自动替换 boot console)            │
│                                                                         │
└────────────────────────────────────────────────────────────────────────┘
```

### 5. 便于调试和故障排查

- 可以通过 `netconsole` 远程收集日志
- 可以将日志同时输出到串口和屏幕
- 内核 panic 时仍可输出到简单设备

---

## 核心源码文件

| 文件 | 功能 |
|------|------|
| `include/linux/console.h` | console 结构定义 |
| `kernel/printk.c` | printk 实现，console 注册/遍历 |
| `drivers/tty/serial/8250/8250.c` | 8250 串口 console 实现 |
| `drivers/video/console/vgacon.c` | VGA 文本控制台 |
| `drivers/net/netconsole.c` | 网络控制台 |
| `arch/x86/kernel/early_printk.c` | x86 早期控制台 |

---

## 总结

Console 子系统的 IoC 模式:

1. **统一接口**: 所有 console 实现相同的 `struct console` 接口
2. **动态注册**: 通过 `register_console()` 在运行时注入
3. **链式调用**: 框架遍历所有 console，依次调用 write
4. **生命周期管理**: boot console 自动被真正 console 替换
5. **零耦合**: printk 与具体输出设备完全解耦

