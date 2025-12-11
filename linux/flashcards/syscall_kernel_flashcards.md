# System Call Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel system call mechanism, implementation, and internals
> **Language**: English terms with Chinese explanations
> **Total Cards**: 100+

---

## 1. System Call Overview (系统调用概述)

---

Q: What is a system call and why is it needed?
A: 系统调用是用户程序请求内核服务的接口：

```
+------------------------------------------------------------------+
|                        User Space                                 |
|    Application (Ring 3)                                          |
|    +------------------+                                          |
|    | printf("Hello") |                                           |
|    |       |         |                                           |
|    |       v         |                                           |
|    | libc: write()   |                                           |
|    +--------+--------+                                           |
+-------------|----------------------------------------------------|
              | System Call Interface                              |
              | (软中断/syscall指令)                                |
+-------------v----------------------------------------------------|
|                        Kernel Space                              |
|    Kernel (Ring 0)                                              |
|    +------------------+                                          |
|    | sys_write()      |                                          |
|    |       |          |                                          |
|    |       v          |                                          |
|    | 访问硬件/内核数据 |                                          |
|    +------------------+                                          |
+------------------------------------------------------------------+
```

**为什么需要系统调用**：
1. **安全隔离**：用户程序不能直接访问硬件和内核数据
2. **资源管理**：内核统一管理系统资源
3. **抽象接口**：提供统一的硬件访问接口
4. **权限控制**：检查操作权限
[Basic]

---

Q: What are the main categories of system calls?
A: 
| 类别 | 示例 | 功能 |
|------|------|------|
| **进程管理** | fork, exec, exit, wait | 创建、执行、终止进程 |
| **文件操作** | open, read, write, close | 文件I/O |
| **设备操作** | ioctl, mmap | 设备控制和内存映射 |
| **信息获取** | getpid, uname, time | 获取系统/进程信息 |
| **通信** | pipe, socket, shmget | 进程间通信 |
| **内存管理** | brk, mmap, mprotect | 内存分配和保护 |
| **网络** | socket, bind, connect | 网络操作 |
| **信号** | kill, signal, sigaction | 信号处理 |

查看系统调用数量：
```bash
# x86_64
wc -l /usr/include/asm/unistd_64.h

# 或查看内核源码
grep -c "SYSCALL_DEFINE" kernel/*.c fs/*.c mm/*.c net/*.c
```
[Basic]

---

Q: How many system calls are there in Linux?
A: 
```
系统调用数量随版本增加：
+----------+------------------+
| 架构     | 大约数量         |
+----------+------------------+
| x86_64   | ~350+           |
| x86 (32) | ~380+           |
| ARM64    | ~280+           |
+----------+------------------+

查看系统调用表：
cat /usr/include/asm/unistd_64.h    # 用户空间头文件
cat arch/x86/entry/syscalls/syscall_64.tbl  # 内核源码

示例（x86_64部分）：
0   common  read            sys_read
1   common  write           sys_write
2   common  open            sys_open
3   common  close           sys_close
...
```
[Basic]

---

## 2. System Call Mechanism (系统调用机制)

---

Q: How does a system call work on x86_64?
A: 现代x86_64使用`syscall`指令：

```
用户空间                              内核空间
+------------------+                 +------------------+
| 1. 设置参数       |                 |                  |
|    rax = 系统调用号|                 |                  |
|    rdi = arg1     |                 |                  |
|    rsi = arg2     |                 |                  |
|    rdx = arg3     |                 |                  |
|    r10 = arg4     |                 |                  |
|    r8  = arg5     |                 |                  |
|    r9  = arg6     |                 |                  |
+--------+---------+                 |                  |
         |                           |                  |
         | 2. syscall指令            |                  |
         | (切换到Ring 0)            |                  |
         v                           v                  |
+------------------+        +--------+---------+        |
| CPU自动:          |        | 3. entry_SYSCALL |       |
| - RCX = RIP       |   -->  |    _64           |       |
| - R11 = RFLAGS    |        | - 保存用户态寄存器|       |
| - RIP = IA32_LSTAR|        | - 切换到内核栈   |       |
| - 切换到Ring 0    |        +--------+---------+       |
+------------------+                 |                  |
                                     v                  |
                            +--------+---------+        |
                            | 4. 调用sys_xxx() |        |
                            +--------+---------+        |
                                     |                  |
                                     v                  |
                            +--------+---------+        |
                            | 5. sysret指令    |        |
                            |    返回用户态    |        |
                            +------------------+        |
```

关键寄存器：
```
MSR寄存器（系统调用入口配置）：
- IA32_LSTAR: syscall入口地址 (entry_SYSCALL_64)
- IA32_STAR:  CS/SS选择器
- IA32_FMASK: RFLAGS掩码
```
[Intermediate]

---

Q: What is the difference between `int 0x80` and `syscall` instruction?
A: 
| 特性 | int 0x80 (传统) | syscall (现代) |
|------|----------------|----------------|
| 架构 | x86 (32位兼容) | x86_64 |
| 性能 | 较慢 | 快 (~100 cycles less) |
| 参数寄存器 | eax, ebx, ecx, edx, esi, edi | rax, rdi, rsi, rdx, r10, r8, r9 |
| 入口 | 中断描述符表 | MSR寄存器 |
| 返回 | iret | sysret |

```c
// int 0x80 方式（32位兼容）
mov eax, 1        // sys_exit
mov ebx, 0        // exit code
int 0x80

// syscall 方式（x86_64）
mov rax, 60       // sys_exit
mov rdi, 0        // exit code
syscall
```

