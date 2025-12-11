# Interrupt and Exception Flashcards for Linux Kernel Implementation

## 基础概念 (Basic Concepts)

Q: [Basic] 中断(Interrupt)和异常(Exception)的根本区别是什么？
A: **中断(Interrupt)**：由外部硬件设备异步产生，与当前执行指令无关
**异常(Exception)**：由CPU同步产生，是当前指令执行的直接结果
```
+------------------+-------------------+
|     Interrupt    |     Exception     |
+------------------+-------------------+
| Asynchronous     | Synchronous       |
| External device  | CPU generated     |
| Timer, NIC, Disk | Page Fault, #DE   |
| IRQ number       | Exception vector  |
+------------------+-------------------+
```

Q: [Basic] Linux内核中，x86架构的中断向量表如何划分？
A: x86架构256个中断向量分配：
```
Vector 0-31:   CPU异常 (Exception)
  0:  #DE - Divide Error
  1:  #DB - Debug
  2:  NMI - Non-Maskable Interrupt
  3:  #BP - Breakpoint
  6:  #UD - Invalid Opcode
  13: #GP - General Protection Fault
  14: #PF - Page Fault

Vector 32-127: 外部设备中断 (IRQ)
  32: FIRST_EXTERNAL_VECTOR

Vector 128:    系统调用 (int 0x80)

Vector 129-255: 其他系统用途
  包括APIC、IPI、thermal等
```

Q: [Basic] 什么是IRQ？IRQ号和中断向量(Vector)的区别是什么？
A: **IRQ (Interrupt Request)**：设备的逻辑中断号，由中断控制器管理
**Vector**：CPU中断向量表(IDT)的索引

关系：`vector = IRQ + FIRST_EXTERNAL_VECTOR (32)`

例如：IRQ 0 (Timer) → Vector 32
```c
/* arch/x86/kernel/irq.c */
irq = __this_cpu_read(vector_irq[vector]);
```

Q: [Intermediate] x86的IDT(中断描述符表)结构是什么？
A: IDT包含256个gate_desc描述符：
```c
/* arch/x86/kernel/traps.c */
gate_desc idt_table[NR_VECTORS];

/* gate_desc结构 (64位) */
struct gate_struct64 {
    u16 offset_low;      // 处理程序地址低16位
    u16 segment;         // 代码段选择子
    unsigned ist : 3;    // Interrupt Stack Table
    unsigned zero0 : 5;
    unsigned type : 5;   // 门类型
    unsigned dpl : 2;    // 描述符特权级
    unsigned p : 1;      // 存在位
    u16 offset_middle;   // 地址中16位
    u32 offset_high;     // 地址高32位
    u32 zero1;
};
```

Q: [Basic] 什么是中断上下文(Interrupt Context)？有什么限制？
A: 中断上下文是指中断处理程序执行时的环境：
- **没有进程上下文**：不能睡眠、不能调度
- **不能访问用户空间**
- **不能持有信号量/互斥锁**
- **只能使用GFP_ATOMIC分配内存**
- **应尽快完成**

检测方法：
```c
if (in_interrupt()) {
    /* 在中断上下文中 */
}
if (in_irq()) {
    /* 在硬中断处理中 */
}
```

Q: [Intermediate] 如何区分in_interrupt()、in_irq()、in_softirq()？
A: 通过preempt_count的不同位来区分：
```c
/* preempt_count布局 */
+--------+--------+--------+--------+
|PREEMPT |SOFTIRQ | HARDIRQ|  NMI   |
+--------+--------+--------+--------+
  bits    bits     bits     bits

in_interrupt() = hardirq_count() || softirq_count()
               // 硬中断或软中断中

in_irq()       = hardirq_count()
               // 仅硬中断中

in_softirq()   = softirq_count()
               // 软中断中（包括bh_disable）

in_nmi()       = NMI标志位
               // NMI处理中
```

---

## 中断处理流程 (Interrupt Handling Flow)

Q: [Intermediate] x86架构中断处理的完整流程是什么？
A: 中断处理流程：
```
+------------------+
| Hardware IRQ     |
+--------+---------+
         |
         v
+------------------+
| CPU saves state  |
| (ss,rsp,rflags,  |
|  cs,rip,errcode) |
+--------+---------+
         |
         v
+------------------+
| IDT lookup       |
| vector -> handler|
+--------+---------+
         |
         v
+------------------+
| common_interrupt |
| (entry_64.S)     |
+--------+---------+
         |
         v
+------------------+
| do_IRQ()         |
| (arch/x86/kernel/|
|  irq.c)          |
+--------+---------+
         |
         v
+------------------+
| generic_handle_  |
| irq_desc()       |
+--------+---------+
         |
         v
+------------------+
| Driver handler   |
| (irqaction->     |
|  handler)        |
+--------+---------+
         |
         v
+------------------+
| irq_exit()       |
| -> do_softirq()  |
+------------------+
```

Q: [Intermediate] do_IRQ()函数的核心逻辑是什么？
A: do_IRQ()是x86中断处理的核心入口：
```c
/* arch/x86/kernel/irq.c */
unsigned int __irq_entry do_IRQ(struct pt_regs *regs)
{
    struct pt_regs *old_regs = set_irq_regs(regs);
    unsigned vector = ~regs->orig_ax;  // 获取中断向量
    unsigned irq;

    exit_idle();      // 退出idle状态
    irq_enter();      // 标记进入中断上下文

    /* 从vector获取IRQ号 */
    irq = __this_cpu_read(vector_irq[vector]);

    /* 调用具体的中断处理 */
    if (!handle_irq(irq, regs)) {
        ack_APIC_irq();  // 无处理程序则确认中断
    }

    irq_exit();       // 退出中断上下文，可能处理softirq
    set_irq_regs(old_regs);
    return 1;
}
```

Q: [Intermediate] irq_enter()和irq_exit()分别做什么？
A: 这两个函数管理中断上下文的进入和退出：
```c
/* kernel/softirq.c */
void irq_enter(void)
{
    rcu_irq_enter();      // RCU相关处理
    if (idle_cpu(cpu) && !in_interrupt()) {
        local_bh_disable();
        tick_check_idle(cpu);  // 更新时间记账
        _local_bh_enable();
    }
    __irq_enter();        // preempt_count += HARDIRQ_OFFSET
}

void irq_exit(void)
{
    account_system_vtime(current);
    trace_hardirq_exit();
    sub_preempt_count(IRQ_EXIT_OFFSET);
    
    /* 退出硬中断后检查是否需要处理软中断 */
    if (!in_interrupt() && local_softirq_pending())
        invoke_softirq();  // 处理pending的softirq
    
    rcu_irq_exit();
}
```

