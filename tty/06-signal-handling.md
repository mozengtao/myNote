# Ctrl+C 信号处理链路详解

## 🎯 学习目标
深入理解从按下Ctrl+C到进程收到SIGINT信号的完整内核路径，掌握终端信号生成和传递机制。

---

## 📊 Ctrl+C 完整处理链路图

```
完整信号处理链路:
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Hardware Layer                                │
│                                                                             │
│  ┌─────────────┐    键盘中断    ┌─────────────────────────────────────────┐   │
│  │  Keyboard   │────IRQ────────▶│           Keyboard Driver              │   │
│  │    ^C       │               │       keyboard_interrupt()             │   │
│  │  (0x03)     │               │              │                         │   │
│  └─────────────┘               │              ▼                         │   │
│                                │     ┌─────────────────┐                 │   │
│                                │     │   Scancode →    │                 │   │
│                                │     │   ASCII (^C)    │                 │   │
│                                │     └─────────────────┘                 │   │
└─────────────────────────────────────────────────────────────────────────────┘
                                │              │
                                │              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                               TTY Layer                                    │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                        TTY Driver                                   │   │
│  │  tty_insert_flip_char(tty, 0x03, TTY_NORMAL)                       │   │
│  │                            │                                        │   │
│  │                            ▼                                        │   │
│  │  tty_flip_buffer_push() ──────────────────────────────────────────┐ │   │
│  └─────────────────────────────────────────────────────────────────────┘ │   │
│                                                                          │   │
│  ┌─────────────────────────────────────────────────────────────────────┐ │   │
│  │                    Line Discipline (N_TTY)                         │ │   │
│  │                                                                     │ │   │
│  │  n_tty_receive_buf_common()                                        │ │   │
│  │              │                                                      │ │   │
│  │              ▼                                                      │ │   │
│  │  n_tty_receive_char_special(tty, 0x03)                            │ │   │
│  │              │                                                      │ │   │
│  │              ▼                                                      │ │   │
│  │  ┌─────────────────────────────────────────────────────────────┐   │ │   │
│  │  │             字符检查与处理                                   │   │ │   │
│  │  │                                                             │   │ │   │
│  │  │  if (L_ISIG(tty)) {                                         │   │ │   │
│  │  │      if (c == INTR_CHAR(tty)) {  // ^C                     │   │ │   │
│  │  │          n_tty_receive_signal_char(tty, SIGINT, c);         │   │ │   │
│  │  │          return;                                            │   │ │   │
│  │  │      }                                                      │   │ │   │
│  │  │  }                                                          │   │ │   │
│  │  └─────────────────────────────────────────────────────────────┘   │ │   │
│  └─────────────────────────────────────────────────────────────────────┘ │   │
│                                ▲                                        │   │
│                                └────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                            Signal Layer                                    │
│                                                                             │
│  n_tty_receive_signal_char(tty, SIGINT, '^C')                             │
│              │                                                              │
│              ▼                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    信号生成和发送                                   │   │
│  │                                                                     │   │
│  │  1. process_echoes(tty);           // 处理echo                     │   │
│  │  2. if (L_ECHO(tty)) echo_char(c); // 显示 ^C                      │   │
│  │  3. isig_handler(tty, SIGINT);     // 信号处理                     │   │
│  │              │                                                      │   │
│  │              ▼                                                      │   │
│  │     ┌─────────────────────────────────────────┐                    │   │
│  │     │  获取前台进程组 (Foreground PGID)        │                    │   │
│  │     │  pgrp = tty->pgrp;                      │                    │   │
│  │     │             │                            │                    │   │
│  │     │             ▼                            │                    │   │
│  │     │  kill_pgrp(pgrp, SIGINT, 1);            │                    │   │
│  │     └─────────────────────────────────────────┘                    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Process Layer                                     │
│                                                                             │
│  kill_pgrp(pgrp, SIGINT, 1)                                                │
│              │                                                              │
│              ▼                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │            进程组信号传递                                           │   │
│  │                                                                     │   │
│  │  do_each_pid_task(pgrp, PIDTYPE_PGID, p) {                         │   │
│  │      group_send_sig_info(SIGINT, info, p, PIDTYPE_PGID);           │   │
│  │  }                                                                  │   │
│  │              │                                                      │   │
│  │              ▼                                                      │   │
│  │  ┌─────────────────────────────────────────┐                       │   │
│  │  │         每个进程接收信号                 │                       │   │
│  │  │                                         │                       │   │
│  │  │ 1. 检查信号权限                         │                       │   │
│  │  │ 2. 检查信号掩码 (SIG_BLOCK)             │                       │   │
│  │  │ 3. 添加到pending信号队列                │                       │   │
│  │  │ 4. 唤醒目标进程 (signal_wake_up)        │                       │   │
│  │  │ 5. 进程调度器处理信号                   │                       │   │
│  │  └─────────────────────────────────────────┘                       │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           User Process                                     │
│                                                                             │
│  进程从内核态返回用户态时:                                                   │
│                                                                             │
│  1. do_signal() 检查待处理信号                                              │
│  2. get_signal() 获取下一个信号                                             │
│  3. 执行信号处理程序或默认动作:                                              │
│                                                                             │
│     ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐      │
│     │   SIG_DFL       │    │   SIG_IGN       │    │  Custom Handler │      │
│     │   (默认动作)     │    │   (忽略信号)     │    │  (用户处理函数)  │      │
│     │                 │    │                 │    │                 │      │
│     │ • SIGINT →      │    │ • 什么都不做     │    │ • 执行用户代码   │      │
│     │   进程终止       │    │                 │    │ • 可恢复执行     │      │
│     │ • SIGKILL →     │    │                 │    │                 │      │
│     │   强制终止       │    │                 │    │                 │      │
│     │ • SIGTSTP →     │    │                 │    │                 │      │
│     │   进程停止       │    │                 │    │                 │      │
│     └─────────────────┘    └─────────────────┘    └─────────────────┘      │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 🏗️ 核心数据结构和函数

### 信号处理相关结构

```c
// include/linux/sched.h - 进程信号信息
struct task_struct {
    /* 信号处理 */
    struct signal_struct *signal;           // 共享信号信息
    struct sighand_struct *sighand;         // 信号处理函数
    sigset_t blocked;                       // 被阻塞的信号集
    sigset_t real_blocked;                  // 真实阻塞信号集
    struct sigpending pending;              // 待处理信号队列
    // ...
};