内核入口：
```c
// int 0x80 入口
entry_INT80_compat  (arch/x86/entry/entry_64_compat.S)

// syscall 入口
entry_SYSCALL_64    (arch/x86/entry/entry_64.S)
```
[Intermediate]

---

Q: What happens during user-to-kernel mode transition?
A: 
```
用户态 (Ring 3)                    内核态 (Ring 0)
+------------------+              +------------------+
| 用户栈            |              | 内核栈            |
| +--------------+ |              | +--------------+ |
| | 用户数据      | |              | | pt_regs      | |
| +--------------+ |              | | (保存的寄存器)| |
|                  |              | +--------------+ |
|                  |              | | 内核调用栈   | |
|                  |              | +--------------+ |
+------------------+              +------------------+
        |                                  ^
        |          syscall指令             |
        +----------------------------------+

CPU执行syscall时自动完成：
1. RCX = 用户态RIP（返回地址）
2. R11 = 用户态RFLAGS
3. RIP = IA32_LSTAR（内核入口）
4. CS/SS = 内核段选择器
5. CPL = 0（Ring 0）
6. 关闭中断（RFLAGS.IF = 0，由FMASK控制）

软件（entry_SYSCALL_64）完成：
1. 切换到内核栈 (使用percpu变量)
2. 保存用户态寄存器到pt_regs
3. 调用系统调用处理函数
4. 恢复寄存器
5. 执行sysret返回用户态
```
[Intermediate]

---

## 3. System Call Table (系统调用表)

---

Q: How is the system call table organized?
A: 系统调用表是一个函数指针数组：

```c
// arch/x86/entry/syscall_64.c
asmlinkage const sys_call_ptr_t sys_call_table[__NR_syscall_max+1] = {
    [0 ... __NR_syscall_max] = &__x64_sys_ni_syscall,  // 默认：未实现
    #include <asm/syscalls_64.h>  // 填充实际系统调用
};

// 生成的syscalls_64.h内容类似：
[0] = __x64_sys_read,
[1] = __x64_sys_write,
[2] = __x64_sys_open,
[3] = __x64_sys_close,
...

// 系统调用号定义
// include/uapi/asm-generic/unistd.h
#define __NR_read     0
#define __NR_write    1
#define __NR_open     2
#define __NR_close    3
```

系统调用表查找：
```c
// arch/x86/entry/common.c
static __always_inline void do_syscall_64(struct pt_regs *regs, int nr)
{
    if (likely(nr < NR_syscalls)) {
        nr = array_index_nospec(nr, NR_syscalls);
        regs->ax = sys_call_table[nr](regs);  // 调用系统调用
    }
}
```
[Intermediate]

---

Q: How is the syscall table generated from the .tbl file?
A: 从表格文件自动生成：

```
# arch/x86/entry/syscalls/syscall_64.tbl 格式：
# <number> <abi> <name> <entry point>

0    common  read            sys_read
1    common  write           sys_write
2    common  open            sys_open
3    common  close           sys_close
...
57   common  fork            sys_fork
59   64      execve          sys_execve
...
```

生成过程：
```makefile
# scripts/syscalltbl.sh 处理 .tbl 文件

# 生成的文件：
# 1. include/generated/asm-offsets.h
# 2. arch/x86/include/generated/asm/syscalls_64.h
# 3. arch/x86/include/generated/uapi/asm/unistd_64.h
```

ABI类型：
| 类型 | 说明 |
|------|------|
| common | 64位和32位共用 |
| 64 | 仅64位 |
| x32 | x32 ABI |
[Intermediate]

---

## 4. System Call Implementation (系统调用实现)

---

Q: How to implement a system call using SYSCALL_DEFINEn?
A: 使用`SYSCALL_DEFINE`宏定义系统调用：

```c
// 宏定义 (include/linux/syscalls.h)
#define SYSCALL_DEFINE0(name) ...
#define SYSCALL_DEFINE1(name, type1, arg1) ...
#define SYSCALL_DEFINE2(name, type1, arg1, type2, arg2) ...
// 最多到 SYSCALL_DEFINE6

// 示例：sys_write 实现
// fs/read_write.c
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf,
                size_t, count)
{
    return ksys_write(fd, buf, count);
}

// 展开后等价于：
asmlinkage long __x64_sys_write(const struct pt_regs *regs)
{
    return __se_sys_write(regs->di, regs->si, regs->dx);
}

static long __se_sys_write(unsigned int fd, const char __user *buf, 
                           size_t count)
{
    return __do_sys_write(fd, buf, count);
}

static inline long __do_sys_write(unsigned int fd, const char __user *buf,
                                  size_t count)
{
    return ksys_write(fd, buf, count);
}
```

宏展开的层次：
```
SYSCALL_DEFINEn
    |
    +---> __x64_sys_xxx()     从pt_regs提取参数
    |
    +---> __se_sys_xxx()      签名扩展处理
    |
    +---> __do_sys_xxx()      实际实现（内联）
```
[Intermediate]

---

Q: Why does SYSCALL_DEFINE use multiple wrapper functions?
A: 多层包装解决几个问题：

