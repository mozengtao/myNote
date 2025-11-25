# Linux 内核中机制与策略分离的典型场景

## 概述

**机制与策略分离（Separation of Mechanism and Policy）** 是 Linux 内核设计的核心哲学之一：

- **机制（Mechanism）**：提供"能做什么"的能力，不关心"怎么决定做什么"
- **策略（Policy）**：决定"在什么情况下做什么"，利用机制提供的能力

这种分离通过 **结构体 + 函数指针（ops 表）** 实现，使内核具有高度的可扩展性和灵活性。

---

## 1. CPU 调度器（Scheduler）

### 机制
核心调度框架：`schedule()`、运行队列 `rq`、上下文切换、抢占点等。

### 策略
具体调度类：CFS（完全公平调度）、RT（实时调度）、DEADLINE（截止期调度）。

### 分离好处
- 新增调度算法只需实现 `sched_class` 回调，不改动核心框架
- 不同类型进程可使用不同调度策略

### 核心代码框架

```c
/* 机制：调度类抽象接口 (kernel/sched/sched.h) */
struct sched_class {
    void (*enqueue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task)(struct rq *rq, struct task_struct *p, int flags);
    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
    /* ... 更多回调 ... */
};

/* 策略：CFS 调度类实现 (kernel/sched/fair.c) */
const struct sched_class fair_sched_class = {
    .enqueue_task       = enqueue_task_fair,
    .dequeue_task       = dequeue_task_fair,
    .pick_next_task     = pick_next_task_fair,
    .task_tick          = task_tick_fair,
};

/* 策略：RT 调度类实现 (kernel/sched/rt.c) */
const struct sched_class rt_sched_class = {
    .enqueue_task       = enqueue_task_rt,
    .dequeue_task       = dequeue_task_rt,
    .pick_next_task     = pick_next_task_rt,
    .task_tick          = task_tick_rt,
};

/* 机制：核心调度函数调用策略 */
void schedule(void)
{
    struct task_struct *next;
    struct rq *rq = this_rq();
    
    /* 通过当前进程的调度类选择下一个进程 */
    next = rq->curr->sched_class->pick_next_task(rq);
    
    if (next != rq->curr)
        context_switch(rq, rq->curr, next);
}
```

---

## 2. 虚拟文件系统（VFS）

### 机制
VFS 层：路径解析、dentry 缓存、权限检查、通用读写流程。

### 策略
具体文件系统：ext4、xfs、btrfs、tmpfs 等。

### 分离好处
- VFS 不关心磁盘数据布局
- 新增文件系统只需实现操作表
- 用户态程序使用统一的系统调用接口

### 核心代码框架

```c
/* 机制：文件操作抽象接口 (include/linux/fs.h) */
struct file_operations {
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    int (*open)(struct inode *, struct file *);
    int (*release)(struct inode *, struct file *);
    int (*mmap)(struct file *, struct vm_area_struct *);
    /* ... 更多回调 ... */
};

struct inode_operations {
    struct dentry *(*lookup)(struct inode *, struct dentry *, unsigned int);
    int (*create)(struct inode *, struct dentry *, umode_t, bool);
    int (*mkdir)(struct inode *, struct dentry *, umode_t);
    int (*unlink)(struct inode *, struct dentry *);
    /* ... 更多回调 ... */
};

/* 策略：ext4 文件系统实现 (fs/ext4/file.c) */
const struct file_operations ext4_file_operations = {
    .read_iter      = ext4_file_read_iter,
    .write_iter     = ext4_file_write_iter,
    .open           = ext4_file_open,
    .release        = ext4_release_file,
    .mmap           = ext4_file_mmap,
};

/* 策略：tmpfs 文件系统实现 (mm/shmem.c) */
const struct file_operations shmem_file_operations = {
    .read_iter      = shmem_file_read_iter,
    .write_iter     = generic_file_write_iter,
    .mmap           = shmem_mmap,
};

/* 机制：VFS 层通过 ops 调用具体实现 */
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    if (file->f_op->read)
        return file->f_op->read(file, buf, count, pos);
    else if (file->f_op->read_iter)
        return new_sync_read(file, buf, count, pos);
    return -EINVAL;
}
```

---

## 3. Netfilter 防火墙框架

### 机制
Netfilter hook 框架：在协议栈关键路径注册钩子点。

### 策略
iptables/nftables 规则：允许、丢弃、NAT、mangle 等动作。

### 分离好处
- Hook 机制稳定，策略由用户空间配置
- 同一机制可实现防火墙、NAT、负载均衡等不同功能

### 核心代码框架

