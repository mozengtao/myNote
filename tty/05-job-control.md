# Job Control 作业控制详解

## 🎯 学习目标
深入理解Unix/Linux的作业控制机制，掌握session、进程组、控制终端的概念和实现，理解shell如何管理前台/后台作业。

---

## 📊 Job Control 完整架构图

```
作业控制层次结构:
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Session                                       │
│                           (会话 - SID)                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      Controlling TTY                                │   │
│  │                     (控制终端)                                       │   │
│  │                   /dev/pts/0 或 /dev/tty1                          │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                  │                                         │
│                                  ▼                                         │
│  ┌─────────────────────┐    ┌─────────────────────┐    ┌─────────────────┐  │
│  │   Process Group 1   │    │   Process Group 2   │    │   Process Group │  │
│  │    (Foreground)     │    │    (Background)     │    │        N        │  │
│  │     PGID=1001       │    │     PGID=1002       │    │    PGID=100N   │  │
│  │                     │    │                     │    │                 │  │
│  │ ┌─────┐ ┌─────┐     │    │ ┌─────┐ ┌─────┐     │    │ ┌─────┐         │  │
│  │ │PID  │ │PID  │     │    │ │PID  │ │PID  │     │    │ │PID  │         │  │
│  │ │1001 │ │1003 │     │    │ │1002 │ │1004 │     │    │ │100N │   ...   │  │
│  │ │     │ │     │     │    │ │     │ │     │     │    │ │     │         │  │
│  │ └─────┘ └─────┘     │    │ └─────┘ └─────┘     │    │ └─────┘         │  │
│  │                     │    │                     │    │                 │  │
│  └─────────────────────┘    └─────────────────────┘    └─────────────────┘  │
│           │                          │                         │             │
│           ▼                          ▼                         ▼             │
│    收到终端信号                   不收到终端信号              不收到终端信号     │
│    (^C, ^Z 等)                  (除非fg到前台)              (除非fg到前台)    │
└─────────────────────────────────────────────────────────────────────────────┘

信号传播路径:
Terminal → TTY Driver → Line Discipline → Session Leader → Foreground PGID

Shell作业管理:
┌─────────────────────────────────────────────────────────────────────────────┐
│                               Shell (bash)                                 │
│                            Session Leader                                  │
│                                                                             │
│  作业表 (Job Table):                                                        │
│  ┌─────┬──────────┬─────────┬────────┬──────────────────────────────────┐   │
│  │ JID │   PGID   │  状态   │  TTY   │            命令                  │   │
│  ├─────┼──────────┼─────────┼────────┼──────────────────────────────────┤   │
│  │  1  │   1001   │Running  │  fg    │  vim file.txt                    │   │
│  │  2  │   1002   │Stopped  │   -    │  sleep 100                       │   │
│  │  3  │   1003   │Running  │   -    │  make -j4 > build.log &          │   │
│  └─────┴──────────┴─────────┴────────┴──────────────────────────────────┘   │
│                                                                             │
│  Shell命令处理:                                                             │
│  • command        → 前台执行 (fg process group)                             │
│  • command &      → 后台执行 (bg process group)                             │
│  • fg %1          → 将作业1移到前台                                          │
│  • bg %2          → 将作业2移到后台继续                                      │
│  • jobs           → 显示作业列表                                             │
│  • kill %3        → 终止作业3                                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 🏗️ 核心数据结构详解

### 进程控制相关结构

```c
// include/linux/sched.h - 进程结构中的session/pgrp信息
struct task_struct {
    pid_t pid;                          // 进程ID
    pid_t tgid;                         // 线程组ID (对进程而言等于pid)
    
    struct task_struct *group_leader;   // 线程组leader
    struct pid_link pids[PIDTYPE_MAX];  // PID链接
    
    // 信号处理
    struct signal_struct *signal;       // 信号结构
    // ...
};

// include/linux/sched/signal.h - 信号和会话信息
struct signal_struct {
    atomic_t count;                     // 引用计数
    atomic_t live;                      // 活动线程数
    
    // 会话和进程组
    struct pid *session;                // 会话ID
    struct pid *pgrp;                   // 进程组ID  
    pid_t __pgrp;                       // 进程组ID缓存
    
    // 控制终端
    struct tty_struct *tty;             // 控制终端
    struct pid *tty_old_pgrp;           // 旧的终端进程组
    
    // 作业控制
    int leader;                         // 是否是会话leader
    
    // 信号处理
    struct k_sigaction action[_NSIG];   // 信号处理数组
    // ...
};

// include/linux/pid.h - PID命名空间和类型  
enum pid_type {
    PIDTYPE_PID,                        // 进程PID
    PIDTYPE_TGID,                       // 线程组ID
    PIDTYPE_PGID,                       // 进程组ID
    PIDTYPE_SID,                        // 会话ID
    PIDTYPE_MAX,
};

