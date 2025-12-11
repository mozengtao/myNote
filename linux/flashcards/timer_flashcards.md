# Time and Timer Structures and API Flashcards for Linux Kernel Implementation

## 基础概念 (Basic Concepts)

Q: [Basic] Linux内核中时间管理的整体架构是什么？
A: Linux时间管理分为多个层次：
```
+----------------------------------------------------------+
|                    应用层 (User Space)                    |
|  gettimeofday() / clock_gettime() / nanosleep() / alarm()|
+----------------------------------------------------------+
                            |
                            v
+----------------------------------------------------------+
|                   系统调用层                              |
|  sys_gettimeofday() / sys_clock_gettime() / sys_nanosleep|
+----------------------------------------------------------+
                            |
                            v
+----------------------------------------------------------+
|                   时间子系统核心                          |
|  +------------------+  +------------------+               |
|  | Timekeeping      |  | Timer Management |               |
|  | (时间记录)        |  | (定时器管理)      |               |
|  +------------------+  +------------------+               |
|         |                     |                          |
|         v                     v                          |
|  +------------------+  +------------------+               |
|  | clocksource      |  | timer_list      |               |
|  | (时钟源)          |  | hrtimer         |               |
|  +------------------+  +------------------+               |
+----------------------------------------------------------+
                            |
                            v
+----------------------------------------------------------+
|                   硬件抽象层                              |
|  +------------------+  +------------------+               |
|  | clock_event_     |  | RTC             |               |
|  | device           |  | (实时时钟)       |               |
|  +------------------+  +------------------+               |
+----------------------------------------------------------+
                            |
                            v
+----------------------------------------------------------+
|                   硬件层                                  |
|  TSC / HPET / PIT / APIC Timer / RTC                     |
+----------------------------------------------------------+
```

Q: [Basic] Linux中有哪些时间表示方式？
A: Linux提供多种时间表示：
```c
/* 1. jiffies - 系统启动后的tick计数 */
extern unsigned long volatile jiffies;  // 32位
extern u64 jiffies_64;                  // 64位

/* 2. struct timespec - 秒+纳秒 */
struct timespec {
    __kernel_time_t tv_sec;   // 秒
    long tv_nsec;             // 纳秒 (0-999999999)
};

/* 3. struct timeval - 秒+微秒 */
struct timeval {
    __kernel_time_t tv_sec;       // 秒
    __kernel_suseconds_t tv_usec; // 微秒 (0-999999)
};

/* 4. ktime_t - 高精度时间（纳秒精度）*/
union ktime {
    s64 tv64;                 // 64位纳秒值
#if BITS_PER_LONG != 64
    struct {
        s32 nsec, sec;        // 分开存储（32位系统）
    } tv;
#endif
};
typedef union ktime ktime_t;

/* 5. time64_t - 64位Unix时间戳 */
typedef s64 time64_t;  // 解决2038年问题

/* 转换关系 */
// 1秒 = 1,000毫秒(ms)
// 1秒 = 1,000,000微秒(us)  
// 1秒 = 1,000,000,000纳秒(ns)
// 1 jiffies = 1/HZ 秒
```

Q: [Basic] 什么是jiffies和HZ？
A: jiffies是系统启动后的tick计数：
```c
/* HZ定义 - 每秒tick数 */
// 常见值：100, 250, 300, 1000
// 配置选项：CONFIG_HZ
#define HZ 100  // 或 250, 1000

/* jiffies定义 */
extern unsigned long volatile __jiffy_data jiffies;

/* 访问jiffies_64(64位计数器) */
u64 get_jiffies_64(void);

/* jiffies与时间转换 */
unsigned long msecs_to_jiffies(const unsigned int m);
unsigned long usecs_to_jiffies(const unsigned int u);
unsigned long timespec_to_jiffies(const struct timespec *value);
void jiffies_to_timespec(unsigned long jiffies, struct timespec *value);

/* 示例 */
// HZ=1000时：1 jiffies = 1ms
// HZ=100时：1 jiffies = 10ms

unsigned long timeout = jiffies + msecs_to_jiffies(100); // 100ms后

/* jiffies比较（处理回绕）*/
#define time_after(a, b)      ((long)((b) - (a)) < 0)
#define time_before(a, b)     time_after(b, a)
#define time_after_eq(a, b)   ((long)((a) - (b)) >= 0)
#define time_before_eq(a, b)  time_after_eq(b, a)

/* 正确的超时检查 */
if (time_after(jiffies, timeout)) {
    // 已超时
}

/* 错误的超时检查（回绕时会出错）*/
if (jiffies > timeout) {  // 不要这样用！
    // ...
}
```

---

## struct timer_list (传统定时器)