Q: [Advanced] handle_level_irq和handle_edge_irq的区别是什么？
A: 电平触发和边沿触发中断的处理策略不同：
```c
/* 电平触发中断 */
void handle_level_irq(unsigned int irq, struct irq_desc *desc)
{
    /* 先mask再处理，防止重复触发 */
    mask_ack_irq(desc);      // 1. 先屏蔽中断
    
    handle_irq_event(desc);  // 2. 执行handler
    
    cond_unmask_irq(desc);   // 3. 处理完再unmask
}

/* 边沿触发中断 */
void handle_edge_irq(unsigned int irq, struct irq_desc *desc)
{
    /* 只ack不mask，可能在处理期间再次触发 */
    desc->irq_data.chip->irq_ack(&desc->irq_data);  // 1. 确认
    
    do {
        handle_irq_event(desc);  // 2. 循环处理
    } while (desc->istate & IRQS_PENDING);
}
```
关键区别：电平触发必须在处理完成前保持mask，否则会持续触发。

---

## 关键数据结构 (Key Data Structures)

Q: [Intermediate] struct irq_desc的核心字段有哪些？
A: irq_desc是每个IRQ的描述符：
```c
/* include/linux/irqdesc.h */
struct irq_desc {
    struct irq_data     irq_data;      // IRQ相关数据
    irq_flow_handler_t  handle_irq;    // 高层处理函数
    struct irqaction   *action;        // 处理程序链表
    
    unsigned int        depth;         // 嵌套禁用计数
    unsigned int        irq_count;     // 中断统计
    unsigned long       last_unhandled;// 未处理中断时间戳
    unsigned int        irqs_unhandled;// 未处理统计
    
    raw_spinlock_t      lock;          // 保护此描述符
    struct cpumask     *percpu_enabled;// Per-CPU启用掩码
    
#ifdef CONFIG_SMP
    const struct cpumask *affinity_hint; // 亲和性提示
#endif
    wait_queue_head_t   wait_for_threads; // 等待线程完成
    const char         *name;          // /proc/interrupts名称
};
```

Q: [Intermediate] struct irq_data包含什么信息？
A: irq_data存储IRQ和芯片相关的数据：
```c
/* include/linux/irq.h */
struct irq_data {
    unsigned int        irq;       // Linux IRQ号
    unsigned long       hwirq;     // 硬件中断号
    unsigned int        node;      // NUMA节点
    unsigned int        state_use_accessors; // 状态标志
    
    struct irq_chip    *chip;      // 中断控制器操作
    struct irq_domain  *domain;    // 中断域
    void               *handler_data;  // 处理程序私有数据
    void               *chip_data;     // 芯片私有数据
    struct msi_desc    *msi_desc;      // MSI描述符
    
#ifdef CONFIG_SMP
    cpumask_var_t       affinity;  // CPU亲和性掩码
#endif
};
```

Q: [Intermediate] struct irqaction的作用是什么？
A: irqaction代表一个注册的中断处理程序：
```c
/* include/linux/interrupt.h */
struct irqaction {
    irq_handler_t       handler;    // 中断处理函数
    unsigned long       flags;      // IRQF_* 标志
    void               *dev_id;     // 设备标识(共享中断必须)
    struct irqaction   *next;       // 链接共享中断的下一个action
    int                 irq;        // IRQ号
    
    irq_handler_t       thread_fn;  // 线程化处理函数
    struct task_struct *thread;     // 中断线程
    unsigned long       thread_flags;
    unsigned long       thread_mask;
    
    const char         *name;       // /proc/interrupts显示名
};
```
共享同一IRQ的多个设备形成irqaction链表。

Q: [Intermediate] struct irq_chip抽象了什么？
A: irq_chip抽象中断控制器的硬件操作：
```c
/* include/linux/irq.h */
struct irq_chip {
    const char *name;              // 芯片名称
    
    /* 生命周期管理 */
    unsigned int (*irq_startup)(struct irq_data *data);
    void (*irq_shutdown)(struct irq_data *data);
    void (*irq_enable)(struct irq_data *data);
    void (*irq_disable)(struct irq_data *data);
    
    /* 中断应答和屏蔽 */
    void (*irq_ack)(struct irq_data *data);      // 确认中断
    void (*irq_mask)(struct irq_data *data);     // 屏蔽中断
    void (*irq_unmask)(struct irq_data *data);   // 取消屏蔽
    void (*irq_eoi)(struct irq_data *data);      // 结束中断
    
    /* SMP亲和性 */
    int (*irq_set_affinity)(struct irq_data *data, 
                            const struct cpumask *dest, 
                            bool force);
    
    /* 类型设置 */
    int (*irq_set_type)(struct irq_data *data, 
                        unsigned int flow_type);
    
    /* 电源管理 */
    int (*irq_set_wake)(struct irq_data *data, unsigned int on);
};
```

---

## 中断注册API (Interrupt Registration API)

Q: [Basic] request_irq()函数的参数含义是什么？
A: request_irq()注册中断处理程序：
```c
int request_irq(
    unsigned int irq,        // IRQ号
    irq_handler_t handler,   // 处理函数
    unsigned long flags,     // IRQF_* 标志
    const char *name,        // /proc/interrupts中的名称
    void *dev_id            // 传给handler的参数
);

/* 返回值：0成功，负数错误码 */

/* 示例 */
ret = request_irq(irq, my_handler,
                  IRQF_SHARED,      // 共享中断
                  "mydevice",       // 设备名
                  &my_device);      // 设备结构
if (ret) {
    dev_err(dev, "Failed to request IRQ\n");
}
```

Q: [Intermediate] 常用的IRQF_*标志有哪些？
A: 中断请求标志定义在interrupt.h中：
```c
/* 触发方式 */
IRQF_TRIGGER_RISING   0x01  // 上升沿触发
IRQF_TRIGGER_FALLING  0x02  // 下降沿触发
IRQF_TRIGGER_HIGH     0x04  // 高电平触发
IRQF_TRIGGER_LOW      0x08  // 低电平触发

/* 行为标志 */
IRQF_SHARED        0x80    // 允许多设备共享此IRQ
IRQF_ONESHOT       0x2000  // 线程处理完前不重新使能
IRQF_NO_SUSPEND    0x4000  // 休眠时不禁用此中断
IRQF_NOBALANCING   0x800   // 禁止IRQ负载均衡

/* 特殊用途 */
IRQF_TIMER         // 定时器中断专用
IRQF_PERCPU        // Per-CPU中断
IRQF_NO_THREAD     // 强制禁止线程化
```