struct pid {
    atomic_t count;                     // 引用计数
    unsigned int level;                 // PID命名空间级别
    struct hlist_head tasks[PIDTYPE_MAX]; // 任务链表
    struct upid numbers[1];             // PID数字 (可变长度)
};
```

### TTY控制结构

```c
// include/linux/tty.h - TTY的作业控制信息
struct tty_struct {
    struct mutex legacy_mutex;          // 锁
    struct tty_driver *driver;          // TTY驱动
    const struct tty_operations *ops;   // 操作函数
    
    // 作业控制相关
    struct pid *session;                // 会话ID
    struct pid *pgrp;                   // 前台进程组ID
    unsigned long flags;                // 标志位
    
    // 信号控制
    int ctrl_status;                    // 控制状态
    int packet;                         // packet模式
    
    // 等待队列
    wait_queue_head_t read_wait;        // 读等待队列
    wait_queue_head_t write_wait;       // 写等待队列
    
    // termios
    struct ktermios termios;            // 终端属性
    struct ktermios termios_locked;     // 锁定的终端属性
    
    char name[64];                      // 设备名称
    // ...
};

// TTY标志位定义
#define TTY_THROTTLED       0           // TTY被限流
#define TTY_IO_ERROR        1           // IO错误
#define TTY_OTHER_CLOSED    2           // 另一端关闭
#define TTY_EXCLUSIVE       3           // 独占模式
#define TTY_DO_WRITE_WAKEUP 5           // 写唤醒
#define TTY_LDISC_OPEN      11          // line discipline已打开
#define TTY_HW_COOK_OUT     14          // 硬件输出处理
#define TTY_HW_COOK_IN      15          // 硬件输入处理
#define TTY_PTY_LOCK        16          // PTY锁定
#define TTY_NO_WRITE_SPLIT  17          // 不分割写入
```

## 🔄 作业控制关键操作流程

### Session创建流程

```c
// kernel/sys.c - 创建新会话
SYSCALL_DEFINE0(setsid)
{
    struct task_struct *group_leader = current->group_leader;
    struct pid *sid = task_pid(group_leader);
    pid_t session = pid_vnr(sid);
    int err = -EPERM;

    write_lock_irq(&tasklist_lock);
    
    /* 不能是进程组leader */
    if (group_leader->pid == group_leader->tgid &&
        same_thread_group(group_leader, group_leader->real_parent))
        goto out;

    /* 确保没有同名的会话 */
    if (find_task_by_pid_ns(session, &init_pid_ns))
        goto out;

    /* 创建新会话和进程组 */
    group_leader->signal->leader = 1;                    // 设为会话leader
    __set_special_pids(sid);                            // 设置session和pgrp

    /* 断开控制终端 */
    proc_clear_tty(group_leader);

    err = session;
out:
    write_unlock_irq(&tasklist_lock);
    if (err > 0) {
        proc_sid_connector(group_leader);
        sched_autogroup_create_attach(group_leader);
    }
    return err;
}

// 设置特殊PID (session和process group)
static void __set_special_pids(struct pid *pid)
{
    struct task_struct *curr = current->group_leader;

    if (task_session(curr) != pid)
        change_pid(curr, PIDTYPE_SID, pid);      // 设置会话ID

    if (task_pgrp(curr) != pid)
        change_pid(curr, PIDTYPE_PGID, pid);     // 设置进程组ID
}
```

### 控制终端设置

```c
// drivers/tty/tty_io.c - 设置控制终端
static int tiocsctty(struct tty_struct *tty, struct file *file, int arg)
{
    int ret = 0;

    read_lock(&tasklist_lock);

    if (current->signal->leader && (task_session(current) == tty->session))
        goto out;

    /*
     * 如果是会话leader且没有控制终端
     */
    if (!current->signal->leader) {
        ret = -EPERM;
        goto out;
    }

    if (tty->session) {
        /*
         * 该终端已经有会话了
         * 如果arg == 1且是root，可以偷取
         */
        if (arg == 1 && capable(CAP_SYS_ADMIN)) {
            /*
             * 偷取该终端
             */
            read_unlock(&tasklist_lock);
            proc_clear_tty_for_session(tty->session);
            read_lock(&tasklist_lock);
        } else {
            ret = -EPERM;
            goto out;
        }
    }

    /* 设置为当前会话的控制终端 */
    proc_set_tty(tty);
out:
    read_unlock(&tasklist_lock);
    return ret;
}

