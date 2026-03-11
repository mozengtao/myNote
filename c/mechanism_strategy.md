## 机制与策略分离
[Linux内核中机制与策略分离的典型场景](Linux内核中机制与策略分离的典型场景.md)  
[C语言机制与策略分离示例代码](C语言机制与策略分离示例代码.md)  
[private_data usage demo](private_data_demo.c)  
[]()  

- 机制
    机制负责'能做什么'，它提供一组能力/钩子/API，本身不决定要怎么用

- 策略
    策略负责'具体怎么做'，决定具体行为

- 机制与策略分离的优势
    可扩展：增加新策略，只需实现一组回调 / 新的策略模块，不改通用机制
    可测试：机制代码可以用 '假策略' 测试（例如 stub 策略），方便单元测试
    可复用：同一机制可以被多个策略使用（例如不同调度算法、不同日志后端）
    隔离复杂性：复杂业务逻辑集中在策略层，机制层代码更小、更稳定

- 关键思想
    机制 = 通用框架 + 钩子
    策略 = 通过钩子填入的具体实现

- 如何实现机制与策略分离
    1. 识别机制
        找出 '总是需要的框架性工作'，例如
            循环调度框架（while(1) 轮询）
            共享内存映射、锁管理
            协议报文的通用解析
    2. 识别策略
        找出 '经常变' 的规则，例如
            不同的转发策略 / ACL / 计费规则
            不同的调度算法 / 优先级规则
            不同的导出目标（collectd / Prometheus / 自定义 log）
    3. 抽象出接口
        设计一个 *_ops 结构体，或一组函数指针
        让机制代码只面向这个接口，而不是具体实现
    4. 通过注入选择策略
        在初始化阶段，根据配置 / 编译选项创建具体策略对象，填好 ops 和 ctx
        把它们交给机制层统一使用
    5. 保持接口稳定
        改策略时，只动新策略模块，不改机制层 API
        机制层只做与策略无关的通用工作（资源管理、调度框架、错误处理）

- C语言中机制与策略分离的实现手段
    函数指针
    回调表
    不透明指针
    模块化设计