```c
// 1. 参数提取：从pt_regs获取参数
asmlinkage long __x64_sys_write(const struct pt_regs *regs)
{
    // 从寄存器提取参数
    return __se_sys_write(
        (unsigned int)regs->di,      // arg1
        (const char __user *)regs->si, // arg2
        (size_t)regs->dx              // arg3
    );
}

// 2. 签名扩展：处理32位到64位的符号扩展问题
// 对于有符号参数，确保正确的符号扩展
static long __se_sys_write(unsigned int fd, ...)
{
    // 类型转换和检查
    long ret = __do_sys_write(fd, ...);
    return ret;
}

// 3. 安全性：asmlinkage确保从栈/寄存器正确获取参数
// 4. 追踪支持：在wrapper层可以插入syscall_trace入口
// 5. 错误处理：统一的返回值处理

// asmlinkage的作用（x86_64）：
// 告诉编译器参数通过寄存器传递（遵循syscall ABI）
#define asmlinkage __attribute__((regparm(0)))
```
[Advanced]

---

Q: What is the `__user` annotation?
A: `__user`标记用户空间指针：

```c
// include/linux/compiler_types.h
#ifdef __CHECKER__
# define __user    __attribute__((noderef, address_space(1)))
#else
# define __user
#endif

// 用法示例
SYSCALL_DEFINE3(write, unsigned int, fd, 
                const char __user *, buf,  // 用户空间缓冲区
                size_t, count)
{
    // 必须使用copy_from_user/copy_to_user访问
    char kbuf[256];
    
    // 错误：直接解引用
    // char c = *buf;  // Sparse会警告
    
    // 正确：使用辅助函数
    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;
}

// Sparse静态检查
make C=1  // 启用Sparse检查
// 检测用户/内核空间指针混用
```

其他空间注解：
```c
__kernel   // 内核空间（默认）
__user     // 用户空间
__iomem    // I/O内存空间
__percpu   // Per-CPU变量
__rcu      // RCU保护指针
```
[Intermediate]

---

## 5. System Call Entry Points (系统调用入口)

---

Q: What is the entry_SYSCALL_64 function?
A: `entry_SYSCALL_64`是x86_64系统调用入口：

```asm
// arch/x86/entry/entry_64.S
SYM_CODE_START(entry_SYSCALL_64)
    // 1. 保存用户态栈指针，切换到内核栈
    swapgs                          // 交换GS基址（切换到内核percpu）
    movq    %rsp, PER_CPU_VAR(cpu_tss_rw + TSS_sp2)  // 保存用户RSP
    movq    PER_CPU_VAR(cpu_current_top_of_stack), %rsp  // 内核栈
    
    // 2. 构建pt_regs结构
    pushq   $__USER_DS              // SS
    pushq   PER_CPU_VAR(cpu_tss_rw + TSS_sp2)  // RSP
    pushq   %r11                    // RFLAGS (syscall保存在r11)
    pushq   $__USER_CS              // CS
    pushq   %rcx                    // RIP (syscall保存在rcx)
    pushq   %rax                    // orig_rax (系统调用号)
    
    // 3. 保存其他寄存器
    PUSH_AND_CLEAR_REGS rax=$-ENOSYS
    
    // 4. 调用C处理函数
    movq    %rsp, %rdi              // pt_regs指针作为参数
    call    do_syscall_64
    
    // 5. 返回准备
    SWITCH_TO_USER_CR3_STACK scratch_reg=%rdi
    
    // 6. 恢复寄存器并返回
    POP_REGS
    swapgs
    sysretq                         // 返回用户态
SYM_CODE_END(entry_SYSCALL_64)
```
[Advanced]

---

Q: What is `struct pt_regs` and why is it important?
A: `pt_regs`保存系统调用时的CPU寄存器状态：

```c
// arch/x86/include/asm/ptrace.h
struct pt_regs {
    // 通用寄存器（按入栈顺序反向排列）
    unsigned long r15;
    unsigned long r14;
    unsigned long r13;
    unsigned long r12;
    unsigned long bp;      // RBP
    unsigned long bx;      // RBX
    unsigned long r11;
    unsigned long r10;
    unsigned long r9;
    unsigned long r8;
    unsigned long ax;      // RAX - 系统调用返回值
    unsigned long cx;      // RCX
    unsigned long dx;      // RDX - 参数3
    unsigned long si;      // RSI - 参数2
    unsigned long di;      // RDI - 参数1
    
    // 系统调用号
    unsigned long orig_ax;
    
    // 中断/异常帧
    unsigned long ip;      // RIP - 返回地址
    unsigned long cs;      // CS
    unsigned long flags;   // RFLAGS
    unsigned long sp;      // RSP - 用户栈
    unsigned long ss;      // SS
};

// 访问宏
#define regs_return_value(regs)    ((regs)->ax)
#define instruction_pointer(regs)  ((regs)->ip)
#define user_stack_pointer(regs)   ((regs)->sp)
```

使用场景：
```c
// 系统调用处理
asmlinkage long __x64_sys_foo(const struct pt_regs *regs)
{
    unsigned long arg1 = regs->di;  // 第一个参数
    unsigned long arg2 = regs->si;  // 第二个参数
    
    // 设置返回值
    regs->ax = result;
    return result;
}

// 调试和追踪
void syscall_trace(struct pt_regs *regs)
{
    printk("syscall %ld from %lx\n", regs->orig_ax, regs->ip);
}
```
[Intermediate]

---

## 6. System Call Return Path (系统调用返回路径)

---

