# Linux 系统启动完整流程深入分析

从上电到 init 进程启动的全过程 (基于 x86 架构和 Linux 3.2 内核)

---

## 目录

- [启动流程简图](#启动流程简图)
- [启动流程概览](#启动流程概览)
- [阶段一: 硬件上电与 BIOS](#阶段一-硬件上电与-bios)
- [阶段二: BootLoader (GRUB)](#阶段二-bootloader-grub)
- [阶段三: 内核实模式启动 (setup)](#阶段三-内核实模式启动-setup)
- [阶段四: 保护模式与内核解压](#阶段四-保护模式与内核解压)
- [阶段五: 内核初始化 (start_kernel)](#阶段五-内核初始化-start_kernel)
- [阶段六: 启动 init 进程](#阶段六-启动-init-进程)
- [关键源码文件](#关键源码文件)

---

## 启动流程概览
```
上电 → BIOS POST → MBR (0x7C00) → GRUB → setup.bin (实模式)
                                            │
                                            ▼
                                      go_to_protected_mode()
                                            │
                                            ▼
                              startup_32 (解压) → startup_32 (内核)
                                            │
                                            ▼
                                      start_kernel()
                                            │
              ┌─────────────────────────────┼─────────────────────────────┐
              ▼                             ▼                             ▼
         trap_init()                   sched_init()                  init_IRQ()
         mm_init()                     vfs_caches_init()             ...
              │                             │                             │
              └─────────────────────────────┼─────────────────────────────┘
                                            │
                                            ▼
                                       rest_init()
                                            │
              ┌─────────────────────────────┼─────────────────────────────┐
              ▼                             ▼                             ▼
         PID 0: idle                   PID 1: init                   PID 2: kthreadd
         (cpu_idle)                    (kernel_init)                 (内核线程父)
                                            │
                                            ▼
                                       init_post()
                                            │
                                            ▼
                                   run_init_process("/sbin/init")
                                            │
                                            ▼
                                      用户态 init 进程
```

## 启动流程概览

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Linux 系统启动完整流程                               │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐
│  上电   │──►│  BIOS   │──►│  MBR    │──►│ GRUB    │──►│  内核   │──►│  init   │
│ Reset   │   │  POST   │   │ Stage1  │   │ Stage2  │   │ 启动    │   │ 进程    │
└─────────┘   └─────────┘   └─────────┘   └─────────┘   └─────────┘   └─────────┘
     │             │             │             │             │             │
     │             │             │             │             │             │
     ▼             ▼             ▼             ▼             ▼             ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│  0xFFFF:FFF0   0x7C00      GRUB        setup.bin    vmlinux     /sbin/init  │
│  (BIOS入口)   (MBR加载)   (配置/选择)  (实模式)    (保护模式)   (用户态)    │
│                                                                              │
│  ──────────────────────────────────────────────────────────────────────────  │
│  实模式 16位              实模式 16位              保护模式 32/64位          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘

时间线:
────────────────────────────────────────────────────────────────────────────────►
     │           │              │               │                │
   上电       ~1秒           ~2秒            ~3秒            ~5秒+
   BIOS      GRUB显示       内核加载        内核初始化      init启动
```

---

## 阶段一: 硬件上电与 BIOS

### 1.1 CPU 复位

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CPU 复位状态                                       │
│                                                                              │
│  上电瞬间，CPU 执行硬件复位:                                                  │
│                                                                              │
│  1. 所有寄存器初始化为预定值                                                  │
│  2. CS:IP 指向 0xFFFF:FFF0 (0xFFFFFFF0)                                     │
│  3. CPU 处于实模式 (Real Mode)                                               │
│  4. 寻址能力: 1MB (20位地址线)                                               │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │  地址空间 (实模式 1MB)                                                  │ │
│  │                                                                         │ │
│  │  0x00000 ┌────────────────┐                                            │ │
│  │          │  中断向量表     │ 1KB                                        │ │
│  │  0x00400 ├────────────────┤                                            │ │
│  │          │  BIOS 数据区   │                                            │ │
│  │  0x00500 ├────────────────┤                                            │ │
│  │          │                │                                            │ │
│  │          │  可用内存      │                                            │ │
│  │          │                │                                            │ │
│  │  0x7C00  │ ◄─ MBR 加载处  │                                            │ │
│  │          │                │                                            │ │
│  │  0x9FC00 ├────────────────┤                                            │ │
│  │          │  扩展 BIOS 数据│                                            │ │
│  │  0xA0000 ├────────────────┤                                            │ │
│  │          │  显存          │                                            │ │
│  │  0xC0000 ├────────────────┤                                            │ │
│  │          │  扩展 ROM      │                                            │ │
│  │  0xF0000 ├────────────────┤                                            │ │
│  │          │  BIOS ROM      │ ◄─ CPU 从这里开始执行                       │ │
│  │  0xFFFFF └────────────────┘                                            │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.2 BIOS POST (Power-On Self-Test)

```
BIOS 执行流程:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. POST (加电自检)
   ├── CPU 测试
   ├── 内存检测 (计数显示)
   ├── 键盘初始化
   ├── 显卡初始化
   └── 外设检测

2. 设备枚举
   ├── PCI/PCIe 设备扫描
   ├── USB 设备枚举
   └── 存储设备检测

3. 中断向量表初始化
   └── 设置 BIOS 中断服务程序

4. 启动设备选择
   ├── 读取 CMOS 中的启动顺序
   └── 从第一个可启动设备加载 MBR

5. 加载 MBR
   ├── 读取第一个扇区 (512 字节)
   ├── 加载到 0x7C00
   ├── 检查 MBR 签名 (0x55AA)
   └── 跳转到 0x7C00 执行
```

### 1.3 MBR 结构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        MBR (Master Boot Record) 结构                         │
│                              512 字节                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  偏移      大小       内容                                                    │
│  ─────────────────────────────────────────────────────────────────────────  │
│  0x000     446B      引导代码 (Bootstrap Code)                               │
│                      └── GRUB Stage 1 或其他 bootloader                      │
│                                                                              │
│  0x1BE     16B       分区表项 1                                              │
│  0x1CE     16B       分区表项 2                                              │
│  0x1DE     16B       分区表项 3                                              │
│  0x1EE     16B       分区表项 4                                              │
│                                                                              │
│  0x1FE     2B        MBR 签名: 0x55 0xAA                                     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘

分区表项结构 (16 字节):
┌───────┬─────────┬─────────────┬─────────────┬─────────────┬────────────┐
│ 状态   │ 起始CHS │   分区类型   │  结束CHS    │  起始LBA    │   大小     │
│ 1B    │  3B     │     1B      │    3B       │    4B       │    4B      │
└───────┴─────────┴─────────────┴─────────────┴─────────────┴────────────┘
```

---

## 阶段二: BootLoader (GRUB)

### 2.1 GRUB 多阶段加载

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        GRUB 加载过程                                         │
└─────────────────────────────────────────────────────────────────────────────┘

Stage 1 (MBR 中, 446 字节)
    │
    │  读取 Stage 1.5 或 Stage 2
    │
    ▼
Stage 1.5 (MBR 后的空隙, ~30KB)
    │
    │  包含文件系统驱动
    │  可以读取 /boot 分区
    │
    ▼
Stage 2 (/boot/grub/)
    │
    ├── 显示启动菜单
    ├── 读取 grub.cfg 配置
    ├── 加载内核镜像 (vmlinuz)
    ├── 加载 initrd/initramfs
    └── 传递启动参数

    ┌─────────────────────────────────────────────────────────────────────┐
    │                      GRUB 菜单示例                                   │
    │                                                                     │
    │  ┌──────────────────────────────────────────────────────────────┐   │
    │  │  GNU GRUB  version 2.02                                      │   │
    │  │                                                              │   │
    │  │  *Ubuntu                                                     │   │
    │  │   Ubuntu (recovery mode)                                     │   │
    │  │   Advanced options for Ubuntu                                │   │
    │  │   Memory test (memtest86+)                                   │   │
    │  │                                                              │   │
    │  │  Use the ↑ and ↓ keys to select which entry is highlighted.  │   │
    │  └──────────────────────────────────────────────────────────────┘   │
    └─────────────────────────────────────────────────────────────────────┘
```

### 2.2 GRUB 加载内核

```bash
# grub.cfg 典型配置
menuentry 'Linux' {
    set root='hd0,msdos1'
    linux   /boot/vmlinuz-3.2.0 root=/dev/sda1 ro quiet
    initrd  /boot/initrd.img-3.2.0
}
```

**GRUB 执行步骤:**

```
1. 加载 vmlinuz 到内存
   ├── setup.bin (实模式代码) ──► 0x90000 附近
   └── vmlinux.bin (压缩内核) ──► 0x100000 (1MB) 以上

2. 加载 initrd 到内存
   └── 临时根文件系统 ──► 高端内存

3. 设置启动参数
   └── 填充 boot_params 结构

4. 跳转到内核入口
   └── 跳转到 setup.bin 的起始地址
```

### 2.3 内核镜像结构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        vmlinuz 内核镜像结构                                   │
└─────────────────────────────────────────────────────────────────────────────┘

vmlinuz 文件:
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │               setup.bin (~15KB)                                       │   │
│  │                                                                       │   │
│  │   ┌─────────────────┐                                                │   │
│  │   │  Boot Sector    │ 512B (兼容性，现已不用)                         │   │
│  │   ├─────────────────┤                                                │   │
│  │   │  Setup Header   │ boot_params 结构                               │   │
│  │   ├─────────────────┤                                                │   │
│  │   │  Setup Code     │ 实模式代码 (arch/x86/boot/)                    │   │
│  │   │  - 硬件检测     │                                                │   │
│  │   │  - 视频模式     │                                                │   │
│  │   │  - 内存探测     │                                                │   │
│  │   │  - 切换保护模式 │                                                │   │
│  │   └─────────────────┘                                                │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │               vmlinux.bin (压缩内核, 几MB)                            │   │
│  │                                                                       │   │
│  │   ┌─────────────────┐                                                │   │
│  │   │  解压存根       │ arch/x86/boot/compressed/                      │   │
│  │   │  (head_32.S)    │                                                │   │
│  │   ├─────────────────┤                                                │   │
│  │   │  压缩的内核     │ gzip/bzip2/lzma/xz 压缩                        │   │
│  │   │  (vmlinux.bin)  │                                                │   │
│  │   └─────────────────┘                                                │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 阶段三: 内核实模式启动 (setup)

### 3.1 Setup 入口 (header.S)

```asm
// arch/x86/boot/header.S

BOOTSEG     = 0x07C0      // 原始引导扇区地址
SYSSEG      = 0x1000      // 历史加载地址 >> 4

    .code16               // 16位实模式代码
    .section ".bstext", "ax"

    .global bootsect_start
bootsect_start:
    # 规范化起始地址
    ljmp    $BOOTSEG, $start2

start2:
    movw    %cs, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    xorw    %sp, %sp
    sti
    cld
    ...
```

### 3.2 Setup 主函数 (main.c)

```c
// arch/x86/boot/main.c

void main(void)
{
    /* 1. 复制启动参数到 zeropage */
    copy_boot_params();

    /* 2. 初始化早期控制台 */
    console_init();

    /* 3. 初始化堆 */
    init_heap();

    /* 4. 验证 CPU 能力 */
    if (validate_cpu()) {
        puts("Unable to boot - please use a kernel appropriate "
             "for your CPU.\n");
        die();
    }

    /* 5. 告诉 BIOS 我们打算运行的 CPU 模式 */
    set_bios_mode();

    /* 6. 检测内存布局 (E820) */
    detect_memory();

    /* 7. 设置键盘重复率 */
    keyboard_set_repeat();

    /* 8. 查询 MCA 信息 */
    query_mca();

    /* 9. 查询 Intel SpeedStep 信息 */
    query_ist();

    /* 10. 查询 APM 信息 */
    query_apm_bios();

    /* 11. 查询 EDD 信息 */
    query_edd();

    /* 12. 设置视频模式 */
    set_video();

    /* 13. 进入保护模式 */
    go_to_protected_mode();
}
```

### 3.3 内存检测 (E820)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        E820 内存映射                                         │
│                                                                              │
│  BIOS 中断 INT 15h, AX=E820h 返回内存映射:                                   │
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │  起始地址           结束地址           类型                           │   │
│  ├──────────────────────────────────────────────────────────────────────┤   │
│  │  0x0000_0000       0x0009_FFFF       可用 (640KB)                    │   │
│  │  0x000A_0000       0x000F_FFFF       保留 (显存/ROM)                 │   │
│  │  0x0010_0000       0x1FFF_FFFF       可用 (主内存)                   │   │
│  │  0x2000_0000       0x200F_FFFF       ACPI 数据                       │   │
│  │  0x2010_0000       0xFFFF_FFFF       保留                            │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│  类型:                                                                        │
│    1 = 可用内存 (Usable)                                                      │
│    2 = 保留 (Reserved)                                                        │
│    3 = ACPI 可回收                                                            │
│    4 = ACPI NVS                                                               │
│    5 = 损坏 (Bad)                                                             │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 阶段四: 保护模式与内核解压

### 4.1 切换到保护模式

```c
// arch/x86/boot/pm.c

void go_to_protected_mode(void)
{
    /* 1. 禁用中断 */
    realmode_switch_hook();

    /* 2. 开启 A20 地址线 */
    if (enable_a20()) {
        puts("A20 gate not responding, unable to boot...\n");
        die();
    }

    /* 3. 重置协处理器 */
    reset_coprocessor();

    /* 4. 屏蔽所有中断 */
    mask_all_interrupts();

    /* 5. 设置 GDT 和 IDT */
    setup_gdt();
    setup_idt();

    /* 6. 跳转到保护模式 */
    protected_mode_jump(boot_params.hdr.code32_start,
                        (u32)&boot_params + (ds() << 4));
}
```

### 4.2 保护模式跳转 (pmjump.S)

```asm
// arch/x86/boot/pmjump.S

GLOBAL(protected_mode_jump)
    movl    %edx, %esi          # 保存 boot_params 地址
    
    xorl    %ebx, %ebx
    movw    %cs, %bx
    shll    $4, %ebx
    addl    %ebx, 2f            # 修正跳转地址
    
    movl    %cr0, %edx
    orb     $X86_CR0_PE, %dl    # 设置 PE 位 (保护模式)
    movl    %edx, %cr0
    
    # 长跳转到 32 位代码段，刷新流水线
    .byte   0x66, 0xea          # ljmpl opcode
2:  .long   in_pm32             # 32位偏移
    .word   __BOOT_CS           # 代码段选择子

    .code32
in_pm32:
    # 设置数据段
    movl    $__BOOT_DS, %eax
    movl    %eax, %ds
    movl    %eax, %es
    movl    %eax, %fs
    movl    %eax, %gs
    movl    %eax, %ss
    
    # 跳转到压缩内核入口 (startup_32)
    jmpl    *%eax
```

### 4.3 内核解压 (head_32.S)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        内核解压过程                                          │
└─────────────────────────────────────────────────────────────────────────────┘

arch/x86/boot/compressed/head_32.S:

startup_32:
    │
    ├── 1. 设置栈和段寄存器
    │
    ├── 2. 计算重定位偏移
    │
    ├── 3. 如果需要，重定位自身
    │
    ├── 4. 清除 BSS 段
    │
    ├── 5. 设置页表 (如果启用 PAE/64位)
    │
    └── 6. 调用 decompress_kernel()
            │
            │  解压算法: gzip/bzip2/lzma/xz/lzo
            │
            ▼
        解压后的 vmlinux 放置在最终位置
            │
            ▼
        跳转到真正的内核入口: startup_32 (head_32.S in kernel)
```

### 4.4 真正的内核入口 (arch/x86/kernel/head_32.S)

```asm
// arch/x86/kernel/head_32.S

__HEAD
ENTRY(startup_32)
    /* 1. 设置段寄存器 */
    movl $(__BOOT_DS),%eax
    movl %eax,%ds
    movl %eax,%es
    movl %eax,%fs
    movl %eax,%gs
    movl %eax,%ss

    /* 2. 清除 BSS */
    cld
    xorl %eax,%eax
    movl $pa(__bss_start),%edi
    movl $pa(__bss_stop),%ecx
    subl %edi,%ecx
    shrl $2,%ecx
    rep ; stosl

    /* 3. 复制启动参数 */
    movl $pa(boot_params),%edi
    movl $(PARAM_SIZE/4),%ecx
    cld
    rep ; movsl

    /* 4. 设置初始页表 */
    call setup_paging

    /* 5. 加载 GDT 和 IDT */
    lgdt early_gdt_descr
    lidt idt_descr

    /* 6. 跳转到 C 代码: start_kernel */
    movl $start_kernel,%eax
    jmp *%eax
```

---

## 阶段五: 内核初始化 (start_kernel)

### 5.1 start_kernel 函数

```c
// init/main.c

asmlinkage void __init start_kernel(void)
{
    char * command_line;

    /* ===== 早期初始化 (中断禁用) ===== */
    
    smp_setup_processor_id();       // 设置处理器 ID
    lockdep_init();                  // 锁依赖检查初始化
    debug_objects_early_init();      // 调试对象初始化
    boot_init_stack_canary();        // 栈保护初始化
    cgroup_init_early();             // cgroup 早期初始化
    
    local_irq_disable();             // 禁用中断
    early_boot_irqs_disabled = true;

    /* ===== 核心子系统初始化 ===== */
    
    tick_init();                     // 时钟滴答初始化
    boot_cpu_init();                 // 启动 CPU 初始化
    page_address_init();             // 页地址映射初始化
    
    printk(KERN_NOTICE "%s", linux_banner);  // 打印 Linux 横幅
    
    setup_arch(&command_line);       // 架构相关初始化 ★
    mm_init_owner(&init_mm, &init_task);
    setup_command_line(command_line);
    setup_nr_cpu_ids();
    setup_per_cpu_areas();           // Per-CPU 变量初始化
    smp_prepare_boot_cpu();

    build_all_zonelists(NULL);       // 构建内存区域列表
    page_alloc_init();               // 页分配器初始化

    printk(KERN_NOTICE "Kernel command line: %s\n", boot_command_line);
    parse_early_param();
    parse_args("Booting kernel", ...);

    /* ===== 内存管理初始化 ===== */
    
    setup_log_buf(0);
    pidhash_init();                  // PID 哈希表初始化
    vfs_caches_init_early();         // VFS 缓存早期初始化
    sort_main_extable();             // 异常表排序
    trap_init();                     // 陷阱/异常处理初始化
    mm_init();                       // 内存管理初始化

    /* ===== 调度器初始化 ===== */
    
    sched_init();                    // 调度器初始化 ★
    preempt_disable();               // 禁用抢占

    /* ===== 中断和定时器初始化 ===== */
    
    idr_init_cache();
    perf_event_init();
    rcu_init();                      // RCU 初始化
    radix_tree_init();
    early_irq_init();
    init_IRQ();                      // 中断初始化 ★
    prio_tree_init();
    init_timers();                   // 定时器初始化
    hrtimers_init();                 // 高精度定时器初始化
    softirq_init();                  // 软中断初始化
    timekeeping_init();
    time_init();                     // 时间初始化
    
    /* ===== 启用中断 ===== */
    
    early_boot_irqs_disabled = false;
    local_irq_enable();              // 启用中断 ★

    /* ===== 更多子系统初始化 ===== */
    
    kmem_cache_init_late();          // SLAB 缓存后期初始化
    console_init();                  // 控制台初始化 ★
    
    if (panic_later)
        panic(panic_later, panic_param);

    lockdep_info();
    
    /* ===== 内存检测完成 ===== */
    
    mem_init();                      // 释放引导内存
    kmem_cache_init();               // SLAB 分配器初始化
    percpu_init_late();
    pgtable_cache_init();
    vmalloc_init();                  // vmalloc 初始化

    /* ===== 进程管理初始化 ===== */
    
    proc_caches_init();              // 进程缓存初始化
    buffer_init();                   // 缓冲区初始化
    key_init();
    security_init();                 // 安全子系统初始化
    vfs_caches_init(totalram_pages); // VFS 缓存初始化 ★
    signals_init();                  // 信号初始化
    page_writeback_init();
    proc_root_init();                // /proc 初始化
    cgroup_init();                   // cgroup 初始化
    cpuset_init();
    taskstats_init_early();
    delayacct_init();

    check_bugs();                    // CPU bug 检测

    acpi_early_init();               // ACPI 早期初始化
    sfi_init_late();

    ftrace_init();                   // ftrace 初始化

    /* ===== 启动其余初始化 ===== */
    
    rest_init();                     // 启动 init 进程 ★
}
```

### 5.2 start_kernel 流程图

```
start_kernel()
      │
      ├── [早期初始化 - 中断禁用]
      │       ├── smp_setup_processor_id()
      │       ├── lockdep_init()
      │       ├── boot_init_stack_canary()
      │       └── local_irq_disable()
      │
      ├── [架构初始化]
      │       ├── setup_arch()        ◄── 内存检测、设备树解析
      │       ├── setup_per_cpu_areas()
      │       └── build_all_zonelists()
      │
      ├── [核心子系统初始化]
      │       ├── trap_init()         ◄── 异常处理
      │       ├── mm_init()           ◄── 内存管理
      │       ├── sched_init()        ◄── 调度器
      │       ├── init_IRQ()          ◄── 中断
      │       └── softirq_init()      ◄── 软中断
      │
      ├── [启用中断]
      │       └── local_irq_enable()
      │
      ├── [更多子系统]
      │       ├── console_init()      ◄── 控制台
      │       ├── mem_init()          ◄── 释放引导内存
      │       ├── vfs_caches_init()   ◄── VFS
      │       ├── proc_root_init()    ◄── /proc
      │       └── signals_init()      ◄── 信号
      │
      └── rest_init()                 ◄── 启动 init 进程
```

---

## 阶段六: 启动 init 进程

### 6.1 rest_init 函数

```c
// init/main.c

static noinline void __init_refok rest_init(void)
{
    int pid;

    rcu_scheduler_starting();
    
    /*
     * 创建 kernel_init 线程 (未来的 init 进程, PID=1)
     * 但暂不运行，因为需要先创建 kthreadd
     */
    kernel_thread(kernel_init, NULL, CLONE_FS | CLONE_SIGHAND);
    
    numa_default_policy();
    
    /*
     * 创建 kthreadd 线程 (PID=2)
     * 这是所有内核线程的父进程
     */
    pid = kernel_thread(kthreadd, NULL, CLONE_FS | CLONE_FILES);
    
    rcu_read_lock();
    kthreadd_task = find_task_by_pid_ns(pid, &init_pid_ns);
    rcu_read_unlock();
    complete(&kthreadd_done);

    /*
     * 当前进程变为 idle 进程 (PID=0)
     */
    init_idle_bootup_task(current);
    preempt_enable_no_resched();
    schedule();                       // 调度其他进程运行

    /* 进入 idle 循环 */
    preempt_disable();
    cpu_idle();                       // 永不返回
}
```

### 6.2 kernel_init 函数

```c
// init/main.c

static int __init kernel_init(void * unused)
{
    /*
     * 等待 kthreadd 准备就绪
     */
    wait_for_completion(&kthreadd_done);

    /* init 可以在任何 CPU 上运行 */
    set_cpus_allowed_ptr(current, cpu_all_mask);
    
    cad_pid = task_pid(current);      // 设置 Ctrl+Alt+Del 处理进程

    /* ===== SMP 初始化 ===== */
    smp_prepare_cpus(setup_max_cpus);
    do_pre_smp_initcalls();
    lockup_detector_init();
    smp_init();                       // 启动其他 CPU ★
    sched_init_smp();

    /* ===== 设备初始化 ===== */
    do_basic_setup();                 // initcall 机制 ★

    /* ===== 打开控制台 ===== */
    if (sys_open((const char __user *) "/dev/console", O_RDWR, 0) < 0)
        printk(KERN_WARNING "Warning: unable to open an initial console.\n");

    (void) sys_dup(0);                // stdin  = /dev/console
    (void) sys_dup(0);                // stdout = /dev/console
                                      // stderr = /dev/console

    /* ===== 准备根文件系统 ===== */
    if (!ramdisk_execute_command)
        ramdisk_execute_command = "/init";

    if (sys_access((const char __user *) ramdisk_execute_command, 0) != 0) {
        ramdisk_execute_command = NULL;
        prepare_namespace();          // 挂载根文件系统
    }

    /*
     * 初始化完成，启动用户空间程序
     */
    init_post();
    return 0;
}
```

### 6.3 init_post - 执行用户态 init

```c
// init/main.c

static noinline int init_post(void)
{
    /* 等待所有异步初始化完成 */
    async_synchronize_full();
    
    /* 释放 __init 内存 */
    free_initmem();
    
    /* 标记只读数据 */
    mark_rodata_ro();
    
    /* 系统进入运行状态 */
    system_state = SYSTEM_RUNNING;
    numa_default_policy();

    /* init 进程不可被杀死 */
    current->signal->flags |= SIGNAL_UNKILLABLE;

    /* ===== 尝试执行 init 程序 ===== */
    
    /* 1. 首先尝试 ramdisk 中的 /init */
    if (ramdisk_execute_command) {
        run_init_process(ramdisk_execute_command);
        printk(KERN_WARNING "Failed to execute %s\n",
               ramdisk_execute_command);
    }

    /* 2. 尝试命令行指定的 init= */
    if (execute_command) {
        run_init_process(execute_command);
        printk(KERN_WARNING "Failed to execute %s.  Attempting "
               "defaults...\n", execute_command);
    }

    /* 3. 尝试标准位置 */
    run_init_process("/sbin/init");
    run_init_process("/etc/init");
    run_init_process("/bin/init");
    run_init_process("/bin/sh");

    /* 4. 全部失败则 panic */
    panic("No init found.  Try passing init= option to kernel. "
          "See Linux Documentation/init.txt for guidance.");
}
```

### 6.4 进程树形成

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          初始进程树                                          │
└─────────────────────────────────────────────────────────────────────────────┘

rest_init() 创建的进程:

    PID 0: idle/swapper
        │
        ├── PID 1: init (kernel_init → /sbin/init)
        │       │
        │       ├── 系统服务进程
        │       ├── 登录进程
        │       └── 用户进程
        │
        └── PID 2: kthreadd
                │
                ├── kworker/...
                ├── ksoftirqd/...
                ├── migration/...
                ├── watchdog/...
                └── 其他内核线程

进程执行流:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. start_kernel() 运行在 PID 0 上下文

2. rest_init() 创建:
   - kernel_init (将成为 PID 1)
   - kthreadd (PID 2)

3. rest_init() 调用 schedule()，让出 CPU

4. kernel_init 获得运行:
   - 完成系统初始化
   - 调用 run_init_process("/sbin/init")
   - execve() 替换为用户态程序

5. init 进程 (PID 1) 开始运行:
   - 读取 /etc/inittab 或 systemd 配置
   - 启动系统服务
   - 启动登录终端
```

### 6.5 do_basic_setup - initcall 机制

```c
static void __init do_basic_setup(void)
{
    cpuset_init_smp();
    usermodehelper_init();
    shmem_init();
    driver_init();                    // 驱动核心初始化
    init_irq_proc();
    do_ctors();
    usermodehelper_enable();
    do_initcalls();                   // 调用所有 initcall ★
}
```

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          initcall 级别                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  级别           宏                          用途                             │
│  ─────────────────────────────────────────────────────────────────────────  │
│  0    early_initcall()              早期初始化                               │
│  1    pure_initcall()               纯初始化                                 │
│  2    core_initcall()               核心子系统                               │
│  3    postcore_initcall()           核心后初始化                             │
│  4    arch_initcall()               架构相关                                 │
│  5    subsys_initcall()             子系统初始化                             │
│  6    fs_initcall()                 文件系统                                 │
│  7    device_initcall() / module_init()  设备驱动                           │
│  8    late_initcall()               延迟初始化                               │
│                                                                              │
│  执行顺序: 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 关键源码文件

### BIOS/BootLoader 阶段

| 文件 | 功能 |
|------|------|
| (GRUB 源码) | BootLoader |

### 内核实模式

| 文件 | 功能 |
|------|------|
| `arch/x86/boot/header.S` | 启动扇区头 |
| `arch/x86/boot/main.c` | 实模式 main 函数 |
| `arch/x86/boot/memory.c` | 内存检测 (E820) |
| `arch/x86/boot/video.c` | 视频模式设置 |
| `arch/x86/boot/pm.c` | 保护模式切换 |
| `arch/x86/boot/pmjump.S` | 保护模式跳转 |

### 内核解压

| 文件 | 功能 |
|------|------|
| `arch/x86/boot/compressed/head_32.S` | 解压入口 |
| `arch/x86/boot/compressed/misc.c` | 解压函数 |

### 内核初始化

| 文件 | 功能 |
|------|------|
| `arch/x86/kernel/head_32.S` | 内核入口 |
| `init/main.c` | start_kernel, rest_init, kernel_init |
| `arch/x86/kernel/setup.c` | setup_arch |
| `kernel/sched.c` | sched_init |
| `kernel/fork.c` | 进程创建 |

---

## 总结

### 启动阶段总览

| 阶段 | 执行位置 | 模式 | 主要工作 |
|------|---------|------|---------|
| BIOS | ROM | 实模式 | POST、硬件初始化、加载 MBR |
| BootLoader | 内存 | 实模式 | 加载内核、传递参数 |
| Setup | 0x90000 | 实模式 | 硬件检测、切换保护模式 |
| 解压 | 0x100000 | 保护模式 | 解压内核 |
| start_kernel | 内核 | 保护模式 | 子系统初始化 |
| init | 用户态 | 保护模式 | 系统服务启动 |

### 关键时间节点

```
上电 ──► BIOS POST (~1秒)
    ──► GRUB 菜单 (~2秒)  
    ──► 内核加载 (~1秒)
    ──► start_kernel (~2秒)
    ──► init 启动 (视系统而定)
    ──► 登录提示符
```

---

*本文档基于 Linux 3.2 内核 x86 架构源码分析*