// fs/proc/base.c - 设置进程的控制终端
void proc_set_tty(struct tty_struct *tty)
{
    unsigned long flags;

    spin_lock_irqsave(&tty->ctrl_lock, flags);
    /*
     * tty->session和tty->pgrp的引用在 disassociate_ctty()中释放
     */
    put_pid(tty->session);
    put_pid(tty->pgrp);
    tty->pgrp = get_pid(task_pgrp(current));
    spin_unlock_irqrestore(&tty->ctrl_lock, flags);
    tty->session = get_pid(task_session(current));
    
    if (current->signal->tty) {
        tty_debug(tty, "OOPS - re-setting TTY!\n");
        tty_kref_put(current->signal->tty);
    }
    
    put_pid(current->signal->tty_old_pgrp);
    current->signal->tty_old_pgrp = NULL;
    current->signal->tty = tty_kref_get(tty);
    current->signal->tty_old_pgrp = get_pid(tty->pgrp);
}
```

### 前台进程组管理

```c
// drivers/tty/tty_io.c - 设置前台进程组
int tcsetpgrp(int fd, pid_t pgrp)
{
    return ioctl(fd, TIOCSPGRP, &pgrp);
}

static int tiocspgrp(struct tty_struct *tty, struct tty_struct *real_tty, pid_t __user *p)
{
    struct pid *pgrp;
    pid_t pgrp_nr;
    int retval = tty_check_change(real_tty);

    if (retval == -EIO)
        return -ENOTTY;
    if (retval)
        return retval;

    if (get_user(pgrp_nr, p))
        return -EFAULT;
    if (pgrp_nr < 0)
        return -EINVAL;

    rcu_read_lock();
    pgrp = find_vpid(pgrp_nr);
    retval = -ESRCH;
    if (!pgrp)
        goto out_unlock;
    retval = -EPERM;
    if (session_of_pgrp(pgrp) != task_session(current))
        goto out_unlock;
    retval = 0;
    put_pid(real_tty->pgrp);
    real_tty->pgrp = get_pid(pgrp);
out_unlock:
    rcu_read_unlock();
    return retval;
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
    /* 遍历进程组中的所有进程 */
    do_each_pid_task(pgrp, PIDTYPE_PGID, p) {
        int err = group_send_sig_info(sig, info, p, PIDTYPE_PGID);
        success |= !err;
        retval = err;
    } while_each_pid_task(pgrp, PIDTYPE_PGID, p);
    return success ? 0 : retval;
}

// 终端信号处理 - 发送信号给前台进程组
void tty_signal_session_leader(struct tty_struct *tty, int exit_session)
{
    struct task_struct *p;
    
    do_each_pid_task(tty->session, PIDTYPE_SID, p) {
        if (p->signal->tty == tty) {
            p->signal->tty = NULL;
            /* 如果进程组leader，发送SIGHUP */
            if (p->signal->leader)
                group_send_sig_info(SIGHUP, SEND_SIG_PRIV, p, PIDTYPE_PGID);
        }
    } while_each_pid_task(tty->session, PIDTYPE_SID, p);
    
    /* 发送SIGCONT给停止的进程 */
    if (tty->pgrp)
        kill_pgrp(tty->pgrp, SIGCONT, 0);
        
    put_pid(tty->session);
    put_pid(tty->pgrp);
    tty->session = NULL;
    tty->pgrp = NULL;
}
```

## 🔧 Shell作业管理实现

### Bash作业控制实现

```c
// bash源码中的作业控制关键函数 (简化版)

// 作业结构定义
typedef struct job {
    char *wd;                    // 工作目录
    PROCESS *pipe;               // 管道进程列表
    pid_t pgrp;                  // 进程组ID
    JOB_STATE state;             // 作业状态
    int flags;                   // 标志
    int deferred;                // 延迟操作
    struct job *next, *prev;     // 链表指针
} JOB;

typedef enum {
    JRUNNING,                    // 运行中
    JSTOPPED,                    // 已停止
    JDEAD                        // 已结束
} JOB_STATE;

// 创建新作业
JOB *make_job_control(pid_t pgrp, int foreground)
{
    JOB *job;
    int job_slot;

    /* 分配作业槽 */
    job_slot = find_job_slot();
    if (job_slot == -1)
        return NULL;

    job = jobs[job_slot] = (JOB *)malloc(sizeof(JOB));
    
    job->pgrp = pgrp;
    job->state = JRUNNING;
    job->flags = foreground ? J_FOREGROUND : 0;
    job->deferred = 0;
    
    /* 如果是前台作业，设置为终端的前台进程组 */
    if (foreground && job_control) {
        tcsetpgrp(shell_tty, pgrp);
        terminal_pgrp = pgrp;
    }
    
    return job;
}