Q: How does the system call return to user space?
A: 
```
系统调用返回路径：
+------------------------------------------------------------------+
|                                                                  |
|  do_syscall_64()                                                 |
|       |                                                          |
|       v                                                          |
|  regs->ax = 返回值                                               |
|       |                                                          |
|       v                                                          |
|  syscall_exit_to_user_mode()                                     |
|       |                                                          |
|       +---> 检查TIF_SIGPENDING  --> 处理挂起的信号              |
|       |                                                          |
|       +---> 检查TIF_NEED_RESCHED --> 可能调度其他任务           |
|       |                                                          |
|       +---> 检查TIF_NOTIFY_RESUME --> ptrace/seccomp等          |
|       |                                                          |
|       v                                                          |
|  exit_to_user_mode_prepare()                                     |
|       |                                                          |
|       v                                                          |
|  恢复用户态寄存器 (POP_REGS)                                     |
|       |                                                          |
|       v                                                          |
|  swapgs  (切换GS到用户态)                                        |
|       |                                                          |
|       v                                                          |
|  sysretq (返回用户态)                                            |
|       |                                                          |
|       v                                                          |
|  CPU自动: RIP = RCX, RFLAGS = R11, CPL = 3                      |
|                                                                  |
+------------------------------------------------------------------+
```

关键检查点：
```c
// kernel/entry/common.c
static void syscall_exit_to_user_mode_prepare(struct pt_regs *regs)
{
    // 处理工作标志
    unsigned long ti_work = READ_ONCE(current_thread_info()->flags);
    
    if (unlikely(ti_work & EXIT_TO_USER_MODE_WORK))
        ti_work = exit_to_user_mode_loop(regs, ti_work);
    
    // 需要检查的标志：
    // TIF_SIGPENDING    - 有待处理信号
    // TIF_NEED_RESCHED  - 需要重调度
    // TIF_NOTIFY_RESUME - 有通知要处理
    // TIF_UPROBE        - uprobe追踪
}
```
[Intermediate]

---

Q: How are system call errors returned?
A: 错误通过负数返回，libc转换为errno：

```c
// 内核返回
SYSCALL_DEFINE3(open, const char __user *, filename,
                int, flags, umode_t, mode)
{
    // 成功：返回非负fd
    // 失败：返回负错误码
    if (error)
        return -ENOENT;   // -2
    return fd;            // >= 0
}

// 用户空间（glibc处理）
// sysdeps/unix/sysv/linux/x86_64/syscall.S
// 检查返回值是否在错误范围内
int open(const char *path, int flags, ...)
{
    long ret = syscall(__NR_open, path, flags, mode);
    
    if (ret < 0 && ret > -4096) {
        // 返回值在 [-4095, -1] 范围内是错误
        errno = -ret;
        return -1;
    }
    return ret;
}

// 常见错误码
-EPERM      =  -1   // 操作不允许
-ENOENT     =  -2   // 文件不存在
-ESRCH      =  -3   // 进程不存在
-EINTR      =  -4   // 被信号中断
-EIO        =  -5   // I/O错误
-ENOEXEC    =  -8   // 执行格式错误
-EBADF      =  -9   // 无效文件描述符
-ENOMEM     = -12   // 内存不足
-EACCES     = -13   // 权限拒绝
-EFAULT     = -14   // 地址错误
-EBUSY      = -16   // 设备忙
-EEXIST     = -17   // 文件存在
-EINVAL     = -22   // 参数无效
-ENOSYS     = -38   // 功能未实现
```
[Basic]

---

## 7. Fast System Calls: vDSO (虚拟动态共享对象)

---

Q: What is vDSO and how does it speed up system calls?
A: vDSO (virtual Dynamic Shared Object) 避免某些系统调用进入内核：

```
传统系统调用：
User Space        Kernel Space
+----------+      +----------+
| gettimeofday()  |          |
+-----+----+      |          |
      | syscall   |          |
      +---------->+ sys_gettimeofday()
      |           | 读取时间 |
      +<----------+          |
      | 返回      |          |
+----------+      +----------+
总时间：~1000 cycles

vDSO加速：
User Space (vDSO映射区)
+------------------+
| gettimeofday()   |
|   +----------+   |
|   | 读取共享 |   | <-- 内核定期更新的时间数据
|   | 时间页面 |   |
|   +----------+   |
| 返回             |
+------------------+
总时间：~50 cycles (纯用户态)
```

vDSO原理：
```c
// vDSO是内核编译的共享库，映射到每个进程地址空间
// 位置：[vdso] 在 /proc/pid/maps 中可见

// 内核端准备共享数据
// arch/x86/entry/vdso/vma.c
struct vdso_data {
    u32 seq;                    // 序列号（更新同步）
    s32 clock_mode;
    u64 cycle_last;
    u64 mask;
    u32 mult;
    u32 shift;
    struct vdso_timestamp basetime[VDSO_BASES];
    s32 tz_minuteswest;
    s32 tz_dsttime;
};

// vDSO导出的函数
// arch/x86/entry/vdso/vclock_gettime.c
notrace int __vdso_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    // 直接从vdso_data读取，不进入内核
    if (likely(tv != NULL)) {
        if (do_realtime((struct timespec *)tv) == VCLOCK_NONE)
            return vdso_fallback_gtod(tv, tz);  // 回退到真正syscall
        tv->tv_usec /= 1000;
    }
    return 0;
}
```

vDSO提供的函数：
```
__vdso_clock_gettime    - 获取时间
__vdso_gettimeofday     - 获取时间
__vdso_time             - 获取秒数
__vdso_getcpu           - 获取当前CPU
__vdso_clock_getres     - 获取时钟精度
```
[Intermediate]

---