Q: [Intermediate] request_threaded_irq()和request_irq()的区别是什么？
A: request_threaded_irq支持线程化中断处理：
```c
int request_threaded_irq(
    unsigned int irq,
    irq_handler_t handler,     // 硬中断处理(快速)
    irq_handler_t thread_fn,   // 线程处理(可睡眠)
    unsigned long flags,
    const char *name,
    void *dev_id
);

/* request_irq实际上是wrapper */
static inline int request_irq(...) {
    return request_threaded_irq(irq, handler, NULL, ...);
}
```

线程化处理流程：
```
IRQ触发 → handler(快速确认) → 返回IRQ_WAKE_THREAD
                                     ↓
                              唤醒中断线程
                                     ↓
                              thread_fn(完整处理)
```

Q: [Intermediate] devm_request_irq()有什么优势？
A: devm_request_irq是资源管理版本，自动释放：
```c
int devm_request_irq(
    struct device *dev,      // 设备结构
    unsigned int irq,
    irq_handler_t handler,
    unsigned long irqflags,
    const char *devname,
    void *dev_id
);

/* 优势：设备移除时自动调用free_irq() */

/* 使用示例 */
ret = devm_request_irq(&pdev->dev, irq, my_handler,
                       0, "mydevice", priv);
if (ret)
    return ret;
/* 无需在remove函数中调用free_irq() */
```

Q: [Basic] 中断处理函数的返回值含义是什么？
A: irqreturn_t定义了三种返回值：
```c
typedef enum irqreturn {
    IRQ_NONE        = 0,  // 不是本设备的中断(共享时使用)
    IRQ_HANDLED     = 1,  // 中断已处理
    IRQ_WAKE_THREAD = 2,  // 请求唤醒中断线程
} irqreturn_t;

/* 典型处理函数 */
static irqreturn_t my_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;
    
    /* 检查是否是本设备的中断 */
    if (!device_has_interrupt(dev))
        return IRQ_NONE;  // 共享中断，不是我的
    
    /* 快速处理或清除中断 */
    device_ack_interrupt(dev);
    
    /* 如果需要更多处理，唤醒线程 */
    return IRQ_WAKE_THREAD;  // 或 IRQ_HANDLED
}
```

---

## 软中断 (Softirq)

Q: [Intermediate] Linux内核定义了哪些softirq类型？
A: 内核预定义了10种softirq：
```c
/* include/linux/interrupt.h */
enum {
    HI_SOFTIRQ = 0,      // 高优先级tasklet
    TIMER_SOFTIRQ,       // 定时器
    NET_TX_SOFTIRQ,      // 网络发送
    NET_RX_SOFTIRQ,      // 网络接收
    BLOCK_SOFTIRQ,       // 块设备
    BLOCK_IOPOLL_SOFTIRQ,// 块设备轮询
    TASKLET_SOFTIRQ,     // 普通tasklet
    SCHED_SOFTIRQ,       // 调度器
    HRTIMER_SOFTIRQ,     // 高精度定时器
    RCU_SOFTIRQ,         // RCU处理
    
    NR_SOFTIRQS          // 总数 = 10
};
```
优先级：编号越小优先级越高

Q: [Intermediate] softirq的处理流程是什么？
A: softirq在__do_softirq()中处理：
```c
/* kernel/softirq.c */
asmlinkage void __do_softirq(void)
{
    struct softirq_action *h;
    __u32 pending;
    int max_restart = MAX_SOFTIRQ_RESTART;  // 10次

    pending = local_softirq_pending();
    __local_bh_disable();
    
restart:
    set_softirq_pending(0);  // 清除pending
    local_irq_enable();      // 允许硬中断

    h = softirq_vec;
    do {
        if (pending & 1) {
            h->action(h);    // 执行softirq处理
        }
        h++;
        pending >>= 1;
    } while (pending);

    local_irq_disable();
    pending = local_softirq_pending();
    
    /* 有新的pending且未达重试上限则重新处理 */
    if (pending && --max_restart)
        goto restart;
    
    /* 超过重试次数，唤醒ksoftirqd处理 */
    if (pending)
        wakeup_softirqd();
}
```

Q: [Intermediate] 如何注册和触发softirq？
A: softirq的注册和触发：
```c
/* 注册softirq处理函数 */
void open_softirq(int nr, void (*action)(struct softirq_action *))
{
    softirq_vec[nr].action = action;
}

/* 示例：网络子系统注册 */
open_softirq(NET_RX_SOFTIRQ, net_rx_action);
open_softirq(NET_TX_SOFTIRQ, net_tx_action);

/* 触发softirq */
void raise_softirq(unsigned int nr)
{
    unsigned long flags;
    local_irq_save(flags);
    raise_softirq_irqoff(nr);  // 设置pending位
    local_irq_restore(flags);
}

/* 在中断上下文中触发（已禁中断） */
void raise_softirq_irqoff(unsigned int nr)
{
    __raise_softirq_irqoff(nr);  // or_softirq_pending(1UL << nr)
    if (!in_interrupt())
        wakeup_softirqd();
}
```

Q: [Intermediate] ksoftirqd内核线程的作用是什么？
A: ksoftirqd处理softirq负载过重的情况：
```c
/* kernel/softirq.c */
static int run_ksoftirqd(void *__bind_cpu)
{
    set_current_state(TASK_INTERRUPTIBLE);
    
    while (!kthread_should_stop()) {
        preempt_disable();
        if (!local_softirq_pending()) {
            preempt_enable_no_resched();
            schedule();  // 无pending则睡眠
            preempt_disable();
        }

        __set_current_state(TASK_RUNNING);
        
        while (local_softirq_pending()) {
            local_irq_disable();
            if (local_softirq_pending())
                __do_softirq();  // 处理softirq
            local_irq_enable();
            cond_resched();  // 允许调度
        }
        preempt_enable();
        set_current_state(TASK_INTERRUPTIBLE);
    }
    return 0;
}
```
每个CPU一个ksoftirqd/N线程，以普通优先级运行。

---