// 等待作业完成
int wait_for_job(int job_index, int foreground)
{
    JOB *job;
    pid_t pid;
    int status, ret;

    job = get_job_by_jid(job_index);
    if (!job)
        return -1;

    /* 前台作业：阻塞等待 */
    if (foreground) {
        while (job->state == JRUNNING) {
            pid = waitpid(-job->pgrp, &status, WUNTRACED);
            
            if (pid == -1) {
                if (errno == ECHILD) {
                    job->state = JDEAD;
                    break;
                }
                continue;
            }
            
            /* 更新进程状态 */
            if (WIFSTOPPED(status)) {
                job->state = JSTOPPED;
                /* 停止的前台作业：shell重新获得终端控制 */
                tcsetpgrp(shell_tty, shell_pgrp);
                terminal_pgrp = shell_pgrp;
                break;
            } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                mark_process_status(job, pid, status);
                if (job_completed(job)) {
                    job->state = JDEAD;
                    /* 前台作业结束：shell重新获得终端控制 */
                    tcsetpgrp(shell_tty, shell_pgrp);
                    terminal_pgrp = shell_pgrp;
                    break;
                }
            }
        }
        ret = job->state == JDEAD ? 0 : 1;
    } else {
        /* 后台作业：非阻塞检查 */
        ret = job_completed(job) ? 0 : 1;
    }
    
    return ret;
}

// fg命令实现 - 将后台作业移到前台
int fg_builtin(char **args)
{
    JOB *job;
    int jid;

    if (args[1] == NULL) {
        jid = current_job;  // 默认当前作业
    } else {
        jid = parse_job_spec(args[1]);  // 解析%1, %+等
    }

    job = get_job_by_jid(jid);
    if (!job) {
        builtin_error("fg: no such job");
        return 1;
    }

    /* 如果作业已停止，发送SIGCONT */
    if (job->state == JSTOPPED) {
        kill(-job->pgrp, SIGCONT);
        job->state = JRUNNING;
    }

    /* 设置为前台进程组 */
    tcsetpgrp(shell_tty, job->pgrp);
    terminal_pgrp = job->pgrp;
    job->flags |= J_FOREGROUND;

    /* 等待作业完成 */
    return wait_for_job(jid, 1);
}

// bg命令实现 - 将停止的作业移到后台继续
int bg_builtin(char **args)
{
    JOB *job;
    int jid;

    if (args[1] == NULL) {
        jid = current_job;
    } else {
        jid = parse_job_spec(args[1]);
    }

    job = get_job_by_jid(jid);
    if (!job) {
        builtin_error("bg: no such job");
        return 1;
    }

    if (job->state != JSTOPPED) {
        builtin_error("bg: job is not stopped");
        return 1;
    }

    /* 发送SIGCONT继续执行 */
    kill(-job->pgrp, SIGCONT);
    job->state = JRUNNING;
    job->flags &= ~J_FOREGROUND;

    printf("[%d] %s\n", jid, job_text(job));
    return 0;
}
```

## 🧪 最小可运行实验

### 实验1：Session和进程组观察

```c
// session_test.c - 观察会话和进程组
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

void print_ids(const char *stage) {
    printf("=== %s ===\n", stage);
    printf("PID: %d\n", getpid());
    printf("PPID: %d\n", getppid());  
    printf("PGID: %d\n", getpgrp());
    printf("SID: %d\n", getsid(0));
    
    // 检查是否有控制终端
    pid_t fg_pgrp = tcgetpgrp(STDIN_FILENO);
    if (fg_pgrp != -1) {
        printf("前台进程组: %d\n", fg_pgrp);
        printf("是否在前台: %s\n", (fg_pgrp == getpgrp()) ? "是" : "否");
    } else {
        printf("无控制终端\n");
    }
    printf("\n");
}

void sig_handler(int sig) {
    printf("[进程%d] 收到信号 %d (%s)\n", getpid(), sig,
           sig == SIGINT ? "SIGINT" :
           sig == SIGTSTP ? "SIGTSTP" :
           sig == SIGTERM ? "SIGTERM" :
           sig == SIGHUP ? "SIGHUP" : "UNKNOWN");
}

int main() {
    pid_t pid;
    
    printf("Job Control 实验程序\n");
    
    // 设置信号处理
    signal(SIGINT, sig_handler);
    signal(SIGTSTP, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGHUP, sig_handler);
    
    print_ids("初始状态");
    
    // 创建子进程
    pid = fork();
    if (pid == 0) {
        // 子进程
        print_ids("子进程 - fork后");
        
        // 创建新的进程组
        if (setpgid(0, 0) == 0) {
            print_ids("子进程 - 新进程组");
        } else {
            perror("setpgid");
        }
        
        printf("[子进程] 进入无限循环，按Ctrl+C测试信号...\n");
        while (1) {
            sleep(1);
            printf("[子进程 %d] 还在运行...\n", getpid());
        }
    } else if (pid > 0) {
        // 父进程
        print_ids("父进程 - fork后");
        
        printf("[父进程] 子进程PID: %d\n", pid);
        
        // 等待一会，让子进程设置进程组
        sleep(2);
        
        printf("[父进程] 子进程的PGID: %d\n", getpgid(pid));
        
        // 等待子进程（会被Ctrl+C中断）
        int status;
        printf("[父进程] 等待子进程结束...\n");
        if (wait(&status) != -1) {
            printf("[父进程] 子进程结束，状态: %d\n", status);
        } else {
            printf("[父进程] 等待被中断: %s\n", strerror(errno));
        }
    } else {
        perror("fork");
        return 1;
    }
    
    return 0;
}
```

### 实验2：控制终端和前台进程组

```c
// terminal_control.c - 控制终端实验
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

