# Linux Kernel PCI Subsystem Deep Dive (v3.2)

## A Code-Driven Walkthrough for Kernel Developers

---

## 1. Subsystem Context (Big Picture)

### 1.1 What Problem Does PCI Solve?

PCI (Peripheral Component Interconnect) 子系统是 Linux 内核中管理 PCI 总线和设备的核心框架。它解决以下问题：

1. **设备发现与枚举** - 自动探测连接到系统的 PCI 设备
2. **资源分配** - 管理 I/O 端口、内存映射区域、中断等硬件资源
3. **配置空间访问** - 提供统一的接口读写 PCI 设备的配置寄存器
4. **驱动绑定** - 将 PCI 设备与对应的设备驱动匹配并绑定
5. **电源管理** - 处理设备的电源状态转换（D0-D3）
6. **热插拔支持** - 支持设备的动态添加和移除

### 1.2 Architecture Diagram

```
═══════════════════════════════════════════════════════════════════════════════
                        LINUX PCI SUBSYSTEM ARCHITECTURE
═══════════════════════════════════════════════════════════════════════════════

                              USER SPACE
    ┌─────────────────────────────────────────────────────────────────────────┐
    │  lspci        setpci       /sys/bus/pci/      /proc/bus/pci/            │
    └───────────────────────────────────┬─────────────────────────────────────┘
                                        │ sysfs / procfs
════════════════════════════════════════│═════════════════════════════════════
                              KERNEL SPACE
                                        │
    ┌───────────────────────────────────▼─────────────────────────────────────┐
    │                        PCI DRIVER MODEL                                  │
    │  ┌─────────────────────────────────────────────────────────────────┐    │
    │  │  struct pci_driver                                              │    │
    │  │  ┌──────────────┬──────────────┬──────────────┬─────────────┐   │    │
    │  │  │ e1000_driver │ ahci_driver  │ xhci_driver  │  ...        │   │    │
    │  │  └──────┬───────┴──────┬───────┴──────┬───────┴─────────────┘   │    │
    │  │         │              │              │                         │    │
    │  │         ▼              ▼              ▼                         │    │
    │  │  ┌──────────────────────────────────────────────────────────┐   │    │
    │  │  │              pci_bus_type (struct bus_type)              │   │    │
    │  │  │  .match()  .probe()  .remove()  .uevent()               │   │    │
    │  │  └──────────────────────────────────────────────────────────┘   │    │
    │  └─────────────────────────────────────────────────────────────────┘    │
    └───────────────────────────────────┬─────────────────────────────────────┘
                                        │
    ┌───────────────────────────────────▼─────────────────────────────────────┐
    │                          PCI CORE LAYER                                  │
    │  ┌───────────────────┐  ┌───────────────────┐  ┌───────────────────┐    │
    │  │   Bus Management  │  │ Resource Mgmt     │  │  Config Access    │    │
    │  │   pci_bus         │  │ setup-res.c       │  │  access.c         │    │
    │  │   probe.c         │  │ setup-bus.c       │  │  pci_read/write   │    │
    │  └─────────┬─────────┘  └─────────┬─────────┘  └─────────┬─────────┘    │
    │            │                      │                      │              │
    │  ┌─────────▼──────────────────────▼──────────────────────▼─────────┐    │
    │  │                    struct pci_dev / struct pci_bus              │    │
    │  │                    (Core Data Structures)                       │    │
    │  └─────────────────────────────────────────────────────────────────┘    │
    └───────────────────────────────────┬─────────────────────────────────────┘
                                        │
    ┌───────────────────────────────────▼─────────────────────────────────────┐
    │                    ARCHITECTURE-SPECIFIC LAYER                          │
    │  ┌───────────────────┐  ┌───────────────────┐  ┌───────────────────┐    │
    │  │  arch/x86/pci/    │  │  arch/arm/pci/    │  │ arch/powerpc/pci/ │    │
    │  │  - legacy.c       │  │  - common.c       │  │  - common.c       │    │
    │  │  - direct.c       │  │  - pci-driver.c   │  │                   │    │
    │  │  - mmconfig.c     │  │                   │  │                   │    │
    │  └─────────┬─────────┘  └─────────┬─────────┘  └─────────┬─────────┘    │
    │            │                      │                      │              │
    │  ┌─────────▼──────────────────────▼──────────────────────▼─────────┐    │
    │  │                    struct pci_ops                                │    │
    │  │                    .read()  .write()                            │    │
    │  └─────────────────────────────────────────────────────────────────┘    │
    └───────────────────────────────────┬─────────────────────────────────────┘
                                        │
════════════════════════════════════════│═════════════════════════════════════
                              HARDWARE LAYER
                                        │
    ┌───────────────────────────────────▼─────────────────────────────────────┐
    │  ┌─────────────┐     ┌─────────────┐     ┌─────────────┐                │
    │  │ PCI Config  │     │   PCI Bus   │     │ PCI Device  │                │
    │  │   Space     │     │   0x0CF8    │     │ Config Regs │                │
    │  │  256/4096B  │     │   0x0CFC    │     │   BARs      │                │
    │  └─────────────┘     └─────────────┘     └─────────────┘                │
    │                                                                         │
    │  ┌──────────────────────────────────────────────────────────────────┐   │
    │  │              PCI/PCIe Physical Bus Topology                      │   │
    │  │                                                                  │   │
    │  │    ┌──────────┐                                                  │   │
    │  │    │ CPU/Root │                                                  │   │
    │  │    │ Complex  │                                                  │   │
    │  │    └────┬─────┘                                                  │   │
    │  │         │                                                        │   │
    │  │    ┌────▼─────┐         ┌──────────┐         ┌──────────┐        │   │
    │  │    │  Root    │─────────│  Bridge  │─────────│  Device  │        │   │
    │  │    │  Port    │         │ (P2P)    │         │  (NIC)   │        │   │
    │  │    └──────────┘         └────┬─────┘         └──────────┘        │   │
    │  │                              │                                   │   │
    │  │                         ┌────▼─────┐                             │   │
    │  │                         │Secondary │                             │   │
    │  │                         │   Bus    │                             │   │
    │  │                         └────┬─────┘                             │   │
    │  │                         ┌────┴────┐                              │   │
    │  │                    ┌────▼───┐ ┌───▼────┐                         │   │
    │  │                    │ Device │ │ Device │                         │   │
    │  │                    │ (GPU)  │ │ (SATA) │                         │   │
    │  │                    └────────┘ └────────┘                         │   │
    │  └──────────────────────────────────────────────────────────────────┘   │
    └─────────────────────────────────────────────────────────────────────────┘
```

### 1.3 How PCI Interacts with Other Subsystems

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      PCI SUBSYSTEM INTERACTIONS                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   Device Model (kobject/sysfs)         DMA / IOMMU                          │
│   ┌───────────────────────┐           ┌───────────────────────┐             │
│   │ - sysfs entries       │◄─────────►│ - DMA mapping         │             │
│   │ - uevent generation   │    PCI    │ - IOMMU configuration │             │
│   │ - device hierarchy    │◄────┬────►│ - DMA coherence       │             │
│   └───────────────────────┘     │     └───────────────────────┘             │
│                                 │                                           │
│   Interrupt Subsystem           │      Power Management                     │
│   ┌───────────────────────┐     │     ┌───────────────────────┐             │
│   │ - Legacy IRQ          │◄────┼────►│ - Runtime PM          │             │
│   │ - MSI/MSI-X           │     │     │ - System PM (suspend) │             │
│   │ - irq_desc allocation │     │     │ - D-states (D0-D3)    │             │
│   └───────────────────────┘     │     └───────────────────────┘             │
│                                 │                                           │
│   Resource Management           │      ACPI (x86)                           │
│   ┌───────────────────────┐     │     ┌───────────────────────┐             │
│   │ - ioport_resource     │◄────┴────►│ - _CRS (resources)    │             │
│   │ - iomem_resource      │           │ - _PRT (routing)      │             │
│   │ - IRQ allocation      │           │ - Hotplug support     │             │
│   └───────────────────────┘           └───────────────────────┘             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Directory & File Map (Code Navigation)

