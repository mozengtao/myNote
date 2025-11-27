# initcall 机制中的依赖注入 (IoC) 模式

## 概述

Linux 内核的 initcall 机制是一种编译时依赖注入。模块通过宏将初始化函数"注册"到特定的链接器节区，内核启动时框架自动遍历并调用这些函数，实现了初始化顺序的控制反转。

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        initcall 机制 IoC 架构                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   编译时: 各模块通过宏注册初始化函数                                         │
│                                                                              │
│   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐    │
│   │  net/ipv4/  │   │drivers/pci/ │   │  fs/ext4/   │   │   mm/       │    │
│   │             │   │             │   │             │   │             │    │
│   │ fs_initcall │   │ subsys_     │   │ fs_initcall │   │ core_       │    │
│   │  (inet_init)│   │   initcall  │   │  (ext4_init)│   │  initcall   │    │
│   │             │   │  (pci_init) │   │             │   │  (mm_init)  │    │
│   └──────┬──────┘   └──────┬──────┘   └──────┬──────┘   └──────┬──────┘    │
│          │                 │                 │                 │            │
│          ▼                 ▼                 ▼                 ▼            │
│   ┌─────────────────────────────────────────────────────────────────────┐  │
│   │                          链接器脚本                                  │  │
│   │                                                                      │  │
│   │   .initcall1.init:  mm_init, ...              (core_initcall)       │  │
│   │   .initcall2.init:  pci_init, ...             (postcore_initcall)   │  │
│   │   .initcall3.init:  ...                       (arch_initcall)       │  │
│   │   .initcall4.init:  pci_driver_init, ...      (subsys_initcall)     │  │
│   │   .initcall5.init:  inet_init, ext4_init, ... (fs_initcall)         │  │
│   │   .initcall6.init:  ...                       (device_initcall)     │  │
│   │   .initcall7.init:  ...                       (late_initcall)       │  │
│   │                                                                      │  │
│   └─────────────────────────────────────────────────────────────────────┘  │
│                                       │                                     │
│   运行时: 内核框架遍历并调用          │                                     │
│                                       ▼                                     │
│   ┌─────────────────────────────────────────────────────────────────────┐  │
│   │                     do_initcalls() @ init/main.c                    │  │
│   │                                                                      │  │
│   │   for (fn = __initcall_start; fn < __initcall_end; fn++)            │  │
│   │       do_one_initcall(*fn);   ◄── 框架控制调用顺序和时机            │  │
│   │                                                                      │  │
│   └─────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│   控制反转体现:                                                              │
│   - 模块不决定何时初始化，由框架决定                                         │
│   - 模块不决定初始化顺序，由 initcall 级别决定                              │
│   - 模块只需声明初始化函数，框架负责调用                                     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 核心代码片段

### 1. initcall 宏定义 - 依赖注入的声明方式

```c
// include/linux/init.h

// 初始化函数指针类型
typedef int (*initcall_t)(void);
typedef void (*exitcall_t)(void);

// 核心宏: 将函数指针放入指定的链接器节区
#define __define_initcall(level, fn, id) \
    static initcall_t __initcall_##fn##id __used \
    __attribute__((__section__(".initcall" level ".init"))) = fn

// 不同级别的 initcall (按执行顺序)
#define early_initcall(fn)          __define_initcall("early", fn, early)
#define pure_initcall(fn)           __define_initcall("0", fn, 0)
#define core_initcall(fn)           __define_initcall("1", fn, 1)
#define core_initcall_sync(fn)      __define_initcall("1s", fn, 1s)
#define postcore_initcall(fn)       __define_initcall("2", fn, 2)
#define postcore_initcall_sync(fn)  __define_initcall("2s", fn, 2s)
#define arch_initcall(fn)           __define_initcall("3", fn, 3)
#define arch_initcall_sync(fn)      __define_initcall("3s", fn, 3s)
#define subsys_initcall(fn)         __define_initcall("4", fn, 4)
#define subsys_initcall_sync(fn)    __define_initcall("4s", fn, 4s)
#define fs_initcall(fn)             __define_initcall("5", fn, 5)
#define fs_initcall_sync(fn)        __define_initcall("5s", fn, 5s)
#define rootfs_initcall(fn)         __define_initcall("rootfs", fn, rootfs)
#define device_initcall(fn)         __define_initcall("6", fn, 6)
#define device_initcall_sync(fn)    __define_initcall("6s", fn, 6s)
#define late_initcall(fn)           __define_initcall("7", fn, 7)
#define late_initcall_sync(fn)      __define_initcall("7s", fn, 7s)

// module_init 默认等于 device_initcall
#define __initcall(fn) device_initcall(fn)
#define module_init(x) __initcall(x);
```

