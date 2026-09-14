# 02 · 示例2：Wireshark Lua Plugin

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)、[01-example-c-plugin-manager.md](01-example-c-plugin-manager.md)。本文用 Wireshark 这个真实存在、体量巨大的 Host 程序，展示 Plugin 六步生命周期在一个**已有框架**里是怎么被落地（以及被弱化）的。

## 1. Problem

Wireshark 本身不可能内置全世界所有网络协议的解析逻辑——协议数量太多、更新太快，也不是每个用户都需要。用户/团队经常会遇到"我们内部有一个自定义协议，希望 Wireshark 能像解析 TCP/HTTP 一样把它的字段展开显示"这种需求。如果每次都要改 Wireshark 的 C/C++ 源码、重新编译整个程序，这件事的门槛会高到几乎没人愿意做。

## 2. Why Lua

Wireshark 本身早就嵌入了 Lua（`lua_State` 由 Wireshark 主进程创建），并且围绕 `Proto`/`Tvb`/`Pinfo`/`TreeItem` 这些 C 结构体做了大量 Binding。用户只需要写一个 `.lua` 文件，放进一个约定的目录，Wireshark 启动时会自动发现并加载——不需要用户理解 Wireshark 的 C 代码、不需要重新编译，改错了也只是这一个 dissector 解析失败，不会导致 Wireshark 崩溃。这正是 Lua "可以被宿主语言完全托管"特性的典型应用。

## 3. Architecture

```text
plugin.lua
    |
    v
Proto
    |
    v
Fields
    |
    v
Dissector
    |
    v
DissectorTable registration
```

`plugin.lua` 是插件文件本身；`Proto` 是这个插件在 Wireshark 里的身份对象；`Fields` 是挂在 `Proto` 上的字段元数据；`Dissector` 是真正的运行时解析函数；`DissectorTable registration` 是把 `Proto` 接到 Wireshark 分发体系上的最后一步。这条链路从上到下，正好对应 Plugin 生命周期里 Load（脚本被执行）到 Register（各级对象依次挂载）的过程。

## 4. Host responsibilities

- **Discovery**：Wireshark 启动时遍历"Personal Lua Plugins"和"Global Lua Plugins"两个固定目录，找出其中所有 `.lua` 文件。这两个目录路径可以在 `Help -> About Wireshark -> Folders` 里查到，因平台而异（例如 Linux 上 Personal 目录通常是 `~/.local/lib/wireshark/plugins/` 或 `~/.wireshark/plugins/`，具体以安装的 Wireshark 版本文档为准）。
- **Load**：对发现的每个 `.lua` 文件，Wireshark 在自己的 Lua 环境里执行它的顶层代码（大致等价于本笔记 C 示例里的 `luaL_loadfile` + `lua_pcall`，只是这一步完全被 Wireshark 内部封装，插件作者看不到、也管不到）。
- **提供 Binding**：`Proto`/`ProtoField`/`Tvb`/`Pinfo`/`TreeItem`/`DissectorTable` 这些全局对象，都是 Wireshark 预先注册进 Lua 环境的 C Binding，插件脚本只是"使用"它们，不需要（也无法）自己实现。
- **在真正收到包时调用 dissector**：这一步完全由 Wireshark 的抓包/解析主循环驱动，插件脚本本身是被动的。

## 5. Lua responsibilities

- 声明一个 `Proto` 对象作为自己的身份。
- 声明一组 `ProtoField`，描述协议里有哪些字段、怎么显示。
- 实现 `dissector(tvb, pinfo, tree)` 函数，从原始字节里读出字段值、写进 Packet Details 树、设置 Info 列文字。
- 调用 `DissectorTable:add(...)` 把自己注册到某张分发表上，声明"这一类包应该交给我处理"。

## 6. Lifecycle

```text
Discovery（Wireshark 扫描 Lua Plugins 目录）
 |
 v
Load（执行 plugin.lua 顶层代码）
 |
 v
Register（Proto.new -> fields 赋值 -> DissectorTable:add）
 |
 v
Event（每收到一个匹配的包，调用一次 dissector）
 |
 v
Unload（进程退出 / Reload Lua Plugins，隐式、无回调）
```