void print_terminal_info() {
    char *tty_name = ttyname(STDIN_FILENO);
    printf("控制终端: %s\n", tty_name ? tty_name : "无");
    
    pid_t fg_pgrp = tcgetpgrp(STDIN_FILENO);
    if (fg_pgrp != -1) {
        printf("前台进程组ID: %d\n", fg_pgrp);
        printf("当前进程组ID: %d\n", getpgrp());
        printf("当前进程在前台: %s\n", 
               (fg_pgrp == getpgrp()) ? "是" : "否");
    } else {
        printf("无法获取前台进程组: %s\n", strerror(errno));
    }
}

int create_session_test() {
    printf("\n=== 创建新会话测试 ===\n");
    
    pid_t old_sid = getsid(0);
    pid_t old_pgid = getpgrp();
    
    printf("创建会话前:\n");
    printf("  SID: %d\n", old_sid);
    printf("  PGID: %d\n", old_pgid);
    printf("  PID: %d\n", getpid());
    
    // 创建新会话
    pid_t new_sid = setsid();
    if (new_sid == -1) {
        printf("setsid()失败: %s\n", strerror(errno));
        printf("可能的原因: 当前进程是进程组leader\n");
        return -1;
    }
    
    printf("创建会话后:\n");
    printf("  SID: %d\n", getsid(0));
    printf("  PGID: %d\n", getpgrp());
    printf("  PID: %d\n", getpid());
    
    // 检查控制终端状态
    printf("控制终端状态:\n");
    print_terminal_info();
    
    return 0;
}

int main() {
    printf("=== 终端控制实验 ===\n");
    
    printf("初始状态:\n");
    print_terminal_info();
    
    // Fork子进程来测试setsid
    pid_t pid = fork();
    if (pid == 0) {
        // 子进程：不是进程组leader，可以调用setsid
        create_session_test();
        
        // 尝试重新获取控制终端
        printf("\n=== 重新获取控制终端测试 ===\n");
        
        // 打开当前终端设备
        char *tty_name = ttyname(STDIN_FILENO);
        if (tty_name) {
            int tty_fd = open(tty_name, O_RDWR);
            if (tty_fd != -1) {
                // 设置为控制终端
                if (ioctl(tty_fd, TIOCSCTTY, 1) == 0) {
                    printf("成功设置控制终端\n");
                    print_terminal_info();
                } else {
                    printf("设置控制终端失败: %s\n", strerror(errno));
                }
                close(tty_fd);
            }
        }
        
        exit(0);
    } else if (pid > 0) {
        // 父进程：等待子进程完成
        wait(NULL);
        
        printf("\n父进程最终状态:\n");
        print_terminal_info();
    } else {
        perror("fork");
        return 1;
    }
    
    return 0;
}
```

### 实验3：模拟Shell作业控制

```c
// mini_shell.c - 简单的shell作业控制实现
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define MAX_JOBS 10
#define MAX_ARGS 64
#define MAX_LINE 1024

typedef struct {
    int jid;                    // 作业ID
    pid_t pgid;                 // 进程组ID
    int state;                  // 状态: 0=无效, 1=运行, 2=停止, 3=完成
    char cmdline[MAX_LINE];     // 命令行
} job_t;

job_t jobs[MAX_JOBS];          // 作业表
int next_jid = 1;              // 下一个作业ID
pid_t shell_pgid;              // Shell进程组ID
int shell_terminal;            // Shell终端fd
int shell_interactive;         // 是否交互式

void init_shell() {
    shell_terminal = STDIN_FILENO;
    shell_interactive = isatty(shell_terminal);
    
    if (shell_interactive) {
        /* 确保shell在前台 */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp())) {
            kill(-shell_pgid, SIGTTIN);
        }
        
        /* 忽略作业控制信号 */
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        
        /* Shell成为自己的进程组leader */
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("setpgid");
            exit(1);
        }
        
        /* 获取终端控制 */
        tcsetpgrp(shell_terminal, shell_pgid);
    }
}