Q: [Intermediate] struct timer_list的结构是什么？
A: timer_list是传统的低精度定时器：
```c
/* include/linux/timer.h */
struct timer_list {
    struct list_head entry;           // 链表节点
    unsigned long expires;            // 到期时间(jiffies)
    struct tvec_base *base;           // 所属的定时器基

    void (*function)(unsigned long);  // 回调函数
    unsigned long data;               // 传给回调的参数

    int slack;                        // 允许的延迟误差

#ifdef CONFIG_TIMER_STATS
    int start_pid;                    // 创建定时器的进程
    void *start_site;                 // 创建位置
    char start_comm[16];              // 进程名
#endif
};

/* 特点 */
1. 精度：jiffies级别（1/HZ秒）
2. 执行上下文：软中断(TIMER_SOFTIRQ)
3. 典型用途：超时处理、周期性任务
4. 优点：低开销
5. 缺点：精度有限
```

Q: [Intermediate] timer_list的初始化和使用方法？
A: 定时器的完整生命周期：
```c
/* 1. 静态定义和初始化 */
static DEFINE_TIMER(my_timer, my_callback, 0, 0);

/* 或展开形式 */
static struct timer_list my_timer = {
    .function = my_callback,
    .expires = 0,
    .data = 0,
};

/* 2. 动态初始化 */
struct timer_list my_timer;

void init_timer(struct timer_list *timer);
// 或使用宏
setup_timer(&my_timer, my_callback, (unsigned long)my_data);

/* 3. 设置到期时间并启动 */
my_timer.expires = jiffies + msecs_to_jiffies(1000); // 1秒后
add_timer(&my_timer);

/* 或一步完成 */
mod_timer(&my_timer, jiffies + HZ);  // 1秒后到期

/* 4. 回调函数 */
static void my_callback(unsigned long data)
{
    struct my_struct *ptr = (struct my_struct *)data;
    
    // 处理定时器事件
    // 注意：运行在软中断上下文！
    
    // 如果需要重复执行
    mod_timer(&my_timer, jiffies + HZ);
}

/* 5. 修改定时器 */
int mod_timer(struct timer_list *timer, unsigned long expires);
// 返回值：0=定时器之前未激活，1=定时器之前已激活

/* 6. 删除定时器 */
int del_timer(struct timer_list *timer);           // 异步删除
int del_timer_sync(struct timer_list *timer);      // 同步删除（等待回调完成）

/* 7. 检查定时器状态 */
int timer_pending(const struct timer_list *timer); // 是否在等待队列中
```

Q: [Intermediate] timer_list的完整使用示例？
A: 一个典型的驱动定时器示例：
```c
#include <linux/timer.h>

struct my_device {
    struct timer_list timer;
    int count;
    spinlock_t lock;
};

/* 定时器回调 */
static void my_timer_callback(unsigned long data)
{
    struct my_device *dev = (struct my_device *)data;
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    dev->count++;
    pr_info("Timer fired, count=%d\n", dev->count);
    spin_unlock_irqrestore(&dev->lock, flags);

    /* 周期性定时器：重新启动 */
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(1000));
}

/* 初始化 */
static int my_init(struct my_device *dev)
{
    spin_lock_init(&dev->lock);
    dev->count = 0;

    /* 初始化定时器 */
    setup_timer(&dev->timer, my_timer_callback, (unsigned long)dev);

    /* 启动定时器 */
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(1000));

    return 0;
}

/* 清理 */
static void my_cleanup(struct my_device *dev)
{
    /* 必须使用del_timer_sync确保回调不再执行 */
    del_timer_sync(&dev->timer);
}

/* 注意事项 */
// 1. 回调运行在软中断上下文，不能睡眠
// 2. 访问共享数据需要适当的锁保护
// 3. 模块卸载前必须删除定时器
// 4. 使用del_timer_sync而不是del_timer
```

---

## struct hrtimer (高精度定时器)

Q: [Intermediate] struct hrtimer的结构是什么？
A: hrtimer提供纳秒级精度的定时器：
```c
/* include/linux/hrtimer.h */
struct hrtimer {
    struct timerqueue_node node;      // 红黑树节点
    ktime_t _softexpires;             // 软到期时间
    enum hrtimer_restart (*function)(struct hrtimer *); // 回调函数
    struct hrtimer_clock_base *base;  // 时钟基
    unsigned long state;              // 状态

#ifdef CONFIG_TIMER_STATS
    int start_pid;
    void *start_site;
    char start_comm[16];
#endif
};

/* hrtimer状态 */
#define HRTIMER_STATE_INACTIVE  0x00  // 未激活
#define HRTIMER_STATE_ENQUEUED  0x01  // 已入队
#define HRTIMER_STATE_CALLBACK  0x02  // 回调执行中
#define HRTIMER_STATE_MIGRATE   0x04  // 正在迁移

/* 回调返回值 */
enum hrtimer_restart {
    HRTIMER_NORESTART,  // 不重启定时器
    HRTIMER_RESTART,    // 重启定时器
};

/* hrtimer vs timer_list */
+------------------+-------------------+-------------------+
|      特性        |   timer_list      |     hrtimer       |
+------------------+-------------------+-------------------+
| 精度             | jiffies(ms级)     | 纳秒级            |
| 数据结构         | 时间轮            | 红黑树            |
| 执行上下文       | 软中断            | 硬中断或软中断    |
| 适用场景         | 超时、延迟        | 精确定时          |
| 开销             | 低                | 相对较高          |
+------------------+-------------------+-------------------+
```

