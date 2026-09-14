# 00 · Pattern E：Plugin —— WHY & Architecture

> 前置阅读：[00-mental-model-overview.md](../00-mental-model-overview.md)。本文复用其中定义的词汇：Host 拥有 Mechanism / Resource / Lifecycle / Stable API；Lua 拥有 Behavior / Policy / Rules / Configuration / Extension。本文展开的是 Lua 一侧列表里最后、也是颗粒度最大的一项——**Extension 的一种特殊组织形式：Plugin**。

## WHY：Plugin 和 Extension 到底是什么关系

很多资料会把 Plugin 和 Extension 直接划等号——"插件不就是扩展吗"。这个说法不算错，但会掩盖一个更重要的区分点，这份笔记不打算这么处理。

- **Extension 强调的是"增加了什么"**：Host 原来不会解析某个协议、不认识某条命令，Extension 加上去之后，Host 多了这一种具体能力。Extension 关心的是"能力的内容"，通常以**单个函数/单段脚本**的颗粒度存在，生命周期很简单——随 Host 启动一次性加载，随 Host 退出一起消失，中间不需要单独被发现、单独被卸载。
- **Plugin 强调的是"以什么形式组织起来"**：Plugin 是**一个具有独立生命周期和接口契约的可插拔模块**。它当然也是在给 Host "增加能力"（这一点和 Extension 一致），但 Plugin 额外要求：这个模块必须能被 Host 单独发现、单独加载、单独初始化、单独接收事件、单独卸载，并且这一整套过程对**任意数量**的模块都要成立，不是"写死一个"，而是"Host 提供一套通用机制，能装下 N 个"。

换句话说：**Extension 回答"多了什么能力"，Plugin 回答"这些能力是以什么样的、可复制的、可管理的形式被组织进 Host 的"**。一个 Extension 只要把自己写成满足 Plugin 契约的形状（有身份、能被发现、有统一的初始化/卸载函数），就升级成了一个 Plugin；反过来，几乎所有 Plugin 系统内部也确实是"很多个 Extension 打包在一套通用管理机制里"。所以更准确的说法是：**Plugin 是 Extension 的一种工程化组织方式，专门解决"当 Extension 的数量从 1 个变成 N 个"之后的管理问题**。

这也解释了为什么 Plugin 的生命周期比 Extension 明显更长、更完整：

```text
Discovery
 |
 v
Load
 |
 v
Register
 |
 v
Lifecycle
 |
 v
Event
 |
 v
Unload
```

- **Discovery**：Host 在启动时（或运行中）主动去"找"有哪些插件存在——扫描一个目录、读一份插件清单、遍历一个已知的文件名模式。Extension 不需要这一步，因为 Extension 通常是开发者在代码里直接 `require`/`dofile` 写死的，Host 不需要"发现"它。
- **Load**：把发现到的插件文件编译、执行，让插件的顶层代码（通常是定义好的一组函数/一个 table）变得可用。
- **Register**：插件把自己的身份、能力"登记"进 Host 的某个管理结构里，Host 由此知道"现在多了一个可以调度的模块"。
- **Lifecycle**：Host 持续跟踪这个插件的状态（是否已初始化、是否正在运行、是否已失败），状态可能在插件的整个存活期内多次变化。
- **Event**：Host 在运行过程中把发生的事情持续通知给所有存活的插件，插件各自决定要不要响应、怎么响应。
- **Unload**：插件被显式地、干净地从 Host 里移除，释放它占用的资源。

Discovery / Register / 显式的 Lifecycle 状态机 / Unload，这四步是 Plugin 相对 Extension 真正多出来的部分；Load 和 Event 两者都有，只是 Plugin 里的 Load 面向"任意数量、运行时发现"的插件集合，Event 面向"任意数量、独立响应"的插件集合。

## Architecture：Plugin 在 Host 架构图里的位置

```text
                 Host
                  |
           Plugin Manager
                  |
       +----------+----------+
       |          |          |
   plugin_a   plugin_b   plugin_c
       |          |          |
      Lua        Lua        Lua
```

逐层解释：

- **Host**：拥有 Mechanism（主循环、事件源、真正的资源句柄）和 Resource，但不认识任何一个具体插件的名字或行为，只认识"Plugin Manager"这一层。
- **Plugin Manager**：这是 Plugin 模式相对其余四种模式**唯一新增的中间层**——一段 C/C++ 代码，负责 Discovery / Load / Register / Lifecycle / Event / Unload 这六步机制本身，但不包含任何一个具体插件的业务逻辑。可以把它理解成"专门用来管理一组 Extension 的调度器"。
- **plugin_a / plugin_b / plugin_c**：每一个都是 Plugin Manager 视角下的一条"记录"——一个身份、一个状态、一份可调用的接口，具体有几个、叫什么名字，Manager 在编译时并不知道，都是 Discovery 阶段才确定下来的。
- **Lua**：每个插件的具体行为用 Lua 实现（可以共享同一个 `lua_State`，也可以每个插件独立一个 `lua_State`——这是一个需要显式取舍的设计问题，在示例 1 里详细展开）。