## Tasklet

Q: [Basic] tasklet是什么？与softirq有什么区别？
A: Tasklet是基于softirq的延迟处理机制：
```
+------------------+-------------------+
|     Softirq      |     Tasklet       |
+------------------+-------------------+
| 静态定义(编译时) | 动态创建          |
| 可并行运行       | 同一tasklet串行   |
| 高性能要求       | 一般驱动使用      |
| 固定10种         | 数量不限          |
+------------------+-------------------+
```

Tasklet保证：
1. 同一tasklet不会在多CPU上并行执行
2. 不同tasklet可以并行执行
3. 运行在softirq上下文（不能睡眠）

Q: [Intermediate] struct tasklet_struct的结构是什么？
A: tasklet结构定义：
```c
/* include/linux/interrupt.h */
struct tasklet_struct {
    struct tasklet_struct *next;  // 链表指针
    unsigned long state;          // 状态标志
    atomic_t count;               // 禁用计数
    void (*func)(unsigned long);  // 处理函数
    unsigned long data;           // 传给func的参数
};

/* 状态标志 */
enum {
    TASKLET_STATE_SCHED,  // 已调度，等待执行
    TASKLET_STATE_RUN     // 正在执行(SMP用)
};
```

Q: [Basic] 如何定义和使用tasklet？
A: tasklet的定义和使用方法：
```c
/* 静态定义 */
DECLARE_TASKLET(my_tasklet, my_tasklet_func, data);
DECLARE_TASKLET_DISABLED(my_tasklet, my_tasklet_func, data);

/* 动态初始化 */
struct tasklet_struct my_tasklet;
tasklet_init(&my_tasklet, my_tasklet_func, data);

/* 调度执行 */
tasklet_schedule(&my_tasklet);    // 普通优先级
tasklet_hi_schedule(&my_tasklet); // 高优先级

/* 禁用/启用 */
tasklet_disable(&my_tasklet);  // 阻止执行
tasklet_enable(&my_tasklet);   // 允许执行

/* 销毁 */
tasklet_kill(&my_tasklet);     // 等待完成并移除

/* 处理函数 */
void my_tasklet_func(unsigned long data)
{
    /* 运行在softirq上下文，不能睡眠 */
    struct my_device *dev = (void *)data;
    process_device_data(dev);
}
```

Q: [Intermediate] tasklet的调度和执行流程是什么？
A: tasklet调度到执行的过程：
```c
/* 调度tasklet */
void tasklet_schedule(struct tasklet_struct *t)
{
    if (!test_and_set_bit(TASKLET_STATE_SCHED, &t->state))
        __tasklet_schedule(t);  // 只调度一次
}

void __tasklet_schedule(struct tasklet_struct *t)
{
    local_irq_save(flags);
    /* 添加到per-CPU链表 */
    t->next = NULL;
    *__this_cpu_read(tasklet_vec.tail) = t;
    __this_cpu_write(tasklet_vec.tail, &(t->next));
    
    raise_softirq_irqoff(TASKLET_SOFTIRQ);  // 触发softirq
    local_irq_restore(flags);
}

/* 执行tasklet (softirq处理函数) */
static void tasklet_action(struct softirq_action *a)
{
    local_irq_disable();
    list = __this_cpu_read(tasklet_vec.head);  // 获取链表
    __this_cpu_write(tasklet_vec.head, NULL);
    local_irq_enable();

    while (list) {
        struct tasklet_struct *t = list;
        list = list->next;

        if (tasklet_trylock(t)) {      // 尝试获取执行权
            if (!atomic_read(&t->count)) {  // 未禁用
                clear_bit(TASKLET_STATE_SCHED, &t->state);
                t->func(t->data);      // 执行处理函数
                tasklet_unlock(t);
                continue;
            }
            tasklet_unlock(t);
        }
        /* 未能执行，重新调度 */
        __tasklet_schedule(t);
    }
}
```

---

## 工作队列 (Workqueue)

Q: [Basic] workqueue与tasklet/softirq的主要区别是什么？
A: workqueue运行在进程上下文，可以睡眠：
```
+---------+------------+------------+
|         | Softirq/   | Workqueue  |
|         | Tasklet    |            |
+---------+------------+------------+
| 上下文  | 中断上下文 | 进程上下文 |
| 睡眠    | 不能       | 可以       |
| 延迟    | 最小       | 较大       |
| 锁      | spinlock   | mutex可用  |
| 适用    | 快速处理   | 耗时操作   |
+---------+------------+------------+
```

Q: [Basic] 如何定义和使用work_struct？
A: work_struct的使用方法：
```c
#include <linux/workqueue.h>

/* 静态定义 */
DECLARE_WORK(my_work, my_work_func);

/* 动态初始化 */
struct work_struct my_work;
INIT_WORK(&my_work, my_work_func);

/* 调度到默认工作队列 */
schedule_work(&my_work);

/* 调度延迟工作 */
struct delayed_work my_delayed_work;
INIT_DELAYED_WORK(&my_delayed_work, my_work_func);
schedule_delayed_work(&my_delayed_work, msecs_to_jiffies(100));

/* 取消工作 */
cancel_work_sync(&my_work);
cancel_delayed_work_sync(&my_delayed_work);

/* 工作函数 */
void my_work_func(struct work_struct *work)
{
    struct my_device *dev = container_of(work, 
                                         struct my_device, 
                                         work);
    /* 可以睡眠、获取mutex等 */
    mutex_lock(&dev->lock);
    process_data(dev);
    mutex_unlock(&dev->lock);
}
```

Q: [Intermediate] 如何创建自定义workqueue？
A: 创建专用workqueue的方法：
```c
/* 创建工作队列 */
struct workqueue_struct *my_wq;

/* 单线程、非绑定 */
my_wq = create_singlethread_workqueue("my_wq");

/* 每CPU一个线程 */
my_wq = alloc_workqueue("my_wq", WQ_HIGHPRI, 0);

/* 工作队列标志 */
WQ_HIGHPRI      // 高优先级worker
WQ_UNBOUND      // 不绑定CPU
WQ_FREEZABLE    // 参与系统休眠
WQ_MEM_RECLAIM  // 内存回收可用

/* 调度到自定义队列 */
queue_work(my_wq, &my_work);
queue_delayed_work(my_wq, &my_delayed_work, delay);

/* 销毁工作队列 */
destroy_workqueue(my_wq);
```

