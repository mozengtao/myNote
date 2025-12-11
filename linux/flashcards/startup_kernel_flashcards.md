# Linux Kernel Startup Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel boot process, initialization, and startup sequence
> **Language**: English terms with Chinese explanations
> **Total Cards**: 100+

---

## 1. Boot Process Overview (启动流程概述)

---

Q: What is the complete Linux boot process?
A: Linux完整启动流程：

```
+==================================================================+
||                  LINUX BOOT PROCESS                            ||
+==================================================================+

电源开启
    |
    v
+------------------------------------------------------------------+
| 1. 固件阶段 (Firmware)                                            |
|    BIOS / UEFI                                                   |
|    - 硬件自检 (POST)                                              |
|    - 初始化硬件                                                   |
|    - 加载引导程序                                                 |
+------------------------------------------------------------------+
    |
    v
+------------------------------------------------------------------+
| 2. 引导加载程序 (Bootloader)                                      |
|    GRUB / systemd-boot / LILO                                    |
|    - 显示启动菜单                                                 |
|    - 加载内核镜像 (vmlinuz)                                       |
|    - 加载initramfs                                                |
|    - 传递内核参数                                                 |
+------------------------------------------------------------------+
    |
    v
+------------------------------------------------------------------+
| 3. 内核早期初始化 (Early Kernel Init)                             |
|    arch/x86/boot/                                                |
|    - 实模式设置                                                   |
|    - 切换到保护模式/长模式                                        |
|    - 解压内核                                                     |
+------------------------------------------------------------------+
    |
    v
+------------------------------------------------------------------+
| 4. 内核初始化 (Kernel Init)                                       |
|    start_kernel()                                                |
|    - 初始化各子系统                                               |
|    - 挂载根文件系统                                               |
|    - 启动init进程                                                 |
+------------------------------------------------------------------+
    |
    v
+------------------------------------------------------------------+
| 5. 用户空间初始化 (User Space Init)                               |
|    systemd / SysVinit / OpenRC                                   |
|    - 启动系统服务                                                 |
|    - 用户登录                                                     |
+------------------------------------------------------------------+
```
[Basic]

---

Q: What are the key files involved in Linux boot?
A: 启动相关的关键文件：

```
/boot/
├── vmlinuz-<version>      # 压缩的内核镜像
├── initramfs-<version>.img  # 初始RAM文件系统
├── System.map-<version>   # 内核符号表
├── config-<version>       # 内核配置
└── grub/                  # GRUB配置
    ├── grub.cfg           # GRUB配置文件
    └── grubenv            # GRUB环境变量

内核源码中的启动代码：
arch/x86/boot/
├── header.S               # 实模式入口
├── main.c                 # 实模式主程序
├── pm.c                   # 保护模式切换
├── compressed/
│   ├── head_64.S          # 64位解压入口
│   └── misc.c             # 解压代码
└── setup.ld               # 链接脚本

arch/x86/kernel/
├── head_64.S              # 64位内核入口
└── head64.c               # 早期64位初始化

init/
├── main.c                 # start_kernel()
└── initramfs.c            # initramfs处理
```

内核镜像类型：
| 类型 | 说明 |
|------|------|
| vmlinux | 未压缩的ELF内核 |
| vmlinuz | 压缩的内核镜像 |
| bzImage | 大内核镜像(>512KB) |
| zImage | 小内核镜像(<512KB) |
[Basic]

---

## 2. BIOS/UEFI Boot (固件启动)

---

Q: What is the difference between BIOS and UEFI boot?
A: 
```
+------------------------------------------------------------------+
|                    BIOS vs UEFI                                   |
+------------------------------------------------------------------+

BIOS (传统):
+------------------+
| 16位实模式        |
| 1MB地址空间限制   |
| MBR分区表        |
| 最大2TB磁盘      |
+--------+---------+
         |
         v
    加载MBR (第一个扇区512字节)
         |
         v
    加载引导程序 (stage1 -> stage2)
         |
         v
    加载内核


UEFI (现代):
+------------------+
| 32/64位保护模式   |
| 无地址空间限制    |
| GPT分区表        |
| 支持超大磁盘      |
| 安全启动         |
+--------+---------+
         |
         v
    读取ESP (EFI System Partition)
         |
         v
    加载EFI应用 (grubx64.efi 或 vmlinuz.efi)
         |
         v
    加载内核
```

| 特性 | BIOS | UEFI |
|------|------|------|
| 位数 | 16位 | 32/64位 |
| 分区表 | MBR | GPT |
| 最大磁盘 | 2TB | 8ZB |
| 启动文件 | 扇区 | FAT文件系统 |
| 安全启动 | 无 | 支持 |
| 启动速度 | 慢 | 快 |
[Basic]

---

Q: How does the kernel support EFI stub boot?
A: EFI stub允许UEFI直接启动内核：

```c
// arch/x86/boot/header.S
// PE头使内核看起来像EFI应用
#ifdef CONFIG_EFI_STUB
    .org    0x3c
    .long   pe_header
    
pe_header:
    .ascii  "PE"
    .word   0
    // COFF文件头
    .word   IMAGE_FILE_MACHINE_AMD64
    .word   section_count
    // ...
#endif

// arch/x86/boot/compressed/eboot.c
// EFI启动入口
efi_status_t efi_pe_entry(efi_handle_t handle, efi_system_table_t *sys_table)
{
    // 设置EFI环境
    efi_early = sys_table;
    
    // 获取内存映射
    status = efi_get_memory_map(&map);
    
    // 设置图形模式
    setup_graphics(boot_params);
    
    // 退出EFI引导服务
    status = exit_boot(boot_params, handle);
    
    // 跳转到内核
    goto_kernel(boot_params, start);
}
```