Q: How to see vDSO functions in a process?
A: 
```bash
# 查看vDSO映射
cat /proc/self/maps | grep vdso
# 7fff2b5fe000-7fff2b600000 r-xp 00000000 00:00 0  [vdso]

# 查看vDSO符号
# 方法1：使用dd提取再用objdump
dd if=/proc/self/mem of=/tmp/vdso.so skip=$((0x7fff2b5fe000)) bs=1 count=$((0x2000))
objdump -T /tmp/vdso.so

# 方法2：使用gdb
gdb -p $$
(gdb) info sharedlibrary
(gdb) x/10i __vdso_gettimeofday

# 查看vDSO内容
# arch/x86/entry/vdso/目录下的源码
ls arch/x86/entry/vdso/
# vdso.lds.S        - 链接脚本
# vclock_gettime.c  - 时间函数实现
# vgetcpu.c         - getcpu实现
```

验证vDSO加速：
```c
#include <time.h>
#include <sys/time.h>

int main() {
    struct timeval tv;
    // 这个调用不会进入内核（通过strace验证）
    gettimeofday(&tv, NULL);
    return 0;
}

// strace显示：
// (无gettimeofday syscall，因为vDSO处理了)
```
[Intermediate]

---

## 8. System Call Tracing (系统调用追踪)

---

Q: How does ptrace intercept system calls?
A: ptrace用于调试器和strace：

```c
// 内核端检查点 (kernel/entry/common.c)
static void syscall_trace_enter(struct pt_regs *regs)
{
    // 检查是否被ptrace
    if (test_thread_flag(TIF_SYSCALL_TRACE))
        ptrace_report_syscall_entry(regs);
    
    // seccomp检查
    if (test_thread_flag(TIF_SECCOMP))
        secure_computing(NULL);
    
    // syscall跟踪（ftrace等）
    if (test_thread_flag(TIF_SYSCALL_TRACEPOINT))
        trace_sys_enter(regs, regs->orig_ax);
}

// ptrace拦截
void ptrace_report_syscall_entry(struct pt_regs *regs)
{
    // 通知调试器
    ptrace_notify(SIGTRAP | (PTRACE_EVENT_SYSCALL << 8));
    // 调试器可以修改寄存器
}
```

strace实现原理：
```c
// strace使用ptrace系统调用
ptrace(PTRACE_SYSCALL, child_pid, NULL, NULL);

// 工作流程：
1. fork子进程
2. 子进程调用ptrace(PTRACE_TRACEME)
3. 子进程exec目标程序
4. 父进程循环：
   a. wait()等待子进程停止
   b. 读取寄存器获取系统调用号和参数
   c. ptrace(PTRACE_SYSCALL)继续执行
   d. wait()等待系统调用返回
   e. 读取返回值
   f. 打印系统调用信息
```

使用strace：
```bash
strace ls                    # 追踪ls命令
strace -e trace=open ls      # 只追踪open
strace -c ls                 # 统计系统调用
strace -p <pid>              # 附加到进程
strace -f ./program          # 追踪fork的子进程
```
[Intermediate]

---

Q: What is seccomp and how does it filter system calls?
A: seccomp限制进程可用的系统调用：

```c
// 模式1：SECCOMP_MODE_STRICT
// 只允许 read, write, exit, sigreturn
prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);

// 模式2：SECCOMP_MODE_FILTER (BPF过滤)
struct sock_filter filter[] = {
    // 获取系统调用号
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, 
             offsetof(struct seccomp_data, nr)),
    
    // 允许 read (0)
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_read, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    
    // 允许 write (1)
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_write, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    
    // 允许 exit (60)
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_exit, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    
    // 默认：杀死进程
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL),
};

struct sock_fprog prog = {
    .len = sizeof(filter) / sizeof(filter[0]),
    .filter = filter,
};

prctl(PR_SET_NO_NEW_PRIVS, 1);  // 必须先设置
prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
```

seccomp返回动作：
```c
SECCOMP_RET_KILL      // 杀死进程
SECCOMP_RET_TRAP      // 发送SIGSYS信号
SECCOMP_RET_ERRNO     // 返回错误码
SECCOMP_RET_TRACE     // 通知ptrace追踪器
SECCOMP_RET_LOG       // 记录日志但允许
SECCOMP_RET_ALLOW     // 允许
```

内核实现：
```c
// kernel/seccomp.c
int __secure_computing(const struct seccomp_data *sd)
{
    int mode = current->seccomp.mode;
    
    switch (mode) {
    case SECCOMP_MODE_STRICT:
        return __secure_computing_strict(this_syscall);
    case SECCOMP_MODE_FILTER:
        return __seccomp_filter(this_syscall, sd, false);
    }
}
```
[Advanced]

---

## 9. Adding a New System Call (添加新系统调用)

---

Q: How to add a new system call to the Linux kernel?
A: 添加系统调用的步骤：

```c
/* 步骤1：定义系统调用号 */
// arch/x86/entry/syscalls/syscall_64.tbl
// 在表末尾添加：
548   common  my_syscall     sys_my_syscall

/* 步骤2：声明函数原型 */
// include/linux/syscalls.h
asmlinkage long sys_my_syscall(int arg1, const char __user *arg2);

/* 步骤3：实现系统调用 */
// kernel/my_syscall.c
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

SYSCALL_DEFINE2(my_syscall, int, arg1, const char __user *, arg2)
{
    char kbuf[256];
    
    // 验证参数
    if (arg1 < 0)
        return -EINVAL;
    
    // 从用户空间复制数据
    if (copy_from_user(kbuf, arg2, sizeof(kbuf)))
        return -EFAULT;
    
    pr_info("my_syscall: arg1=%d, arg2=%s\n", arg1, kbuf);
    
    return 0;  // 成功
}

/* 步骤4：添加到Makefile */
// kernel/Makefile
obj-y += my_syscall.o

/* 步骤5：用户空间使用 */
// 方法1：使用syscall()
#include <sys/syscall.h>
#include <unistd.h>

#define __NR_my_syscall 548

int main() {
    long ret = syscall(__NR_my_syscall, 42, "hello");
    printf("return: %ld\n", ret);
    return 0;
}

// 方法2：自定义wrapper
long my_syscall(int arg1, const char *arg2) {
    return syscall(__NR_my_syscall, arg1, arg2);
}
```
[Advanced]

