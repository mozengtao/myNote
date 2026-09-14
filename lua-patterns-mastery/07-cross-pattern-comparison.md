# 07 · 横向对比：5 种模式放在一张表里

> 前置阅读：[00-mental-model-overview.md](00-mental-model-overview.md) 与 [01-extension/00-why-and-architecture.md](01-extension/00-why-and-architecture.md) ~ [05-plugin/00-why-and-architecture.md](05-plugin/00-why-and-architecture.md)。本文不重新论证每个 Pattern 的本质区别（那部分已经在各自的 `00-why-and-architecture.md` 里讲清楚），只做两件事：把结论收进一张表方便速查，以及展开"它们可以组合"这个前面五篇分开讲、容易被忽略的关系。

## 1. 核心对比表

| Pattern | Lua 主要角色 | Host 主要角色 | Lua 是否决定行为 | 是否增加能力 | 是否有生命周期 |
|---|---|---|---|---|---|
| Extension | Extension Logic | Core | 是 | 是 | 通常有 |
| Policy | Decision | Mechanism | 是 | 不一定 | 通常较弱 |
| Configuration | Runtime Config | Runtime | 部分 | 通常不是 | 通常仅启动时 |
| Rule Engine | Rules + Actions | Execution | 是 | 通常不是 | 有 |
| Plugin | Independent Module | Plugin Host | 是 | 是 | 强 |

### 逐列展开：为什么每个 Pattern 落在这一格

**Lua 主要角色**

- Extension：Lua 承载的是"具体新增的那段行为本身"（一个命令的实现、一个协议的解析逻辑），角色就是被 Host 调用执行的 Extension Logic，见 [01-extension/00-why-and-architecture.md](01-extension/00-why-and-architecture.md) 里"`New Behavior` 完全是 Lua 侧的产物"。
- Policy：Lua 只负责在一次调用里产出一个 Decision（ALLOW/DENY、一个枚举），不承担真正执行动作的职责，见 [02-policy/00-why-and-architecture.md](02-policy/00-why-and-architecture.md) 里"`Decision` 只是选哪个 Mechanism 的凭证，不是 Mechanism 本身"。
- Configuration：Lua 脚本执行一次，产出的是一份 Runtime Config（数据 + 少量派生计算），执行完就和 Lua 脱钩，见 [03-configuration/00-why-and-architecture.md](03-configuration/00-why-and-architecture.md) 里"转换完成后，这份数据就和 Lua 彻底脱钩了"。
- Rule Engine：Lua 侧维护的是一组结构统一的 Rules，外加与每条规则绑定的 Action 描述，见 [04-rule-engine/00-why-and-architecture.md](04-rule-engine/00-why-and-architecture.md) 里"每条规则都遵循同一个形状——一个 `match` 加一个 `action`"。
- Plugin：Lua 承载的是一整个具备身份和生命周期的 Independent Module，见 [05-plugin/00-why-and-architecture.md](05-plugin/00-why-and-architecture.md) 里"Plugin 是一个具有独立生命周期和接口契约的可插拔模块"。

**Host 主要角色**

- Extension：Host 是 Core——只提供稳定的注册入口，不关心注册进来的具体行为，见 [01-extension/00-why-and-architecture.md](01-extension/00-why-and-architecture.md) 的 `Stable Core API`。
- Policy：Host 是 Mechanism 的持有者——真正执行 ALLOW/DENY/ATTACK/FLEE 这些动作的代码都在 Host 侧，Lua 只挑一个，见 [02-policy/00-why-and-architecture.md](02-policy/00-why-and-architecture.md) 的 `Mechanism -> Policy -> Decision` 链。
- Configuration：Host 是 Runtime 本身——用 Lua 产出的参数表来初始化自己的网络监听、线程池等真实机制，见 [03-configuration/00-why-and-architecture.md](03-configuration/00-why-and-architecture.md) 的 Architecture 一节。
- Rule Engine：Host 是 Execution 方——真正丢包、真正转发请求、真正发告警邮件的副作用永远在 Host 一侧执行，见 [04-rule-engine/00-why-and-architecture.md](04-rule-engine/00-why-and-architecture.md) 里"Lua 全程不直接接触 socket/邮件系统等真实资源"。
- Plugin：Host 是 Plugin Host（Plugin Manager 所在的那一层）——只认识"Plugin Manager"，不认识任何具体插件的名字或行为，见 [05-plugin/00-why-and-architecture.md](05-plugin/00-why-and-architecture.md) 的 Architecture 一节。

