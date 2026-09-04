# 第二部分：第一支柱 —— struct 就是 Object

> **核心断言**：在 Linux Kernel 的对象模型里，`struct` 不是「一堆字段的袋子」，而是 **Object 本身**——它承载状态、提供身份、约束生命周期，并成为所有关系图的节点。

没有 `class` 关键字，不等于没有对象。内核把对象能力压缩进一件最朴素的语言原语：`struct`。读内核代码时，第一件事不是找虚函数表，而是问：**这个 `struct` 代表哪个对象？谁拥有它？谁负责销毁它？**

---

## 2.1 为什么 struct IS the Object

在 C++ / Java 里，对象通常被理解为：

```
Object = Type + Instance State + Methods (bound somehow)
```

在内核里，等价式更直接：

```
Object = struct instance
```

原因有三层：

1. **状态的唯一容器**  
   进程、文件、inode、socket、device、mm_struct……内核世界里几乎每一个「活着的东西」都是某个 `struct` 的实例。没有平行的 class 运行时对象；`struct` 就是那份状态。

2. **身份由地址给出**  
   内核几乎从不依赖「对象 ID 服务」。一个 `task_struct *` 指向的那块内存，就是该进程对象的身份。指针比较、哈希表键、引用计数目标——全部围绕这块内存的地址。

3. **方法不内嵌在类型语法里，但绑定在对象图上**  
   行为通过 ops 表（下一章）挂在对象或其类型描述符上。于是：

```
+---------------------------+
| struct instance           |
|  - fields (state)         |
|  - *ops / *f_op (behavior)|
|  - *related objects       |
+---------------------------+
            |
            v
     Kernel Object
```

所以正确心智模型不是「C 勉强模拟 OOP」，而是：

> **Kernel Objects = Struct Objects。**  
> ops 决定它能做什么；`container_of` 决定它在继承链上是谁。

---

## 2.2 Object = 状态 + 身份 + 生命周期

一个真正可操作的内核对象，至少具备三要素：

| 要素 | 含义 | 在 struct 中的体现 |
|------|------|-------------------|
| **State（状态）** | 对象此刻知道什么 | 成员字段：计数器、标志位、指针、锁、链表节点 |
| **Identity（身份）** | 如何唯一指称它 | 实例地址；有时辅以 pid、inode number、dev_t 等逻辑键 |
| **Lifecycle（生命周期）** | 何时创建、何时可访问、何时销毁 | 分配函数、引用计数、RCU、kref、释放回调 |

缺任一要素，代码就会「看起来像对象，用起来像裸内存」：

- 只有字段、没有清晰释放路径 → 泄漏或 use-after-free
- 只有指针、没有身份约定 → 误把临时缓冲当成长期对象
- 只有生命周期、状态散落全局变量 → 无法组合、无法并发保护

### 2.2.1 状态：Member Variables as Fields

面向对象课本里的 *member variable*，在内核里就是 **struct field**。命名往往直接暴露语义：

```
pseudocode: task_struct (conceptual, not kernel copy)

struct task_struct {
    int            pid;          /* logical identity helper */
    volatile long  state;        /* scheduling state */
    struct mm_struct *mm;        /* address space object */
    struct files_struct *files;  /* open file table object */
    const struct cred *cred;     /* credentials object */
    struct list_head tasks;      /* intrusive list node */
    /* ... many more fields ... */
};
```

注意三点教学价值：

1. **字段即公开契约的一部分**（对内核内部子系统而言）——改布局可能破坏 ABI / 模块兼容假设。
2. **指针字段经常是「到另一个对象的边」**，不是简单整数。
3. **侵入式节点**（如 `list_head`）把对象直接链进容器，这是内核对象图的典型形态。

### 2.2.2 身份：地址为主，逻辑键为辅

```
+------------------+
| task_struct @P   |  <-- P 就是对象身份
|   pid = 1234     |  <-- 逻辑键，便于用户态 / 查找
|   mm  --------+  |
+---------------|--+
                v
         mm_struct @Q
```

查找路径可以是「按 pid 找 task」，但一旦拿到指针，后续所有操作都以该指针为真相来源。这与托管语言里的对象句柄不同：内核里 **指针即句柄**。