EFI启动方式：
```bash
# 直接从EFI启动内核
efibootmgr -c -d /dev/sda -p 1 -L "Linux" \
    -l '\vmlinuz-linux' \
    -u 'root=/dev/sda2 initrd=\initramfs-linux.img'

# 查看EFI变量
efivar -l
ls /sys/firmware/efi/efivars/
```
[Intermediate]

---

## 3. Bootloader (引导加载程序)

---

Q: How does GRUB load the Linux kernel?
A: GRUB加载内核的过程：

```
GRUB启动流程：
+------------------------------------------------------------------+
|                                                                  |
|  Stage 1 (MBR, 446字节)                                          |
|  +----------------------------------------------------------+   |
|  | 加载stage 1.5或stage 2                                    |   |
|  +----------------------------------------------------------+   |
|                         |                                        |
|                         v                                        |
|  Stage 1.5 (文件系统驱动)                                        |
|  +----------------------------------------------------------+   |
|  | 理解文件系统，加载stage 2                                  |   |
|  +----------------------------------------------------------+   |
|                         |                                        |
|                         v                                        |
|  Stage 2 (GRUB核心)                                              |
|  +----------------------------------------------------------+   |
|  | 显示菜单                                                  |   |
|  | 解析grub.cfg                                              |   |
|  | 加载内核和initramfs                                       |   |
|  +----------------------------------------------------------+   |
|                         |                                        |
|                         v                                        |
|  加载内核                                                        |
|  +----------------------------------------------------------+   |
|  | 1. 将vmlinuz加载到内存                                    |   |
|  | 2. 将initramfs加载到内存                                  |   |
|  | 3. 设置boot_params结构                                    |   |
|  | 4. 跳转到内核入口                                         |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

GRUB配置示例：
```bash
# /boot/grub/grub.cfg
menuentry 'Linux' {
    set root='hd0,gpt2'
    linux /vmlinuz-linux root=/dev/sda3 ro quiet
    initrd /initramfs-linux.img
}

# GRUB命令
linux   vmlinuz root=/dev/sda2      # 加载内核
initrd  initramfs.img               # 加载initramfs
boot                                # 启动
```

boot_params结构：
```c
// arch/x86/include/uapi/asm/bootparam.h
struct boot_params {
    struct screen_info screen_info;
    struct apm_bios_info apm_bios_info;
    __u8  _pad2[4];
    __u64 tboot_addr;
    struct ist_info ist_info;
    // ...
    struct setup_header hdr;          // 启动头
    // ...
    struct e820_table e820_table;     // 内存映射
    // ...
};

// 启动头
struct setup_header {
    __u8    setup_sects;
    __u16   root_flags;
    __u32   syssize;
    __u16   ram_size;
    __u16   vid_mode;
    __u16   root_dev;
    __u16   boot_flag;              // 0xAA55
    // ...
    __u32   cmd_line_ptr;           // 命令行指针
    __u32   initrd_addr_max;
    __u32   kernel_alignment;
    __u8    relocatable_kernel;
    // ...
};
```
[Intermediate]

---

## 4. Real Mode to Protected Mode (实模式到保护模式)

---

Q: How does the kernel transition from real mode to protected mode?
A: 模式切换过程：

```
+------------------------------------------------------------------+
|                Mode Transition (x86_64)                           |
+------------------------------------------------------------------+
|                                                                  |
|  实模式 (Real Mode)                                              |
|  +----------------------------------------------------------+   |
|  | - 16位寻址                                                |   |
|  | - 1MB地址空间                                             |   |
|  | - 无内存保护                                              |   |
|  | - 直接访问硬件                                            |   |
|  +----------------------------------------------------------+   |
|                         |                                        |
|                         | go_to_protected_mode()                 |
|                         v                                        |
|  保护模式 (Protected Mode, 32位)                                 |
|  +----------------------------------------------------------+   |
|  | - 32位寻址                                                |   |
|  | - 4GB地址空间                                             |   |
|  | - 段保护和分页                                            |   |
|  +----------------------------------------------------------+   |
|                         |                                        |
|                         | 启用分页，设置CR4.PAE                   |
|                         v                                        |
|  长模式 (Long Mode, 64位)                                        |
|  +----------------------------------------------------------+   |
|  | - 64位寻址                                                |   |
|  | - 扩展地址空间                                            |   |
|  | - 必须启用分页                                            |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

代码流程：
```c
// arch/x86/boot/main.c
void main(void)
{
    // 复制启动头
    copy_boot_params();
    
    // 控制台初始化
    console_init();
    
    // 堆初始化
    init_heap();
    
    // 检测内存
    detect_memory();
    
    // 键盘初始化
    keyboard_init();
    
    // 查询硬件信息
    query_ist();
    query_apm_bios();
    
    // 切换到保护模式
    go_to_protected_mode();
}

// arch/x86/boot/pm.c
void go_to_protected_mode(void)
{
    // 设置IDT
    setup_idt();
    
    // 设置GDT
    setup_gdt();
    
    // 重置协处理器
    reset_coprocessor();
    
    // 屏蔽PIC中断
    mask_all_interrupts();
    
    // 实际切换
    protected_mode_jump(boot_params.hdr.code32_start,
                        (u32)&boot_params + (ds() << 4));
}
```