**Lua 是否决定行为**

- Extension/Policy/Rule Engine/Plugin 都是"是"：这四种模式里，Lua 的输出（新行为、Decision、命中的规则、插件的响应）都会直接改变 Host 这一次的实际动作走向。
- Configuration 是"部分"：Configuration 决定的是"参数取什么值"，间接影响 Host 的行为细节（例如超时时间长短），但通常不决定"走哪个分支""要不要执行某个动作"这类是非判断，见 [03-configuration/00-why-and-architecture.md](03-configuration/00-why-and-architecture.md) 里 Configuration 与 Policy 的区分——"Policy 是每次都问，Configuration 是问一次，记住答案"。

**是否增加能力**

- Extension、Plugin 是"是"：两者都在 Host 原本不具备的能力集合里新增条目，Plugin 是 Extension 的工程化组织形式，见 [05-plugin/00-why-and-architecture.md](05-plugin/00-why-and-architecture.md) 开篇的关系论证。
- Policy、Rule Engine 是"不一定"：两者默认不新增能力，只是在 Host 已有能力之间做选择/匹配；但如果 Policy 的某个分支或 Rule Engine 的某条 Action 恰好触发了一个 Host 原本没有暴露过的新副作用，也可能间接带来新能力——这就是"不一定"而不是绝对的"否"。
- Configuration 是"通常不是"：Configuration 改的是参数，不改变 Host 能做什么事情的集合，见 [03-configuration/00-why-and-architecture.md](03-configuration/00-why-and-architecture.md) 与 Extension 的对比——"Extension 回答能做什么新的事，Configuration 回答已有的事该用什么参数跑"。

**是否有生命周期**

- Plugin 是"强"：有显式的 Discovery/Load/Register/Lifecycle/Event/Unload 六步状态机，见 [05-plugin/00-why-and-architecture.md](05-plugin/00-why-and-architecture.md)。
- Extension 是"通常有"：至少有"注册一次、随 Host 退出一起销毁"这样简化版的生命周期，但没有 Plugin 那种运行时 Discovery 和显式状态机，见 [01-extension/00-why-and-architecture.md](01-extension/00-why-and-architecture.md) 的 Lifecycle 一节。
- Rule Engine 是"有"：规则表整体作为一份数据有加载/热更新的生命周期，但**单条规则**没有独立生命周期，只是表里的一条记录，见 [04-rule-engine/00-why-and-architecture.md](04-rule-engine/00-why-and-architecture.md) 与 Plugin 的对比。
- Policy 是"通常较弱"：一次决策随 Host 的一次调用而生、随返回而消亡，没有跨调用保留的状态机，见 [02-policy/00-why-and-architecture.md](02-policy/00-why-and-architecture.md) 与 Plugin 的对比——"Policy 只是请求处理管线里的一个决策点，没有独立的加载/卸载概念"。
- Configuration 是"通常仅启动时"：`lua_State` 通常"加载 -> 读取 -> 立刻 `lua_close`"，生命周期短且单次，见 [03-configuration/00-why-and-architecture.md](03-configuration/00-why-and-architecture.md) 的 Architecture 一节。

## 2. 单向衍生关系

某些 Pattern 是另一些 Pattern 的"更工程化的组织方式"或"典型实现载体"，这是一种单向的衍生/包含关系，不是对等关系：

```text
Extension
    |
    +-- Plugin
    |
    +-- Policy

Configuration
    |
    +-- 可以包含 Policy / Extension

Rule Engine
    |
    +-- 可以实现 Policy
```

- `Extension -> Plugin`：Plugin 是"把 Extension 组织成可发现、可管理的独立模块"之后的产物，见 [05-plugin/00-why-and-architecture.md](05-plugin/00-why-and-architecture.md) 开篇——"Plugin 是 Extension 的一种工程化组织方式"。
- `Extension -> Policy`：这条线画的是"载体复用"而不是"包含"——一个 Extension 的具体实现内部，完全可能需要做一次决策（例如这个新命令要不要允许当前用户执行），这时候 Extension 内部那一小段判断逻辑，其形状就是一次 Policy。
- `Configuration -> 可以包含 Policy / Extension`：一份 Configuration 脚本里，`endpoint = function(env) ... end` 这样的字段本质上就是一次小型 Policy（根据环境选一个值）；如果配置脚本里还允许定义钩子函数供 Host 在特定时机调用，那部分钩子在行为上就接近 Extension。
- `Rule Engine -> 可以实现 Policy`：一次 Policy 决策，如果本身就是"匹配一组条件，选出对应结果"的形状，完全可以用 Rule Engine 的 `match+action` 框架来实现——这时 Rule Engine 是 Policy 的一种具体落地方式，而不是与 Policy 平行的另一个东西。