// include/linux/sched/signal.h - 信号队列
struct sigpending {
    struct list_head list;                  // 信号队列链表
    sigset_t signal;                        // 信号位图
};

struct sigqueue {
    struct list_head list;                  // 链表节点
    int flags;                              // 标志
    kernel_siginfo_t info;                  // 信号信息
    struct user_struct *user;               // 发送信号的用户
};

// 内核信号信息
typedef struct kernel_siginfo {
    int si_signo;                           // 信号编号
    int si_errno;                           // 错误码
    int si_code;                            // 信号代码
    
    union {
        int _pad[SI_PAD_SIZE];
        
        /* kill() */
        struct {
            pid_t _pid;                     // 发送进程PID
            uid_t _uid;                     // 发送进程UID
        } _kill;
        
        /* 终端相关 */
        struct {
            pid_t _pid;                     // 发送进程PID
            uid_t _uid;                     // 发送进程UID
            int _status;                    // 退出状态
            clock_t _utime;
            clock_t _stime;
        } _sigchld;
        // ...
    } _sifields;
} kernel_siginfo_t;
```

### TTY信号生成函数

```c
// drivers/tty/n_tty.c - 信号字符处理
static void n_tty_receive_signal_char(struct tty_struct *tty, int signal, unsigned char c)
{
    struct n_tty_data *ldata = tty->disc_data;
    
    /* 首先处理echo */
    if (L_ECHO(tty)) {
        echo_char(c, tty);
        commit_echoes(tty);
    } else {
        process_echoes(tty);
    }
    
    /* 如果启用了流控制，重新开始输出 */
    if (I_IXON(tty))
        start_tty(tty);
    
    /* 调用信号处理 */
    isig(signal, tty);
}

// drivers/tty/tty_io.c - 信号处理核心函数
static void isig(int sig, struct tty_struct *tty)
{
    struct pid *pgrp;
    
    if (!tty->session)
        return;
        
    /* 获取前台进程组 */
    pgrp = tty->pgrp;
    if (pgrp) {
        /* 发送信号给整个进程组 */
        kill_pgrp(pgrp, sig, 1);
    }
    
    /* 刷新输入缓冲区 (除非设置了NOFLSH) */
    if (!L_NOFLSH(tty)) {
        /* 清空输入缓冲区 */
        n_tty_flush_buffer(tty);
        /* 清空输出缓冲区 */
        if (tty->driver->flush_buffer)
            tty->driver->flush_buffer(tty);
    }
}

// 具体的信号字符处理函数
static void n_tty_receive_char_special(struct tty_struct *tty, unsigned char c, char flag)
{
    struct n_tty_data *ldata = tty->disc_data;
    
    /* 检查是否启用信号处理 */
    if (L_ISIG(tty)) {
        if (c == INTR_CHAR(tty)) {                 // ^C
            n_tty_receive_signal_char(tty, SIGINT, c);
            return;
        }
        if (c == QUIT_CHAR(tty)) {                 // ^\
            n_tty_receive_signal_char(tty, SIGQUIT, c);  
            return;
        }
        if (c == SUSP_CHAR(tty)) {                 // ^Z
            n_tty_receive_signal_char(tty, SIGTSTP, c);
            return;
        }
    }
    
    /* 其他字符处理... */
}
```

### 信号发送到进程组

```c
// kernel/signal.c - 向进程组发送信号
int kill_pgrp(struct pid *pid, int sig, int priv)
{
    int ret;
    
    read_lock(&tasklist_lock);
    ret = __kill_pgrp_info(sig, __si_special(priv), pid);
    read_unlock(&tasklist_lock);
    
    return ret;
}