这里最值得注意的是 Wireshark **没有**提供类似示例1里 `plugin_shutdown()` 这样显式的卸载回调。`plugin.lua` 一旦被加载，它注册的 `Proto`/`DissectorTable` 挂载点会一直存在，直到整个 Lua 环境被销毁（进程退出）或用户手动执行 `Tools -> Reload Lua Plugins`（这个操作会重新创建 Lua 环境、重新执行所有插件文件，而不是先调用某个"卸载函数"再清理）。这一点会在第 10 步 Failure 和第 16 步里进一步展开。

## 7. Data Flow（C -> Lua）

- Wireshark 的抓包引擎（C/C++）把原始字节封装成 `Tvb` 对象，把包的元信息封装成 `Pinfo` 对象，在调用 `dissector(tvb, pinfo, tree)` 时作为参数传入——这是"Host 主动调用 Lua，并把数据带过去"的典型例子，和示例1里 `lua_pcall` 传入事件名/payload 是同一套模式，只是这里的参数是更复杂的 Binding 对象（`userdata` + `metatable`），不是简单字符串。
- `tvb(0, 2):uint()` 这样的调用，实际上是 Lua 侧对 `Tvb` 这个 userdata 调用其 metatable 里定义的 C 函数，从原始字节里按大端读出一个整数——这一步跨越 Lua/C 边界，但对写脚本的人来说只是"看起来像一个普通函数调用"。

## 8. Control Flow（Lua -> C）

- `tree:add(...)`、`pinfo.cols.protocol = ...` 这些调用，形式上是 Lua 调用一个对象的方法/给一个字段赋值，实际执行的都是 Wireshark 侧 Binding 层注册好的 C 函数——Lua 通过这些函数反向影响 Wireshark 的界面显示（Packet Details 树、Column 文字）。
- `DissectorTable:add(23456, my_plugin_proto)` 是这个插件唯一一次"主动注册自己"的控制流：在这一行执行之前，Wireshark 完全不知道 `myplugin` 协议存在；执行之后，Wireshark 的包分发逻辑里多了一条"TCP 端口 23456 -> my_plugin_proto.dissector"的路由。这是"Lua 主动发起，但只能发起 Host 允许的调用"的具体体现——插件不能随意抢占任意端口的解析权，只能往 Wireshark 已经开放的 `DissectorTable` 里"挂"。

## 9. State

- **Host 侧 State**：Wireshark 自己维护的、所有已注册协议/字段/挂载点的全局注册表，这是 Wireshark 内部的 C 数据结构，`plugin.lua` 只是往里追加了一条记录，脚本本身看不到这个注册表的其他内容。
- **Lua 侧 State**：本示例的 `plugin.lua` 里没有维护任何跨包的可变状态（`fields`、`KIND_NAMES` 都是加载时就确定、之后只读的常量表）。这是一个刻意的简化：真实的复杂 dissector 经常会维护一些跨包状态（例如 TCP 流重组缓冲区），但那已经超出 Plugin 生命周期本身要讲的内容。
- 对比示例1的 `plugin_b.lua`：那里的 `event_count` 是"插件自己攒的、跨事件持续存在"的 State；这里如果要加类似的东西（比如统计"目前一共解析过多少个 MyPlugin 包"），做法是一样的——在 `dissector` 函数外面声明一个 `local` 变量，在函数内部累加。

## 10. Failure