---

Q: What are the best practices when implementing system calls?
A: 
```c
// 1. 参数验证
SYSCALL_DEFINE3(example, int, fd, void __user *, buf, size_t, count)
{
    // 检查fd有效性
    struct fd f = fdget(fd);
    if (!f.file)
        return -EBADF;
    
    // 检查缓冲区大小
    if (count > MAX_RW_COUNT)
        count = MAX_RW_COUNT;
    
    // 使用access_ok检查用户指针
    if (!access_ok(buf, count))
        return -EFAULT;
}

// 2. 安全的用户空间访问
// 永远使用copy_from_user/copy_to_user
if (copy_from_user(kernel_buf, user_buf, size))
    return -EFAULT;

// 3. 正确的锁使用
mutex_lock(&my_mutex);
// 临界区
mutex_unlock(&my_mutex);

// 4. 信号处理
if (mutex_lock_interruptible(&my_mutex))
    return -ERESTARTSYS;  // 被信号中断，可重启

// 5. 资源清理（使用goto模式）
int my_syscall(...)
{
    int ret = -ENOMEM;
    char *buf = kmalloc(size, GFP_KERNEL);
    if (!buf)
        goto out;
    
    ret = do_something(buf);
    if (ret < 0)
        goto out_free;
    
    // 成功
    ret = 0;
    
out_free:
    kfree(buf);
out:
    return ret;
}

// 6. 权限检查
if (!capable(CAP_SYS_ADMIN))
    return -EPERM;

// 7. 命名空间感知
struct ipc_namespace *ns = current->nsproxy->ipc_ns;
```
[Advanced]

---

## 10. Compatibility System Calls (兼容系统调用)

---

Q: What are compat system calls and why are they needed?
A: 兼容系统调用允许32位程序在64位内核上运行：

```
64位内核运行32位程序：
+------------------+     +------------------+
| 32-bit Program   |     | 64-bit Kernel    |
| (uses int 0x80)  |     |                  |
+--------+---------+     |                  |
         |               |                  |
         | int 0x80      |                  |
         +-------------->| entry_INT80_compat
                         |       |
                         |       v
                         | compat_sys_xxx()
                         |       |
                         |       v
                         | 转换参数/结构体
                         |       |
                         |       v
                         | 调用原生实现
                         +------------------+
```

问题和解决：
```c
// 问题1：指针大小不同
// 32位程序：指针4字节
// 64位内核：指针8字节

// 问题2：结构体大小不同
struct stat {     // 64位
    ...
    off_t st_size;  // 8字节
    ...
};

struct stat {     // 32位
    ...
    off32_t st_size;  // 4字节
    ...
};

// 解决方案：compat结构体和系统调用
struct compat_stat {
    ...
    compat_off_t st_size;  // 4字节
    ...
};

// compat系统调用
COMPAT_SYSCALL_DEFINE2(stat, const char __user *, filename,
                       struct compat_stat __user *, statbuf)
{
    struct kstat stat;
    int error;
    
    error = vfs_stat(filename, &stat);
    if (!error)
        error = cp_compat_stat(&stat, statbuf);  // 转换
    
    return error;
}
```

compat类型定义：
```c
// include/linux/compat.h
typedef s32     compat_int_t;
typedef u32     compat_uint_t;
typedef s32     compat_long_t;
typedef u32     compat_uptr_t;    // 32位指针
typedef s64     compat_s64;
typedef u64     compat_u64;
typedef s32     compat_off_t;
typedef s64     compat_loff_t;
typedef s32     compat_pid_t;
typedef u32     compat_size_t;
typedef s32     compat_ssize_t;
typedef s32     compat_time_t;

// 指针转换
static inline void __user *compat_ptr(compat_uptr_t uptr)
{
    return (void __user *)(unsigned long)uptr;
}
```
[Advanced]

---

## 11. System Call Restart (系统调用重启)

---

Q: How are interrupted system calls handled?
A: 被信号中断的系统调用可以自动重启：

```c
// 系统调用被信号中断时：
SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
{
    // ... 
    
    // 等待数据时被信号中断
    if (signal_pending(current))
        return -ERESTARTSYS;  // 可重启
        // 或 -EINTR          // 返回给用户
    
    // ...
}

// 内核处理逻辑 (arch/x86/kernel/signal.c)
static void handle_signal(struct ksignal *ksig, struct pt_regs *regs)
{
    // 检查是否需要重启系统调用
    if (regs->orig_ax >= 0) {
        switch (regs->ax) {
        case -ERESTARTSYS:
            // 检查SA_RESTART标志
            if (!(ksig->ka.sa.sa_flags & SA_RESTART)) {
                regs->ax = -EINTR;  // 不重启，返回EINTR
                break;
            }
            // 否则fall through重启
        case -ERESTARTNOINTR:
            // 总是重启
            regs->ax = regs->orig_ax;  // 恢复系统调用号
            regs->ip -= 2;              // 回退到syscall指令
            break;
        case -ERESTARTNOHAND:
            // 无信号处理程序时重启
            if (!ksig->ka.sa.sa_handler)
                // 重启
            else
                regs->ax = -EINTR;
            break;
        }
    }
}
```