static int __kill_pgrp_info(int sig, struct kernel_siginfo *info, struct pid *pgrp)
{
    struct task_struct *p = NULL;
    int retval, success;
    
    success = 0;
    retval = -ESRCH;
    
    /* 遍历进程组中的每个进程 */
    do_each_pid_task(pgrp, PIDTYPE_PGID, p) {
        int err = group_send_sig_info(sig, info, p, PIDTYPE_PGID);
        success |= !err;
        retval = err;
    } while_each_pid_task(pgrp, PIDTYPE_PGID, p);
    
    return success ? 0 : retval;
}

// 向单个进程发送信号
static int group_send_sig_info(int sig, struct kernel_siginfo *info,
                              struct task_struct *p, enum pid_type type)
{
    int ret;
    
    rcu_read_lock();
    ret = check_kill_permission(sig, info, p);
    if (!ret && sig)
        ret = do_send_sig_info(sig, info, p, type);
    rcu_read_unlock();
    
    return ret;
}

static int do_send_sig_info(int sig, struct kernel_siginfo *info,
                           struct task_struct *p, enum pid_type type)
{
    unsigned long flags;
    int ret = -ESRCH;
    
    if (lock_task_sighand(p, &flags)) {
        ret = send_signal(sig, info, p, type);
        unlock_task_sighand(p, &flags);
    }
    
    return ret;
}
```

### 信号队列管理

```c
// kernel/signal.c - 信号发送的核心实现
static int send_signal(int sig, struct kernel_siginfo *info,
                      struct task_struct *t, enum pid_type type)
{
    int from_ancestor_ns = 0;
    struct sigpending *pending;
    struct sigqueue *q;
    int override_rlimit;
    int ret = 0, result;
    
    /* 检查信号是否被忽略 */
    if (!prepare_signal(sig, t, false))
        goto ret;
    
    /* 选择信号队列 (共享或私有) */
    pending = (type != PIDTYPE_PID) ? &t->signal->shared_pending : &t->pending;
    
    /* 检查是否已经有相同的信号排队 */
    if (legacy_queue(pending, sig))
        goto ret;
    
    /* 分配信号队列项 */
    q = __sigqueue_alloc(sig, t, GFP_ATOMIC, override_rlimit);
    if (q) {
        list_add_tail(&q->list, &pending->list);
        switch ((unsigned long) info) {
        case (unsigned long) SEND_SIG_NOINFO:
            clear_siginfo(&q->info);
            q->info.si_signo = sig;
            q->info.si_errno = 0;
            q->info.si_code = SI_KERNEL;
            q->info.si_pid = 0;
            q->info.si_uid = 0;
            break;
        case (unsigned long) SEND_SIG_PRIV:
            clear_siginfo(&q->info);
            q->info.si_signo = sig;
            q->info.si_errno = 0;
            q->info.si_code = SI_KERNEL;
            q->info.si_pid = 0;
            q->info.si_uid = 0;
            break;
        default:
            copy_siginfo(&q->info, info);
            break;
        }
    }
    
out_set:
    /* 设置信号位 */
    sigaddset(&pending->signal, sig);
    
    /* 完成信号发送 */
    complete_signal(sig, t, type);
ret:
    return ret;
}

// 完成信号发送 - 唤醒目标进程
static void complete_signal(int sig, struct task_struct *p, enum pid_type type)
{
    struct signal_struct *signal = p->signal;
    struct task_struct *t;
    
    /* 
     * 如果是终止信号且进程组有其他进程，
     * 确保整个进程组都收到信号
     */
    if (want_signal(sig, p)) {
        t = p;
        /* 
         * 选择一个合适的线程来处理信号
         * 优先选择当前运行的线程
         */
        if (!thread_group_empty(p)) {
            /* 遍历线程组，找到最适合的线程 */
            t = signal->curr_target;
            while (!want_signal(sig, t)) {
                t = next_thread(t);
                if (t == signal->curr_target)
                    break;
            }
        }
        signal->curr_target = t;
    }
    
    /* 如果是致命信号，设置组退出标志 */
    if (sig_fatal(p, sig) && 
        !(signal->flags & SIGNAL_GROUP_EXIT) &&
        !sigismember(&t->real_blocked, sig) &&
        (sig == SIGKILL || !t->ptrace)) {
        /*
         * 致命信号将杀死整个线程组
         */
        if (!sig_kernel_coredump(sig)) {
            signal->flags = SIGNAL_GROUP_EXIT;
            signal->group_exit_code = sig;
            signal->group_stop_count = 0;
            for (t = next_thread(p); t != p; t = next_thread(t)) {
                task_clear_jobctl_pending(t, JOBCTL_PENDING_MASK);
                set_tsk_thread_flag(t, TIF_SIGPENDING);
            }
        }
    }
    
    /* 唤醒目标进程 */
    signal_wake_up(t, sig == SIGKILL);
}
```

## 🔄 信号处理的完整流程

### 从接收到处理的时序

```
时序图 - Ctrl+C 信号处理:

