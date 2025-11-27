# 控制台子系统中的依赖注入模式

> 文件路径: `/tmp/linux-ioc-patterns/02_console_subsystem.md`
> 内核版本: Linux 3.2
> 难度: ⭐⭐

---

## 1. 模式概述

控制台子系统实现了内核输出与具体输出设备的完全解耦。`printk()` 函数不知道消息将输出到哪里，具体的输出设备通过 `register_console()` 动态注入。

### DI/IoC 的具体表现形式

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      控制台子系统的依赖注入模式                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   内核各子系统                                                               │
│   ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐             │
│   │ 调度器  │ │ 内存管理│ │ 网络栈  │ │ 驱动    │ │ 文件系统│             │
│   └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘             │
│        │           │           │           │           │                    │
│        └───────────┴───────────┴─────┬─────┴───────────┘                    │
│                                      │                                       │
│                                      ▼                                       │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │                            printk()                                   │  │
│   │                                                                       │  │
│   │   不关心输出到哪里，只负责:                                           │  │
│   │   1. 格式化消息                                                       │  │
│   │   2. 存入 ring buffer                                                │  │
│   │   3. 遍历 console_drivers 链表调用 write                              │  │
│   │                                                                       │  │
│   └───────────────────────────────┬──────────────────────────────────────┘  │
│                                   │                                          │
│                                   │ for_each_console(con):                  │
│                                   │     con->write(con, msg, len)           │
│                                   │                                          │
│           ┌───────────────────────┼───────────────────────┐                 │
│           │                       │                       │                  │
│           ▼                       ▼                       ▼                  │
│   ┌───────────────┐       ┌───────────────┐       ┌───────────────┐         │
│   │ 串口 console  │       │ VGA console   │       │ netconsole    │         │
│   │               │       │               │       │               │         │
│   │ write() ─────►│───────│ write() ─────►│───────│ write() ─────►│──►      │
│   │   串口输出    │       │   屏幕输出    │       │   网络输出    │  网络    │
│   └───────────────┘       └───────────────┘       └───────────────┘         │
│                                                                              │
│   控制反转:                                                                  │
│   • printk 不决定输出到哪里 → 由注册的 console 决定                         │
│   • printk 不决定输出格式 → 由 console 的 write 实现决定                    │
│   • 可以同时存在多个 console，全部收到输出                                  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 设计动机

### 要解决的问题

| 问题 | 解决方案 |
|------|----------|
| **早期启动无驱动** | earlycon 直接操作硬件，无需驱动框架 |
| **多输出设备** | 链表结构支持多个 console 并存 |
| **热插拔输出** | 运行时可以添加/移除 console |
| **调试灵活性** | 可以动态切换输出到串口、网络等 |
| **启动阶段过渡** | boot console 自动让位给正式 console |

### 设计目标

1. **printk 与输出设备解耦**: printk 只管格式化，不管输出
2. **支持多阶段启动**: early → boot → normal console
3. **支持多输出并行**: 同时输出到串口和屏幕
4. **易于扩展**: 添加新的输出方式只需实现 console 结构

---

## 3. 核心数据结构

### 3.1 console 结构

```c
// include/linux/console.h (第 114-127 行)

// 控制台标志
#define CON_PRINTBUFFER (1)   // 注册时打印缓冲区中的历史消息
#define CON_CONSDEV     (2)   // 首选控制台设备
#define CON_ENABLED     (4)   // 控制台已启用
#define CON_BOOT        (8)   // 早期启动控制台 (会被替换)
#define CON_ANYTIME     (16)  // 即使 CPU 离线也可安全调用
#define CON_BRL         (32)  // 盲文设备

struct console {
    char    name[16];               // 控制台名称 (如 "ttyS", "tty")

    // ===== 依赖注入点: 输出操作 =====
    void    (*write)(struct console *, const char *, unsigned);  // 写入函数
    int     (*read)(struct console *, char *, unsigned);         // 读取函数 (可选)

    // 获取关联的 tty 驱动
    struct tty_driver *(*device)(struct console *, int *);

    // 屏幕解除保护
    void    (*unblank)(void);

    // ===== 依赖注入点: 初始化 =====
    int     (*setup)(struct console *, char *);      // 设置函数
    int     (*early_setup)(void);                    // 早期设置函数

    short   flags;                  // 控制台标志
    short   index;                  // 控制台索引 (如 ttyS0 的 0)
    int     cflag;                  // 终端配置
    void    *data;                  // 私有数据
    struct  console *next;          // 链表指针
};
```

