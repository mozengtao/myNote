# 09 · Pattern 选择决策树

> 前置阅读：[00-mental-model-overview.md](00-mental-model-overview.md)、[07-cross-pattern-comparison.md](07-cross-pattern-comparison.md)、[08-unified-case-network-server.md](08-unified-case-network-server.md)。本文把前面所有论证收敛成一个可以直接照着走的决策过程，分两种用法：**正向**（我在设计一个新系统，该选哪个 Pattern）和**反向**（我在读一个陌生项目的代码，它看起来像哪个 Pattern）。

## 1. 正向决策树：我为什么要引入 Lua？

```text
我为什么要引入 Lua？
        |
        +-- 我要给 Host 增加新能力？
        |        |
        |        v
        |     Extension
        |
        +-- 我要决定 Host 应该怎么做？
        |        |
        |        v
        |      Policy
        |
        +-- 我要定义 Runtime 参数？
        |        |
        |        v
        |   Configuration
        |
        +-- 我要定义大量 IF/THEN 规则？
        |        |
        |        v
        |   Rule Engine
        |
        +-- 我要一个可独立加载/卸载的模块？
                 |
                 v
               Plugin
```

问自己"我要引入 Lua，是为了解决下面哪一句话描述的问题"，选中的那一句对应哪个分支，就先假设是哪个 Pattern。如果同时命中好几句（例如"既要新增能力，又要能独立加载卸载"），说明你的需求本身就是组合型的，回到 [07-cross-pattern-comparison.md](07-cross-pattern-comparison.md) 第 3 节的组合关系——大概率答案是 Plugin（用它把其余几种手法包起来），而不是强行只选一个。

## 2. 五行归纳：一句话动词

```text
Extension       = Add Capability
Policy          = Make Decision
Configuration   = Describe Runtime
Rule Engine     = Match Rules -> Actions
Plugin          = Encapsulated Extension
```

这五行是整棵决策树的压缩版：如果你只能记住一件事，记这五个"动词短语"就够——遇到任何一个具体系统，先问它对应哪个动词，再回到对应 Pattern 目录细读。

## 3. 反向判断：如何用这棵决策树看懂一个陌生项目

设计一个新系统时可以正向问自己"我要做什么"，但更常见的情况是你打开一个陌生的、已经嵌入了 Lua 的 C/C++ 项目，需要反过来从代码线索猜它属于哪种 Pattern。下面是几条直接从函数签名/数据形状就能判断的线索：

- **看到 `register_xxx(name, callback)` 这种签名** —— 大概率是 **Extension**。Host 暴露一个"登记新行为"的入口，`callback` 是 Lua 侧填进去的具体行为，Host 自己不实现 `callback` 里的逻辑。对照 [01-extension/code/01-c-command-extension/main.c](01-extension/code/01-c-command-extension/main.c) 的 `register_command(name, callback)`。

- **看到函数名是 `allow_xxx`/`should_xxx`/`can_xxx`，返回一个布尔值（或等价的 ALLOW/DENY 字符串）** —— 大概率是 **Policy**。这类命名本身就在暗示"这是一次是非判断"，而不是"这是一段完整的新行为"。对照 [02-policy/code/01-c-server-access-policy/policy.lua](02-policy/code/01-c-server-access-policy/policy.lua) 里的决策函数形状。

- **看到一个纯赋值的全局 table，没有函数字段（或函数字段极少、只用来做派生计算）** —— 大概率是 **Configuration**。如果打开一个 `.lua` 文件发现里面全是 `xxx = 123`/`xxx = "abc"`/`xxx = { a, b, c }` 这种声明式赋值，几乎不涉及"响应某个事件该做什么"，那它的角色就是产出一份参数表。对照 [03-configuration/code/01-c-app-configuration/config.lua](03-configuration/code/01-c-app-configuration/config.lua)。

- **看到一个数组，每一项都有 `match`/`condition` 字段和 `action` 字段** —— 大概率是 **Rule Engine**。这种"数组的数组元素结构完全一致，都是条件加动作"的形状，是 Rule Engine 独有的数据特征，Policy 和 Configuration 都不会长成这样。对照 [04-rule-engine/code/01-firewall-rule-engine/rules.lua](04-rule-engine/code/01-firewall-rule-engine/rules.lua)。

- **看到脚本被要求必须实现一组固定的回调，例如 `init`/`shutdown`/`on_xxx`，并且这组回调名字在多个不同脚本文件里重复出现** —— 大概率是 **Plugin**。"多个文件遵守同一份接口契约"是 Plugin 相对 Extension 的关键特征——单个 Extension 通常只需要实现它自己的那一个函数，不需要遵守一份所有同类脚本都要遵守的通用契约。对照 [05-plugin/code/01-c-plugin-manager/plugin_a.lua](05-plugin/code/01-c-plugin-manager/plugin_a.lua) 和 [05-plugin/code/01-c-plugin-manager/plugin_b.lua](05-plugin/code/01-c-plugin-manager/plugin_b.lua)——两个文件都实现了同一组 `plugin_init`/`on_event`/`plugin_shutdown`。

这几条线索不是严格的判定规则（真实项目里的命名千奇百怪，且前面讲过五种模式可以组合），但足够作为"第一印象"的快速筛选——拿到一个陌生项目，先看它的 Lua 绑定函数签名、脚本里的数据形状，再用这几条线索定个大致方向，之后细读代码验证。

下一篇 [10-final-mental-model.md](10-final-mental-model.md) 会给出整套笔记最终的统一心智模型图，并回答开篇提出的全部问题。