- **加载阶段报错**（`plugin.lua` 本身有语法错误，或者顶层代码执行时抛错）：Wireshark 会在启动日志/`Tools -> Reload Lua Plugins` 后的对话框里报告这个 `.lua` 文件加载失败，但**不会导致 Wireshark 崩溃或影响其他插件**——这和示例1里"脚本加载失败，不计入插件表"是同一个失败等级，只是这里的"插件表"是 Wireshark 内部维护、脚本作者看不到的结构。
- **dissector 运行时报错**（例如访问了一个不存在的字段、除零）：Wireshark 会在 Packet Details 里显示一个"Malformed Packet"或异常提示，通常只影响这一个包/这一个协议的解析结果，不会让 Wireshark 崩溃，也不会阻止其他协议的 dissector 继续正常工作——这体现了和示例1一样的"错误隔离"原则，只是这里的隔离机制是 Wireshark C 代码内部对 dissector 调用做的 `pcall`（Wireshark 的 dissector 引擎本身就是用 `lua_pcall` 风格的保护调用来执行 Lua dissector 的），脚本作者不需要自己写保护逻辑。
- **没有卸载回调意味着什么失败模式**：如果插件在 Register 阶段做了什么"不可逆"的事情（例如往一个全局表里追加了太多数据、注册了会重复触发的定时器），由于没有 `plugin_shutdown()`，唯一的"恢复手段"是重启 Wireshark 进程或依赖 `Reload Lua Plugins` 重新执行整个文件（而不是先清理再重载）。这是本示例 Failure 讨论里，和示例1、示例3最大的不同点。

## 11. Complete Host Code

Wireshark 本身是 Host（一个体量巨大的 C/C++ 程序），本示例不需要、也不可能提供它的源码；Host 提供的是**运行时环境和一整套 Binding**（`Proto`/`ProtoField`/`Tvb`/`Pinfo`/`TreeItem`/`DissectorTable` 等全局对象），这些由 Wireshark 自身实现，不属于本笔记的示例代码范围。

## 12. Complete Lua Code