Hardware    TTY Driver    Line Disc    Signal Kern    Process     User Space
    │            │            │             │           │            │
    │ 键盘中断     │            │             │           │            │
    ├─────────────▶            │             │           │            │
    │            │ 字符接收     │             │           │            │
    │            ├─────────────▶             │           │            │
    │            │            │ ^C检测       │           │            │
    │            │            ├─────────────▶           │            │
    │            │            │             │ 发送SIGINT │            │
    │            │            │             ├───────────▶            │
    │            │            │             │           │ 进程调度    │
    │            │            │             │           ├────────────▶
    │            │            │             │           │            │ 信号处理
    │            │            │             │           │            │ (默认/自定义)
    │            │            │             │           │            ├─ 进程终止
    │            │            │             │           │            │  或
    │            │            │             │           │            ├─ 执行处理函数
    │            │            │             │           │            │  后继续执行

时间轴说明:
T0: 用户按下Ctrl+C
T1: 键盘硬件产生中断 (~1us)
T2: TTY驱动处理字符 (~10us)  
T3: Line Discipline识别^C (~1us)
T4: 内核发送SIGINT给进程组 (~100us)
T5: 目标进程被调度执行 (~1ms)
T6: 进程处理信号 (用户定义)
```

### 信号字符配置和检查

```c
// termios中的信号字符配置
struct termios {
    // ...
    cc_t c_cc[NCCS];           // 控制字符数组
};

// 控制字符索引
#define VINTR    0             // ^C - 中断字符
#define VQUIT    1             // ^\ - 退出字符  
#define VSUSP    10            // ^Z - 挂起字符

// 获取控制字符的宏
#define INTR_CHAR(tty)    ((tty)->termios.c_cc[VINTR])
#define QUIT_CHAR(tty)    ((tty)->termios.c_cc[VQUIT])
#define SUSP_CHAR(tty)    ((tty)->termios.c_cc[VSUSP])

// 检查标志位的宏
#define L_ISIG(tty)       ((tty)->termios.c_lflag & ISIG)
#define L_ECHO(tty)       ((tty)->termios.c_lflag & ECHO)
#define L_NOFLSH(tty)     ((tty)->termios.c_lflag & NOFLSH)

// Line Discipline中的字符检查逻辑
static inline int is_signal_char(struct tty_struct *tty, unsigned char c)
{
    if (!L_ISIG(tty))
        return 0;
        
    return (c == INTR_CHAR(tty) ||    // ^C
            c == QUIT_CHAR(tty) ||    // ^\  
            c == SUSP_CHAR(tty));     // ^Z
}

// 获取对应的信号编号
static inline int char_to_signal(struct tty_struct *tty, unsigned char c)
{
    if (c == INTR_CHAR(tty))
        return SIGINT;
    else if (c == QUIT_CHAR(tty))
        return SIGQUIT;
    else if (c == SUSP_CHAR(tty))
        return SIGTSTP;
    else
        return 0;
}
```

## 🧪 最小可运行实验

### 实验1：跟踪Ctrl+C处理路径

```c
// signal_trace.c - 跟踪信号处理过程
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <string.h>

volatile sig_atomic_t signal_received = 0;
volatile sig_atomic_t signal_count = 0;

void detailed_signal_handler(int sig, siginfo_t *info, void *context) {
    signal_received = sig;
    signal_count++;
    
    /* 获取当前时间 */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    
    /* 信号处理函数中只能调用async-signal-safe函数 */
    char msg[256];
    int len = snprintf(msg, sizeof(msg),
        "\n[信号处理器 %ld.%09ld] 收到信号 %d\n"
        "  发送进程PID: %d\n"
        "  发送进程UID: %d\n" 
        "  信号代码: %d (%s)\n"
        "  处理次数: %d\n",
        ts.tv_sec, ts.tv_nsec, sig,
        info->si_pid, info->si_uid, info->si_code,
        (info->si_code == SI_KERNEL) ? "内核生成" :
        (info->si_code == SI_USER) ? "用户发送" : "其他",
        signal_count);
    
    write(STDOUT_FILENO, msg, len);
}

void setup_signal_handler() {
    struct sigaction sa;
    
    /* 设置详细的信号处理器 */
    sa.sa_sigaction = detailed_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART;  // 获取详细信号信息
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        exit(1);
    }
    
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        perror("sigaction SIGQUIT");
        exit(1);
    }
    
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("sigaction SIGTSTP"); 
        exit(1);
    }
}

void print_process_info() {
    printf("=== 进程信息 ===\n");
    printf("PID: %d\n", getpid());
    printf("PGID: %d\n", getpgrp());
    printf("SID: %d\n", getsid(0));
    
    /* 检查前台进程组 */
    pid_t fg_pgrp = tcgetpgrp(STDIN_FILENO);
    if (fg_pgrp != -1) {
        printf("前台进程组: %d\n", fg_pgrp);
        printf("在前台: %s\n", (fg_pgrp == getpgrp()) ? "是" : "否");
    }
    
    /* 显示termios设置 */
    printf("当前终端信号字符:\n");
    system("stty -a | grep -E '(intr|quit|susp)'");
}