Q: [Intermediate] hrtimer的初始化和使用方法？
A: hrtimer的完整使用流程：
```c
#include <linux/hrtimer.h>
#include <linux/ktime.h>

/* 1. 初始化 */
struct hrtimer my_hrtimer;

void hrtimer_init(struct hrtimer *timer,
                  clockid_t clock_id,      // 时钟类型
                  enum hrtimer_mode mode); // 模式

// 时钟类型
CLOCK_REALTIME    // 墙上时钟（可调整）
CLOCK_MONOTONIC   // 单调时钟（不可调整，推荐）
CLOCK_BOOTTIME    // 包含睡眠时间的单调时钟

// 模式
HRTIMER_MODE_ABS  // 绝对时间
HRTIMER_MODE_REL  // 相对时间

/* 示例初始化 */
hrtimer_init(&my_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
my_hrtimer.function = my_hrtimer_callback;

/* 2. 回调函数 */
static enum hrtimer_restart my_hrtimer_callback(struct hrtimer *timer)
{
    // 处理定时器事件
    pr_info("hrtimer fired!\n");

    // 如果需要周期性执行
    hrtimer_forward_now(timer, ktime_set(1, 0)); // 1秒后
    return HRTIMER_RESTART;

    // 或者单次执行
    // return HRTIMER_NORESTART;
}

/* 3. 启动定时器 */
ktime_t kt = ktime_set(0, 500000000);  // 0.5秒
hrtimer_start(&my_hrtimer, kt, HRTIMER_MODE_REL);

/* 4. 取消定时器 */
int hrtimer_cancel(struct hrtimer *timer);
int hrtimer_try_to_cancel(struct hrtimer *timer);
// hrtimer_cancel：等待回调完成
// hrtimer_try_to_cancel：不等待，可能返回-1表示回调正在执行

/* 5. 检查状态 */
int hrtimer_active(const struct hrtimer *timer);
int hrtimer_is_queued(struct hrtimer *timer);
ktime_t hrtimer_get_remaining(const struct hrtimer *timer);
```

Q: [Intermediate] hrtimer的完整使用示例？
A: 一个高精度定时器的完整示例：
```c
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#define TIMER_INTERVAL_NS  100000000  // 100ms = 100,000,000 ns

static struct hrtimer my_hrtimer;
static ktime_t interval;
static int count = 0;

/* 回调函数 */
static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
    ktime_t now = timer->base->get_time();
    
    count++;
    pr_info("[%lld] Timer callback #%d\n", ktime_to_ns(now), count);

    /* 周期性执行 */
    hrtimer_forward(timer, now, interval);
    return HRTIMER_RESTART;
}

/* 模块初始化 */
static int __init my_module_init(void)
{
    pr_info("Initializing hrtimer module\n");

    /* 设置间隔 */
    interval = ktime_set(0, TIMER_INTERVAL_NS);

    /* 初始化hrtimer */
    hrtimer_init(&my_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    my_hrtimer.function = timer_callback;

    /* 启动定时器 */
    hrtimer_start(&my_hrtimer, interval, HRTIMER_MODE_REL);

    return 0;
}

/* 模块清理 */
static void __exit my_module_exit(void)
{
    int ret = hrtimer_cancel(&my_hrtimer);
    if (ret)
        pr_info("Timer was still active\n");
    
    pr_info("Timer fired %d times\n", count);
}

module_init(my_module_init);
module_exit(my_module_exit);
MODULE_LICENSE("GPL");
```

---

## ktime_t操作 (ktime API)

Q: [Intermediate] ktime_t的操作函数有哪些？
A: ktime_t提供丰富的时间操作API：
```c
/* 创建ktime_t */
ktime_t ktime_set(const s64 secs, const unsigned long nsecs);
// 例：ktime_set(1, 500000000) = 1.5秒

#define ktime_zero  ((ktime_t){ .tv64 = 0 })

/* 从其他类型转换 */
ktime_t ns_to_ktime(u64 ns);
ktime_t ms_to_ktime(u64 ms);
ktime_t timespec_to_ktime(const struct timespec ts);
ktime_t timeval_to_ktime(const struct timeval tv);

/* 转换为其他类型 */
s64 ktime_to_ns(const ktime_t kt);
s64 ktime_to_us(const ktime_t kt);
s64 ktime_to_ms(const ktime_t kt);
struct timespec ktime_to_timespec(const ktime_t kt);
struct timeval ktime_to_timeval(const ktime_t kt);

/* 算术运算 */
ktime_t ktime_add(const ktime_t a, const ktime_t b);
ktime_t ktime_add_ns(const ktime_t kt, u64 nsec);
ktime_t ktime_add_us(const ktime_t kt, u64 usec);
ktime_t ktime_sub(const ktime_t a, const ktime_t b);
ktime_t ktime_sub_ns(const ktime_t kt, u64 nsec);

/* 比较 */
int ktime_compare(const ktime_t cmp1, const ktime_t cmp2);
// 返回：<0(cmp1<cmp2), 0(相等), >0(cmp1>cmp2)

bool ktime_after(const ktime_t cmp1, const ktime_t cmp2);
bool ktime_before(const ktime_t cmp1, const ktime_t cmp2);

/* 获取当前时间 */
ktime_t ktime_get(void);              // CLOCK_MONOTONIC
ktime_t ktime_get_real(void);         // CLOCK_REALTIME
ktime_t ktime_get_boottime(void);     // CLOCK_BOOTTIME

/* 示例 */
ktime_t start = ktime_get();
/* 执行操作 */
ktime_t end = ktime_get();
ktime_t delta = ktime_sub(end, start);
pr_info("Operation took %lld ns\n", ktime_to_ns(delta));
```