关键寄存器：
```asm
# CR0 - 控制寄存器0
CR0.PE = 1    # 保护模式使能
CR0.PG = 1    # 分页使能

# CR4 - 控制寄存器4
CR4.PAE = 1   # 物理地址扩展

# EFER MSR - 扩展功能使能寄存器
EFER.LME = 1  # 长模式使能
EFER.LMA = 1  # 长模式激活（只读）
```
[Advanced]

---

## 5. Kernel Decompression (内核解压)

---

Q: How is the compressed kernel decompressed?
A: 内核解压过程：

```
+------------------------------------------------------------------+
|                  Kernel Decompression                             |
+------------------------------------------------------------------+
|                                                                  |
|  vmlinuz 结构:                                                   |
|  +----------------------------------------------------------+   |
|  | setup.bin      | 实模式代码和数据                          |   |
|  +----------------------------------------------------------+   |
|  | vmlinux.bin    | 压缩的内核                                |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  解压流程:                                                       |
|                                                                  |
|  1. Bootloader加载vmlinuz到内存                                  |
|                         |                                        |
|                         v                                        |
|  2. 执行实模式代码 (setup.bin)                                   |
|                         |                                        |
|                         v                                        |
|  3. 切换到保护模式/长模式                                        |
|                         |                                        |
|                         v                                        |
|  4. 跳转到解压代码 (arch/x86/boot/compressed/)                   |
|                         |                                        |
|                         v                                        |
|  5. 解压vmlinux到最终位置                                        |
|     extract_kernel()                                             |
|                         |                                        |
|                         v                                        |
|  6. 跳转到解压后的内核入口                                       |
|     startup_64 (arch/x86/kernel/head_64.S)                      |
|                                                                  |
+------------------------------------------------------------------+
```

解压代码：
```c
// arch/x86/boot/compressed/misc.c
asmlinkage __visible void *extract_kernel(void *rmode, memptr heap,
                                           unsigned char *input_data,
                                           unsigned long input_len,
                                           unsigned char *output,
                                           unsigned long output_len)
{
    // 初始化
    sanitize_boot_params(boot_params);
    
    // 选择输出位置
    choose_random_location(input_data, input_len,
                           output, output_len,
                           &output, &virt_addr);
    
    // 解压内核
    __decompress(input_data, input_len, NULL, NULL,
                 output, output_len, NULL, error);
    
    // 解析ELF并重定位
    parse_elf(output);
    handle_relocations(output, output_len, virt_addr);
    
    return output;
}
```

支持的压缩算法：
```c
// 编译时选择
CONFIG_KERNEL_GZIP      // gzip压缩
CONFIG_KERNEL_BZIP2     // bzip2压缩
CONFIG_KERNEL_LZMA      // lzma压缩
CONFIG_KERNEL_XZ        // xz压缩
CONFIG_KERNEL_LZO       // lzo压缩
CONFIG_KERNEL_LZ4       // lz4压缩
CONFIG_KERNEL_ZSTD      // zstd压缩
```
[Intermediate]

---

## 6. start_kernel() (内核初始化入口)

---

Q: What happens in start_kernel()?
A: `start_kernel()`是内核初始化的核心：

```c
// init/main.c
asmlinkage __visible void __init start_kernel(void)
{
    // ======== 早期初始化 ========
    set_task_stack_end_magic(&init_task);
    smp_setup_processor_id();
    
    // 调试/追踪初始化
    cgroup_init_early();
    local_irq_disable();  // 禁用中断
    boot_cpu_init();
    page_address_init();
    
    // 打印版本信息
    pr_notice("%s", linux_banner);
    
    // 架构相关初始化
    setup_arch(&command_line);
    
    // ======== 核心子系统初始化 ========
    // 内存管理
    mm_init_cpumask(&init_mm);
    setup_per_cpu_areas();
    smp_prepare_boot_cpu();
    build_all_zonelists(NULL);
    page_alloc_init();
    
    // 命令行处理
    parse_early_param();
    parse_args("Booting kernel", ...);
    
    // 调度器初始化
    sched_init();
    
    // IRQ初始化
    early_irq_init();
    init_IRQ();
    
    // 时间初始化
    init_timers();
    hrtimers_init();
    softirq_init();
    timekeeping_init();
    time_init();
    
    // RCU初始化
    rcu_init();
    
    // ======== 控制台初始化 ========
    console_init();
    
    // ======== 内存初始化 ========
    mem_init();
    kmem_cache_init();
    
    // ======== 其他子系统 ========
    calibrate_delay();        // 计算bogomips
    pidmap_init();
    anon_vma_init();
    thread_stack_cache_init();
    cred_init();
    fork_init();
    proc_caches_init();
    buffer_init();
    key_init();
    security_init();
    vfs_caches_init();
    signals_init();
    
    // ======== 最终初始化 ========
    proc_root_init();
    cpuset_init();
    cgroup_init();
    
    // 启用调度
    sched_clock_postinit();
    
    // ======== 启动其他CPU ========
    smp_init();
    
    // ======== 启动init进程 ========
    arch_call_rest_init();
    // -> rest_init()
    // -> kernel_init()
    // -> run_init_process()
}
```
[Intermediate]

---

Q: What is the call flow from start_kernel to init process?
A: 从start_kernel到init进程的调用链：

