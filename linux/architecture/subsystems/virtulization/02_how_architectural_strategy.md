# HOW｜架构策略

## 1. Guest/Host 分离

```
GUEST/HOST SEPARATION
+=============================================================================+
|                                                                              |
|  THE FUNDAMENTAL SPLIT                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │                    PRIVILEGE RING MODEL                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  Traditional OS:                 With VMX:                       │    │ |
|  │  │  ┌──────────────┐               ┌──────────────────────────┐    │    │ |
|  │  │  │   Ring 3     │               │      VMX NON-ROOT        │    │    │ |
|  │  │  │  User apps   │               │  ┌──────────────────┐    │    │    │ |
|  │  │  ├──────────────┤               │  │ Ring 3: Guest App│    │    │    │ |
|  │  │  │   Ring 0     │               │  ├──────────────────┤    │    │    │ |
|  │  │  │   Kernel     │               │  │ Ring 0: Guest OS │    │    │    │ |
|  │  │  └──────────────┘               │  └──────────────────┘    │    │    │ |
|  │  │                                  └────────────┬─────────────┘    │    │ |
|  │  │                                               │                  │    │ |
|  │  │                                     VM Exit   │   VM Entry       │    │ |
|  │  │                                               ▼                  │    │ |
|  │  │                                  ┌──────────────────────────┐    │    │ |
|  │  │                                  │       VMX ROOT           │    │    │ |
|  │  │                                  │  ┌──────────────────┐    │    │    │ |
|  │  │                                  │  │ Ring 3: QEMU     │    │    │    │ |
|  │  │                                  │  ├──────────────────┤    │    │    │ |
|  │  │                                  │  │ Ring 0: KVM/Linux│    │    │    │ |
|  │  │                                  │  └──────────────────┘    │    │    │ |
|  │  │                                  └──────────────────────────┘    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  KEY INSIGHT:                                                            │ |
|  │  • Guest runs at Ring 0 but in "non-root" mode                           │ |
|  │  • Guest THINKS it has full control                                      │ |
|  │  • Hardware TRAPS sensitive operations to host                           │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  KVM ARCHITECTURE: KERNEL AS HYPERVISOR                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  User Space:                                                     │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ┌─────────────────────────────────────────────────────┐   │ │    │ |
|  │  │  │  │                      QEMU                            │   │ │    │ |
|  │  │  │  │                                                      │   │ │    │ |
|  │  │  │  │  Responsibilities:                                   │   │ │    │ |
|  │  │  │  │  • Device emulation (disk, network, USB, ...)        │   │ │    │ |
|  │  │  │  │  • VM configuration                                  │   │ │    │ |
|  │  │  │  │  • Display output                                    │   │ │    │ |
|  │  │  │  │  • Migration support                                 │   │ │    │ |
|  │  │  │  │                                                      │   │ │    │ |
|  │  │  │  │  Uses /dev/kvm:                                      │   │ │    │ |
|  │  │  │  │  • ioctl(KVM_CREATE_VM)                              │   │ │    │ |
|  │  │  │  │  • ioctl(KVM_CREATE_VCPU)                            │   │ │    │ |
|  │  │  │  │  • ioctl(KVM_RUN)                                    │   │ │    │ |
|  │  │  │  │                                                      │   │ │    │ |
|  │  │  │  └─────────────────────────────────────────────────────┘   │ │    │ |
|  │  │  │                              │                              │ │    │ |
|  │  │  └──────────────────────────────┼──────────────────────────────┘ │    │ |
|  │  │                                 │ /dev/kvm                       │    │ |
|  │  │  ════════════════════════════════════════════════════════════   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Kernel Space:                                                   │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ┌─────────────────────────────────────────────────────┐   │ │    │ |
|  │  │  │  │                      KVM                             │   │ │    │ |
|  │  │  │  │                                                      │   │ │    │ |
|  │  │  │  │  Responsibilities:                                   │   │ │    │ |
|  │  │  │  │  • CPU virtualization (VMCS management)              │   │ │    │ |
|  │  │  │  │  • Memory virtualization (EPT/shadow PT)             │   │ │    │ |
|  │  │  │  │  • Interrupt injection                               │   │ │    │ |
|  │  │  │  │  • VM exit handling                                  │   │ │    │ |
|  │  │  │  │                                                      │   │ │    │ |
|  │  │  │  └─────────────────────────────────────────────────────┘   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ┌─────────────────────────────────────────────────────┐   │ │    │ |
|  │  │  │  │                  Linux Kernel                        │   │ │    │ |
|  │  │  │  │                                                      │   │ │    │ |
|  │  │  │  │  • Scheduling (vCPUs are threads)                    │   │ │    │ |
|  │  │  │  │  • Memory management (guest RAM = mmap'd memory)     │   │ │    │ |
|  │  │  │  │  • I/O (vhost, disk I/O)                             │   │ │    │ |
|  │  │  │  │                                                      │   │ │    │ |
|  │  │  │  └─────────────────────────────────────────────────────┘   │ │    │ |
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

**Guest/Host 分离**：

**基本分割**：
- 传统 OS：Ring 3（用户应用）、Ring 0（内核）
- 有 VMX：
  - VMX NON-ROOT：Guest 运行（Ring 3 Guest App + Ring 0 Guest OS）
  - VMX ROOT：Host 运行（Ring 3 QEMU + Ring 0 KVM/Linux）
  - VM Exit/Entry 在两者之间切换

**关键洞见**：
- Guest 在 Ring 0 运行但在"non-root"模式
- Guest 认为它有完全控制权
- 硬件将敏感操作捕获到 host

**KVM 架构：内核作为 Hypervisor**

**用户空间 (QEMU)**：
- 设备模拟（磁盘、网络、USB）
- VM 配置、显示输出、迁移支持
- 使用 /dev/kvm：KVM_CREATE_VM、KVM_CREATE_VCPU、KVM_RUN

**内核空间 (KVM + Linux)**：
- KVM：CPU 虚拟化（VMCS 管理）、内存虚拟化（EPT）、中断注入、VM exit 处理
- Linux：调度（vCPU 是线程）、内存管理（guest RAM = mmap 内存）、I/O

---

## 2. VM Exit 处理

```
VM EXIT HANDLING
+=============================================================================+
|                                                                              |
|  WHAT CAUSES VM EXITS                                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  UNCONDITIONAL EXITS (always exit):                              │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  • CPUID            - Guest queries CPU features            │ │    │ |
|  │  │  │  • INVD             - Invalidate caches                     │ │    │ |
|  │  │  │  • VMCALL           - Hypercall to hypervisor               │ │    │ |
|  │  │  │  • Triple fault     - Unrecoverable guest error             │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  CONDITIONAL EXITS (configurable via VMCS):                      │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  • I/O instructions (IN, OUT)                               │ │    │ |
|  │  │  │  • MSR access (RDMSR, WRMSR)                                │ │    │ |
|  │  │  │  • CR access (MOV to/from CR0, CR3, CR4)                    │ │    │ |
|  │  │  │  • Interrupt window                                         │ │    │ |
|  │  │  │  • HLT instruction                                          │ │    │ |
|  │  │  │  • EPT violation (page fault in guest physical)             │ │    │ |
|  │  │  │  • External interrupts                                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  VM EXIT HANDLING FLOW                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  [Guest Running in VMX non-root]                                 │    │ |
|  │  │          │                                                       │    │ |
|  │  │          │  Guest executes: outb(0x3f8, 'A')  // Serial port     │    │ |
|  │  │          │                                                       │    │ |
|  │  │          ▼                                                       │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────┐    │    │ |
|  │  │  │                    VM EXIT                               │    │    │ |
|  │  │  │                                                          │    │    │ |
|  │  │  │  1. Hardware saves guest state to VMCS                   │    │    │ |
|  │  │  │     • Guest RIP, RSP, RFLAGS                             │    │    │ |
|  │  │  │     • Guest CR3, segment registers                       │    │    │ |
|  │  │  │                                                          │    │    │ |
|  │  │  │  2. Hardware loads host state from VMCS                  │    │    │ |
|  │  │  │     • Host RIP → vmx_vmexit_handler                      │    │    │ |
|  │  │  │     • Host RSP, CR3, segments                            │    │    │ |
|  │  │  │                                                          │    │    │ |
|  │  │  │  3. Exit reason stored in VMCS                           │    │    │ |
|  │  │  │     • EXIT_REASON_IO_INSTRUCTION                         │    │    │ |
|  │  │  │     • Exit qualification: port 0x3f8, direction out      │    │    │ |
|  │  │  │                                                          │    │    │ |
|  │  │  └──────────────────────────┬───────────────────────────────┘    │    │ |
|  │  │                              │                                   │    │ |
|  │  │                              ▼                                   │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────┐    │    │ |
|  │  │  │               vmx_handle_exit() [KVM]                    │    │    │ |
|  │  │  │                                                          │    │    │ |
|  │  │  │  switch (exit_reason) {                                  │    │    │ |
|  │  │  │      case EXIT_REASON_IO_INSTRUCTION:                    │    │    │ |
|  │  │  │          handle_io(vcpu);  // Decode, emulate or exit    │    │    │ |
|  │  │  │          break;                                          │    │    │ |
|  │  │  │      case EXIT_REASON_EPT_VIOLATION:                     │    │    │ |
|  │  │  │          handle_ept_violation(vcpu);                     │    │    │ |
|  │  │  │          break;                                          │    │    │ |
|  │  │  │      case EXIT_REASON_EXTERNAL_INTERRUPT:                │    │    │ |
|  │  │  │          // Let Linux handle the interrupt               │    │    │ |
|  │  │  │          break;                                          │    │    │ |
|  │  │  │      ...                                                 │    │    │ |
|  │  │  │  }                                                       │    │    │ |
|  │  │  │                                                          │    │    │ |
|  │  │  └──────────────────────────┬───────────────────────────────┘    │    │ |
|  │  │                              │                                   │    │ |
|  │  │           ┌──────────────────┴──────────────────┐                │    │ |
|  │  │           │                                     │                │    │ |
|  │  │           ▼                                     ▼                │    │ |
|  │  │  ┌─────────────────────┐            ┌─────────────────────┐     │    │ |
|  │  │  │  KVM handles in-    │            │  Return to QEMU     │     │    │ |
|  │  │  │  kernel (fast path) │            │  (exit to userspace)│     │    │ |
|  │  │  │                     │            │                     │     │    │ |
|  │  │  │  • EPT violation    │            │  • Unhandled I/O    │     │    │ |
|  │  │  │  • Interrupt inject │            │  • MMIO             │     │    │ |
|  │  │  │  • HLT (schedule)   │            │  • Halt (no work)   │     │    │ |
|  │  │  │                     │            │                     │     │    │ |
|  │  │  │  VMRESUME →         │            │  vcpu->run->        │     │    │ |
|  │  │  │  back to guest      │            │    exit_reason =    │     │    │ |
|  │  │  │                     │            │    KVM_EXIT_IO      │     │    │ |
|  │  │  └─────────────────────┘            └──────────┬──────────┘     │    │ |
|  │  │                                                 │                │    │ |
|  │  │                                                 ▼                │    │ |
|  │  │                                     ┌─────────────────────┐     │    │ |
|  │  │                                     │  QEMU handles       │     │    │ |
|  │  │                                     │                     │     │    │ |
|  │  │                                     │  Emulate serial port│     │    │ |
|  │  │                                     │  Output 'A' to pty  │     │    │ |
|  │  │                                     │                     │     │    │ |
|  │  │                                     │  ioctl(KVM_RUN) →   │     │    │ |
|  │  │                                     │  resume guest       │     │    │ |
|  │  │                                     └─────────────────────┘     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**VM Exit 处理**：

