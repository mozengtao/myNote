# 第一部分：Dependency Injection 的 Mental Model

> 本文档是《Kernel Dependency Injection Mental Model》系列的第 0 篇，建立贯穿全系列的整体框架。
> 后续每一篇（`01-*.md` ~ `08-*.md`）讲的都是本文这个"骨架"的一个具体实例化。

---

## 1. 什么是 Dependency Injection

先剥掉设计模式的术语外壳，只看本质：

> 一个模块要完成工作，需要用到"某种能力"（可能是一个函数、一个对象、一份配置）。
> **这份能力从哪里来** —— 是模块自己 `new`/写死出来的，还是从外部"塞"进来的？

- 如果是模块自己决定、自己构造 —— 这是**紧耦合**（tight coupling）。
- 如果是外部构造好，通过参数、字段、注册表"喂"给模块 —— 这就是 **Dependency Injection**。

DI 的核心不是某个框架、某个注解，而是一条设计公约：

```
调用者（Core / Framework）不实现细节，
细节由被调用者（Implementation）提供，
双方通过一份"事先约定的接口"（Contract）连接。
```

在 Java/Spring 里，这条公约靠 **IoC 容器 + 反射 + 注解**在运行期自动完成"装配"。
在 Linux Kernel（纯 C，无 GC、无反射、无异常）里，这条公约靠：

- 函数指针（function pointer）
- 结构体字段（struct field）
- 注册表（linked list / array / hash table）
- 链接器（linker section）

四种朴素机制手工拼出来。**没有魔法，全部显式、全部可以在源码里读到。**

---

## 2. C 语言里的 DI 与 Java Spring 的本质区别

| 维度 | Java / Spring | C / Linux Kernel |
|---|---|---|
| 依赖描述方式 | 注解 `@Autowired`、接口类型 | 函数指针类型、结构体字段类型 |
| 装配时机 | 运行期，容器扫描 + 反射 | **编译期决定"形状"，运行期只是"填指针"** |
| 装配主体 | IoC Container（Spring `ApplicationContext`） | 没有容器；由调用方模块自己在初始化时"注册" |
| 多态实现 | 接口 + 动态代理/虚方法表（JVM 自动生成） | 手写的"ops table"（`struct xxx_operations`），本质是手工 vtable |
| 生命周期管理 | 容器管理 Bean 的创建/销毁（Scope） | 手动引用计数（`kref`）、手动 `module_get/put`、手动 `RCU` |
| 错误处理 | 异常（Exception） | 返回值 + `NULL` 检查 + `WARN_ON`/`BUG_ON` |
| 性能代价 | 反射、代理、容器扫描 —— 有明显运行期开销 | **零运行期开销**：一次指针间接跳转，等价于手写虚函数调用 |
| 可发现性 | 容器里可以"反射"出所有 Bean | 只能靠约定的注册路径（如 `__initcall` section）静态发现 |

一句话总结：

> **Spring 的 DI 是"运行期用反射自动装配”；Kernel 的 DI 是"编译期约定形状，运行期手工插指针”。**
> 两者解决的是同一个问题（解耦"谁用"和"谁实现"），但 Kernel 版本完全不依赖任何运行期基础设施。

---

## 3. 为什么 Linux Kernel 到处都是 DI

Kernel 面临的场景是所有 C 系统里最极端的"多态需求 + 零容忍开销"组合：

1. **一个内核，无数种硬件**：同一份 `vfs_read()` 要同时驱动 ext4、NFS、/proc、pipe、socket——不可能在 VFS 里写
   `if (fs == ext4) ... else if (fs == nfs) ...`，这样每加一种文件系统都要改核心代码，属于"开闭原则"的反面。
2. **模块可以在运行期插入/卸载**：`insmod`/`rmmod` 时新驱动要能"接入"已经运行的内核，核心代码不能重新编译。
3. **子系统边界必须稳定**：VFS 团队和 ext4 团队几乎不用互相看对方代码，只需要遵守 `file_operations` 这份"契约"。
4. **性能极限场景**：中断处理、调度器热路径、网络收包路径——每一次分派都是纳秒级预算，不能有反射、不能有异常、不能有虚拟机开销。

DI 在这里不是"优雅的设计模式"，而是**唯一在 C 语言、无运行时、无 GC 条件下能同时满足"解耦 + 可扩展 + 零成本"的手段**。

---

## 4. 为什么 Kernel 不需要 IoC Container

IoC 容器要解决的问题本质上是："**如何在运行期自动发现并组装大量对象及其依赖关系图**"。
Kernel 不需要这个，原因如下：

1. **依赖关系在编译期就是已知、静态、树状的**：一个 `struct pci_driver` 要注入到哪个 bus，代码里写得清清楚楚
   （`pci_register_driver()`），不需要"扫描 classpath"去发现。
2. **没有"循环依赖"这种需要自动解析的复杂图**：Kernel 的依赖关系几乎都是单向的：
   `driver -> bus -> core`，`file_operations -> VFS`，从来不会出现"A 依赖 B，B 又反过来依赖 A 的具体实现"这种需要容器做拓扑排序的场景。