---

## struct delayed_work (延迟工作)

Q: [Intermediate] struct delayed_work是什么？
A: delayed_work结合了工作队列和定时器：
```c
/* include/linux/workqueue.h */
struct delayed_work {
    struct work_struct work;    // 工作项
    struct timer_list timer;    // 定时器
};

/* 特点 */
1. 延迟执行工作项
2. 运行在进程上下文（可以睡眠）
3. 由工作队列调度执行
4. 内部使用timer_list实现延迟

/* 与timer_list对比 */
+------------------+-------------------+-------------------+
|      特性        |   timer_list      |  delayed_work     |
+------------------+-------------------+-------------------+
| 执行上下文       | 软中断(不能睡眠)  | 进程上下文(可睡眠)|
| 适用场景         | 简单、快速操作    | 复杂、可能阻塞    |
| 精度             | jiffies           | jiffies           |
| 取消复杂度       | 简单              | 需要flush         |
+------------------+-------------------+-------------------+
```

Q: [Intermediate] delayed_work的使用方法？
A: delayed_work的完整使用示例：
```c
#include <linux/workqueue.h>

struct my_device {
    struct delayed_work dwork;
    int data;
};

/* 1. 静态定义（模块级别）*/
static DECLARE_DELAYED_WORK(my_dwork, my_work_func);

/* 2. 动态初始化 */
struct delayed_work my_dwork;
INIT_DELAYED_WORK(&my_dwork, my_work_func);

/* 3. 工作函数 */
static void my_work_func(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct my_device *dev = container_of(dwork, struct my_device, dwork);

    // 可以睡眠！
    msleep(100);
    
    pr_info("Delayed work executed, data=%d\n", dev->data);

    // 如果需要周期性执行
    schedule_delayed_work(&dev->dwork, msecs_to_jiffies(1000));
}

/* 4. 调度延迟工作 */
// 在系统工作队列上调度
bool schedule_delayed_work(struct delayed_work *dwork, 
                           unsigned long delay);

// 在指定工作队列上调度
bool queue_delayed_work(struct workqueue_struct *wq,
                        struct delayed_work *dwork,
                        unsigned long delay);

/* 5. 修改延迟时间 */
bool mod_delayed_work(struct workqueue_struct *wq,
                      struct delayed_work *dwork,
                      unsigned long delay);

/* 6. 取消延迟工作 */
bool cancel_delayed_work(struct delayed_work *dwork);
bool cancel_delayed_work_sync(struct delayed_work *dwork);

/* 7. 等待完成 */
void flush_delayed_work(struct delayed_work *dwork);

/* 完整示例 */
static void init_my_device(struct my_device *dev)
{
    INIT_DELAYED_WORK(&dev->dwork, my_work_func);
    dev->data = 42;
    
    // 1秒后执行
    schedule_delayed_work(&dev->dwork, msecs_to_jiffies(1000));
}

static void cleanup_my_device(struct my_device *dev)
{
    // 取消并等待正在执行的工作完成
    cancel_delayed_work_sync(&dev->dwork);
}
```

---

## 时钟源 (clocksource)

Q: [Intermediate] struct clocksource的结构是什么？
A: clocksource是时间读取的抽象：
```c
/* include/linux/clocksource.h */
struct clocksource {
    /* 热路径数据 */
    cycle_t (*read)(struct clocksource *cs);  // 读取cycles
    cycle_t cycle_last;                        // 上次读取值
    cycle_t mask;                              // 有效位掩码
    u32 mult;                                  // 乘数(用于转换)
    u32 shift;                                 // 移位(用于转换)
    u64 max_idle_ns;                           // 最大空闲时间
    u32 maxadj;                                // 最大调整值

    const char *name;                          // 名称
    struct list_head list;                     // 链表节点
    int rating;                                // 评分(越高越好)
    
    int (*enable)(struct clocksource *cs);     // 启用
    void (*disable)(struct clocksource *cs);   // 禁用
    unsigned long flags;                       // 标志
    void (*suspend)(struct clocksource *cs);   // 挂起
    void (*resume)(struct clocksource *cs);    // 恢复
};

/* 常见clocksource评分 */
// 1-99:   不推荐使用
// 100-199: 基础时钟(PIT)
// 200-299: 较好时钟
// 300-399: 优秀时钟(HPET)
// 400+:    最好时钟(TSC)

/* cycles到纳秒转换 */
// ns = (cycles * mult) >> shift
static inline s64 clocksource_cyc2ns(cycle_t cycles, u32 mult, u32 shift)
{
    return ((u64) cycles * mult) >> shift;
}
```

