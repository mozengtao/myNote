# 08 · 统一案例：同一个 Network Server 演出 5 种角色

> 前置阅读：[00-mental-model-overview.md](00-mental-model-overview.md)、[07-cross-pattern-comparison.md](07-cross-pattern-comparison.md)。本文用一个假想的、最简单的 Network Server 场景，把前面 5 个 Pattern 目录里分散的示例串在一条时间线上——同一个 Host，只是把 Lua 挂在架构图的不同位置，就分别演出了 Extension / Policy / Configuration / Rule Engine / Plugin 五种角色。本文不重写完整代码，每一种角色给出一段最小 ASCII 图 + 简短说明，需要完整可运行版本时跳转到对应 Pattern 目录的示例。

设想场景：一个用 C 写的、监听 TCP 端口的 Network Server，主循环是 `accept -> parse -> handle -> respond`。下面依次让 Lua 挂在这条主循环的不同挂点上。

## 1. Extension：Lua 让 Server 认识一种新协议

```text
Server
 |
 v
Lua
 |
 v
Add custom protocol
```

Server 原本的 `parse` 阶段只认识它编译时写好的协议格式。如果运营方想让它临时认识一种新的自定义协议（不改 Server 二进制），可以把"如何识别、如何拆解这种新协议"整段逻辑写成 Lua，Server 在 `parse` 阶段把原始字节交给 Lua，Lua 返回结构化的字段。Server 核心代码从未被要求"理解"这种新协议，它只是多提供了一个"把字节交给 Lua 看"的挂点。完整版本请看 [01-extension/02-example-wireshark-dissector.md](01-extension/02-example-wireshark-dissector.md)——Wireshark 本身就是一个需要不断认识新协议的"Server"（虽然它处理的是抓包文件而不是实时 TCP 流，但"给 Lua 一段原始字节，要求它解析出字段结构"这个挂点形状完全一致）。

## 2. Policy：Lua 决定这次请求该不该被处理

```text
Request
 |
 v
Lua
 |
 v
ALLOW / DENY
```

请求已经被 Server 正常 `parse` 出来了，协议、字段都是 Server 原本就认识的，Server 也已经写好了"如何真正处理一个请求"和"如何拒绝一个请求"这两段代码。这里唯一悬而未决的是"这一次、这个来源，到底该走哪一条"。Lua 在这个挂点上只做一件事：读取请求的上下文（来源 IP、请求内容摘要、当前时间），返回 ALLOW 或 DENY，不接触真正的 socket 读写。完整版本请看 [02-policy/01-example-c-server-access-policy.md](02-policy/01-example-c-server-access-policy.md)。

## 3. Configuration：Lua 描述 Server 该用什么参数启动

```text
config.lua
 |
 v
Server Configuration
```

这个挂点根本不在请求处理主循环里，而在 Server 进程启动的最早期——`accept` 循环还没开始跑，Server 需要先知道该监听哪个端口、开几个 worker 线程、超时时间设多少。`config.lua` 执行一次，产出的 table 被 Host 转换成 C struct，之后请求处理主循环读的是这份 C struct，不会在处理每个请求时反过来问 Lua。完整版本请看 [03-configuration/01-example-c-app-configuration.md](03-configuration/01-example-c-app-configuration.md)。

## 4. Rule Engine：Lua 用一组规则决定请求该被怎么处理

```text
Request
 |
 v
Rules
 |
 v
Action
```

和 Policy 的区别在于：这里不是一次任意形状的 if/else 判断，而是有一整批结构统一的规则（例如按 URL 路径、按 HTTP method、按 Header 值做路由），Server 把请求丢给 Lua 侧的规则引擎，规则引擎逐条匹配，返回命中的规则对应的 Action（转发到哪个 backend、返回什么状态码）。完整版本请看 [04-rule-engine/02-example-http-routing-rule-engine.md](04-rule-engine/02-example-http-routing-rule-engine.md)。

## 5. Plugin：Lua 把一组能力封装成独立可插拔的模块

```text
Server
 |
 v
Plugin Manager
 |
 v
auth.lua
routing.lua
logging.lua
```