```
drivers/pci/
├── Kconfig                 → PCI 配置选项
├── Makefile                → 编译规则
│
├── probe.c                 → ★ PCI 设备探测与总线扫描 (最重要)
│                              - pci_scan_device()
│                              - pci_scan_slot()
│                              - pci_scan_child_bus()
│                              - pci_create_bus()
│
├── pci.c                   → ★ PCI 核心功能实现
│                              - pci_enable_device()
│                              - pci_set_master()
│                              - pci_request_regions()
│                              - pci_save_state() / pci_restore_state()
│
├── pci-driver.c            → ★ PCI 驱动模型
│                              - __pci_register_driver()
│                              - pci_bus_match()
│                              - pci_device_probe()
│                              - pci_bus_type 定义
│
├── access.c                → 配置空间访问封装
│                              - pci_bus_read/write_config_*()
│                              - pci_lock (spinlock)
│
├── bus.c                   → 总线管理
│                              - pci_bus_add_device()
│                              - pci_bus_alloc_resource()
│
├── setup-res.c             → 资源分配与映射
│                              - pci_assign_resource()
│
├── setup-bus.c             → 总线资源配置
│                              - pci_bus_size_bridges()
│                              - pci_bus_assign_resources()
│
├── setup-irq.c             → 中断配置
│
├── search.c                → 设备搜索功能
│                              - pci_get_device()
│                              - pci_find_bus()
│
├── remove.c                → 设备移除
│                              - pci_remove_bus_device()
│
├── quirks.c                → 硬件 quirks (兼容性修复)
│                              - 特定设备的 workarounds
│
├── msi.c                   → MSI/MSI-X 中断支持
│
├── slot.c                  → PCI 插槽管理
│
├── rom.c                   → 扩展 ROM 支持
│
├── vpd.c                   → Vital Product Data 支持
│
├── proc.c                  → /proc/bus/pci 接口
│
├── pci-sysfs.c             → sysfs 接口
│
├── hotplug/                → 热插拔支持
│   ├── pci_hotplug_core.c
│   ├── pciehp/             → PCIe 热插拔
│   └── acpiphp/            → ACPI 热插拔
│
├── pcie/                   → PCIe 特定功能
│   ├── aspm.c              → Active State PM
│   ├── aer/                → Advanced Error Reporting
│   └── portdrv_core.c      → PCIe 端口驱动
│
└── pci.h                   → 内部头文件

include/linux/
├── pci.h                   → ★ 主要公共头文件
│                              - struct pci_dev 定义
│                              - struct pci_bus 定义
│                              - struct pci_driver 定义
│
├── pci_ids.h               → PCI 设备 ID 定义
│
└── pci_regs.h              → PCI 配置空间寄存器定义

arch/x86/pci/               → x86 架构特定
├── common.c                → 通用 x86 PCI 支持
├── direct.c                → 直接 I/O 端口访问
├── mmconfig.c              → MMCONFIG (PCIe ECAM)
├── legacy.c                → 传统 BIOS 方法
├── acpi.c                  → ACPI 集成
└── irq.c                   → 中断路由
```

---

## 3. Core Data Structures

### 3.1 struct pci_dev - PCI 设备核心结构

```c
/* include/linux/pci.h */

/*
 * The pci_dev structure is used to describe PCI devices.
 */
struct pci_dev {
    /* ═══════════════════════════════════════════════════════════════════ */
    /* 总线拓扑相关                                                         */
    /* ═══════════════════════════════════════════════════════════════════ */
    struct list_head bus_list;      /* 在所属总线设备链表中的节点 */
    struct pci_bus  *bus;           /* 此设备所在的 PCI 总线 */
    struct pci_bus  *subordinate;   /* 如果是桥，指向其次级总线 */

    void        *sysdata;           /* 系统/架构特定数据钩子 */
    struct proc_dir_entry *procent; /* /proc/bus/pci 中的条目 */
    struct pci_slot *slot;          /* 设备所在的物理插槽 */

    /* ═══════════════════════════════════════════════════════════════════ */
    /* 设备标识                                                             */
    /* ═══════════════════════════════════════════════════════════════════ */
    unsigned int    devfn;          /* 编码的设备号和功能号 (slot << 3 | func) */
    unsigned short  vendor;         /* 厂商 ID (16-bit) */
    unsigned short  device;         /* 设备 ID (16-bit) */
    unsigned short  subsystem_vendor;
    unsigned short  subsystem_device;
    unsigned int    class;          /* 类代码 (3 bytes: base,sub,prog-if) */
    u8              revision;       /* 修订版本号 */
    u8              hdr_type;       /* 头部类型 (0=endpoint, 1=bridge, 2=cardbus) */
    
    /* ═══════════════════════════════════════════════════════════════════ */
    /* PCIe 相关                                                            */
    /* ═══════════════════════════════════════════════════════════════════ */
    u8              pcie_cap;       /* PCIe capability 在配置空间中的偏移 */
    u8              pcie_type:4;    /* PCIe 设备/端口类型 */
    u8              pcie_mpss:3;    /* 支持的最大负载大小 */
    
    /* ═══════════════════════════════════════════════════════════════════ */
    /* 驱动绑定                                                             */
    /* ═══════════════════════════════════════════════════════════════════ */
    struct pci_driver *driver;      /* 已绑定的驱动程序 */
    
    /* ═══════════════════════════════════════════════════════════════════ */
    /* DMA 配置                                                             */
    /* ═══════════════════════════════════════════════════════════════════ */
    u64             dma_mask;       /* DMA 地址掩码 */
    struct device_dma_parameters dma_parms;

    /* ═══════════════════════════════════════════════════════════════════ */
    /* 电源管理                                                             */
    /* ═══════════════════════════════════════════════════════════════════ */
    pci_power_t     current_state;  /* 当前电源状态 (D0-D3) */
    int             pm_cap;         /* PM capability 偏移 */
    unsigned int    pme_support:5;  /* 可生成 PME# 的状态位图 */
    unsigned int    d1_support:1;
    unsigned int    d2_support:1;
    unsigned int    d3_delay;       /* D3->D0 转换延迟 (ms) */

    /* ═══════════════════════════════════════════════════════════════════ */
    /* 设备模型集成                                                         */
    /* ═══════════════════════════════════════════════════════════════════ */
    struct device   dev;            /* 嵌入的通用设备结构 */
    int             cfg_size;       /* 配置空间大小 (256 或 4096) */

    /* ═══════════════════════════════════════════════════════════════════ */
    /* 资源 (BAR - Base Address Registers)                                 */
    /* ═══════════════════════════════════════════════════════════════════ */
    unsigned int    irq;            /* 中断号 */
    struct resource resource[DEVICE_COUNT_RESOURCE];  /* I/O 和内存区域 */

    /* ═══════════════════════════════════════════════════════════════════ */
    /* 状态标志                                                             */
    /* ═══════════════════════════════════════════════════════════════════ */
    unsigned int    is_added:1;     /* 已添加到设备模型 */
    unsigned int    is_busmaster:1; /* 设备是总线主设备 */
    unsigned int    no_msi:1;       /* 禁用 MSI */
    unsigned int    msi_enabled:1;  /* MSI 已启用 */
    unsigned int    msix_enabled:1; /* MSI-X 已启用 */
    unsigned int    is_pcie:1;      /* 是 PCIe 设备 */
    unsigned int    state_saved:1;  /* 状态已保存 */

    atomic_t        enable_cnt;     /* pci_enable_device 计数 */
    u32             saved_config_space[16]; /* 挂起时保存的配置空间 */
};
```

**内存布局图：**