```
start_kernel()
    |
    v
arch_call_rest_init()
    |
    v
rest_init()
    |
    +---> kernel_thread(kernel_init, ...)  // 创建init进程 (PID 1)
    |
    +---> kernel_thread(kthreadd, ...)     // 创建kthreadd (PID 2)
    |
    +---> cpu_startup_entry()              // 成为idle进程 (PID 0)
    
kernel_init()  [PID 1]
    |
    +---> kernel_init_freeable()
    |         |
    |         +---> do_basic_setup()
    |         |         |
    |         |         +---> driver_init()
    |         |         +---> do_initcalls()  // 执行所有initcall
    |         |
    |         +---> prepare_namespace()
    |         |         |
    |         |         +---> mount_root()    // 挂载根文件系统
    |         |
    |         +---> integrity_load_keys()
    |
    +---> run_init_process()               // 执行init程序
              |
              +---> 尝试 /sbin/init
              +---> 尝试 /etc/init
              +---> 尝试 /bin/init
              +---> 尝试 /bin/sh
```

代码实现：
```c
// init/main.c
static int __ref kernel_init(void *unused)
{
    // 等待kthreadd就绪
    wait_for_completion(&kthreadd_done);
    
    // 可释放的初始化
    kernel_init_freeable();
    
    // 释放init内存
    async_synchronize_full();
    free_initmem();
    mark_readonly();
    
    // 设置系统状态
    system_state = SYSTEM_RUNNING;
    
    // 执行init程序
    if (ramdisk_execute_command) {
        ret = run_init_process(ramdisk_execute_command);
        if (!ret)
            return 0;
    }
    
    if (execute_command) {
        ret = run_init_process(execute_command);
        if (!ret)
            return 0;
    }
    
    // 尝试默认init
    if (!try_to_run_init_process("/sbin/init") ||
        !try_to_run_init_process("/etc/init") ||
        !try_to_run_init_process("/bin/init") ||
        !try_to_run_init_process("/bin/sh"))
        return 0;
    
    panic("No working init found.");
}
```
[Intermediate]

---

## 7. initcall Mechanism (initcall机制)

---

Q: What is the initcall mechanism?
A: initcall是内核模块初始化的机制：

```c
// include/linux/init.h
// initcall级别（按顺序执行）
#define pure_initcall(fn)           __define_initcall(fn, 0)
#define core_initcall(fn)           __define_initcall(fn, 1)
#define core_initcall_sync(fn)      __define_initcall(fn, 1s)
#define postcore_initcall(fn)       __define_initcall(fn, 2)
#define postcore_initcall_sync(fn)  __define_initcall(fn, 2s)
#define arch_initcall(fn)           __define_initcall(fn, 3)
#define arch_initcall_sync(fn)      __define_initcall(fn, 3s)
#define subsys_initcall(fn)         __define_initcall(fn, 4)
#define subsys_initcall_sync(fn)    __define_initcall(fn, 4s)
#define fs_initcall(fn)             __define_initcall(fn, 5)
#define fs_initcall_sync(fn)        __define_initcall(fn, 5s)
#define rootfs_initcall(fn)         __define_initcall(fn, rootfs)
#define device_initcall(fn)         __define_initcall(fn, 6)
#define device_initcall_sync(fn)    __define_initcall(fn, 6s)
#define late_initcall(fn)           __define_initcall(fn, 7)
#define late_initcall_sync(fn)      __define_initcall(fn, 7s)

// module_init是device_initcall的别名
#define module_init(x)  __initcall(x);
#define __initcall(fn)  device_initcall(fn)
```

initcall执行：
```c
// init/main.c
static void __init do_initcalls(void)
{
    int level;
    
    for (level = 0; level < ARRAY_SIZE(initcall_levels) - 1; level++)
        do_initcall_level(level);
}

static void __init do_initcall_level(int level)
{
    initcall_entry_t *fn;
    
    for (fn = initcall_levels[level]; fn < initcall_levels[level+1]; fn++)
        do_one_initcall(initcall_from_entry(fn));
}

int __init_or_module do_one_initcall(initcall_t fn)
{
    int ret;
    
    // 追踪开始
    trace_initcall_start(fn);
    
    // 执行initcall
    ret = fn();
    
    // 追踪结束
    trace_initcall_finish(fn, ret);
    
    return ret;
}
```

initcall级别说明：
```
+--------+------------------+----------------------------------+
| 级别   | 名称             | 用途                             |
+--------+------------------+----------------------------------+
| 0      | pure             | 纯初始化，无依赖                 |
| 1      | core             | 核心子系统                       |
| 2      | postcore         | 核心后期                         |
| 3      | arch             | 架构相关                         |
| 4      | subsys           | 子系统初始化                     |
| 5      | fs               | 文件系统                         |
| rootfs | rootfs           | 根文件系统                       |
| 6      | device           | 设备驱动（module_init）          |
| 7      | late             | 延迟初始化                       |
+--------+------------------+----------------------------------+
```
[Intermediate]

---

Q: How to trace initcall execution?
A: 追踪initcall执行：

```bash
# 内核命令行参数
initcall_debug           # 打印每个initcall的执行时间
initcall_blacklist=fn1,fn2  # 黑名单，跳过特定initcall

# dmesg输出示例
[    0.000000] calling  early_irq_init+0x0/0x1e @ 1
[    0.000000] initcall early_irq_init+0x0/0x1e returned 0 after 0 usecs
[    0.000000] calling  init_IRQ+0x0/0x23 @ 1
[    0.000000] initcall init_IRQ+0x0/0x23 returned 0 after 0 usecs

# 查看initcall耗时
dmesg | grep "initcall.*returned.*after"

# 内核配置
CONFIG_KALLSYMS=y        # 需要符号表
CONFIG_PRINTK_TIME=y     # 打印时间戳
```