```c
// 回调表
// 典型写法: 机制定义一个 '操作表(ops)'，策略提供具体实现
typedef struct {
    int  (*open)(void *ctx);
    int  (*close)(void *ctx);
    int  (*send)(void *ctx, const void *buf, size_t len);
    int  (*recv)(void *ctx, void *buf, size_t len);
} net_ops_t;

typedef struct {
    net_ops_t *ops;  // 策略
    void      *ctx;  // 策略上下文（socket、fd、DPDK port 等）
} net_if_t;

// 机制：上层只依赖 net_if_t，不关心具体策略
int net_send_packet(net_if_t *iface, const void *buf, size_t len)
{
    return iface->ops->send(iface->ctx, buf, len);
}
/*
机制层：net_send_packet 等函数，只知道通过 ops->send 发送
策略层：提供 tcp_ops、udp_ops、dpdk_ops 等不同实现
当换成 DPDK、AF_XDP 或普通 socket，只需要换 ops 和 ctx，机制不变
*/

// 上下文结构 + 回调
// 常见于库或框架：机制定义一个“框架流程”，留出若干回调点
typedef struct {
    int  (*on_packet)(void *user_ctx, const uint8_t *pkt, size_t len);
    void (*on_error)(void *user_ctx, int err);
    void *user_ctx;
} packet_handler_t;

// 机制：轮询收包，并在适当时机调用策略
void run_loop(int fd, packet_handler_t *handler)
{
    uint8_t buf[2048];
    ssize_t len;

    for (;;) {
        len = recv(fd, buf, sizeof(buf), 0);
        if (len < 0) {
            if (handler->on_error)
                handler->on_error(handler->user_ctx, errno);
            continue;
        }
        if (handler->on_packet)
            handler->on_packet(handler->user_ctx, buf, (size_t)len);
    }
}

/*
run_loop 是数据面机制：负责收包、循环、错误处理框架
策略通过 handler->on_packet 和 user_ctx 决定“每个包怎么处理”
*/

// void * + 用户自定义数据结构
// 机制定义操作接口和一个不透明的 void * 句柄，策略定义真正的结构和行为
typedef struct allocator allocator_t;

allocator_t *allocator_create(void *impl, 
                              void *(*alloc)(void *impl, size_t),
                              void (*dealloc)(void *impl, void *));
void        *allocator_alloc(allocator_t *a, size_t size);
void         allocator_free(allocator_t *a, void *p);

/*
机制：只实现 allocator_t 的调度、统计、锁等通用逻辑
策略：在 impl 中放不同算法的数据结构（buddy/链表/slab），并提供相应分配/释放函数
*/

// 调度器（Scheduler）与调度算法
// 机制: 提供“添加任务”、“选择下一个任务”、“记录调度统计”等基本操作
// 策略: FIFO、优先级队列、加权轮询（WRR）、WFQ、DRR 等
// 定义通用的 scheduler_t
typedef struct scheduler_ops {
    void (*enqueue)(void *ctx, task_t *t);
    task_t *(*dequeue)(void *ctx);
} scheduler_ops_t;

typedef struct {
    scheduler_ops_t *ops;
    void            *ctx;
} scheduler_t;
// 各种调度算法实现自己的 ctx + ops，统一挂在 scheduler_t 上使用
// 很多网络栈、特别是 DPDK HQoS、SF 队列这类代码，都采用这种模式

// 日志机制与日志策略
机制：提供 log_debug, log_info, log_error 等统一入口，并处理线程安全、格式化
策略：
    输出到 syslog
    输出到文件
    输出到远程 server 或 ring buffer

C 实现：
    在 log_init() 时注册一个 log_backend 回调表（write_line, flush）
    不同部署通过配置选择不同策略实现
```

## mark a function parameter as intentionally unused
```c
void handler(int event, void *ctx)
{
    (void)ctx;  // ctx required by the signature, but unused
    printf("event = %d\n", event);
}

// how (void)parameter; works
It performs a cast-to-void, which evaluates parameter but explicitly throws away the result.
This counts as a “use”, so the compiler no longer warns.
It has zero runtime effect: the compiler optimizes it out completely.

(void)parameter; is used to:
    ✔ Silence "unused parameter" warnings
    ✔ Indicate intentional non-use
    ✔ Guarantee zero runtime cost
```

## kobject
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// 简单的链表实现（放在最前以便后续结构体使用）
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

// 遍历宏
#define list_for_each(pos, head) \
    for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = NULL;
    entry->prev = NULL;
}

// 模拟 kref (引用计数)
struct kref {
    int refcount;
};

// 模拟 kobject
struct kobject {
    char name[32];
    struct kref kref;
    void (*release)(struct kobject *kobj);
    struct list_head list;
};

// 全局对象列表
static LIST_HEAD(object_list);

// 初始化链表头
void list_head_init(void)
{
    INIT_LIST_HEAD(&object_list);
}

// kref 操作函数
void kref_init(struct kref *kref)
{
    kref->refcount = 1;
    printf("kref_init: refcount = 1\n");
}

void kref_get(struct kref *kref)
{
    kref->refcount++;
    printf("kref_get: refcount = %d\n", kref->refcount);
}

int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
    kref->refcount--;
    printf("kref_put: refcount = %d\n", kref->refcount);
    
    if (kref->refcount == 0) {
        if (release) {
            release(kref);
        }
        return 1;
    }
    return 0;
}

// kobject 操作函数
void kobject_init(struct kobject *kobj, const char *name)
{
    strncpy(kobj->name, name, sizeof(kobj->name) - 1);
    kobj->name[sizeof(kobj->name) - 1] = '\0';
    kref_init(&kobj->kref);
    INIT_LIST_HEAD(&kobj->list);
    
    // 添加到全局列表
    list_add(&kobj->list, &object_list);
    printf("kobject_init: created '%s'\n", name);
}