```c
/* 机制：Netfilter hook 点定义 (include/uapi/linux/netfilter.h) */
enum nf_inet_hooks {
    NF_INET_PRE_ROUTING,
    NF_INET_LOCAL_IN,
    NF_INET_FORWARD,
    NF_INET_LOCAL_OUT,
    NF_INET_POST_ROUTING,
    NF_INET_NUMHOOKS
};

/* 机制：hook 操作结构 (include/linux/netfilter.h) */
struct nf_hook_ops {
    nf_hookfn           *hook;      /* 回调函数 */
    struct net_device   *dev;
    int                 pf;         /* 协议族 */
    unsigned int        hooknum;    /* hook 点 */
    int                 priority;   /* 优先级 */
};

/* 策略：iptables filter 表实现 */
static unsigned int iptable_filter_hook(void *priv,
                                        struct sk_buff *skb,
                                        const struct nf_hook_state *state)
{
    /* 根据规则表匹配并执行动作 */
    return ipt_do_table(skb, state, priv);
}

static struct nf_hook_ops filter_ops[] = {
    {
        .hook       = iptable_filter_hook,
        .pf         = NFPROTO_IPV4,
        .hooknum    = NF_INET_LOCAL_IN,
        .priority   = NF_IP_PRI_FILTER,
    },
    /* ... 更多 hook ... */
};

/* 机制：注册 hook */
int nf_register_net_hooks(struct net *net,
                          const struct nf_hook_ops *reg,
                          unsigned int n);

/* 机制：协议栈调用 hook 链 */
int NF_HOOK(uint8_t pf, unsigned int hook, struct net *net,
            struct sock *sk, struct sk_buff *skb,
            struct net_device *in, struct net_device *out,
            int (*okfn)(struct net *, struct sock *, struct sk_buff *))
{
    /* 遍历该 hook 点上注册的所有回调 */
    return nf_hook(pf, hook, net, sk, skb, in, out, okfn);
}
```

---

## 4. Traffic Control（tc）流量控制

### 机制
qdisc 框架：统一的队列抽象，enqueue/dequeue 接口。

### 策略
具体队列算法：pfifo、fq_codel、HTB、TBF 等。

### 分离好处
- 发包路径只依赖抽象 qdisc
- 可通过 tc 命令动态更换限速/AQM 策略

### 核心代码框架

```c
/* 机制：qdisc 操作抽象接口 (include/net/sch_generic.h) */
struct Qdisc_ops {
    const char          *id;
    int                 (*enqueue)(struct sk_buff *skb, struct Qdisc *sch,
                                   struct sk_buff **to_free);
    struct sk_buff *    (*dequeue)(struct Qdisc *sch);
    int                 (*init)(struct Qdisc *sch, struct nlattr *arg,
                                struct netlink_ext_ack *extack);
    void                (*reset)(struct Qdisc *sch);
    void                (*destroy)(struct Qdisc *sch);
    /* ... 更多回调 ... */
};

struct Qdisc {
    const struct Qdisc_ops  *ops;
    u32                     handle;
    u32                     parent;
    struct netdev_queue     *dev_queue;
    /* ... 更多字段 ... */
};

/* 策略：pfifo 队列实现 (net/sched/sch_fifo.c) */
static int pfifo_enqueue(struct sk_buff *skb, struct Qdisc *sch,
                         struct sk_buff **to_free)
{
    if (sch->q.qlen < sch->limit)
        return qdisc_enqueue_tail(skb, sch);
    return qdisc_drop(skb, sch, to_free);
}

static struct sk_buff *pfifo_dequeue(struct Qdisc *sch)
{
    return qdisc_dequeue_head(sch);
}

static struct Qdisc_ops pfifo_qdisc_ops = {
    .id         = "pfifo",
    .enqueue    = pfifo_enqueue,
    .dequeue    = pfifo_dequeue,
    /* ... */
};

/* 策略：fq_codel 队列实现 (net/sched/sch_fq_codel.c) */
static struct Qdisc_ops fq_codel_qdisc_ops = {
    .id         = "fq_codel",
    .enqueue    = fq_codel_enqueue,
    .dequeue    = fq_codel_dequeue,
    /* ... */
};

/* 机制：发包时调用 qdisc */
int dev_queue_xmit(struct sk_buff *skb)
{
    struct Qdisc *q = dev->qdisc;
    
    /* 通过 ops 调用具体策略 */
    rc = q->ops->enqueue(skb, q, &to_free);
    
    /* 触发发送 */
    __qdisc_run(q);
    return rc;
}
```

---

## 5. CPUFreq 频率调节

### 机制
cpufreq 核心框架：统一的频率控制 API，与硬件驱动交互。

### 策略
governors：performance、powersave、ondemand、schedutil 等。