### 2.2.3 生命周期：谁分配、谁引用、谁释放

内核对象很少「函数返回后自然消亡」。典型模式：

1. **Allocator 创建**：`kmalloc` / `kmem_cache_alloc` / 专用构造函数
2. **Publisher 发布**：插入哈希表、链表、idr、RCU 保护的全局结构
3. **Users 持引用**：`get` / `kref_get` / `refcount_inc`
4. **Last put 销毁**：`put` 到零 → 释放回调 → `kfree` / RCU grace period 后释放

**Ownership 问题必须显式回答**，否则对象模型崩塌：

| 问题 | 必须明确 |
|------|----------|
| Who allocates? | 哪个子系统、哪个函数负责 `alloc` |
| Who refcounts? | 哪些路径会 `get/put` |
| Who frees? | 通常是「最后一个 put」，偶尔是明确的 owner 单线程释放 |
| Who may observe? | 是否在 RCU / 锁保护下可读 |

---

## 2.3 封装：没有 `private`，照样隐藏实现

C 没有 `private` / `protected`。内核用 **编译单元边界 + 头文件策略** 做封装。

### 2.3.1 Header Visibility

常见分层：

```
+------------------------+     +------------------------+
| public header          |     | private header / .c    |
|  - opaque pointer API  |     |  - full struct def     |
|  - ops registration    |     |  - helper internals    |
|  - get/put wrappers    |     |  - lock rules          |
+------------------------+     +------------------------+
```

对外只给：

- 不完整类型（forward declaration）
- 操作函数或 ops 注册点
- 文档化的生命周期 API

对内才给完整 `struct` 定义。

### 2.3.2 Forward Declarations（不完整类型）

```
pseudocode

/* public.h */
struct widget;                       /* incomplete type */

struct widget *widget_create(void);
void           widget_destroy(struct widget *w);
int            widget_start(struct widget *w);

/* widget.c */
struct widget {
    int flags;
    spinlock_t lock;
    void *private_data;
};
```

调用方只能持有 `struct widget *`，不能 `w->flags`。这就是 C 版 encapsulation：**类型完整信息是特权，不是语法关键字。**

### 2.3.3 Private Pointers

另一种模式：公共结构里放一个 `void *private` / `void *driver_data`，让框架对象与驱动私有状态解耦：

```
+------------------+
| struct device    |
|  ...             |
|  void *driver_data ----+
+------------------+     |
                         v
                 +------------------+
                 | driver private   |
                 |  regs, dma, ...  |
                 +------------------+
```

框架看见统一的 `device`；驱动看见自己的私有对象。封装边界清晰：**公共对象不膨胀成上帝结构，私有状态不泄漏到框架头文件。**

---

## 2.4 对象关系与 Ownership

内核不是孤立对象的集合，而是 **对象图（object graph）**。每条边都要标明关系语义：

| 关系 | 典型含义 | Ownership 提示 |
|------|----------|----------------|
| 强拥有 | A 销毁则 B 必须销毁 | A 分配并释放 B |
| 共享引用 | 多者可指向 B | B 有 refcount |
| 弱/观察 | 不可阻止 B 释放 | RCU / 世代计数 / 明确 invalidate |
| 嵌入 | B 物理上是 A 的一部分 | 随 A 同生共死（见支柱三） |

读代码时，对每个指针字段问：

> 这是 **owns**、**borrows**、还是 **embeds**？

答错一个，use-after-free 或泄漏几乎必然。

---

## 2.5 案例：`task_struct` 成员心智图（教学简化）

下面是教学用 ASCII 视图，**不是内核源码拷贝**，字段名为概念标签：

```
+----------------------------------------------------------+
| task_struct                                              |
|----------------------------------------------------------|
| pid / tgid                                               |
| state / exit_state                                       |
| stack / usage (refcount-like)                            |
|                                                          |
| mm ----------------------+                               |
| active_mm ---------------|-+                             |
|                          | |                             |
| files -------------------|-+---+                         |
|                          | |   |                         |
| cred / real_cred --------|-+---+-+                       |
|                          | |   | |                       |
| tasks (list_head)        | |   | |                       |
| children / sibling       | |   | |                       |
| ...                      | |   | |                       |
+--------------------------|-|---|-|-----+                 |
                           | |   | |     |                 |
                           v v   v v     v                 |
                      related objects (see next section)   |
+----------------------------------------------------------+
```