这次 Lua 不再是挂在主循环的某一个单点上，而是 Server 在启动时交出一整层控制权给 Plugin Manager——`auth.lua`、`routing.lua`、`logging.lua` 各自是一个独立加载、独立初始化、独立接收事件、独立卸载的模块，Server 只认识"Plugin Manager"这一层，不关心具体有几个插件、叫什么名字。完整版本请看 [05-plugin/01-example-c-plugin-manager.md](05-plugin/01-example-c-plugin-manager.md)。

### 如果这三个插件要同时存在，会怎样组合前面四种角色

假设这个 Server 真的同时需要 `auth.lua`、`routing.lua`、`logging.lua` 三个插件，拆开看每一个插件内部实际在做什么，会发现它们分别对应前面讲过的角色：

- `auth.lua` 内部其实是一个 **Policy**：收到一次连接/请求事件时，读取来源信息，返回 ALLOW/DENY——挂点形状和第 2 节完全一致，只是这次决策逻辑被封装在一个有独立生命周期的插件里，而不是直接挂在 Server 主循环上。
- `routing.lua` 内部其实是一个 **Rule Engine**：维护一张"路径/方法 -> 目标 backend"的规则表，每次请求进来时逐条匹配——挂点形状和第 3 节完全一致。
- `logging.lua` 在 `plugin_init` 阶段读取一份 **Configuration**（日志级别、输出路径、是否记录请求体），初始化好之后运行期间不再反复读这份配置。

这三个插件互相之间在 Lua 层面完全隔离（各自的 `lua_State` 互不共享全局变量），但从 Server 的视角，三者都只是"Plugin Manager 管理的一个模块"，Server 不需要知道 `auth.lua` 内部用的是 Policy 形状还是别的形状。这与 [07-cross-pattern-comparison.md](07-cross-pattern-comparison.md) 第 3 节"一个 Plugin 内部同时用到 Configuration/Policy/Rule Engine 三种手法"讲的是同一件事，只是这里把它拆成三个独立插件而不是塞进一个插件内部——两种组织方式都合法，取决于这几种职责是否需要独立发现、独立加载、独立卸载。

## 6. 总结：同一个 Host，Lua 挂的位置不同，就是不同的 Pattern

上面五节用的是完全同一个 Network Server（同一套 `accept -> parse -> handle -> respond` 主循环、同一个 Host 二进制），Lua 本身也没有任何特殊之处——五次都只是一个 `lua_State`、一次 `lua_pcall`。真正决定"这次算 Extension 还是 Policy 还是别的什么"的，是下面三个维度的组合：

1. **Lua 是在请求处理管线的哪个阶段被调用**：是在 `parse` 阶段（Extension，负责认出新的字节格式）？是在 `parse` 和 `handle` 之间的一个决策点（Policy）？是在进程启动、主循环还没跑起来之前（Configuration）？是在 `handle` 阶段本身，处理一整批结构化输入（Rule Engine）？还是根本不在这条主循环里，而是主循环把控制权整体委托给一个独立的管理层（Plugin）？
2. **调用的触发时机不同**：每次遇到无法识别的字节触发一次（Extension）？每次请求到达触发一次（Policy）？只在启动时触发一次（Configuration）？每次请求到达但触发的是一整套规则匹配而不是单次判断（Rule Engine）？还是由 Discovery/Load 决定何时触发，之后按事件持续触发（Plugin）？
3. **Lua 返回值被 Host 如何使用**：当作"新解析出来的协议字段"直接展示/转发（Extension）？当作"选哪个已有分支"的凭证（Policy）？当作"初始化用的参数表"，转换成 C struct 后就不再理会 Lua（Configuration）？当作"一组待执行的 Action 描述"，交给 Host 去执行真正的副作用（Rule Engine）？还是根本不直接使用某个单次返回值，而是持续维护一个模块的状态和生命周期（Plugin）？

把这三个维度的具体取值分别代入，就能还原出这份笔记讲过的每一种 Pattern。下一篇 [09-pattern-decision-tree.md](09-pattern-decision-tree.md) 把这套判断过程整理成一棵可以直接照着走的决策树。