### 分离好处
- 不同平台共用一套 cpufreq 机制
- 运行时可动态切换 governor

### 核心代码框架

```c
/* 机制：governor 抽象接口 (include/linux/cpufreq.h) */
struct cpufreq_governor {
    char                    name[CPUFREQ_NAME_LEN];
    int (*init)(struct cpufreq_policy *policy);
    void (*exit)(struct cpufreq_policy *policy);
    int (*start)(struct cpufreq_policy *policy);
    void (*stop)(struct cpufreq_policy *policy);
    void (*limits)(struct cpufreq_policy *policy);
    /* ... */
};

/* 策略：performance governor (drivers/cpufreq/cpufreq_performance.c) */
static void cpufreq_gov_performance_limits(struct cpufreq_policy *policy)
{
    /* 始终使用最高频率 */
    __cpufreq_driver_target(policy, policy->max, CPUFREQ_RELATION_H);
}

static struct cpufreq_governor cpufreq_gov_performance = {
    .name       = "performance",
    .init       = cpufreq_gov_performance_init,
    .limits     = cpufreq_gov_performance_limits,
};

/* 策略：ondemand governor (drivers/cpufreq/cpufreq_ondemand.c) */
static void od_dbs_update(struct cpufreq_policy *policy)
{
    unsigned int load = dbs_update(policy);
    
    /* 根据负载动态调整频率 */
    if (load > od_tuners->up_threshold)
        __cpufreq_driver_target(policy, policy->max, CPUFREQ_RELATION_H);
    else
        __cpufreq_driver_target(policy, freq_next, CPUFREQ_RELATION_L);
}

static struct cpufreq_governor cpufreq_gov_ondemand = {
    .name       = "ondemand",
    .init       = od_init,
    .exit       = od_exit,
    .start      = od_start,
    .stop       = od_stop,
    .limits     = od_limits,
};

/* 机制：注册 governor */
int cpufreq_register_governor(struct cpufreq_governor *governor);
```

---

## 6. I/O 调度器（Block Layer）

### 机制
块层请求队列框架：组织读写请求，提供插入、合并、出队接口。

### 策略
I/O 调度算法：mq-deadline、kyber、BFQ 等。

### 分离好处
- 块层机制统一
- 策略可按设备特性灵活选择

### 核心代码框架

```c
/* 机制：电梯（调度器）操作接口 (include/linux/elevator.h) */
struct elevator_mq_ops {
    int (*init_sched)(struct request_queue *, struct elevator_type *);
    void (*exit_sched)(struct elevator_queue *);
    bool (*allow_merge)(struct request_queue *, struct request *,
                        struct bio *);
    void (*insert_requests)(struct blk_mq_hw_ctx *,
                            struct list_head *, bool);
    struct request *(*dispatch_request)(struct blk_mq_hw_ctx *);
    /* ... */
};

struct elevator_type {
    struct elevator_mq_ops  ops;
    const char              *elevator_name;
    /* ... */
};

/* 策略：mq-deadline 调度器 (block/mq-deadline.c) */
static struct request *dd_dispatch_request(struct blk_mq_hw_ctx *hctx)
{
    struct deadline_data *dd = hctx->queue->elevator->elevator_data;
    
    /* 按截止时间和方向选择请求 */
    if (!list_empty(&dd->fifo_list[READ]) && deadline_check(dd, READ))
        return deadline_next_request(dd, READ);
    /* ... */
}

static struct elevator_type mq_deadline = {
    .ops = {
        .insert_requests    = dd_insert_requests,
        .dispatch_request   = dd_dispatch_request,
        .init_sched         = dd_init_sched,
        .exit_sched         = dd_exit_sched,
    },
    .elevator_name = "mq-deadline",
};

/* 策略：BFQ 调度器 (block/bfq-iosched.c) */
static struct elevator_type iosched_bfq_mq = {
    .ops = {
        .insert_requests    = bfq_insert_requests,
        .dispatch_request   = bfq_dispatch_request,
        /* ... */
    },
    .elevator_name = "bfq",
};

/* 机制：注册调度器 */
int elv_register(struct elevator_type *e);
```

---

## 7. Linux 安全模块（LSM）

### 机制
LSM hook 框架：在安全敏感操作点提供回调。

### 策略
安全模块：SELinux、AppArmor、Smack 等。

### 分离好处
- 内核核心不内嵌特定安全策略
- 不同安全模型可并存或切换

### 核心代码框架