**什么导致 VM Exits**：

**无条件退出**（总是退出）：
- CPUID（查询 CPU 特性）
- INVD（使缓存无效）
- VMCALL（到 hypervisor 的 hypercall）
- Triple fault（不可恢复的 guest 错误）

**条件退出**（通过 VMCS 配置）：
- I/O 指令（IN, OUT）
- MSR 访问
- CR 访问
- 中断窗口、HLT 指令
- EPT 违规（guest 物理地址页错误）
- 外部中断

**VM Exit 处理流程**：

1. **硬件保存 guest 状态到 VMCS**：RIP, RSP, RFLAGS, CR3
2. **硬件从 VMCS 加载 host 状态**：Host RIP → vmx_vmexit_handler
3. **退出原因存储在 VMCS**：EXIT_REASON_IO_INSTRUCTION

4. **vmx_handle_exit() [KVM]**：
   - switch (exit_reason) 分发到对应处理器

5. **处理路径**：
   - **快路径（KVM 内核处理）**：EPT 违规、中断注入、HLT → VMRESUME 返回 guest
   - **慢路径（返回 QEMU）**：未处理 I/O、MMIO → QEMU 模拟 → ioctl(KVM_RUN) 恢复

---

## 3. VM 和 vCPU 的生命周期

```
LIFECYCLE OF VMs AND vCPUs
+=============================================================================+
|                                                                              |
|  VM CREATION                                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  QEMU startup:                                                   │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  1. Open /dev/kvm                                           │ │    │ |
|  │  │  │     fd = open("/dev/kvm", O_RDWR);                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  2. Check KVM version                                       │ │    │ |
|  │  │  │     ioctl(fd, KVM_GET_API_VERSION);                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  3. Create VM                                               │ │    │ |
|  │  │  │     vm_fd = ioctl(fd, KVM_CREATE_VM, 0);                    │ │    │ |
|  │  │  │     // Returns fd for the VM                                │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  4. Configure VM memory                                     │ │    │ |
|  │  │  │     guest_mem = mmap(NULL, size, PROT_READ|PROT_WRITE,      │ │    │ |
|  │  │  │                      MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);     │ │    │ |
|  │  │  │     struct kvm_userspace_memory_region region = {           │ │    │ |
|  │  │  │         .slot = 0,                                          │ │    │ |
|  │  │  │         .guest_phys_addr = 0,                               │ │    │ |
|  │  │  │         .memory_size = size,                                │ │    │ |
|  │  │  │         .userspace_addr = (uint64_t)guest_mem,              │ │    │ |
|  │  │  │     };                                                      │ │    │ |
|  │  │  │     ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &region);      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  In kernel (kvm_create_vm):                                      │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  • Allocate struct kvm                                      │ │    │ |
|  │  │  │  • Initialize memory slot array                             │ │    │ |
|  │  │  │  • Setup EPT root (if EPT enabled)                          │ │    │ |
|  │  │  │  • Create IRQ routing tables                                │ │    │ |
|  │  │  │  • Initialize ioeventfd/irqfd lists                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  vCPU CREATION AND EXECUTION                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  QEMU creates vCPU:                                              │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  5. Create vCPU                                             │ │    │ |
|  │  │  │     vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, cpu_id);        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  6. Map kvm_run structure                                   │ │    │ |
|  │  │  │     kvm_run = mmap(NULL, size, PROT_READ|PROT_WRITE,        │ │    │ |
|  │  │  │                    MAP_SHARED, vcpu_fd, 0);                 │ │    │ |
|  │  │  │     // Shared between QEMU and kernel                       │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  7. Set initial CPU state                                   │ │    │ |
|  │  │  │     struct kvm_regs regs = { .rip = 0xfff0, ... };          │ │    │ |
|  │  │  │     ioctl(vcpu_fd, KVM_SET_REGS, &regs);                    │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  8. Start vCPU thread                                       │ │    │ |
|  │  │  │     pthread_create(&vcpu_thread, NULL, vcpu_thread_fn, ...);│ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  In kernel (kvm_vcpu_create):                                    │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  • Allocate struct kvm_vcpu                                 │ │    │ |
|  │  │  │  • Allocate VMCS (one per vCPU)                             │ │    │ |
|  │  │  │  • Initialize VMCS fields                                   │ │    │ |
|  │  │  │  • Setup MSR bitmap                                         │ │    │ |
|  │  │  │  • Allocate kvm_run page for userspace                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  vCPU EXECUTION LOOP                                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  void *vcpu_thread_fn(void *arg) {                               │    │ |
|  │  │      while (running) {                                           │    │ |
|  │  │          ┌───────────────────────────────────────────────────┐  │    │ |
|  │  │          │                                                    │  │    │ |
|  │  │          │  // Enter guest                                    │  │    │ |
|  │  │          │  ret = ioctl(vcpu_fd, KVM_RUN, NULL);              │  │    │ |
|  │  │          │                                                    │  │    │ |
|  │  │          │  // Guest ran until VM exit                        │  │    │ |
|  │  │          │  // kvm_run now contains exit info                 │  │    │ |
|  │  │          │                                                    │  │    │ |
|  │  │          └───────────────────────────────────────────────────┘  │    │ |
|  │  │                               │                                  │    │ |
|  │  │                               ▼                                  │    │ |
|  │  │          switch (kvm_run->exit_reason) {                         │    │ |
|  │  │          ┌───────────────────────────────────────────────────┐  │    │ |
|  │  │          │                                                    │  │    │ |
|  │  │          │  case KVM_EXIT_IO:                                 │  │    │ |
|  │  │          │      handle_io(kvm_run);  // Emulate I/O           │  │    │ |
|  │  │          │      break;                                        │  │    │ |
|  │  │          │                                                    │  │    │ |
|  │  │          │  case KVM_EXIT_MMIO:                               │  │    │ |
|  │  │          │      handle_mmio(kvm_run); // Emulate MMIO         │  │    │ |
|  │  │          │      break;                                        │  │    │ |
|  │  │          │                                                    │  │    │ |
|  │  │          │  case KVM_EXIT_SHUTDOWN:                           │  │    │ |
|  │  │          │      running = false;      // Guest shutdown       │  │    │ |
|  │  │          │      break;                                        │  │    │ |
|  │  │          │                                                    │  │    │ |
|  │  │          │  case KVM_EXIT_HLT:                                │  │    │ |
|  │  │          │      wait_for_interrupt(); // Wait                 │  │    │ |
|  │  │          │      break;                                        │  │    │ |
|  │  │          │                                                    │  │    │ |
|  │  │          └───────────────────────────────────────────────────┘  │    │ |
|  │  │          }                                                       │    │ |
|  │  │      }                                                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  VM DESTRUCTION                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. Signal vCPU threads to stop                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. Wait for vCPU threads to exit                                │    │ |
|  │  │     pthread_join(vcpu_thread, NULL);                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. Close vCPU file descriptors                                  │    │ |
|  │  │     close(vcpu_fd);  // Releases struct kvm_vcpu, VMCS           │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. Close VM file descriptor                                     │    │ |
|  │  │     close(vm_fd);    // Releases struct kvm, EPT, memory slots   │    │ |
|  │  │                                                                  │    │ |
|  │  │  5. Unmap guest memory                                           │    │ |
|  │  │     munmap(guest_mem, size);                                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**VM 和 vCPU 的生命周期**：

**VM 创建**：

**QEMU 启动**：
1. 打开 `/dev/kvm`
2. 检查 KVM 版本
3. 创建 VM：`ioctl(fd, KVM_CREATE_VM, 0)`
4. 配置 VM 内存：mmap guest 内存，`KVM_SET_USER_MEMORY_REGION`

**内核（kvm_create_vm）**：
- 分配 `struct kvm`
- 初始化内存槽数组
- 设置 EPT 根
- 创建 IRQ 路由表

**vCPU 创建和执行**：

**QEMU 创建 vCPU**：
5. 创建 vCPU：`ioctl(vm_fd, KVM_CREATE_VCPU, cpu_id)`
6. 映射 kvm_run 结构
7. 设置初始 CPU 状态
8. 启动 vCPU 线程

**内核（kvm_vcpu_create）**：
- 分配 `struct kvm_vcpu`
- 分配 VMCS（每 vCPU 一个）
- 初始化 VMCS 字段

**vCPU 执行循环**：
```
while (running) {
    ioctl(vcpu_fd, KVM_RUN, NULL);  // 进入 guest
    switch (kvm_run->exit_reason) {
        case KVM_EXIT_IO: handle_io();
        case KVM_EXIT_MMIO: handle_mmio();
        case KVM_EXIT_SHUTDOWN: running = false;
        case KVM_EXIT_HLT: wait_for_interrupt();
    }
}
```

**VM 销毁**：
1. 信号 vCPU 线程停止
2. 等待 vCPU 线程退出
3. 关闭 vCPU 文件描述符
4. 关闭 VM 文件描述符
5. 取消映射 guest 内存