### 每个 Plugin 都应该具有的五个要素

不管 Host 是 C 程序、Wireshark 还是 Neovim，一个"合格"的 Plugin 在概念上都应该能回答下面五个问题，只是每个 Host 具体怎么落地不同：

- **Identity（身份）**：这个插件叫什么名字？靠什么区分于其他插件？（通常是文件名、`Proto` 对象、模块名）
- **Registration（注册）**：这个插件通过什么动作，让 Host 知道"我存在，请把我纳入管理"？（调用一个注册函数、赋值一个约定的全局表、`Proto.new()`）
- **API（接口契约）**：Host 和插件之间约定好的、双向都遵守的函数集合——插件必须实现哪些函数供 Host 调用（`plugin_init`/`on_event`），Host 提供哪些函数供插件调用（`host_log`）？
- **Lifecycle（生命周期状态）**：这个插件当前处于哪个阶段？是刚加载还是已经在运行，还是已经失败/已经卸载？
- **State（状态）**：这个插件内部维护的、只属于它自己的数据（计数器、缓存、开关），和其他插件的 State 完全隔离。

这五项在 C + Lua 的示例（[01-example-c-plugin-manager.md](01-example-c-plugin-manager.md)）里会逐条落地成具体的代码结构；Wireshark 和 Neovim 两个真实 Host 会展示同样的五要素在**已有框架**里是怎么被弱化或者用另一种形式实现的。

## 本质区别：Plugin vs 其余四种模式

- **vs Extension**：Extension 是**单一新增能力**，颗粒度是"一个函数/一段脚本"，没有独立的 Discovery/Register/Unload 机制；Plugin 是**多个独立模块 + 一套统一管理器**，颗粒度是"一个完整模块"，天生要解决"N 个模块怎么被同一套机制发现、调度、卸载"的问题。
- **vs Policy**：Policy 关心"这一次该怎么决策"，通常连接一次 Host 调用和一次返回值；Plugin 关心的是"这个模块要不要存在、现在处于什么状态"，是跨越整个模块生命周期的更大范围问题。
- **vs Configuration**：Configuration 关心"运行时参数应该是什么"，是一次性的（或按需重新加载的）取值过程；Plugin 关心的是一个持续存活、持续响应事件的模块本身。
- **vs Rule Engine**：Rule Engine 关心"一批结构统一的规则怎么被匹配"，规则本身没有独立生命周期，只是数据表里的一条条记录；Plugin 里的每一个插件都有自己独立的生命周期状态机。

更准确地说，Policy / Configuration / Rule Engine 都是"**在一次调用里，Lua 返回给 Host 什么样的结果**"这个问题的三种不同答案，而 Plugin 是更高的一层封装——**一个 Plugin 内部完全可以同时用到 Policy、Configuration、Rule Engine 三种手法**，只是这些手法这时候是被封装在一个具备独立生命周期的模块内部，对 Host 而言只暴露"这是一个插件"这一层接口：

```text
Plugin
  |
  +-- Configuration
  |
  +-- Policy
  |
  +-- Rule Engine
```

举例：一个"访问控制"插件在 `plugin_init` 阶段读取一份 Configuration（超时时间、白名单文件路径），在 `on_event` 收到一次连接请求时执行一段 Policy（这个 IP 现在允许连接吗），内部还维护一张 Rule Engine 式的规则表（"来自内网 -> 直接放行"这类批量规则）。从 Host 的角度，它只知道"这是一个叫 `access_control` 的插件，有身份、有生命周期、有状态"，并不关心插件内部到底用了哪几种手法。这也是为什么 Plugin 排在这套 Mental Model 的最后一个 Pattern：前四种是"积木"，Plugin 是"用积木搭出来的、可以整体拿起放下的模块"。

## 核心不是 Lua，而是这个抽象

Discovery -> Load -> Register -> Lifecycle -> Event -> Unload 这套六步生命周期，本质上是操作系统内核模块（`insmod`/`rmmod`）、浏览器扩展、IDE 插件系统、`dlopen`/`dlclose` 动态库这些机制的共同抽象，跟 Lua 没有必然联系。Lua 在这里扮演的角色是"用最低成本实现 Load/Unload"：`luaL_loadfile` + `lua_pcall` 就是"加载一个模块"，`lua_close` 就是"卸载一个模块并保证不留资源"，比起用 `dlopen` 加载编译好的 `.so` 插件，Lua 版本不需要为每个插件单独编译、不需要处理 ABI 兼容问题，出错时也不会让整个 Host 进程崩溃。如果 Lua 消失，这套六步抽象依然成立，只是 Load/Unload 要换成别的载体（详见三个示例第 16 步的具体讨论）。