### 3.2 全局数据

```c
// kernel/printk.c (第 85-86 行)

// 全局 console 链表头
struct console *console_drivers;
EXPORT_SYMBOL_GPL(console_drivers);

// 遍历宏
#define for_each_console(con) \
    for (con = console_drivers; con != NULL; con = con->next)
```

### 3.3 结构关系图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Console 链表结构                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   console_drivers                                                            │
│        │                                                                     │
│        ▼                                                                     │
│   ┌────────────────┐     ┌────────────────┐     ┌────────────────┐         │
│   │ struct console │     │ struct console │     │ struct console │         │
│   │                │     │                │     │                │         │
│   │ name = "ttyS"  │     │ name = "tty"   │     │ name = "netcon"│         │
│   │ index = 0      │     │ index = 0      │     │ index = 0      │         │
│   │ flags = 0x7    │     │ flags = 0x5    │     │ flags = 0x5    │         │
│   │                │     │                │     │                │         │
│   │ write = serial │     │ write = vga_   │     │ write = net_   │         │
│   │         _write │     │         write  │     │         write  │         │
│   │ setup = serial │     │ setup = vga_   │     │ setup = net_   │         │
│   │         _setup │     │         setup  │     │         setup  │         │
│   │                │     │                │     │                │         │
│   │ next ──────────┼────►│ next ──────────┼────►│ next = NULL    │         │
│   │                │     │                │     │                │         │
│   └────────────────┘     └────────────────┘     └────────────────┘         │
│                                                                              │
│   printk 调用时遍历整个链表:                                                 │
│   for_each_console(con) {                                                   │
│       if (con->flags & CON_ENABLED)                                         │
│           con->write(con, msg, len);   // 每个 console 都输出              │
│   }                                                                         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. 代码流程分析

### 4.1 注册机制 - register_console()

```c
// kernel/printk.c (第 1416-1566 行)

void register_console(struct console *newcon)
{
    int i;
    unsigned long flags;
    struct console *bcon = NULL;

    // 1. 检查 boot console 冲突
    if (console_drivers && newcon->flags & CON_BOOT) {
        for_each_console(bcon) {
            if (!(bcon->flags & CON_BOOT)) {
                printk(KERN_INFO "Too late to register bootconsole %s%d\n",
                    newcon->name, newcon->index);
                return;
            }
        }
    }

    // 2. 保存当前的 boot console
    if (console_drivers && console_drivers->flags & CON_BOOT)
        bcon = console_drivers;

    // 3. 调用 early_setup (如果提供)
    if (newcon->early_setup)
        newcon->early_setup();

    // 4. 匹配命令行参数 (如 console=ttyS0,115200)
    for (i = 0; i < MAX_CMDLINECONSOLES && console_cmdline[i].name[0]; i++) {
        if (strcmp(console_cmdline[i].name, newcon->name) != 0)
            continue;
        if (newcon->index >= 0 &&
            newcon->index != console_cmdline[i].index)
            continue;
        if (newcon->index < 0)
            newcon->index = console_cmdline[i].index;

        // 5. 调用 setup (依赖注入点)
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

    // 6. 添加到链表
    console_lock();
    if ((newcon->flags & CON_CONSDEV) || console_drivers == NULL) {
        // 插入链表头
        newcon->next = console_drivers;
        console_drivers = newcon;
    } else {
        // 插入链表第二位
        newcon->next = console_drivers->next;
        console_drivers->next = newcon;
    }

    // 7. 如果需要，打印历史消息
    if (newcon->flags & CON_PRINTBUFFER) {
        // 遍历 ring buffer，调用 newcon->write 输出
        // ...
    }
    console_unlock();

    // 8. 替换 boot console
    if (bcon && ((newcon->flags & (CON_CONSDEV | CON_BOOT)) == CON_CONSDEV)) {
        printk(KERN_INFO "console [%s%d] enabled, bootconsole disabled\n",
            newcon->name, newcon->index);
        for_each_console(bcon) {
            if (bcon->flags & CON_BOOT)
                unregister_console(bcon);
        }
    } else {
        printk(KERN_INFO "%sconsole [%s%d] enabled\n",
            (newcon->flags & CON_BOOT) ? "boot" : "",
            newcon->name, newcon->index);
    }
}
EXPORT_SYMBOL(register_console);
```