int add_job(pid_t pgid, char *cmdline) {
    int i;
    
    for (i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].state == 0) {  // 找到空槽
            jobs[i].jid = next_jid++;
            jobs[i].pgid = pgid;
            jobs[i].state = 1;     // 运行状态
            strcpy(jobs[i].cmdline, cmdline);
            return jobs[i].jid;
        }
    }
    return -1;  // 作业表满
}

job_t *get_job(int jid) {
    int i;
    for (i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].jid == jid && jobs[i].state != 0)
            return &jobs[i];
    }
    return NULL;
}

void list_jobs() {
    int i;
    printf("活动作业:\n");
    for (i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].state != 0) {
            printf("[%d] %s %s\n", jobs[i].jid,
                   jobs[i].state == 1 ? "Running" :
                   jobs[i].state == 2 ? "Stopped" : "Done",
                   jobs[i].cmdline);
        }
    }
}

int builtin_fg(char **args) {
    int jid;
    job_t *job;
    
    if (args[1] == NULL) {
        printf("用法: fg %%<jid>\n");
        return 1;
    }
    
    if (args[1][0] == '%') {
        jid = atoi(&args[1][1]);
    } else {
        jid = atoi(args[1]);
    }
    
    job = get_job(jid);
    if (!job) {
        printf("fg: 作业 %d 不存在\n", jid);
        return 1;
    }
    
    /* 如果作业停止，发送SIGCONT */
    if (job->state == 2) {
        kill(-job->pgid, SIGCONT);
    }
    
    job->state = 1;  // 设为运行状态
    
    /* 将作业放到前台 */
    tcsetpgrp(shell_terminal, job->pgid);
    
    /* 等待作业完成或停止 */
    int status;
    pid_t wpid;
    while ((wpid = waitpid(-job->pgid, &status, WUNTRACED)) > 0) {
        if (WIFSTOPPED(status)) {
            job->state = 2;  // 停止状态
            printf("\n[%d] 已停止: %s\n", job->jid, job->cmdline);
            break;
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job->state = 3;  // 完成状态
            break;
        }
    }
    
    /* Shell重新获得前台控制 */
    tcsetpgrp(shell_terminal, shell_pgid);
    
    return 0;
}

int builtin_bg(char **args) {
    int jid;
    job_t *job;
    
    if (args[1] == NULL) {
        printf("用法: bg %%<jid>\n");
        return 1;
    }
    
    if (args[1][0] == '%') {
        jid = atoi(&args[1][1]);
    } else {
        jid = atoi(args[1]);
    }
    
    job = get_job(jid);
    if (!job) {
        printf("bg: 作业 %d 不存在\n", jid);
        return 1;
    }
    
    if (job->state != 2) {
        printf("bg: 作业 %d 未停止\n", jid);
        return 1;
    }
    
    /* 发送SIGCONT */
    kill(-job->pgid, SIGCONT);
    job->state = 1;  // 运行状态
    
    printf("[%d] %s &\n", job->jid, job->cmdline);
    
    return 0;
}

void execute_command(char **args, int background) {
    pid_t pid, pgid;
    int jid;
    
    if ((pid = fork()) == 0) {
        /* 子进程 */
        
        /* 创建新进程组 */
        pgid = getpid();
        if (setpgid(0, pgid) < 0) {
            perror("setpgid");
            exit(1);
        }
        
        /* 前台作业获得终端控制 */
        if (!background && shell_interactive) {
            tcsetpgrp(shell_terminal, pgid);
        }
        
        /* 恢复默认信号处理 */
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        
        /* 执行命令 */
        execvp(args[0], args);
        printf("%s: 命令未找到\n", args[0]);
        exit(1);
    } else if (pid > 0) {
        /* 父进程(shell) */
        
        /* 设置子进程的进程组 */
        pgid = pid;
        setpgid(pid, pgid);
        
        /* 添加到作业表 */
        char cmdline[MAX_LINE];
        strcpy(cmdline, args[0]);
        for (int i = 1; args[i]; i++) {
            strcat(cmdline, " ");
            strcat(cmdline, args[i]);
        }
        if (background) strcat(cmdline, " &");
        
        jid = add_job(pgid, cmdline);
        
        if (background) {
            printf("[%d] %d\n", jid, pid);
        } else {
            /* 前台作业：等待完成 */
            tcsetpgrp(shell_terminal, pgid);
            
            int status;
            pid_t wpid;
            job_t *job = get_job(jid);
            
            while ((wpid = waitpid(-pgid, &status, WUNTRACED)) > 0) {
                if (WIFSTOPPED(status)) {
                    job->state = 2;  // 停止
                    printf("\n[%d] 已停止: %s\n", jid, cmdline);
                    break;
                } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    job->state = 3;  // 完成
                    break;
                }
            }
            
            /* Shell重新获得终端控制 */
            tcsetpgrp(shell_terminal, shell_pgid);
        }
    } else {
        perror("fork");
    }
}