void print_signal_mask() {
    sigset_t current_mask;
    
    if (sigprocmask(0, NULL, &current_mask) == 0) {
        printf("当前信号掩码:\n");
        printf("  SIGINT %s\n", sigismember(&current_mask, SIGINT) ? "被阻塞" : "未阻塞");
        printf("  SIGQUIT %s\n", sigismember(&current_mask, SIGQUIT) ? "被阻塞" : "未阻塞");
        printf("  SIGTSTP %s\n", sigismember(&current_mask, SIGTSTP) ? "被阻塞" : "未阻塞");
    }
}

int main() {
    printf("=== Ctrl+C 信号处理跟踪实验 ===\n");
    
    print_process_info();
    print_signal_mask();
    setup_signal_handler();
    
    printf("\n开始监听信号...\n");
    printf("测试方法:\n");
    printf("  1. 按 Ctrl+C (SIGINT)\n");
    printf("  2. 按 Ctrl+\\ (SIGQUIT)\n"); 
    printf("  3. 按 Ctrl+Z (SIGTSTP)\n");
    printf("  4. 输入 'quit' 退出程序\n\n");
    
    char input[100];
    while (1) {
        printf("输入命令 (或按信号键): ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin)) {
            if (strncmp(input, "quit", 4) == 0) {
                break;
            }
            printf("收到输入: %s", input);
        } else {
            if (errno == EINTR) {
                printf("输入被信号中断\n");
                errno = 0;
            }
        }
        
        if (signal_received) {
            printf("信号处理完成，继续执行...\n");
            signal_received = 0;
        }
    }
    
    printf("程序正常退出，共处理 %d 个信号\n", signal_count);
    return 0;
}
```

### 实验2：信号字符自定义实验

```c
// custom_signal_chars.c - 自定义信号字符实验
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

struct termios original_termios;

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
    printf("\n终端设置已恢复\n");
}

void signal_handler(int sig) {
    const char *sig_name = 
        (sig == SIGINT) ? "SIGINT" :
        (sig == SIGQUIT) ? "SIGQUIT" :
        (sig == SIGTSTP) ? "SIGTSTP" : "UNKNOWN";
        
    printf("\n[收到信号] %s (%d)\n", sig_name, sig);
    
    if (sig == SIGQUIT) {
        restore_terminal();
        exit(0);
    }
}

void print_signal_chars(struct termios *t) {
    printf("当前信号字符配置:\n");
    printf("  INTR (中断): ");
    if (t->c_cc[VINTR] == _POSIX_VDISABLE) {
        printf("禁用\n");
    } else if (iscntrl(t->c_cc[VINTR])) {
        printf("^%c (0x%02x)\n", t->c_cc[VINTR] + '@', t->c_cc[VINTR]);
    } else {
        printf("'%c' (0x%02x)\n", t->c_cc[VINTR], t->c_cc[VINTR]);
    }
    
    printf("  QUIT (退出): ");
    if (t->c_cc[VQUIT] == _POSIX_VDISABLE) {
        printf("禁用\n");
    } else if (iscntrl(t->c_cc[VQUIT])) {
        printf("^%c (0x%02x)\n", t->c_cc[VQUIT] + '@', t->c_cc[VQUIT]);
    } else {
        printf("'%c' (0x%02x)\n", t->c_cc[VQUIT], t->c_cc[VQUIT]);
    }
    
    printf("  SUSP (挂起): ");
    if (t->c_cc[VSUSP] == _POSIX_VDISABLE) {
        printf("禁用\n");
    } else if (iscntrl(t->c_cc[VSUSP])) {
        printf("^%c (0x%02x)\n", t->c_cc[VSUSP] + '@', t->c_cc[VSUSP]);
    } else {
        printf("'%c' (0x%02x)\n", t->c_cc[VSUSP], t->c_cc[VSUSP]);
    }
    
    printf("  ISIG标志: %s\n", (t->c_lflag & ISIG) ? "开启" : "关闭");
}

void test_custom_signal_chars() {
    struct termios new_termios;
    
    printf("\n=== 测试1: 修改中断字符为 Ctrl+X ===\n");
    
    tcgetattr(STDIN_FILENO, &new_termios);
    new_termios.c_cc[VINTR] = 0x18;  // Ctrl+X (ASCII 24)
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
    
    print_signal_chars(&new_termios);
    printf("现在按 Ctrl+X 来中断，Ctrl+C 无效\n");
    printf("按 Enter 继续下一个测试: ");
    getchar();
    
    printf("\n=== 测试2: 禁用信号字符处理 ===\n");
    
    new_termios.c_lflag &= ~ISIG;  // 关闭信号处理
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
    
    print_signal_chars(&new_termios);
    printf("现在所有信号字符都被禁用，按任何键都不会产生信号\n");
    printf("输入 'next' 继续: ");
    char buffer[100];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        printf("接收到: %s", buffer);
    }
    
    printf("\n=== 测试3: 恢复默认设置并禁用特定字符 ===\n");
    
    tcgetattr(STDIN_FILENO, &new_termios);
    new_termios.c_lflag |= ISIG;           // 重新开启信号处理
    new_termios.c_cc[VINTR] = 0x03;        // 恢复 Ctrl+C  
    new_termios.c_cc[VQUIT] = 0x1C;        // 恢复 Ctrl+\
    new_termios.c_cc[VSUSP] = _POSIX_VDISABLE; // 禁用 Ctrl+Z
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
    
    print_signal_chars(&new_termios);
    printf("现在 Ctrl+C 和 Ctrl+\\ 有效，但 Ctrl+Z 被禁用\n");
    printf("按 Ctrl+\\ 退出程序\n");
}

