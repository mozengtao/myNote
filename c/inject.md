## 类型擦除 + 依赖注入
![依赖注入 (Dependency Injection)](./DI/Dependency_Injection_in_C.md)  
[Dependency Injection](https://github.com/rahff/Software_book) #pdf  
[Dependency Injection in .NET](https://github.com/huutoannht/Books)#pdf  
[Design Pattern Online Training using .NET](https://dotnettutorials.net/lesson/design-patterns-online-training/)  
[Dependency Injection](https://blog.ploeh.dk/tags/#Dependency%20Injection-ref)  
[Hands-On Dependency Injection in Go](https://github.com/PacktPublishing/Hands-On-Dependency-Injection-in-Go)  
[]()  
[]()  
[]()  
[]()  

## 核心概念：
```
    WITHOUT DI (紧耦合)                    WITH DI (松耦合)

    ┌─────────────────┐                   ┌─────────────────┐
    │  OrderService   │                   │  OrderService   │
    │                 │                   │                 │
    │  FileLogger log │──┐                │  Logger *logger │──► <<interface>>
    │  log.write(...) │  │                │  logger->log()  │         Logger
    └─────────────────┘  │                └─────────────────┘            │
                         │                                          ┌────┴────┐
                         ▼                                          ▼         ▼
                ┌─────────────┐                                  FileLogger  MockLogger
                │ FileLogger  │
                │ (hardcoded) │
                └─────────────┘
```

## C语言实现关键技术：
```c
    /* 接口 = 函数指针结构体 */
    typedef struct Logger {
        void (*log)(struct Logger *self, const char *msg);
        void (*destroy)(struct Logger *self);
        void *private_data;  /* 实现特定数据 */
    } Logger;

    /* 构造函数注入 */
    OrderService* order_service_create(Logger *logger) {
        OrderService *svc = malloc(sizeof(OrderService));
        svc->logger = logger;  /* 注入依赖 */
        return svc;
    }
```

## 识别需要依赖注入的代码坏味道：
```c
    坏味道	        示例
    直接实例化	    FileLogger *log = file_logger_create();
    全局变量	    static Database *g_database;
    硬编码调用	    send_email("smtp.example.com", ...)
    条件分支切换	if (use_file) {...} else if (use_syslog) {...}
```

## 重构6步骤：
1. 识别依赖 - 找到函数内创建的具体对象
2. 提取接口 - 定义函数指针结构体
3. 添加依赖字段 - 在主结构体中添加接口指针
4. 构造函数注入 - 通过创建函数传入依赖
5. 使用接口 - 业务逻辑只调用接口方法
6. 组合根 - 在 main 中创建和组装所有对象


- 类型擦除  =  统一接口 + void* 作为 Opaque 实例
- 依赖注入  =  外部提供/赋值/注册 ops，实现控制反转和可替换实现

- 类型擦除 (Type Erasure)
	把具体类型隐藏/擦掉，只通过一个统一的抽象接口来操作对象

    - C实现方法:
        void* 作为通用容器（类型被“擦掉”）
        函数指针 (*fn)(void*) 作为统一操作接口

- 依赖注入 (Dependency Injection, DI)
	一个模块不自己创建它依赖的对象，而是由外部把依赖"注入"（传入）给它

    - 目的:
        解耦
        方便替换实现(Mock/Test/Plugin)
        控制反转(IoC: Inversion of Control)(你不找依赖，而是依赖找你)

    - C实现方法:
        通过函数参数传入依赖
        通过结构体保存依赖的接口或void*指针

## 示例
### version 1: 函数参数注入依赖
```c
/* version 1: 函数参数注入依赖 */

#include <stdio.h>
#include <string.h>

// -------- 抽象依赖接口（类型擦除封装） --------
typedef struct {
    void *instance;                     // 被擦除的具体类型
    void (*logFn)(void *, const char *);// 统一操作接口
} Logger;

// -------- 具体依赖实现 1 --------
typedef struct {
    char prefix[16];
} ConsoleLogger;

void consoleLog(void *self, const char *msg) {
    ConsoleLogger *logger = (ConsoleLogger*)self;
    printf("%s: %s\n", logger->prefix, msg);
}

// -------- 具体依赖实现 2（可被替换）--------
typedef struct { } SimpleLogger;

void simpleLog(void *self, const char *msg) {
    (void)self;
    printf("[LOG] %s\n", msg);
}

// -------- 需要被注入依赖的函数 --------
void fun(Logger logger) {
    logger.logFn(logger.instance, "Linux boot study is fun!");
}

// -------- main 负责创建并注入依赖 --------
int main() {
    // 创建 logger 实现 1
    ConsoleLogger clog;
    strcpy(clog.prefix, "Kernel");

    Logger logger1 = { &clog, consoleLog };
    fun(logger1);  // 依赖注入 + 类型擦除调用

    // 也可以替换为实现 2
    SimpleLogger slog;
    Logger logger2 = { &slog, simpleLog };
    fun(logger2);  // 无需修改 fun 内部代码

    return 0;
}
```

### version 2: 结构体成员注入依赖
```c
#include <stdio.h>
#include <string.h>

// 被擦除的依赖接口
typedef struct {
    void *instance;
    void (*logFn)(void *, const char *);
} Logger;

// 具体依赖实现
typedef struct {
    char prefix[16];
} ConsoleLogger;

void consoleLog(void *self, const char *msg) {
    ConsoleLogger *logger = (ConsoleLogger*)self;
    printf("%s: %s\n", logger->prefix, msg);
}

// 业务模块结构体，依赖通过成员注入
typedef struct {
    Logger logger; // 依赖
} Module;

void fun(Module *module) {
    module->logger.logFn(module->logger.instance, "Injected by struct member!");
}

int main() {
    // 创建具体 Logger
    ConsoleLogger clog;
    strcpy(clog.prefix, "Boot");

    // 注入到 Module 结构体成员
    Module m = { .logger = { &clog, consoleLog } };

    fun(&m);
    return 0;
}
```

### version 3: 函数指针表模拟面向对象 + 多态
```c
#include <stdio.h>
#include <string.h>

// ----- 模拟抽象基类 -----
typedef struct Logger Logger;

typedef struct {
    void (*log)(Logger*, const char *);
} LoggerOps;

struct Logger {
    void *instance;
    LoggerOps *ops; // vtable
};

// ----- 具体类 1 -----
typedef struct {
    char prefix[16];
} ConsoleLogger;

void consoleLogger_log(Logger *logger, const char *msg) {
    ConsoleLogger *c = (ConsoleLogger*)logger->instance;
    printf("%s: %s\n", c->prefix, msg);
}

LoggerOps consoleOps = {
    .log = (void(*)(Logger*,const char*))consoleLogger_log
};

// ----- 具体类 2 -----
typedef struct { } SimpleLogger;

void simpleLogger_log(Logger *logger, const char *msg) {
    (void)logger;
    printf("[SIMPLE] %s\n", msg);
}

LoggerOps simpleOps = {
    .log = (void(*)(Logger*,const char*))simpleLogger_log
};

// ----- 业务代码，依赖抽象 Logger（多态） -----
typedef struct {
    Logger logger; // 通过成员组合持有
} Module;

void fun(Module *m) {
    m->logger.ops->log((Logger*)&m->logger.ops[-1], "Polymorphism style!"); 
    // ↑ 这里只是演示 Linux-like ops 调用，不推荐在真实代码里这么写
}

int main() {
    // 创建具体对象 1
    ConsoleLogger clog;
    strcpy(clog.prefix, "Kernel");
    Logger logger1 = { .instance = &clog, .ops = &consoleOps };

    // 创建具体对象 2
    SimpleLogger slog;
    Logger logger2 = { .instance = &slog, .ops = &simpleOps };

    Module m1 = { .logger = logger1 };
    Module m2 = { .logger = logger2 };

    printf("--- module 1 ---\n");
    fun(&m1);

    printf("--- module 2 ---\n");
    fun(&m2);

    return 0;
}
```

### version 4: Mini 插件框架
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ----- 插件 Ops 接口（抽象能力，类似 Linux driver/file ops）-----
typedef struct {
    const char *name;
    void (*run)(void*);
    void (*destroy)(void*);
} PluginOps;

// ----- 插件实例的通用容器（类型擦除）-----
typedef struct Plugin {
    void *instance;
    PluginOps *ops;
} Plugin;

// ----- 插件实现 1 -----
typedef struct {
    char message[64];
} PluginA;

void pluginA_run(void *self) {
    PluginA *a = (PluginA*)self;
    printf("[PluginA Run] %s\n", a->message);
}

void pluginA_destroy(void *self) {
    printf("[PluginA Destroy]\n");
    free(self);
}

PluginOps pluginA_ops = {
    .name = "pluginA",
    .run = pluginA_run,
    .destroy = pluginA_destroy,
    .destroy = pluginA_destroy
};

// ----- 插件实现 2 -----
typedef struct { int x; } PluginB;

void pluginB_run(void *self) {
    PluginB *b = (PluginB*)self;
    printf("[PluginB Run] x = %d\n", b->x);
}

void pluginB_destroy(void *self) {
    printf("[PluginB Destroy]\n");
    free(self);
}

PluginOps pluginB_ops = {
    .name = "pluginB",
    .run = pluginB_run,
    .destroy = pluginB_destroy
};

// ----- 框架核心：插件注册与控制反转（依赖注入点）-----
#define MAX_PLUGINS 8
Plugin *registry[MAX_PLUGINS];
int count = 0;

void registerPlugin(void *instance, PluginOps *ops) {
    if (count >= MAX_PLUGINS) return;
    registry[count] = malloc(sizeof(Plugin));
    registry[count]->instance = instance;
    registry[count]->ops = ops;
    count++;
}

void runAll() {
    for (int i = 0; i < count; i++) {
        printf(">>> running %s\n", registry[i]->ops->name);
        registry[i]->ops->run(registry[i]->instance);
    }
}

void cleanup() {
    for (int i = 0; i < count; i++) {
        registry[i]->ops->destroy(registry[i]->instance);
        free(registry[i]);
    }
}

int main() {
    // 创建并注入 PluginA
    PluginA *a = malloc(sizeof(PluginA));
    strcpy(a->message, "Hello from pluginA");

    // 创建并注入 PluginB
    PluginB *b = malloc(sizeof(PluginB));
    b->x = 42;

    // 注册到框架（依赖注入 + 类型擦除）
    registerPlugin(a, &pluginA_ops);
    registerPlugin(b, &pluginB_ops);

    // 运行插件
    runAll();

    // 清理
    cleanup();
    return 0;
}
```

## linux 内核代码中类型擦除与依赖注入 核心代码摘要
```c
// 类型擦除 (Type Erasure)：void* + ops（函数指针表 / vtable）
// 抽象能力通过 ops 描述，实例用 void* 擦除具体类型
struct file_operations {
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    int     (*open) (struct inode *, struct file *);
};

struct device_driver {
    const char *name;
    int (*probe)(struct device *);   // 统一入口
    void(*remove)(struct device *);
};

// 具体实例是 opaque 的，只看成 void*
struct file {
    const struct file_operations *f_op;
    void *private_data;  // 类型被擦除
}

调用点：不关心实际类型，只关心 ops

file->f_op->read(...);
drv->probe(dev);  // 统一调用

// 依赖注入 (Dependency Injection / IoC)：核心不自己创建，由框架层传入或注册
// 框架层创建硬件抽象对象，并注入 Driver ops
driver->probe = my_probe_impl;  // 通过赋值注入依赖实现

// 平台/硬件信息由外部注入，而不是 Driver 内部生成
int my_probe_impl(struct device *dev) {
    struct of_node *dn = dev->of_node; 
    const void *dt_data = of_get_property(dn, "config", NULL);
    // ↑ 设备配置信息来自 Device Tree (bootloader/框架传入)
}
```

### Linux 典型数据流（控制反转）：

Bootloader 解析 DTB -> 创建 device -> 注册/匹配 driver -> 调用 driver->probe(device)

Driver 不负责 new 设备对象或平台数据，它只是被动实现 ops->probe()，依赖由外部注入

```c

Linux `start_kernel()` 中的 IoC（控制反转）体现总结
    - Linux 启动初始化遵循典型的**控制反转 (IoC)** 设计：
        **核心（上层）代码掌控流程**
        **具体模块只注册能力/回调 (ops)，不直接执行调用**
        **对象和依赖实例由框架层创建或注入**
        **调用时机由框架编排，而非模块内部决定**

1. Console 机制：依赖注入 + 回调注册 (IoC + DI)

    - 关键点
        `start_kernel()` 调用 `console_init()` 只是初始化 console 框架
        **具体 `write` 实现由 Console Driver 通过 `register_console()` 注入**
        print/log 代码 later 只通过 `console->write()` 调用，不关心具体类型

    - 核心代码摘要
        ```c
        void console_init(void) {
            // 初始化 console 框架（console_list 等），但不包含具体 write 实现
        }

        register_console(&my_console);  // Console driver 注入具体 ops

        struct console {
            void (*write)(struct console *con, const char *s, unsigned n); // 回调表
        };
    
    - 说明
        print 代码不是主动控制谁打印、怎么打印，而是把控制权交给具体 console driver 通过 ops 提供实现 → 控制反转

- Bus/Driver/Device Model：设备由框架创建，Probe later 触发
    - 关键点
        内核先 bus_register() 建立统一总线框架
        driver_register() 只是注册 ops，不决定 probe 时机
        struct device 由 Bootloader 解析 DT、ACPI、bus scan 或 hotplug 生成并注入 Driver
        framework later 触发 driver->probe(dev)

    - 核心代码摘要
    ```c
    bus_register(&platform_bus_type); // 内核建立 Bus 框架

    driver_register(&drv); 
        // 仅注册 probe/remove/suspend 等 ops，不立即调用

    platform_device_register(dev); 
        // 设备由 platform/框架创建并注入 model

    ---------------------------------------

    struct device_driver {
        const char *name;
        int  (*probe)  (struct device *dev);
        void (*remove) (struct device *dev);
    };

    struct device {
        const struct device_driver *driver; // later 由框架依赖注入
        void *platform_data; // 类型擦除，opaque 依赖实例
    };
    ```

    -说明
        创建对象 + 调用调用时机控制权在 Bus 框架，不在 driver → IoC

- initcall 机制：初始化函数由模块注册，由内核 later 按 section 调用
    - 关键点
        模块通过 *_initcall(fn) 注册初始化能力
        start_kernel() later 统一按预定义顺序调用，不是模块内部调用
        让内核初始化过程可扩展 + 可定制

    - 核心代码摘要
    ```c
    early_initcall(my_early_fn);   // 关键基础能力注册
    core_initcall(my_core_fn);     // 核心机制注册
    device_initcall(my_driver_fn); // 设备驱动类初始化注册

    // start_kernel later 统一调用所有注册的 initcall（控制反转）
    do_initcalls();
    ```

    - 说明
        initcall 的注册者提供能力定义，真正执行者是内核框架，调用顺序来自 ELF section 注册布局 → IoC

-  ops 结构体 + void* 实例：Linux style OOP 抽象与多态
    - 关键点
        通过函数指针表实现面向对象与多态
        通过 void* 擦除类型实现Opaque 依赖实例
        具体能力通过 ops 提供，依赖通过成员注入 (DI)

    - 核心代码摘要
    ```c
    struct file_operations {
        ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
        int     (*open) (struct inode *, struct file *);
    };

    struct net_device_ops {
        int (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);
    };

    struct file {
        const struct file_operations *f_op; // 依赖注入 (DI)
        void *private_data; // 类型擦除 (Type Erasure)
    };
    ```

    - 说明
        这些系统调用点 later 通过 file->f_op->read()、dev->netdev_ops->ndo_start_xmit() 进行调用，
        上层不关心具体类型，只关心行为接口 ops → IoC + Polymorphism

-  IoC 在 Linux 启动阶段数据 & 控制流总览
    Power On
    ↓ (硬件流程掌控者)
    BIOS / Firmware (UEFI)
    ↓ (加载与包裹执行环境，但不掌控 kernel init 逻辑)
    Bootloader (GRUB / U-Boot / etc)
    ↓ (解析硬件配置，构建 Device Tree / 平台信息)
    Kernel (`start_kernel()`)
    ↓ (构建 IoC 框架容器，注册机制)
    bus_register(), console_init()
    ↓ (注册模块能力)
    driver_register(), *_initcall(fn)
    ↓ (Later 触发——调用时机交给 framework)
    driver->probe(device), console->write(...), do_initcalls()
    ↓ (最后控制反转交到用户态进一步接管)
    PID1 init 进程启动

- 一句话总结
Linux 启动初始化中的 IoC 本质：
    框架层创建对象 + 掌控调用时机；
    模块层注册能力/ops + 接收依赖实例；
    调用点不关心底层类型，仅调用 ops 指针

## 依赖注入DI/IoC(Inversion of Control)
[linux驱动模型](01_driver_model_ioc.md)  
[linux console子系统](02_console_subsystem_ioc.md)  
[VFS多态](03_vfs_polymorphic_ops_ioc.md)  
[网络子系统](04_net_device_ops_ioc.md)  
[initcall编排机制](05_initcall_mechanism_ioc.md)  

- linux-ioc-patterns
├── README.md                    (360行)  - 索引与学习指南
├── 01_driver_model.md          (947行)  - Linux 驱动模型
├── 02_console_subsystem.md     (786行)  - 控制台子系统
├── 03_vfs_file_operations.md   (807行)  - VFS 多态 ops
├── 04_net_device_ops.md        (920行)  - 网络设备 ops
└── 05_initcall_mechanism.md    (791行)  - initcall 机制

## 五种 IoC 模式对比
    场景	        注入时机	                控制反转体现	                           核心好处
    Driver Model	运行时 (driver_register)	驱动不主动找设备，框架自动匹配绑定	        热插拔、统一生命周期
    Console	        运行时 (register_console)	printk 不知道输出到哪，console 动态注入	   多输出并行、运行时切换
    VFS ops	        运行时 (inode 创建时)	    read/write 调用不关心具体文件系统	        统一接口、多态实现
    net_device_ops	运行时 (probe 时)	        协议栈不关心具体硬件	                   硬件抽象、中间层设备
    initcall	    编译时 (链接器)	            模块不决定初始化时机和顺序	                自动排序、零耦合

## IoC 共同模式
```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Linux 内核 IoC 通用模式                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   1. 定义接口契约 (函数指针结构体)                                           │
│      struct xxx_ops {                                                        │
│          int (*operation_a)(...);                                           │
│          int (*operation_b)(...);                                           │
│      };                                                                      │
│                                                                              │
│   2. 具体实现注入 (填充函数指针)                                             │
│      static const struct xxx_ops my_ops = {                                 │
│          .operation_a = my_impl_a,                                          │
│          .operation_b = my_impl_b,                                          │
│      };                                                                      │
│                                                                              │
│   3. 注册到框架 (关联对象与 ops)                                             │
│      obj->ops = &my_ops;                                                    │
│      register_xxx(obj);                                                     │
│                                                                              │
│   4. 框架调用 (多态分发)                                                     │
│      obj->ops->operation_a(...);                                            │
│                                                                              │
│   好处:                                                                      │
│   - 调用者与实现解耦                                                         │
│   - 支持运行时替换                                                           │
│   - 统一的生命周期管理                                                       │
│   - 易于扩展新实现                                                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```