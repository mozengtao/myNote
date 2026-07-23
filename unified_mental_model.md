# 编程语言的统一心智模型（Unified Mental Model）

> **学习一门语言，不应首先关注语法，而应首先理解它"最核心的驱动力（Driving Principle）"。**
>
> 每门语言都有一个第一性原理（First Principle），它决定了程序员思考问题、组织代码以及设计系统的方式。

- [language mental models](language_mental_models/README.md)

---

## 总览

| 语言 | 核心驱动力 (Driving Principle) | 一句话理解 | 关键词 |
|------|-------------------------------|-----------|---------|
| **Shell** | 数据流（Data Flow） | 程序就是文本流的加工管道 | Pipe、Filter、Stream |
| **Python** | 协议（Protocol） | 对象能做什么，由它支持哪些协议决定 | Duck Typing、Magic Method、Iterable |
| **Go** | 组合（Composition） | 使用小接口、小组件拼装系统 | Interface、Embedding、Composition |
| **Rust** | 所有权（Ownership） | 数据必须有明确的拥有者 | Move、Borrow、Lifetime |
| **C** | 内存（Memory） | 程序就是对内存和地址的操作 | Pointer、Address、Buffer |
| **C++** | 生命周期（Lifetime） | 资源跟随对象生命周期自动管理 | RAII、Scope、Destructor |
| **Java** | 职责（Responsibility） | 系统由多个对象协同完成各自职责 | Class、Interface、DI、IOC |
| **JavaScript** | 事件（Event） | 程序由事件驱动，而不是顺序执行 | Event Loop、Promise、Async/Await |
| **Haskell** | 变换（Transformation） | 程序就是纯函数之间的组合 | Pure Function、Composition |
| **SQL** | 声明（Declaration） | 描述"要什么"，而不是"怎么做" | Set、Projection、Selection |

---

# 每种语言真正关心什么？

## Shell —— 数据如何流动？

```
Input
   │
   ▼
Filter
   │
   ▼
Transform
   │
   ▼
Output
```

Shell 程序员思考的是：

> **数据从哪里来？经过哪些处理？最终流向哪里？**

---

## Python —— 对象支持哪些行为？

```
Object
    │
    ▼
Protocol
    │
    ▼
Behavior
```

Python 程序员思考的是：

> **对象是否实现了某个协议（Protocol）？**

而不是：

> 它属于哪个类？

---

## Go —— 如何组合组件？

```
Logger
Config
Database
Cache
      │
      ▼
   Application
```

Go 程序员思考的是：

> **如何把多个职责单一的小组件组合成一个完整系统？**

---

## Rust —— 数据归谁所有？

```
Data
   │
   ▼
Owner
   │
   ├── Borrow
   └── Move
```

Rust 程序员思考的是：

> **谁拥有这份数据？生命周期是否安全？**

---

## C —— 数据存放在哪里？

```
Memory
   │
   ▼
Pointer
   │
   ▼
Bytes
```

C 程序员思考的是：

> **地址是什么？数据在哪里？**

---

## C++ —— 生命周期什么时候结束？

```
Construct
      │
      ▼
 Use Resource
      │
      ▼
 Destruct
```

C++ 程序员思考的是：

> **资源是否绑定到对象生命周期？**

---

## Java —— 谁负责这件事情？

```
Controller
      │
      ▼
Service
      │
      ▼
Repository
      │
      ▼
Database
```

Java 程序员思考的是：

> **职责如何划分？对象如何协作？**

---

## JavaScript —— 谁会触发下一步？

```
Event
   │
   ▼
Callback
   │
   ▼
Event Loop
```

JavaScript 程序员思考的是：

> **什么时候收到事件？事件回来之后执行什么？**

---

## Haskell —— 如何组合函数？

```
Input
   │
   ▼
filter
   │
   ▼
map
   │
   ▼
reduce
```

Haskell 程序员思考的是：

> **如何通过函数组合完成数据变换？**

---

## SQL —— 我想得到什么结果？

```
Table
   │
   ▼
Selection
   │
   ▼
Projection
   │
   ▼
Result Set
```

SQL 程序员思考的是：

> **目标集合是什么？**

而不是：

> 如何遍历每一行？

---

# 黄金法则（Golden Rules）

| 语言 | 黄金法则 |
|------|----------|
| Shell | 不要想着调用命令，而要想着建立数据流。 |
| Python | 不要想着对象属于什么类，而要想着它支持什么协议。 |
| Go | 不要想着继承，而要想着组合。 |
| Rust | 不要想着复制数据，而要想着所有权如何流转。 |
| C | 不要想着对象，而要想着内存布局。 |
| C++ | 不要想着什么时候释放资源，而要想着生命周期。 |
| Java | 不要想着实现细节，而要想着职责划分。 |
| JavaScript | 不要想着顺序执行，而要想着事件驱动。 |
| Haskell | 不要想着修改变量，而要想着构造新的值。 |
| SQL | 不要想着遍历数据，而要想着描述目标集合。 |

---

# 最终统一视角（The Unified View）

如果把所有语言进一步抽象，可以发现：

```
                编程语言
                     │
      ┌──────────────┼──────────────┐
      │              │              │
   数据(Data)     对象(Object)    资源(Resource)
      │              │              │
      ▼              ▼              ▼
 Shell          Python/Java      C/C++/Rust
      │              │              │
      ▼              ▼              ▼
 Data Flow      Protocol        Ownership/Lifetime


                    组合(Composition)
                           │
                           ▼
                          Go


                   事件(Event Driven)
                           │
                           ▼
                     JavaScript


                  声明(Declaration)
                           │
                           ▼
                          SQL


                 数学变换(Transformation)
                           │
                           ▼
                        Haskell
```

---

# 第一性原则（First Principle）

学习任何一门语言，都可以先问自己一个问题：

> **这门语言最希望程序员关注什么？**

一旦抓住这个"驱动力"，其语法、标准库和最佳实践都会围绕这一核心展开，自然形成统一的心智模型，而不是零散地记忆各种语法规则。