## 3. 组合关系：它们可以叠在一起

上面的衍生关系已经暗示了一个更普遍的事实——**这五种 Pattern 不是互斥的分类，而是"Lua 在 Host 架构图里挂的位置"的五种典型形状，同一个模块内部完全可以同时具备好几种形状**：

```text
Plugin
  |
  +-- Configuration
  |
  +-- Policy
  |
  +-- Rule Engine
```

这张图的含义：一个 Plugin（有身份、有生命周期、可独立加载卸载的最大颗粒度单元）内部，`plugin_init` 阶段做的事情往往是 Configuration（读取一份初始参数），运行中收到单次事件时做的判断往往是 Policy（这一次该不该响应），如果这个插件本身需要处理大量结构相似的输入，内部很可能还会长出一套 Rule Engine（一张 `match+action` 表）。Host 从头到尾只看到"这是一个插件"，内部这三层手法的存在与否，Host 完全不关心。

### 具体例子：一个假想的 `access_control` 插件

现有代码目前没有哪一个 `.lua` 文件同时把三种手法都写全，[05-plugin/code/01-c-plugin-manager/plugin_b.lua](05-plugin/code/01-c-plugin-manager/plugin_b.lua) 只体现了 Plugin 的生命周期三件套（`plugin_init`/`on_event`/`plugin_shutdown`）加一个简单的模块级状态计数器，没有内部再细分出 Policy/Rule Engine 的结构。用一段简短的讲解性伪代码把三层手法在一个插件内部同时出现的样子补完：

```lua
-- 伪代码，仅用于说明"一个 Plugin 内部同时用到三种手法"，不是新建的可运行示例

-- 1. Configuration 手法：plugin_init 阶段读取一份声明式参数，只在加载时算一次
local cfg = {
    max_failed_attempts = 5,
    whitelist = { "10.0.0.1", "10.0.0.2" },
}

function plugin_init(host_name)
    host_log("access_control 已加载，白名单共 " .. #cfg.whitelist .. " 条")
end

-- 2. Policy 手法：每次收到一次连接事件时，当场决定 ALLOW 还是 DENY
local function is_whitelisted(ip)
    for _, addr in ipairs(cfg.whitelist) do
        if addr == ip then return true end
    end
    return false
end

-- 3. Rule Engine 手法：一组结构统一的 match+action 规则，逐条匹配
local rules = {
    { match = function(ev) return is_whitelisted(ev.ip) end,
      action = "ALLOW" },
    { match = function(ev) return ev.failed_attempts >= cfg.max_failed_attempts end,
      action = "DENY" },
}

function on_event(name, payload)
    local ev = decode_event(payload) -- 假设已经拆成 { ip=..., failed_attempts=... }
    for _, rule in ipairs(rules) do
        if rule.match(ev) then
            host_log("access_control 决策: " .. rule.action .. " for " .. ev.ip)
            return
        end
    end
    host_log("access_control 决策: 默认 DENY for " .. ev.ip)
end

function plugin_shutdown()
    host_log("access_control 已卸载")
end
```

对 Host（[05-plugin/code/01-c-plugin-manager/plugin_manager.c](05-plugin/code/01-c-plugin-manager/plugin_manager.c) 里的 `plugin_manager.c`）而言，这个插件和 [05-plugin/code/01-c-plugin-manager/plugin_a.lua](05-plugin/code/01-c-plugin-manager/plugin_a.lua)、[05-plugin/code/01-c-plugin-manager/plugin_b.lua](05-plugin/code/01-c-plugin-manager/plugin_b.lua) 没有任何区别——一样是 `plugin_init`/`on_event`/`plugin_shutdown` 三个约定函数，一样走 Discovery/Load/Register/Event/Unload 六步生命周期。三种手法的组合完全被封装在插件内部，这正是 [05-plugin/00-why-and-architecture.md](05-plugin/00-why-and-architecture.md) 里"Host 只知道这是一个插件，并不关心插件内部到底用了哪几种手法"这句话的具体展开。

下一篇 [08-unified-case-network-server.md](08-unified-case-network-server.md) 会用同一个 Network Server 场景，展示 Lua 挂在架构图不同位置时，如何分别单独演出这五种角色。