int main() {
    printf("=== 自定义信号字符实验 ===\n");
    
    /* 保存原始终端设置 */
    tcgetattr(STDIN_FILENO, &original_termios);
    atexit(restore_terminal);
    
    /* 设置信号处理器 */
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);  
    signal(SIGTSTP, signal_handler);
    
    printf("原始配置:\n");
    print_signal_chars(&original_termios);
    
    test_custom_signal_chars();
    
    /* 主循环 */
    printf("\n等待信号...\n");
    while (1) {
        pause();  // 等待信号
    }
    
    return 0;
}
```

### 实验3：使用strace观察信号系统调用

```bash
#!/bin/bash
# signal_strace.sh - 使用strace跟踪信号处理

echo "=== Ctrl+C 信号处理系统调用跟踪 ==="

# 编译测试程序
echo "编译测试程序..."
gcc -o signal_trace signal_trace.c
gcc -o custom_signal_chars custom_signal_chars.c

echo -e "\n1. 跟踪信号相关的系统调用:"
echo "启动strace跟踪程序，然后按Ctrl+C观察..."

# 在后台启动目标程序
echo "testing" | timeout 10 strace -e trace=rt_sigaction,rt_sigprocmask,rt_sigreturn,kill,tgkill -o signal.trace ./signal_trace &
TRACE_PID=$!

sleep 2

# 向进程发送信号进行测试
echo "发送SIGINT信号到进程 $TRACE_PID"
kill -INT $TRACE_PID

wait $TRACE_PID 2>/dev/null || true

echo -e "\n系统调用跟踪结果:"
if [ -f signal.trace ]; then
    cat signal.trace
    echo -e "\n分析:"
    echo "- rt_sigaction: 设置信号处理器"
    echo "- rt_sigprocmask: 修改信号掩码" 
    echo "- rt_sigreturn: 从信号处理器返回"
    echo "- kill/tgkill: 发送信号"
fi

echo -e "\n2. 观察TTY层的ioctl调用:"
echo "跟踪termios相关的系统调用..."

timeout 5 strace -e trace=ioctl -s 200 -o termios.trace ./custom_signal_chars < /dev/null &
TERMIOS_PID=$!

sleep 1
kill -INT $TERMIOS_PID 2>/dev/null || true
wait $TERMIOS_PID 2>/dev/null || true

if [ -f termios.trace ]; then
    echo "Termios相关调用:"
    grep -E "(TCGETS|TCSETS|TCSETSW|TCSETSF)" termios.trace || echo "无termios调用"
fi

echo -e "\n3. 使用ps观察信号状态:"
echo "查看进程的信号信息..."

# 启动一个测试进程
sleep 30 &
TEST_PID=$!

echo "测试进程PID: $TEST_PID"
echo "进程信号状态:"
if [ -f /proc/$TEST_PID/status ]; then
    grep -E "^Sig" /proc/$TEST_PID/status
    echo "说明:"
    echo "  SigQ: 信号队列使用/限制"
    echo "  SigPnd: 待处理信号掩码"
    echo "  SigBlk: 被阻塞信号掩码"
    echo "  SigIgn: 被忽略信号掩码"
    echo "  SigCgt: 被捕获信号掩码"
fi

# 发送信号并再次检查
echo -e "\n发送SIGTERM后的状态:"
kill -TERM $TEST_PID
sleep 0.1

if [ -f /proc/$TEST_PID/status ]; then
    grep -E "^Sig" /proc/$TEST_PID/status 2>/dev/null || echo "进程已终止"
fi

wait $TEST_PID 2>/dev/null || true

# 清理
rm -f signal.trace termios.trace signal_trace custom_signal_chars

echo -e "\n实验完成！"
```

### 实验4：内核信号路径验证

```c
// kernel_signal_path.c - 验证内核信号传递路径
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

void print_process_tree() {
    printf("当前进程树:\n");
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pstree -p %d 2>/dev/null || ps -eo pid,ppid,pgid,sid,tty,cmd | grep -E '(PID|%d)'", getpid(), getpid());
    system(cmd);
}

void child_signal_handler(int sig) {
    printf("[子进程%d] 收到信号 %d\n", getpid(), sig);
    fflush(stdout);
}