Q: [Intermediate] 如何注册自定义clocksource？
A: clocksource注册流程：
```c
/* 定义clocksource */
static cycle_t my_read(struct clocksource *cs)
{
    return my_hardware_read_counter();
}

static struct clocksource my_clocksource = {
    .name   = "my_clock",
    .rating = 300,
    .read   = my_read,
    .mask   = CLOCKSOURCE_MASK(32),  // 32位计数器
    .flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

/* 注册 */
int __init my_clocksource_init(void)
{
    // 计算mult和shift
    // freq_hz是时钟频率
    clocksource_register_hz(&my_clocksource, freq_hz);
    return 0;
}

/* 注销 */
void my_clocksource_exit(void)
{
    clocksource_unregister(&my_clocksource);
}

/* clocksource标志 */
#define CLOCK_SOURCE_IS_CONTINUOUS        0x01  // 连续计数
#define CLOCK_SOURCE_MUST_VERIFY          0x02  // 需要验证
#define CLOCK_SOURCE_WATCHDOG             0x10  // 用作看门狗
#define CLOCK_SOURCE_VALID_FOR_HRES       0x20  // 可用于高精度
#define CLOCK_SOURCE_UNSTABLE             0x40  // 不稳定

/* 查看系统clocksource */
// $ cat /sys/devices/system/clocksource/clocksource0/available_clocksource
// tsc hpet acpi_pm
// $ cat /sys/devices/system/clocksource/clocksource0/current_clocksource
// tsc
```

---

## 时钟事件设备 (clock_event_device)

Q: [Intermediate] struct clock_event_device的结构是什么？
A: clock_event_device产生定时中断：
```c
/* include/linux/clockchips.h */
struct clock_event_device {
    /* 事件处理 */
    void (*event_handler)(struct clock_event_device *);  // 事件回调
    
    /* 设置下次事件 */
    int (*set_next_event)(unsigned long evt,
                          struct clock_event_device *);
    int (*set_next_ktime)(ktime_t expires,
                          struct clock_event_device *);
    
    ktime_t next_event;           // 下次事件时间
    u64 max_delta_ns;             // 最大间隔(ns)
    u64 min_delta_ns;             // 最小间隔(ns)
    u32 mult;                     // 乘数
    u32 shift;                    // 移位
    enum clock_event_mode mode;   // 当前模式
    unsigned int features;        // 特性
    
    void (*broadcast)(const struct cpumask *mask);  // 广播
    void (*set_mode)(enum clock_event_mode mode,
                     struct clock_event_device *);   // 设置模式
    
    const char *name;             // 名称
    int rating;                   // 评分
    int irq;                      // IRQ号
    const struct cpumask *cpumask; // CPU掩码
    struct list_head list;        // 链表
};

/* 时钟事件模式 */
enum clock_event_mode {
    CLOCK_EVT_MODE_UNUSED,        // 未使用
    CLOCK_EVT_MODE_SHUTDOWN,      // 关闭
    CLOCK_EVT_MODE_PERIODIC,      // 周期模式
    CLOCK_EVT_MODE_ONESHOT,       // 单次模式
    CLOCK_EVT_MODE_RESUME,        // 恢复
};

/* 时钟事件特性 */
#define CLOCK_EVT_FEAT_PERIODIC   0x000001  // 支持周期模式
#define CLOCK_EVT_FEAT_ONESHOT    0x000002  // 支持单次模式
#define CLOCK_EVT_FEAT_KTIME      0x000004  // 支持ktime设置
#define CLOCK_EVT_FEAT_C3STOP     0x000008  // C3状态时停止
```

---

## 延迟函数 (Delay Functions)

Q: [Intermediate] 内核中的延迟函数有哪些？
A: 内核提供多种延迟方法：
```c
/* 1. 忙等待（不可睡眠上下文）*/
#include <linux/delay.h>

void ndelay(unsigned long nsecs);  // 纳秒延迟
void udelay(unsigned long usecs);  // 微秒延迟
void mdelay(unsigned long msecs);  // 毫秒延迟

// 注意：这些是忙等待，会消耗CPU！
// 适用：中断上下文、自旋锁内
// 限制：udelay一般不超过1000us

/* 2. 睡眠延迟（可睡眠上下文）*/
#include <linux/delay.h>

void msleep(unsigned int msecs);         // 至少睡眠指定毫秒
unsigned long msleep_interruptible(unsigned int msecs); // 可中断
void ssleep(unsigned int seconds);       // 睡眠秒数

void usleep_range(unsigned long min, unsigned long max); // 微秒范围
// 推荐：允许调度器优化

/* 3. 高精度睡眠 */
#include <linux/hrtimer.h>

void hrtimer_nanosleep(struct timespec *rqtp,
                       struct timespec *rmtp,
                       const enum hrtimer_mode mode,
                       const clockid_t clockid);

/* 延迟选择指南 */
+------------------+----------------------+------------------+
|    延迟时间      |     忙等待           |     睡眠         |
+------------------+----------------------+------------------+
| < 10us           | udelay()             | 不适合           |
| 10us - 20ms      | udelay()/mdelay()    | usleep_range()   |
| > 20ms           | 不推荐               | msleep()         |
+------------------+----------------------+------------------+

/* 示例 */
// 中断处理程序中
spin_lock_irqsave(&lock, flags);
udelay(10);  // 10微秒忙等待
spin_unlock_irqrestore(&lock, flags);

// 进程上下文中
usleep_range(1000, 2000);  // 1-2毫秒睡眠
msleep(100);               // 100毫秒睡眠
```