要点：

- **大量字段是对象边**，不是标量。
- `list_head` 类字段说明对象参与侵入式容器。
- 引用计数相关字段（概念上的 usage）说明生命周期由共享拥有。

---

## 2.6 对象图：task → mm → files → cred

把「进程」理解成单一结构是不够的；它是一张图：

```
+---------------+
| task_struct   |
|  (process /   |
|   thread obj) |
+-------+-------+
        |
        | owns / shares
        +------------------+------------------+
        |                  |                  |
        v                  v                  v
+---------------+  +---------------+  +---------------+
| mm_struct     |  | files_struct  |  | cred          |
| address space |  | fd table      |  | identity/caps |
+---------------+  +-------+-------+  +---------------+
                           |
                           | indexes
                           v
                   +---------------+
                   | file objects  |
                   | (struct file) |
                   +---------------+
```

关系解读（教学层）：

- `task` 通过 `mm` 连接地址空间对象；线程组可共享同一 `mm`。
- `files_struct` 管理打开文件表；多个 task 可共享（`CLONE_FILES` 一类语义）。
- `cred` 描述凭据；切换权限时常替换指针并正确管理旧对象生命周期。

这张图说明支柱一的真正意义：

> **理解内核 = 理解对象图 + 每条边的 ownership/refcount 规则。**  
> 不是背函数名。

---

## 2.7 心智模型：Kernel Objects = Struct Objects

把第一章的总图收束到支柱一：

```
Everything interesting in the kernel
              |
              v
        a struct instance
              |
     +--------+--------+
     |                 |
     v                 v
  state/identity    edges to other structs
     |
     +---- later: ops table (pillar 2)
     |
     +---- later: embedded base + container_of (pillar 3)
```

日常阅读口诀：

1. 看见 `struct X` → 问「这是什么对象？」
2. 看见字段 → 问「状态还是关系边？」
3. 看见 `get/put/kref` → 问「生命周期协议是什么？」
4. 看见不完整类型 → 问「封装边界在哪里？」

---

## 2.7.1 从「数据结构」升级到「对象」的阅读训练

新手读内核时，习惯把 `struct` 当成算法课里的「记录类型」：字段排列整齐，函数在别处。资深内核读者则自动切换到对象视角：

| 算法课视角 | Kernel OO 视角 |
|------------|----------------|
| 这是一块内存布局 | 这是一个活着的对象实例 |
| 字段是数据 | 字段是状态 + 关系边 + 同步原语 |
| 链表是容器算法 | 侵入式节点说明对象参与哪个集合 |
| 释放就是 `kfree` | 释放是生命周期协议的最后一步 |

训练方法很具体：打开任意子系统，选一个核心 `struct`，只做三件事——

1. 画出它直接指向的其他 `struct`（一度邻居）。  
2. 标出每条边是 owns / shares / borrows / embeds。  
3. 找到创建点与最终销毁点（可能隔着 RCU 宽限期）。

做完这三步，你对「对象」的理解会超过背十个 API 名字。

### 对象不是「大结构体崇拜」

支柱一不是鼓励把一切塞进巨型 `struct`。恰恰相反：

- **过大的对象** 难封装、难并发、难演进。  
- **过碎的对象** 则让 ownership 与锁规则爆炸。

好的内核对象粒度，通常对齐一个可以独立叙述的抽象：一个打开的文件、一个网络套接字、一个可调度实体、一个可探测的设备。粒度对了，ops 表（支柱二）和嵌入继承（支柱三）才有干净的挂载点。

### 同步原语也是对象状态的一部分

锁、等待队列、RCU 头、原子计数器，在对象模型里不是「额外附件」，而是 **状态机合法性的守卫**。没有它们，字段只是竞赛条件下的随机比特。因此阅读对象时，应同时阅读：

```
+---------------------------+
| object                    |
|  payload fields           |
|  lock / seqcount / RCU    |  <-- guards transitions
|  refcount                 |  <-- guards lifetime
+---------------------------+
```