```lua
-- plugin.lua
--
-- Pattern E · Plugin 在 Wireshark 里的形态：一个放进
-- Personal/Global Lua Plugins 目录、被 Wireshark 启动时自动
-- 发现并加载的 .lua 文件。
--
-- 与 01-extension/code/02-wireshark-dissector/mini_extension.lua 用的
-- 是同一套 Proto/ProtoField/dissector/DissectorTable API，
-- 但这份文件的讲解重点不是"怎么解析字节"，而是
-- "Wireshark Plugin = Host Framework（Wireshark 自身）+
--   Lua Protocol Plugin（这个文件）"这个定位。
--
-- 玩具协议 MyPlugin Proto 的报文格式（大端，与 Extension 示例的协议
-- 字段布局一致，只是改了协议名/字段名，避免和 01-extension 混淆）：
--
--   +----------------+----------------+------------------+-----------------+
--   | magic (2 bytes)| kind (1 byte)  | length (2 bytes)  | payload (N)     |
--   +----------------+----------------+------------------+-----------------+
--
-- magic  : 固定值 0xB17E，用来快速识别这是不是 MyPlugin 报文
-- kind   : 消息种类，1 = OPEN，2 = DATA，其余 = UNKNOWN
-- length : payload 部分的字节长度
--
-- 简化点：不处理 TCP 分片重组、不处理校验和，只做单包定长头部解析，
-- 聚焦 Plugin 生命周期本身，不是协议解析细节。

------------------------------------------------------------
-- Discovery + Load（由 Wireshark 完成，这份脚本不需要写任何代码）
--
-- Wireshark 启动时会遍历"Personal Lua Plugins"和"Global Lua
-- Plugins"两个固定目录（具体路径见 Help -> About Wireshark ->
-- Folders），把其中每一个 .lua 文件当作一个插件，逐个执行其
-- 顶层代码——这就是 Discovery（找出目录下有哪些 .lua 文件）
-- 和 Load（执行它们）。这两步完全由 Wireshark 这个 Host 框架
-- 负责，插件作者不需要、也没有办法插手。
------------------------------------------------------------

------------------------------------------------------------
-- Register 第 1 步：声明协议对象
--
-- Proto.new()（这里用简写 Proto(...)）创建的对象既是这个插件的
-- Identity（在 Wireshark "About Plugins" 列表、Filter 栏里显示的
-- 名字都来自它），也是后面所有 Register 动作的挂载对象——
-- ProtoField 挂在它的 .fields 上，dissector 函数挂在它的
-- .dissector 上。可以把 my_plugin_proto 理解成这个插件在
-- Wireshark 里的"注册表条目本身"。
------------------------------------------------------------
local my_plugin_proto = Proto("myplugin", "My Plugin Protocol")

------------------------------------------------------------
-- Register 第 2 步：声明字段元数据
--
-- ProtoField 不是"运行时数据"，而是字段的元数据（名字、显示方式、
-- 过滤器表达式），决定了 Packet Details 树里怎么展示、
-- Filter 栏里可以写 myplugin.kind == 1 这样的表达式。
------------------------------------------------------------
local fields = {
    magic   = ProtoField.uint16("myplugin.magic",  "Magic",  base.HEX),
    kind    = ProtoField.uint8("myplugin.kind",    "Kind",   base.DEC),
    length  = ProtoField.uint16("myplugin.length", "Length", base.DEC),
    payload = ProtoField.bytes("myplugin.payload", "Payload"),
}
my_plugin_proto.fields = fields

local KIND_NAMES = {
    [1] = "OPEN",
    [2] = "DATA",
}

local MAGIC_VALUE = 0xB17E
local HEADER_LEN = 5 -- magic(2) + kind(1) + length(2)

------------------------------------------------------------
-- Register 第 3 步：dissector 函数
--
-- dissector 是这个 Plugin 真正的"运行时行为"（对应 Mental Model
-- 里 Lua 拥有的 Behavior）：Wireshark 的 Dissector Framework
-- 对每一个被路由到这里的包都会调用它一次。
--
-- 参数：
--   tvb   - Tvb，对当前包原始字节的只读视图
--   pinfo - Pinfo，包的元信息（可读写，例如设置 Column 显示文本）
--   tree  - TreeItem，Packet Details 树的根节点
------------------------------------------------------------
function my_plugin_proto.dissector(tvb, pinfo, tree)
    local len = tvb:len()
    if len < HEADER_LEN then
        return 0 -- 数据不够一个完整头部，告诉框架"我没能识别这个包"
    end

    local magic = tvb(0, 2):uint()
    if magic ~= MAGIC_VALUE then
        return 0 -- 不是 MyPlugin 报文
    end

    pinfo.cols.protocol = my_plugin_proto.name

    local kind = tvb(2, 1):uint()
    local payload_len = tvb(3, 2):uint()
    local kind_name = KIND_NAMES[kind] or "UNKNOWN"

    pinfo.cols.info = string.format("MyPlugin %s (kind=%d, len=%d)", kind_name, kind, payload_len)

    local subtree = tree:add(my_plugin_proto, tvb(), "My Plugin Protocol, Kind: " .. kind_name)
    subtree:add(fields.magic, tvb(0, 2))
    subtree:add(fields.kind, tvb(2, 1)):append_text(" (" .. kind_name .. ")")
    subtree:add(fields.length, tvb(3, 2))

    local available = len - HEADER_LEN
    local actual_payload_len = math.min(payload_len, available)
    if actual_payload_len > 0 then
        subtree:add(fields.payload, tvb(HEADER_LEN, actual_payload_len))
    end

    return HEADER_LEN + actual_payload_len
end

------------------------------------------------------------
-- Register 第 4 步：挂载点注册（DissectorTable）
--
-- DissectorTable 是"谁来决定把包转给我"的注册表：Wireshark 内部
-- 维护着很多张这样的表（按 TCP/UDP 端口分发、按以太网类型分发……），
-- 这一行代码把 my_plugin_proto 挂到 tcp.port 这张表的 23456 端口上，
-- 等价于告诉 Wireshark："TCP 端口 23456 上的数据交给我解析"。
--
-- 这一步就是 Plugin 生命周期里的 Register：脚本执行到这一行之前，
-- Wireshark 完全不知道 MyPlugin 协议的存在；执行到这一行之后，
-- 它才真正"挂进"了 Wireshark 的分发体系。
------------------------------------------------------------
local tcp_port_table = DissectorTable.get("tcp.port")
tcp_port_table:add(23456, my_plugin_proto)

------------------------------------------------------------
-- Lifecycle / Unload：Wireshark 的 Lua Plugin 没有对应的
-- plugin_shutdown() 概念。进程退出（或用户执行
-- Tools -> Reload Lua Plugins）时，之前加载的 Lua 环境被整体丢弃，
-- 这就是它的"Unload"——一次性的、隐式的，不给插件作者任何
-- 主动清理的机会。详见本文档正文第 10 步 Failure 和第 16 步的讨论。
------------------------------------------------------------

print("[plugin.lua] MyPlugin dissector 已注册到 TCP 端口 23456")
```

