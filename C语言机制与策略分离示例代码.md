# C语言机制与策略分离示例代码

本文档通过多个可编译的用户空间C代码示例，帮助理解Linux内核中常见的"机制与策略分离"设计思想。

---

## 目录

1. [基本概念](#基本概念)
2. [示例1：调度器模型](#示例1调度器模型)
3. [示例2：文件系统VFS模型](#示例2文件系统vfs模型)
4. [示例3：网络过滤器模型](#示例3网络过滤器模型)
5. [编译与运行](#编译与运行)

---

## 基本概念

| 概念 | 说明 | 内核示例 |
|------|------|----------|
| **机制(Mechanism)** | 提供通用能力和框架，不关心具体行为 | VFS、调度框架、Netfilter hook |
| **策略(Policy)** | 决定具体行为，通过回调函数实现 | ext4、CFS调度、iptables规则 |
| **连接方式** | 结构体 + 函数指针 + 注册函数 | `struct file_operations` |

---

## 示例1：调度器模型

模拟Linux调度器的机制与策略分离。

### 代码：`scheduler_demo.c`

```c
/*
 * 调度器机制与策略分离示例
 * 
 * 机制：调度框架（运行队列、调度循环）
 * 策略：具体调度算法（FIFO、优先级）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================
 * 第一部分：数据结构定义
 *============================================*/

/* 任务结构 */
struct task {
    int pid;
    int priority;      /* 优先级：数字越小优先级越高 */
    char name[32];
    struct task *next;
};

/* 运行队列 */
struct run_queue {
    struct task *head;
    int count;
};

/*============================================
 * 第二部分：策略接口定义（ops结构体）
 *============================================*/

/* 调度类操作表 - 这是"机制"与"策略"的桥梁 */
struct sched_class {
    const char *name;
    
    /* 入队：将任务加入运行队列 */
    void (*enqueue)(struct run_queue *rq, struct task *t);
    
    /* 选择下一个任务 */
    struct task *(*pick_next)(struct run_queue *rq);
    
    /* 出队：从运行队列移除任务 */
    void (*dequeue)(struct run_queue *rq, struct task *t);
};

/*============================================
 * 第三部分：策略实现1 - FIFO调度
 *============================================*/

/* FIFO入队：追加到队尾 */
static void fifo_enqueue(struct run_queue *rq, struct task *t)
{
    t->next = NULL;
    if (!rq->head) {
        rq->head = t;
    } else {
        struct task *p = rq->head;
        while (p->next)
            p = p->next;
        p->next = t;
    }
    rq->count++;
    printf("  [FIFO] 入队: %s (pid=%d)\n", t->name, t->pid);
}

/* FIFO选择：取队首 */
static struct task *fifo_pick_next(struct run_queue *rq)
{
    return rq->head;
}

/* FIFO出队：移除队首 */
static void fifo_dequeue(struct run_queue *rq, struct task *t)
{
    if (rq->head == t) {
        rq->head = t->next;
        rq->count--;
    }
}

/* FIFO调度类实例 */
static struct sched_class fifo_sched = {
    .name      = "FIFO",
    .enqueue   = fifo_enqueue,
    .pick_next = fifo_pick_next,
    .dequeue   = fifo_dequeue,
};

/*============================================
 * 第四部分：策略实现2 - 优先级调度
 *============================================*/

/* 优先级入队：按优先级插入（小数字优先） */
static void prio_enqueue(struct run_queue *rq, struct task *t)
{
    t->next = NULL;
    
    /* 空队列或优先级最高，插入队首 */
    if (!rq->head || t->priority < rq->head->priority) {
        t->next = rq->head;
        rq->head = t;
    } else {
        /* 找到合适位置插入 */
        struct task *p = rq->head;
        while (p->next && p->next->priority <= t->priority)
            p = p->next;
        t->next = p->next;
        p->next = t;
    }
    rq->count++;
    printf("  [PRIO] 入队: %s (pid=%d, prio=%d)\n", 
           t->name, t->pid, t->priority);
}

/* 优先级选择：取队首（已排序） */
static struct task *prio_pick_next(struct run_queue *rq)
{
    return rq->head;
}

/* 优先级出队 */
static void prio_dequeue(struct run_queue *rq, struct task *t)
{
    if (rq->head == t) {
        rq->head = t->next;
        rq->count--;
    }
}

/* 优先级调度类实例 */
static struct sched_class prio_sched = {
    .name      = "Priority",
    .enqueue   = prio_enqueue,
    .pick_next = prio_pick_next,
    .dequeue   = prio_dequeue,
};

/*============================================
 * 第五部分：机制层 - 调度框架
 *============================================*/

/* 全局调度器状态 */
static struct {
    struct run_queue rq;
    struct sched_class *sched;   /* 当前使用的调度策略 */
} scheduler;

/* 初始化调度器 */
void scheduler_init(struct sched_class *sc)
{
    scheduler.rq.head = NULL;
    scheduler.rq.count = 0;
    scheduler.sched = sc;
    printf("调度器初始化，使用策略: %s\n", sc->name);
}

/* 添加任务 - 机制层调用策略层 */
void scheduler_add_task(struct task *t)
{
    scheduler.sched->enqueue(&scheduler.rq, t);
}

/* 调度循环 - 纯机制，不关心具体算法 */
void scheduler_run(void)
{
    struct task *t;
    
    printf("\n开始调度循环 [%s策略]:\n", scheduler.sched->name);
    printf("----------------------------------------\n");
    
    while ((t = scheduler.sched->pick_next(&scheduler.rq)) != NULL) {
        printf("  运行任务: %s (pid=%d, prio=%d)\n", 
               t->name, t->pid, t->priority);
        
        /* 模拟任务执行完成 */
        scheduler.sched->dequeue(&scheduler.rq, t);
        free(t);
    }
    
    printf("----------------------------------------\n");
    printf("所有任务完成\n\n");
}

/*============================================
 * 第六部分：测试主函数
 *============================================*/

static struct task *create_task(int pid, int prio, const char *name)
{
    struct task *t = malloc(sizeof(*t));
    t->pid = pid;
    t->priority = prio;
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->next = NULL;
    return t;
}

int main(void)
{
    printf("=== C语言机制与策略分离示例 ===\n\n");
    
    /* 测试1：使用FIFO策略 */
    printf("【测试1】FIFO调度策略\n");
    scheduler_init(&fifo_sched);
    scheduler_add_task(create_task(1, 3, "低优先级任务"));
    scheduler_add_task(create_task(2, 1, "高优先级任务"));
    scheduler_add_task(create_task(3, 2, "中优先级任务"));
    scheduler_run();
    
    /* 测试2：使用优先级策略 - 只需切换策略，机制代码不变 */
    printf("【测试2】优先级调度策略\n");
    scheduler_init(&prio_sched);
    scheduler_add_task(create_task(1, 3, "低优先级任务"));
    scheduler_add_task(create_task(2, 1, "高优先级任务"));
    scheduler_add_task(create_task(3, 2, "中优先级任务"));
    scheduler_run();
    
    return 0;
}
```

---

## 示例2：文件系统VFS模型

模拟Linux VFS的机制与策略分离。

### 代码：`vfs_demo.c`

```c
/*
 * VFS机制与策略分离示例
 * 
 * 机制：VFS框架（统一的文件操作接口）
 * 策略：具体文件系统（内存FS、磁盘FS）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================
 * 第一部分：核心数据结构
 *============================================*/

#define MAX_DATA 256

/* 文件结构 */
struct file {
    char name[64];
    char data[MAX_DATA];
    int size;
    int pos;                        /* 当前读写位置 */
    struct file_operations *f_op;   /* 文件操作表 */
    void *private_data;             /* 文件系统私有数据 */
};

/*============================================
 * 第二部分：操作表定义（策略接口）
 *============================================*/

/* 文件操作表 - 类似内核的 struct file_operations */
struct file_operations {
    const char *fs_name;
    int (*open)(struct file *f);
    int (*read)(struct file *f, char *buf, int len);
    int (*write)(struct file *f, const char *buf, int len);
    int (*close)(struct file *f);
};

/*============================================
 * 第三部分：策略实现1 - 内存文件系统
 *============================================*/

static int memfs_open(struct file *f)
{
    printf("  [MemFS] 打开文件: %s\n", f->name);
    f->pos = 0;
    return 0;
}

static int memfs_read(struct file *f, char *buf, int len)
{
    int avail = f->size - f->pos;
    int n = (len < avail) ? len : avail;
    
    if (n > 0) {
        memcpy(buf, f->data + f->pos, n);
        f->pos += n;
    }
    printf("  [MemFS] 读取 %d 字节\n", n);
    return n;
}

static int memfs_write(struct file *f, const char *buf, int len)
{
    int space = MAX_DATA - f->pos;
    int n = (len < space) ? len : space;
    
    if (n > 0) {
        memcpy(f->data + f->pos, buf, n);
        f->pos += n;
        if (f->pos > f->size)
            f->size = f->pos;
    }
    printf("  [MemFS] 写入 %d 字节\n", n);
    return n;
}

static int memfs_close(struct file *f)
{
    printf("  [MemFS] 关闭文件: %s\n", f->name);
    return 0;
}

/* 内存文件系统操作表 */
static struct file_operations memfs_ops = {
    .fs_name = "MemFS",
    .open    = memfs_open,
    .read    = memfs_read,
    .write   = memfs_write,
    .close   = memfs_close,
};

/*============================================
 * 第四部分：策略实现2 - 加密文件系统
 *============================================*/

/* 简单异或加密 */
static void xor_encrypt(char *data, int len, char key)
{
    for (int i = 0; i < len; i++)
        data[i] ^= key;
}

static int encfs_open(struct file *f)
{
    printf("  [EncFS] 打开加密文件: %s\n", f->name);
    f->pos = 0;
    return 0;
}

static int encfs_read(struct file *f, char *buf, int len)
{
    int avail = f->size - f->pos;
    int n = (len < avail) ? len : avail;
    
    if (n > 0) {
        memcpy(buf, f->data + f->pos, n);
        xor_encrypt(buf, n, 0x5A);  /* 解密 */
        f->pos += n;
    }
    printf("  [EncFS] 读取并解密 %d 字节\n", n);
    return n;
}

static int encfs_write(struct file *f, const char *buf, int len)
{
    int space = MAX_DATA - f->pos;
    int n = (len < space) ? len : space;
    
    if (n > 0) {
        memcpy(f->data + f->pos, buf, n);
        xor_encrypt(f->data + f->pos, n, 0x5A);  /* 加密 */
        f->pos += n;
        if (f->pos > f->size)
            f->size = f->pos;
    }
    printf("  [EncFS] 加密并写入 %d 字节\n", n);
    return n;
}

static int encfs_close(struct file *f)
{
    printf("  [EncFS] 关闭加密文件: %s\n", f->name);
    return 0;
}

/* 加密文件系统操作表 */
static struct file_operations encfs_ops = {
    .fs_name = "EncFS",
    .open    = encfs_open,
    .read    = encfs_read,
    .write   = encfs_write,
    .close   = encfs_close,
};

/*============================================
 * 第五部分：机制层 - VFS框架
 *============================================*/

/* VFS层：创建文件 */
struct file *vfs_create(const char *name, struct file_operations *ops)
{
    struct file *f = calloc(1, sizeof(*f));
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->f_op = ops;
    printf("VFS创建文件: %s (使用%s)\n", name, ops->fs_name);
    return f;
}

/* VFS层：打开文件 - 调用具体FS的open */
int vfs_open(struct file *f)
{
    if (f->f_op && f->f_op->open)
        return f->f_op->open(f);
    return -1;
}

/* VFS层：读文件 */
int vfs_read(struct file *f, char *buf, int len)
{
    if (f->f_op && f->f_op->read)
        return f->f_op->read(f, buf, len);
    return -1;
}

/* VFS层：写文件 */
int vfs_write(struct file *f, const char *buf, int len)
{
    if (f->f_op && f->f_op->write)
        return f->f_op->write(f, buf, len);
    return -1;
}

/* VFS层：关闭文件 */
int vfs_close(struct file *f)
{
    int ret = 0;
    if (f->f_op && f->f_op->close)
        ret = f->f_op->close(f);
    free(f);
    return ret;
}

/*============================================
 * 第六部分：测试主函数
 *============================================*/

int main(void)
{
    char buf[64];
    
    printf("=== VFS机制与策略分离示例 ===\n\n");
    
    /* 测试1：普通内存文件系统 */
    printf("【测试1】MemFS - 普通内存文件系统\n");
    struct file *f1 = vfs_create("test.txt", &memfs_ops);
    vfs_open(f1);
    vfs_write(f1, "Hello World!", 12);
    f1->pos = 0;  /* 重置位置 */
    memset(buf, 0, sizeof(buf));
    vfs_read(f1, buf, sizeof(buf));
    printf("  读取内容: \"%s\"\n", buf);
    vfs_close(f1);
    
    printf("\n");
    
    /* 测试2：加密文件系统 - VFS代码完全不变 */
    printf("【测试2】EncFS - 加密文件系统\n");
    struct file *f2 = vfs_create("secret.txt", &encfs_ops);
    vfs_open(f2);
    vfs_write(f2, "Secret Data!", 12);
    f2->pos = 0;
    memset(buf, 0, sizeof(buf));
    vfs_read(f2, buf, sizeof(buf));
    printf("  读取内容: \"%s\"\n", buf);
    vfs_close(f2);
    
    return 0;
}
```

---

## 示例3：网络过滤器模型

模拟Linux Netfilter的hook机制。

### 代码：`netfilter_demo.c`

```c
/*
 * Netfilter机制与策略分离示例
 * 
 * 机制：Hook框架（在数据包处理路径上提供挂载点）
 * 策略：过滤规则（防火墙、NAT等）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================
 * 第一部分：基础定义
 *============================================*/

/* Hook返回值 */
#define NF_ACCEPT  0   /* 接受数据包 */
#define NF_DROP    1   /* 丢弃数据包 */

/* Hook点定义 */
enum nf_hook_point {
    NF_PREROUTING,
    NF_INPUT,
    NF_FORWARD,
    NF_OUTPUT,
    NF_POSTROUTING,
    NF_MAX_HOOKS
};

static const char *hook_names[] = {
    "PREROUTING", "INPUT", "FORWARD", "OUTPUT", "POSTROUTING"
};

/* 模拟网络数据包 */
struct packet {
    char src_ip[16];
    char dst_ip[16];
    int src_port;
    int dst_port;
    char protocol[8];
    char data[64];
};

/*============================================
 * 第二部分：Hook框架（机制层）
 *============================================*/

/* Hook回调函数类型 */
typedef int (*nf_hook_fn)(struct packet *pkt, void *priv);

/* Hook节点 */
struct nf_hook_entry {
    nf_hook_fn hook;
    void *priv;
    int priority;
    const char *name;
    struct nf_hook_entry *next;
};

/* Hook链表头 */
static struct nf_hook_entry *hooks[NF_MAX_HOOKS];

/* 注册Hook - 机制层提供的API */
int nf_register_hook(enum nf_hook_point point, nf_hook_fn fn,
                     void *priv, int priority, const char *name)
{
    struct nf_hook_entry *entry = malloc(sizeof(*entry));
    entry->hook = fn;
    entry->priv = priv;
    entry->priority = priority;
    entry->name = name;
    entry->next = NULL;
    
    /* 按优先级插入 */
    struct nf_hook_entry **pp = &hooks[point];
    while (*pp && (*pp)->priority < priority)
        pp = &(*pp)->next;
    entry->next = *pp;
    *pp = entry;
    
    printf("注册Hook: %s 在 %s (优先级=%d)\n", 
           name, hook_names[point], priority);
    return 0;
}

/* 执行Hook链 - 机制层核心 */
int nf_hook(enum nf_hook_point point, struct packet *pkt)
{
    struct nf_hook_entry *e;
    int verdict = NF_ACCEPT;
    
    printf("\n[%s] 处理数据包: %s:%d -> %s:%d\n",
           hook_names[point], 
           pkt->src_ip, pkt->src_port,
           pkt->dst_ip, pkt->dst_port);
    
    for (e = hooks[point]; e != NULL; e = e->next) {
        verdict = e->hook(pkt, e->priv);
        printf("  ├─ [%s] 判定: %s\n", 
               e->name, verdict == NF_ACCEPT ? "ACCEPT" : "DROP");
        
        if (verdict == NF_DROP)
            break;
    }
    
    printf("  └─ 最终结果: %s\n", 
           verdict == NF_ACCEPT ? "通过" : "丢弃");
    return verdict;
}

/*============================================
 * 第三部分：策略实现1 - 简单防火墙
 *============================================*/

/* 防火墙规则 */
struct fw_rule {
    char blocked_ip[16];
};

static int firewall_hook(struct packet *pkt, void *priv)
{
    struct fw_rule *rule = priv;
    
    /* 检查是否匹配阻止规则 */
    if (strcmp(pkt->src_ip, rule->blocked_ip) == 0)
        return NF_DROP;
    
    return NF_ACCEPT;
}

/* 创建防火墙规则 */
static struct fw_rule fw_rule1 = { .blocked_ip = "10.0.0.100" };

/*============================================
 * 第四部分：策略实现2 - 端口过滤
 *============================================*/

struct port_filter {
    int blocked_port;
};

static int port_filter_hook(struct packet *pkt, void *priv)
{
    struct port_filter *pf = priv;
    
    if (pkt->dst_port == pf->blocked_port)
        return NF_DROP;
    
    return NF_ACCEPT;
}

static struct port_filter pf_rule = { .blocked_port = 23 };  /* 阻止telnet */

/*============================================
 * 第五部分：策略实现3 - 日志记录
 *============================================*/

static int logging_hook(struct packet *pkt, void *priv)
{
    (void)priv;
    printf("  │  [LOG] %s %s:%d -> %s:%d\n",
           pkt->protocol, pkt->src_ip, pkt->src_port,
           pkt->dst_ip, pkt->dst_port);
    return NF_ACCEPT;  /* 日志不影响数据包 */
}

/*============================================
 * 第六部分：测试主函数
 *============================================*/

int main(void)
{
    printf("=== Netfilter机制与策略分离示例 ===\n\n");
    
    /* 注册各种策略Hook */
    printf("【注册Hook】\n");
    nf_register_hook(NF_INPUT, logging_hook, NULL, 0, "Logger");
    nf_register_hook(NF_INPUT, firewall_hook, &fw_rule1, 10, "Firewall");
    nf_register_hook(NF_INPUT, port_filter_hook, &pf_rule, 20, "PortFilter");
    
    /* 模拟数据包处理 */
    printf("\n【数据包处理测试】\n");
    
    /* 测试1：正常数据包 */
    struct packet pkt1 = {
        .src_ip = "192.168.1.1",
        .dst_ip = "192.168.1.100",
        .src_port = 12345,
        .dst_port = 80,
        .protocol = "TCP"
    };
    nf_hook(NF_INPUT, &pkt1);
    
    /* 测试2：被防火墙阻止的IP */
    struct packet pkt2 = {
        .src_ip = "10.0.0.100",    /* 被阻止的IP */
        .dst_ip = "192.168.1.100",
        .src_port = 54321,
        .dst_port = 80,
        .protocol = "TCP"
    };
    nf_hook(NF_INPUT, &pkt2);
    
    /* 测试3：被端口过滤阻止 */
    struct packet pkt3 = {
        .src_ip = "192.168.1.50",
        .dst_ip = "192.168.1.100",
        .src_port = 11111,
        .dst_port = 23,            /* telnet端口被阻止 */
        .protocol = "TCP"
    };
    nf_hook(NF_INPUT, &pkt3);
    
    return 0;
}
```

---

## 编译与运行

### 编译命令

```bash
# 编译调度器示例
gcc -o scheduler_demo scheduler_demo.c -Wall

# 编译VFS示例
gcc -o vfs_demo vfs_demo.c -Wall

# 编译Netfilter示例
gcc -o netfilter_demo netfilter_demo.c -Wall
```

### 预期输出

#### 调度器示例输出

```
=== C语言机制与策略分离示例 ===

【测试1】FIFO调度策略
调度器初始化，使用策略: FIFO
  [FIFO] 入队: 低优先级任务 (pid=1)
  [FIFO] 入队: 高优先级任务 (pid=2)
  [FIFO] 入队: 中优先级任务 (pid=3)

开始调度循环 [FIFO策略]:
----------------------------------------
  运行任务: 低优先级任务 (pid=1, prio=3)
  运行任务: 高优先级任务 (pid=2, prio=1)
  运行任务: 中优先级任务 (pid=3, prio=2)
----------------------------------------
所有任务完成

【测试2】优先级调度策略
调度器初始化，使用策略: Priority
  [PRIO] 入队: 低优先级任务 (pid=1, prio=3)
  [PRIO] 入队: 高优先级任务 (pid=2, prio=1)
  [PRIO] 入队: 中优先级任务 (pid=3, prio=2)

开始调度循环 [Priority策略]:
----------------------------------------
  运行任务: 高优先级任务 (pid=2, prio=1)
  运行任务: 中优先级任务 (pid=3, prio=2)
  运行任务: 低优先级任务 (pid=1, prio=3)
----------------------------------------
所有任务完成
```

---

## 设计要点总结

### 机制与策略分离的核心模式

```
┌─────────────────────────────────────────────────────────┐
│                    应用层 / 用户代码                      │
│         调用统一接口：vfs_read(), scheduler_run()         │
└─────────────────────────┬───────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    机制层 (Mechanism)                    │
│  • 定义操作表结构体 (struct xxx_operations)               │
│  • 提供注册/注销函数                                      │
│  • 实现通用流程，通过函数指针调用策略                       │
└─────────────────────────┬───────────────────────────────┘
                          │ 函数指针调用
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    策略层 (Policy)                       │
│  • 实现具体的回调函数                                     │
│  • 填充操作表结构体                                       │
│  • 通过注册函数挂载到机制层                               │
└─────────────────────────────────────────────────────────┘
```

### 关键代码模式

```c
/* 1. 定义操作表（接口） */
struct xxx_operations {
    int (*op1)(args...);
    int (*op2)(args...);
};

/* 2. 策略实现 */
static int my_op1(args...) { /* 具体实现 */ }
static int my_op2(args...) { /* 具体实现 */ }

static struct xxx_operations my_ops = {
    .op1 = my_op1,
    .op2 = my_op2,
};

/* 3. 机制层调用 */
void mechanism_do_something(struct xxx_operations *ops)
{
    if (ops && ops->op1)
        ops->op1(args...);  /* 通过函数指针调用策略 */
}
```

### 优势

| 优势 | 说明 |
|------|------|
| **解耦** | 机制代码和策略代码独立开发、测试 |
| **可扩展** | 新增策略只需实现操作表，不改机制代码 |
| **可配置** | 运行时可切换不同策略 |
| **复用** | 多种策略共享同一套机制代码 |

---

## 与Linux内核的对应关系

| 本文示例 | Linux内核对应 |
|----------|---------------|
| `struct sched_class` | `kernel/sched/sched.h` 中的 `struct sched_class` |
| `struct file_operations` | `include/linux/fs.h` 中的 `struct file_operations` |
| `nf_hook_fn` | `include/linux/netfilter.h` 中的 `nf_hookfn` |
| `fifo_sched` / `prio_sched` | `kernel/sched/fair.c` (CFS) / `kernel/sched/rt.c` (RT) |
| `memfs_ops` / `encfs_ops` | `fs/ext4/file.c` / `fs/xfs/xfs_file.c` 等 |