### 4.2 调用路径 - printk 如何输出

```c
// kernel/printk.c (简化版)

int vprintk(const char *fmt, va_list args)
{
    unsigned long flags;
    int printed_len = 0;
    char *p;
    static char textbuf[LOG_LINE_MAX];

    // 1. 格式化消息
    printed_len = vscnprintf(textbuf, sizeof(textbuf), fmt, args);

    // 2. 存入 ring buffer
    log_store(textbuf, printed_len, ...);

    // 3. 输出到所有 console
    if (console_trylock()) {
        console_unlock();  // 触发输出
    }

    return printed_len;
}

void console_unlock(void)
{
    // ...
    // 输出 ring buffer 中的消息到所有 console
    call_console_drivers(start, end);
    // ...
}

// 关键: 遍历并调用所有 console 的 write
static void call_console_drivers(unsigned start, unsigned end)
{
    struct console *con;

    for_each_console(con) {
        if (exclusive_console && con != exclusive_console)
            continue;
        if (!(con->flags & CON_ENABLED))
            continue;
        if (!con->write)
            continue;

        // 控制反转: 调用注入的 write 函数
        con->write(con, &LOG_BUF(start), end - start);
    }
}
```

### 4.3 Earlycon - 启动早期的依赖注入

```c
// arch/x86/kernel/early_printk.c

// earlycon 的 write 实现 - 直接操作硬件
static void early_serial_write(struct console *con,
                               const char *s,
                               unsigned n)
{
    while (*s && n-- > 0) {
        if (*s == '\n')
            early_serial_putc('\r');
        early_serial_putc(*s);  // 直接写串口寄存器
        s++;
    }
}

static void early_serial_putc(unsigned char ch)
{
    unsigned timeout = 0xffff;

    // 等待发送缓冲区空
    while ((inb(early_serial_base + LSR) & THRE) == 0 && --timeout)
        cpu_relax();

    // 直接写数据寄存器
    outb(ch, early_serial_base + TXR);
}

// earlycon 结构
static struct console early_serial_console = {
    .name       = "earlyser",
    .write      = early_serial_write,   // 注入: 早期输出函数
    .flags      = CON_PRINTBUFFER | CON_BOOT,
    .index      = -1,
};

// 由内核参数 earlyprintk= 触发
void __init setup_early_printk(const char *cmdline)
{
    // 解析参数
    if (!strncmp(buf, "serial", 6)) {
        early_serial_init(buf + 6);
        early_console = &early_serial_console;
    }

    // 注册 earlycon
    register_console(early_console);
}
```

### 4.4 完整调用流程

```
                    启动早期                          正常运行
                        │                                │
        ┌───────────────┼────────────────────────────────┤
        │               │                                │
        ▼               │                                │
  setup_early_printk()  │                                │
        │               │                                │
        ▼               │                                │
  register_console      │                                │
  (&early_serial_       │                                │
   console)             │                                │
        │               │                                │
        │               ▼                                │
        │       serial8250_console_init()                │
        │               │                                │
        │               ▼                                │
        │       register_console                         │
        │       (&serial8250_console)                    │
        │               │                                │
        │               │  检测到非 boot console         │
        │               │  注销 early_serial_console     │
        │               │                                │
        │               ▼                                │
        │       printk() 输出到                          │
        │       serial8250_console                       │
        │                                                │
        │                                                ▼
        │                                        netconsole_init()
        │                                                │
        │                                                ▼
        │                                        register_console
        │                                        (&netconsole)
        │                                                │
        │                                                ▼
        │                                        printk() 同时输出到:
        │                                        - serial8250_console
        │                                        - netconsole
        │
        └──────────────────────────────────────────────────────────────
                        时间轴 ──────────────────────────────────────►
```

---

## 5. 实际案例

### 案例1: 8250 串口 console