Q: [Intermediate] schedule_timeout的用法是什么？
A: schedule_timeout是更灵活的延迟机制：
```c
#include <linux/sched.h>

/* 设置当前任务状态并调度 */
signed long schedule_timeout(signed long timeout);

// 返回值：剩余的jiffies（0表示完全超时）

/* 使用模式 */
// 1. 不可中断睡眠
set_current_state(TASK_UNINTERRUPTIBLE);
schedule_timeout(msecs_to_jiffies(1000));

// 2. 可中断睡眠
set_current_state(TASK_INTERRUPTIBLE);
if (schedule_timeout(msecs_to_jiffies(1000))) {
    // 被信号中断
}

/* 便捷函数 */
signed long schedule_timeout_interruptible(signed long timeout);
signed long schedule_timeout_uninterruptible(signed long timeout);
signed long schedule_timeout_killable(signed long timeout);

/* 等待事件带超时 */
#define wait_event_timeout(wq, condition, timeout) \
    ...

long wait_event_interruptible_timeout(wait_queue_head_t wq,
                                       int condition,
                                       long timeout);

/* 示例：带超时的等待 */
DECLARE_WAIT_QUEUE_HEAD(my_wait);
int data_ready = 0;

// 生产者
data_ready = 1;
wake_up(&my_wait);

// 消费者
long ret = wait_event_interruptible_timeout(my_wait, 
                                            data_ready,
                                            msecs_to_jiffies(5000));
if (ret == 0) {
    pr_warn("Timeout waiting for data\n");
} else if (ret < 0) {
    pr_warn("Interrupted by signal\n");
} else {
    pr_info("Data ready, remaining time: %ld\n", ret);
}
```

---

## 获取当前时间 (Getting Current Time)

Q: [Intermediate] 如何在内核中获取当前时间？
A: 内核提供多种获取时间的方法：
```c
/* 1. 获取jiffies */
unsigned long j = jiffies;
u64 j64 = get_jiffies_64();

/* 2. 获取单调时间（推荐用于测量间隔）*/
ktime_t ktime_get(void);
void ktime_get_ts(struct timespec *ts);

// 更高精度版本
ktime_t ktime_get_raw(void);  // 原始硬件时间

/* 3. 获取墙上时间（实际日期时间）*/
ktime_t ktime_get_real(void);
void getnstimeofday(struct timespec *ts);  // 旧API
void ktime_get_real_ts(struct timespec *ts);

/* 4. 获取启动时间（包含睡眠）*/
ktime_t ktime_get_boottime(void);

/* 5. 获取TAI时间 */
ktime_t ktime_get_clocktai(void);

/* 获取当前秒数 */
time64_t ktime_get_real_seconds(void);    // 实时时钟
time64_t ktime_get_seconds(void);         // 单调时钟

/* 时间类型对比 */
+------------------+---------------------------+------------------+
|    时间类型      |         特点              |     用途         |
+------------------+---------------------------+------------------+
| CLOCK_MONOTONIC  | 单调递增，不受NTP调整     | 测量时间间隔     |
| CLOCK_REALTIME   | 墙上时间，可被调整        | 日志时间戳       |
| CLOCK_BOOTTIME   | 包含系统睡眠时间          | 设备唤醒定时     |
| CLOCK_TAI        | 原子时间，无闰秒          | 科学计算         |
+------------------+---------------------------+------------------+

/* 示例：测量代码执行时间 */
ktime_t start, end;
s64 elapsed_ns;

start = ktime_get();
/* 执行要测量的代码 */
end = ktime_get();

elapsed_ns = ktime_to_ns(ktime_sub(end, start));
pr_info("Operation took %lld ns\n", elapsed_ns);
```

---

## 时间轮(Timer Wheel)

