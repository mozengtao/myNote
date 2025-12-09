# C Language Design Patterns in Linux Kernel

This directory contains detailed analysis of 12 classic design patterns as implemented in the Linux kernel, along with user-space C code examples that mimic the kernel's implementation style.

## Pattern Categories

```
+=========================================================================+
||                      C DESIGN PATTERNS OVERVIEW                       ||
+=========================================================================+
|                                                                         |
|   +---------------------------+                                         |
|   |    CREATIONAL PATTERNS    |  Focus: Object creation mechanisms      |
|   +---------------------------+                                         |
|   | 01. Singleton             |  Single global instance                 |
|   | 02. Factory               |  Object creation delegation             |
|   | 03. Prototype             |  Object cloning/copying                 |
|   +---------------------------+                                         |
|                                                                         |
|   +---------------------------+                                         |
|   |    STRUCTURAL PATTERNS    |  Focus: Object composition              |
|   +---------------------------+                                         |
|   | 04. Adapter               |  Interface conversion                   |
|   | 05. Decorator             |  Dynamic behavior extension             |
|   | 06. Composite             |  Tree-like object structures            |
|   | 07. Bridge                |  Abstraction/Implementation split       |
|   +---------------------------+                                         |
|                                                                         |
|   +---------------------------+                                         |
|   |    BEHAVIORAL PATTERNS    |  Focus: Object interaction              |
|   +---------------------------+                                         |
|   | 08. Strategy              |  Algorithm encapsulation                |
|   | 09. Observer              |  Event notification                     |
|   | 10. Command               |  Request encapsulation                  |
|   | 11. Iterator              |  Sequential access                      |
|   | 12. State                 |  State-dependent behavior               |
|   +---------------------------+                                         |
|                                                                         |
+=========================================================================+
```

---

## Document Structure

Each pattern document includes:

1. **Pattern Overview** - ASCII diagram and concept explanation
2. **Linux Kernel Implementation** - Real kernel code examples with analysis
3. **Architecture Diagram** - Visual representation of kernel implementation
4. **Advantages Analysis** - Benefits table with explanations
5. **User-Space Example** - Complete, compilable C code mimicking kernel style
6. **Flow Diagram** - Execution flow visualization
7. **Key Implementation Points** - Summary of important implementation details

---

## Pattern Index

### Creational Patterns

| # | Pattern | Kernel Example | Key Concept |
|---|---------|----------------|-------------|
| 01 | [Singleton](01_singleton_pattern.md) | sep_dev, init_task | Static variable + access function |
| 02 | [Factory](02_factory_pattern.md) | socket creation, crypto_alloc_tfm | Registration table + create function |
| 03 | [Prototype](03_prototype_pattern.md) | fork(), sk_clone(), dup_mm() | Object cloning with memcpy |

### Structural Patterns

| # | Pattern | Kernel Example | Key Concept |
|---|---------|----------------|-------------|
| 04 | [Adapter](04_adapter_pattern.md) | VFS layer, I2C SMBus emulation | Interface translation layer |
| 05 | [Decorator](05_decorator_pattern.md) | ftrace, block tracing | Nested function wrappers |
| 06 | [Composite](06_composite_pattern.md) | Device tree, kobject hierarchy | Tree structure with uniform interface |
| 07 | [Bridge](07_bridge_pattern.md) | Device-Driver model | Separate abstraction and implementation |

### Behavioral Patterns

| # | Pattern | Kernel Example | Key Concept |
|---|---------|----------------|-------------|
| 08 | [Strategy](08_strategy_pattern.md) | Scheduler classes, file_operations | Function pointer struct |
| 09 | [Observer](09_observer_pattern.md) | Notifier chains | Registration + callback mechanism |
| 10 | [Command](10_command_pattern.md) | Work queues, block requests | Encapsulated requests in queue |
| 11 | [Iterator](11_iterator_pattern.md) | list_for_each, bus_for_each_dev | Sequential access macros/functions |
| 12 | [State](12_state_pattern.md) | Device FSM, connection states | State table + event dispatch |

---

## How to Compile Examples

Each document contains a complete C program. To compile:

```bash
# For most examples:
gcc -o pattern_name pattern_name.c

# For examples with threading (singleton, command, observer):
gcc -o pattern_name pattern_name.c -pthread

# For strategy pattern example:
gcc -o strategy strategy.c -lm
```

---

## Key C Techniques Used

```
+-------------------------------------------------------------------+
|              C TECHNIQUES FOR DESIGN PATTERNS                     |
+-------------------------------------------------------------------+
|                                                                   |
| 1. Function Pointers                                              |
|    struct ops { int (*read)(void *); };                           |
|    -> Strategy, Observer, Command, State                          |
|                                                                   |
| 2. Struct Embedding                                               |
|    struct child { struct parent base; int extra; };               |
|    -> Composite, Decorator, Bridge                                |
|                                                                   |
| 3. container_of Macro                                             |
|    #define container_of(ptr, type, member) ...                    |
|    -> Iterator, Composite                                         |
|                                                                   |
| 4. Static Variables                                               |
|    static struct singleton *instance = NULL;                      |
|    -> Singleton                                                   |
|                                                                   |
| 5. Registration Tables                                            |
|    static struct factory factories[MAX_TYPES];                    |
|    -> Factory, Observer, State                                    |
|                                                                   |
| 6. Linked Lists                                                   |
|    struct list_head { struct list_head *next, *prev; };           |
|    -> Observer, Iterator, Command                                 |
|                                                                   |
| 7. Opaque Pointers (void *)                                       |
|    void *private_data;                                            |
|    -> All patterns (for type-specific data)                       |
|                                                                   |
+-------------------------------------------------------------------+
```

---

## Linux Kernel Code References

The kernel code examples are from these locations:

- `include/linux/list.h` - List implementation
- `include/linux/notifier.h` - Notifier chains
- `include/linux/workqueue.h` - Work queues
- `include/linux/device.h` - Device model
- `include/linux/fs.h` - File operations
- `kernel/sched/sched.h` - Scheduler classes
- `kernel/fork.c` - Process cloning
- `net/socket.c` - Socket factory
- `crypto/api.c` - Crypto algorithm factory
- `drivers/s390/cio/device.h` - Device state machine

---

## 中文说明

本目录包含12种经典设计模式在Linux内核中的实现分析，以及模仿内核风格的用户空间C代码示例。

### 创建型模式（聚焦对象创建）
1. **单例模式** - 通过静态变量+封装函数确保全局唯一实例
2. **工厂模式** - 统一通过工厂函数创建对象，隐藏实现细节
3. **原型模式** - 通过克隆函数复制已有对象

### 结构型模式（聚焦对象组合）
4. **适配器模式** - 通过中间层转换不兼容接口
5. **装饰器模式** - 嵌套结构体+函数指针动态扩展功能
6. **组合模式** - 将对象组合为树形结构统一处理
7. **桥接模式** - 分离抽象与实现，支持独立扩展

### 行为型模式（聚焦对象交互）
8. **策略模式** - 将算法封装为独立策略，函数指针动态切换
9. **观察者模式** - 定义对象间依赖，状态变化自动通知
10. **命令模式** - 将请求封装为对象，支持队列化和撤销
11. **迭代器模式** - 封装遍历逻辑，提供统一遍历接口
12. **状态模式** - 将状态封装为独立结构体，替代if-else

每个文档都包含ASCII图示、内核代码分析、用户空间完整示例代码及详细注释。

