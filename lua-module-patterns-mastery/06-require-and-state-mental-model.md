# 06 · `require()` 运行时模型 + Module / Object / Framework / Plugin State 横向对比

> 前置阅读：[00-mental-model-overview.md](00-mental-model-overview.md) 以及五个 Pattern 的 `00-why-and-architecture.md`。本文把每个 Pattern 文档里反复出现的 "见 06" 的 foreshadow 一次性收口：先建立 `require()` 的正确运行时模型，再把 Module State / Object State / Framework State / Plugin State 四种 State 放在同一张表里对比。

## 1. `require()` 的 Mental Model

```text
local X = require("x")
```

```text
require("x")
     |
     v
Module Loader
     |
     +-- 1. 查 package.loaded["x"]，命中就直接返回缓存值，不重新执行 x.lua
     |
     +-- 2. 未命中：按 package.path 找到 x.lua 文件
     |
     v
execute x.lua （作为一个函数体执行一次，从头到尾）
     |
     v
x.lua 内部 `return M` 的返回值
     |
     v
写入 package.loaded["x"] = M   （缓存下来，供下一次 require("x") 直接命中）
     |
     v
X   （这次 require() 调用得到的返回值，其实就是 package.loaded["x"]）
```

### 逐条解释

- **`require()` 与 Module 的关系**：`require("x")` 不是"把 x.lua 的内容复制粘贴到当前文件"（那是 `dofile`/`#include` 式的语义），而是"找到、执行（如果还没执行过）、缓存、返回 x.lua 的返回值"。Module 这个概念在 Lua 里没有专门的语言级关键字，`require()` + "文件返回一个值"就是 Lua 对"Module"的完整定义。
- **为什么通常 `return M`**：一个 Lua 文件本质上就是一个函数体（可以有参数 `...`，可以有返回值）。`require()` 只关心这个函数体的**返回值**，所以"这个文件对外暴露什么"完全由 `return` 语句决定——不写 `return`，`require()` 拿到的就是 `true`（Lua 5.2+ 的兼容行为）或 `nil`，调用者什么都拿不到。`return M` 把一个 table 作为这个"函数"的唯一输出，这个 table 就成了 Module 的全部公开面。
- **为什么 Lua Module 通常用 Table 作为载体**：`return` 只能返回值（不限于一个，但通常只用一个），而 Lua 里能同时装下"多个函数 + 多个常量"又能被一次赋值捕获的唯一原生结构就是 table。这也是为什么 [00-mental-model-overview.md](00-mental-model-overview.md) 第 1 节要强调"table 天然给出 namespace，又不需要发明新的语言概念"。
- **Module 是否每次 `require()` 都重新执行**：不会。`require()` 内部维护一个全局的 `package.loaded` 表，`package.loaded["x"]` 第一次被填充之后，后续任意位置、任意次数的 `require("x")` 都直接返回这个缓存值，x.lua 顶层代码只会被执行一次（除非显式 `package.loaded["x"] = nil` 之后再 `require`，这属于刻意的"重新加载"操作，正常业务代码不会这么做）。
- **Module Cache 的基本 Mental Model**：把 `package.loaded` 想象成一个"进程级单例表"：key 是 module 名字，value 是这个 module 文件执行一次之后的返回值。整个进程生命周期内，任何地方拿到的 `require("x")` 返回值都是**同一个 table 的同一个引用**，不是各自的副本。
- **Stateful Module 为什么能形成共享 State**：既然所有调用者拿到的都是"同一个引用"，如果这个 table（或它闭包捕获的 `local` 变量）里有可变字段，A 处的修改会立刻被 B 处看到——这正是 Pattern 2（Private+Public）例子 `user_service.lua` 里 `users` 表能在多次 `create()`/`find()` 调用之间"记住"数据的根本原因：不是 `user_service.lua` 有什么特殊机制，只是它是**同一个被缓存的 table**，而 table 本身在 Lua 里永远是引用语义。

### 一张图总结 Module Cache 的效果

```text
file_a.lua: local us = require("user_service")   \
file_b.lua: local us = require("user_service")    }---> 同一个 us 引用
file_c.lua: local us = require("user_service")   /       (package.loaded["user_service"])

us.create(...)  在 file_a.lua 里插入一条记录
us.find(...)    在 file_b.lua 里能读到这条记录   <-- 因为是同一份 users 表
```

## 2. 四种 State 的横向对比

五个 Pattern 各自的 `00-why-and-architecture.md` 第 7 节（State）都只讲了"自己这一种"State，并 foreshadow 了这张总表——现在统一给出：