void test_process_group_signal() {
    pid_t pids[3];
    int i;
    
    printf("\n=== 进程组信号传递测试 ===\n");
    printf("父进程PID: %d, PGID: %d\n", getpid(), getpgrp());
    
    /* 创建多个子进程 */
    for (i = 0; i < 3; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            /* 子进程 */
            signal(SIGINT, child_signal_handler);
            signal(SIGTERM, child_signal_handler);
            
            printf("[子进程%d] PID=%d, PGID=%d, 开始等待信号...\n", 
                   i, getpid(), getpgrp());
            fflush(stdout);
            
            /* 等待信号 */
            while (1) {
                pause();
            }
            exit(0);
        } else if (pids[i] > 0) {
            printf("创建子进程 %d: PID=%d\n", i, pids[i]);
        } else {
            perror("fork");
            exit(1);
        }
    }
    
    sleep(2);  /* 让子进程准备好 */
    
    print_process_tree();
    
    printf("\n发送SIGINT到整个进程组...\n");
    if (kill(-getpgrp(), SIGINT) == -1) {
        perror("kill process group");
    }
    
    sleep(1);
    
    printf("发送SIGTERM终止所有子进程...\n");
    for (i = 0; i < 3; i++) {
        kill(pids[i], SIGTERM);
    }
    
    /* 等待所有子进程 */
    for (i = 0; i < 3; i++) {
        int status;
        if (waitpid(pids[i], &status, 0) != -1) {
            printf("子进程 %d 退出，状态: %d\n", pids[i], status);
        }
    }
}

void test_session_signal() {
    pid_t pid;
    
    printf("\n=== 会话信号测试 ===\n");
    
    pid = fork();
    if (pid == 0) {
        /* 子进程：创建新会话 */
        pid_t old_sid = getsid(0);
        pid_t new_sid = setsid();
        
        if (new_sid == -1) {
            perror("setsid");
            exit(1);
        }
        
        printf("[新会话] 原会话ID: %d, 新会话ID: %d\n", old_sid, new_sid);
        printf("[新会话] PID: %d, PGID: %d, SID: %d\n", 
               getpid(), getpgrp(), getsid(0));
        
        /* 检查是否有控制终端 */
        if (open("/dev/tty", O_RDONLY) == -1) {
            printf("[新会话] 没有控制终端 (期望的)\n");
        } else {
            printf("[新会话] 仍有控制终端 (意外)\n");
        }
        
        signal(SIGHUP, child_signal_handler);
        signal(SIGINT, child_signal_handler);
        
        printf("[新会话] 等待信号...\n");
        fflush(stdout);
        
        while (1) {
            sleep(1);
        }
        
        exit(0);
    } else if (pid > 0) {
        printf("创建新会话子进程: %d\n", pid);
        sleep(2);
        
        printf("向新会话发送SIGHUP...\n");
        kill(pid, SIGHUP);
        
        sleep(1);
        
        printf("向新会话发送SIGINT...\n");  
        kill(pid, SIGINT);
        
        sleep(1);
        
        printf("终止新会话进程...\n");
        kill(pid, SIGKILL);
        
        wait(NULL);
    } else {
        perror("fork");
    }
}

int main() {
    printf("=== 内核信号传递路径验证 ===\n");
    
    /* 防止父进程被子进程的信号影响 */
    signal(SIGINT, SIG_IGN);
    
    test_process_group_signal();
    test_session_signal();
    
    printf("\n所有测试完成\n");
    return 0;
}
```

## 🚨 常见坑 & Debug方法

### 1. 信号字符不起作用

**问题**: 按Ctrl+C没有反应
```bash
# 检查ISIG标志
stty -a | grep -E "(isig|-isig)"

# 检查信号字符设置
stty -a | grep intr

# 检查进程是否在前台
ps -o pid,pgid,sid,tty,stat,cmd | grep $$

# 恢复默认设置
stty sane
```

### 2. 自定义信号处理器不被调用

**问题**: 设置了信号处理器但没有执行
```c
// 检查信号掩码
void check_signal_mask() {
    sigset_t mask;
    sigprocmask(0, NULL, &mask);  // 获取当前掩码
    
    printf("SIGINT %s\n", sigismember(&mask, SIGINT) ? "被阻塞" : "未阻塞");
    
    // 检查信号处理方式
    struct sigaction sa;
    sigaction(SIGINT, NULL, &sa);
    
    if (sa.sa_handler == SIG_DFL)
        printf("SIGINT: 默认处理\n");
    else if (sa.sa_handler == SIG_IGN)
        printf("SIGINT: 忽略\n");
    else
        printf("SIGINT: 自定义处理器\n");
}
```

### 3. 进程组信号传递失败

**问题**: kill(-pgid, SIGINT)没有影响所有进程
```bash
# 检查进程组关系
ps -eo pid,ppid,pgid,sid,tty,cmd --forest

# 验证进程组ID
echo "当前PGID: $(ps -o pgid= -p $$)"