Q: [Advanced] CMWQ(Concurrency Managed Workqueue)的工作原理是什么？
A: CMWQ是现代workqueue的实现方式：
```
+-------------------+
| workqueue         |
| (逻辑工作队列)    |
+---------+---------+
          |
          v
+-------------------+
| pool_workqueue    |
| (Per-CPU或Unbound)|
+---------+---------+
          |
          v
+-------------------+
| worker_pool       |
| (线程池)          |
+---------+---------+
          |
          v
+-------------------+
| worker            |
| (实际工作线程)    |
+-------------------+

特性：
1. 动态worker管理：按需创建/销毁工作线程
2. 并发控制：限制同时运行的work数量
3. CPU亲和性：bound workqueue绑定CPU
4. 优先级支持：高优先级work优先执行
```

---

## 中断控制 (Interrupt Control)

Q: [Basic] local_irq_disable/enable和disable_irq/enable_irq的区别是什么？
A: 两组函数控制范围不同：
```c
/* 控制本CPU所有中断 */
local_irq_disable();   // 禁用本CPU中断
local_irq_enable();    // 启用本CPU中断

/* 保存/恢复中断状态 */
unsigned long flags;
local_irq_save(flags);     // 保存状态并禁用
local_irq_restore(flags);  // 恢复之前状态

/* 控制特定IRQ */
disable_irq(irq);          // 禁用特定IRQ(等待处理完成)
disable_irq_nosync(irq);   // 禁用特定IRQ(不等待)
enable_irq(irq);           // 启用特定IRQ

/* 区别 */
local_irq_*:  仅影响当前CPU，其他CPU不受影响
disable_irq:  全局禁用该IRQ，所有CPU都不会收到
```

Q: [Intermediate] local_bh_disable/enable是做什么的？
A: local_bh_*控制软中断和tasklet的执行：
```c
/* kernel/softirq.c */
void local_bh_disable(void)
{
    __local_bh_disable(SOFTIRQ_DISABLE_OFFSET);
}

void local_bh_enable(void)
{
    /* 启用BH时检查是否有pending的softirq */
    if (unlikely(!in_interrupt() && local_softirq_pending()))
        do_softirq();
}

/* 使用场景 */
local_bh_disable();
/* 临界区：softirq/tasklet不会执行 */
/* 但硬中断仍可发生 */
local_bh_enable();

/* 与spin_lock_bh结合 */
spin_lock_bh(&lock);  // = spin_lock + local_bh_disable
/* 临界区 */
spin_unlock_bh(&lock);
```

Q: [Intermediate] synchronize_irq()的作用是什么？
A: synchronize_irq()等待中断处理完成：
```c
/* kernel/irq/manage.c */
void synchronize_irq(unsigned int irq)
{
    struct irq_desc *desc = irq_to_desc(irq);
    
    /* 等待硬中断处理完成 */
    while (irqd_irq_inprogress(&desc->irq_data))
        cpu_relax();
    
    /* 等待线程化中断完成 */
    wait_event(desc->wait_for_threads,
               !atomic_read(&desc->threads_active));
}

/* 使用场景：驱动卸载时确保安全 */
void my_driver_remove(struct device *dev)
{
    disable_irq(irq);      // 禁止新中断
    synchronize_irq(irq);  // 等待正在处理的完成
    free_irq(irq, dev);    // 安全释放
}
```

---

## 中断亲和性 (IRQ Affinity)

Q: [Basic] 什么是中断亲和性？如何设置？
A: 中断亲和性指定哪些CPU可以处理特定IRQ：
```c
/* 内核API设置 */
int irq_set_affinity(unsigned int irq, 
                     const struct cpumask *mask);

/* 用户空间设置 */
$ echo 3 > /proc/irq/24/smp_affinity  // CPU 0,1
$ echo 2 > /proc/irq/24/smp_affinity  // 仅CPU 1

/* 查看当前亲和性 */
$ cat /proc/irq/24/smp_affinity
00000003  // bitmap表示CPU 0和1

/* 亲和性提示（非强制） */
int irq_set_affinity_hint(unsigned int irq, 
                          const struct cpumask *m);
```

Q: [Intermediate] irqbalance守护进程如何工作？
A: irqbalance自动平衡中断负载：
```
+-------------------+
| irqbalance daemon |
+---------+---------+
          |
          v
+-------------------+
| 1. 收集统计信息   |
|    /proc/interrupts|
|    /proc/stat     |
+---------+---------+
          |
          v
+-------------------+
| 2. 计算负载分布   |
|    考虑NUMA拓扑   |
|    考虑电源状态   |
+---------+---------+
          |
          v
+-------------------+
| 3. 设置亲和性     |
|    /proc/irq/N/   |
|    smp_affinity   |
+-------------------+

原则：
- 分散中断到多CPU
- 考虑NUMA局部性
- 避免某CPU过载
```

---

## 异常处理 (Exception Handling)

Q: [Intermediate] x86架构trap_init()注册了哪些异常处理程序？
A: trap_init()初始化所有CPU异常：
```c
/* arch/x86/kernel/traps.c */
void __init trap_init(void)
{
    set_intr_gate(0, &divide_error);      // #DE 除法错误
    set_intr_gate_ist(1, &debug, DEBUG_STACK);  // #DB 调试
    set_intr_gate_ist(2, &nmi, NMI_STACK);      // NMI
    set_system_intr_gate(3, &int3);       // #BP 断点
    set_system_intr_gate(4, &overflow);   // #OF 溢出
    set_intr_gate(5, &bounds);            // #BR 边界检查
    set_intr_gate(6, &invalid_op);        // #UD 无效操作码
    set_intr_gate(7, &device_not_available); // #NM 设备不可用
    set_intr_gate_ist(8, &double_fault, DOUBLEFAULT_STACK);
    set_intr_gate(10, &invalid_TSS);      // #TS 无效TSS
    set_intr_gate(11, &segment_not_present); // #NP 段不存在
    set_intr_gate_ist(12, &stack_segment, STACKFAULT_STACK);
    set_intr_gate(13, &general_protection); // #GP 一般保护
    set_intr_gate(14, &page_fault);       // #PF 页错误
    set_intr_gate(16, &coprocessor_error); // #MF 协处理器
    set_intr_gate(17, &alignment_check);  // #AC 对齐检查
    set_intr_gate_ist(18, &machine_check, MCE_STACK);
    set_intr_gate(19, &simd_coprocessor_error); // #XF SIMD
}
```