int main() {
    char line[MAX_LINE];
    char *args[MAX_ARGS];
    int background;
    
    init_shell();
    
    while (1) {
        printf("mini_shell> ");
        fflush(stdout);
        
        if (!fgets(line, MAX_LINE, stdin)) {
            break;  // EOF
        }
        
        /* 解析命令行 */
        int argc = 0;
        char *token = strtok(line, " \t\n");
        background = 0;
        
        while (token && argc < MAX_ARGS - 1) {
            if (strcmp(token, "&") == 0) {
                background = 1;
                break;
            }
            args[argc++] = token;
            token = strtok(NULL, " \t\n");
        }
        args[argc] = NULL;
        
        if (argc == 0) continue;  // 空命令
        
        /* 处理内置命令 */
        if (strcmp(args[0], "jobs") == 0) {
            list_jobs();
        } else if (strcmp(args[0], "fg") == 0) {
            builtin_fg(args);
        } else if (strcmp(args[0], "bg") == 0) {
            builtin_bg(args);
        } else if (strcmp(args[0], "exit") == 0) {
            break;
        } else {
            execute_command(args, background);
        }
    }
    
    return 0;
}
```

### 实验4：使用ps和strace观察

```bash
#!/bin/bash
# job_control_analysis.sh - 分析作业控制

echo "=== 作业控制分析实验 ==="

echo "1. 当前进程层次结构:"
ps -eo pid,ppid,pgid,sid,tty,stat,cmd --forest

echo -e "\n2. 启动后台作业并观察:"
sleep 30 &
bg_pid=$!
echo "后台进程PID: $bg_pid"

echo "进程组信息:"
ps -o pid,ppid,pgid,sid,tty,stat,cmd -p $bg_pid

echo -e "\n3. 启动前台作业，然后按Ctrl+Z停止:"
echo "运行: sleep 20"
echo "等待几秒后按Ctrl+Z..."

# 使用timeout避免无限等待
timeout 10 sleep 20 &
fg_pid=$!

echo -e "\n4. 观察停止的作业:"
jobs -l

echo -e "\n5. 使用strace观察tcsetpgrp调用:"
echo "在另一个终端运行以下命令观察shell的系统调用:"
echo "strace -e trace=ioctl,rt_sigaction -p $$"

echo -e "\n6. 测试信号发送:"
echo "发送SIGTERM给后台进程组:"
if kill -TERM -$bg_pid 2>/dev/null; then
    echo "信号发送成功"
else  
    echo "进程可能已结束"
fi

wait 2>/dev/null || true
echo "实验结束"
```

## 🚨 常见坑 & Debug方法

### 1. 进程组和会话混淆

**问题**: 不理解PGID和SID的区别
```bash
# 查看完整的进程关系
ps -eo pid,ppid,pgid,sid,tty,comm --forest

# 查看特定进程的详细信息
cat /proc/$PID/stat | cut -d' ' -f1,4,5,6,7
# 字段含义: PID PPID PGRP SESSION TTY_NR
```

### 2. 控制终端丢失

**问题**: setsid()后无法接收键盘信号
```c
// 检查控制终端状态
void check_controlling_terminal() {
    int fd = open("/dev/tty", O_RDWR);
    if (fd == -1) {
        if (errno == ENXIO) {
            printf("没有控制终端\n");
        } else {
            perror("open /dev/tty");
        }
    } else {
        printf("有控制终端\n");
        
        // 获取前台进程组
        pid_t pgrp = tcgetpgrp(fd);
        printf("前台进程组: %d\n", pgrp);
        printf("当前进程组: %d\n", getpgrp());
        
        close(fd);
    }
}
```

### 3. 信号不能传递到预期进程

**问题**: Ctrl+C不能终止特定进程
```bash
# 检查信号传递路径
# 1. 确认终端前台进程组
ps -o pid,pgid,sid,tty,stat,cmd | grep pts/0

# 2. 发送测试信号
kill -INT -$PGID  # 发送给整个进程组

# 3. 检查信号掩码
grep Sig /proc/$PID/status
# SigQ: 排队信号
# SigPnd: 待处理信号
# SigBlk: 被阻塞信号  
# SigIgn: 被忽略信号
# SigCgt: 被捕获信号
```

### 4. 作业状态跟踪错误

**问题**: 后台作业状态更新不及时
```c
// 使用SIGCHLD处理子进程状态变化
void sigchld_handler(int sig) {
    pid_t pid;
    int status;
    
    /* 非阻塞等待所有僵尸进程 */
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        update_job_status(pid, status);
        
        if (WIFEXITED(status)) {
            printf("[进程%d] 正常退出，状态: %d\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("[进程%d] 被信号终止: %d\n", pid, WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
            printf("[进程%d] 被信号停止: %d\n", pid, WSTOPSIG(status));
        } else if (WIFCONTINUED(status)) {
            printf("[进程%d] 继续执行\n", pid);
        }
    }
}