## 13. Execution

1. 找到 Wireshark 的 Lua Plugins 目录：打开 Wireshark，`Help -> About Wireshark -> Folders`，查看 "Personal Lua Plugins" 一行对应的路径（如果该目录不存在需要自己创建）。
2. 把 `plugin.lua` 复制到这个目录里。
3. 重启 Wireshark，或者在菜单里执行 `Tools -> Reload Lua Plugins`（不需要重启也能生效，这就是 Load 阶段被重新触发一次）。
4. 打开 `Help -> About Wireshark -> Plugins`，在列表里应该能看到 `plugin.lua`，类型（Type）列显示为 Lua 脚本——这一步确认 Discovery + Load 都成功了。
5. 用 `Statistics -> Endpoints` 或者直接抓包验证：任何 TCP 端口号为 23456 的流量，Wireshark 的 Protocol 列会显示 `MYPLUGIN`（对应 `my_plugin_proto.name`），Packet Details 树里能展开看到 `My Plugin Protocol` 子树，字段值和 `Magic`/`Kind`/`Length`/`Payload` 一一对应。也可以直接在 Filter 栏输入 `myplugin.kind == 1` 验证 `ProtoField` 是否注册成功。

## 14. Step-by-step walkthrough

1. Wireshark 启动，Discovery 阶段扫描 Lua Plugins 目录，找到 `plugin.lua`（连同同目录下可能存在的其他插件文件）。
2. Load 阶段：Wireshark 在自己内部的 Lua 环境里执行 `plugin.lua` 的顶层代码。此时 `Proto`/`ProtoField`/`DissectorTable` 这些全局对象已经由 Wireshark 提前注入，脚本可以直接使用。
3. 执行到 `local my_plugin_proto = Proto("myplugin", "My Plugin Protocol")`：Wireshark 内部创建一个新的协议对象，此时它还没有任何字段、也没有被任何 `DissectorTable` 引用，等价于示例1里"`lua_State` 创建完了，但插件还没调用 `plugin_init`"这个中间状态。
4. 执行 `my_plugin_proto.fields = fields`：把 4 个 `ProtoField` 挂到协议对象上，Wireshark 据此知道这个协议"看起来长什么样"。
5. 定义 `my_plugin_proto.dissector = function(tvb, pinfo, tree) ... end`：这一步只是把一个 Lua 函数赋值给协议对象的一个字段，此时函数还没有被调用过。
6. 执行 `tcp_port_table:add(23456, my_plugin_proto)`：这是真正的 Register 完成时刻——从这一行执行完毕开始，Wireshark 的包分发逻辑知道"TCP 端口 23456 的数据应该交给 `my_plugin_proto.dissector`"。
7. 脚本顶层最后一行 `print(...)` 执行，日志会出现在 Wireshark 的调试控制台（如果开启了 `-g`/`--log-level` 之类的调试输出，或者用 `tshark` 从命令行加载）。
8. 用户开始抓包，一旦某个 TCP 流的端口号匹配 23456，Wireshark 的 C 代码会调用 `my_plugin_proto.dissector(tvb, pinfo, tree)`——这是 Event 阶段的具体体现：每一个匹配的包都是一次"事件"，dissector 函数就是这次事件的处理器。
9. 用户关闭 Wireshark：Lua 环境连同 `my_plugin_proto`、`fields`、`tcp_port_table` 里的这条注册记录一起被销毁，没有任何显式的"卸载"代码被执行——这是 Wireshark Plugin 相对示例1、示例3明显更弱的一环。

