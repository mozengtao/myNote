# sys_getpid 系统调用完整执行路径追踪

基于 Linux 3.2 内核 x86-32 架构

---

## 目录

- [执行路径概览](#执行路径概览)
- [1. 用户态调用入口](#1-用户态调用入口)
- [2. 陷入内核的机制](#2-陷入内核的机制)
- [3. 实际处理函数](#3-实际处理函数)
- [4. 返回用户态的过程](#4-返回用户态的过程)
- [完整时序图](#完整时序图)
- [关键源码位置](#关键源码位置)

---

## 执行路径概览

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    sys_getpid 完整执行路径                                    │
└─────────────────────────────────────────────────────────────────────────────┘

用户态 (Ring 3)                                 内核态 (Ring 0)
──────────────                                  ──────────────

getpid()                                        
  │ (glibc 包装)                                
  ▼                                             
┌─────────────────┐                             
│ mov $20, %eax   │  系统调用号 = 20            
│ int $0x80       │  ───────────────────────────►┌─────────────────────────┐
│   或 sysenter   │                              │ system_call (entry_32.S)│
└─────────────────┘                              │   SAVE_ALL              │
                                                 │   call *sys_call_table  │
                                                 └───────────┬─────────────┘
                                                             │
                                                             ▼
                                                 ┌─────────────────────────┐
                                                 │ sys_getpid (timer.c)    │
                                                 │   task_tgid_vnr(current)│
                                                 │   return pid            │
                                                 └───────────┬─────────────┘
                                                             │
                                                             ▼
                                                 ┌─────────────────────────┐
                                                 │ syscall_exit            │
                                                 │   RESTORE_REGS          │
┌─────────────────┐                              │   iret                  │
│ 返回值在 %eax   │  ◄───────────────────────────└─────────────────────────┘
│ (即当前进程PID)  │                              
└─────────────────┘                              
```

---

## 1. 用户态调用入口

### 1.1 用户程序调用

```c
// 用户程序代码
#include <stdio.h>
#include <unistd.h>

int main() {
    pid_t pid = getpid();  // glibc 包装函数
    printf("My PID is: %d\n", pid);
    return 0;
}
```

### 1.2 glibc 包装函数

```c
// glibc: sysdeps/unix/sysv/linux/getpid.c (简化版)

pid_t __getpid(void)
{
    // 现代 glibc 会缓存 PID，首次调用才真正进入内核
    // 通过 VDSO 或 syscall 指令调用
    return INLINE_SYSCALL(getpid, 0);
}
weak_alias(__getpid, getpid)
```

### 1.3 汇编层面调用

```asm
# glibc 生成的系统调用代码 (x86-32)

# 方式一: int $0x80 (传统)
    movl    $20, %eax        # __NR_getpid = 20
    int     $0x80            # 触发软中断，陷入内核
    # 返回后，结果在 %eax

# 方式二: sysenter (快速，通过 VDSO)
    movl    $20, %eax        # 系统调用号
    movl    %esp, %ebp       # 保存用户栈指针
    sysenter                 # 快速进入内核
    # 返回后，结果在 %eax
```

### 1.4 系统调用号定义

```c
// arch/x86/include/asm/unistd_32.h
#define __NR_getpid     20
```

```asm
// arch/x86/kernel/syscall_table_32.S:22
// 系统调用表中的位置

ENTRY(sys_call_table)
    .long sys_restart_syscall    /* 0 */
    .long sys_exit               /* 1 */
    ...
    .long sys_lseek              /* 19 */
    .long sys_getpid             /* 20 */  ◄── 这里
    .long sys_mount              /* 21 */
    ...
```

---

## 2. 陷入内核的机制

### 2.1 int $0x80 软中断方式

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      int $0x80 陷入过程                                      │
└─────────────────────────────────────────────────────────────────────────────┘

用户态执行: int $0x80
              │
              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CPU 硬件自动完成                                     │
│                                                                              │
│  1. 检测到软中断指令，触发特权级切换                                          │
│                                                                              │
│  2. 从 TSS (Task State Segment) 中读取:                                      │
│     - SS0: 内核态栈段选择子                                                  │
│     - ESP0: 内核态栈指针                                                     │
│                                                                              │
│  3. 切换到内核栈，依次压入用户态现场:                                         │
│     ┌──────────────┐                                                         │
│     │ SS (用户态)  │                                                         │
│     │ ESP (用户态) │                                                         │
│     │ EFLAGS       │                                                         │
│     │ CS (用户态)  │                                                         │
│     │ EIP (用户态) │ ◄── 返回地址 (int $0x80 的下一条指令)                   │
│     └──────────────┘                                                         │
│                                                                              │
│  4. 从 IDT[0x80] 读取中断描述符:                                             │
│     - 获取中断处理程序的 CS:EIP                                              │
│     - 跳转到 system_call 入口                                                │
│                                                                              │
│  特权级变化: Ring 3 ──────────────────► Ring 0                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
              │
              ▼
        system_call 入口
```

### 2.2 sysenter 快速系统调用方式

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      sysenter 陷入过程                                       │
└─────────────────────────────────────────────────────────────────────────────┘

用户态执行: sysenter
              │
              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CPU 硬件自动完成                                     │
│                                                                              │
│  直接从 MSR (Model Specific Register) 寄存器读取:                            │
│                                                                              │
│  - SYSENTER_CS_MSR  (0x174) → CS                                            │
│  - SYSENTER_ESP_MSR (0x175) → ESP                                           │
│  - SYSENTER_EIP_MSR (0x176) → EIP (指向 ia32_sysenter_target)               │
│                                                                              │
│  特点:                                                                        │
│  - 不查 IDT，速度更快                                                        │
│  - 不自动保存用户态寄存器，需软件处理                                         │
│  - 禁用中断 (清除 EFLAGS.IF)                                                 │
│                                                                              │
│  比 int $0x80 快约 25-40%                                                    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
              │
              ▼
        ia32_sysenter_target 入口
```

### 2.3 system_call 入口代码分析

```asm
// arch/x86/kernel/entry_32.S:499

ENTRY(system_call)
    RING0_INT_FRAME             # 标记栈帧类型

    pushl_cfi %eax              # 保存 orig_eax (系统调用号 20)
    
    SAVE_ALL                    # ★ 保存所有用户态寄存器
    
    GET_THREAD_INFO(%ebp)       # 获取当前线程的 thread_info
                                # %ebp = current_thread_info()
    
    # 检查是否需要系统调用跟踪/审计
    testl $_TIF_WORK_SYSCALL_ENTRY, TI_flags(%ebp)
    jnz syscall_trace_entry     # 有跟踪则跳转处理
    
    # ★ 验证系统调用号
    cmpl $(nr_syscalls), %eax   # eax=20, nr_syscalls≈350
    jae syscall_badsys          # 如果 >= 350，无效调用

syscall_call:
    # ★ 核心: 调用系统调用处理函数
    call *sys_call_table(,%eax,4)
    # 计算: sys_call_table + 20 * 4 = sys_call_table + 80
    # 取出地址处的函数指针 = sys_getpid
    # 执行 call sys_getpid
    
    # ★ 保存返回值
    movl %eax, PT_EAX(%esp)     # 将 sys_getpid 返回值存入栈中 eax 位置
```

### 2.4 SAVE_ALL 宏详解

```asm
// arch/x86/kernel/entry_32.S

.macro SAVE_ALL
    cld                         # 清除方向标志
    PUSH_GS                     # 保存 %gs
    pushl_cfi %fs               # 保存 %fs
    pushl_cfi %es               # 保存 %es
    pushl_cfi %ds               # 保存 %ds
    pushl_cfi %eax              # 保存 %eax (会被返回值覆盖)
    pushl_cfi %ebp              # 保存 %ebp
    pushl_cfi %edi              # 保存 %edi
    pushl_cfi %esi              # 保存 %esi
    pushl_cfi %edx              # 保存 %edx
    pushl_cfi %ecx              # 保存 %ecx
    pushl_cfi %ebx              # 保存 %ebx
    
    # 加载内核数据段
    movl $(__USER_DS), %edx
    movl %edx, %ds
    movl %edx, %es
    movl $(__KERNEL_PERCPU), %edx
    movl %edx, %fs
    SET_KERNEL_GS %edx
.endm
```

### 2.5 内核栈布局 (pt_regs)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    调用 sys_getpid 时的内核栈布局                             │
└─────────────────────────────────────────────────────────────────────────────┘

高地址
        ┌──────────────────────────────────────────┐
  +0x40 │  SS (用户态栈段)                         │ ← CPU 自动保存
        ├──────────────────────────────────────────┤
  +0x3C │  ESP (用户态栈指针)                      │ ← CPU 自动保存
        ├──────────────────────────────────────────┤
  +0x38 │  EFLAGS (标志寄存器)                     │ ← CPU 自动保存
        ├──────────────────────────────────────────┤
  +0x34 │  CS (用户态代码段)                       │ ← CPU 自动保存
        ├──────────────────────────────────────────┤
  +0x30 │  EIP (用户态返回地址)                    │ ← CPU 自动保存
        ├──────────────────────────────────────────┤
  +0x2C │  orig_eax = 20 (系统调用号)              │ ← pushl %eax
        ├──────────────────────────────────────────┤
  +0x28 │  gs                                      │ ┐
        ├──────────────────────────────────────────┤ │
  +0x24 │  fs                                      │ │
        ├──────────────────────────────────────────┤ │
  +0x20 │  es                                      │ │
        ├──────────────────────────────────────────┤ │
  +0x1C │  ds                                      │ │
        ├──────────────────────────────────────────┤ │
  +0x18 │  eax = 20 → 返回值 (PID)                 │ │ SAVE_ALL 保存
        ├──────────────────────────────────────────┤ │
  +0x14 │  ebp                                     │ │
        ├──────────────────────────────────────────┤ │
  +0x10 │  edi                                     │ │
        ├──────────────────────────────────────────┤ │
  +0x0C │  esi                                     │ │
        ├──────────────────────────────────────────┤ │
  +0x08 │  edx                                     │ │
        ├──────────────────────────────────────────┤ │
  +0x04 │  ecx                                     │ │
        ├──────────────────────────────────────────┤ │
  +0x00 │  ebx                                     │ ┘
        └──────────────────────────────────────────┘
低地址 (ESP)
```

---

## 3. 实际处理函数

### 3.1 sys_getpid 源码

```c
// kernel/timer.c:1350-1358

/*
 * The "pid" is actually the "tgid" (thread group ID).
 * 
 * Some comments: 
 * - pid 和 tgid 在单线程进程中是相同的
 * - 在多线程进程中，所有线程共享相同的 tgid (即主线程的 pid)
 * - 这是 SMP 安全的，因为 current->tgid 不会改变
 */
SYSCALL_DEFINE0(getpid)
{
    return task_tgid_vnr(current);
}
```

### 3.2 SYSCALL_DEFINE0 宏展开

```c
// include/linux/syscalls.h

#define SYSCALL_DEFINE0(name) asmlinkage long sys_##name(void)

// 展开后等价于:
asmlinkage long sys_getpid(void)
{
    return task_tgid_vnr(current);
}
```

### 3.3 current 宏

```c
// arch/x86/include/asm/current.h

DECLARE_PER_CPU(struct task_struct *, current_task);

static __always_inline struct task_struct *get_current(void)
{
    return percpu_read_stable(current_task);
}

#define current get_current()

// current 指向当前 CPU 上正在运行的进程的 task_struct
```

### 3.4 task_tgid_vnr 函数调用链

```c
// include/linux/sched.h:1678-1681

static inline pid_t task_tgid_vnr(struct task_struct *tsk)
{
    return pid_vnr(task_tgid(tsk));
}

// task_tgid: 获取线程组的 pid 结构
static inline struct pid *task_tgid(struct task_struct *task)
{
    return task->group_leader->pids[PIDTYPE_PID].pid;
}

// kernel/pid.c
pid_t pid_vnr(struct pid *pid)
{
    // 返回在当前 PID 命名空间中的虚拟 PID
    return pid_nr_ns(pid, task_active_pid_ns(current));
}

pid_t pid_nr_ns(struct pid *pid, struct pid_namespace *ns)
{
    struct upid *upid;
    pid_t nr = 0;

    if (pid && ns->level <= pid->level) {
        upid = &pid->numbers[ns->level];
        if (upid->ns == ns)
            nr = upid->nr;  // 返回实际的 PID 数值
    }
    return nr;
}
```

### 3.5 执行流程图

```
sys_getpid()
      │
      ├── current                        
      │       │
      │       └── percpu_read(current_task)
      │               │
      │               └── 返回当前进程的 task_struct 指针
      │
      └── task_tgid_vnr(current)
              │
              ├── task_tgid(current)
              │       │
              │       └── current->group_leader->pids[PIDTYPE_PID].pid
              │               │
              │               └── 返回 struct pid 指针
              │
              └── pid_vnr(pid)
                      │
                      ├── task_active_pid_ns(current)
                      │       │
                      │       └── 获取当前进程的 PID 命名空间
                      │
                      └── pid_nr_ns(pid, ns)
                              │
                              └── pid->numbers[ns->level].nr
                                      │
                                      └── 返回 PID 数值 (如 1234)
```

### 3.6 task_struct 中的相关字段

```c
// include/linux/sched.h

struct task_struct {
    // ...
    
    pid_t pid;                      // 进程 ID (对线程是唯一的)
    pid_t tgid;                     // 线程组 ID (主线程的 pid)
    
    struct task_struct *group_leader; // 指向线程组领导者
    
    struct pid_link pids[PIDTYPE_MAX]; // PID 链接
    
    // ...
};

// PID 类型
enum pid_type {
    PIDTYPE_PID,    // 进程 ID
    PIDTYPE_PGID,   // 进程组 ID
    PIDTYPE_SID,    // 会话 ID
    PIDTYPE_MAX
};
```

---

## 4. 返回用户态的过程

### 4.1 syscall_exit 处理

```asm
// arch/x86/kernel/entry_32.S:512-520

syscall_exit:
    LOCKDEP_SYS_EXIT
    
    # ★ 关闭中断，防止在检查和返回之间发生竞态
    DISABLE_INTERRUPTS(CLBR_ANY)
    TRACE_IRQS_OFF
    
    # ★ 检查是否有待处理的工作
    movl TI_flags(%ebp), %ecx       # 读取 thread_info->flags
    testl $_TIF_ALLWORK_MASK, %ecx  # 检查标志位
    jne syscall_exit_work           # 有工作则跳转处理
    # 对于简单的 getpid，通常直接返回
```

### 4.2 _TIF_ALLWORK_MASK 检查项

```c
// arch/x86/include/asm/thread_info.h

#define _TIF_ALLWORK_MASK   \
    (_TIF_SIGPENDING |       // 有待处理信号
     _TIF_NEED_RESCHED |     // 需要重新调度
     _TIF_SINGLESTEP |       // 单步调试
     _TIF_SYSCALL_EMU |      // 系统调用模拟
     _TIF_SYSCALL_AUDIT |    // 系统调用审计
     _TIF_SECCOMP |          // 安全计算模式
     _TIF_USER_RETURN_NOTIFY) // 用户返回通知
```

### 4.3 syscall_exit_work 处理 (如果有待处理工作)

```asm
syscall_exit_work:
    testl $_TIF_WORK_SYSCALL_EXIT, %ecx
    jz work_pending
    
    TRACE_IRQS_ON
    ENABLE_INTERRUPTS(CLBR_ANY)     # 开中断
    
    movl %esp, %eax
    call syscall_trace_leave        # 系统调用跟踪
    jmp resume_userspace

work_pending:
    testb $_TIF_NEED_RESCHED, %cl
    jz work_notifysig
    
work_resched:
    call schedule                   # ★ 重新调度
    LOCKDEP_SYS_EXIT
    DISABLE_INTERRUPTS(CLBR_ANY)
    TRACE_IRQS_OFF
    movl TI_flags(%ebp), %ecx
    andl $_TIF_WORK_MASK, %ecx
    jz restore_all
    testb $_TIF_NEED_RESCHED, %cl
    jnz work_resched

work_notifysig:
    # 处理待处理信号
    call do_notify_resume
    jmp resume_userspace
```

### 4.4 restore_all - 恢复寄存器

```asm
// arch/x86/kernel/entry_32.S:522-538

restore_all:
    TRACE_IRQS_IRET

restore_all_notrace:
    # 检查返回模式 (普通/LDT/VM86)
    movl PT_EFLAGS(%esp), %eax
    movb PT_OLDSS(%esp), %ah
    movb PT_CS(%esp), %al
    andl $(X86_EFLAGS_VM | (SEGMENT_TI_MASK << 8) | SEGMENT_RPL_MASK), %eax
    cmpl $((SEGMENT_LDT << 8) | USER_RPL), %eax
    CFI_REMEMBER_STATE
    je ldt_ss                       # LDT SS 需特殊处理

restore_nocheck:
    RESTORE_REGS 4                  # ★ 恢复所有寄存器，跳过 orig_eax

irq_return:
    INTERRUPT_RETURN                # ★ iret 返回用户态
```

### 4.5 RESTORE_REGS 宏

```asm
.macro RESTORE_REGS pop=0
    popl_cfi %ebx                   # 恢复 %ebx
    popl_cfi %ecx                   # 恢复 %ecx
    popl_cfi %edx                   # 恢复 %edx
    popl_cfi %esi                   # 恢复 %esi
    popl_cfi %edi                   # 恢复 %edi
    popl_cfi %ebp                   # 恢复 %ebp
    popl_cfi %eax                   # ★ 恢复 %eax (包含 sys_getpid 返回值)
    popl_cfi %ds                    # 恢复 %ds
    popl_cfi %es                    # 恢复 %es
    popl_cfi %fs                    # 恢复 %fs
    POP_GS \pop                     # 恢复 %gs
.endm
```

### 4.6 iret 指令执行

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         iret 指令执行过程                                    │
└─────────────────────────────────────────────────────────────────────────────┘

执行 iret 前的栈:
        ┌──────────────────┐
        │  SS (用户态)     │
        ├──────────────────┤
        │  ESP (用户态)    │
        ├──────────────────┤
        │  EFLAGS          │
        ├──────────────────┤
        │  CS (用户态)     │
        ├──────────────────┤
  ESP → │  EIP (用户态)    │ ← 返回地址
        └──────────────────┘

CPU 执行 iret:
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│  1. 从栈中弹出: EIP, CS, EFLAGS                                              │
│                                                                              │
│  2. 检测到特权级变化 (CS.RPL = 3, 当前 CPL = 0)                              │
│     Ring 0 → Ring 3                                                         │
│                                                                              │
│  3. 从栈中弹出: ESP, SS                                                      │
│                                                                              │
│  4. 加载段寄存器:                                                            │
│     - CS ← 用户态代码段                                                      │
│     - SS ← 用户态栈段                                                        │
│                                                                              │
│  5. 恢复 EFLAGS (包括中断标志 IF)                                            │
│                                                                              │
│  6. 切换到用户态栈 (SS:ESP)                                                  │
│                                                                              │
│  7. 跳转到用户态 EIP 继续执行                                                │
│                                                                              │
│  此时 %eax = sys_getpid() 的返回值 = 当前进程 PID                            │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
              │
              ▼
        用户程序继续执行
        返回值在 %eax 寄存器中
```

### 4.7 sysexit 快速返回 (对应 sysenter)

```asm
// arch/x86/kernel/entry_32.S:436-444

sysenter_exit:
    movl PT_EIP(%esp), %edx         # edx = 用户态 EIP (返回地址)
    movl PT_OLDESP(%esp), %ecx      # ecx = 用户态 ESP
    xorl %ebp, %ebp                 # 清零 ebp
    TRACE_IRQS_ON
    mov PT_FS(%esp), %fs            # 恢复 fs
    PTGS_TO_GS                      # 恢复 gs
    ENABLE_INTERRUPTS_SYSEXIT       # sti; sysexit
    
# sysexit 指令:
# - 从 SYSENTER_CS_MSR 计算 CS 和 SS
# - ECX → ESP (用户态栈)
# - EDX → EIP (用户态返回地址)
# - 开启中断
# - 切换到用户态
```

---

## 完整时序图

### 时序图 (int $0x80 方式)

```
用户态                                     内核态
───────                                    ───────
   │
   │ [1] getpid()
   │
   ▼
┌──────────────────────┐
│ glibc 包装函数       │
│                      │
│ mov $20, %eax        │
│ int $0x80            │───────────────────────────────────────────┐
└──────────────────────┘                                           │
                                                                   │
   ════════════════════════════════════════════════════════════════
                                                                   │
                                           ┌───────────────────────┘
                                           │
                                           ▼
                         ┌─────────────────────────────────────────────────┐
                         │ [2] CPU 自动操作:                                │
                         │     - 从 TSS 读取 SS0:ESP0                      │
                         │     - 切换到内核栈                               │
                         │     - 压入 SS, ESP, EFLAGS, CS, EIP             │
                         │     - 从 IDT[0x80] 加载处理程序                  │
                         └─────────────────────────────────────────────────┘
                                           │
                                           ▼
                         ┌─────────────────────────────────────────────────┐
                         │ [3] system_call 入口:                           │
                         │     pushl %eax        # 保存系统调用号 20       │
                         │     SAVE_ALL          # 保存所有寄存器          │
                         │     GET_THREAD_INFO   # 获取线程信息            │
                         └─────────────────────────────────────────────────┘
                                           │
                                           ▼
                         ┌─────────────────────────────────────────────────┐
                         │ [4] 系统调用分发:                                │
                         │     cmpl $(nr_syscalls), %eax  # 验证调用号     │
                         │     call *sys_call_table(,%eax,4)               │
                         │                                                 │
                         │     计算: sys_call_table + 20*4                 │
                         │          = sys_getpid 函数地址                  │
                         └─────────────────────────────────────────────────┘
                                           │
                                           ▼
                         ┌─────────────────────────────────────────────────┐
                         │ [5] sys_getpid() 执行:                          │
                         │                                                 │
                         │     // kernel/timer.c:1355                      │
                         │     SYSCALL_DEFINE0(getpid)                     │
                         │     {                                           │
                         │         return task_tgid_vnr(current);          │
                         │     }                                           │
                         │                                                 │
                         │     执行流程:                                    │
                         │     current ──► task_tgid() ──► pid_vnr()       │
                         │                                                 │
                         │     假设返回 PID = 1234                         │
                         └─────────────────────────────────────────────────┘
                                           │
                                           ▼
                         ┌─────────────────────────────────────────────────┐
                         │ [6] 保存返回值:                                  │
                         │     movl %eax, PT_EAX(%esp)                     │
                         │     # %eax = 1234 保存到栈中                     │
                         └─────────────────────────────────────────────────┘
                                           │
                                           ▼
                         ┌─────────────────────────────────────────────────┐
                         │ [7] syscall_exit:                               │
                         │     DISABLE_INTERRUPTS     # 关中断             │
                         │     testl TIF_ALLWORK_MASK # 检查待处理工作     │
                         │                                                 │
                         │     (getpid 通常无待处理工作，直接返回)          │
                         └─────────────────────────────────────────────────┘
                                           │
                                           ▼
                         ┌─────────────────────────────────────────────────┐
                         │ [8] restore_all:                                │
                         │     RESTORE_REGS           # 恢复寄存器         │
                         │     # %eax 恢复为 1234                          │
                         └─────────────────────────────────────────────────┘
                                           │
                                           ▼
                         ┌─────────────────────────────────────────────────┐
                         │ [9] iret:                                       │
                         │     - 弹出 EIP, CS, EFLAGS                      │
                         │     - 检测特权级变化 (Ring 0 → Ring 3)          │
                         │     - 弹出 ESP, SS                              │
                         │     - 切换到用户态                              │
                         └─────────────────────────────────────────────────┘
                                           │
   ════════════════════════════════════════│═══════════════════════════════
                                           │
                                           ▼
┌──────────────────────┐◄──────────────────┘
│ [10] 用户态继续执行  │
│                      │
│ %eax = 1234          │
│ (getpid 返回值)      │
│                      │
│ pid_t pid = 1234;    │
└──────────────────────┘
   │
   ▼
继续执行用户程序...
```

### 简化版时序图

```
┌──────────┐                                              ┌──────────┐
│ 用户程序  │                                              │   内核   │
└────┬─────┘                                              └────┬─────┘
     │                                                         │
     │ ① getpid()                                              │
     │────────────────────────────────────────────────────────►│
     │    mov $20, %eax; int $0x80                             │
     │                                                         │
     │                     ② CPU: 保存现场，切换栈，跳转        │
     │                     ─────────────────────────────────►  │
     │                                                         │
     │                     ③ system_call: SAVE_ALL             │
     │                     ─────────────────────────────────►  │
     │                                                         │
     │                     ④ call *sys_call_table(,%eax,4)     │
     │                     ─────────────────────────────────►  │
     │                                                         │
     │                     ⑤ sys_getpid:                       │
     │                        return task_tgid_vnr(current)    │
     │                        // 返回 1234                     │
     │                     ─────────────────────────────────►  │
     │                                                         │
     │                     ⑥ movl %eax, PT_EAX(%esp)           │
     │                     ─────────────────────────────────►  │
     │                                                         │
     │                     ⑦ syscall_exit: 检查标志            │
     │                     ─────────────────────────────────►  │
     │                                                         │
     │                     ⑧ RESTORE_REGS                      │
     │                     ─────────────────────────────────►  │
     │                                                         │
     │                     ⑨ iret: 返回用户态                  │
     │◄────────────────────────────────────────────────────────│
     │    %eax = 1234                                          │
     │                                                         │
     │ ⑩ pid = 1234                                            │
     │                                                         │
     ▼                                                         ▼
```

---

## 关键源码位置

### 用户态入口

| 组件 | 位置 |
|------|------|
| glibc getpid | `sysdeps/unix/sysv/linux/getpid.c` |
| 系统调用号 | `arch/x86/include/asm/unistd_32.h:20` |

### 内核入口

| 组件 | 文件 | 行号 |
|------|------|------|
| 系统调用表 | `arch/x86/kernel/syscall_table_32.S` | 22 |
| system_call 入口 | `arch/x86/kernel/entry_32.S` | 499-511 |
| sysenter 入口 | `arch/x86/kernel/entry_32.S` | 376-492 |
| SAVE_ALL 宏 | `arch/x86/kernel/entry_32.S` | 195-212 |

### 处理函数

| 组件 | 文件 | 行号 |
|------|------|------|
| sys_getpid | `kernel/timer.c` | 1355-1358 |
| task_tgid_vnr | `include/linux/sched.h` | 1678-1681 |
| pid_vnr | `kernel/pid.c` | |
| current 宏 | `arch/x86/include/asm/current.h` | |

### 返回路径

| 组件 | 文件 | 行号 |
|------|------|------|
| syscall_exit | `arch/x86/kernel/entry_32.S` | 512-520 |
| restore_all | `arch/x86/kernel/entry_32.S` | 522-538 |
| RESTORE_REGS | `arch/x86/kernel/entry_32.S` | 219-231 |
| sysenter_exit | `arch/x86/kernel/entry_32.S` | 436-444 |

---

## 总结

### sys_getpid 执行流程要点

| 阶段 | 关键操作 | 代码位置 |
|------|---------|---------|
| 用户调用 | `mov $20, %eax; int $0x80` | glibc |
| 陷入内核 | CPU 自动保存现场，跳转到 system_call | 硬件 |
| 保存寄存器 | `SAVE_ALL` 宏 | entry_32.S:502 |
| 分发调用 | `call *sys_call_table(,%eax,4)` | entry_32.S:510 |
| 执行处理 | `task_tgid_vnr(current)` | timer.c:1357 |
| 保存返回值 | `movl %eax, PT_EAX(%esp)` | entry_32.S:511 |
| 检查标志 | `testl $_TIF_ALLWORK_MASK` | entry_32.S:519 |
| 恢复寄存器 | `RESTORE_REGS` | entry_32.S:536 |
| 返回用户态 | `iret` 或 `sysexit` | entry_32.S:538 |

### 性能特点

- getpid 是最简单的系统调用之一
- 不涉及阻塞、不需要锁
- 现代 glibc 会缓存 PID，减少系统调用次数
- 通过 sysenter/sysexit 可以获得比 int $0x80 更好的性能

---

*本文档基于 Linux 3.2 内核 x86-32 架构源码分析*