自定义initcall示例：
```c
// 定义初始化函数
static int __init my_early_init(void)
{
    pr_info("My early initialization\n");
    return 0;
}

static int __init my_device_init(void)
{
    pr_info("My device initialization\n");
    return 0;
}

static int __init my_late_init(void)
{
    pr_info("My late initialization\n");
    return 0;
}

// 注册不同级别的initcall
core_initcall(my_early_init);      // 早期
device_initcall(my_device_init);   // 设备级（等同于module_init）
late_initcall(my_late_init);       // 延迟

// __init标记：代码在初始化后可释放
// __initdata标记：数据在初始化后可释放
```
[Intermediate]

---

## 8. Memory Initialization (内存初始化)

---

Q: How is memory initialized during boot?
A: 内存初始化阶段：

```
+------------------------------------------------------------------+
|                  Memory Initialization Stages                     |
+------------------------------------------------------------------+
|                                                                  |
|  1. BIOS/EFI提供内存映射 (E820)                                  |
|     +----------------------------------------------------------+ |
|     | 类型1: 可用内存 (usable)                                  | |
|     | 类型2: 保留内存 (reserved)                                | |
|     | 类型3: ACPI可回收                                         | |
|     | 类型4: ACPI NVS                                           | |
|     +----------------------------------------------------------+ |
|                         |                                        |
|                         v                                        |
|  2. 早期内存分配器 (memblock)                                    |
|     +----------------------------------------------------------+ |
|     | - 简单的启动时内存分配器                                  | |
|     | - 维护可用/保留内存区域                                   | |
|     | - 用于页表、per-cpu数据等                                 | |
|     +----------------------------------------------------------+ |
|                         |                                        |
|                         v                                        |
|  3. 页表初始化                                                   |
|     +----------------------------------------------------------+ |
|     | - 建立恒等映射                                            | |
|     | - 建立内核映射                                            | |
|     | - 启用分页                                                | |
|     +----------------------------------------------------------+ |
|                         |                                        |
|                         v                                        |
|  4. Zone和Node初始化                                             |
|     +----------------------------------------------------------+ |
|     | - ZONE_DMA, ZONE_DMA32, ZONE_NORMAL, ZONE_HIGHMEM        | |
|     | - NUMA节点初始化                                          | |
|     +----------------------------------------------------------+ |
|                         |                                        |
|                         v                                        |
|  5. 伙伴系统初始化                                               |
|     +----------------------------------------------------------+ |
|     | - 将memblock区域转换为页                                  | |
|     | - 初始化free_area链表                                     | |
|     +----------------------------------------------------------+ |
|                         |                                        |
|                         v                                        |
|  6. slab分配器初始化                                             |
|     +----------------------------------------------------------+ |
|     | - kmem_cache_init()                                       | |
|     | - 创建通用缓存                                            | |
|     +----------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

E820内存映射：
```c
// arch/x86/kernel/e820.c
void __init e820__memory_setup(void)
{
    // 从BIOS获取内存映射
    e820__memory_setup_default();
    
    // 打印内存映射
    e820__print_table("BIOS-provided");
}

// dmesg输出示例
// BIOS-provided physical RAM map:
// BIOS-e820: [mem 0x0000000000000000-0x000000000009fbff] usable
// BIOS-e820: [mem 0x000000000009fc00-0x000000000009ffff] reserved
// BIOS-e820: [mem 0x00000000000f0000-0x00000000000fffff] reserved
// BIOS-e820: [mem 0x0000000000100000-0x00000000dffeffff] usable
// BIOS-e820: [mem 0x00000000dfff0000-0x00000000dfffffff] ACPI data
```

memblock API：
```c
// 分配内存
void *memblock_alloc(phys_addr_t size, phys_addr_t align);

// 保留内存
int memblock_reserve(phys_addr_t base, phys_addr_t size);

// 释放内存
int memblock_free(phys_addr_t base, phys_addr_t size);

// 查看memblock状态
cat /sys/kernel/debug/memblock/memory
cat /sys/kernel/debug/memblock/reserved
```
[Advanced]

---

## 9. initramfs/initrd (初始RAM文件系统)

---

Q: What is initramfs and how does it work?
A: initramfs是早期用户空间：

```
+------------------------------------------------------------------+
|                    initramfs Purpose                              |
+------------------------------------------------------------------+
|                                                                  |
|  问题：内核如何挂载根文件系统？                                   |
|  - 根文件系统可能在各种设备上（RAID、LVM、加密、网络）             |
|  - 需要加载驱动才能访问这些设备                                   |
|  - 但驱动在根文件系统上...                                        |
|                                                                  |
|  解决方案：initramfs                                              |
|  - 包含必要驱动和工具的临时根文件系统                             |
|  - 被打包在内核镜像中或单独加载                                   |
|  - 用于：                                                        |
|    * 加载存储驱动                                                 |
|    * 组装RAID/LVM                                                 |
|    * 解密LUKS                                                     |
|    * 网络启动(NFS root)                                          |
|    * 其他早期初始化                                               |
|                                                                  |
+------------------------------------------------------------------+