Q: [Advanced] Linux定时器的时间轮实现原理？
A: timer_list使用层级时间轮：
```c
/* 时间轮结构 */
// 5级时间轮，每级64个槽位

+---------------------------+
|  TV1 (0-255 jiffies)      | <- 256个槽位，每槽1 jiffies
+---------------------------+
|  TV2 (256-16383)          | <- 64个槽位，每槽256 jiffies  
+---------------------------+
|  TV3 (16384-1048575)      | <- 64个槽位
+---------------------------+
|  TV4 (1M-64M)             | <- 64个槽位
+---------------------------+
|  TV5 (64M-4G)             | <- 64个槽位
+---------------------------+

/* 工作原理 */
1. 添加定时器：根据expires计算槽位
2. 每个tick：处理TV1当前槽位
3. TV1转满一圈：从TV2迁移定时器到TV1
4. 以此类推...

/* 代码结构 */
#define TVN_BITS 6
#define TVR_BITS 8
#define TVN_SIZE (1 << TVN_BITS)  // 64
#define TVR_SIZE (1 << TVR_BITS)  // 256

struct tvec {
    struct list_head vec[TVN_SIZE];  // 64个槽位
};

struct tvec_root {
    struct list_head vec[TVR_SIZE];  // 256个槽位
};

struct tvec_base {
    spinlock_t lock;
    struct timer_list *running_timer;
    unsigned long timer_jiffies;
    unsigned long next_timer;
    struct tvec_root tv1;           // 最细粒度
    struct tvec tv2;                // 第2级
    struct tvec tv3;                // 第3级
    struct tvec tv4;                // 第4级
    struct tvec tv5;                // 第5级
};

/* 定时器索引计算 */
#define INDEX(N) ((base->timer_jiffies >> (TVR_BITS + (N) * TVN_BITS)) & TVN_MASK)

/* 添加定时器到时间轮 */
static void internal_add_timer(struct tvec_base *base,
                               struct timer_list *timer)
{
    unsigned long expires = timer->expires;
    unsigned long idx = expires - base->timer_jiffies;
    struct list_head *vec;

    if (idx < TVR_SIZE) {
        // TV1: 0-255 jiffies
        int i = expires & TVR_MASK;
        vec = base->tv1.vec + i;
    } else if (idx < 1 << (TVR_BITS + TVN_BITS)) {
        // TV2: 256-16383 jiffies
        int i = (expires >> TVR_BITS) & TVN_MASK;
        vec = base->tv2.vec + i;
    }
    // ... TV3, TV4, TV5

    list_add_tail(&timer->entry, vec);
}
```

---

## RTC (Real Time Clock)

Q: [Intermediate] RTC子系统的结构是什么？
A: RTC提供持久化时间存储：
```c
/* include/linux/rtc.h */
struct rtc_device {
    struct device dev;
    struct module *owner;
    
    int id;
    char name[RTC_DEVICE_NAME_SIZE];
    
    const struct rtc_class_ops *ops;  // 操作函数
    struct mutex ops_lock;
    
    struct cdev char_dev;
    unsigned long flags;
    
    unsigned long irq_data;
    spinlock_t irq_lock;
    wait_queue_head_t irq_queue;
    
    struct rtc_task *irq_task;
    spinlock_t irq_task_lock;
    int irq_freq;
    int max_user_freq;
    
    struct timerqueue_head timerqueue;
    struct rtc_timer aie_timer;
    struct rtc_timer uie_rtctimer;
    struct hrtimer pie_timer;
    int pie_enabled;
    struct work_struct irqwork;
};

/* RTC操作函数 */
struct rtc_class_ops {
    int (*open)(struct device *);
    void (*release)(struct device *);
    int (*ioctl)(struct device *, unsigned int, unsigned long);
    int (*read_time)(struct device *, struct rtc_time *);
    int (*set_time)(struct device *, struct rtc_time *);
    int (*read_alarm)(struct device *, struct rtc_wkalrm *);
    int (*set_alarm)(struct device *, struct rtc_wkalrm *);
    int (*proc)(struct device *, struct seq_file *);
    int (*set_mmss)(struct device *, unsigned long secs);
    int (*irq_set_state)(struct device *, int enabled);
    int (*irq_set_freq)(struct device *, int freq);
    int (*read_callback)(struct device *, int data);
    int (*alarm_irq_enable)(struct device *, unsigned int enabled);
};

/* RTC时间结构 */
struct rtc_time {
    int tm_sec;     // 秒 (0-59)
    int tm_min;     // 分 (0-59)
    int tm_hour;    // 时 (0-23)
    int tm_mday;    // 日 (1-31)
    int tm_mon;     // 月 (0-11)
    int tm_year;    // 年 (从1900起)
    int tm_wday;    // 星期 (0-6, 0=周日)
    int tm_yday;    // 年内天数 (0-365)
    int tm_isdst;   // 夏令时
};

/* 用户空间接口 */
// /dev/rtc0 或 /dev/rtc
// ioctl: RTC_RD_TIME, RTC_SET_TIME, RTC_ALM_SET, RTC_ALM_READ等
```

---

## 时间相关系统调用 (Time System Calls)