## 15. Mental Model

```text
Host（Wireshark 主程序，C/C++）
 |
 +-- Mechanism   : 抓包引擎 / Dissector Framework 主循环 / Packet Details UI
 +-- Resource    : Tvb（包字节）/ Pinfo（包元信息）/ TreeItem（UI 树节点）
 +-- Lifecycle   : Discovery 扫描目录 -> Load 执行脚本；卸载仅在进程退出时隐式发生
 +-- Stable API  : Proto / ProtoField / DissectorTable 等一整套 Binding
          |
          v
         Lua（plugin.lua）
          |
 +-- Behavior    : dissector 函数具体怎么读字段、怎么显示
 +-- Extension   : MyPlugin 协议本身就是给 Wireshark 新增的一种协议解析能力
```

对比示例1的 Mental Model：这里 Lua 侧几乎没有独立维护的 State（本示例是只读常量表），Register 阶段（Proto/Fields/DissectorTable）比 Load 阶段更值得关注，Lifecycle 里 Unload 这一格几乎是空的——这些都是"归类为 Plugin，但更靠近 Extension 一端"的具体证据。

## 16. Why this is this Pattern

Wireshark 的 Lua Plugin 之所以仍然归类为 **Plugin**，而不是退化成 Extension，理由是：它具备独立文件（`plugin.lua` 是一个单独的、可以被复制/删除/替换的文件，而不是写死在 Wireshark 源码里的一段逻辑）、独立注册（`Proto`/`DissectorTable` 提供了完整的身份和挂载点，和其他协议插件互不干扰）、可以被 Wireshark 提供的统一机制发现和管理（`Help -> About Wireshark -> Plugins` 列表统一展示所有已加载的 Lua 插件，`Tools -> Reload Lua Plugins` 统一重新加载所有插件）——数量不固定、身份独立、被统一管理，这三点和示例1的判断标准是一致的。

但同时要诚实地承认：**不是所有 Host 的 Plugin 生命周期都一样完整**。Wireshark 的 Unload 阶段很弱（没有 `plugin_shutdown`，卸载即进程退出或整体重新执行），这是它更偏 Extension 一端的证据之一。可以把 Plugin 生命周期完整度想象成一个连续区间，示例1（自己写的 Manager）在最完整的一端，Wireshark 在中间偏 Extension 一侧，示例3的 Neovim 会展示另一种不完整——生态惯例上很少有人真正实现卸载逻辑，但框架本身是支持的。

**如果 Lua 消失，Host 还能做什么？** Wireshark 内置的、用 C/C++ 写的几百种协议解析器完全不受影响——它们本来就不依赖 Lua。Lua 消失只影响"用户/第三方通过 Lua 脚本自行添加的协议解析能力"这一类插件，Wireshark 核心功能（抓包、过滤、显示已知协议）完全不受影响。这再次印证 Mental Model 的核心结论：Lua 只负责"易变、允许用户自己扩展"的那一部分，不负责 Host 的核心机制。

**如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？** 如果换成 JSON/YAML：只能描述静态的字段布局（类似"偏移量 0-2 是 magic，2-3 是 kind"这种声明式描述），确实有真实工具做到类似效果（例如某些通用二进制协议解析器允许用声明式配置定义定长字段），但**没有办法表达 `dissector` 里 `math.min(payload_len, available)` 这类运行时计算逻辑**，遇到需要条件判断、可变长度字段、嵌套解析的协议就会露出短板——这时候 Plugin 就退化成了纯 Configuration。如果换成纯 C（编译成 `.so`，用类似 `dlopen` 的机制加载），Wireshark 实际上也支持这种"C 语言插件"（真正的 Wireshark Plugin 架构本身既支持 C 插件也支持 Lua 插件），解析性能会更好，但每次修改都要重新编译、需要处理 Wireshark 版本升级带来的 ABI 变化，用户自己写一个协议解析器的门槛会陡增——这正是 Wireshark 选择同时提供 Lua Plugin 路径的原因：用可接受的性能损失换取"改一行 Lua 立即生效"的开发体验。