# 发送信号到进程组
kill -INT -$(ps -o pgid= -p $$)
```

### 4. 使用debugger调试信号处理

```bash
# 使用gdb调试信号处理
gdb ./signal_trace
(gdb) handle SIGINT nostop noprint pass   # 让SIGINT传递给程序
(gdb) break detailed_signal_handler       # 在信号处理器设置断点
(gdb) run
# 程序运行后按Ctrl+C，会在信号处理器中停下
```

### 5. 内核调试信息

```bash
# 查看内核信号统计 (如果可用)
cat /proc/interrupts | grep keyboard

# 查看进程信号统计  
cat /proc/self/stat | cut -d' ' -f10,12
# 字段10: 接收到的信号数
# 字段12: 被忽略的信号数

# 使用ftrace跟踪信号处理 (需要root)
echo 'do_signal' > /sys/kernel/debug/tracing/set_ftrace_filter
echo function > /sys/kernel/debug/tracing/current_tracer
echo 1 > /sys/kernel/debug/tracing/tracing_on
```

## 📋 高级应用场景

### 1. 信号安全的异步处理

```c
// 信号安全的事件循环实现
volatile sig_atomic_t signal_pending = 0;
int signal_pipe[2];

void async_signal_handler(int sig) {
    char byte = sig;
    write(signal_pipe[1], &byte, 1);  // 信号安全
    signal_pending = 1;
}

void setup_async_signals() {
    if (pipe(signal_pipe) == -1) {
        perror("pipe");
        exit(1);
    }
    
    /* 设置非阻塞 */
    fcntl(signal_pipe[1], F_SETFL, O_NONBLOCK);
    
    signal(SIGINT, async_signal_handler);
    signal(SIGTERM, async_signal_handler);
}

void event_loop() {
    fd_set readfds;
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(signal_pipe[0], &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        int maxfd = (signal_pipe[0] > STDIN_FILENO) ? 
                    signal_pipe[0] : STDIN_FILENO;
        
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) > 0) {
            if (FD_ISSET(signal_pipe[0], &readfds)) {
                /* 处理信号 */
                char sig;
                while (read(signal_pipe[0], &sig, 1) == 1) {
                    printf("异步处理信号: %d\n", sig);
                    if (sig == SIGTERM || sig == SIGINT) {
                        printf("收到终止信号，优雅退出...\n");
                        return;
                    }
                }
            }
            
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                /* 处理输入 */
                char buffer[256];
                if (fgets(buffer, sizeof(buffer), stdin)) {
                    printf("输入: %s", buffer);
                }
            }
        }
    }
}
```

### 2. 终端信号转发器

```c
// 实现一个信号转发器，类似nohup但更灵活
void signal_forwarder() {
    pid_t child_pid = fork();
    
    if (child_pid == 0) {
        /* 子进程：执行实际工作 */
        execvp(argv[1], &argv[1]);
        perror("execvp");
        exit(1);
    } else {
        /* 父进程：信号转发器 */
        
        /* 捕获所有可能的信号 */
        for (int sig = 1; sig < NSIG; sig++) {
            if (sig != SIGKILL && sig != SIGSTOP && sig != SIGCHLD) {
                signal(sig, forward_signal);
            }
        }
        
        /* 等待子进程结束 */
        int status;
        waitpid(child_pid, &status, 0);
        
        exit(WEXITSTATUS(status));
    }
}

void forward_signal(int sig) {
    /* 转发信号给子进程 */
    kill(child_pid, sig);
    
    /* 记录信号转发 */
    syslog(LOG_INFO, "转发信号 %d 给进程 %d", sig, child_pid);
}
```

### 3. 信号驱动的I/O处理

```c
// 使用SIGIO实现信号驱动I/O
void setup_sigio(int fd) {
    int flags;
    
    /* 设置信号接收进程 */
    if (fcntl(fd, F_SETOWN, getpid()) == -1) {
        perror("fcntl F_SETOWN");
        exit(1);
    }
    
    /* 启用异步I/O */
    flags = fcntl(fd, F_GETFL);
    if (fcntl(fd, F_SETFL, flags | O_ASYNC) == -1) {
        perror("fcntl F_SETFL");
        exit(1);
    }
}

void sigio_handler(int sig) {
    char buffer[1024];
    ssize_t n;
    
    /* 非阻塞读取所有可用数据 */
    while ((n = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
        write(STDOUT_FILENO, "SIGIO: ", 7);
        write(STDOUT_FILENO, buffer, n);
    }
}
```

## 🎯 学习检查点

完成本模块后，你应该能够：

1. ✅ 理解从Ctrl+C按键到进程收到SIGINT的完整路径
2. ✅ 掌握Line Discipline中信号字符识别的实现机制
3. ✅ 理解信号如何从TTY传递到前台进程组
4. ✅ 知道如何自定义信号字符和处理方式
5. ✅ 能够使用strace等工具调试信号处理问题
6. ✅ 理解信号安全编程的要求和技巧
7. ✅ 掌握进程组信号传递的内核实现

---

**下一步**: 学习 [read()/write() 与阻塞机制详解](07-read-write-blocking.md)