Q: [Intermediate] 主要的时间系统调用有哪些？
A: Linux提供丰富的时间系统调用：
```c
/* 1. 获取/设置时间 */
// 获取墙上时间
int gettimeofday(struct timeval *tv, struct timezone *tz);

// 获取高精度时间
int clock_gettime(clockid_t clk_id, struct timespec *tp);

// 设置时间（需要权限）
int settimeofday(const struct timeval *tv, const struct timezone *tz);
int clock_settime(clockid_t clk_id, const struct timespec *tp);

/* 2. 睡眠/延迟 */
unsigned int sleep(unsigned int seconds);
int usleep(useconds_t usec);
int nanosleep(const struct timespec *req, struct timespec *rem);
int clock_nanosleep(clockid_t clock_id, int flags,
                    const struct timespec *request,
                    struct timespec *remain);

/* 3. 定时器 */
// POSIX定时器
int timer_create(clockid_t clockid, struct sigevent *sevp, timer_t *timerid);
int timer_settime(timer_t timerid, int flags,
                  const struct itimerspec *new_value,
                  struct itimerspec *old_value);
int timer_gettime(timer_t timerid, struct itimerspec *curr_value);
int timer_delete(timer_t timerid);

// 传统alarm
unsigned int alarm(unsigned int seconds);

// setitimer
int setitimer(int which, const struct itimerval *new_value,
              struct itimerval *old_value);
int getitimer(int which, struct itimerval *curr_value);

/* 4. timerfd (推荐) */
int timerfd_create(int clockid, int flags);
int timerfd_settime(int fd, int flags,
                    const struct itimerspec *new_value,
                    struct itimerspec *old_value);
int timerfd_gettime(int fd, struct itimerspec *curr_value);

/* timerfd示例 */
int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
struct itimerspec its = {
    .it_value = { .tv_sec = 1, .tv_nsec = 0 },     // 首次触发
    .it_interval = { .tv_sec = 1, .tv_nsec = 0 }, // 周期
};
timerfd_settime(tfd, 0, &its, NULL);

// 可以用poll/epoll监控
struct pollfd pfd = { .fd = tfd, .events = POLLIN };
poll(&pfd, 1, -1);

uint64_t expirations;
read(tfd, &expirations, sizeof(expirations));
```

---

## 调试和诊断 (Debugging and Diagnostics)

Q: [Intermediate] 如何调试时间和定时器问题？
A: Linux提供多种调试工具：
```bash
# 1. 查看系统时钟源
$ cat /sys/devices/system/clocksource/clocksource0/current_clocksource
tsc

$ cat /sys/devices/system/clocksource/clocksource0/available_clocksource
tsc hpet acpi_pm

# 2. 查看定时器统计
$ cat /proc/timer_list
Timer List Version: v0.7
HRTIMER_MAX_CLOCK_BASES: 3
now at 123456789012345 nsecs
...

$ cat /proc/timer_stats  # 需要CONFIG_TIMER_STATS
Timer Stats Version: v0.3
Sample period: 5.000 s
...

# 3. 查看CPU频率和TSC
$ cat /proc/cpuinfo | grep -E 'cpu MHz|tsc'

# 4. 时钟中断统计
$ cat /proc/interrupts | grep -i timer
$ cat /proc/interrupts | grep -i hrtimer

# 5. ftrace跟踪定时器
$ echo 'timer:*' >> /sys/kernel/debug/tracing/set_event
$ echo 'hrtimer:*' >> /sys/kernel/debug/tracing/set_event
$ cat /sys/kernel/debug/tracing/trace

# 6. perf分析
$ perf stat -e 'timer:*' -a sleep 10

# 7. 内核配置选项
CONFIG_TIMER_STATS=y          # 定时器统计
CONFIG_HIGH_RES_TIMERS=y      # 高精度定时器
CONFIG_NO_HZ=y                # tickless模式
CONFIG_HZ_1000=y              # 1000Hz tick
```

Q: [Intermediate] 定时器使用的最佳实践是什么？
A: 遵循以下最佳实践：
```c
/* 1. 选择正确的定时器类型 */
// 超时、粗粒度周期任务 -> timer_list
// 精确定时需求 -> hrtimer
// 可睡眠的延迟任务 -> delayed_work

/* 2. 正确初始化和清理 */
// 初始化
setup_timer(&my_timer, callback, data);
// 或
INIT_DELAYED_WORK(&my_dwork, work_func);
// 或
hrtimer_init(&my_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

// 清理
del_timer_sync(&my_timer);           // timer_list
cancel_delayed_work_sync(&my_dwork); // delayed_work
hrtimer_cancel(&my_hrtimer);         // hrtimer

/* 3. 回调函数注意事项 */
// timer_list回调：软中断上下文，不能睡眠
void timer_callback(unsigned long data)
{
    // 使用spin_lock_irqsave保护共享数据
    // 不能调用睡眠函数
}

// delayed_work回调：进程上下文，可以睡眠
void work_callback(struct work_struct *work)
{
    // 可以使用mutex
    // 可以调用睡眠函数
}

// hrtimer回调：根据配置可能是硬中断
enum hrtimer_restart hrtimer_callback(struct hrtimer *timer)
{
    // 通常不能睡眠
    // 快速执行
}

/* 4. 使用正确的时间比较 */
// 正确
if (time_after(jiffies, timeout)) { ... }

// 错误（会受回绕影响）
if (jiffies > timeout) { ... }

/* 5. 模块卸载时确保定时器停止 */
static void __exit my_exit(void)
{
    // 先禁止新的定时器启动
    atomic_set(&running, 0);
    
    // 然后同步删除
    del_timer_sync(&my_timer);
    
    // 或
    cancel_delayed_work_sync(&my_dwork);
}

/* 6. 避免定时器风暴 */
// 使用round_jiffies对齐多个定时器
my_timer.expires = round_jiffies(jiffies + HZ);

// 使用timer_slack减少唤醒
my_timer.slack = HZ / 10;  // 允许10%误差
```

