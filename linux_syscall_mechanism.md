# Linux 系统调用实现机制及执行流程深入分析

基于 Linux 3.2 内核源码 (x86 架构)

---

## 目录

- [系统调用执行流程](#系统调用执行流程)
- [系统调用概述](#系统调用概述)
- [系统调用表结构](#系统调用表结构)
- [系统调用进入机制](#系统调用进入机制)
- [内核栈与寄存器保存](#内核栈与寄存器保存)
- [系统调用处理流程](#系统调用处理流程)
- [系统调用返回流程](#系统调用返回流程)
- [系统调用定义宏](#系统调用定义宏)
- [实例分析](#实例分析)
- [关键源码文件](#关键源码文件)

---

## 系统调用执行流程
```
用户空间                              内核空间
────────                              ────────
                                      
write(fd, buf, len)                   
    │                                 
    ▼                                 
libc 包装函数                          
    │                                 
    ├── eax = 4 (系统调用号)           
    ├── ebx = fd                      
    ├── ecx = buf                     
    ├── edx = len                     
    │                                 
    ▼                                 
int $0x80 或 sysenter                 
    │                                 
    ════════════════════════════════► 
                                      │
                                      ▼
                              system_call 入口
                                      │
                                      ▼
                              检查 eax < NR_syscalls
                                      │
                                      ▼
                              call *sys_call_table(, %eax, 4)
                              ─────────────────────────────
                                      │
                                      │  eax=4, 偏移=4*4=16字节
                                      │  指向 sys_write
                                      ▼
                              sys_write(fd, buf, len)
                                      │
                                      ▼
                              返回值放入 eax
    ◄════════════════════════════════ 
    │                                 
    ▼                                 
返回用户空间                           
```

## 系统调用概述

### 什么是系统调用

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              系统调用层次图                                   │
│                                                                              │
│   用户空间                                                                    │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  应用程序                                                            │   │
│   │     │                                                                │   │
│   │     ▼                                                                │   │
│   │  C 库函数 (glibc)                                                   │   │
│   │  write() → 设置寄存器 → 触发陷入                                     │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                              │                                               │
│   ══════════════════════════════════════════════════════════════════════════ │
│                              │  用户态 / 内核态 边界                         │
│                              │  (特权级切换: Ring 3 → Ring 0)               │
│                              ▼                                               │
│   内核空间                                                                    │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  系统调用入口 (entry_32.S / entry_64.S)                             │   │
│   │     │                                                                │   │
│   │     ▼                                                                │   │
│   │  系统调用表查找 (sys_call_table)                                    │   │
│   │     │                                                                │   │
│   │     ▼                                                                │   │
│   │  系统调用处理函数 (sys_write)                                       │   │
│   │     │                                                                │   │
│   │     ▼                                                                │   │
│   │  内核子系统 (VFS, 调度器, 内存管理...)                               │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 系统调用触发方式

| 架构 | 传统方式 | 快速方式 |
|------|---------|---------|
| x86-32 | `int $0x80` | `sysenter` (Intel) / `syscall` (AMD) |
| x86-64 | - | `syscall` |

---

## 系统调用表结构

### x86-32 系统调用表

```asm
// arch/x86/kernel/syscall_table_32.S

ENTRY(sys_call_table)
    .long sys_restart_syscall    /* 0 - 重启被中断的系统调用 */
    .long sys_exit               /* 1 - 进程退出 */
    .long ptregs_fork            /* 2 - 创建子进程 */
    .long sys_read               /* 3 - 读文件 */
    .long sys_write              /* 4 - 写文件 */
    .long sys_open               /* 5 - 打开文件 */
    .long sys_close              /* 6 - 关闭文件 */
    .long sys_waitpid            /* 7 - 等待子进程 */
    .long sys_creat              /* 8 - 创建文件 */
    .long sys_link               /* 9 - 创建硬链接 */
    .long sys_unlink             /* 10 - 删除文件 */
    ...
    /* 共约 350 个系统调用 */
```

### 表结构示意

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        sys_call_table 内存布局                               │
│                                                                              │
│   索引   偏移       内容 (函数指针)          对应系统调用                      │
│  ──────────────────────────────────────────────────────────────────────────  │
│    0     0x00    ┌──────────────────┐                                        │
│                  │ sys_restart_syscall │ ──► restart_syscall()              │
│    1     0x04    ├──────────────────┤                                        │
│                  │ sys_exit           │ ──► exit()                          │
│    2     0x08    ├──────────────────┤                                        │
│                  │ ptregs_fork        │ ──► fork()                          │
│    3     0x0C    ├──────────────────┤                                        │
│                  │ sys_read           │ ──► read()                          │
│    4     0x10    ├──────────────────┤                                        │
│                  │ sys_write          │ ──► write()                         │
│    5     0x14    ├──────────────────┤                                        │
│                  │ sys_open           │ ──► open()                          │
│          ...     │       ...          │                                      │
│                  └──────────────────┘                                        │
│                                                                              │
│   查找公式: sys_call_table + (syscall_nr × 4)                                │
│   例如 write (nr=4): sys_call_table + 4×4 = sys_call_table + 0x10            │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 处理函数命名规则

```c
// 1. 标准系统调用: sys_xxx
sys_read, sys_write, sys_open, sys_close ...

// 2. 需要完整寄存器上下文: ptregs_xxx
ptregs_fork      // fork 需要复制父进程寄存器
ptregs_clone     // clone 同上
ptregs_execve    // execve 需要设置新的寄存器
ptregs_sigreturn // 信号返回需要恢复寄存器

// 3. 未实现/已废弃: sys_ni_syscall
.long sys_ni_syscall  /* old break syscall holder */
// 返回 -ENOSYS
```

---

## 系统调用进入机制

### 方式一: int $0x80 (传统软中断)

```
用户空间调用 write(fd, buf, len):

    movl $4, %eax       # 系统调用号 (write = 4)
    movl fd, %ebx       # 第1个参数
    movl buf, %ecx      # 第2个参数
    movl len, %edx      # 第3个参数
    int $0x80           # 触发软中断
    
    # 返回值在 %eax 中
```

**中断处理流程：**
```
int $0x80
    │
    ├── 1. CPU 自动完成:
    │       ├── 从 TSS 加载内核栈指针 (SS:ESP)
    │       ├── 压入用户态 SS, ESP, EFLAGS, CS, EIP
    │       └── 从 IDT[0x80] 加载中断处理程序地址
    │
    └── 2. 跳转到 system_call 入口
```

### 方式二: sysenter (快速系统调用)

```
用户空间调用 (通过 vDSO):

    movl $4, %eax       # 系统调用号
    movl fd, %ebx       # 参数
    movl buf, %ecx
    movl len, %edx
    movl %esp, %ebp     # 保存用户栈指针
    sysenter            # 快速进入内核
```

**sysenter 特点：**
- 无需查 IDT，直接从 MSR 寄存器读取入口地址
- 不自动保存用户态寄存器，需要软件处理
- 比 int $0x80 快约 25%

### sysenter 入口处理

```asm
// arch/x86/kernel/entry_32.S

ENTRY(ia32_sysenter_target)
    # ESP 已经指向内核栈 (由 sysenter 指令设置)
    
    # 手动构建中断帧 (模拟 int $0x80 的栈)
    pushl $__USER_DS          # SS
    pushl %ebp                # 用户 ESP (之前保存在 EBP)
    pushfl                    # EFLAGS
    orl $X86_EFLAGS_IF, (%esp) # 确保中断开启
    pushl $__USER_CS          # CS
    pushl ((TI_sysenter_return)-THREAD_SIZE+8+4*4)(%esp) # 返回地址
    
    pushl %eax                # 保存系统调用号 (orig_eax)
    SAVE_ALL                  # 保存所有寄存器
    ENABLE_INTERRUPTS(CLBR_NONE)
    
    # 加载第6个参数 (从用户栈)
    cmpl $__PAGE_OFFSET-3, %ebp
    jae syscall_fault
    movl (%ebp), %ebp
    movl %ebp, PT_EBP(%esp)
    
    GET_THREAD_INFO(%ebp)
    
    # 检查是否需要跟踪
    testl $_TIF_WORK_SYSCALL_ENTRY, TI_flags(%ebp)
    jnz sysenter_audit
    
sysenter_do_call:
    cmpl $(nr_syscalls), %eax
    jae syscall_badsys
    call *sys_call_table(,%eax,4)   # 调用处理函数
    movl %eax, PT_EAX(%esp)         # 保存返回值
```

---

## 内核栈与寄存器保存

### 内核栈布局 (pt_regs 结构)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          内核栈帧布局                                        │
│                                                                              │
│   高地址                                                                     │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  0x40(%esp)  │  %oldss    │  用户态 SS (CPU 自动保存)              │   │
│   ├──────────────┼────────────┼────────────────────────────────────────┤   │
│   │  0x3C(%esp)  │  %oldesp   │  用户态 ESP (CPU 自动保存)             │   │
│   ├──────────────┼────────────┼────────────────────────────────────────┤   │
│   │  0x38(%esp)  │  %eflags   │  标志寄存器 (CPU 自动保存)             │   │
│   ├──────────────┼────────────┼────────────────────────────────────────┤   │
│   │  0x34(%esp)  │  %cs       │  代码段 (CPU 自动保存)                 │   │
│   ├──────────────┼────────────┼────────────────────────────────────────┤   │
│   │  0x30(%esp)  │  %eip      │  返回地址 (CPU 自动保存)               │   │
│   ├──────────────┼────────────┼────────────────────────────────────────┤   │
│   │  0x2C(%esp)  │  orig_eax  │  原始系统调用号                        │   │
│   ├──────────────┼────────────┼────────────────────────────────────────┤   │
│   │  0x28(%esp)  │  %gs       │  ┐                                     │   │
│   ├──────────────┼────────────┤  │                                     │   │
│   │  0x24(%esp)  │  %fs       │  │                                     │   │
│   ├──────────────┼────────────┤  │  SAVE_ALL 宏保存                    │   │
│   │  0x20(%esp)  │  %es       │  │                                     │   │
│   ├──────────────┼────────────┤  │                                     │   │
│   │  0x1C(%esp)  │  %ds       │  │                                     │   │
│   ├──────────────┼────────────┤  │                                     │   │
│   │  0x18(%esp)  │  %eax      │  │  (返回值写回这里)                   │   │
│   ├──────────────┼────────────┤  │                                     │   │
│   │  0x14(%esp)  │  %ebp      │  │  第6个参数                          │   │
│   ├──────────────┼────────────┤  │                                     │   │
│   │  0x10(%esp)  │  %edi      │  │  第5个参数                          │   │
│   ├──────────────┼────────────┤  │                                     │   │
│   │  0x0C(%esp)  │  %esi      │  │  第4个参数                          │   │
│   ├──────────────┼────────────┤  │                                     │   │
│   │  0x08(%esp)  │  %edx      │  │  第3个参数                          │   │
│   ├──────────────┼────────────┤  │                                     │   │
│   │  0x04(%esp)  │  %ecx      │  │  第2个参数                          │   │
│   ├──────────────┼────────────┤  │                                     │   │
│   │  0x00(%esp)  │  %ebx      │  ┘  第1个参数                          │   │
│   └──────────────┴────────────┴────────────────────────────────────────┘   │
│   低地址 (ESP)                                                              │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### SAVE_ALL 宏

```asm
// arch/x86/kernel/entry_32.S

.macro SAVE_ALL
    cld
    PUSH_GS                   # 保存 %gs
    pushl_cfi %fs
    pushl_cfi %es
    pushl_cfi %ds
    pushl_cfi %eax
    pushl_cfi %ebp
    pushl_cfi %edi
    pushl_cfi %esi
    pushl_cfi %edx
    pushl_cfi %ecx
    pushl_cfi %ebx
    movl $(__USER_DS), %edx   # 加载内核数据段
    movl %edx, %ds
    movl %edx, %es
    movl $(__KERNEL_PERCPU), %edx
    movl %edx, %fs
    SET_KERNEL_GS %edx
.endm
```

### 参数传递约定

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      x86-32 系统调用参数传递                                 │
│                                                                              │
│   寄存器        用途                                                         │
│  ────────────────────────────────────────────────────────────────────────   │
│   %eax         系统调用号                                                    │
│   %ebx         第1个参数                                                     │
│   %ecx         第2个参数                                                     │
│   %edx         第3个参数                                                     │
│   %esi         第4个参数                                                     │
│   %edi         第5个参数                                                     │
│   %ebp         第6个参数 (需从用户栈加载)                                    │
│                                                                              │
│   返回值       %eax                                                          │
│                                                                              │
│  ────────────────────────────────────────────────────────────────────────   │
│                      x86-64 系统调用参数传递                                 │
│                                                                              │
│   %rax         系统调用号                                                    │
│   %rdi         第1个参数                                                     │
│   %rsi         第2个参数                                                     │
│   %rdx         第3个参数                                                     │
│   %r10         第4个参数 (注意不是 %rcx)                                     │
│   %r8          第5个参数                                                     │
│   %r9          第6个参数                                                     │
│                                                                              │
│   返回值       %rax                                                          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 系统调用处理流程

### 完整执行流程

```
用户空间: write(1, "hello", 5)
                │
                ▼
┌───────────────────────────────────────────────────────────────────────────┐
│ 1. glibc 包装函数                                                          │
│                                                                            │
│    movl $4, %eax        # NR_write = 4                                    │
│    movl $1, %ebx        # fd = 1 (stdout)                                 │
│    movl $msg, %ecx      # buf = "hello"                                   │
│    movl $5, %edx        # count = 5                                       │
│    int $0x80            # 或 call *%gs:SYSENTER_RETURN                    │
│                                                                            │
└───────────────────────────────────────────────────────────────────────────┘
                │
                │ 特权级切换 Ring 3 → Ring 0
                ▼
┌───────────────────────────────────────────────────────────────────────────┐
│ 2. system_call 入口 (entry_32.S:499)                                       │
│                                                                            │
│    ENTRY(system_call)                                                      │
│        RING0_INT_FRAME                                                     │
│        pushl %eax               # 保存 orig_eax                           │
│        SAVE_ALL                 # 保存所有寄存器                            │
│        GET_THREAD_INFO(%ebp)    # 获取当前线程信息                         │
│                                                                            │
│        # 检查是否需要系统调用跟踪                                           │
│        testl $_TIF_WORK_SYSCALL_ENTRY, TI_flags(%ebp)                      │
│        jnz syscall_trace_entry                                             │
│                                                                            │
│        # 检查系统调用号是否有效                                             │
│        cmpl $(nr_syscalls), %eax                                           │
│        jae syscall_badsys                                                  │
│                                                                            │
└───────────────────────────────────────────────────────────────────────────┘
                │
                ▼
┌───────────────────────────────────────────────────────────────────────────┐
│ 3. syscall_call - 调用处理函数                                             │
│                                                                            │
│    syscall_call:                                                           │
│        call *sys_call_table(,%eax,4)  # 调用 sys_write                    │
│                                                                            │
│    计算: sys_call_table + 4 × 4 = sys_call_table + 16                      │
│    指向: sys_write 函数地址                                                 │
│                                                                            │
└───────────────────────────────────────────────────────────────────────────┘
                │
                ▼
┌───────────────────────────────────────────────────────────────────────────┐
│ 4. sys_write 处理函数 (fs/read_write.c)                                    │
│                                                                            │
│    SYSCALL_DEFINE3(write, unsigned int, fd,                               │
│                    const char __user *, buf, size_t, count)               │
│    {                                                                       │
│        struct file *file;                                                  │
│        ssize_t ret = -EBADF;                                              │
│        struct fd f = fdget(fd);                                           │
│                                                                            │
│        if (f.file) {                                                      │
│            ret = vfs_write(f.file, buf, count, &pos);                     │
│            fdput(f);                                                      │
│        }                                                                   │
│        return ret;                                                        │
│    }                                                                       │
│                                                                            │
└───────────────────────────────────────────────────────────────────────────┘
                │
                │ 返回值在 %eax
                ▼
┌───────────────────────────────────────────────────────────────────────────┐
│ 5. 保存返回值                                                              │
│                                                                            │
│    movl %eax, PT_EAX(%esp)      # 存入栈帧的 eax 位置                      │
│                                                                            │
└───────────────────────────────────────────────────────────────────────────┘
                │
                ▼
             syscall_exit
```

### 系统调用跟踪 (ptrace)

```asm
syscall_trace_entry:
    movl $-ENOSYS, PT_EAX(%esp)
    movl %esp, %eax
    call syscall_trace_enter        # 通知 ptrace
    
    # 重新加载系统调用号 (可能被修改)
    movl PT_ORIG_EAX(%esp), %eax
    cmpl $(nr_syscalls), %eax
    jnae syscall_call
    jmp syscall_exit
```

---

## 系统调用返回流程

### syscall_exit 处理

```asm
// arch/x86/kernel/entry_32.S

syscall_exit:
    LOCKDEP_SYS_EXIT
    DISABLE_INTERRUPTS(CLBR_ANY)    # 关中断，防止竞态
    TRACE_IRQS_OFF
    
    movl TI_flags(%ebp), %ecx       # 检查线程标志
    testl $_TIF_ALLWORK_MASK, %ecx  # 是否有待处理工作?
    jne syscall_exit_work           # 有则处理
    
restore_all:
    TRACE_IRQS_IRET
    
restore_all_notrace:
    movl PT_EFLAGS(%esp), %eax      # 检查返回模式
    movb PT_OLDSS(%esp), %ah
    movb PT_CS(%esp), %al
    andl $(X86_EFLAGS_VM | ...), %eax
    cmpl $((SEGMENT_LDT << 8) | USER_RPL), %eax
    je ldt_ss                       # 特殊处理 LDT SS
    
restore_nocheck:
    RESTORE_REGS 4                  # 恢复寄存器
    
irq_return:
    INTERRUPT_RETURN                # iret 返回用户空间
```

### syscall_exit_work - 处理待处理工作

```asm
syscall_exit_work:
    testl $_TIF_WORK_SYSCALL_EXIT, %ecx
    jz work_pending
    
    TRACE_IRQS_ON
    ENABLE_INTERRUPTS(CLBR_ANY)
    
    movl %esp, %eax
    call syscall_trace_leave        # ptrace 通知
    jmp resume_userspace
    
work_pending:
    testb $_TIF_NEED_RESCHED, %cl
    jz work_notifysig
    
work_resched:
    call schedule                   # 重新调度
    LOCKDEP_SYS_EXIT
    DISABLE_INTERRUPTS(CLBR_ANY)
    TRACE_IRQS_OFF
    movl TI_flags(%ebp), %ecx
    andl $_TIF_WORK_MASK, %ecx
    jz restore_all
    testb $_TIF_NEED_RESCHED, %cl
    jnz work_resched
    
work_notifysig:
    # 处理信号...
    call do_notify_resume
    jmp resume_userspace
```

### 返回流程图

```
syscall_exit
      │
      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           检查 TI_flags                                      │
│                                                                              │
│   _TIF_ALLWORK_MASK 包括:                                                    │
│   - _TIF_NEED_RESCHED   需要重新调度                                         │
│   - _TIF_SIGPENDING     有待处理信号                                         │
│   - _TIF_NOTIFY_RESUME  需要通知                                             │
│   - _TIF_SYSCALL_TRACE  系统调用跟踪                                         │
│   - ...                                                                      │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
      │
      ├── 有待处理工作 ──► syscall_exit_work
      │                          │
      │                          ├── 需要调度 ──► schedule()
      │                          ├── 有信号 ──► do_notify_resume()
      │                          └── 有跟踪 ──► syscall_trace_leave()
      │
      └── 无待处理工作 ──► restore_all
                                │
                                ▼
                         RESTORE_REGS
                                │
                                ▼
                            iret
                                │
                                ▼
                     返回用户空间继续执行
```

---

## 系统调用定义宏

### SYSCALL_DEFINEx 宏

```c
// include/linux/syscalls.h

#define SYSCALL_DEFINE0(name)    asmlinkage long sys_##name(void)
#define SYSCALL_DEFINE1(name, ...) SYSCALL_DEFINEx(1, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE2(name, ...) SYSCALL_DEFINEx(2, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE3(name, ...) SYSCALL_DEFINEx(3, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE4(name, ...) SYSCALL_DEFINEx(4, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE5(name, ...) SYSCALL_DEFINEx(5, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE6(name, ...) SYSCALL_DEFINEx(6, _##name, __VA_ARGS__)

// 展开后
#define __SYSCALL_DEFINEx(x, name, ...)                         \
    asmlinkage long sys##name(__SC_DECL##x(__VA_ARGS__))
```

### 使用示例

```c
// fs/read_write.c

SYSCALL_DEFINE3(write, unsigned int, fd, 
                const char __user *, buf, 
                size_t, count)
{
    struct fd f = fdget(fd);
    ssize_t ret = -EBADF;
    
    if (f.file) {
        loff_t pos = file_pos_read(f.file);
        ret = vfs_write(f.file, buf, count, &pos);
        file_pos_write(f.file, pos);
        fdput(f);
    }
    
    return ret;
}

// 展开后:
asmlinkage long sys_write(unsigned int fd, 
                          const char __user *buf, 
                          size_t count)
{
    // 函数体...
}
```

### asmlinkage 关键字

```c
// 告诉编译器: 所有参数从栈上传递，而不是通过寄存器
// 在 x86 上展开为:
#define asmlinkage __attribute__((regparm(0)))
```

---

## 实例分析

### write() 系统调用完整流程

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      write(1, "hello\n", 6) 完整流程                         │
└─────────────────────────────────────────────────────────────────────────────┘

用户程序:
    printf("hello\n");  ──► glibc ──► write(1, "hello\n", 6)
                                            │
══════════════════════════════════════════════════════════════════════════════
                                            │
                                            ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ glibc 的 write() 实现                                                        │
│                                                                              │
│   # 设置寄存器                                                                │
│   mov $4, %eax          # __NR_write = 4                                    │
│   mov $1, %ebx          # fd = 1 (stdout)                                   │
│   mov $msg, %ecx        # buf 地址                                           │
│   mov $6, %edx          # count = 6                                         │
│                                                                              │
│   # 通过 vDSO 调用 (优先使用 sysenter)                                       │
│   call *%gs:SYSENTER_RETURN                                                 │
│   # 或者: int $0x80                                                          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
                                            │
                              ┌─────────────┴─────────────┐
                              │                           │
                              ▼                           ▼
                    ┌─────────────────┐        ┌─────────────────┐
                    │   int $0x80     │        │    sysenter     │
                    │                 │        │                 │
                    │ 查 IDT[0x80]   │        │ 读 MSR 寄存器   │
                    │ 跳转 system_call│        │ 跳转 sysenter_ │
                    │                 │        │        target   │
                    └────────┬────────┘        └────────┬────────┘
                             │                          │
                             └────────────┬─────────────┘
                                          │
                                          ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ 内核入口                                                                     │
│                                                                              │
│   1. 切换到内核栈 (从 TSS 获取)                                              │
│   2. 保存用户态寄存器 (SAVE_ALL)                                             │
│   3. 设置内核段寄存器 (DS, ES, FS, GS)                                       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
                                          │
                                          ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ 系统调用分发                                                                 │
│                                                                              │
│   cmpl $(nr_syscalls), %eax     # 检查调用号有效性                           │
│   jae syscall_badsys                                                         │
│                                                                              │
│   call *sys_call_table(,%eax,4) # sys_call_table[4] = sys_write             │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
                                          │
                                          ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ sys_write() 执行                                                             │
│                                                                              │
│   sys_write(fd=1, buf="hello\n", count=6)                                   │
│       │                                                                      │
│       ├── fdget(1) ──► 获取 stdout 的 struct file                           │
│       │                                                                      │
│       ├── vfs_write(file, buf, count, &pos)                                 │
│       │       │                                                              │
│       │       ├── 权限检查                                                   │
│       │       │                                                              │
│       │       └── file->f_op->write()  ──► 具体驱动                         │
│       │               │                                                      │
│       │               └── tty_write() ──► 终端输出 "hello\n"                │
│       │                                                                      │
│       └── 返回 6 (写入字节数)                                                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
                                          │
                                          ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ 返回用户空间                                                                 │
│                                                                              │
│   movl %eax, PT_EAX(%esp)       # 保存返回值 6                              │
│                                                                              │
│   # 检查待处理工作                                                            │
│   testl $_TIF_ALLWORK_MASK, TI_flags(%ebp)                                  │
│   jne syscall_exit_work         # 处理信号、调度等                           │
│                                                                              │
│   RESTORE_REGS                  # 恢复用户寄存器                              │
│   iret                          # 返回用户空间                                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
                                          │
══════════════════════════════════════════════════════════════════════════════
                                          │
                                          ▼
用户程序继续执行，返回值 6 在 %eax 中
```

---

## 关键源码文件

### 系统调用入口

| 文件 | 功能 |
|------|------|
| `arch/x86/kernel/entry_32.S` | x86-32 系统调用入口 |
| `arch/x86/kernel/entry_64.S` | x86-64 系统调用入口 |
| `arch/x86/kernel/syscall_table_32.S` | x86-32 系统调用表 |

### 系统调用定义

| 文件 | 功能 |
|------|------|
| `include/linux/syscalls.h` | 系统调用声明和宏定义 |
| `include/asm/unistd.h` | 系统调用号定义 |
| `kernel/sys.c` | 通用系统调用实现 |
| `fs/read_write.c` | read/write 实现 |
| `fs/open.c` | open/close 实现 |
| `kernel/fork.c` | fork/clone 实现 |
| `kernel/exit.c` | exit 实现 |

### 相关头文件

| 文件 | 功能 |
|------|------|
| `arch/x86/include/asm/ptrace.h` | pt_regs 结构定义 |
| `arch/x86/include/asm/thread_info.h` | thread_info 结构 |
| `include/linux/thread_info.h` | TIF 标志定义 |

---

## 总结

### 系统调用核心机制

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          系统调用核心要点                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  1. 触发方式:                                                                 │
│     - int $0x80: 传统软中断，通过 IDT 查找入口                               │
│     - sysenter: 快速系统调用，直接从 MSR 读取入口                            │
│                                                                              │
│  2. 参数传递:                                                                 │
│     - x86-32: eax(调用号), ebx/ecx/edx/esi/edi/ebp(参数1-6)                 │
│     - x86-64: rax(调用号), rdi/rsi/rdx/r10/r8/r9(参数1-6)                   │
│                                                                              │
│  3. 执行流程:                                                                 │
│     用户态 → 特权级切换 → 保存寄存器 → 查表调用 → 返回值 → 恢复 → 用户态     │
│                                                                              │
│  4. 返回处理:                                                                 │
│     - 检查待处理信号                                                          │
│     - 检查是否需要调度                                                        │
│     - 恢复寄存器并返回                                                        │
│                                                                              │
│  5. 系统调用表:                                                               │
│     - 函数指针数组，索引为系统调用号                                          │
│     - 调用: call *sys_call_table(,%eax,4)                                   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 性能优化

| 优化 | 说明 |
|------|------|
| sysenter/syscall | 比 int $0x80 快约 25% |
| vDSO | 某些调用完全在用户空间完成 |
| 快速路径 | 无跟踪/无待处理工作时直接返回 |

---

*本文档基于 Linux 3.2 内核 x86 架构源码分析*

