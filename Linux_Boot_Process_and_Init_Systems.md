# Linux 启动流程与初始化系统详解

## 目录

1. [Linux 启动流程概述](#1-linux-启动流程概述)
2. [三种初始化系统对比](#2-三种初始化系统对比)
3. [SysVinit 详解](#3-sysvinit-详解)
4. [Upstart 详解](#4-upstart-详解)
5. [systemd 详解](#5-systemd-详解)
6. [/etc/inittab 文件详解](#6-etcinittab-文件详解)
7. [Init Script 执行顺序机制](#7-init-script-执行顺序机制)

---

## 1. Linux 启动流程概述

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Linux Boot Process Overview                                     │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   Power On                                                                          │
    │       │                                                                             │
    │       ▼                                                                             │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │                         STAGE 1: BIOS/UEFI                                  │   │
    │   │                                                                             │   │
    │   │   - Power-On Self Test (POST)                                               │   │
    │   │   - Hardware initialization (CPU, RAM, peripherals)                         │   │
    │   │   - Find bootable device (HDD, SSD, USB, Network)                           │   │
    │   │   - Load bootloader from MBR/GPT                                            │   │
    │   │                                                                             │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │       │                                                                             │
    │       ▼                                                                             │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │                         STAGE 2: Bootloader (GRUB)                          │   │
    │   │                                                                             │   │
    │   │   - Display boot menu (if configured)                                       │   │
    │   │   - Load kernel image (vmlinuz) into memory                                 │   │
    │   │   - Load initial RAM disk (initramfs/initrd)                                │   │
    │   │   - Pass kernel parameters                                                  │   │
    │   │   - Transfer control to kernel                                              │   │
    │   │                                                                             │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │       │                                                                             │
    │       ▼                                                                             │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │                         STAGE 3: Kernel                                     │   │
    │   │                                                                             │   │
    │   │   - Decompress and initialize kernel                                        │   │
    │   │   - Initialize memory management                                            │   │
    │   │   - Detect and initialize hardware (drivers)                                │   │
    │   │   - Mount initramfs as temporary root                                       │   │
    │   │   - Execute /init in initramfs                                              │   │
    │   │   - Mount real root filesystem                                              │   │
    │   │   - Start /sbin/init (PID 1)                                                │   │
    │   │                                                                             │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │       │                                                                             │
    │       ▼                                                                             │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │                         STAGE 4: Init System                                │   │
    │   │                                                                             │   │
    │   │   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐                     │   │
    │   │   │  SysVinit   │ OR │   Upstart   │ OR │   systemd   │                     │   │
    │   │   │ (1983-2010) │    │ (2006-2014) │    │ (2010-now)  │                     │   │
    │   │   └─────────────┘    └─────────────┘    └─────────────┘                     │   │
    │   │                                                                             │   │
    │   │   - Start system services                                                   │   │
    │   │   - Configure networking                                                    │   │
    │   │   - Mount filesystems                                                       │   │
    │   │   - Start user login services                                               │   │
    │   │                                                                             │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │       │                                                                             │
    │       ▼                                                                             │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │                         STAGE 5: User Space                                 │   │
    │   │                                                                             │   │
    │   │   - Login prompt (getty) or Display Manager (GDM, LightDM)                  │   │
    │   │   - User session starts                                                     │   │
    │   │   - Desktop environment loads (if GUI)                                      │   │
    │   │                                                                             │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- **BIOS/UEFI 阶段**：硬件自检，找到启动设备，加载引导程序
- **Bootloader 阶段**：GRUB 加载内核和初始内存盘到内存
- **Kernel 阶段**：内核初始化硬件，挂载根文件系统，启动 init 进程
- **Init 阶段**：第一个用户空间进程（PID 1），负责启动所有其他服务
- **User Space 阶段**：用户登录，桌面环境启动

---

## 2. 三种初始化系统对比

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                    Three Init Systems Comparison                                        │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   Timeline:                                                                         │
    │                                                                                     │
    │   1983                    2006                    2010                    2024      │
    │     │                       │                       │                       │       │
    │     ├───────────────────────┼───────────────────────┼───────────────────────┤       │
    │     │      SysVinit         │       Upstart         │       systemd         │       │
    │     │    (Traditional)      │    (Transitional)     │      (Modern)         │       │
    │     │                       │                       │                       │       │
    │                                                                                     │
    │   Distributions:                                                                    │
    │                                                                                     │
    │   SysVinit:  Slackware, older Debian, older RHEL/CentOS                             │
    │   Upstart:   Ubuntu 6.10-14.10, RHEL 6, Chrome OS (early)                           │
    │   systemd:   Ubuntu 15.04+, Debian 8+, RHEL 7+, Fedora 15+, Arch, openSUSE          │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘


    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                         Feature Comparison                                          │
    │                                                                                     │
    │   ┌─────────────────┬─────────────────┬─────────────────┬─────────────────┐         │
    │   │    Feature      │    SysVinit     │     Upstart     │     systemd     │         │
    │   ├─────────────────┼─────────────────┼─────────────────┼─────────────────┤         │
    │   │ Startup Model   │   Sequential    │   Event-based   │   Parallel +    │         │
    │   │                 │   (one by one)  │   (triggers)    │   Dependencies  │         │
    │   ├─────────────────┼─────────────────┼─────────────────┼─────────────────┤         │
    │   │ Config Format   │   Shell scripts │   Stanzas       │   INI-like      │         │
    │   │                 │   + /etc/inittab│   (.conf files) │   (.service)    │         │
    │   ├─────────────────┼─────────────────┼─────────────────┼─────────────────┤         │
    │   │ Dependency      │   Implicit      │   Event-based   │   Explicit      │         │
    │   │ Management      │   (via numbers) │   (start on)    │   (After/Wants) │         │
    │   ├─────────────────┼─────────────────┼─────────────────┼─────────────────┤         │
    │   │ Parallelism     │   None          │   Limited       │   Full          │         │
    │   ├─────────────────┼─────────────────┼─────────────────┼─────────────────┤         │
    │   │ Service         │   Manual        │   Automatic     │   Automatic     │         │
    │   │ Respawn         │   (respawn)     │   (respawn)     │   (Restart=)    │         │
    │   ├─────────────────┼─────────────────┼─────────────────┼─────────────────┤         │
    │   │ Socket          │   No            │   No            │   Yes           │         │
    │   │ Activation      │                 │                 │                 │         │
    │   ├─────────────────┼─────────────────┼─────────────────┼─────────────────┤         │
    │   │ Logging         │   syslog        │   syslog        │   journald      │         │
    │   ├─────────────────┼─────────────────┼─────────────────┼─────────────────┤         │
    │   │ Complexity      │   Simple        │   Medium        │   Complex       │         │
    │   └─────────────────┴─────────────────┴─────────────────┴─────────────────┘         │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- **SysVinit**：最传统的初始化系统，使用 shell 脚本，按顺序执行，简单但慢
- **Upstart**：Ubuntu 开发的过渡方案，基于事件驱动，支持有限并行
- **systemd**：现代标准，完全并行启动，显式依赖管理，功能最强大

---

## 3. SysVinit 详解

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         SysVinit Boot Process                                           │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    Kernel starts /sbin/init (SysVinit)
           │
           ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                         Read /etc/inittab                                           │
    │                                                                                     │
    │   id:3:initdefault:                    ◄── Default runlevel = 3                     │
    │   si::sysinit:/etc/init.d/rcS          ◄── First script to run                      │
    │   l3:3:wait:/etc/init.d/rc 3           ◄── Run scripts for runlevel 3               │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
           │
           ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                         Execute /etc/init.d/rcS (System Init)                       │
    │                                                                                     │
    │   - Mount /proc, /sys filesystems                                                   │
    │   - Set hostname                                                                    │
    │   - Initialize hardware                                                             │
    │   - Load kernel modules                                                             │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
           │
           ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                         Execute /etc/init.d/rc 3                                    │
    │                                                                                     │
    │   Reads /etc/rc3.d/ directory:                                                      │
    │                                                                                     │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │  /etc/rc3.d/                                                                │   │
    │   │                                                                             │   │
    │   │  K01service_a  ──► Stop service_a  (K = Kill)                               │   │
    │   │  K05service_b  ──► Stop service_b                                           │   │
    │   │  S10network    ──► Start network   (S = Start)                              │   │
    │   │  S12syslog     ──► Start syslog                                             │   │
    │   │  S20ssh        ──► Start ssh                                                │   │
    │   │  S30cron       ──► Start cron                                               │   │
    │   │  S50apache2    ──► Start apache2                                            │   │
    │   │  S99local      ──► Start local scripts                                      │   │
    │   │                                                                             │   │
    │   │  These are symlinks to /etc/init.d/                                         │   │
    │   │                                                                             │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │                                                                                     │
    │   Execution Order:                                                                  │
    │   1. All K* scripts (stop services) - sorted by number                              │
    │   2. All S* scripts (start services) - sorted by number                             │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
           │
           ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                         Start getty (Login Prompts)                                 │
    │                                                                                     │
    │   1:2345:respawn:/sbin/getty 38400 tty1                                             │
    │   2:23:respawn:/sbin/getty 38400 tty2                                               │
    │   ...                                                                               │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
           │
           ▼
       System Ready
```

**说明**：
- SysVinit 读取 `/etc/inittab` 确定默认运行级别
- 首先执行系统初始化脚本 `/etc/init.d/rcS`
- 然后执行对应运行级别目录下的脚本（如 `/etc/rc3.d/`）
- 脚本按文件名中的数字顺序**串行执行**
- 最后启动 getty 提供登录终端

### Runlevel 目录结构

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Runlevel Directory Structure                                    │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    /etc/
    ├── init.d/                          # Actual service scripts
    │   ├── network                      # Network service script
    │   ├── ssh                          # SSH service script
    │   ├── apache2                      # Apache service script
    │   └── ...
    │
    ├── rc0.d/                           # Runlevel 0 (Halt)
    │   ├── K01apache2 -> ../init.d/apache2
    │   ├── K05ssh -> ../init.d/ssh
    │   └── K90network -> ../init.d/network
    │
    ├── rc1.d/                           # Runlevel 1 (Single-user)
    │   └── ...
    │
    ├── rc2.d/                           # Runlevel 2 (Multi-user, no network)
    │   └── ...
    │
    ├── rc3.d/                           # Runlevel 3 (Multi-user, with network)
    │   ├── S10network -> ../init.d/network
    │   ├── S20ssh -> ../init.d/ssh
    │   └── S50apache2 -> ../init.d/apache2
    │
    ├── rc4.d/                           # Runlevel 4 (Unused)
    │   └── ...
    │
    ├── rc5.d/                           # Runlevel 5 (GUI)
    │   └── ...
    │
    └── rc6.d/                           # Runlevel 6 (Reboot)
        ├── K01apache2 -> ../init.d/apache2
        ├── K05ssh -> ../init.d/ssh
        └── K90network -> ../init.d/network
```

**说明**：
- `/etc/init.d/` 存放实际的服务脚本
- `/etc/rcN.d/` 存放指向 `init.d` 脚本的符号链接
- 链接名决定执行顺序：`S10` 在 `S20` 之前执行
- `S` 开头的链接在进入该运行级别时执行 `start`
- `K` 开头的链接在离开该运行级别时执行 `stop`

---

## 4. Upstart 详解

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Upstart Boot Process                                            │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    Kernel starts /sbin/init (Upstart)
           │
           ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                         Emit "startup" Event                                        │
    │                                                                                     │
    │   Upstart emits the initial "startup" event                                         │
    │   Jobs waiting for "startup" event will start                                       │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
           │
           ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                         Event-Driven Job Execution                                  │
    │                                                                                     │
    │   /etc/init/mountall.conf:                                                          │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │  start on startup                   ◄── Trigger: startup event              │   │
    │   │  task                                                                       │   │
    │   │  exec /sbin/mountall                                                        │   │
    │   │  emit filesystem                    ◄── Emits new event when done           │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │                                                                                     │
    │                           │                                                         │
    │                           ▼ (emits "filesystem" event)                              │
    │                                                                                     │
    │   /etc/init/networking.conf:                                                        │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │  start on filesystem                ◄── Waits for filesystem event          │   │
    │   │  stop on runlevel [016]                                                     │   │
    │   │  exec /etc/init.d/networking start                                          │   │
    │   │  emit net-device-up                 ◄── Emits new event when done           │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │                                                                                     │
    │                           │                                                         │
    │                           ▼ (emits "net-device-up" event)                           │
    │                                                                                     │
    │   /etc/init/ssh.conf:                                                               │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │  start on net-device-up             ◄── Waits for network event             │   │
    │   │  stop on runlevel [!2345]                                                   │   │
    │   │  respawn                            ◄── Auto-restart if crashes             │   │
    │   │  exec /usr/sbin/sshd -D                                                     │   │
    │   └─────────────────────────────────────────────────────────────────────────────┘   │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
           │
           ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                         Event Chain                                                 │
    │                                                                                     │
    │   startup ──► mountall ──► filesystem ──► networking ──► net-device-up ──► ssh     │
    │                                                                                     │
    │   Multiple jobs can start in parallel if they wait for the same event              │
    │                                                                                     │
    │                    filesystem                                                       │
    │                        │                                                            │
    │            ┌───────────┼───────────┐                                                │
    │            ▼           ▼           ▼                                                │
    │       networking    udev-finish  dbus     (Parallel start)                          │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- Upstart 使用**事件驱动**模型
- 服务通过 `start on <event>` 声明何时启动
- 服务完成后可以 `emit <event>` 触发其他服务
- 等待同一事件的多个服务可以**并行启动**
- 比 SysVinit 更灵活，但不如 systemd 强大

### Upstart Job 配置文件格式

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Upstart Job Configuration                                       │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    /etc/init/example.conf
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   # Description                                                                     │
    │   description "Example service"                                                     │
    │   author "Admin <admin@example.com>"                                                │
    │                                                                                     │
    │   # When to start/stop                                                              │
    │   start on (filesystem and net-device-up IFACE!=lo)                                 │
    │   stop on runlevel [016]                                                            │
    │                                                                                     │
    │   # Process management                                                              │
    │   respawn                          # Restart if crashes                             │
    │   respawn limit 10 5               # Max 10 restarts in 5 seconds                   │
    │                                                                                     │
    │   # Environment                                                                     │
    │   env LANG=en_US.UTF-8                                                              │
    │   env HOME=/root                                                                    │
    │                                                                                     │
    │   # Pre-start script                                                                │
    │   pre-start script                                                                  │
    │       mkdir -p /var/run/example                                                     │
    │   end script                                                                        │
    │                                                                                     │
    │   # Main process                                                                    │
    │   exec /usr/bin/example-daemon --config /etc/example.conf                           │
    │                                                                                     │
    │   # Post-stop script                                                                │
    │   post-stop script                                                                  │
    │       rm -f /var/run/example/example.pid                                            │
    │   end script                                                                        │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 配置文件位于 `/etc/init/` 目录，以 `.conf` 结尾
- `start on` 和 `stop on` 定义启动和停止条件
- 支持复杂的事件表达式（and, or）
- `respawn` 使服务崩溃后自动重启

---

## 5. systemd 详解

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         systemd Boot Process                                            │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    Kernel starts /sbin/init -> /lib/systemd/systemd
           │
           ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                         Load default.target                                         │
    │                                                                                     │
    │   /etc/systemd/system/default.target -> /lib/systemd/system/graphical.target        │
    │                                                                                     │
    │   or multi-user.target for servers                                                  │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
           │
           ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                         Build Dependency Tree                                       │
    │                                                                                     │
    │                              default.target                                         │
    │                            (graphical.target)                                       │
    │                                    │                                                │
    │                                    │ Wants                                          │
    │                                    ▼                                                │
    │                            multi-user.target                                        │
    │                                    │                                                │
    │               ┌────────────────────┼────────────────────┐                           │
    │               │ Wants              │ Wants              │ Wants                     │
    │               ▼                    ▼                    ▼                           │
    │         ssh.service          cron.service        apache2.service                    │
    │               │                    │                    │                           │
    │               │ After              │ After              │ After                     │
    │               ▼                    ▼                    ▼                           │
    │                            basic.target                                             │
    │                                    │                                                │
    │               ┌────────────────────┼────────────────────┐                           │
    │               │ Requires           │ Requires           │ Requires                  │
    │               ▼                    ▼                    ▼                           │
    │        sysinit.target      sockets.target       timers.target                       │
    │               │                                                                     │
    │               │ Requires                                                            │
    │               ▼                                                                     │
    │         local-fs.target                                                             │
    │               │                                                                     │
    │               │ After                                                               │
    │               ▼                                                                     │
    │       local-fs-pre.target                                                           │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
           │
           ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                         Parallel Execution                                          │
    │                                                                                     │
    │   Time ──────────────────────────────────────────────────────────────────────────►  │
    │                                                                                     │
    │   ┌──────────────┐                                                                  │
    │   │local-fs.target│                                                                 │
    │   └──────┬───────┘                                                                  │
    │          │                                                                          │
    │          ▼                                                                          │
    │   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                              │
    │   │sysinit.target│  │sockets.target│  │timers.target │  (Parallel)                  │
    │   └──────┬───────┘  └──────┬───────┘  └──────┬───────┘                              │
    │          │                 │                 │                                      │
    │          └─────────────────┼─────────────────┘                                      │
    │                            ▼                                                        │
    │                     ┌──────────────┐                                                │
    │                     │ basic.target │                                                │
    │                     └──────┬───────┘                                                │
    │                            │                                                        │
    │          ┌─────────────────┼─────────────────┐                                      │
    │          ▼                 ▼                 ▼                                      │
    │   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                              │
    │   │ ssh.service  │  │ cron.service │  │apache2.service│  (Parallel)                 │
    │   └──────────────┘  └──────────────┘  └──────────────┘                              │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- systemd 读取 `default.target` 确定启动目标
- 构建完整的**依赖关系图**
- 所有无依赖冲突的服务**并行启动**
- 大大缩短启动时间

### systemd Unit 文件格式

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         systemd Unit File Structure                                     │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    /lib/systemd/system/apache2.service
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   [Unit]                                                                            │
    │   Description=The Apache HTTP Server                                                │
    │   Documentation=man:apache2(8)                                                      │
    │   After=network.target remote-fs.target nss-lookup.target                           │
    │   Wants=network-online.target                                                       │
    │                                                                                     │
    │   [Service]                                                                         │
    │   Type=forking                                                                      │
    │   Environment=APACHE_STARTED_BY_SYSTEMD=true                                        │
    │   ExecStart=/usr/sbin/apachectl start                                               │
    │   ExecStop=/usr/sbin/apachectl graceful-stop                                        │
    │   ExecReload=/usr/sbin/apachectl graceful                                           │
    │   PrivateTmp=true                                                                   │
    │   Restart=on-failure                                                                │
    │   RestartSec=5                                                                      │
    │                                                                                     │
    │   [Install]                                                                         │
    │   WantedBy=multi-user.target                                                        │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    Dependency Directives:
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   After=X        Start this unit after X has started (ordering only)                │
    │   Before=X       Start this unit before X starts (ordering only)                    │
    │   Requires=X     This unit needs X; if X fails, this fails too                      │
    │   Wants=X        This unit wants X, but can run without it                          │
    │   Conflicts=X    Cannot run at the same time as X                                   │
    │   BindsTo=X      Like Requires, but also stops when X stops                         │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- **[Unit]**：描述信息和依赖关系
- **[Service]**：服务类型、启动/停止命令、重启策略
- **[Install]**：定义如何启用服务（属于哪个 target）

---

## 6. /etc/inittab 文件详解

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         /etc/inittab File Structure                                     │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   Line Format:  id:runlevels:action:process                                         │
    │                                                                                     │
    │   ┌────────┬───────────────────────────────────────────────────────────────────┐    │
    │   │ Field  │ Description                                                       │    │
    │   ├────────┼───────────────────────────────────────────────────────────────────┤    │
    │   │ id     │ Unique identifier (1-4 characters)                                │    │
    │   │runlevels│ Runlevels where this entry applies (0-6, S)                      │    │
    │   │ action │ How init should handle this entry                                 │    │
    │   │ process│ Command/script to execute                                         │    │
    │   └────────┴───────────────────────────────────────────────────────────────────┘    │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

### 完整的 /etc/inittab 示例

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Complete /etc/inittab Example                                   │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    # /etc/inittab - init daemon configuration
    #
    # Default runlevel
    # 0 = halt, 1 = single-user, 2-5 = multi-user, 6 = reboot
    
    id:3:initdefault:
    │  │      │
    │  │      └── Action: set default runlevel (no process needed)
    │  └── Runlevel: 3 (multi-user with networking)
    └── ID: "id" (identifier for this entry)
    
    # System initialization
    si::sysinit:/etc/init.d/rcS
    │    │         │
    │    │         └── Process: run /etc/init.d/rcS script
    │    └── Action: sysinit (run once at boot before anything else)
    └── ID: "si"
    
    # Runlevel scripts
    l0:0:wait:/etc/init.d/rc 0
    l1:1:wait:/etc/init.d/rc 1
    l2:2:wait:/etc/init.d/rc 2
    l3:3:wait:/etc/init.d/rc 3
    l4:4:wait:/etc/init.d/rc 4
    l5:5:wait:/etc/init.d/rc 5
    l6:6:wait:/etc/init.d/rc 6
    │  │  │         │
    │  │  │         └── Process: run /etc/init.d/rc with runlevel argument
    │  │  └── Action: wait (init waits for process to complete)
    │  └── Runlevel: applies to this specific runlevel
    └── ID: "l0" through "l6"
    
    # Ctrl+Alt+Del handling
    ca:12345:ctrlaltdel:/sbin/shutdown -t1 -a -r now
    │    │        │              │
    │    │        │              └── Process: reboot command
    │    │        └── Action: ctrlaltdel (run when Ctrl+Alt+Del pressed)
    │    └── Runlevels: 1,2,3,4,5
    └── ID: "ca"
    
    # Power failure handling (requires UPS and powerd)
    pf::powerfail:/sbin/shutdown -f -h +2 "Power Failure; System Shutting Down"
    po::powerokwait:/sbin/shutdown -c "Power Restored; Shutdown Cancelled"
    
    # Getty processes (login terminals)
    1:2345:respawn:/sbin/getty 38400 tty1
    2:23:respawn:/sbin/getty 38400 tty2
    3:23:respawn:/sbin/getty 38400 tty3
    4:23:respawn:/sbin/getty 38400 tty4
    5:23:respawn:/sbin/getty 38400 tty5
    6:23:respawn:/sbin/getty 38400 tty6
    │  │     │           │
    │  │     │           └── Process: getty command for terminal
    │  │     └── Action: respawn (restart if process dies)
    │  └── Runlevels: 2,3 (or 2,3,4,5 for tty1)
    └── ID: "1" through "6"
    
    # Serial console (optional)
    T0:23:respawn:/sbin/getty -L ttyS0 9600 vt100
```

### Action 类型详解

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         inittab Action Types                                            │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    ┌──────────────────┬──────────────────────────────────────────────────────────────────┐
    │ Action           │ Description                                                      │
    ├──────────────────┼──────────────────────────────────────────────────────────────────┤
    │ initdefault      │ Sets the default runlevel (no process field needed)              │
    │                  │ Example: id:3:initdefault:                                       │
    ├──────────────────┼──────────────────────────────────────────────────────────────────┤
    │ sysinit          │ Run during system boot, before any other entries                 │
    │                  │ Init waits for completion. Runlevel field is ignored.            │
    │                  │ Example: si::sysinit:/etc/init.d/rcS                             │
    ├──────────────────┼──────────────────────────────────────────────────────────────────┤
    │ wait             │ Run when entering the specified runlevel                         │
    │                  │ Init waits for process to complete before continuing             │
    │                  │ Example: l3:3:wait:/etc/init.d/rc 3                              │
    ├──────────────────┼──────────────────────────────────────────────────────────────────┤
    │ once             │ Run once when entering the specified runlevel                    │
    │                  │ Init does NOT wait for completion                                │
    │                  │ Example: ud:once:/sbin/update                                    │
    ├──────────────────┼──────────────────────────────────────────────────────────────────┤
    │ respawn          │ Start process; restart it whenever it terminates                 │
    │                  │ Used for getty and long-running daemons                          │
    │                  │ Example: 1:2345:respawn:/sbin/getty 38400 tty1                   │
    ├──────────────────┼──────────────────────────────────────────────────────────────────┤
    │ ctrlaltdel       │ Run when init receives SIGINT (Ctrl+Alt+Del)                     │
    │                  │ Example: ca:12345:ctrlaltdel:/sbin/shutdown -r now               │
    ├──────────────────┼──────────────────────────────────────────────────────────────────┤
    │ powerfail        │ Run when init receives SIGPWR (power failure)                    │
    │                  │ Example: pf::powerfail:/sbin/shutdown -h +5                      │
    ├──────────────────┼──────────────────────────────────────────────────────────────────┤
    │ powerokwait      │ Run when power is restored (SIGPWR with /etc/powerstatus = OK)   │
    │                  │ Example: po::powerokwait:/sbin/shutdown -c                       │
    ├──────────────────┼──────────────────────────────────────────────────────────────────┤
    │ boot             │ Run during boot (after sysinit), runlevel field ignored          │
    │                  │ Init does NOT wait for completion                                │
    ├──────────────────┼──────────────────────────────────────────────────────────────────┤
    │ bootwait         │ Run during boot (after sysinit), runlevel field ignored          │
    │                  │ Init WAITS for completion                                        │
    ├──────────────────┼──────────────────────────────────────────────────────────────────┤
    │ off              │ Do nothing (entry is disabled)                                   │
    │                  │ Useful for temporarily disabling an entry                        │
    └──────────────────┴──────────────────────────────────────────────────────────────────┘
```

**说明**：
- **initdefault**：设置默认运行级别，系统启动时自动进入
- **sysinit**：系统初始化，最先执行，用于基本系统设置
- **wait**：等待执行完成，用于运行级别切换脚本
- **respawn**：进程退出后自动重启，用于 getty 等关键服务

---

## 7. Init Script 执行顺序机制

### SysVinit 执行顺序

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         SysVinit Script Execution Order                                 │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    Step 1: /etc/init.d/rc reads /etc/rcN.d/ directory
    
    Step 2: Sort all symlinks alphabetically
    
    /etc/rc3.d/
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   K01apache2      ◄── K scripts first (stop services from previous runlevel)        │
    │   K05mysql                                                                          │
    │   K10postfix                                                                        │
    │   S10network      ◄── Then S scripts (start services for this runlevel)             │
    │   S12syslog                                                                         │
    │   S15dbus                                                                           │
    │   S20ssh                                                                            │
    │   S25bluetooth                                                                      │
    │   S30cron                                                                           │
    │   S50apache2                                                                        │
    │   S55mysql                                                                          │
    │   S90postfix                                                                        │
    │   S99local                                                                          │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
    
    Step 3: Execute in order
    
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   for script in K*; do                                                              │
    │       $script stop                                                                  │
    │   done                                                                              │
    │                                                                                     │
    │   for script in S*; do                                                              │
    │       $script start                                                                 │
    │   done                                                                              │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
    
    Execution Timeline (Sequential):
    
    Time ──────────────────────────────────────────────────────────────────────────────►
    
    ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐
    │K01     │ │K05     │ │S10     │ │S12     │ │S20     │ │S50     │ │S99     │
    │apache2 │ │mysql   │ │network │ │syslog  │ │ssh     │ │apache2 │ │local   │
    │ stop   │ │ stop   │ │ start  │ │ start  │ │ start  │ │ start  │ │ start  │
    └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ └────────┘
         │          │          │          │          │          │          │
         ▼          ▼          ▼          ▼          ▼          ▼          ▼
        Done       Done       Done       Done       Done       Done       Done
```

**说明**：
- 脚本按**文件名字母顺序**排序执行
- `K` 脚本（Kill）先执行，用于停止上一个运行级别的服务
- `S` 脚本（Start）后执行，用于启动当前运行级别的服务
- 数字越小越先执行：`S10` 在 `S20` 之前
- 所有脚本**串行执行**，一个完成后才执行下一个

### 如何设置脚本优先级

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Setting Script Priority                                         │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    Method 1: Manual symlink creation
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   # Create startup link with priority 50                                            │
    │   ln -s /etc/init.d/myservice /etc/rc3.d/S50myservice                               │
    │                                                                                     │
    │   # Create stop link with priority 10                                               │
    │   ln -s /etc/init.d/myservice /etc/rc0.d/K10myservice                               │
    │   ln -s /etc/init.d/myservice /etc/rc6.d/K10myservice                               │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
    
    Method 2: update-rc.d (Debian/Ubuntu)
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   # Add with default priorities                                                     │
    │   update-rc.d myservice defaults                                                    │
    │                                                                                     │
    │   # Add with specific start/stop priorities                                         │
    │   update-rc.d myservice defaults 50 10                                              │
    │   #                              │   │                                              │
    │   #                              │   └── Stop priority (K10)                        │
    │   #                              └── Start priority (S50)                           │
    │                                                                                     │
    │   # Add to specific runlevels                                                       │
    │   update-rc.d myservice start 50 2 3 4 5 . stop 10 0 1 6 .                          │
    │   #                       │   │  │ │ │ │   │    │  │ │ │                            │
    │   #                       │   │  └─┴─┴─┘   │    │  └─┴─┘                            │
    │   #                       │   │   runlevels│    │   runlevels                       │
    │   #                       │   priority     │    priority                            │
    │   #                       action           action                                   │
    │                                                                                     │
    │   # Remove service                                                                  │
    │   update-rc.d myservice remove                                                      │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
    
    Method 3: chkconfig (RHEL/CentOS)
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   # Script header (required in /etc/init.d/myservice):                              │
    │   #!/bin/bash                                                                       │
    │   # chkconfig: 2345 50 10                                                           │
    │   #            │    │  │                                                            │
    │   #            │    │  └── Stop priority                                            │
    │   #            │    └── Start priority                                              │
    │   #            └── Runlevels to enable                                              │
    │   # description: My custom service                                                  │
    │                                                                                     │
    │   # Add service                                                                     │
    │   chkconfig --add myservice                                                         │
    │                                                                                     │
    │   # Enable/disable for specific runlevels                                           │
    │   chkconfig --level 35 myservice on                                                 │
    │   chkconfig --level 01246 myservice off                                             │
    │                                                                                     │
    │   # List all services                                                               │
    │   chkconfig --list                                                                  │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- **手动创建符号链接**：直接控制优先级数字
- **update-rc.d**：Debian/Ubuntu 的工具，自动管理符号链接
- **chkconfig**：RHEL/CentOS 的工具，通过脚本头部注释定义优先级

### LSB Init Script 头部

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         LSB Init Script Header                                          │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    /etc/init.d/myservice
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   #!/bin/bash                                                                       │
    │   ### BEGIN INIT INFO                                                               │
    │   # Provides:          myservice                                                    │
    │   # Required-Start:    $network $syslog                                             │
    │   # Required-Stop:     $network $syslog                                             │
    │   # Default-Start:     2 3 4 5                                                      │
    │   # Default-Stop:      0 1 6                                                        │
    │   # Short-Description: My custom service                                            │
    │   # Description:       This is a longer description of my service                   │
    │   #                    that can span multiple lines.                                │
    │   ### END INIT INFO                                                                 │
    │                                                                                     │
    │   # Script body follows...                                                          │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    LSB Facilities (Virtual Dependencies):
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   $local_fs    - All local filesystems mounted                                      │
    │   $network     - Basic networking available                                         │
    │   $named       - DNS name resolution available                                      │
    │   $portmap     - Portmap service (RPC)                                              │
    │   $remote_fs   - All remote filesystems mounted (NFS, etc.)                         │
    │   $syslog      - System logging available                                           │
    │   $time        - System time set                                                    │
    │   $all         - All services started                                               │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- LSB (Linux Standard Base) 头部定义服务的依赖关系
- `Required-Start` 指定必须先启动的服务
- `insserv` 等工具根据这些依赖自动计算启动顺序
- 虚拟依赖（如 `$network`）代表一组相关服务

---

## 附录：常用命令速查

### SysVinit 命令

```bash
# 查看当前运行级别
runlevel

# 切换运行级别
init 3
telinit 5

# 管理服务
service myservice start
service myservice stop
service myservice status
service myservice restart

# 查看服务状态
chkconfig --list              # RHEL/CentOS
update-rc.d -n myservice defaults  # Debian (dry-run)
```

### systemd 命令

```bash
# 查看启动顺序分析
systemd-analyze
systemd-analyze blame
systemd-analyze critical-chain

# 查看依赖关系
systemctl list-dependencies
systemctl list-dependencies --reverse ssh.service

# 管理服务
systemctl start myservice
systemctl stop myservice
systemctl restart myservice
systemctl status myservice

# 启用/禁用开机启动
systemctl enable myservice
systemctl disable myservice

# 查看/设置默认 target
systemctl get-default
systemctl set-default multi-user.target
```

### Upstart 命令

```bash
# 管理服务
start myservice
stop myservice
restart myservice
status myservice

# 查看所有作业
initctl list

# 查看作业状态
initctl status myservice

# 手动触发事件
initctl emit myevent
```