**说明**: 宏将函数指针放入特定的节区 (如 `.initcall5.init`)，链接器按节区顺序排列。

---

### 2. 链接器脚本 - 定义节区顺序

```c
// include/asm-generic/vmlinux.lds.h

#define INIT_CALLS                                          \
    VMLINUX_SYMBOL(__initcall_start) = .;                  \
    *(.initcallearly.init)                                  \
    INIT_CALLS_LEVEL(0)     /* pure_initcall */            \
    INIT_CALLS_LEVEL(1)     /* core_initcall */            \
    INIT_CALLS_LEVEL(1s)    /* core_initcall_sync */       \
    INIT_CALLS_LEVEL(2)     /* postcore_initcall */        \
    INIT_CALLS_LEVEL(2s)    /* postcore_initcall_sync */   \
    INIT_CALLS_LEVEL(3)     /* arch_initcall */            \
    INIT_CALLS_LEVEL(3s)    /* arch_initcall_sync */       \
    INIT_CALLS_LEVEL(4)     /* subsys_initcall */          \
    INIT_CALLS_LEVEL(4s)    /* subsys_initcall_sync */     \
    INIT_CALLS_LEVEL(5)     /* fs_initcall */              \
    INIT_CALLS_LEVEL(5s)    /* fs_initcall_sync */         \
    *(.initcallrootfs.init)                                 \
    INIT_CALLS_LEVEL(6)     /* device_initcall */          \
    INIT_CALLS_LEVEL(6s)    /* device_initcall_sync */     \
    INIT_CALLS_LEVEL(7)     /* late_initcall */            \
    INIT_CALLS_LEVEL(7s)    /* late_initcall_sync */       \
    VMLINUX_SYMBOL(__initcall_end) = .;

#define INIT_CALLS_LEVEL(level)                             \
    VMLINUX_SYMBOL(__initcall##level##_start) = .;         \
    *(.initcall##level##.init)                              \
    *(.initcall##level##s.init)
```

---

### 3. 框架执行 initcall - 控制反转的核心

```c
// init/main.c

// 所有 initcall 的起始和结束位置 (由链接器定义)
extern initcall_t __initcall_start[], __initcall_end[], __early_initcall_end[];

// 执行单个 initcall
int __init_or_module do_one_initcall(initcall_t fn)
{
    int count = preempt_count();
    int ret;
    char msgbuf[64];

    // 调试信息
    if (initcall_debug)
        printk("calling  %pF\n", fn);
    
    // 调用注入的初始化函数
    ret = fn();
    
    msgbuf[0] = 0;

    // 检查初始化函数是否正确恢复状态
    if (ret) {
        sprintf(msgbuf, "error code %d ", ret);
    }
    if (preempt_count() != count) {
        strlcat(msgbuf, "preemption imbalance ", sizeof(msgbuf));
        preempt_count() = count;
    }
    if (irqs_disabled()) {
        strlcat(msgbuf, "disabled interrupts ", sizeof(msgbuf));
        local_irq_enable();
    }
    if (msgbuf[0]) {
        printk("initcall %pF returned with %s\n", fn, msgbuf);
    }

    return ret;
}

// 遍历执行所有 initcall
static void __init do_initcalls(void)
{
    initcall_t *fn;

    // 框架遍历所有注册的初始化函数
    for (fn = __early_initcall_end; fn < __initcall_end; fn++)
        do_one_initcall(*fn);
}

// 基本设置阶段
static void __init do_basic_setup(void)
{
    cpuset_init_smp();
    usermodehelper_init();
    shmem_init();
    driver_init();          // 初始化驱动模型
    init_irq_proc();
    do_ctors();             // C++ 构造函数 (如果有)
    usermodehelper_enable();
    do_initcalls();         // 执行所有 initcall (依赖注入点)
}

// 早期 initcall (SMP 初始化前)
static void __init do_pre_smp_initcalls(void)
{
    initcall_t *fn;

    for (fn = __initcall_start; fn < __early_initcall_end; fn++)
        do_one_initcall(*fn);
}
```

