# 第一部分：为什么 Linux Kernel 是 "Object-Oriented Design in C"

## 1.1 先澄清一个被反复混淆的概念

很多人会说：

> Linux Kernel is Object-Oriented C.

这句话本身没错，但若理解成「Kernel 是用面向对象语言写的」，那就错了。

必须把两个词拆开：

| 概念 | 含义 | Kernel 是否具备 |
|------|------|-----------------|
| **OOP Language** | 语言本身提供 class、virtual、inheritance、RTTI 等语法与运行时 | **否** |
| **OOD (Object-Oriented Design)** | 用对象、接口、多态、封装来组织系统结构 | **是** |

Linux Kernel 是：

- **语言层面**：纯 C（历史上是 C89/GNU C，现代是 C11 + GNU 扩展）
- **设计层面**：高度对象化的子系统架构

这不是文字游戏。它决定了你读源码时的第一反应：

- 错误反应：「这里怎么没有 class？」
- 正确反应：「这里的 Object / Interface / Dispatch 分别藏在哪个 struct 里？」

---

## 1.2 为什么 Kernel 不是 OOP Language

Kernel 刻意不使用 C++，原因不是「C++ 不好」，而是 **内核约束与 C++ 的语言语义冲突**。

### 1.2.1 ABI 必须长期稳定、可预测

内核模块、驱动、以及用户可见的数据结构布局，要求：

- 结构体布局可手工控制
- 没有编译器隐式插入的 vptr
- 没有 name mangling 带来的符号不稳定
- 没有异常展开表、typeinfo、动态初始化依赖

C++ 的对象模型会在你看不见的地方插入：

```
+------------------+
| vptr (hidden)    |  <-- 编译器插入
+------------------+
| your fields...   |
+------------------+
```

内核作者需要 **每一字节都在自己掌控之中**。

### 1.2.2 隐式代码是内核的敌人

C++ 构造函数、析构函数、运算符重载、异常、模板实例化，都会产生「看不见的代码路径」。

内核要求：

- 失败路径显式（返回错误码，而不是 throw）
- 内存分配策略显式（GFP_KERNEL / GFP_ATOMIC）
- 锁、睡眠、中断上下文显式

Linus Torvalds 多次强调的核心不是「讨厌 C++」，而是：

> 内核里不允许有大量不可见的控制流。

### 1.2.3 运行时环境极度受限

内核早期启动、中断上下文、NMI、原子上下文里：

- 不能随便分配内存
- 不能睡眠
- 不能依赖复杂的全局构造顺序

C++ 的动态初始化、异常、部分标准库语义，与这些约束不兼容。

因此：

> Kernel 选择 C，不是因为「不会 OOP」，而是因为要用 **最可控的工具** 实现 **最彻底的 OOD**。

---

## 1.3 为什么 Kernel 却是 OOD

OOD 的本质不是语法，而是这五个能力：

1. **封装**：对外暴露接口，隐藏实现细节
2. **抽象**：用稳定接口描述「能做什么」，不绑定「怎么做」
3. **继承**：共享公共行为与状态，特化差异
4. **多态**：同一调用点，运行时落到不同实现
5. **动态绑定**：行为在运行期决定，而不是编译期写死

Kernel 用三样东西精确对应这五个能力：

```
+------------------+     +------------------+     +------------------+
| struct           |     | function pointer |     | container_of     |
| (Object/State)   |     | (Method/VTable)  |     | (Inheritance)    |
+------------------+     +------------------+     +------------------+
         |                        |                        |
         +------------------------+------------------------+
                                  |
                                  v
                     Object-Oriented Design in C
```

举例：

- `struct file` + `file_operations` → 文件对象的多态读写
- `struct device` 嵌入在 `pci_dev` → 设备继承
- `bus_type.match/probe` → 总线框架的策略分派
- `sched_class.enqueue_task` → 调度策略的可替换实现

所以人们说 Kernel is Object-Oriented C，真正含义是：

> **Kernel 用 C 手工实现了一套完整、显式、可审计的对象模型。**

---

## 1.4 为什么几乎所有 Subsystem 都采用 OO Design

因为内核面对的是 **无限多样的硬件与策略，却要提供统一框架**。

### 1.4.1 硬件多样性

- 网卡：Intel / Broadcom / Mellanox …
- 存储：NVMe / SCSI / virtio …
- 总线：PCI / USB / I2C / SPI / Platform …

若每个驱动都直接改核心代码，内核会爆炸。

OO Design 给出的解法是：

```
Framework (stable interface)
     |
     |  ops table
     v
Driver A / Driver B / Driver C  (pluggable implementations)
```

### 1.4.2 策略可替换

调度器要支持 CFS / RT / DL；文件系统要支持 ext4 / xfs / btrfs；安全模块要支持 SELinux / AppArmor。

这些都是典型的 **Strategy Pattern**：

- 框架固定调用点
- 实现填入不同 ops
- 运行时选择策略对象

### 1.4.3 生命周期统一管理

设备热插拔、模块卸载、文件系统卸载、进程退出，都需要：

- 引用计数
- 父子关系
- release 回调

这正是对象生命周期管理。`kobject` / `kref` 体系就是为此而生。