initramfs vs initrd:
+------------------+------------------+
|    initramfs     |     initrd       |
+------------------+------------------+
| cpio归档          | 块设备镜像        |
| 直接解压到rootfs  | 需要文件系统驱动  |
| 简单高效          | 复杂             |
| 现代方式          | 旧方式           |
+------------------+------------------+
```

initramfs处理：
```c
// init/initramfs.c
static int __init populate_rootfs(void)
{
    // 解压initramfs到rootfs
    char *err = unpack_to_rootfs(__initramfs_start, __initramfs_size);
    if (err)
        panic("%s", err);
    
    // 处理外部initrd
    if (initrd_start) {
        err = unpack_to_rootfs((char *)initrd_start,
                               initrd_end - initrd_start);
    }
    
    return 0;
}
rootfs_initcall(populate_rootfs);
```

创建initramfs：
```bash
# 使用mkinitcpio (Arch Linux)
mkinitcpio -p linux

# 使用dracut (Fedora/RHEL)
dracut --force /boot/initramfs-$(uname -r).img $(uname -r)

# 使用update-initramfs (Debian/Ubuntu)
update-initramfs -u

# 手动创建
find . | cpio -o -H newc | gzip > /boot/initramfs.img

# 查看内容
lsinitrd /boot/initramfs.img        # dracut
lsinitcpio /boot/initramfs.img      # mkinitcpio
zcat initramfs.img | cpio -tv       # 通用
```

initramfs中的init脚本：
```bash
#!/bin/sh
# /init - initramfs中的init脚本

# 挂载必要的文件系统
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev

# 加载必要驱动
modprobe ext4
modprobe sd_mod

# 找到根设备
# 可能需要等待设备出现
for i in $(seq 1 10); do
    [ -b /dev/sda2 ] && break
    sleep 1
done

# 挂载真正的根文件系统
mount -o ro /dev/sda2 /mnt/root

# 切换到真正的根
exec switch_root /mnt/root /sbin/init
```
[Intermediate]

---

## 10. Kernel Command Line (内核命令行)

---

Q: How does kernel command line work?
A: 内核命令行参数处理：

```c
// 参数传递方式
// 1. Bootloader传递（GRUB的linux行）
// 2. 编译时默认（CONFIG_CMDLINE）
// 3. 设备树（chosen节点）

// 参数解析
// init/main.c
static int __init unknown_bootoption(char *param, char *val, ...)
{
    // 未知参数处理
}

void __init parse_early_param(void)
{
    // 解析早期参数（在内存初始化前）
    parse_early_options(boot_command_line);
}

// 参数定义宏
// include/linux/init.h

// 早期参数（在setup_arch之前）
#define early_param(str, fn) \
    __setup_param(str, fn, fn, 1)

// 普通参数
#define __setup(str, fn) \
    __setup_param(str, fn, fn, 0)

// 模块参数
#define module_param(name, type, perm) \
    module_param_named(name, name, type, perm)
```

常用内核参数：
```bash
# 启动相关
init=/bin/sh              # 指定init程序
root=/dev/sda2            # 根文件系统设备
rootfstype=ext4           # 根文件系统类型
ro / rw                   # 只读/读写挂载根
rootwait                  # 等待根设备出现
rootdelay=5               # 延迟N秒

# 控制台
console=ttyS0,115200      # 串口控制台
console=tty0              # 虚拟控制台
loglevel=7                # 日志级别
quiet                     # 安静启动
debug                     # 调试模式

# 内存
mem=4G                    # 限制内存
memmap=256M$0x100000000   # 保留内存区域
hugepages=100             # 大页数量
crashkernel=256M          # kdump预留内存

# CPU/调度
maxcpus=4                 # 最大CPU数
nosmp                     # 禁用SMP
isolcpus=2,3              # 隔离CPU
nohz=off                  # 禁用动态tick

# 安全
selinux=0/1               # SELinux开关
enforcing=0/1             # SELinux模式
apparmor=1                # AppArmor

# 调试
initcall_debug            # initcall调试
earlyprintk=serial        # 早期打印
nokaslr                   # 禁用KASLR
panic=10                  # panic后重启时间
```

查看和修改：
```bash
# 查看当前命令行
cat /proc/cmdline

# GRUB临时修改
# 启动菜单按e编辑

# GRUB永久修改
# /etc/default/grub
GRUB_CMDLINE_LINUX="quiet splash"
update-grub
```
[Intermediate]

---

## 11. SMP Boot (多处理器启动)

---

Q: How are additional CPUs brought up?
A: 多处理器启动流程：

```
+------------------------------------------------------------------+
|                    SMP Boot Process                               |
+------------------------------------------------------------------+
|                                                                  |
|  BSP (Bootstrap Processor) - CPU 0                               |
|     |                                                            |
|     | 执行start_kernel()                                         |
|     |                                                            |
|     v                                                            |
|  smp_init()                                                      |
|     |                                                            |
|     +---> smp_cpus_done()                                        |
|     |                                                            |
|     +---> for_each_present_cpu(cpu)                              |
|     |         |                                                  |
|     |         v                                                  |
|     |     cpu_up(cpu) --> _cpu_up()                              |
|     |         |                                                  |
|     |         v                                                  |
|     |     __cpu_up(cpu, idle_thread)                             |
|     |         |                                                  |
|     |         +---> 发送SIPI到AP                                 |
|     |                                                            |
|     v                                                            |
|  APs (Application Processors) - CPU 1, 2, ...                    |
|     |                                                            |
|     | 接收SIPI，从实模式开始                                      |
|     |                                                            |
|     v                                                            |
|  secondary_startup_64()                                          |
|     |                                                            |
|     v                                                            |
|  start_secondary()                                               |
|     |                                                            |
|     +---> cpu_init()                                             |
|     +---> x86_cpuinit.setup_percpu_clockev()                     |
|     +---> cpu_startup_entry()  --> idle循环                      |
|                                                                  |
+------------------------------------------------------------------+
```

代码流程：
```c
// arch/x86/kernel/smpboot.c
void __init native_smp_prepare_cpus(unsigned int max_cpus)
{
    // 初始化每个CPU的数据
    for_each_possible_cpu(i) {
        // ...
    }
}