---

### 4. 各子系统使用示例

```c
// 网络子系统初始化
// net/ipv4/af_inet.c
static int __init inet_init(void)
{
    // 注册协议族
    sock_register(&inet_family_ops);
    
    // 初始化 TCP, UDP, ICMP 等
    tcp_init();
    udp_init();
    icmp_init();
    
    return 0;
}
fs_initcall(inet_init);  // 注入到 fs_initcall 级别

// PCI 子系统初始化
// drivers/pci/pci.c
static int __init pci_init(void)
{
    pci_fixup_devices();
    return 0;
}
postcore_initcall(pci_init);  // 注入到 postcore_initcall 级别

// PCI 驱动框架初始化
// drivers/pci/pci-driver.c
static int __init pci_driver_init(void)
{
    return bus_register(&pci_bus_type);
}
subsys_initcall(pci_driver_init);  // 注入到 subsys_initcall 级别

// ext4 文件系统初始化
// fs/ext4/super.c
static int __init ext4_init_fs(void)
{
    ext4_init_sysfs();
    ext4_init_mballoc();
    ext4_init_xattr();
    return register_filesystem(&ext4_fs_type);
}
module_init(ext4_init_fs);  // 等于 device_initcall
```

---

### 5. console_initcall - 专用的 initcall

```c
// include/linux/init.h

// console 专用的 initcall 节区
#define console_initcall(fn) \
    static initcall_t __initcall_##fn \
    __used __section(.con_initcall.init) = fn

// init/main.c
extern initcall_t __con_initcall_start[], __con_initcall_end[];

// 在 console_init() 中调用
void __init console_init(void)
{
    initcall_t *call;

    // 遍历 console_initcall 注册的函数
    for (call = __con_initcall_start; call < __con_initcall_end; call++)
        (*call)();
}
```

---

### 6. security_initcall - 安全模块专用

```c
// include/linux/init.h

#define security_initcall(fn) \
    static initcall_t __initcall_##fn \
    __used __section(.security_initcall.init) = fn

// 使用示例: SELinux
// security/selinux/hooks.c
static __init int selinux_init(void)
{
    security_add_hooks(selinux_hooks, ARRAY_SIZE(selinux_hooks));
    return 0;
}
security_initcall(selinux_init);
```

---

## initcall 级别详解

| 级别 | 宏名 | 执行时机 | 典型用途 |
|------|------|----------|----------|
| early | `early_initcall` | SMP 初始化前 | 早期调试、irq |
| 0 | `pure_initcall` | 最早，无依赖 | 纯变量初始化 |
| 1 | `core_initcall` | 核心子系统 | 内存、调度器 |
| 2 | `postcore_initcall` | 核心后 | PCI 枚举 |
| 3 | `arch_initcall` | 架构相关 | ACPI、IOMMU |
| 4 | `subsys_initcall` | 子系统 | 驱动模型、总线 |
| 5 | `fs_initcall` | 文件系统 | VFS、网络协议 |
| rootfs | `rootfs_initcall` | 根文件系统 | initramfs |
| 6 | `device_initcall` | 设备驱动 | 大多数驱动 |
| 7 | `late_initcall` | 最后 | 清理、非关键 |