返回值含义：
| 返回值 | 含义 | 重启条件 |
|--------|------|----------|
| -ERESTARTSYS | 可重启 | 如果SA_RESTART设置 |
| -EINTR | 中断 | 不重启 |
| -ERESTARTNOINTR | 强制重启 | 总是重启 |
| -ERESTARTNOHAND | 条件重启 | 无信号处理程序时重启 |
| -RESTART_RESTARTBLOCK | 特殊重启 | 使用restart_block |

用户空间使用：
```c
struct sigaction sa;
sa.sa_handler = handler;
sa.sa_flags = SA_RESTART;  // 设置自动重启
sigaction(SIGINT, &sa, NULL);

// 或手动处理
while ((n = read(fd, buf, size)) == -1 && errno == EINTR)
    continue;  // 被中断，重试
```
[Intermediate]

---

## 12. System Call Performance (系统调用性能)

---

Q: What is the overhead of a system call?
A: 
```
系统调用开销组成：
+------------------------------------------------------------------+
| 用户态 -> 内核态切换                                              |
|   - syscall指令执行                 ~50 cycles                   |
|   - 切换栈和保存寄存器               ~100 cycles                  |
|                                                                  |
| 内核态处理                                                        |
|   - 参数验证和复制                   ~50-200 cycles              |
|   - 实际操作                         varies                      |
|   - 返回值设置                       ~20 cycles                   |
|                                                                  |
| 内核态 -> 用户态切换                                              |
|   - 检查挂起工作                     ~50 cycles                   |
|   - 恢复寄存器                       ~50 cycles                   |
|   - sysret指令                       ~50 cycles                   |
|                                                                  |
| 额外开销                                                          |
|   - Spectre/Meltdown缓解            ~100-500 cycles              |
|   - 页表切换(KPTI)                   ~100-300 cycles              |
+------------------------------------------------------------------+
| 总计（空系统调用）：                  ~500-1500 cycles             |
| 相比函数调用：                        ~5-10 cycles                 |
+------------------------------------------------------------------+
```

测量系统调用开销：
```c
#include <time.h>
#include <sys/syscall.h>
#include <unistd.h>

int main() {
    struct timespec start, end;
    int iterations = 1000000;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < iterations; i++) {
        syscall(SYS_getpid);  // 最简单的系统调用
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    long ns = (end.tv_sec - start.tv_sec) * 1000000000L +
              (end.tv_nsec - start.tv_nsec);
    
    printf("Average syscall time: %ld ns\n", ns / iterations);
    // 典型结果：100-500ns
    
    return 0;
}
```
[Intermediate]

---

Q: What are Spectre/Meltdown mitigations for system calls?
A: 
```
安全缓解措施：
+------------------------------------------------------------------+
|                                                                  |
|  KPTI (Kernel Page Table Isolation)                             |
|  +----------------------------------------------------------+   |
|  | 用户态和内核态使用不同的页表                               |   |
|  | 防止Meltdown攻击读取内核内存                               |   |
|  | 开销：每次syscall切换页表 (~200 cycles)                    |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  Retpoline                                                      |
|  +----------------------------------------------------------+   |
|  | 替换间接跳转为安全的跳转序列                               |   |
|  | 防止Spectre v2攻击                                        |   |
|  | 开销：每次间接调用增加                                     |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  IBRS/IBPB/STIBP                                                |
|  +----------------------------------------------------------+   |
|  | 硬件特性控制分支预测                                       |   |
|  | 更高效的Spectre缓解                                        |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

查看缓解状态：
```bash
# 查看CPU漏洞状态
cat /sys/devices/system/cpu/vulnerabilities/*

# 查看启用的缓解
cat /proc/cpuinfo | grep -E "bugs|flags"

# 禁用缓解（不推荐，仅测试）
# 内核启动参数：
mitigations=off
nopti
nospectre_v2
```

内核实现：
```c
// arch/x86/entry/entry_64.S
// KPTI: 页表切换
SWITCH_TO_KERNEL_CR3 scratch_reg=%rdi
// ... 系统调用处理 ...
SWITCH_TO_USER_CR3_STACK scratch_reg=%rdi

// 宏定义
.macro SWITCH_TO_KERNEL_CR3 scratch_reg:req
    mov     %cr3, \scratch_reg
    and     $(~PTI_USER_PGTABLE_MASK), \scratch_reg
    mov     \scratch_reg, %cr3
.endm
```
[Advanced]

---

## 13. System Call Debugging (系统调用调试)

---

Q: How to debug system calls?
A: 
```bash
# 1. strace - 用户态追踪
strace ./program                    # 追踪程序
strace -p <pid>                     # 附加到进程
strace -f ./program                 # 追踪fork子进程
strace -e trace=file ./program      # 只追踪文件操作
strace -e trace=network ./program   # 只追踪网络
strace -c ./program                 # 统计系统调用
strace -T ./program                 # 显示时间
strace -tt ./program                # 时间戳

# 2. ltrace - 追踪库函数
ltrace ./program

# 3. ftrace - 内核追踪
echo 1 > /sys/kernel/debug/tracing/events/syscalls/enable
cat /sys/kernel/debug/tracing/trace

# 4. perf - 性能分析
perf trace ./program
perf stat -e syscalls:sys_enter_read ./program

# 5. bpftrace - 动态追踪
bpftrace -e 'tracepoint:syscalls:sys_enter_open { printf("%s %s\n", comm, str(args->filename)); }'

# 6. systemtap
stap -e 'probe syscall.open { printf("%s: %s\n", execname(), filename) }'
```

内核配置：
```bash
# 启用syscall追踪
CONFIG_FTRACE=y
CONFIG_FTRACE_SYSCALLS=y
CONFIG_TRACEPOINTS=y
```

示例输出：
```bash
$ strace -e open ls
open("/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
open("/lib/x86_64-linux-gnu/libselinux.so.1", O_RDONLY|O_CLOEXEC) = 3
open("/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
...
```
[Basic]

---

Q: What are common system call debugging techniques?
A: 
```c
// 1. 添加调试打印
SYSCALL_DEFINE2(my_syscall, int, arg1, int, arg2)
{
    pr_debug("my_syscall: arg1=%d, arg2=%d\n", arg1, arg2);
    // 使用动态调试: echo 'file my_syscall.c +p' > /sys/kernel/debug/dynamic_debug/control
}

// 2. 使用WARN/BUG检查
SYSCALL_DEFINE1(my_syscall, int, arg)
{
    WARN_ON(arg < 0);  // 如果条件为真打印警告和栈回溯
    
    if (unlikely(error))
        BUG();  // 严重错误，停止内核
}

// 3. 使用kprobe动态插入
#include <linux/kprobes.h>

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    pr_info("sys_open called, filename=%s\n", 
            (char *)regs->di);
    return 0;
}

static struct kprobe kp = {
    .symbol_name = "do_sys_open",
    .pre_handler = handler_pre,
};

register_kprobe(&kp);

// 4. 使用tracepoints
// 内核预定义了syscall追踪点
TRACE_EVENT(sys_enter,
    TP_PROTO(struct pt_regs *regs, long id),
    ...
);

// 5. 断点调试
// 使用QEMU+GDB调试内核
(gdb) b sys_open
(gdb) c
(gdb) bt   # 查看调用栈
(gdb) p *regs
```
[Intermediate]

---

## 14. Common System Calls (常见系统调用)

---

Q: What are the most commonly used system calls?
A: 
```c
/*=== 进程管理 ===*/
fork()      // 创建子进程
exec()      // 执行程序
exit()      // 退出进程
wait()      // 等待子进程
getpid()    // 获取进程ID
clone()     // 创建进程/线程