int native_cpu_up(unsigned int cpu, struct task_struct *tidle)
{
    // 设置初始化栈
    initial_stack = tidle->stack + THREAD_SIZE;
    
    // 唤醒AP
    err = do_boot_cpu(apicid, cpu, tidle);
    
    return 0;
}

static int do_boot_cpu(int apicid, int cpu, struct task_struct *idle)
{
    // 发送INIT IPI
    apic_icr_write(APIC_INT_LEVELTRIG | APIC_INT_ASSERT | APIC_DM_INIT, apicid);
    
    // 等待
    udelay(10000);
    
    // 发送STARTUP IPI (SIPI)
    apic_icr_write(APIC_DM_STARTUP | (start_eip >> 12), apicid);
    
    // 等待AP启动
    while (!cpu_online(cpu)) {
        // ...
    }
    
    return 0;
}

// AP启动入口
void start_secondary(void *unused)
{
    // CPU初始化
    cpu_init();
    
    // 通知BSP已就绪
    set_cpu_online(cpu, true);
    
    // 进入idle循环
    cpu_startup_entry(CPUHP_AP_ONLINE_IDLE);
}
```

CPU热插拔状态：
```c
// include/linux/cpuhotplug.h
enum cpuhp_state {
    CPUHP_OFFLINE,
    CPUHP_CREATE_THREADS,
    CPUHP_PERF_PREPARE,
    // ...
    CPUHP_AP_ONLINE,
    CPUHP_AP_ONLINE_IDLE,
    CPUHP_ONLINE,
};
```
[Advanced]

---

## 12. Device Initialization (设备初始化)

---

Q: How are devices initialized during boot?
A: 设备初始化流程：

```
+------------------------------------------------------------------+
|                  Device Initialization                            |
+------------------------------------------------------------------+
|                                                                  |
|  do_basic_setup()                                                |
|     |                                                            |
|     +---> driver_init()           // 驱动核心初始化               |
|     |         |                                                  |
|     |         +---> devtmpfs_init()                              |
|     |         +---> devices_init()                               |
|     |         +---> buses_init()                                 |
|     |         +---> classes_init()                               |
|     |         +---> platform_bus_init()                          |
|     |                                                            |
|     +---> do_initcalls()          // 执行所有initcall            |
|               |                                                  |
|               +---> arch_initcall  // PCI、ACPI枚举              |
|               |                                                  |
|               +---> subsys_initcall // 子系统（USB、网络）        |
|               |                                                  |
|               +---> device_initcall // 设备驱动                   |
|                                                                  |
|  设备发现和绑定:                                                  |
|  +----------------------------------------------------------+   |
|  | 1. 总线枚举（PCI、USB、platform）                          |   |
|  | 2. 创建设备对象                                           |   |
|  | 3. 匹配驱动程序                                           |   |
|  | 4. 调用驱动probe()                                        |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

PCI设备枚举：
```c
// drivers/pci/probe.c
// PCI枚举在arch_initcall中
static int __init pci_driver_init(void)
{
    return bus_register(&pci_bus_type);
}
postcore_initcall(pci_driver_init);

// PCI扫描
void pci_scan_child_bus(struct pci_bus *bus)
{
    for (devfn = 0; devfn < 256; devfn += 8)
        pci_scan_slot(bus, devfn);
}
```

平台设备：
```c
// 设备树解析
// drivers/of/platform.c
static int __init of_platform_default_populate_init(void)
{
    // 从设备树创建platform_device
    of_platform_default_populate(NULL, NULL, NULL);
    return 0;
}
arch_initcall_sync(of_platform_default_populate_init);
```

查看启动时设备：
```bash
# 查看设备
lspci -v               # PCI设备
lsusb -v               # USB设备
ls /sys/devices/       # 所有设备

# 查看驱动绑定
dmesg | grep -i "bound"

# 查看initcall执行
dmesg | grep "initcall"
```
[Intermediate]

---

## 13. Boot Debugging (启动调试)

---

Q: How to debug kernel boot issues?
A: 启动调试方法：

```bash
# 内核命令行调试参数
debug                     # 启用调试消息
initcall_debug            # initcall执行追踪
earlyprintk=serial,ttyS0,115200  # 早期打印到串口
console=ttyS0,115200      # 串口控制台
loglevel=8                # 最高日志级别
ignore_loglevel           # 忽略日志级别限制
bootconfig               # 启用bootconfig

# 内存调试
memtest=4                 # 内存测试
memblock=debug           # memblock调试
page_poison=1            # 页毒化

# 禁用功能以定位问题
nosmp                     # 禁用多处理器
noacpi                    # 禁用ACPI
acpi=off                  # 完全禁用ACPI
noapic                    # 禁用APIC
nomodeset                 # 禁用内核模式设置
nokaslr                   # 禁用地址随机化

# 恢复模式
init=/bin/sh              # 直接进入shell
rescue                    # 救援模式
single / 1                # 单用户模式

# 查看启动日志
dmesg                     # 内核消息
journalctl -b             # 当前启动日志
journalctl -b -1          # 上次启动日志
cat /var/log/boot.log     # 启动日志文件
```