```
时间轴:
start_kernel()
    │
    ├──► early_initcall          (SMP 前)
    │
    ├──► smp_init()              (启动其他 CPU)
    │
    └──► do_basic_setup()
          │
          └──► do_initcalls()
                │
                ├──► pure_initcall (0)
                ├──► core_initcall (1)
                ├──► postcore_initcall (2)
                ├──► arch_initcall (3)
                ├──► subsys_initcall (4)
                ├──► fs_initcall (5)
                ├──► rootfs_initcall
                ├──► device_initcall (6)
                └──► late_initcall (7)
```

---

## 这样做的好处

### 1. 初始化顺序自动管理

```
传统方式 (显式调用):
void kernel_init() {
    mm_init();
    pci_init();
    net_init();
    fs_init();
    driver_a_init();
    driver_b_init();
    // 添加新驱动需要修改此函数
}

initcall 方式 (声明式):
// 各模块独立声明
core_initcall(mm_init);
postcore_initcall(pci_init);
fs_initcall(net_init);
fs_initcall(fs_init);
device_initcall(driver_a_init);
device_initcall(driver_b_init);
// 添加新驱动只需添加声明
```

### 2. 模块化与解耦

- 每个子系统独立声明自己的初始化
- 不需要中央注册表
- 添加/删除模块不影响其他代码

### 3. 编译时依赖注入

```
编译时:                      运行时:
┌─────────────┐             ┌─────────────┐
│ driver_a.c  │             │             │
│ module_init │──┐          │  do_       │
└─────────────┘  │          │  initcalls │
                 │          │             │
┌─────────────┐  │  链接器  │  遍历执行  │
│ driver_b.c  │──┼────────► │  所有      │
│ module_init │  │          │  initcall  │
└─────────────┘  │          │             │
                 │          └─────────────┘
┌─────────────┐  │
│ fs/ext4.c   │──┘
│ module_init │
└─────────────┘
```

### 4. 清晰的依赖层次

```
依赖关系通过 initcall 级别表达:

core_initcall (内存管理)
       │
       ▼
postcore_initcall (PCI 枚举)
       │
       ▼
subsys_initcall (总线驱动)
       │
       ▼
device_initcall (设备驱动)
```

### 5. 模块支持

```c
// 静态编译时: 使用 initcall 机制
#ifndef MODULE
#define module_init(x)  __initcall(x);
#endif

// 模块加载时: 导出 init_module
#ifdef MODULE
#define module_init(initfn)                 \
    static inline initcall_t __inittest(void)   \
    { return initfn; }                      \
    int init_module(void) __attribute__((alias(#initfn)));
#endif
```

相同的代码可以:
- 编译进内核 → 使用 initcall
- 编译成模块 → 使用 init_module

---

## __init 和 __initdata - 初始化内存回收

```c
// include/linux/init.h

// 初始化代码放入 .init.text 节区
#define __init      __section(.init.text) __cold notrace

// 初始化数据放入 .init.data 节区
#define __initdata  __section(.init.data)

// 这些节区在初始化完成后被释放
// kernel_init() 最后调用:
void __init free_initmem(void)
{
    // 释放 .init 节区占用的内存
    free_init_pages("unused kernel memory",
                    (unsigned long)__init_begin,
                    (unsigned long)__init_end);
}
```

---

## 核心源码文件

| 文件 | 功能 |
|------|------|
| `include/linux/init.h` | initcall 宏定义 |
| `init/main.c` | do_initcalls 实现 |
| `include/asm-generic/vmlinux.lds.h` | 链接器脚本模板 |
| `arch/x86/kernel/vmlinux.lds.S` | x86 链接器脚本 |

---

## 总结

initcall 机制的 IoC 模式:

1. **编译时注入**: 通过宏和链接器节区实现"注册"
2. **运行时遍历**: 框架统一遍历调用所有 initcall
3. **级别分层**: 通过不同级别控制初始化顺序
4. **零耦合**: 模块之间不需要相互引用
5. **可释放**: `__init` 标记的代码/数据可在初始化后释放

这是一种独特的"编译时依赖注入"，不同于运行时的动态注入，但同样实现了控制反转的核心思想。

