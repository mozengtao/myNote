# WHERE｜源代码地图

## 1. arch/x86/kvm/ 目录结构

```
ARCH/X86/KVM/ DIRECTORY STRUCTURE
+=============================================================================+
|                                                                              |
|  KVM SOURCE ORGANIZATION                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ARCHITECTURE-INDEPENDENT (virt/kvm/)                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  virt/kvm/                                                       │    │ |
|  │  │  ├── kvm_main.c        ◄── /dev/kvm interface, VM/vCPU ioctls   │    │ |
|  │  │  │                          kvm_dev_ioctl(), kvm_vm_ioctl()      │    │ |
|  │  │  │                          kvm_vcpu_ioctl()                     │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── eventfd.c         ◄── ioeventfd/irqfd implementation       │    │ |
|  │  │  │                          Fast I/O notification               │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── irqchip.c         ◄── Generic interrupt routing            │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── coalesced_mmio.c  ◄── Batched MMIO for performance         │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── async_pf.c        ◄── Async page fault handling            │    │ |
|  │  │  │                                                               │    │ |
|  │  │  └── vfio.c            ◄── VFIO integration for passthrough     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  ARCHITECTURE-SPECIFIC (arch/x86/kvm/)                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  arch/x86/kvm/                                                   │    │ |
|  │  │  │                                                               │    │ |
|  │  │  │  CORE X86 KVM                                                 │    │ |
|  │  │  ├── x86.c             ◄── x86-specific vcpu operations         │    │ |
|  │  │  │                          kvm_arch_vcpu_ioctl_run()            │    │ |
|  │  │  │                          kvm_x86_ops dispatch                 │    │ |
|  │  │  │                          CPUID handling                       │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── emulate.c         ◄── x86 instruction emulator             │    │ |
|  │  │  │                          For instructions that can't run     │    │ |
|  │  │  │                          directly (real mode, etc.)          │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── mmu.c             ◄── MMU/EPT implementation               │    │ |
|  │  │  │                          Shadow page tables                   │    │ |
|  │  │  │                          kvm_mmu_page_fault()                 │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── mmu/              ◄── MMU subsystem                         │    │ |
|  │  │  │   ├── mmu.c                                                   │    │ |
|  │  │  │   ├── tdp_mmu.c     ◄── Two-Dimensional Paging (EPT/NPT)     │    │ |
|  │  │  │   ├── page_track.c  ◄── Page tracking for write protection   │    │ |
|  │  │  │   └── spte.c        ◄── Shadow PTE operations                │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── irq.c             ◄── Virtual interrupt controller         │    │ |
|  │  │  ├── i8259.c           ◄── Virtual PIC emulation                │    │ |
|  │  │  ├── ioapic.c          ◄── Virtual IOAPIC emulation             │    │ |
|  │  │  ├── lapic.c           ◄── Virtual LAPIC emulation              │    │ |
|  │  │  │                                                               │    │ |
|  │  │  │  INTEL VMX SPECIFIC                                           │    │ |
|  │  │  ├── vmx/              ◄── Intel VT-x implementation            │    │ |
|  │  │  │   ├── vmx.c         ◄── Main VMX file                        │    │ |
|  │  │  │   │                      vmx_vcpu_run(), vmx_handle_exit()   │    │ |
|  │  │  │   ├── vmenter.S     ◄── VM entry/exit assembly               │    │ |
|  │  │  │   ├── vmcs.c        ◄── VMCS manipulation                    │    │ |
|  │  │  │   ├── posted_intr.c ◄── Posted interrupt support             │    │ |
|  │  │  │   ├── nested.c      ◄── Nested virtualization (L1/L2)        │    │ |
|  │  │  │   └── pmu_intel.c   ◄── Virtual PMU                          │    │ |
|  │  │  │                                                               │    │ |
|  │  │  │  AMD SVM SPECIFIC                                             │    │ |
|  │  │  ├── svm/              ◄── AMD-V implementation                 │    │ |
|  │  │  │   ├── svm.c         ◄── Main SVM file                        │    │ |
|  │  │  │   ├── vmenter.S                                               │    │ |
|  │  │  │   ├── nested.c                                                │    │ |
|  │  │  │   └── sev.c         ◄── Secure Encrypted Virtualization      │    │ |
|  │  │  │                                                               │    │ |
|  │  │  └── cpuid.c           ◄── CPUID emulation/filtering            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  INCLUDE FILES                                                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  include/linux/kvm_host.h     ◄── Core KVM structures           │    │ |
|  │  │                                    struct kvm, struct kvm_vcpu   │    │ |
|  │  │                                                                  │    │ |
|  │  │  include/uapi/linux/kvm.h     ◄── User API (ioctl interface)    │    │ |
|  │  │                                    KVM_CREATE_VM, KVM_RUN, etc.  │    │ |
|  │  │                                                                  │    │ |
|  │  │  arch/x86/include/asm/kvm_host.h  ◄── x86 arch-specific structs │    │ |
|  │  │                                        kvm_vcpu_arch, kvm_arch   │    │ |
|  │  │                                                                  │    │ |
|  │  │  arch/x86/include/asm/vmx.h   ◄── VMX constants and macros      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**源代码组织**：

**架构无关（virt/kvm/）**：
- `kvm_main.c`：/dev/kvm 接口，VM/vCPU ioctl
- `eventfd.c`：ioeventfd/irqfd 实现
- `irqchip.c`：通用中断路由
- `vfio.c`：VFIO 直通集成

**架构特定（arch/x86/kvm/）**：
- `x86.c`：x86 特定 vcpu 操作，kvm_arch_vcpu_ioctl_run()
- `emulate.c`：x86 指令模拟器
- `mmu.c` / `mmu/`：MMU/EPT 实现，shadow page tables
- `irq.c`, `lapic.c`, `ioapic.c`：虚拟中断控制器

**Intel VMX（vmx/）**：
- `vmx.c`：主 VMX 文件，vmx_vcpu_run(), vmx_handle_exit()
- `vmenter.S`：VM entry/exit 汇编
- `nested.c`：嵌套虚拟化

**AMD SVM（svm/）**：
- `svm.c`：主 SVM 文件
- `sev.c`：安全加密虚拟化

---

## 2. 架构锚点：struct kvm

```
ARCHITECTURAL ANCHOR: STRUCT KVM
+=============================================================================+
|                                                                              |
|  WHERE TO FIND STRUCT KVM                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Definition: include/linux/kvm_host.h                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct kvm {                                                    │    │ |
|  │  │      spinlock_t mmu_lock;                                        │    │ |
|  │  │      struct mutex slots_lock;                                    │    │ |
|  │  │      struct mm_struct *mm;                                       │    │ |
|  │  │      struct kvm_memslots __rcu *memslots[KVM_ADDRESS_SPACE_NUM]; │    │ |
|  │  │      struct kvm_vcpu *vcpus[KVM_MAX_VCPUS];                      │    │ |
|  │  │      atomic_t online_vcpus;                                      │    │ |
|  │  │      struct kvm_io_bus __rcu *buses[KVM_NR_BUSES];               │    │ |
|  │  │      struct kvm_irq_routing_table __rcu *irq_routing;            │    │ |
|  │  │      struct kvm_arch arch;    // x86-specific                    │    │ |
|  │  │      refcount_t users_count;                                     │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Key field: struct kvm_arch (arch/x86/include/asm/kvm_host.h)            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct kvm_arch {                                               │    │ |
|  │  │      unsigned long n_used_mmu_pages;                             │    │ |
|  │  │      unsigned long n_max_mmu_pages;                              │    │ |
|  │  │      struct hlist_head mmu_page_hash[KVM_NUM_MMU_PAGES];         │    │ |
|  │  │      struct list_head active_mmu_pages;                          │    │ |
|  │  │                                                                  │    │ |
|  │  │      struct kvm_pic *vpic;          // Virtual PIC               │    │ |
|  │  │      struct kvm_ioapic *vioapic;    // Virtual IOAPIC            │    │ |
|  │  │      struct kvm_pit *vpit;          // Virtual PIT               │    │ |
|  │  │                                                                  │    │ |
|  │  │      u64 tsc_offset;                // TSC offset for VM         │    │ |
|  │  │      u64 last_tsc_nsec;                                          │    │ |
|  │  │                                                                  │    │ |
|  │  │      struct kvm_apic_map __rcu *apic_map;                        │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  KVM LIFECYCLE                                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  CREATION (ioctl KVM_CREATE_VM):                                 │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  kvm_dev_ioctl_create_vm()  [virt/kvm/kvm_main.c]           │ │    │ |
|  │  │  │      │                                                      │ │    │ |
|  │  │  │      ▼                                                      │ │    │ |
|  │  │  │  kvm_create_vm()                                            │ │    │ |
|  │  │  │      ├── kzalloc(sizeof(struct kvm))                        │ │    │ |
|  │  │  │      ├── kvm_arch_init_vm()      // x86 specific setup      │ │    │ |
|  │  │  │      │       └── Setup virtual PIC, PIT, IOAPIC             │ │    │ |
|  │  │  │      ├── hardware_enable_all()   // Enable VMX on all CPUs  │ │    │ |
|  │  │  │      └── Returns VM fd                                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  MEMORY CONFIGURATION (ioctl KVM_SET_USER_MEMORY_REGION):        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  kvm_vm_ioctl_set_memory_region()                           │ │    │ |
|  │  │  │      │                                                      │ │    │ |
|  │  │  │      ▼                                                      │ │    │ |
|  │  │  │  __kvm_set_memory_region()                                  │ │    │ |
|  │  │  │      ├── Validate region (no overlap, aligned)              │ │    │ |
|  │  │  │      ├── Allocate kvm_memory_slot                           │ │    │ |
|  │  │  │      ├── Setup EPT mappings                                 │ │    │ |
|  │  │  │      └── Store in memslots array                            │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  DESTRUCTION (close VM fd):                                      │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  kvm_destroy_vm()                                           │ │    │ |
|  │  │  │      ├── Stop all vCPUs                                     │ │    │ |
|  │  │  │      ├── kvm_arch_destroy_vm()    // Free virtual devices   │ │    │ |
|  │  │  │      ├── Free all memory slots                              │ │    │ |
|  │  │  │      ├── Free EPT tables                                    │ │    │ |
|  │  │  │      └── kfree(kvm)                                         │ │    │ |
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

**架构锚点：struct kvm**

**位置**：`include/linux/kvm_host.h`

**关键字段**：
- `mmu_lock`：MMU/EPT 修改锁
- `memslots[]`：Guest 物理 → Host 虚拟映射
- `vcpus[]`：vCPU 数组
- `buses[]`：I/O 总线处理器
- `irq_routing`：中断路由表
- `arch`：x86 特定（vpic, vioapic, vpit, tsc_offset）

**生命周期**：

**创建（KVM_CREATE_VM）**：
1. `kvm_create_vm()` 分配 struct kvm
2. `kvm_arch_init_vm()` 设置虚拟 PIC、PIT、IOAPIC
3. `hardware_enable_all()` 在所有 CPU 上启用 VMX

**内存配置（KVM_SET_USER_MEMORY_REGION）**：
1. 验证区域（无重叠、对齐）
2. 分配 kvm_memory_slot
3. 设置 EPT 映射

**销毁（关闭 VM fd）**：
1. 停止所有 vCPU
2. `kvm_arch_destroy_vm()` 释放虚拟设备
3. 释放内存槽和 EPT 表

---

## 3. 控制中心：kvm_run()

```
CONTROL HUB: KVM_RUN()
+=============================================================================+
|                                                                              |
|  LOCATION: virt/kvm/kvm_main.c + arch/x86/kvm/x86.c                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Call chain for KVM_RUN:                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  User: ioctl(vcpu_fd, KVM_RUN, NULL)                             │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  kvm_vcpu_ioctl()      [virt/kvm/kvm_main.c]                     │    │ |
|  │  │              │                                                   │    │ |
|  │  │              │  case KVM_RUN:                                    │    │ |
|  │  │              │      return kvm_arch_vcpu_ioctl_run();            │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  kvm_arch_vcpu_ioctl_run()  [arch/x86/kvm/x86.c]                 │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ├── sigprocmask()  // Block signals during run      │    │ |
|  │  │              ├── vcpu_load()    // Associate vCPU with pCPU      │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  vcpu_run()                                                      │    │ |
|  │  │              │                                                   │    │ |
|  │  │              │  for (;;) {                                       │    │ |
|  │  │              │      if (signal_pending)                          │    │ |
|  │  │              │          break;                                   │    │ |
|  │  │              │                                                   │    │ |
|  │  │              │      if (vcpu->arch.mp_state == RUNNABLE)         │    │ |
|  │  │              │          r = vcpu_enter_guest();                  │    │ |
|  │  │              │      else                                         │    │ |
|  │  │              │          kvm_vcpu_block();  // HLT, wait for IRQ  │    │ |
|  │  │              │                                                   │    │ |
|  │  │              │      if (r == KVM_EXIT_*)                         │    │ |
|  │  │              │          break;  // Exit to userspace             │    │ |
|  │  │              │  }                                                │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  vcpu_enter_guest()    [arch/x86/kvm/x86.c]                      │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ├── inject_pending_event()  // Inject IRQs          │    │ |
|  │  │              ├── kvm_mmu_reload()        // Sync page tables     │    │ |
|  │  │              ├── preempt_disable()                               │    │ |
|  │  │              │                                                   │    │ |
|  │  │              │  guest_enter_irqoff();                            │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  kvm_x86_ops->run()    // VMX or SVM specific                    │    │ |
|  │  │              │                                                   │    │ |
|  │  │              │  For Intel: vmx_vcpu_run()  [arch/x86/kvm/vmx/]   │    │ |
|  │  │              │  For AMD:   svm_vcpu_run()  [arch/x86/kvm/svm/]   │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  ========= VMLAUNCH/VMRESUME (Guest runs) =========              │    │ |
|  │  │                                                                  │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  ========= VM EXIT =========                                     │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  vmx_handle_exit() / svm_handle_exit()                           │    │ |
|  │  │              │                                                   │    │ |
|  │  │              │  Read exit_reason from VMCS/VMCB                  │    │ |
|  │  │              │  Dispatch to handler                              │    │ |
|  │  │              │                                                   │    │ |
|  │  │              └── Return 0 (stay in kernel loop)                  │    │ |
|  │  │                  OR return 1 (exit to userspace)                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  OTHER CONTROL HUBS                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Memory Management:                                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  kvm_mmu_page_fault()   [arch/x86/kvm/mmu/mmu.c]                │    │ |
|  │  │      • Handle EPT violations                                    │    │ |
|  │  │      • Install guest page mappings                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  kvm_tdp_page_fault()   [arch/x86/kvm/mmu/tdp_mmu.c]            │    │ |
|  │  │      • Two-dimensional paging (EPT/NPT)                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Interrupt Handling:                                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  kvm_set_irq()          [virt/kvm/irqchip.c]                    │    │ |
|  │  │      • Route interrupt to correct vCPU                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  kvm_apic_set_irq()     [arch/x86/kvm/lapic.c]                  │    │ |
|  │  │      • Inject into virtual LAPIC                                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  I/O Handling:                                                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  kvm_io_bus_read/write() [virt/kvm/kvm_main.c]                  │    │ |
|  │  │      • In-kernel I/O device dispatch                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  vcpu_mmio_read/write()  [arch/x86/kvm/x86.c]                   │    │ |
|  │  │      • MMIO emulation                                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**控制中心：kvm_run()**

**KVM_RUN 调用链**：
1. 用户调用 `ioctl(vcpu_fd, KVM_RUN, NULL)`
2. `kvm_vcpu_ioctl()` → `kvm_arch_vcpu_ioctl_run()`
3. `vcpu_run()` 主循环：
   - 检查信号
   - 如果可运行则 `vcpu_enter_guest()`，否则阻塞
   - 如果需要退出到用户空间则 break

4. `vcpu_enter_guest()`：
   - 注入待处理中断
   - 同步页表
   - 调用 `kvm_x86_ops->run()`（vmx_vcpu_run 或 svm_vcpu_run）

5. **VMLAUNCH/VMRESUME**：Guest 运行

6. **VM EXIT**：
   - `vmx_handle_exit()` 读取退出原因
   - 分发到处理器
   - 返回 0（留在内核循环）或 1（退出到用户空间）

**其他控制中心**：
- **内存管理**：`kvm_mmu_page_fault()`、`kvm_tdp_page_fault()`
- **中断处理**：`kvm_set_irq()`、`kvm_apic_set_irq()`
- **I/O 处理**：`kvm_io_bus_read/write()`、`vcpu_mmio_read/write()`

---

## 4. 阅读策略

```
READING STRATEGY
+=============================================================================+
|                                                                              |
|  RECOMMENDED READING ORDER                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  LEVEL 1: USER API                                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. include/uapi/linux/kvm.h                                     │    │ |
|  │  │     • All ioctl definitions                                      │    │ |
|  │  │     • struct kvm_run (shared with userspace)                     │    │ |
|  │  │     • Understand QEMU-KVM interface                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. Documentation/virt/kvm/api.rst                               │    │ |
|  │  │     • Comprehensive API documentation                            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LEVEL 2: CORE FRAMEWORK                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  3. include/linux/kvm_host.h                                     │    │ |
|  │  │     • struct kvm, struct kvm_vcpu                                │    │ |
|  │  │     • Core data structures                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. virt/kvm/kvm_main.c                                          │    │ |
|  │  │     • /dev/kvm device                                            │    │ |
|  │  │     • ioctl dispatch                                             │    │ |
|  │  │     • kvm_create_vm(), kvm_vcpu_ioctl()                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LEVEL 3: X86 IMPLEMENTATION                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  5. arch/x86/kvm/x86.c                                           │    │ |
|  │  │     • kvm_arch_vcpu_ioctl_run()                                  │    │ |
|  │  │     • vcpu_enter_guest()                                         │    │ |
|  │  │     • x86 instruction emulation dispatch                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  6. arch/x86/include/asm/kvm_host.h                              │    │ |
|  │  │     • kvm_vcpu_arch, kvm_arch                                    │    │ |
|  │  │     • x86-specific structures                                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LEVEL 4: VMX DEEP DIVE                                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  7. arch/x86/kvm/vmx/vmx.c                                       │    │ |
|  │  │     • vmx_vcpu_run()                                             │    │ |
|  │  │     • vmx_handle_exit()                                          │    │ |
|  │  │     • Exit reason handlers                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  8. arch/x86/kvm/vmx/vmenter.S                                   │    │ |
|  │  │     • VM entry/exit assembly                                     │    │ |
|  │  │     • Context save/restore                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  9. arch/x86/include/asm/vmx.h                                   │    │ |
|  │  │     • VMCS field definitions                                     │    │ |
|  │  │     • Exit reason codes                                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LEVEL 5: SUBSYSTEMS                                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  10. arch/x86/kvm/mmu/mmu.c                                      │    │ |
|  │  │      • EPT/shadow page table management                          │    │ |
|  │  │      • kvm_mmu_page_fault()                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  11. arch/x86/kvm/lapic.c                                        │    │ |
|  │  │      • Virtual LAPIC emulation                                   │    │ |
|  │  │      • Interrupt delivery                                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  12. virt/kvm/eventfd.c                                          │    │ |
|  │  │      • ioeventfd/irqfd                                           │    │ |
|  │  │      • Fast I/O notification                                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**推荐阅读顺序**：

**第 1 层：用户 API**
1. `include/uapi/linux/kvm.h`：ioctl 定义、struct kvm_run
2. `Documentation/virt/kvm/api.rst`：API 文档

**第 2 层：核心框架**
3. `include/linux/kvm_host.h`：struct kvm、struct kvm_vcpu
4. `virt/kvm/kvm_main.c`：/dev/kvm、ioctl 分发

**第 3 层：x86 实现**
5. `arch/x86/kvm/x86.c`：kvm_arch_vcpu_ioctl_run()、vcpu_enter_guest()
6. `arch/x86/include/asm/kvm_host.h`：x86 特定结构

**第 4 层：VMX 深入**
7. `arch/x86/kvm/vmx/vmx.c`：vmx_vcpu_run()、vmx_handle_exit()
8. `arch/x86/kvm/vmx/vmenter.S`：VM entry/exit 汇编
9. `arch/x86/include/asm/vmx.h`：VMCS 字段、退出原因

**第 5 层：子系统**
10. `arch/x86/kvm/mmu/mmu.c`：EPT 管理
11. `arch/x86/kvm/lapic.c`：虚拟 LAPIC
12. `virt/kvm/eventfd.c`：ioeventfd/irqfd

---

## 5. 验证方法

```
VALIDATION APPROACH
+=============================================================================+
|                                                                              |
|  METHOD 1: KVM STATISTICS                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # View VM exit statistics                                               │ |
|  │  cat /sys/kernel/debug/kvm/*/vcpu*/kvm_exit                              │ |
|  │                                                                          │ |
|  │  # Example output:                                                       │ |
|  │  EXIT_REASON_IO_INSTRUCTION: 12345                                       │ |
|  │  EXIT_REASON_EPT_VIOLATION: 6789                                         │ |
|  │  EXIT_REASON_EXTERNAL_INTERRUPT: 45678                                   │ |
|  │                                                                          │ |
|  │  # Overall KVM stats                                                     │ |
|  │  cat /sys/kernel/debug/kvm/*                                             │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 2: QEMU MONITOR                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Connect to QEMU monitor                                               │ |
|  │  (qemu) info kvm                                                         │ |
|  │  kvm support: enabled                                                    │ |
|  │                                                                          │ |
|  │  (qemu) info registers                                                   │ |
|  │  # Shows vCPU register state                                             │ |
|  │                                                                          │ |
|  │  (qemu) info cpus                                                        │ |
|  │  # Shows vCPU status                                                     │ |
|  │                                                                          │ |
|  │  (qemu) x /10i $rip                                                      │ |
|  │  # Disassemble at guest RIP                                              │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 3: PERF KVM                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Profile VM exits                                                      │ |
|  │  perf kvm stat live                                                      │ |
|  │                                                                          │ |
|  │  # Record and analyze                                                    │ |
|  │  perf kvm record -a sleep 10                                             │ |
|  │  perf kvm stat report                                                    │ |
|  │                                                                          │ |
|  │  # Sample output:                                                        │ |
|  │  Event           Samples  Time%                                          │ |
|  │  EXTERNAL_INT     45000   45.2%                                          │ |
|  │  IO_INSTRUCTION   30000   30.1%                                          │ |
|  │  EPT_VIOLATION    15000   15.0%                                          │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 4: FTRACE                                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Trace KVM events                                                      │ |
|  │  echo 'kvm:*' > /sys/kernel/debug/tracing/set_event                      │ |
|  │  echo 1 > /sys/kernel/debug/tracing/tracing_on                           │ |
|  │                                                                          │ |
|  │  # Run VM, then read trace                                               │ |
|  │  cat /sys/kernel/debug/tracing/trace                                     │ |
|  │                                                                          │ |
|  │  # Sample output:                                                        │ |
|  │  kvm_entry: vcpu 0                                                       │ |
|  │  kvm_exit: reason IO_INSTRUCTION info1 0x3f8 info2 0x0                   │ |
|  │  kvm_emulate_insn: 0:e6 f8 (real)                                        │ |
|  │  kvm_entry: vcpu 0                                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 5: SIMPLE KVM PROGRAM                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Write a minimal KVM program to understand the API:                      │ |
|  │  ┌────────────────────────────────────────────────────────────────────┐ │ |
|  │  │                                                                     │ │ |
|  │  │  // Minimal KVM example                                             │ │ |
|  │  │  int kvm = open("/dev/kvm", O_RDWR);                                │ │ |
|  │  │  int vmfd = ioctl(kvm, KVM_CREATE_VM, 0);                           │ │ |
|  │  │  int vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, 0);                      │ │ |
|  │  │                                                                     │ │ |
|  │  │  // Map guest memory                                                │ │ |
|  │  │  void *mem = mmap(NULL, 4096, ...);                                 │ │ |
|  │  │  memcpy(mem, guest_code, sizeof(guest_code));                       │ │ |
|  │  │                                                                     │ │ |
|  │  │  // Set memory region                                               │ │ |
|  │  │  struct kvm_userspace_memory_region region = {...};                 │ │ |
|  │  │  ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);                  │ │ |
|  │  │                                                                     │ │ |
|  │  │  // Run                                                             │ │ |
|  │  │  struct kvm_run *run = mmap(..., vcpufd, 0);                        │ │ |
|  │  │  while (1) {                                                        │ │ |
|  │  │      ioctl(vcpufd, KVM_RUN, NULL);                                  │ │ |
|  │  │      switch (run->exit_reason) {                                    │ │ |
|  │  │          case KVM_EXIT_IO: ...                                      │ │ |
|  │  │          case KVM_EXIT_HLT: return;                                 │ │ |
|  │  │      }                                                              │ │ |
|  │  │  }                                                                  │ │ |
|  │  │                                                                     │ │ |
|  │  └────────────────────────────────────────────────────────────────────┘ │ |
|  │                                                                          │ |
|  │  See: tools/testing/selftests/kvm/ for examples                          │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**验证方法**：

**方法 1：KVM 统计**
- `/sys/kernel/debug/kvm/*/vcpu*/kvm_exit`：VM exit 统计

**方法 2：QEMU 监视器**
- `info kvm`：KVM 支持状态
- `info registers`：vCPU 寄存器状态
- `x /10i $rip`：在 guest RIP 处反汇编

**方法 3：perf kvm**
- `perf kvm stat live`：实时 VM exit 分析
- `perf kvm record/stat report`：记录并分析

**方法 4：ftrace**
- `echo 'kvm:*' > set_event`：跟踪 KVM 事件
- 看到 kvm_entry、kvm_exit、kvm_emulate_insn

**方法 5：简单 KVM 程序**
- 写一个最小 KVM 程序理解 API
- 参考：`tools/testing/selftests/kvm/`