QEMU调试：
```bash
# 使用QEMU调试内核
qemu-system-x86_64 \
    -kernel bzImage \
    -initrd initramfs.img \
    -append "console=ttyS0 nokaslr" \
    -nographic \
    -s -S  # GDB服务器

# 连接GDB
gdb vmlinux
(gdb) target remote :1234
(gdb) break start_kernel
(gdb) continue
```

常见启动问题：
```
+--------+-------------------+--------------------------------+
| 症状   | 可能原因          | 解决方法                       |
+--------+-------------------+--------------------------------+
| 无输出 | 控制台配置错误    | 检查console参数                |
| 卡住   | 驱动挂起          | 添加initcall_debug             |
| panic  | 根设备未找到      | 检查root参数，添加rootwait     |
| oops   | 内存/驱动问题     | 检查栈回溯，禁用可疑模块        |
| 无法   | initramfs问题     | 重建initramfs                  |
| 登录   | init进程失败      | 检查init=参数                  |
+--------+-------------------+--------------------------------+
```

printk调试：
```c
// 添加调试打印
pr_debug("Debug message\n");       // KERN_DEBUG
pr_info("Info message\n");         // KERN_INFO
pr_notice("Notice message\n");     // KERN_NOTICE
pr_warn("Warning message\n");      // KERN_WARNING
pr_err("Error message\n");         // KERN_ERR
pr_crit("Critical message\n");     // KERN_CRIT
pr_emerg("Emergency message\n");   // KERN_EMERG

// 早期打印（在控制台初始化前）
early_printk("Early message\n");
```
[Basic]

---

## 14. Summary (总结)

---

Q: Summarize the Linux kernel boot process.
A: 
```
+==================================================================+
||                 LINUX KERNEL BOOT PROCESS                      ||
+==================================================================+

完整启动流程:
+------------------------------------------------------------------+
| 1. 固件 (BIOS/UEFI)                                              |
|    - POST自检                                                    |
|    - 加载bootloader                                              |
+------------------------------------------------------------------+
          |
          v
+------------------------------------------------------------------+
| 2. Bootloader (GRUB)                                             |
|    - 显示菜单                                                    |
|    - 加载vmlinuz + initramfs                                     |
|    - 传递内核参数                                                |
+------------------------------------------------------------------+
          |
          v
+------------------------------------------------------------------+
| 3. 实模式代码 (arch/x86/boot/)                                   |
|    - header.S -> main.c                                          |
|    - 检测硬件、内存                                              |
|    - 切换到保护模式/长模式                                       |
+------------------------------------------------------------------+
          |
          v
+------------------------------------------------------------------+
| 4. 解压代码 (arch/x86/boot/compressed/)                          |
|    - 解压vmlinux                                                 |
|    - 重定位内核                                                  |
+------------------------------------------------------------------+
          |
          v
+------------------------------------------------------------------+
| 5. start_kernel() (init/main.c)                                  |
|    - 初始化各子系统                                              |
|    - 内存、调度、IRQ、时间...                                    |
|    - do_initcalls()                                              |
+------------------------------------------------------------------+
          |
          v
+------------------------------------------------------------------+
| 6. rest_init()                                                   |
|    - 创建kernel_init (PID 1)                                     |
|    - 创建kthreadd (PID 2)                                        |
|    - BSP成为idle (PID 0)                                         |
+------------------------------------------------------------------+
          |
          v
+------------------------------------------------------------------+
| 7. kernel_init()                                                 |
|    - 解压initramfs                                               |
|    - 执行/init (initramfs中)                                     |
|    - 或挂载根文件系统                                            |
|    - 执行/sbin/init                                              |
+------------------------------------------------------------------+
          |
          v
+------------------------------------------------------------------+
| 8. systemd/init                                                  |
|    - 启动系统服务                                                |
|    - 进入多用户模式                                              |
+------------------------------------------------------------------+


关键数据结构:
+------------------+------------------------------------------+
| boot_params      | 启动参数（BIOS数据、内存映射等）           |
| setup_header     | 内核setup头                              |
| e820_table       | 内存映射表                               |
| init_task        | 第一个进程（swapper/idle）               |
+------------------+------------------------------------------+


关键函数调用链:
start_kernel()
    |
    +---> setup_arch()           // 架构初始化
    +---> mm_init()              // 内存管理初始化
    +---> sched_init()           // 调度器初始化
    +---> init_IRQ()             // 中断初始化
    +---> time_init()            // 时间初始化
    +---> console_init()         // 控制台初始化
    +---> rest_init()            // 创建init进程
              |
              +---> kernel_init()
                        |
                        +---> do_basic_setup()
                        |          |
                        |          +---> do_initcalls()
                        |
                        +---> run_init_process()


initcall级别:
    pure(0) -> core(1) -> postcore(2) -> arch(3) -> 
    subsys(4) -> fs(5) -> rootfs -> device(6) -> late(7)
```
[Basic]

---

*Total: 100+ cards covering Linux kernel startup and boot process*