```c
// drivers/tty/serial/8250/8250.c

// 串口输出实现
static void serial8250_console_write(struct console *co,
                                     const char *s,
                                     unsigned int count)
{
    struct uart_8250_port *up = &serial8250_ports[co->index];
    unsigned long flags;
    unsigned int ier;
    int locked = 1;

    // 获取锁
    if (oops_in_progress)
        locked = spin_trylock_irqsave(&up->port.lock, flags);
    else
        spin_lock_irqsave(&up->port.lock, flags);

    // 保存并禁用中断
    ier = serial_in(up, UART_IER);
    serial_out(up, UART_IER, 0);

    // 输出每个字符
    uart_console_write(&up->port, s, count, serial8250_console_putchar);

    // 恢复中断
    wait_for_xmitr(up, BOTH_EMPTY);
    serial_out(up, UART_IER, ier);

    if (locked)
        spin_unlock_irqrestore(&up->port.lock, flags);
}

// 设置函数 - 解析波特率等参数
static int __init serial8250_console_setup(struct console *co, char *options)
{
    struct uart_port *port;
    int baud = 9600;
    int bits = 8;
    int parity = 'n';
    int flow = 'n';

    // 获取端口
    if (co->index >= nr_uarts)
        co->index = 0;
    port = &serial8250_ports[co->index].port;
    if (!port->iobase && !port->membase)
        return -ENODEV;

    // 解析选项 (如 "115200n8")
    if (options)
        uart_parse_options(options, &baud, &parity, &bits, &flow);

    return uart_set_options(port, co, baud, parity, bits, flow);
}

// 串口 console 结构 - 依赖注入
static struct console serial8250_console = {
    .name       = "ttyS",
    .write      = serial8250_console_write,  // 注入: 输出函数
    .device     = uart_console_device,
    .setup      = serial8250_console_setup,  // 注入: 设置函数
    .flags      = CON_PRINTBUFFER,
    .index      = -1,
    .data       = &serial8250_reg,
};

// 初始化
static int __init serial8250_console_init(void)
{
    serial8250_isa_init_ports();
    register_console(&serial8250_console);
    return 0;
}
console_initcall(serial8250_console_init);
```

### 案例2: VGA 文本控制台

```c
// drivers/video/console/vgacon.c

// VGA 输出 - 直接写显存
static void vgacon_write(struct console *con, const char *s, unsigned n)
{
    unsigned short *p;
    int x, y;

    // 获取当前光标位置
    vgacon_get_cursor(&x, &y);

    // 计算显存地址
    p = (unsigned short *)vga_vram_base + y * vga_video_num_columns + x;

    while (n-- > 0) {
        char c = *s++;

        if (c == '\n') {
            x = 0;
            y++;
            if (y >= vga_video_num_lines) {
                // 滚屏
                vgacon_scroll();
                y--;
            }
        } else {
            // 写入显存: 属性 + 字符
            *p++ = vga_attr | (unsigned short)c;
            x++;
            if (x >= vga_video_num_columns) {
                x = 0;
                y++;
            }
        }
    }

    // 更新光标
    vgacon_set_cursor(x, y);
}

static struct console vga_con = {
    .name       = "tty",
    .write      = vgacon_write,      // 注入: VGA 输出
    .setup      = vgacon_setup,
    .flags      = CON_PRINTBUFFER,
    .index      = 0,
};
```

### 案例3: 网络控制台 (netconsole)

```c
// drivers/net/netconsole.c

// 网络输出 - 通过 UDP 发送
static void write_msg(struct console *con, const char *msg, unsigned int len)
{
    int frag, left;
    unsigned long flags;
    struct netconsole_target *nt, *tmp;

    spin_lock_irqsave(&target_list_lock, flags);

    // 遍历所有目标服务器
    list_for_each_entry_safe(nt, tmp, &target_list, list) {
        // 分片发送 (UDP 有大小限制)
        for (left = len; left;) {
            frag = min(left, MAX_PRINT_CHUNK);

            // 发送 UDP 包
            netpoll_send_udp(&nt->np, msg, frag);

            msg += frag;
            left -= frag;
        }
    }

    spin_unlock_irqrestore(&target_list_lock, flags);
}

static struct console netconsole = {
    .name       = "netcon",
    .write      = write_msg,         // 注入: 网络输出
    .flags      = CON_ENABLED,
    .index      = -1,
};

// 模块加载时注册
static int __init init_netconsole(void)
{
    int err;

    // 解析模块参数
    // netconsole=@/eth0,@192.168.1.100/

    err = netpoll_setup(&nt->np);
    if (err)
        goto fail;

    register_console(&netconsole);
    printk(KERN_INFO "netconsole: network logging started\n");
    return 0;
}
module_init(init_netconsole);
```

---

## 6. 优势分析

### 6.1 多输出并行