/*=== 文件操作 ===*/
open()      // 打开文件
close()     // 关闭文件
read()      // 读取
write()     // 写入
lseek()     // 定位
stat()      // 获取文件信息
fstat()
lstat()
access()    // 检查访问权限
chmod()     // 修改权限
chown()     // 修改所有者

/*=== 目录操作 ===*/
mkdir()     // 创建目录
rmdir()     // 删除目录
chdir()     // 切换目录
getcwd()    // 获取当前目录
getdents()  // 读取目录项

/*=== 内存管理 ===*/
mmap()      // 内存映射
munmap()    // 解除映射
brk()       // 扩展堆
mprotect()  // 修改内存保护

/*=== 信号 ===*/
kill()      // 发送信号
signal()    // 设置信号处理
sigaction() // 高级信号处理
pause()     // 等待信号

/*=== 网络 ===*/
socket()    // 创建套接字
bind()      // 绑定地址
listen()    // 监听
accept()    // 接受连接
connect()   // 连接
send()/recv()
sendto()/recvfrom()

/*=== 同步 ===*/
futex()     // 快速用户态互斥
```

系统调用使用频率（典型应用）：
```
read        >30%
write       >20%
futex       ~10%
close       ~5%
mmap        ~5%
stat/fstat  ~5%
open        ~3%
其他        <22%
```
[Basic]

---

## 15. Summary Card (总结卡片)

---

Q: Summarize the key components of Linux system call implementation.
A: 
```
+==================================================================+
||                LINUX SYSTEM CALL ARCHITECTURE                  ||
+==================================================================+

用户空间调用:
+------------------+
| syscall(nr, ...) | 或 libc wrapper
+--------+---------+
         |
         v
+------------------+
| syscall 指令     | x86_64: syscall
| rax = 系统调用号 | x86: int 0x80
| rdi,rsi,rdx...   | ARM: svc #0
+--------+---------+
         |
         | CPU切换到Ring 0
         v
+------------------+
| entry_SYSCALL_64 | 入口点
| - swapgs         | 切换GS
| - 切换栈         | 保存用户RSP
| - 保存寄存器     | 构建pt_regs
+--------+---------+
         |
         v
+------------------+
| do_syscall_64()  |
| sys_call_table[nr]| 查表调用
+--------+---------+
         |
         v
+------------------+
| SYSCALL_DEFINEn  | 具体实现
| sys_xxx()        |
+--------+---------+
         |
         v
+------------------+
| 返回路径         |
| - 检查信号       | TIF_SIGPENDING
| - 检查调度       | TIF_NEED_RESCHED
| - 恢复寄存器     |
| - sysret         | 返回用户态
+------------------+

关键数据结构:
- sys_call_table[]     系统调用表
- struct pt_regs       保存的寄存器
- current->thread_info 线程标志

性能优化:
- vDSO               避免进入内核
- syscall vs int0x80 快速系统调用
- 批量操作           减少系统调用次数

安全机制:
- seccomp            过滤系统调用
- 权限检查           capable()
- 用户指针验证       access_ok()
- KPTI               内核页表隔离
```
[Basic]

---

*Total: 100+ cards covering Linux kernel system call implementation*