| State 类型 | Owner（归属） | 生命周期 | 是否共享 |
|---|---|---|---|
| **Module State** | Module（文件顶层的 `local` 变量，或挂在返回的 `M` 上） | 从 `require()` 第一次执行这个文件开始，到进程退出为止（受 Module Cache 影响，只初始化一次） | 通常共享——所有 `require` 到这个 Module 的调用者看到的是同一份数据 |
| **Object State** | Object（`new()`/构造函数返回的那个实例表） | 从 `.new(...)` 返回开始，到这个实例不再被任何变量引用、被 GC 回收为止 | 不共享——每个实例独立一份，`alice` 和 `bob` 互不影响 |
| **Framework State** | Framework（Plugin 模式里 `framework.lua` 内部的 registry 之类的表） | 与 Framework 自身的生命周期一致，通常是"进程启动到退出"，或者更细一点"Framework 模块被 `require` 到卸载" | Framework 内部共享——所有通过 Framework 提供的注册/查询接口进入的代码都看到同一份 registry |
| **Plugin State** | 单个 Plugin（这个 Plugin 文件自己的 `local` 变量） | 从这个 Plugin 模块第一次被 `require` 开始，到进程退出为止（同样受 Module Cache 影响） | 通常只在 Plugin 内部共享——其它 Plugin、Framework 本身都不可见，也不应该被直接读写 |

### 为什么这张表必须按"归属"而不是"写法"来区分

单看语法，Module State 和 Plugin State 其实是**同一种机制**——都是"文件顶层的 `local` 变量，受益于 `require` 的进程级缓存"。它们被拆成两行,不是因为写法不同,而是因为**在架构图里挂的位置不同、被谁允许读写不同**：

```text
Module State（Pattern 2 的例子）                  Plugin State（Pattern 5 的例子）
     |                                                  |
     v                                                  v
  这个 Module 自己的调用者                        这个 Plugin 自己
 （谁 require 它，谁就能通过 Public API 读写）   （Framework 不读它，其它 Plugin 不读它）
```

也就是说，"Module State vs Plugin State"的区别不是 Lua 语言层面的区别，而是**这个 Module 在整个系统架构图里，是被"随便什么人 require"，还是被"只有 Framework 通过统一契约调用"**——这正是 Pattern 5（Plugin）比 Pattern 2（Private+Public）多出来的那一层组织约束（Contract + Discovery/Registration，详见 [05-plugin/00-why-and-architecture.md](05-plugin/00-why-and-architecture.md)）。

Object State 和其余三种的本质区别则是**语言机制层面**的：Module/Framework/Plugin State 都只有"一份"（因为背后是同一个被缓存的 table），Object State 有"N 份"（因为 `setmetatable({}, Class)` 每次调用都会创建一张全新的 table）——这就是为什么 Object State 那一行"是否共享"写的是"不共享"，而其它三行写的是"（内部）共享"。

## 3. 一张图看四种 State 分别"住在哪张 table 里"

```text
package.loaded
     |
     +-- ["table_module_x"]  ->  M (通常没有可变字段 == 无 State)
     |
     +-- ["private_public_x"] -> M
     |         |
     |         +-- (闭包里的 local state_table)   <-- Module State，M 的所有调用者共享
     |
     +-- ["class_module_x"]  ->  ClassTable        <-- 只装方法，不装 Object State
     |         ^                        ^
     |         |                        |
     |     __index                  __index
     |         |                        |
     |     instance_a               instance_b      <-- 各自的 Object State，互不共享
     |
     +-- ["plugin_framework"] -> Framework
               |
               +-- (闭包里的 local registry)         <-- Framework State，Framework 独占
               |
               +-- require("plugins.a") -> PluginA
               |         +-- (闭包里的 local plugin_a_state)   <-- Plugin A 自己的 State
               |
               +-- require("plugins.b") -> PluginB
                         +-- (闭包里的 local plugin_b_state)   <-- Plugin B 自己的 State，PluginA 看不到
```

## 4. 小结：判断一份 State 属于哪一类的三个问题

1. **它挂在哪张 table 上？** 挂在被 `require` 缓存的 Module 返回值上（或其闭包里）——是 Module/Framework/Plugin State 三者之一；挂在 `.new()` 每次新建的实例表上——是 Object State。
2. **谁被允许读写它？** 只有这个 Module 自己的 Public API——Module State；只有 Framework 提供的注册/查询函数——Framework State；只有这个 Plugin 自己的代码——Plugin State；只有这个实例自己的方法（通过 `self`）——Object State。
3. **创建一个新的"使用者"会不会得到一份新的 State？** 不会（永远是同一份）——Module/Framework/Plugin State；会（`new()` 一次就多一份）——Object State。

下一篇 [07-dependency-direction.md](07-dependency-direction.md) 在这个 State 模型的基础上，讨论"谁依赖谁"的方向问题。
