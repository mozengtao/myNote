# WHAT｜具体架构

## 1. 模式：Hypervisor 模型

```
PATTERNS: HYPERVISOR MODEL
+=============================================================================+
|                                                                              |
|  TYPE 1 vs TYPE 2 HYPERVISORS                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  TYPE 1 (Bare Metal):                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐                 │    │ |
|  │  │  │    VM 1     │ │    VM 2     │ │    VM 3     │                 │    │ |
|  │  │  └──────┬──────┘ └──────┬──────┘ └──────┬──────┘                 │    │ |
|  │  │         │               │               │                        │    │ |
|  │  │  ═══════╪═══════════════╪═══════════════╪════════════════════   │    │ |
|  │  │         │               │               │                        │    │ |
|  │  │         └───────────────┴───────────────┘                        │    │ |
|  │  │                         │                                        │    │ |
|  │  │                         ▼                                        │    │ |
|  │  │         ┌──────────────────────────────────────┐                 │    │ |
|  │  │         │            HYPERVISOR                │                 │    │ |
|  │  │         │      (runs directly on HW)           │                 │    │ |
|  │  │         │                                      │                 │    │ |
|  │  │         │  Examples: Xen, VMware ESXi,         │                 │    │ |
|  │  │         │            Microsoft Hyper-V         │                 │    │ |
|  │  │         └──────────────────────────────────────┘                 │    │ |
|  │  │                         │                                        │    │ |
|  │  │  ═══════════════════════╪═══════════════════════════════════════ │    │ |
|  │  │                         │                                        │    │ |
|  │  │                         ▼                                        │    │ |
|  │  │         ┌──────────────────────────────────────┐                 │    │ |
|  │  │         │             HARDWARE                 │                 │    │ |
|  │  │         └──────────────────────────────────────┘                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  TYPE 2 (Hosted):                                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐│    │ |
|  │  │  │    VM 1     │ │    VM 2     │ │  Browser    │ │   Editor    ││    │ |
|  │  │  └──────┬──────┘ └──────┬──────┘ └──────┬──────┘ └──────┬──────┘│    │ |
|  │  │         │               │               │               │       │    │ |
|  │  │         │               │               │               │       │    │ |
|  │  │         └───────────────┴───────────────┴───────────────┘       │    │ |
|  │  │                              │                                   │    │ |
|  │  │                              ▼                                   │    │ |
|  │  │         ┌──────────────────────────────────────┐                 │    │ |
|  │  │         │          HOST OS (Linux)             │                 │    │ |
|  │  │         │                                      │                 │    │ |
|  │  │         │  ┌────────────────────────────────┐  │                 │    │ |
|  │  │         │  │ QEMU + KVM (hypervisor module) │  │                 │    │ |
|  │  │         │  └────────────────────────────────┘  │                 │    │ |
|  │  │         │                                      │                 │    │ |
|  │  │         │  Examples: KVM, VirtualBox, VMware   │                 │    │ |
|  │  │         │            Workstation               │                 │    │ |
|  │  │         └──────────────────────────────────────┘                 │    │ |
|  │  │                         │                                        │    │ |
|  │  │  ═══════════════════════╪═══════════════════════════════════════ │    │ |
|  │  │                         │                                        │    │ |
|  │  │                         ▼                                        │    │ |
|  │  │         ┌──────────────────────────────────────┐                 │    │ |
|  │  │         │             HARDWARE                 │                 │    │ |
|  │  │         └──────────────────────────────────────┘                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  KVM: THE HYBRID MODEL                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  KVM is BOTH Type 1 and Type 2:                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • Linux kernel IS the hypervisor (Type 1 aspect)                │    │ |
|  │  │  • Runs on full Linux OS (Type 2 aspect)                         │    │ |
|  │  │  • Best of both worlds:                                          │    │ |
|  │  │    - Full Linux ecosystem (drivers, tools)                       │    │ |
|  │  │    - Direct hardware control (near Type 1 performance)           │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ┌───────────────────────────────────────────────────────┐ │ │    │ |
|  │  │  │  │                    GUEST VMs                          │ │ │    │ |
|  │  │  │  │                                                       │ │ │    │ |
|  │  │  │  │  Each VM is a QEMU process:                           │ │ │    │ |
|  │  │  │  │  • vCPUs = threads in QEMU                            │ │ │    │ |
|  │  │  │  │  • RAM = mmap'd memory region                         │ │ │    │ |
|  │  │  │  │  • Devices = QEMU device emulation                    │ │ │    │ |
|  │  │  │  │                                                       │ │ │    │ |
|  │  │  │  └───────────────────────────────────────────────────────┘ │ │    │ |
|  │  │  │                              │                              │ │    │ |
|  │  │  │                              │ ioctl(/dev/kvm)              │ │    │ |
|  │  │  │                              ▼                              │ │    │ |
|  │  │  │  ┌───────────────────────────────────────────────────────┐ │ │    │ |
|  │  │  │  │                 LINUX KERNEL + KVM                    │ │ │    │ |
|  │  │  │  │                                                       │ │ │    │ |
|  │  │  │  │  • KVM module: CPU/memory virtualization              │ │ │    │ |
|  │  │  │  │  • Linux scheduler: schedules vCPU threads            │ │ │    │ |
|  │  │  │  │  • Linux memory: manages guest RAM                    │ │ │    │ |
|  │  │  │  │  • Linux I/O: vhost for networking/block              │ │ │    │ |
|  │  │  │  │                                                       │ │ │    │ |
|  │  │  │  └───────────────────────────────────────────────────────┘ │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**模式：Hypervisor 模型**

**Type 1（裸机）**：
- Hypervisor 直接运行在硬件上
- 例子：Xen、VMware ESXi、Hyper-V

**Type 2（托管）**：
- Hypervisor 运行在 Host OS 上
- 例子：VirtualBox、VMware Workstation

**KVM：混合模型**
- 既是 Type 1 又是 Type 2：
  - Linux 内核就是 hypervisor（Type 1 方面）
  - 运行在完整 Linux OS 上（Type 2 方面）
- 两全其美：
  - 完整 Linux 生态系统（驱动、工具）
  - 直接硬件控制（接近 Type 1 性能）

每个 VM 是一个 QEMU 进程：
- vCPU = QEMU 中的线程
- RAM = mmap 的内存区域
- 设备 = QEMU 设备模拟

---

## 2. 核心结构：kvm 和 vcpu

```
CORE STRUCTURES: KVM AND VCPU
+=============================================================================+
|                                                                              |
|  STRUCT KVM (include/linux/kvm_host.h)                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct kvm {                                                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* MEMORY MANAGEMENT */                                         │    │ |
|  │  │  struct kvm_memslots *memslots[KVM_ADDRESS_SPACE_NUM];           │    │ |
|  │  │      // Guest physical → Host virtual mapping                    │    │ |
|  │  │      // Multiple slots for different regions                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct mm_struct *mm;     // Host process mm (for mmap)         │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* vCPU MANAGEMENT */                                           │    │ |
|  │  │  struct list_head vcpus;   // List of all vCPUs                  │    │ |
|  │  │  int online_vcpus;         // Number of created vCPUs            │    │ |
|  │  │  int max_vcpus;            // Maximum allowed                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* INTERRUPT ROUTING */                                         │    │ |
|  │  │  struct kvm_irq_routing_table *irq_routing;                      │    │ |
|  │  │      // Maps GSI → MSI or virtual IOAPIC pin                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* I/O BUS EMULATION */                                         │    │ |
|  │  │  struct kvm_io_bus *buses[KVM_NR_BUSES];                         │    │ |
|  │  │      // PIO and MMIO handlers                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* ARCHITECTURE-SPECIFIC */                                     │    │ |
|  │  │  struct kvm_arch arch;     // EPT root, APIC, etc.               │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* SYNCHRONIZATION */                                           │    │ |
|  │  │  spinlock_t mmu_lock;      // MMU/EPT modifications              │    │ |
|  │  │  struct mutex slots_lock;  // Memory slot changes                │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* STATISTICS */                                                │    │ |
|  │  │  struct kvm_stat_data *debugfs_entries;                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT KVM_VCPU (include/linux/kvm_host.h)                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct kvm_vcpu {                                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* IDENTITY */                                                  │    │ |
|  │  │  struct kvm *kvm;          // Parent VM                          │    │ |
|  │  │  int vcpu_id;              // vCPU index                         │    │ |
|  │  │  int cpu;                  // Physical CPU running on (-1 if not)│    │ |
|  │  │                                                                  │    │ |
|  │  │  /* EXECUTION STATE */                                           │    │ |
|  │  │  int mode;                 // IN_GUEST_MODE, OUTSIDE_GUEST_MODE  │    │ |
|  │  │  struct mutex mutex;       // Per-vCPU lock                      │    │ |
|  │  │  struct kvm_run *run;      // Shared with userspace (QEMU)       │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* GUEST STATE */                                               │    │ |
|  │  │  struct kvm_vcpu_arch arch;// VMCS pointer, registers, etc.      │    │ |
|  │  │      //  .regs[]           // General purpose registers          │    │ |
|  │  │      //  .cr0, .cr3, .cr4  // Control registers                  │    │ |
|  │  │      //  .vmcs             // Virtual Machine Control Structure  │    │ |
|  │  │      //  .apic             // Virtual LAPIC state                │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* PREEMPTION */                                                │    │ |
|  │  │  bool preempted;           // Was preempted while in guest       │    │ |
|  │  │  struct task_struct *task; // Host task for this vCPU            │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* INTERRUPT INJECTION */                                       │    │ |
|  │  │  bool wants_irq;           // Guest wants interrupt              │    │ |
|  │  │  struct kvm_queued_exception exception;                          │    │ |
|  │  │  struct kvm_queued_interrupt interrupt;                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* STATISTICS */                                                │    │ |
|  │  │  struct kvm_vcpu_stat stat; // Exit counts, timing               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT KVM_RUN (shared with userspace)                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct kvm_run {                                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* REQUEST FROM QEMU TO KVM */                                  │    │ |
|  │  │  __u8 request_interrupt_window;  // Want interrupt notification  │    │ |
|  │  │  __u8 immediate_exit;            // Exit immediately             │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* RESPONSE FROM KVM TO QEMU */                                 │    │ |
|  │  │  __u32 exit_reason;              // Why we exited                │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* EXIT-SPECIFIC DATA */                                        │    │ |
|  │  │  union {                                                         │    │ |
|  │  │      struct {  /* KVM_EXIT_IO */                                 │    │ |
|  │  │          __u8 direction;         // In or out                    │    │ |
|  │  │          __u8 size;              // 1, 2, or 4 bytes             │    │ |
|  │  │          __u16 port;             // I/O port                     │    │ |
|  │  │          __u32 count;            // Rep count                    │    │ |
|  │  │          __u64 data_offset;      // Offset in run struct         │    │ |
|  │  │      } io;                                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │      struct {  /* KVM_EXIT_MMIO */                               │    │ |
|  │  │          __u64 phys_addr;        // Guest physical address       │    │ |
|  │  │          __u8 data[8];           // Data read/written            │    │ |
|  │  │          __u32 len;              // Access size                  │    │ |
|  │  │          __u8 is_write;                                          │    │ |
|  │  │      } mmio;                                                     │    │ |
|  │  │                                                                  │    │ |
|  │  │      struct { /* KVM_EXIT_HYPERCALL */ } hypercall;              │    │ |
|  │  │      struct { /* KVM_EXIT_INTERNAL_ERROR */ } internal;          │    │ |
|  │  │      // ... more exit types                                      │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**核心结构：kvm 和 vcpu**

**struct kvm**（include/linux/kvm_host.h）：
- **内存管理**：`memslots[]` Guest 物理 → Host 虚拟映射
- **vCPU 管理**：`vcpus` 列表、在线数、最大数
- **中断路由**：`irq_routing` GSI → MSI 映射
- **I/O 总线模拟**：`buses[]` PIO 和 MMIO 处理器
- **架构特定**：`arch` EPT 根、APIC 等
- **同步**：`mmu_lock`、`slots_lock`

**struct kvm_vcpu**：
- **身份**：`kvm`（父 VM）、`vcpu_id`、`cpu`（物理 CPU）
- **执行状态**：`mode`（IN_GUEST_MODE）、`run`（与用户空间共享）
- **Guest 状态**：`arch`（VMCS 指针、寄存器、CR0/CR3/CR4、APIC）
- **抢占**：`preempted`、`task`（Host 任务）
- **中断注入**：`wants_irq`、`exception`、`interrupt`

**struct kvm_run**（与用户空间共享）：
- QEMU → KVM 请求：`request_interrupt_window`、`immediate_exit`
- KVM → QEMU 响应：`exit_reason`
- 退出特定数据：`io`（端口、大小、方向）、`mmio`（地址、数据、大小）

---

## 3. 控制流：VM Exit 路径

```
CONTROL FLOW: VM EXIT PATH
+=============================================================================+
|                                                                              |
|  DETAILED VM EXIT PATH (x86/Intel)                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  [QEMU calls ioctl(KVM_RUN)]                                     │    │ |
|  │  │          │                                                       │    │ |
|  │  │          ▼                                                       │    │ |
|  │  │  kvm_vcpu_ioctl()  [virt/kvm/kvm_main.c]                         │    │ |
|  │  │          │                                                       │    │ |
|  │  │          ▼                                                       │    │ |
|  │  │  kvm_arch_vcpu_ioctl_run()  [arch/x86/kvm/x86.c]                 │    │ |
|  │  │          │                                                       │    │ |
|  │  │          ▼                                                       │    │ |
|  │  │  vcpu_run()                                                      │    │ |
|  │  │          │                                                       │    │ |
|  │  │          │  ┌────────────────────────────────────────────────┐   │    │ |
|  │  │          │  │                VCPU RUN LOOP                   │   │    │ |
|  │  │          │  │                                                 │   │    │ |
|  │  │          │  │  while (1) {                                    │   │    │ |
|  │  │          │  │      vcpu_enter_guest();                        │   │    │ |
|  │  │          │  │      if (need_exit_to_userspace)                │   │    │ |
|  │  │          │  │          break;                                 │   │    │ |
|  │  │          │  │  }                                              │   │    │ |
|  │  │          │  │                                                 │   │    │ |
|  │  │          │  └────────────────────────────────────────────────┘   │    │ |
|  │  │          │                                                       │    │ |
|  │  │          ▼                                                       │    │ |
|  │  │  vcpu_enter_guest()                                              │    │ |
|  │  │          │                                                       │    │ |
|  │  │          ├── kvm_mmu_reload()          // Sync EPT if needed     │    │ |
|  │  │          ├── kvm_vcpu_inject_irq()     // Inject pending IRQ     │    │ |
|  │  │          ├── preempt_disable()                                   │    │ |
|  │  │          │                                                       │    │ |
|  │  │          ▼                                                       │    │ |
|  │  │  vmx_vcpu_run()  [arch/x86/kvm/vmx.c]                            │    │ |
|  │  │          │                                                       │    │ |
|  │  │          │  1. Load guest state                                  │    │ |
|  │  │          │     vmcs_writel(GUEST_RSP, vcpu->arch.regs[RSP]);     │    │ |
|  │  │          │     vmcs_writel(GUEST_RIP, vcpu->arch.regs[RIP]);     │    │ |
|  │  │          │                                                       │    │ |
|  │  │          │  2. VMLAUNCH or VMRESUME  ◄───────────────────────┐  │    │ |
|  │  │          │     // CPU enters VMX non-root mode               │  │    │ |
|  │  │          │     // Guest code runs at native speed            │  │    │ |
|  │  │          │                                                   │  │    │ |
|  │  │          │         ~~~~~~ GUEST RUNNING ~~~~~~               │  │    │ |
|  │  │          │                                                   │  │    │ |
|  │  │          │  3. VM EXIT (hardware)                            │  │    │ |
|  │  │          │     // CPU back to VMX root mode                  │  │    │ |
|  │  │          │     // Guest state saved to VMCS                  │  │    │ |
|  │  │          │                                                       │    │ |
|  │  │          ▼                                                       │    │ |
|  │  │  vmx_handle_exit()                                               │    │ |
|  │  │          │                                                       │    │ |
|  │  │          │  exit_reason = vmcs_read32(VM_EXIT_REASON);           │    │ |
|  │  │          │                                                       │    │ |
|  │  │          │  ┌────────────────────────────────────────────────┐   │    │ |
|  │  │          │  │  EXIT REASON HANDLERS                          │   │    │ |
|  │  │          │  │                                                 │   │    │ |
|  │  │          │  │  [EXIT_REASON_EPT_VIOLATION]                    │   │    │ |
|  │  │          │  │      └── handle_ept_violation()                 │   │    │ |
|  │  │          │  │          └── kvm_mmu_page_fault()               │   │    │ |
|  │  │          │  │              // Map guest page, VMRESUME ──────┼───┘    │ |
|  │  │          │  │                                                 │   │    │ |
|  │  │          │  │  [EXIT_REASON_IO_INSTRUCTION]                   │   │    │ |
|  │  │          │  │      └── handle_io()                            │   │    │ |
|  │  │          │  │          ├── kernel I/O (if in-kernel handler)  │   │    │ |
|  │  │          │  │          │   └── VMRESUME ─────────────────────┼───┘    │ |
|  │  │          │  │          └── return KVM_EXIT_IO (to QEMU)       │   │    │ |
|  │  │          │  │                                                 │   │    │ |
|  │  │          │  │  [EXIT_REASON_EXTERNAL_INTERRUPT]               │   │    │ |
|  │  │          │  │      └── // Do nothing, let Linux handle        │   │    │ |
|  │  │          │  │          └── VMRESUME ─────────────────────────┼───┘    │ |
|  │  │          │  │                                                 │   │    │ |
|  │  │          │  │  [EXIT_REASON_HLT]                              │   │    │ |
|  │  │          │  │      └── kvm_emulate_halt()                     │   │    │ |
|  │  │          │  │          └── if (has_pending_irq)               │   │    │ |
|  │  │          │  │                  VMRESUME ─────────────────────┼───┘    │ |
|  │  │          │  │              else                               │   │    │ |
|  │  │          │  │                  return KVM_EXIT_HLT            │   │    │ |
|  │  │          │  │                                                 │   │    │ |
|  │  │          │  └────────────────────────────────────────────────┘   │    │ |
|  │  │          │                                                       │    │ |
|  │  │          │  preempt_enable()                                     │    │ |
|  │  │          │                                                       │    │ |
|  │  │          ▼                                                       │    │ |
|  │  │  [Return to QEMU if needed]                                      │    │ |
|  │  │          │                                                       │    │ |
|  │  │          ▼                                                       │    │ |
|  │  │  QEMU: kvm_run->exit_reason                                      │    │ |
|  │  │        Handle I/O, MMIO, etc.                                    │    │ |
|  │  │        ioctl(KVM_RUN) again                                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**控制流：VM Exit 路径**

**详细 VM Exit 路径**：

1. **QEMU 调用 ioctl(KVM_RUN)**

2. **内核入口**：
   - `kvm_vcpu_ioctl()` → `kvm_arch_vcpu_ioctl_run()` → `vcpu_run()`

3. **vCPU 运行循环**：
   ```
   while (1) {
       vcpu_enter_guest();
       if (need_exit_to_userspace)
           break;
   }
   ```

4. **vcpu_enter_guest()**：
   - `kvm_mmu_reload()`：同步 EPT
   - `kvm_vcpu_inject_irq()`：注入待处理 IRQ
   - `preempt_disable()`

5. **vmx_vcpu_run()**：
   - 加载 guest 状态到 VMCS
   - **VMLAUNCH/VMRESUME**：CPU 进入 VMX non-root 模式
   - Guest 代码以原生速度运行
   - **VM EXIT**：CPU 返回 VMX root 模式

6. **vmx_handle_exit()**：
   - 读取退出原因
   - **EPT 违规**：`handle_ept_violation()` → 映射页 → VMRESUME
   - **I/O 指令**：`handle_io()` → 内核处理或返回 QEMU
   - **外部中断**：让 Linux 处理 → VMRESUME
   - **HLT**：如有待处理 IRQ 则 VMRESUME，否则返回 QEMU

7. **返回 QEMU**：处理 I/O/MMIO，再次 ioctl(KVM_RUN)

---

## 4. 扩展点

```
EXTENSION POINTS
+=============================================================================+
|                                                                              |
|  1. DEVICE EMULATION                                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  QEMU Device Model:                                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • Add new virtual devices without kernel changes                │    │ |
|  │  │  • QEMU's QOM (QEMU Object Model)                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  Example: Adding a new PCI device                                │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  static void my_device_class_init(ObjectClass *klass) {     │ │    │ |
|  │  │  │      DeviceClass *dc = DEVICE_CLASS(klass);                 │ │    │ |
|  │  │  │      PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);          │ │    │ |
|  │  │  │      pc->realize = my_device_realize;                       │ │    │ |
|  │  │  │      pc->vendor_id = 0x1234;                                │ │    │ |
|  │  │  │      pc->device_id = 0x5678;                                │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  In-Kernel Device Emulation (vhost):                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • Bypass QEMU for performance-critical devices                  │    │ |
|  │  │  • vhost-net, vhost-blk, vhost-scsi                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │  Guest ──► virtio ──► kernel vhost ──► physical device     │ │    │ |
|  │  │  │           (no QEMU in data path!)                           │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  2. PARAVIRTUALIZATION (virtio)                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Instead of emulating real hardware:                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  Emulated (slow):                Paravirt (fast):                │    │ |
|  │  │  ┌────────────────────┐          ┌────────────────────┐         │    │ |
|  │  │  │ Guest driver:      │          │ Guest driver:      │         │    │ |
|  │  │  │  outb(0x1f0, data) │          │  virtqueue_add()   │         │    │ |
|  │  │  │    │               │          │    │               │         │    │ |
|  │  │  │    │ VM exit       │          │    │ no exit       │         │    │ |
|  │  │  │    ▼               │          │    ▼               │         │    │ |
|  │  │  │ QEMU emulates ATA  │          │ Notify host via    │         │    │ |
|  │  │  │ register behavior  │          │ single doorbell    │         │    │ |
|  │  │  │                    │          │ write              │         │    │ |
|  │  │  └────────────────────┘          └────────────────────┘         │    │ |
|  │  │                                                                  │    │ |
|  │  │  virtio devices:                                                 │    │ |
|  │  │  • virtio-net: ~90% of native network throughput                 │    │ |
|  │  │  • virtio-blk: ~95% of native disk performance                   │    │ |
|  │  │  • virtio-gpu, virtio-fs, virtio-vsock, ...                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  3. PASS-THROUGH (VFIO)                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Give guest direct access to physical hardware:                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Guest ──► physical GPU (via IOMMU protection)              │ │    │ |
|  │  │  │        ──► physical NVMe                                    │ │    │ |
|  │  │  │        ──► physical NIC (SR-IOV VF)                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  100% native performance!                                   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  VFIO provides:                                                  │    │ |
|  │  │  • Safe device assignment (IOMMU protection)                     │    │ |
|  │  │  • Interrupt remapping                                           │    │ |
|  │  │  • DMA remapping                                                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**扩展点**：

**1. 设备模拟**：
- **QEMU 设备模型**：无需内核修改添加新虚拟设备
- **内核内设备模拟（vhost）**：绕过 QEMU 用于性能关键设备

**2. 半虚拟化（virtio）**：
- 模拟（慢）：outb() → VM exit → QEMU 模拟
- 半虚拟化（快）：virtqueue_add() → 单次 doorbell 写
- virtio 设备：
  - virtio-net: ~90% 原生网络吞吐量
  - virtio-blk: ~95% 原生磁盘性能

**3. 直通（VFIO）**：
- 给 guest 直接访问物理硬件：
  - GPU（通过 IOMMU 保护）
  - NVMe
  - NIC（SR-IOV VF）
- 100% 原生性能！
- VFIO 提供：安全设备分配、中断重映射、DMA 重映射

---

## 5. 限制：I/O 性能

```
LIMITS: I/O PERFORMANCE
+=============================================================================+
|                                                                              |
|  THE I/O PERFORMANCE GAP                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Overhead breakdown for emulated I/O:                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  Guest writes to I/O port:                                       │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │  1. VM Exit                    ~1000 cycles                 │ │    │ |
|  │  │  │  2. KVM exit handler           ~500 cycles                  │ │    │ |
|  │  │  │  3. Copy to userspace          ~200 cycles                  │ │    │ |
|  │  │  │  4. Context switch to QEMU     ~5000 cycles                 │ │    │ |
|  │  │  │  5. QEMU I/O dispatch          ~500 cycles                  │ │    │ |
|  │  │  │  6. Device emulation           ~1000 cycles                 │ │    │ |
|  │  │  │  7. Host I/O syscall           ~1000 cycles                 │ │    │ |
|  │  │  │  8. Return to KVM              ~5000 cycles                 │ │    │ |
|  │  │  │  9. VM Entry                   ~1000 cycles                 │ │    │ |
|  │  │  │  ─────────────────────────────────────────                  │ │    │ |
|  │  │  │  TOTAL: ~15000 cycles per I/O                               │ │    │ |
|  │  │  │  At 3 GHz: ~5 microseconds                                  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Compare: native I/O ~100 nanoseconds                       │ │    │ |
|  │  │  │  Overhead: 50x!                                             │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MITIGATION HIERARCHY                                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Performance (worst to best):                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Level 1: FULL EMULATION (slowest)                          │ │    │ |
|  │  │  │  ┌──────────────────────────────────────────────────────┐  │ │    │ |
|  │  │  │  │  Guest ─► VM exit ─► KVM ─► QEMU ─► syscall          │  │ │    │ |
|  │  │  │  │  Every I/O causes exit to userspace                  │  │ │    │ |
|  │  │  │  │  50-100x native latency                              │  │ │    │ |
|  │  │  │  └──────────────────────────────────────────────────────┘  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Level 2: IOEVENTFD/IRQFD (better)                          │ │    │ |
|  │  │  │  ┌──────────────────────────────────────────────────────┐  │ │    │ |
|  │  │  │  │  Guest write ─► eventfd ─► wake QEMU (no KVM exit)   │  │ │    │ |
|  │  │  │  │  Interrupt ─► irqfd ─► inject (no QEMU)              │  │ │    │ |
|  │  │  │  │  10-20x native latency                               │  │ │    │ |
|  │  │  │  └──────────────────────────────────────────────────────┘  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Level 3: VHOST (better)                                    │ │    │ |
|  │  │  │  ┌──────────────────────────────────────────────────────┐  │ │    │ |
|  │  │  │  │  Guest ─► virtqueue ─► kernel vhost ─► device        │  │ │    │ |
|  │  │  │  │  No QEMU in data path                                │  │ │    │ |
|  │  │  │  │  3-5x native latency                                 │  │ │    │ |
|  │  │  │  └──────────────────────────────────────────────────────┘  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Level 4: VFIO PASSTHROUGH (native)                         │ │    │ |
|  │  │  │  ┌──────────────────────────────────────────────────────┐  │ │    │ |
|  │  │  │  │  Guest ─► direct HW access (via IOMMU)               │  │ │    │ |
|  │  │  │  │  1x native latency (with overhead for IOMMU)         │  │ │    │ |
|  │  │  │  └──────────────────────────────────────────────────────┘  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Trade-offs:                                                     │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │              Performance  Flexibility  Isolation  Cost      │ │    │ |
|  │  │  │  Emulated    Poor         Excellent    Excellent  Low       │ │    │ |
|  │  │  │  virtio      Good         Good         Excellent  Low       │ │    │ |
|  │  │  │  vhost       Very Good    Good         Excellent  Low       │ │    │ |
|  │  │  │  VFIO        Excellent    Poor         Good       High      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**限制：I/O 性能**

**I/O 性能差距**：

模拟 I/O 开销分解：
1. VM Exit: ~1000 周期
2. KVM 退出处理: ~500 周期
3. 拷贝到用户空间: ~200 周期
4. 上下文切换到 QEMU: ~5000 周期
5. QEMU I/O 分发: ~500 周期
6. 设备模拟: ~1000 周期
7. Host I/O 系统调用: ~1000 周期
8. 返回 KVM: ~5000 周期
9. VM Entry: ~1000 周期

**总计**: ~15000 周期/I/O（~5 微秒）
**对比**: 原生 I/O ~100 纳秒
**开销**: 50 倍！

**缓解层次**：

| 级别 | 方法 | 延迟 |
|------|------|------|
| 1 | 完全模拟 | 50-100x 原生 |
| 2 | ioeventfd/irqfd | 10-20x 原生 |
| 3 | vhost | 3-5x 原生 |
| 4 | VFIO 直通 | 1x 原生 |

**权衡**：
- 模拟：性能差，灵活性/隔离性优秀，成本低
- VFIO：性能优秀，灵活性差，成本高