结论：

> Kernel 不是「偶尔用一下 OOP 味道的写法」，而是 **系统性地把整个操作系统建成对象图**。

---

## 1.5 为什么 Linus 从不说 Kernel 是 C++，却一直采用对象模型

因为 Linus 关心的是：

1. **可读性**：行为在函数指针表里一目了然
2. **可控性**：没有隐式构造/析构/异常
3. **可扩展性**：新驱动只填 ops，不改框架
4. **可调试性**：调用链显式，`file->f_op->read` 比虚函数跳转更易追踪（在内核文化里）

对象模型是目标；C++ 只是实现对象模型的一种语言手段。  
Kernel 选择了另一种手段：**显式的 C 对象协议**。

这带来一个反直觉但极其重要的事实：

> 很多「用了 C++」的项目，对象边界反而更模糊；  
> Kernel「没用 C++」，对象边界却异常清晰。

原因就在于：Kernel 把对象模型写成了 **强制的架构约定**，而不是可选的编码风格。

---

## 1.6 Kernel 世界最重要的一句话：Everything is a struct

在用户空间 C++ 世界，你习惯说：

> Everything is an object.

在 Kernel 世界，等价且更精确的说法是：

> **Everything is a struct.**

因为在 C 里，对象的唯一载体就是结构体实例。

### 1.6.1 为什么核心实体全部是 struct

逐个看：

**`task_struct`**  
进程/线程控制块。状态（state）、地址空间（mm）、打开文件（files）、凭证（cred）、信号（signal）全部是成员。它是调度与进程管理的中心对象。

**`inode`**  
文件系统中「文件实体」的对象。权限、大小、时间戳、以及 `i_op` / `i_fop` 行为表。

**`file`**  
一次打开实例。同一个 inode 可有多个 file。`f_op` 决定这次打开如何读写。

**`socket`**  
网络通信端点对象。绑定协议族 ops，承载连接状态。

**`device`**  
驱动模型的通用设备基类对象。几乎所有具体设备都嵌入它。

**`net_device`**  
网络设备对象。队列、特性、`netdev_ops`、统计信息。

**`kobject`**  
内核对象基础设施。名字、父对象、kset、sysfs、release。

**`dentry`**  
目录项缓存对象。连接名字与 inode，是路径查找的核心。

**`super_block`**  
一次挂载的文件系统实例对象。`s_op` 定义该 FS 的超级块级行为。

**`vm_area_struct`**  
进程地址空间中的一段映射对象。`vm_ops` 定义缺页、close 等行为。

**`mm_struct`**  
进程的内存描述对象。管理 VMA 集合、页表、RSS 等。

它们全部是 struct，不是巧合，而是必然：

```
Kernel World
     |
     v
  Everything
     |
     v
   struct          <--- 对象的唯一物理形态
     |
     +-- fields     (state)
     +-- ops *      (behavior)
     +-- embedded   (inheritance)
     +-- pointers   (relations / ownership)
```

### 1.6.2 第一层心智模型

请把下面这张图刻进脑子：

```
+--------------------------------------------------+
|                 Kernel Universe                  |
|                                                  |
|   task   inode   file   socket   device   ...    |
|     |      |      |       |        |             |
|     +------+------+-------+--------+             |
|                      |                           |
|                      v                           |
|                   struct                         |
|                      |                           |
|         +------------+------------+              |
|         |            |            |              |
|         v            v            v              |
|      状态字段     ops表指针    嵌入的基类成员     |
+--------------------------------------------------+
```

记住：

- 看见一个子系统，先找 **主对象 struct**
- 再找它的 **ops**
- 再找它是否 **嵌入了某个基类 struct**
- 再找 `container_of` 如何在回调里找回派生对象

这就是 Kernel 阅读的起点。

---

## 1.7 设计原则：语言不是对象，约定才是对象

从本章导出三条原则：

1. **OOD 不依赖 OOP 语法**  
   没有 class 也能有对象模型；关键是边界是否清晰。

2. **显式优于隐式**  
   Kernel 把 vtable、继承、析构都写成看得见的 C 代码。

3. **struct 是对象协议的载体**  
   学会读 struct，就等于学会读 Kernel 的对象图。

---

## 1.8 常见误区

- **误区 A**：觉得 Kernel「没用 OOP」。  
  纠正：没用 OOP *Language*，但全面使用 OOD。

- **误区 B**：觉得函数指针只是 callback。  
  纠正：在 Kernel 里，函数指针常常是 *virtual method*。

- **误区 C**：觉得嵌入 struct 只是「组合」。  
  纠正：当公共 API 以基类指针操作、再用 `container_of` 恢复派生类型时，它就是 inheritance。

- **误区 D**：一上来就抠源码细节。  
  纠正：先建立 Object / ops / container_of 心智模型，再下潜细节。

---

## 1.9 本章小结

- Kernel 不是 OOP Language，但是成熟的 OOD 系统。
- 拒绝 C++ 是为了可控、显式、ABI 稳定。
- 几乎所有子系统都用对象模型解决多样性与可扩展性。
- **Everything is a struct** 是第一层心智模型。

下一章开始拆解三大支柱的第一根：**为什么 struct 就是 Object**。