```c
/* 机制：LSM hook 定义 (include/linux/lsm_hooks.h) */
union security_list_options {
    int (*bprm_check_security)(struct linux_binprm *bprm);
    int (*file_permission)(struct file *file, int mask);
    int (*inode_permission)(struct inode *inode, int mask);
    int (*socket_create)(int family, int type, int protocol, int kern);
    /* ... 数百个 hook ... */
};

struct security_hook_list {
    struct hlist_node       list;
    struct hlist_head       *head;
    union security_list_options hook;
    const char              *lsm;
};

/* 策略：SELinux 实现 (security/selinux/hooks.c) */
static int selinux_file_permission(struct file *file, int mask)
{
    struct inode *inode = file_inode(file);
    /* 检查 SELinux 策略是否允许该操作 */
    return file_has_perm(current_cred(), file, file_to_av(file));
}

static struct security_hook_list selinux_hooks[] = {
    LSM_HOOK_INIT(file_permission, selinux_file_permission),
    LSM_HOOK_INIT(inode_permission, selinux_inode_permission),
    /* ... */
};

/* 策略：AppArmor 实现 (security/apparmor/lsm.c) */
static int apparmor_file_permission(struct file *file, int mask)
{
    /* 检查 AppArmor profile 是否允许该操作 */
    return aa_file_perm(OP_FPERM, current_cred(), file, mask);
}

static struct security_hook_list apparmor_hooks[] = {
    LSM_HOOK_INIT(file_permission, apparmor_file_permission),
    /* ... */
};

/* 机制：调用 LSM hook */
int security_file_permission(struct file *file, int mask)
{
    /* 遍历所有注册的安全模块 */
    return call_int_hook(file_permission, 0, file, mask);
}
```

---

## 8. Cgroups 资源控制

### 机制
cgroup 核心：分层组织、资源计量接口。

### 策略
控制器：cpu、memory、io、pids 等；用户空间配置。

### 分离好处
- 内核提供资源限制能力
- 容器运行时决定具体限制策略

### 核心代码框架

```c
/* 机制：cgroup 子系统接口 (include/linux/cgroup-defs.h) */
struct cgroup_subsys {
    struct cgroup_subsys_state *(*css_alloc)(struct cgroup_subsys_state *parent);
    void (*css_free)(struct cgroup_subsys_state *css);
    int (*css_online)(struct cgroup_subsys_state *css);
    void (*css_offline)(struct cgroup_subsys_state *css);
    void (*attach)(struct cgroup_taskset *tset);
    void (*fork)(struct task_struct *task);
    void (*exit)(struct task_struct *task);
    /* ... */
    const char *name;
    int id;
};

/* 策略：CPU 控制器 (kernel/sched/core.c) */
struct cgroup_subsys cpu_cgrp_subsys = {
    .css_alloc      = cpu_cgroup_css_alloc,
    .css_free       = cpu_cgroup_css_free,
    .css_online     = cpu_cgroup_css_online,
    .attach         = cpu_cgroup_attach,
    .fork           = cpu_cgroup_fork,
    .name           = "cpu",
};

/* 策略：内存控制器 (mm/memcontrol.c) */
struct cgroup_subsys memory_cgrp_subsys = {
    .css_alloc      = mem_cgroup_css_alloc,
    .css_free       = mem_cgroup_css_free,
    .css_online     = mem_cgroup_css_online,
    .attach         = mem_cgroup_attach,
    .name           = "memory",
};

/* 机制：注册子系统 */
#define SUBSYS(_x) [_x ## _cgrp_id] = &_x ## _cgrp_subsys,
struct cgroup_subsys *cgroup_subsys[] = {
    #include <linux/cgroup_subsys.h>
};
```

---

## 总结

| 子系统 | 机制 | 策略 | 核心接口 |
|--------|------|------|----------|
| 调度器 | schedule() 框架 | CFS/RT/DEADLINE | `struct sched_class` |
| VFS | 虚拟文件系统层 | ext4/xfs/btrfs | `struct file_operations` |
| Netfilter | hook 框架 | iptables 规则 | `struct nf_hook_ops` |
| tc | qdisc 框架 | pfifo/fq_codel/HTB | `struct Qdisc_ops` |
| CPUFreq | 频率控制框架 | governors | `struct cpufreq_governor` |
| Block | 块层队列 | mq-deadline/BFQ | `struct elevator_mq_ops` |
| LSM | 安全 hook 框架 | SELinux/AppArmor | `struct security_hook_list` |
| Cgroups | 资源控制框架 | cpu/memory/io 控制器 | `struct cgroup_subsys` |

### 设计模式要点

1. **结构体 + 函数指针**：所有策略通过 ops 结构体注册
2. **注册/注销函数**：`register_*()` / `unregister_*()` 系列
3. **运行时可替换**：策略可在运行时动态切换
4. **模块化**：策略可编译为独立内核模块
5. **统一抽象**：上层代码只依赖抽象接口，不依赖具体实现