Q: [Advanced] do_page_fault()的处理流程是什么？
A: 页错误处理是最复杂的异常处理之一：
```c
/* arch/x86/mm/fault.c */
dotraplinkage void do_page_fault(struct pt_regs *regs, 
                                  unsigned long error_code)
{
    struct task_struct *tsk = current;
    struct mm_struct *mm = tsk->mm;
    unsigned long address = read_cr2();  // 获取错误地址

    /* 1. 内核空间错误 */
    if (fault_in_kernel_space(address)) {
        /* vmalloc区域错误 - 同步页表 */
        if (vmalloc_fault(address) >= 0)
            return;
        /* 真正的内核错误 - oops */
        bad_area_nosemaphore(regs, error_code, address);
        return;
    }

    /* 2. 用户空间错误 */
    if (user_mode_vm(regs))
        local_irq_enable();

    /* 3. 不能处理的情况 */
    if (in_atomic() || !mm) {
        bad_area_nosemaphore(regs, error_code, address);
        return;
    }

    /* 4. 查找VMA */
    down_read(&mm->mmap_sem);
    vma = find_vma(mm, address);
    
    if (!vma || address < vma->vm_start) {
        /* 可能是栈扩展 */
        if (expand_stack(vma, address))
            goto bad_area;
    }

    /* 5. 权限检查和页分配 */
    fault = handle_mm_fault(mm, vma, address, flags);
    
    up_read(&mm->mmap_sem);
}
```

Q: [Intermediate] 页错误error_code的含义是什么？
A: x86页错误的error_code位定义：
```c
/* arch/x86/mm/fault.c */
enum x86_pf_error_code {
    PF_PROT   = 1 << 0,  // 0=页不存在 1=保护违规
    PF_WRITE  = 1 << 1,  // 0=读访问 1=写访问
    PF_USER   = 1 << 2,  // 0=内核态 1=用户态
    PF_RSVD   = 1 << 3,  // 页表保留位被设置
    PF_INSTR  = 1 << 4,  // 0=数据访问 1=取指令
};

/* 解析示例 */
error_code = 0x7:
  PF_PROT  = 1  // 保护违规（页存在）
  PF_WRITE = 1  // 写操作
  PF_USER  = 1  // 用户态
  => 用户态尝试写只读页
```

---

## 线程化中断 (Threaded Interrupts)

Q: [Intermediate] 什么是线程化中断？有什么优势？
A: 线程化中断将中断处理放到内核线程中执行：
```
传统中断：
+---------+    +---------+
| hardirq | -> | softirq | -> 完成
+---------+    +---------+
  不可抢占      不可抢占

线程化中断：
+---------+    +------------+
| hardirq | -> | irq thread | -> 完成
+---------+    +------------+
  最小化         可抢占、可睡眠

优势：
1. 减少中断延迟（快速返回）
2. 可被实时任务抢占
3. 支持优先级调度
4. 可以使用睡眠锁
```

Q: [Intermediate] IRQF_ONESHOT标志的作用是什么？
A: IRQF_ONESHOT用于线程化中断的安全处理：
```c
/* 含义：线程处理完成前不重新使能中断 */

request_threaded_irq(irq, 
    my_hardirq_handler,  // 快速handler
    my_thread_fn,        // 线程处理
    IRQF_ONESHOT,        // 关键标志
    "mydevice", 
    dev);

/* 处理流程 */
1. 中断触发 → 调用hardirq handler
2. hardirq返回IRQ_WAKE_THREAD
3. 中断保持mask状态（ONESHOT）
4. 唤醒中断线程
5. 线程执行thread_fn
6. 线程完成后unmask中断

/* 必要性：防止电平触发中断持续触发 */
没有ONESHOT时：
  handler返回 → unmask → 电平仍有效 → 再次触发
有ONESHOT时：
  handler返回 → 保持mask → 线程处理 → unmask
```

Q: [Advanced] 如何强制所有中断线程化？
A: 可以通过内核启动参数强制线程化：
```bash
# 启动参数
threadirqs

# 效果
- 所有中断（除了标记IRQF_NO_THREAD的）都会线程化
- 用于实时系统或调试
- /proc/interrupts会显示中断线程

# 代码路径
/* kernel/irq/manage.c */
if (force_irqthreads && !(new->flags & IRQF_NO_THREAD)) {
    /* 强制线程化 */
}

# 不能线程化的中断
IRQF_NO_THREAD   // 明确禁止
IRQF_TIMER       // 定时器中断
IRQF_PERCPU      // Per-CPU中断
```

---

## 中断控制器 (Interrupt Controllers)

Q: [Basic] PIC和APIC有什么区别？
A: PIC和APIC是不同时代的中断控制器：
```
+------------------+---------------------+
|       PIC        |        APIC         |
| (8259A)          | (Advanced PIC)      |
+------------------+---------------------+
| 15个IRQ          | 224+个IRQ           |
| 单CPU            | 多CPU支持           |
| 边沿触发为主     | 电平/边沿都支持     |
| 固定优先级       | 可编程优先级        |
| 无路由能力       | 可路由到任意CPU     |
| 古老硬件         | 现代x86标准         |
+------------------+---------------------+

APIC组成：
+----------+     +----------+
| Local    |     | Local    |
| APIC     |     | APIC     |
| (CPU 0)  |     | (CPU 1)  |
+----+-----+     +----+-----+
     |                |
     +-------+--------+
             |
       +-----+-----+
       |  I/O APIC |
       | (外设中断)|
       +-----------+
```

Q: [Intermediate] I/O APIC的重定向表是如何工作的？
A: I/O APIC通过重定向表路由中断：
```
/* 重定向表条目结构 */
struct IO_APIC_route_entry {
    __u32 vector      : 8;   // 中断向量(IDT索引)
    __u32 delivery    : 3;   // 投递模式
    __u32 dest_mode   : 1;   // 目标模式
    __u32 delivery_status : 1;
    __u32 polarity    : 1;   // 极性
    __u32 irr         : 1;
    __u32 trigger     : 1;   // 触发方式
    __u32 mask        : 1;   // 屏蔽位
    __u32 __reserved  : 15;
    __u32 dest        : 8;   // 目标CPU
};

投递模式：
- Fixed:    固定投递到指定CPU
- Lowest:   投递到优先级最低的CPU
- NMI:      作为NMI投递
- INIT:     INIT信号
- ExtINT:   外部中断

/* 设置示例 */
entry.vector = 0x31;       // 向量49
entry.delivery = 0;        // Fixed模式
entry.dest_mode = 0;       // Physical目标
entry.trigger = 1;         // 电平触发
entry.dest = 0;            // CPU 0
```