```c
// 同一条消息可以同时输出到多个设备
printk("System error!\n");

// 输出到:
// 1. 串口 (调试人员可以看到)
// 2. VGA 屏幕 (本地用户可以看到)
// 3. 网络 (远程日志服务器可以收集)

// 实现原理:
for_each_console(con) {
    con->write(con, msg, len);  // 每个 console 都调用
}
```

### 6.2 启动阶段平滑过渡

```
时间 ─────────────────────────────────────────────────────────►

│ 启动早期           │ 初始化阶段         │ 正常运行
│ (无驱动框架)       │ (驱动加载)         │
│                    │                    │
│ earlycon           │                    │
│ (直接操作硬件)     │                    │
│      │             │                    │
│      │             │ 8250 console       │
│      │             │ (驱动方式)         │
│      │             │     │              │
│      └─────────────┼─────┘ 自动替换    │
│                    │                    │
│                    │                    │ netconsole
│                    │                    │ (可选添加)
```

### 6.3 扩展性

| 添加新的 console 类型 | 需要的工作 |
|----------------------|------------|
| USB 串口 console | 实现 write/setup，调用 register_console |
| 蓝牙 console | 实现 write/setup，调用 register_console |
| LCD 显示 console | 实现 write/setup，调用 register_console |

printk 代码完全不需要修改！

---

## 7. 对比思考

### 如果不使用 console 框架

```c
// 传统方式: printk 直接调用特定设备

void printk(const char *fmt, ...)
{
    char buf[1024];
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    // 问题1: 硬编码输出设备
    serial_write(buf);       // 只能输出到串口
    // 或者
    vga_write(buf);         // 只能输出到 VGA

    // 问题2: 如果要支持多设备，需要条件编译
    #ifdef CONFIG_SERIAL_CONSOLE
        serial_write(buf);
    #endif
    #ifdef CONFIG_VGA_CONSOLE
        vga_write(buf);
    #endif
    #ifdef CONFIG_NET_CONSOLE
        net_write(buf);
    #endif

    // 问题3: 无法运行时添加/移除输出设备
    // 问题4: 每次添加新设备都要修改 printk 代码
}
```

---

## 8. 相关 API

### Console 注册

```c
// 注册控制台
void register_console(struct console *newcon);

// 注销控制台
int unregister_console(struct console *console);

// 添加首选控制台 (从命令行参数)
int add_preferred_console(char *name, int idx, char *options);
```

### Console 遍历

```c
// 遍历所有 console
#define for_each_console(con) \
    for (con = console_drivers; con != NULL; con = con->next)

// 检查 console 是否绑定
int con_is_bound(const struct consw *csw);
```

### Console 操作

```c
// 获取/释放 console 锁
void console_lock(void);
void console_unlock(void);
int console_trylock(void);

// 检查是否持有锁
int is_console_locked(void);

// 挂起/恢复 console
void suspend_console(void);
void resume_console(void);
```

### 命令行参数

```bash
# 内核命令行参数
console=ttyS0,115200n8      # 串口 console，波特率 115200
console=tty0                 # VGA console
console=ttyS0 console=tty0   # 同时输出到两个

# 早期控制台
earlyprintk=serial,0x3f8,115200
earlyprintk=vga
```

---

## 🤔 思考题

1. **如果同时配置了 `console=ttyS0` 和 `console=tty0`，哪个是"首选"console？**
   - 提示: 查看 CON_CONSDEV 标志的作用

2. **为什么 earlycon 需要单独实现，不能使用正常的串口驱动？**
   - 提示: 考虑启动早期的环境限制

3. **netconsole 如何处理网络不可用的情况？**
   - 提示: 查看 `netpoll_send_udp` 的实现

4. **如何在运行时动态添加/移除网络日志目标？**
   - 提示: netconsole 使用 configfs 接口

---

## 📚 相关源码文件

| 文件 | 行数 | 内容 |
|------|------|------|
| `include/linux/console.h` | 1-184 | console 结构定义 |
| `kernel/printk.c` | 1-1700 | printk 和 console 管理 |
| `drivers/tty/serial/8250/8250.c` | - | 8250 串口 console |
| `drivers/video/console/vgacon.c` | - | VGA 文本 console |
| `drivers/net/netconsole.c` | - | 网络 console |
| `arch/x86/kernel/early_printk.c` | - | x86 早期 console |