```
struct pci_dev
┌─────────────────────────────────────────────────────────────────────┐
│  bus_list              (16 bytes)  - 链表节点                       │
├─────────────────────────────────────────────────────────────────────┤
│  *bus                  (8 bytes)   - 所属总线指针                   │
│  *subordinate          (8 bytes)   - 次级总线指针 (桥)              │
│  *sysdata              (8 bytes)   - 系统数据                       │
├─────────────────────────────────────────────────────────────────────┤
│  devfn                 (4 bytes)   - 设备/功能号                    │
│  vendor                (2 bytes)   - 厂商 ID                        │
│  device                (2 bytes)   - 设备 ID                        │
├─────────────────────────────────────────────────────────────────────┤
│  *driver               (8 bytes)   - 绑定的驱动                     │
│  dma_mask              (8 bytes)   - DMA 掩码                       │
├─────────────────────────────────────────────────────────────────────┤
│  current_state         (4 bytes)   - 电源状态                       │
│  pm_cap                (4 bytes)   - PM capability 偏移             │
├─────────────────────────────────────────────────────────────────────┤
│  dev (struct device)   (~600 bytes) - 嵌入的设备结构                │
├─────────────────────────────────────────────────────────────────────┤
│  irq                   (4 bytes)   - 中断号                         │
│  resource[17]          (~340 bytes) - 资源数组 (BAR0-5, ROM, etc)   │
├─────────────────────────────────────────────────────────────────────┤
│  flags (bitfields)     (~4 bytes)  - 各种状态标志                   │
│  saved_config_space[16] (64 bytes) - 保存的配置空间                 │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.2 struct pci_bus - PCI 总线结构

```c
/* include/linux/pci.h */

struct pci_bus {
    /* ═══════════════════════════════════════════════════════════════════ */
    /* 总线层次结构                                                         */
    /* ═══════════════════════════════════════════════════════════════════ */
    struct list_head node;          /* 在全局/父总线链表中的节点 */
    struct pci_bus  *parent;        /* 父总线 (上游桥所在的总线) */
    struct list_head children;      /* 子总线链表 (下游总线) */
    struct list_head devices;       /* 此总线上的设备链表 */
    struct pci_dev  *self;          /* 作为父总线看到的桥设备 */
    struct list_head slots;         /* 此总线上的插槽链表 */

    /* ═══════════════════════════════════════════════════════════════════ */
    /* 资源                                                                 */
    /* ═══════════════════════════════════════════════════════════════════ */
    struct resource *resource[PCI_BRIDGE_RESOURCE_NUM];  /* 桥窗口 */
    struct list_head resources;     /* 路由到此总线的地址空间 */

    /* ═══════════════════════════════════════════════════════════════════ */
    /* 配置访问                                                             */
    /* ═══════════════════════════════════════════════════════════════════ */
    struct pci_ops  *ops;           /* 配置空间访问操作 */
    void            *sysdata;       /* 系统特定扩展钩子 */
    struct proc_dir_entry *procdir; /* /proc/bus/pci 目录 */

    /* ═══════════════════════════════════════════════════════════════════ */
    /* 总线编号                                                             */
    /* ═══════════════════════════════════════════════════════════════════ */
    unsigned char   number;         /* 总线号 */
    unsigned char   primary;        /* 主总线号 (桥上游) */
    unsigned char   secondary;      /* 次总线号 (此总线) */
    unsigned char   subordinate;    /* 最大下游总线号 */

    /* ═══════════════════════════════════════════════════════════════════ */
    /* 总线速度                                                             */
    /* ═══════════════════════════════════════════════════════════════════ */
    unsigned char   max_bus_speed;  /* 最大支持速度 */
    unsigned char   cur_bus_speed;  /* 当前速度 */

    /* ═══════════════════════════════════════════════════════════════════ */
    /* 设备模型集成                                                         */
    /* ═══════════════════════════════════════════════════════════════════ */
    char            name[48];       /* 总线名称 */
    struct device   *bridge;        /* 桥设备 */
    struct device   dev;            /* 嵌入的设备结构 */
    
    unsigned int    is_added:1;     /* 已添加到设备模型 */
};
```

**总线层次结构图：**

```
pci_root_buses (全局链表头)
       │
       ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  pci_bus (Bus 0 - Root Bus)                                              │