---

## MSI/MSI-X中断 (Message Signaled Interrupts)

Q: [Intermediate] MSI中断与传统中断有什么区别？
A: MSI使用内存写代替中断引脚：
```
传统中断:
+--------+     IRQ线      +--------+
| Device | ------------> | I/O    |
+--------+               | APIC   |
                         +--------+

MSI中断:
+--------+   Memory Write  +--------+
| Device | --------------> | CPU    |
+--------+   (特定地址)    | LAPIC  |

优势：
1. 无需中断引脚，减少硬件复杂度
2. 避免中断共享
3. 支持多队列（MSI-X）
4. 更低延迟
```

Q: [Intermediate] 如何在驱动中启用MSI/MSI-X？
A: MSI的启用方法：
```c
/* 启用MSI */
int pci_enable_msi(struct pci_dev *dev);

/* 启用MSI-X */
int pci_enable_msix_range(struct pci_dev *dev,
                          struct msix_entry *entries,
                          int minvec, int maxvec);

/* 完整示例 */
int my_probe(struct pci_dev *pdev)
{
    int nvec, i;
    struct msix_entry *entries;

    /* 尝试MSI-X */
    nvec = pci_msix_vec_count(pdev);
    if (nvec > 0) {
        entries = kcalloc(nvec, sizeof(*entries), GFP_KERNEL);
        for (i = 0; i < nvec; i++)
            entries[i].entry = i;
        
        nvec = pci_enable_msix_range(pdev, entries, 1, nvec);
        if (nvec > 0) {
            /* MSI-X启用成功 */
            for (i = 0; i < nvec; i++)
                request_irq(entries[i].vector, ...);
            return 0;
        }
    }
    
    /* 回退到MSI */
    if (pci_enable_msi(pdev) == 0) {
        request_irq(pdev->irq, ...);
        return 0;
    }
    
    /* 回退到传统中断 */
    request_irq(pdev->irq, ...);
    return 0;
}
```

---

## NMI (Non-Maskable Interrupt)

Q: [Intermediate] NMI中断有什么特殊性？
A: NMI是不可屏蔽中断，具有特殊性：
```
特性：
1. 不能被软件禁用（cli无效）
2. 最高优先级
3. 用于严重错误和性能监控
4. 有专用中断栈

用途：
- 硬件错误（内存ECC、总线错误）
- 看门狗超时
- 性能分析（perf）
- 系统调试（SysRq）
- 内核死锁检测

/* NMI处理必须非常小心 */
- 不能调用可能睡眠的函数
- 不能获取任何锁（可能死锁）
- 只能使用per-CPU数据
- 用nmi_enter()/nmi_exit()标记
```

Q: [Advanced] 如何编写NMI安全的代码？
A: NMI处理有严格限制：
```c
/* NMI处理程序模板 */
dotraplinkage notrace void do_nmi(struct pt_regs *regs, 
                                   long error_code)
{
    nmi_enter();  // 标记进入NMI
    
    /* 只能使用NMI安全的操作 */
    
    /* 1. 读取per-CPU数据 */
    struct my_data *data = this_cpu_ptr(&my_percpu_data);
    
    /* 2. 使用无锁数据结构 */
    /* 3. 使用trylock而非lock */
    if (raw_spin_trylock(&some_lock)) {
        /* 获取成功 */
        raw_spin_unlock(&some_lock);
    }
    
    /* 4. 避免printk（除非必要） */
    
    /* 5. 不能分配内存 */
    
    nmi_exit();  // 标记退出NMI
}

/* NMI安全的通知链 */
atomic_notifier_call_chain(&nmi_chain, ...);
/* 不能使用blocking_notifier */
```

---

## 中断调试 (Interrupt Debugging)

Q: [Basic] 如何查看系统中断统计信息？
A: 通过/proc和工具查看中断信息：
```bash
# 查看中断统计
$ cat /proc/interrupts
           CPU0       CPU1
  0:        100          0   IO-APIC   2-edge      timer
  1:       1234        567   IO-APIC   1-edge      i8042
  8:          0          0   IO-APIC   8-edge      rtc0
 24:      50000      60000   PCI-MSI   524288-edge eth0

# 查看软中断统计
$ cat /proc/softirqs
                    CPU0       CPU1
          HI:          0          0
       TIMER:     123456     234567
      NET_TX:       1234       2345
      NET_RX:     456789     567890

# 实时监控
$ watch -n1 cat /proc/interrupts

# 查看IRQ亲和性
$ cat /proc/irq/24/smp_affinity
3

# 查看IRQ处理程序
$ cat /proc/irq/24/name
eth0
```

Q: [Intermediate] 如何追踪中断处理延迟？
A: 使用ftrace追踪中断：
```bash
# 启用中断追踪
$ echo irq > /sys/kernel/debug/tracing/set_event

# 或更精细控制
$ echo 'irq:irq_handler_entry' >> /sys/kernel/debug/tracing/set_event
$ echo 'irq:irq_handler_exit' >> /sys/kernel/debug/tracing/set_event
$ echo 'irq:softirq_entry' >> /sys/kernel/debug/tracing/set_event
$ echo 'irq:softirq_exit' >> /sys/kernel/debug/tracing/set_event

# 查看追踪结果
$ cat /sys/kernel/debug/tracing/trace
# tracer: nop
#
#           TASK-PID    CPU#    TIMESTAMP  FUNCTION
#              | |       |          |         |
          <idle>-0     [000]  1234.567890: irq_handler_entry: irq=24 name=eth0
          <idle>-0     [000]  1234.567895: irq_handler_exit: irq=24 ret=handled
          <idle>-0     [000]  1234.567900: softirq_entry: vec=3 [NET_RX]
          <idle>-0     [000]  1234.567950: softirq_exit: vec=3 [NET_RX]

# 使用perf分析
$ perf record -e 'irq:*' -a sleep 10
$ perf report
```

