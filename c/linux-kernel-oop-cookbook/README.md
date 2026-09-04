# Linux Kernel Object Model Cookbook

> **Linux Kernel 用纯 C 实现面向对象设计的完整心智模型教程**

本 Cookbook 帮助熟悉 C 语言、但没有 Kernel 背景的读者真正理解：

> **Linux Kernel 如何仅依靠 C 语言，实现接近 C++ 面向对象（Object-Oriented Design）的能力。**

不需要 `class`、不需要 `virtual`、不需要语言级 inheritance，却依然实现：

- 封装（Encapsulation）
- 继承（Inheritance）
- 多态（Polymorphism）
- 抽象（Abstraction）
- 动态绑定（Dynamic Dispatch）

---

## 核心论点（一句话）

Linux Kernel 的 Object Model 建立在三大支柱之上：

1. **`struct`** —— 表示对象（Object），承载状态、生命周期与对象关系
2. **function pointer（ops table）** —— 表示行为（Method），实现接口、多态与动态绑定
3. **`container_of`（配合嵌入式 struct）** —— 表示继承（Inheritance by Embedding），完成基类与派生类之间的类型恢复

```
Kernel World
     |
     v
  Everything is a struct
     |
     +-- ops table --------> Dynamic Dispatch
     |
     +-- embedded struct
              |
              v
         container_of -----> Inheritance
              |
              v
         Runtime Object Model
```

---

## 阅读路径

| 顺序 | 章节 | 内容 |
|------|------|------|
| 1 | [01-why-oo-in-c.md](01-why-oo-in-c.md) | 为什么 Kernel 是 OOD 而非 OOP Language |
| 2 | [02-pillar-1-struct.md](02-pillar-1-struct.md) | 支柱一：struct 即 Object |
| 3 | [03-pillar-2-ops.md](03-pillar-2-ops.md) | 支柱二：函数指针即 Method / Virtual |
| 4 | [04-pillar-3-container-of.md](04-pillar-3-container-of.md) | 支柱三：container_of 即 Inheritance |
| 5 | [05-kernel-object-model.md](05-kernel-object-model.md) | Kernel Object Model 总体架构 |
| 6 | [06-cases-vfs.md](06-cases-vfs.md) | 真实案例：VFS 与内存 |
| 7 | [07-cases-net.md](07-cases-net.md) | 真实案例：网络栈 |
| 8 | [08-cases-driver-model.md](08-cases-driver-model.md) | 真实案例：驱动模型 |
| 9 | [09-cases-core.md](09-cases-core.md) | 真实案例：调度 / 中断 / LSM |
| 10 | [10-userspace-examples-1-5.md](10-userspace-examples-1-5.md) | 用户空间完整示例 1–5 |
| 11 | [11-userspace-examples-6-10.md](11-userspace-examples-6-10.md) | 用户空间完整示例 6–10 |
| 12 | [12-container-of-deep-dive.md](12-container-of-deep-dive.md) | container_of 彻底推导 |
| 13 | [13-kernel-vs-cpp-java-rust.md](13-kernel-vs-cpp-java-rust.md) | Kernel vs C++ / Java / Rust / Go |
| 14 | [14-final-mental-model.md](14-final-mental-model.md) | 最终心智模型与阅读 Checklist |
| 15 | [15-summary.md](15-summary.md) | 总结、大图与常见误区 |

---

## 可编译示例

所有用户空间示例位于 [`examples/`](examples/)：

```bash
cd examples
make          # 编译全部
make run      # 编译并运行全部，打印真实输出
make clean    # 清理
```

每个示例都使用同一个 [`kobj.h`](examples/kobj.h) 中的 `container_of`，证明：**同一个宏支撑十种完全不同的对象场景**。

| 编号 | 文件 | 主题 |
|------|------|------|
| 01 | `01_animal.c` | Animal / Dog / Cat 继承与虚函数 |
| 02 | `02_widget.c` | GUI Widget：Button / Label / Window |
| 03 | `03_shape.c` | Shape：Circle / Rectangle / Triangle |
| 04 | `04_plugin.c` | Plugin Framework |
| 05 | `05_protocol.c` | 网络协议：TCP / UDP / QUIC |
| 06 | `06_database.c` | 数据库驱动：MySQL / SQLite / PostgreSQL |
| 07 | `07_filesystem.c` | 文件系统：EXT4 / XFS / FAT |
| 08 | `08_driver.c` | 设备驱动框架：PCI / USB / Platform |
| 09 | `09_mq.c` | 消息队列：Producer / Consumer / Dispatcher |
| 10 | `10_game_engine.c` | 游戏引擎：Entity / NPC / Monster / Player |

---

## 心智模型速览

看到任意 Kernel Subsystem，第一反应应该是这 9 个问题（详见 [14-final-mental-model.md](14-final-mental-model.md)）：

1. Object 在哪里？
2. Base Struct 在哪里？
3. Derived Struct 在哪里？
4. ops 在哪里？
5. container_of 在哪里？
6. 谁拥有谁？
7. 生命周期是谁管理？
8. 动态绑定发生在哪里？
9. 对象之间如何协作？

---

## 设计原则（贯穿全书）

- **Everything is a struct** —— 对象就是内存中的结构体实例
- **ops table is the vtable** —— 行为表就是手写的虚函数表
- **Embedding is inheritance** —— 嵌入基类成员 = is-a
- **container_of recovers the derived type** —— 从基类指针找回完整对象
- **Framework owns the protocol, driver owns the policy** —— 框架定接口，实现填 ops

---

## 适用读者

- 熟悉 C，想理解 Kernel 设计模式
- 熟悉 C++ OOP，想看纯 C 如何等价实现
- 正在读 VFS / Driver Model / Network Stack 源码，需要统一思维框架

---

## 许可与说明

本教程是教学材料。案例使用伪代码描述设计意图，**不复制 Linux Kernel 源码**。用户空间示例均为自包含 C11 代码，可直接 `gcc` 编译。