│  ┌──────────────────────────────────────────────────────────────────┐    │
│  │ number=0, parent=NULL, ops=pci_root_ops                          │    │
│  │                                                                  │    │
│  │ devices ───► pci_dev ──► pci_dev ──► pci_dev (bridge) ──► ...    │    │
│  │              (00:00.0)   (00:01.0)   (00:02.0)                   │    │
│  │                                          │                       │    │
│  │ children ──────────────────────────────────┘                     │    │
│  └──────────────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  pci_bus (Bus 1 - Secondary Bus behind bridge 00:02.0)                   │
│  ┌──────────────────────────────────────────────────────────────────┐    │
│  │ number=1, parent=Bus 0, self=00:02.0                             │    │
│  │ primary=0, secondary=1, subordinate=2                            │    │
│  │                                                                  │    │
│  │ devices ───► pci_dev ──► pci_dev ──► ...                         │    │
│  │              (01:00.0)   (01:00.1)                               │    │
│  │                                                                  │    │
│  │ children ───► (Bus 2 if there's another bridge)                  │    │
│  └──────────────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.3 struct pci_driver - PCI 驱动结构

```c
/* include/linux/pci.h */

struct pci_driver {
    struct list_head node;                  /* 驱动链表节点 */
    const char *name;                       /* 驱动名称 */
    
    /* 设备匹配表 - 必须非空才能调用 probe */
    const struct pci_device_id *id_table;
    
    /* 驱动回调函数 */
    int  (*probe)  (struct pci_dev *dev, const struct pci_device_id *id);
    void (*remove) (struct pci_dev *dev);
    int  (*suspend) (struct pci_dev *dev, pm_message_t state);
    int  (*resume) (struct pci_dev *dev);
    void (*shutdown) (struct pci_dev *dev);
    
    /* 错误处理 */
    struct pci_error_handlers *err_handler;
    
    /* 嵌入的通用驱动结构 */
    struct device_driver    driver;
    struct pci_dynids       dynids;         /* 动态 ID 列表 */
};
```

### 3.4 struct pci_ops - 配置空间访问操作

```c
/* include/linux/pci.h */

struct pci_ops {
    int (*read)(struct pci_bus *bus, unsigned int devfn, 
                int where, int size, u32 *val);
    int (*write)(struct pci_bus *bus, unsigned int devfn, 
                 int where, int size, u32 val);
};
```

**配置空间访问示意图：**

```
                    PCI Configuration Space Access
═══════════════════════════════════════════════════════════════════════════

  pci_read_config_dword(dev, PCI_VENDOR_ID, &val)
            │
            ▼
  pci_bus_read_config_dword(bus, devfn, pos, &val)  [access.c]
            │
            ├── raw_spin_lock_irqsave(&pci_lock)    // 加锁
            │
            ├── bus->ops->read(bus, devfn, pos, 4, &val)
            │         │
            │         ▼
            │    ┌─────────────────────────────────────────┐
            │    │  Architecture-specific read function    │
            │    │                                         │
            │    │  x86: pci_conf1_read() or pci_mmcfg_read()
            │    │                                         │
            │    │  I/O Port method (Type 1):              │
            │    │    outl(0x80000000 | bus<<16 |          │
            │    │         devfn<<8 | pos, 0xCF8)          │
            │    │    val = inl(0xCFC)                     │
            │    │                                         │
            │    │  MMCONFIG method (PCIe ECAM):           │
            │    │    addr = mmcfg_base + (bus<<20) +      │
            │    │           (devfn<<12) + pos             │
            │    │    val = readl(addr)                    │
            │    └─────────────────────────────────────────┘
            │
            └── raw_spin_unlock_irqrestore(&pci_lock)  // 解锁
```

### 3.5 struct pci_device_id - 设备匹配表

```c
/* include/linux/mod_devicetable.h */

struct pci_device_id {
    __u32 vendor, device;           /* 厂商/设备 ID (PCI_ANY_ID = 匹配任意) */
    __u32 subvendor, subdevice;     /* 子系统厂商/设备 ID */
    __u32 class, class_mask;        /* 类代码和掩码 */
    kernel_ulong_t driver_data;     /* 驱动私有数据 */
};

/* 常用宏 */
#define PCI_DEVICE(vend, dev) \
    .vendor = (vend), .device = (dev), \
    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

#define PCI_DEVICE_CLASS(dev_class, dev_class_mask) \
    .vendor = PCI_ANY_ID, .device = PCI_ANY_ID, \
    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, \
    .class = (dev_class), .class_mask = (dev_class_mask)
```

---

## 4. Entry Points & Call Paths

### 4.1 Key Entry Points

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         PCI SUBSYSTEM ENTRY POINTS                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  1. 内核初始化 (Kernel Initialization)                                       │
│     ────────────────────────────────────                                    │
│     postcore_initcall(pci_driver_init)     → 注册 pci_bus_type              │
│     postcore_initcall(pcibus_class_init)   → 注册 pcibus_class              │
│     subsys_initcall(pcibios_init)          → 架构特定初始化                  │
│     arch_initcall(pci_arch_init)           → x86 PCI 初始化                  │
│                                                                              │
│  2. 驱动注册 (Driver Registration)                                          │
│     ────────────────────────────────                                        │
│     pci_register_driver()                  → 注册 PCI 驱动                   │
│     pci_unregister_driver()                → 注销 PCI 驱动                   │
│                                                                              │
│  3. 设备操作 (Device Operations)                                            │
│     ────────────────────────────                                            │
│     pci_enable_device()                    → 启用设备                        │
│     pci_disable_device()                   → 禁用设备                        │
│     pci_set_master()                       → 使能总线主控                    │
│     pci_request_regions()                  → 请求资源区域                    │
│     pci_ioremap_bar()                      → 映射 BAR                        │
│                                                                              │
│  4. 热插拔 (Hotplug)                                                        │
│     ────────────────                                                        │
│     pci_rescan_bus()                       → 重新扫描总线                    │
│     pci_stop_and_remove_bus_device()       → 移除设备                        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Initialization Call Path

```
═══════════════════════════════════════════════════════════════════════════════
                         PCI SUBSYSTEM INITIALIZATION
═══════════════════════════════════════════════════════════════════════════════

start_kernel()
    │
    └─► do_basic_setup()
            │
            └─► do_initcalls()
                    │
                    ├─► [postcore_initcall] pci_driver_init()
                    │        │
                    │        └─► bus_register(&pci_bus_type)
                    │               │
                    │               └─► 注册 "pci" 总线到设备模型
                    │                   pci_bus_type.match = pci_bus_match
                    │                   pci_bus_type.probe = pci_device_probe
                    │
                    ├─► [postcore_initcall] pcibus_class_init()
                    │        │
                    │        └─► class_register(&pcibus_class)
                    │               │
                    │               └─► 注册 "pci_bus" 类
                    │
                    └─► [arch_initcall] pci_arch_init() (x86)
                             │
                             ├─► pci_direct_probe()  或  pci_pcbios_init()
                             │
                             └─► pcibios_init()
                                    │
                                    ├─► pci_legacy_init() 或 pci_acpi_init()
                                    │
                                    └─► pci_scan_bus()
                                            │
                                            └─► pci_scan_bus_parented()
                                                    │
                                                    ├─► pci_create_bus()
                                                    │       │
                                                    │       └─► 创建根总线
                                                    │
                                                    └─► pci_scan_child_bus()
                                                            │
                                                            └─► 递归扫描所有设备
```

### 4.3 Device Enumeration Call Path

```
═══════════════════════════════════════════════════════════════════════════════
                        PCI DEVICE ENUMERATION FLOW
═══════════════════════════════════════════════════════════════════════════════

pci_scan_child_bus(bus)                               [probe.c]
    │
    ├─► for (devfn = 0; devfn < 0x100; devfn += 8)
    │        │
    │        └─► pci_scan_slot(bus, devfn)
    │                │
    │                └─► pci_scan_single_device(bus, devfn)
    │                        │
    │                        ├─► pci_get_slot()  // 检查是否已存在
    │                        │
    │                        ├─► pci_scan_device()  // 探测设备
    │                        │       │
    │                        │       ├─► pci_bus_read_config_dword(VENDOR_ID)
    │                        │       │       │
    │                        │       │       └─► 读取 vendor:device ID
    │                        │       │           0xFFFFFFFF = 空插槽
    │                        │       │
    │                        │       ├─► alloc_pci_dev()  // 分配 pci_dev
    │                        │       │
    │                        │       └─► pci_setup_device()
    │                        │               │
    │                        │               ├─► 读取 header type
    │                        │               ├─► 读取 class code
    │                        │               ├─► pci_read_irq()
    │                        │               └─► pci_read_bases()  // 读取 BARs
    │                        │
    │                        └─► pci_device_add()
    │                                │
    │                                ├─► device_initialize()
    │                                ├─► pci_fixup_device()
    │                                ├─► pci_init_capabilities()
    │                                │       │
    │                                │       ├─► pci_msi_init_pci_dev()
    │                                │       ├─► pci_pm_init()
    │                                │       └─► pci_iov_init()
    │                                │
    │                                └─► list_add_tail(&dev->bus_list)
    │
    └─► for each bridge device:
            │
            └─► pci_scan_bridge(bus, dev, max, pass)
                    │
                    ├─► pci_add_new_bus()  // 创建次级总线
                    │
                    └─► pci_scan_child_bus(child)  // 递归扫描
```

### 4.4 Driver Registration & Binding

```
═══════════════════════════════════════════════════════════════════════════════
                       PCI DRIVER REGISTRATION & BINDING
═══════════════════════════════════════════════════════════════════════════════

/* 驱动注册流程 */
module_init(my_driver_init)
    │
    └─► pci_register_driver(&my_pci_driver)
            │
            └─► __pci_register_driver()                      [pci-driver.c]
                    │
                    ├─► drv->driver.bus = &pci_bus_type
                    │
                    ├─► driver_register(&drv->driver)
                    │       │
                    │       └─► bus_add_driver()
                    │               │
                    │               └─► driver_attach()
                    │                       │
                    │                       └─► 对总线上每个设备调用:
                    │                               │
                    │                               └─► __driver_attach()
                    │                                       │
                    │                                       └─► 见下方绑定流程
                    │
                    └─► pci_create_newid_file()  // sysfs 接口


/* 设备-驱动绑定流程 */
__driver_attach(dev, drv)
    │
    ├─► driver_match_device(drv, dev)
    │       │
    │       └─► pci_bus_match(dev, drv)                      [pci-driver.c]
    │               │
    │               └─► pci_match_device(drv, dev)
    │                       │
    │                       ├─► 检查 dynids (动态 ID)
    │                       │
    │                       └─► pci_match_id(drv->id_table, dev)
    │                               │
    │                               └─► pci_match_one_device()
    │                                       │
    │                                       └─► 比较 vendor, device,
    │                                           subvendor, subdevice, class
    │
    └─► driver_probe_device(drv, dev)  // 如果匹配成功
            │
            └─► really_probe()
                    │
                    └─► dev->bus->probe(dev)  = pci_device_probe()
                            │
                            └─► __pci_device_probe()
                                    │
                                    ├─► pci_match_device()  // 再次匹配
                                    │
                                    ├─► pci_call_probe(drv, dev, id)
                                    │       │
                                    │       ├─► pm_runtime_get_noresume()
                                    │       │
                                    │       └─► drv->probe(dev, id)  // 驱动 probe!
                                    │               │
                                    │               └─► my_probe() 被调用
                                    │
                                    └─► pci_dev->driver = drv  // 绑定
```

---

## 5. Core Workflows (Code-Driven)

### 5.1 BAR (Base Address Register) Reading

```c
/* drivers/pci/probe.c */

/**
 * __pci_read_base - read a PCI BAR
 *
 * 读取 BAR 的过程:
 * 1. 保存原始值
 * 2. 写全 1
 * 3. 读回来确定大小
 * 4. 恢复原始值
 */
int __pci_read_base(struct pci_dev *dev, enum pci_bar_type type,
                    struct resource *res, unsigned int pos)
{
    u32 l, sz, mask;
    u16 orig_cmd;
    
    /* 禁用 I/O 和内存访问 */
    if (!dev->mmio_always_on) {
        pci_read_config_word(dev, PCI_COMMAND, &orig_cmd);
        pci_write_config_word(dev, PCI_COMMAND,
            orig_cmd & ~(PCI_COMMAND_MEMORY | PCI_COMMAND_IO));
    }
    
    /* 读取当前值 */
    pci_read_config_dword(dev, pos, &l);
    
    /* 写入全 1 来确定大小 */
    pci_write_config_dword(dev, pos, l | mask);
    pci_read_config_dword(dev, pos, &sz);
    
    /* 恢复原始值 */
    pci_write_config_dword(dev, pos, l);
    
    /* 计算大小和类型 */
    /* ... 详细计算逻辑 ... */
}
```

**BAR 大小探测原理：**

```
BAR Size Detection Algorithm
═══════════════════════════════════════════════════════════════════════════

假设 BAR 管理 4KB 内存区域 (0x1000 bytes):

Step 1: 读取原始值
        BAR = 0xFEB00000  (基地址)
              ├── 高位: 实际地址
              └── 低位: 类型标志

Step 2: 写入全 1
        写入 0xFFFFFFFF

Step 3: 读回值
        BAR = 0xFFFFF000
              │     └──── 低 12 位为 0, 表示 4KB 对齐
              └── 高位反映硬编码的大小掩码

Step 4: 计算大小
        size = ~(0xFFFFF000 & mask) + 1
             = ~0xFFFFF000 + 1
             = 0x00001000  = 4096 bytes

Step 5: 恢复原始值
        BAR = 0xFEB00000

结果:
    res->start = 0xFEB00000
    res->end   = 0xFEB00FFF
    res->flags = IORESOURCE_MEM
```

### 5.2 Device Enable Workflow

```c
/* drivers/pci/pci.c */

/**
 * pci_enable_device - Initialize device before it's used by a driver.
 */
int pci_enable_device(struct pci_dev *dev)
{
    return __pci_enable_device_flags(dev, IORESOURCE_MEM | IORESOURCE_IO);
}

static int __pci_enable_device_flags(struct pci_dev *dev, resource_size_t flags)
{
    int err;
    int i, bars = 0;

    /* 仅在首次启用时进行设置 */
    if (atomic_add_return(1, &dev->enable_cnt) > 1)
        return 0;

    /* 确定需要启用哪些 BAR */
    for (i = 0; i < DEVICE_COUNT_RESOURCE; i++)
        if (dev->resource[i].flags & flags)
            bars |= (1 << i);

    /* 架构特定的启用 */
    err = do_pci_enable_device(dev, bars);
    if (err < 0)
        atomic_dec(&dev->enable_cnt);
    return err;
}

static int do_pci_enable_device(struct pci_dev *dev, int bars)
{
    int err;
    
    /* 架构特定: 可能设置电源状态等 */
    err = pci_set_power_state(dev, PCI_D0);
    if (err < 0 && err != -EIO)
        return err;
    
    /* 实际启用 */
    err = pcibios_enable_device(dev, bars);
    if (err < 0)
        return err;
    
    /* 修复某些设备的 quirks */
    pci_fixup_device(pci_fixup_enable, dev);
    
    return 0;
}
```

**启用设备流程图：**

```
pci_enable_device(dev)
         │
         ▼
┌────────────────────────────────────────────────────────────────────────────┐
│  if (atomic_add_return(1, &enable_cnt) > 1)                                │
│      return 0;  // 已经启用，直接返回                                       │
└───────────────────────────────────┬────────────────────────────────────────┘
                                    │ 首次启用
                                    ▼
┌────────────────────────────────────────────────────────────────────────────┐
│  do_pci_enable_device(dev, bars)                                           │
│      │                                                                     │
│      ├─► pci_set_power_state(dev, PCI_D0)                                  │
│      │       │                                                             │
│      │       └─► 将设备从 D3 唤醒到 D0 工作状态                            │
│      │                                                                     │
│      ├─► pcibios_enable_device(dev, bars)                                  │
│      │       │                                                             │
│      │       └─► pci_enable_resources(dev, bars)                           │
│      │               │                                                     │
│      │               ├─► 检查所有 BAR 是否已分配资源                       │
│      │               │                                                     │
│      │               └─► 设置 PCI_COMMAND 寄存器                           │
│      │                       │                                             │
│      │                       ├─► PCI_COMMAND_IO (启用 I/O 访问)            │
│      │                       └─► PCI_COMMAND_MEMORY (启用内存访问)         │
│      │                                                                     │
│      └─► pci_fixup_device(pci_fixup_enable, dev)                           │
│              │                                                             │
│              └─► 应用设备特定的修复                                        │
└────────────────────────────────────────────────────────────────────────────┘
```

### 5.3 Bus Master Enable

```c
/* drivers/pci/pci.c */

/**
 * pci_set_master - enables bus-mastering for device dev
 */
void pci_set_master(struct pci_dev *dev)
{
    u16 cmd;
    
    pci_read_config_word(dev, PCI_COMMAND, &cmd);
    if (!(cmd & PCI_COMMAND_MASTER)) {
        dev_dbg(&dev->dev, "enabling bus mastering\n");
        cmd |= PCI_COMMAND_MASTER;
        pci_write_config_word(dev, PCI_COMMAND, cmd);
    }
    dev->is_busmaster = 1;
    
    /* 设置延迟计时器 */
    pcibios_set_master(dev);
}
```

---

## 6. Important Algorithms & Mechanisms

### 6.1 Bus Enumeration Algorithm

```
═══════════════════════════════════════════════════════════════════════════════
                        PCI BUS ENUMERATION ALGORITHM
═══════════════════════════════════════════════════════════════════════════════

总线枚举采用深度优先搜索 (DFS) 策略：

Input: root_bus (通常是 bus 0)
Output: 所有发现的设备和总线

Algorithm pci_scan_child_bus(bus):
    max_bus_number = bus.secondary
    
    /* 第一步: 扫描当前总线上的所有设备 */
    for devfn = 0 to 255 step 8:   // 每个插槽 (slot)
        dev = pci_scan_slot(bus, devfn)
        if dev:
            if dev.multifunction:
                for func = 1 to 7:
                    pci_scan_single_device(bus, devfn + func)
    
    /* 第二步: 对发现的桥进行两遍扫描 */
    for pass = 0 to 1:
        for each dev in bus.devices:
            if dev.hdr_type == PCI_HEADER_TYPE_BRIDGE:
                max_bus_number = pci_scan_bridge(bus, dev, max, pass)
    
    return max_bus_number


Algorithm pci_scan_bridge(parent_bus, bridge_dev, max, pass):
    if pass == 0:
        /* 第一遍: 临时配置 */
        if !already_configured:
            disable_forwarding()
        return max
    
    if pass == 1:
        /* 第二遍: 实际扫描 */
        child_bus = create_child_bus(parent_bus, bridge_dev, max + 1)
        
        /* 配置桥的总线号寄存器 */
        primary = parent_bus.number
        secondary = child_bus.number
        subordinate = 0xFF  // 暂时设为最大值
        
        write_bridge_bus_numbers(primary, secondary, subordinate)
        
        /* 递归扫描子总线 */
        max = pci_scan_child_bus(child_bus)
        
        /* 更新 subordinate 为实际值 */
        child_bus.subordinate = max
        write_subordinate(max)
        
        return max


示例拓扑:

         Bus 0 (Root)
         ┌───────────────────────────────────────┐
         │  00:00.0 Host Bridge                  │
         │  00:01.0 VGA Controller               │
         │  00:1c.0 PCIe Root Port  ───────────┐ │
         │  00:1d.0 USB Controller              │ │
         │  00:1f.0 ISA Bridge                  │ │
         └──────────────────────────────────────┼─┘
                                                │
                         Bus 1 (Secondary)      ▼
                         ┌─────────────────────────────┐
                         │  01:00.0 Network Controller │
                         │  01:00.1 Network Controller │
                         └─────────────────────────────┘
```

### 6.2 Device-Driver Matching Algorithm

```c
/* drivers/pci/pci.h */

/**
 * pci_match_one_device - Tell if a PCI device matches a pci_device_id
 */
static inline const struct pci_device_id *
pci_match_one_device(const struct pci_device_id *id, const struct pci_dev *dev)
{
    /*
     * 匹配算法 (所有条件必须满足):
     * 
     * 1. vendor ID 匹配 (PCI_ANY_ID 表示任意)
     * 2. device ID 匹配
     * 3. subvendor ID 匹配
     * 4. subdevice ID 匹配
     * 5. class code 匹配 (使用 class_mask)
     */
    if ((id->vendor == PCI_ANY_ID || id->vendor == dev->vendor) &&
        (id->device == PCI_ANY_ID || id->device == dev->device) &&
        (id->subvendor == PCI_ANY_ID || id->subvendor == dev->subsystem_vendor) &&
        (id->subdevice == PCI_ANY_ID || id->subdevice == dev->subsystem_device) &&
        !((id->class ^ dev->class) & id->class_mask))
        return id;
    return NULL;
}
```

**匹配流程图：**

```
                    Device-Driver Matching
═══════════════════════════════════════════════════════════════════════════

           Device                              Driver
    ┌────────────────────┐              ┌────────────────────┐
    │ vendor = 0x8086    │              │ id_table:          │
    │ device = 0x10d3    │              │ ┌────────────────┐ │
    │ subvendor = 0x1028 │              │ │ v=0x8086       │ │
    │ subdevice = 0x0276 │              │ │ d=0x10d3       │ │
    │ class = 0x020000   │              │ │ sv=PCI_ANY_ID  │ │
    │                    │              │ │ sd=PCI_ANY_ID  │ │
    └─────────┬──────────┘              │ │ class=0x020000 │ │
              │                         │ │ mask=0xFFFF00  │ │
              │                         │ └────────────────┘ │
              │                         │ ┌────────────────┐ │
              │                         │ │ (更多条目...)   │ │
              │                         │ └────────────────┘ │
              │                         └─────────┬──────────┘
              │                                   │
              ▼                                   ▼
    ┌─────────────────────────────────────────────────────────┐
    │                  pci_match_one_device()                 │
    │                                                         │
    │  Check 1: vendor == 0x8086?     ✓                      │
    │  Check 2: device == 0x10d3?     ✓                      │
    │  Check 3: subvendor == ANY?     ✓ (ANY matches all)    │
    │  Check 4: subdevice == ANY?     ✓ (ANY matches all)    │
    │  Check 5: (class ^ dev_class) & mask == 0?             │
    │           (0x020000 ^ 0x020000) & 0xFFFF00 == 0  ✓     │
    │                                                         │
    │  Result: MATCH! → Return &id                           │
    └─────────────────────────────────────────────────────────┘
```

---

## 7. Concurrency & Synchronization

### 7.1 Key Locks

```c
/* drivers/pci/access.c */
static DEFINE_RAW_SPINLOCK(pci_lock);  /* 保护配置空间访问 */

/* drivers/pci/pci.h */
extern struct rw_semaphore pci_bus_sem;  /* 保护总线和设备链表 */

/* 使用模式:
 * 
 * pci_lock (raw_spinlock):
 *   - 保护所有配置空间读写
 *   - 在中断上下文中也安全
 *   - 非常短的临界区
 *
 * pci_bus_sem (rw_semaphore):
 *   - 保护 pci_root_buses 链表
 *   - 保护每个 bus 的 devices 链表
 *   - 读者可以并行
 *   - 写者独占
 */
```

### 7.2 Configuration Space Access Locking

```c
/* drivers/pci/access.c */

#define PCI_OP_READ(size,type,len) \
int pci_bus_read_config_##size \
    (struct pci_bus *bus, unsigned int devfn, int pos, type *value) \
{                                                                   \
    int res;                                                        \
    unsigned long flags;                                            \
    u32 data = 0;                                                   \
    if (PCI_##size##_BAD) return PCIBIOS_BAD_REGISTER_NUMBER;       \
    raw_spin_lock_irqsave(&pci_lock, flags);    /* 加锁 */         \
    res = bus->ops->read(bus, devfn, pos, len, &data);             \
    *value = (type)data;                                            \
    raw_spin_unlock_irqrestore(&pci_lock, flags); /* 解锁 */       \
    return res;                                                     \
}
```

### 7.3 Bus/Device List Protection

```c
/* 添加设备到总线 */
void pci_device_add(struct pci_dev *dev, struct pci_bus *bus)
{
    /* ... 初始化 ... */
    
    down_write(&pci_bus_sem);           /* 写锁 */
    list_add_tail(&dev->bus_list, &bus->devices);
    up_write(&pci_bus_sem);
}

/* 遍历设备 */
void some_function(void)
{
    struct pci_dev *dev;
    
    down_read(&pci_bus_sem);            /* 读锁 */
    list_for_each_entry(dev, &bus->devices, bus_list) {
        /* 操作 dev */
    }
    up_read(&pci_bus_sem);
}
```

### 7.4 User Configuration Space Access Blocking

```c
/* drivers/pci/access.c */

/* 
 * 某些操作期间需要阻止用户空间访问配置空间
 * (例如 BIST 测试, 电源状态转换)
 */
static DECLARE_WAIT_QUEUE_HEAD(pci_ucfg_wait);

static noinline void pci_wait_ucfg(struct pci_dev *dev)
{
    DECLARE_WAITQUEUE(wait, current);

    __add_wait_queue(&pci_ucfg_wait, &wait);
    do {
        set_current_state(TASK_UNINTERRUPTIBLE);
        raw_spin_unlock_irq(&pci_lock);
        schedule();
        raw_spin_lock_irq(&pci_lock);
    } while (dev->block_ucfg_access);
    __remove_wait_queue(&pci_ucfg_wait, &wait);
}

void pci_block_user_cfg_access(struct pci_dev *dev)
{
    unsigned long flags;
    raw_spin_lock_irqsave(&pci_lock, flags);
    dev->block_ucfg_access = 1;
    raw_spin_unlock_irqrestore(&pci_lock, flags);
}

void pci_unblock_user_cfg_access(struct pci_dev *dev)
{
    unsigned long flags;
    raw_spin_lock_irqsave(&pci_lock, flags);
    dev->block_ucfg_access = 0;
    raw_spin_unlock_irqrestore(&pci_lock, flags);
    wake_up_all(&pci_ucfg_wait);
}
```

---

## 8. Performance Considerations

### 8.1 Hot Path vs Cold Path

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     HOT PATH vs COLD PATH                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  HOT PATH (频繁执行, 性能关键):                                             │
│  ────────────────────────────────                                           │
│  ・配置空间读写 (pci_read/write_config_*)                                   │
│    - 使用 raw_spinlock (不可被抢占)                                         │
│    - 内联函数优化                                                           │
│    - 但配置空间访问本身很慢 (~100-1000 cycles)                              │
│                                                                              │
│  ・设备匹配 (pci_match_one_device)                                          │
│    - 内联函数                                                               │
│    - 简单比较操作                                                           │
│                                                                              │
│  COLD PATH (初始化/罕见操作):                                               │
│  ─────────────────────────────                                              │
│  ・总线扫描 (pci_scan_*)                                                    │
│    - 仅在启动或热插拔时                                                     │
│    - 可以使用较重的同步                                                     │
│                                                                              │
│  ・驱动注册 (pci_register_driver)                                           │
│    - 模块加载时一次                                                         │
│                                                                              │
│  ・资源分配 (pci_assign_resource)                                           │
│    - 设备初始化时一次                                                       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 8.2 NUMA Awareness

```c
/* drivers/pci/pci-driver.c */

static int pci_call_probe(struct pci_driver *drv, struct pci_dev *dev,
                          const struct pci_device_id *id)
{
    int error, node;
    struct drv_dev_and_id ddi = { drv, dev, id };

    /*
     * NUMA 优化: 在设备所在的 NUMA 节点上执行 probe
     * 这样驱动分配的内存更可能在本地节点
     */
    node = dev_to_node(&dev->dev);
    if (node >= 0) {
        int cpu;
        get_online_cpus();
        cpu = cpumask_any_and(cpumask_of_node(node), cpu_online_mask);
        if (cpu < nr_cpu_ids)
            error = work_on_cpu(cpu, local_pci_probe, &ddi);
        else
            error = local_pci_probe(&ddi);
        put_online_cpus();
    } else
        error = local_pci_probe(&ddi);
    return error;
}
```

---

## 9. Common Pitfalls & Bugs

### 9.1 典型错误

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      COMMON PCI DRIVER MISTAKES                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  1. 忘记调用 pci_enable_device()                                            │
│     ─────────────────────────────────                                       │
│     错误: 直接访问设备资源                                                   │
│     结果: 设备可能处于 D3 状态，访问返回全 1                                 │
│     修复: probe() 开始时调用 pci_enable_device()                            │
│                                                                              │
│  2. 忘记调用 pci_set_master()                                               │
│     ───────────────────────────                                             │
│     错误: 需要 DMA 的设备未设置 bus master                                   │
│     结果: 设备无法发起内存访问，DMA 失败                                     │
│     修复: DMA 设备需要调用 pci_set_master()                                  │
│                                                                              │
│  3. 资源泄露                                                                 │
│     ──────────                                                              │
│     错误: probe() 失败时未释放已分配资源                                     │
│     错误: remove() 中未释放所有资源                                          │
│     修复: 使用 goto cleanup 模式                                             │
│                                                                              │
│  4. 在中断上下文访问配置空间                                                 │
│     ────────────────────────────────                                        │
│     错误: 中断处理程序中调用 pci_read_config_*()                             │
│     结果: 可能死锁 (如果中断发生在持有 pci_lock 时)                          │
│     修复: 使用 tasklet 或 workqueue 延迟访问                                 │
│                                                                              │
│  5. 错误的 id_table 终止                                                     │
│     ────────────────────────                                                │
│     错误: id_table 数组没有空终止符                                          │
│     结果: 匹配循环越界访问                                                   │
│     修复: 以 { } 或 { 0, } 结尾                                              │
│                                                                              │
│  6. 未检查 pci_ioremap_bar() 返回值                                          │
│     ──────────────────────────────────                                      │
│     错误: 直接使用可能为 NULL 的返回值                                       │
│     结果: 内核崩溃                                                           │
│     修复: 始终检查返回值                                                     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 9.2 正确的 PCI 驱动模板

```c
/* 正确的 PCI 驱动 probe 函数模板 */

static int my_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int err;
    void __iomem *mmio;
    struct my_device *mydev;

    /* 1. 启用设备 */
    err = pci_enable_device(pdev);
    if (err)
        return err;

    /* 2. 请求 MMIO/IO 资源 */
    err = pci_request_regions(pdev, DRIVER_NAME);
    if (err)
        goto err_disable;

    /* 3. 设置 bus master (如果需要 DMA) */
    pci_set_master(pdev);

    /* 4. 映射 BAR */
    mmio = pci_ioremap_bar(pdev, 0);
    if (!mmio) {
        err = -ENOMEM;
        goto err_release;
    }

    /* 5. 分配设备私有数据 */
    mydev = kzalloc(sizeof(*mydev), GFP_KERNEL);
    if (!mydev) {
        err = -ENOMEM;
        goto err_unmap;
    }

    /* 6. 设置 DMA 掩码 */
    err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
    if (err) {
        err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
        if (err)
            goto err_free;
    }

    /* 7. 初始化设备 */
    mydev->mmio = mmio;
    pci_set_drvdata(pdev, mydev);

    return 0;

err_free:
    kfree(mydev);
err_unmap:
    iounmap(mmio);
err_release:
    pci_release_regions(pdev);
err_disable:
    pci_disable_device(pdev);
    return err;
}

static void my_remove(struct pci_dev *pdev)
{
    struct my_device *mydev = pci_get_drvdata(pdev);

    /* 逆序释放所有资源 */
    kfree(mydev);
    iounmap(mydev->mmio);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
}
```

---

## 10. How to Read This Code Yourself

### 10.1 Suggested Reading Order

```
推荐阅读顺序:

Level 1: 基础 (先理解这些)
──────────────────────────
1. include/linux/pci.h
   - struct pci_dev (核心设备结构)
   - struct pci_bus (总线结构)
   - struct pci_driver (驱动结构)
   - 常用宏和内联函数

2. drivers/pci/pci-driver.c
   - pci_bus_type 定义
   - pci_register_driver()
   - pci_bus_match()
   - pci_device_probe()

Level 2: 核心功能
──────────────────
3. drivers/pci/probe.c
   - pci_scan_device()
   - pci_setup_device()
   - pci_scan_slot()
   - pci_scan_child_bus()

4. drivers/pci/pci.c
   - pci_enable_device()
   - pci_set_master()
   - pci_save_state() / pci_restore_state()

5. drivers/pci/access.c
   - pci_bus_read/write_config_*()
   - pci_lock 的使用

Level 3: 资源管理
──────────────────
6. drivers/pci/setup-res.c
7. drivers/pci/setup-bus.c
8. drivers/pci/bus.c

Level 4: 高级主题
──────────────────
9. drivers/pci/msi.c (MSI/MSI-X)
10. drivers/pci/pcie/* (PCIe 特定)
11. drivers/pci/hotplug/* (热插拔)
```

### 10.2 Useful Grep/Cscope Commands

```bash
# 查找核心结构定义
grep -n "struct pci_dev {" include/linux/pci.h
grep -n "struct pci_bus {" include/linux/pci.h
grep -n "struct pci_driver {" include/linux/pci.h

# 查找关键函数
grep -rn "pci_scan_device" drivers/pci/
grep -rn "pci_enable_device" drivers/pci/
grep -rn "pci_register_driver" drivers/pci/

# 查找配置空间访问
grep -rn "pci_read_config" drivers/pci/
grep -rn "pci_write_config" drivers/pci/

# 查找总线类型定义
grep -rn "pci_bus_type" drivers/pci/

# 使用 cscope
cscope -R  # 在 drivers/pci/ 目录
# 然后搜索:
# - "Find this C symbol: pci_scan_slot"
# - "Find functions calling this function: pci_enable_device"
```

---

## 11. Summary & Mental Model

### 11.1 One-Paragraph Summary

Linux PCI 子系统是一个层次化的设备管理框架，它在系统启动时通过读取配置空间自动发现并枚举所有 PCI 设备，为每个设备创建 `pci_dev` 结构并加入总线层次结构。驱动程序通过 `pci_register_driver()` 注册自己的 `id_table`，内核使用设备的 vendor/device ID 进行匹配，匹配成功后调用驱动的 `probe()` 函数。驱动通常需要调用 `pci_enable_device()` 唤醒设备、`pci_request_regions()` 获取 BAR 资源、`pci_ioremap_bar()` 映射内存，然后才能与硬件交互。整个系统通过 `pci_lock` 保护配置空间访问，通过 `pci_bus_sem` 保护设备链表。

### 11.2 Key Invariants

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          KEY INVARIANTS                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  1. 每个 pci_dev 必须属于一个且仅一个 pci_bus                                │
│     dev->bus 指针始终有效                                                    │
│                                                                              │
│  2. 配置空间访问必须在 pci_lock 保护下进行                                   │
│     即使是单字节读写                                                         │
│                                                                              │
│  3. 设备必须先 enable 才能访问其资源                                         │
│     pci_enable_device() 在 pci_ioremap_bar() 之前                           │
│                                                                              │
│  4. driver 绑定后 pci_dev->driver 非空                                       │
│     解绑后变为 NULL                                                          │
│                                                                              │
│  5. 桥设备的 subordinate 指向次级总线                                        │
│     非桥设备的 subordinate 为 NULL                                           │
│                                                                              │
│  6. 总线号在整个域内唯一                                                     │
│     bus->number 不会重复                                                     │
│                                                                              │
│  7. enable_cnt 跟踪 pci_enable_device() 调用次数                            │
│     必须对称调用 pci_disable_device()                                        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 11.3 Mental Model

```
                         PCI Subsystem Mental Model
═══════════════════════════════════════════════════════════════════════════════

把 PCI 子系统想象成一个"设备管理公司":

1. 总线扫描 = "户口普查"
   - 启动时遍历所有可能的地址 (bus:device:function)
   - 发现存在的设备，登记其基本信息 (vendor, device ID)
   - 建立层次结构 (root bus → bridges → child buses)

2. 驱动注册 = "服务商登记"
   - 驱动声明自己能服务哪些设备 (id_table)
   - 提供服务能力 (probe, remove, suspend/resume)

3. 设备匹配 = "配对服务"
   - 将设备的 ID 与驱动的 id_table 比对
   - 找到匹配的驱动，建立绑定关系

4. 设备启用 = "激活账户"
   - 唤醒设备 (从 D3 到 D0)
   - 启用访问权限 (I/O, Memory)
   - 分配资源 (BAR mapping)

5. 配置空间 = "设备档案"
   - 每个设备有 256/4096 字节的配置信息
   - 通过特殊方式访问 (I/O port 或 MMCONFIG)
   - 受 pci_lock 保护

核心公式:
  PCI Device = pci_dev (软件表示) + Config Space (硬件信息) + BARs (资源)
  PCI Driver = id_table (匹配规则) + callbacks (操作方法)
  Binding = match(device, driver) → driver->probe(device)
```

---

## 12. What to Study Next

### 12.1 Related Subsystems

```
推荐学习顺序:

1. DMA / IOMMU 子系统
   ────────────────────
   - 理解 DMA mapping API
   - IOMMU (VT-d, AMD-Vi)
   - 与 PCI 设备的 DMA 操作紧密相关
   
   关键文件:
   - kernel/dma/
   - drivers/iommu/
   
2. 中断子系统 (IRQ)
   ─────────────────
   - Legacy INTx 中断
   - MSI/MSI-X 中断
   - 中断路由和分配
   
   关键文件:
   - drivers/pci/msi.c
   - kernel/irq/
   
3. 设备模型 (Device Model)
   ────────────────────────
   - kobject/kset
   - sysfs 暴露
   - uevent 机制
   
   关键文件:
   - drivers/base/
   - fs/sysfs/
   
4. 电源管理 (PM)
   ──────────────
   - Runtime PM
   - System PM (suspend/resume)
   - PCI 电源状态
   
   关键文件:
   - drivers/pci/pci.c (电源相关)
   - kernel/power/
   
5. PCIe 高级特性
   ──────────────
   - ASPM (Active State PM)
   - AER (Advanced Error Reporting)
   - SR-IOV (Virtual Functions)
   
   关键文件:
   - drivers/pci/pcie/
   - drivers/pci/iov.c
```

### 12.2 Practical Exercises

```
练习建议:

1. 初级
   ─────
   - 使用 lspci -vvv 查看系统 PCI 设备
   - 读取 /sys/bus/pci/devices/*/config
   - 编写简单的 PCI 设备 probe 驱动 (不操作硬件)

2. 中级
   ─────
   - 为虚拟设备编写完整驱动 (QEMU edu device)
   - 实现 BAR 映射和基本读写
   - 添加 MSI 中断支持

3. 高级
   ─────
   - 分析真实网卡驱动 (e1000e)
   - 理解 DMA 操作流程
   - 实现设备热插拔处理
```

---

## Appendix: Quick Reference

### A.1 Common PCI API

```c
/* 驱动注册 */
pci_register_driver(struct pci_driver *)
pci_unregister_driver(struct pci_driver *)

/* 设备启用/禁用 */
pci_enable_device(struct pci_dev *)
pci_disable_device(struct pci_dev *)
pci_set_master(struct pci_dev *)

/* 资源管理 */
pci_request_regions(struct pci_dev *, const char *)
pci_release_regions(struct pci_dev *)
pci_ioremap_bar(struct pci_dev *, int bar)

/* 配置空间访问 */
pci_read_config_byte/word/dword(struct pci_dev *, int, u8/u16/u32 *)
pci_write_config_byte/word/dword(struct pci_dev *, int, u8/u16/u32)

/* 设备查找 */
pci_get_device(unsigned int vendor, unsigned int device, struct pci_dev *from)
pci_get_subsys(vendor, device, subvendor, subdevice, from)
pci_find_bus(int domain, int busnr)

/* 电源管理 */
pci_set_power_state(struct pci_dev *, pci_power_t)
pci_save_state(struct pci_dev *)
pci_restore_state(struct pci_dev *)

/* DMA */
pci_set_dma_mask(struct pci_dev *, u64)
pci_set_consistent_dma_mask(struct pci_dev *, u64)

/* MSI/MSI-X */
pci_enable_msi(struct pci_dev *)
pci_disable_msi(struct pci_dev *)
pci_enable_msix(struct pci_dev *, struct msix_entry *, int)
pci_disable_msix(struct pci_dev *)
```

### A.2 PCI Configuration Space Layout

```
Standard PCI Configuration Space (256 bytes)
═══════════════════════════════════════════════════════════════════════════════

Offset  Size  Name                    Description
──────  ────  ────────────────────    ─────────────────────────────────────────
0x00    2     Vendor ID               厂商标识 (0xFFFF = 无设备)
0x02    2     Device ID               设备标识
0x04    2     Command                 命令寄存器 (I/O, Memory, Bus Master)
0x06    2     Status                  状态寄存器
0x08    1     Revision ID             修订号
0x09    1     Prog IF                 编程接口
0x0A    1     Sub Class               子类代码
0x0B    1     Base Class              基类代码
0x0C    1     Cache Line Size         缓存行大小
0x0D    1     Latency Timer           延迟计时器
0x0E    1     Header Type             头部类型 (0=普通, 1=桥, 2=CardBus)
0x0F    1     BIST                    内建自测试
0x10    4     BAR0                    Base Address Register 0
0x14    4     BAR1                    Base Address Register 1
0x18    4     BAR2                    Base Address Register 2
0x1C    4     BAR3                    Base Address Register 3
0x20    4     BAR4                    Base Address Register 4
0x24    4     BAR5                    Base Address Register 5
0x28    4     CardBus CIS Ptr         CardBus CIS 指针
0x2C    2     Subsystem Vendor ID     子系统厂商 ID
0x2E    2     Subsystem ID            子系统 ID
0x30    4     Expansion ROM           扩展 ROM 基地址
0x34    1     Capabilities Ptr        Capabilities 链表指针
0x35    7     Reserved                保留
0x3C    1     Interrupt Line          中断线 (IRQ)
0x3D    1     Interrupt Pin           中断引脚 (INTA#-INTD#)
0x3E    1     Min_Gnt                 最小授权
0x3F    1     Max_Lat                 最大延迟
0x40+         Device-specific         设备特定配置
```