问句升级为：「哪些字段在哪把锁下可读？哪些转换允许在原子上下文发生？」——这才是内核对象的完整语义，而不是只看 C 类型声明。

---

## 2.8 设计原则

1. **一个 struct 一个核心抽象**  
   不要把无关生命周期的状态硬塞进同一对象。

2. **生命周期协议写进 API，而不是靠约定口口相传**  
   `create/destroy` 或 `get/put` 必须成对、可审计。

3. **指针字段必须有 ownership 注释（至少在脑中）**  
   owns / shared / borrowed / RCU-protected。

4. **能不完整类型就不暴露完整布局**  
   减少跨模块布局耦合。

5. **对象图优于全局变量网**  
   把状态挂在对象上，框架才能扩展与并发。

---

## 2.9 最佳实践

- 构造函数里把对象带到「可安全 put」的最小完备状态；失败路径对称回滚。
- 发布到全局可见结构 **之前** 完成初始化（防止半初始化对象被看见）。
- 热路径上的对象查找要固定锁/RCU 规则，并与释放路径匹配。
- 调试时打印对象地址 + 关键逻辑键；地址帮助确认身份，逻辑键帮助对齐用户可见语义。
- 为长生命周期对象建立「不变式清单」：哪些字段创建后只读、哪些允许运行时迁移、哪些必须与另一对象联变。
- 当两个子系统共享同一对象时，把 get/put 规则写进其中一侧的文档注释，并保证另一侧不发明第二套协议。
- 优先让「看起来像对象」的东西真的成为对象：有明确构造、有明确销毁、有明确并发规则；避免散落的全局表 + 临时索引伪装成设计。

### 实践检查单（提交前问自己）

1. 这个 `struct` 的名字能否一句话说清抽象？  
2. 所有出站指针是否都能回答 owns/shares/borrows/embeds？  
3. 失败路径是否会留下「已发布但未完成」的对象？  
4. 最后一个引用消失时，释放函数是否唯一且可到达？  
5. 若明天要挂一张 ops 表，挂点是否自然（对象上或类型描述符上）？

若第 5 问答「不自然」，说明对象边界可能切错了——这正是支柱一与支柱二的接口处。

---

## 2.10 常见陷阱

| 陷阱 | 后果 |
|------|------|
| 把 struct 当临时包，随意栈上返回指针 | 悬空指针 |
| 忘记共享边需要 refcount | UAF / 泄漏 |
| 对外暴露完整内部布局后随意增删字段 | 模块/ABI 悲剧 |
| 混淆「嵌入」与「指针拥有」 | 双重释放或忘释放 |
| 只看字段名不看对象图 | 改一处、炸一片 |
| 用布尔旗标堆砌代替显式状态机 | 非法状态组合难审 |
| 在对象仍被哈希表引用时直接 `kfree` | 并发查找踩飞 |
| 把「逻辑键相等」当成「同一对象」 | 身份错乱、错误回收 |

### 陷阱深挖：身份与查找键

`pid`、`inode number`、`dev_t` 是查找键，不是对象本体。键可以复用，对象地址在其生命周期内才是稳定身份。调试时若只打印键而不打印指针，你会把「新对象碰巧复用旧键」误判为「同一对象还活着」。支柱一要求你始终区分：

> **查找键服务于命名与检索；对象指针服务于身份与生命周期。**

---

## 2.11 本章小结，并连接到另外两大支柱

本章建立第一支柱：

> **`struct` 就是 Object：状态容器、身份载体、生命周期单位、关系图节点。**

但对象若只有状态，系统仍是僵硬的。内核还需要：

- **行为如何挂到对象上、如何在运行时替换** → 第二支柱：**function pointer / ops table**（Method、Virtual Function、Interface）
- **如何在「嵌入的基类视图」与「完整派生对象」之间切换** → 第三支柱：**`container_of`**（Inheritance by Embedding）

三句话串起来：

```
struct 定义「是谁、有什么」
ops    定义「能做什么、怎么分派」
container_of 定义「在继承布局上如何找回完整自我」
```

下一章进入第二支柱：为什么 `file->f_op->read(file, ...)` 就是内核版的 `file.read()`。