Q: [Intermediate] 如何处理中断风暴(Interrupt Storm)？
A: 中断风暴的检测和处理：
```c
/* 内核的中断风暴检测 */
/* kernel/irq/spurious.c */

#define IRQ_POLL_TIMEOUT  (HZ/10)  // 100ms
#define IRQ_UNHANDLED_THRESHOLD  99900  // 99.9%

/* 检测逻辑 */
if (desc->irq_count > 100000) {
    if (desc->irqs_unhandled * 1000 > desc->irq_count * 999) {
        /* 超过99.9%未处理 */
        __report_bad_irq(irq, desc, action_ret);
        /* 禁用该IRQ */
        desc->depth++;
        desc->irq_data.chip->irq_disable(&desc->irq_data);
    }
}

/* 用户空间监控脚本 */
#!/bin/bash
prev=0
while true; do
    curr=$(grep "eth0" /proc/interrupts | awk '{print $2}')
    rate=$((curr - prev))
    if [ $rate -gt 100000 ]; then
        echo "Warning: IRQ rate $rate/s"
    fi
    prev=$curr
    sleep 1
done
```

---

## 最佳实践 (Best Practices)

Q: [Intermediate] 中断处理程序的编写原则是什么？
A: 中断处理的最佳实践：
```c
/* 1. 快速返回原则 */
static irqreturn_t my_handler(int irq, void *dev_id)
{
    /* 只做必要的事 */
    status = read_hw_status(dev);
    if (!(status & MY_IRQ_PENDING))
        return IRQ_NONE;  // 不是我的中断
    
    /* 确认中断 */
    ack_hw_interrupt(dev);
    
    /* 将耗时工作推迟 */
    tasklet_schedule(&dev->tasklet);
    // 或 schedule_work(&dev->work);
    // 或 return IRQ_WAKE_THREAD;
    
    return IRQ_HANDLED;
}

/* 2. 正确使用锁 */
spin_lock(&dev->lock);  // 不是spin_lock_irq!
/* 因为已在中断上下文 */
spin_unlock(&dev->lock);

/* 3. 共享中断必须检查 */
if (!device_has_pending_irq(dev))
    return IRQ_NONE;

/* 4. 避免printk */
if (net_ratelimit())  // 限速
    pr_warn("...");
```

Q: [Advanced] 如何设计高性能中断处理？
A: 高性能中断处理的设计模式：
```
1. NAPI模式（网络）
   +--------+     +--------+     +--------+
   | IRQ    | --> | 禁用   | --> | Poll   |
   | 触发   |     | IRQ    |     | 模式   |
   +--------+     +--------+     +--------+
                                      |
                       完成 <---------+
                         |
                  +------+------+
                  | 重新使能IRQ |
                  +-------------+

2. 中断合并（Coalescing）
   - 硬件延迟中断
   - 累积多个事件
   - 减少中断频率

3. Per-CPU队列
   +------+  +------+  +------+
   | CPU0 |  | CPU1 |  | CPU2 |
   | Ring |  | Ring |  | Ring |
   +------+  +------+  +------+
      ↑         ↑         ↑
   MSI-X 0   MSI-X 1   MSI-X 2
   - 每个CPU独立队列
   - 避免锁竞争
   - 利用CPU缓存

4. 忙轮询（Busy Polling）
   - 绕过中断
   - 应用层直接轮询
   - 适合超低延迟场景
```

Q: [Intermediate] 驱动中断处理的错误处理模式？
A: 中断资源管理的标准模式：
```c
static int my_probe(struct platform_device *pdev)
{
    struct my_device *dev;
    int irq, ret;

    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    /* 获取IRQ号 */
    irq = platform_get_irq(pdev, 0);
    if (irq < 0)
        return irq;

    /* 使用devm_request_irq自动管理 */
    ret = devm_request_irq(&pdev->dev, irq,
                           my_irq_handler,
                           IRQF_SHARED,
                           dev_name(&pdev->dev),
                           dev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to request IRQ %d\n", irq);
        return ret;
    }

    /* devm_分配的资源会自动释放 */
    platform_set_drvdata(pdev, dev);
    return 0;
}

/* remove函数几乎为空 */
static int my_remove(struct platform_device *pdev)
{
    /* devm_*自动清理 */
    return 0;
}
```

---

## 常见错误 (Common Mistakes)

Q: [Intermediate] 中断处理中常见的编程错误有哪些？
A: 常见错误及解决方法：
```c
/* 错误1: 在中断上下文中睡眠 */
static irqreturn_t bad_handler(int irq, void *dev_id)
{
    msleep(10);  // 错误！会导致调度
    mutex_lock(&lock);  // 错误！可能睡眠
    return IRQ_HANDLED;
}
/* 解决：使用线程化中断或推迟到workqueue */

/* 错误2: 忘记释放中断 */
static void my_remove(...)
{
    /* 忘记free_irq() */
}
/* 解决：使用devm_request_irq() */

/* 错误3: 共享中断不检查 */
static irqreturn_t shared_handler(int irq, void *dev_id)
{
    /* 直接处理，未检查是否是自己的中断 */
    return IRQ_HANDLED;  // 错误！
}
/* 解决：总是检查设备状态 */

/* 错误4: 中断和进程上下文竞争 */
void process_context(void)
{
    spin_lock(&lock);  // 可能被中断打断
    /* 如果中断也获取这个锁会死锁 */
}
/* 解决：使用spin_lock_irqsave() */

/* 错误5: 在中断中做过多工作 */
static irqreturn_t slow_handler(int irq, void *dev_id)
{
    for (i = 0; i < 1000; i++)
        process_packet(i);  // 太慢！
    return IRQ_HANDLED;
}
/* 解决：推迟到softirq/tasklet/workqueue */
```

Q: [Advanced] 如何避免中断处理中的死锁？
A: 避免死锁的策略：
```c
/* 场景1：中断和进程上下文共享锁 */
/* 错误代码 */
spin_lock(&lock);           // 进程上下文
/* 此时中断到来，handler也要获取lock → 死锁 */

/* 正确做法 */
spin_lock_irqsave(&lock, flags);  // 禁止本地中断
/* ... */
spin_unlock_irqrestore(&lock, flags);

/* 场景2：嵌套中断 */
/* 如果handler A可能触发handler B */
/* 且两者共享锁，需要小心 */

/* 场景3：softirq和进程上下文 */
spin_lock_bh(&lock);  // 禁止softirq
/* ... */
spin_unlock_bh(&lock);

/* 锁层次规则 */
/*
 * 获取顺序：hardirq < softirq < process
 * spin_lock_irq:    禁hardirq
 * spin_lock_bh:     禁softirq
 * spin_lock:        不禁止任何
 */
```