3. **注册即装配，没有"配置 vs 代码"分离的需求**：Spring 需要 XML/注解把"装配关系"和"业务代码"解耦，是因为 Java 应用要支持
   多环境部署配置切换；Kernel 的"配置"就是 `.config` + 编译，装配关系写在 C 代码里天经地义。
4. **反射在 C 里不存在**：没有反射就没有"运行期自动发现类型并实例化"的能力，Kernel 必须靠**开发者手写注册调用**
   （`module_init()`、`register_xxx()`）来替代容器的"自动扫描"。

所以 Kernel 版本的 DI 可以理解为："**手工挡的 IoC**"——把 Spring 容器在运行期做的事情，拆解成三个动作，
分别下放给：**编译器**（决定形状/类型安全）、**链接器**（决定谁参与初始化，见 `06-initcall.md`）、
**开发者代码**（决定何时注册/注销）。三者合力，替代了一个运行期容器。

---

## 5. Kernel 的 DI 如何做到零运行时成本（Zero-cost Abstraction）

关键在于：**Kernel 的"多态"从来不是"运行期决定类型再动态分派"，而是"编译期已确定形状，运行期只做一次指针跳转"。**

```
Java 虚方法调用：
  obj.read()
      |
      v
  JVM 查虚方法表（vtable，运行期由 JVM 维护）
      |
      v
  可能触发 JIT 反优化 / 内联失败 / 边界检查
      |
      v
  实际方法体

C 函数指针调用（Kernel ops table）：
  file->f_op->read(file, buf, count, pos)
      |
      v
  一次内存读（取 f_op 指针）
      |
      v
  一次内存读（取 read 字段）
      |
      v
  一次 CALL（indirect call，CPU 分支预测器可学习）
      |
      v
  实际函数体
```

代价对比：

| 开销来源 | Java/Spring | Kernel ops table |
|---|---|---|
| 类型信息查找 | 运行期反射/vtable 查找 | 无——编译期已固定结构体布局 |
| 内存分配 | 容器可能为代理对象额外分配 | 无额外分配，`ops` 通常是 `static const` 只读数据，多个实例共享 |
| 分支预测 | JIT 相关，不确定 | `f_op` 通常在对象生命周期内不变，CPU indirect branch predictor 命中率高 |
| Cache 行为 | 对象头、代理对象增加 cache miss | `struct xxx_operations` 常声明为 `const`，被大量实例共享，天然 cache 友好 |
| 抽象层数 | 容器 -> 代理 -> 接口 -> 实现，多层 | 调用者 -> 一次指针跳转 -> 实现，一层 |

这正是 C++/Rust 意义上的 **zero-cost abstraction**：抽象（"我不关心具体是谁实现的"）没有引入运行期额外开销，
所有开销都被"编译期确定内存布局"和"函数指针取代虚函数表"提前消化掉了。

---

## 6. 整体架构图

```
                        Framework / Core
                     (VFS, 调度器, 总线, 通知链...)

                               |
                               |  inject dependency
                               |  (在初始化/注册时机注入)
                               v
              +----------------+-----------------+
              |                                   |
          function                             object
       (单个能力点)                         (一组相关能力)
              |                                   |
          callback                          ops table
     (1 个函数指针字段)               (N 个函数指针组成的结构体)
              |                                   |
              +----------------+-----------------+
                               |
                               |  runtime dispatch
                               |  (core 通过指针调用，不知道具体是谁)
                               v
                        implementation
                (driver / filesystem / algorithm / policy...)
```

这张图是全系列的总纲，后面每一个具体 DI 手法都是它的"特化"：

- `callback` 分支 -> **01 Function Pointer Callback**
- `ops table` 分支 -> **02 Operations Table**
- "谁在什么时候把指针塞进去" -> **03 Context Injection**、**04 Driver Registration**
- "多个实现同时存在、广播通知" -> **05 Notifier Chain**
- "谁负责触发注册这个动作本身" -> **06 Initcall**
- "任务级别的注入 + 异步执行" -> **07 Workqueue**
- "在关键路径上插入可插拔的强制策略" -> **08 Security Hooks (LSM)**

---

## 7. 核心公约（贯穿全系列）

Kernel 的 DI 本质都是同一句话的不同实现：

> **"不要自己决定依赖，而是由外部注入。"**
>
> Core 只依赖"契约"（函数指针的类型签名、结构体的字段布局），不依赖任何具体实现；
> 具体实现在**它自己选择的时机**（模块加载、设备探测、子系统初始化）把自己"挂”上去；
> Core 到了该调用的时候，通过契约里约定好的指针，**盲调**——不知道也不关心背后是谁。

用一张状态图总结"谁在何时做什么"：

```
[编译期]                          [注册期]                         [运行期]
  |                                  |                                 |
  定义契约                       实现填充契约                     Core 盲调契约
  (struct xxx_operations           (static const struct               (obj->ops->fn(...))
   的字段/函数指针签名)              xxx_operations my_ops = {...})       |
  |                                  |                                 v
  Core 只 #include 头文件         调用 register_xxx(&my_ops)        走到具体实现
  从不 #include 具体实现              |                                 |
                                     v                                 v
                              Core 的注册表/链表/                  返回值/副作用
                              linker section 里多了一项           回到 Core 继续往下走
```

带着这张图，进入后面每一个具体手法的详细拆解。