struct kobject *kobject_get(struct kobject *kobj)
{
    if (kobj) {
        kref_get(&kobj->kref);
    }
    return kobj;
}

static void kobject_release(struct kref *kref)
{
    struct kobject *kobj = (struct kobject *)((char *)kref - 
                         (unsigned long)(&((struct kobject *)0)->kref));
    
    printf("kobject_release: releasing '%s'\n", kobj->name);
    
    // 从列表中删除
    list_del(&kobj->list);
    
    // 调用用户提供的release函数
    if (kobj->release) {
        kobj->release(kobj);
    }
}

void kobject_put(struct kobject *kobj)
{
    if (kobj) {
        printf("kobject_put: putting '%s'\n", kobj->name);
        kref_put(&kobj->kref, kobject_release);
    }
}

// 示例设备结构
struct my_device {
    char description[64];
    int device_id;
    struct kobject kobj;
};

void device_release(struct kobject *kobj)
{
    struct my_device *dev = list_entry(kobj, struct my_device, kobj);
    
    printf("device_release: freeing device '%s' (id=%d)\n", 
           dev->description, dev->device_id);
    
    // 实际释放设备内存
    free(dev);
}

// 创建设备函数
struct my_device *create_device(const char *desc, int id)
{
    struct my_device *dev = malloc(sizeof(struct my_device));
    if (!dev) {
        return NULL;
    }
    
    strncpy(dev->description, desc, sizeof(dev->description) - 1);
    dev->description[sizeof(dev->description) - 1] = '\0';
    dev->device_id = id;
    
    // 初始化内嵌的 kobject
    kobject_init(&dev->kobj, desc);
    dev->kobj.release = device_release;
    
    return dev;
}

// 显示当前所有对象
void show_all_objects(void)
{
    struct list_head *pos;
    struct kobject *kobj;
    
    printf("\n=== Current Objects ===\n");
    list_for_each(pos, &object_list) {
        kobj = list_entry(pos, struct kobject, list);
        printf("Object: %s (refcount=%d)\n", kobj->name, kobj->kref.refcount);
    }
    printf("=======================\n\n");
}

// 测试函数
void test_kobject_lifecycle(void)
{
    printf("=== 开始 kobject 生命周期测试 ===\n\n");
    
    // 创建设备对象
    struct my_device *dev1 = create_device("Network Card", 1);
    struct my_device *dev2 = create_device("USB Controller", 2);
    
    show_all_objects();
    
    // 模拟多个模块使用设备
    printf("=== 模拟模块A使用网络卡 ===\n");
    struct kobject *dev1_ref1 = kobject_get(&dev1->kobj);
    
    printf("=== 模拟模块B使用网络卡 ===\n");
    struct kobject *dev1_ref2 = kobject_get(&dev1->kobj);
    
    show_all_objects();
    
    // 模块B停止使用
    printf("=== 模块B停止使用网络卡 ===\n");
    kobject_put(dev1_ref2);
    
    show_all_objects();
    
    // 尝试释放 USB 控制器（应该不会真的释放，因为还有引用）
    printf("=== 尝试释放USB控制器 ===\n");
    kobject_put(&dev2->kobj);
    
    show_all_objects();
    
    // 模块A停止使用网络卡
    printf("=== 模块A停止使用网络卡 ===\n");
    kobject_put(dev1_ref1);
    
    show_all_objects();
    
    // 最终释放网络卡
    printf("=== 最终释放网络卡 ===\n");
    kobject_put(&dev1->kobj);
    
    show_all_objects();
    
    // 最终释放USB控制器
    printf("=== 最终释放USB控制器 ===\n");
    kobject_put(&dev2->kobj);
    
    show_all_objects();
}

int main(void)
{
    // 初始化全局链表
    list_head_init();
    
    // 运行测试
    test_kobject_lifecycle();
    
    printf("=== 测试完成 ===\n");
    return 0;
}
```