void setup_sigchld() {
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}
```

### 5. 使用strace调试作业控制

```bash
# 跟踪作业控制相关的系统调用
strace -e trace=fork,clone,setpgid,setsid,ioctl,rt_sigaction,wait4 bash

# 关注的系统调用:
# setpgid  - 设置进程组
# setsid   - 创建会话
# ioctl(..., TIOCSPGRP, ...) - 设置前台进程组
# ioctl(..., TIOCGPGRP, ...) - 获取前台进程组
# rt_sigaction - 信号处理设置
# wait4    - 等待子进程状态变化

# 跟踪特定进程的信号
strace -e trace=rt_sigaction,rt_sigprocmask,kill -p $PID
```

## 📋 实际应用场景

### 1. 守护进程创建

```c
// 标准守护进程创建过程
int daemonize() {
    pid_t pid, sid;
    
    /* 第一次fork */
    pid = fork();
    if (pid < 0) return -1;    // fork失败
    if (pid > 0) exit(0);      // 父进程退出
    
    /* 子进程成为会话leader */
    sid = setsid();
    if (sid < 0) return -1;
    
    /* 第二次fork - 防止重新获得控制终端 */
    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(0);      // 第一个子进程退出
    
    /* 改变工作目录 */
    chdir("/");
    
    /* 设置文件权限掩码 */
    umask(0);
    
    /* 关闭所有打开的文件描述符 */
    int max_fd = sysconf(_SC_OPEN_MAX);
    for (int fd = 0; fd < max_fd; fd++) {
        close(fd);
    }
    
    /* 重定向标准输入输出错误到/dev/null */
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, 0);  // stdin
    dup2(fd, 1);  // stdout  
    dup2(fd, 2);  // stderr
    
    return 0;
}
```

### 2. SSH客户端实现中的PTY管理

```c
// SSH客户端中的伪终端和作业控制
int ssh_client_pty_setup() {
    int master_fd;
    pid_t pid;
    
    /* 创建伪终端 */
    master_fd = posix_openpt(O_RDWR);
    grantpt(master_fd);
    unlockpt(master_fd);
    
    char *slave_name = ptsname(master_fd);
    
    pid = fork();
    if (pid == 0) {
        /* 子进程：处理本地终端输入输出 */
        int slave_fd = open(slave_name, O_RDWR);
        
        /* 成为会话leader */
        setsid();
        
        /* 设置控制终端 */
        ioctl(slave_fd, TIOCSCTTY, 1);
        
        /* 重定向标准输入输出 */
        dup2(slave_fd, 0);
        dup2(slave_fd, 1);
        dup2(slave_fd, 2);
        
        /* 执行本地shell或命令 */
        execl("/bin/bash", "bash", NULL);
    } else {
        /* 父进程：处理网络通信和master_fd之间的数据传输 */
        handle_ssh_pty_data(master_fd, ssh_socket);
    }
    
    return 0;
}
```

### 3. 容器中的init进程实现

```c
// 容器init进程 - 负责回收僵尸进程和信号处理
void container_init_sigchld(int sig) {
    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("回收子进程 %d\n", pid);
    }
}

int container_init_main() {
    /* 设置信号处理 */
    signal(SIGCHLD, container_init_sigchld);
    
    /* 忽略其他信号，除了SIGTERM用于优雅关闭 */
    for (int i = 1; i < NSIG; i++) {
        if (i != SIGCHLD && i != SIGTERM && i != SIGKILL && i != SIGSTOP) {
            signal(i, SIG_IGN);
        }
    }
    
    /* 启动主应用进程 */
    pid_t app_pid = fork();
    if (app_pid == 0) {
        /* 恢复信号处理 */
        for (int i = 1; i < NSIG; i++) {
            signal(i, SIG_DFL);
        }
        
        execv("/app/main", argv);
        exit(1);
    }
    
    /* init进程主循环 */
    while (1) {
        pause();  // 等待信号
    }
    
    return 0;
}
```

## 🎯 学习检查点

完成本模块后，你应该能够：

1. ✅ 理解session、process group、控制终端的层次关系
2. ✅ 掌握setsid()、setpgid()、tcsetpgrp()等关键系统调用
3. ✅ 理解shell如何实现前台/后台作业管理
4. ✅ 知道信号如何从终端传递到进程组
5. ✅ 能够实现简单的作业控制功能
6. ✅ 理解守护进程创建和PTY在SSH中的应用
7. ✅ 会调试作业控制相关的问题

---

**下一步**: 深入学习 [Ctrl+C 信号处理链路详解](06-signal-handling.md)