# 01-2 · 示例：Wireshark Lua Dissector

> Pattern：Extension（参见 [00-why-and-architecture.md](00-why-and-architecture.md)）。
> 完整代码：[code/02-wireshark-dissector/mini_extension.lua](code/02-wireshark-dissector/mini_extension.lua)。

## 1. Problem

Wireshark 原生认识几百种协议，但永远不会认识我们自己发明的一个玩具协议"MyProto"。如果想让 Wireshark 能够把 MyProto 报文的字段（magic / type / length / payload）解析成 Packet Details 树里可读的结构，并且可以用 Filter 表达式（例如 `myproto.type == 1`）过滤，标准做法是给 Wireshark 增加一个新的 C dissector 插件——但这要求下载 Wireshark 源码、写 C 代码、重新编译整个 Wireshark。

我们希望的效果是：不改 Wireshark 一行 C 代码、不重新编译，只丢一个 `.lua` 文件进去，Wireshark 就"学会"解析 MyProto。

## 2. Why Lua

- Wireshark 从很早的版本就内置了 Lua 解释器，并且专门为"写协议 dissector"设计了一套 API（`Proto`、`ProtoField`、`Tvb`、`Pinfo`、`TreeItem`、`DissectorTable`），这套 API 本身就是 Extension Pattern 的 Stable Core API；
- 协议格式经常需要调试、试错（字段偏移量、类型编号都可能一开始就写错），用 Lua 写可以改完直接 `Ctrl+Shift+L` 重新加载脚本验证，不需要重新编译 Wireshark 这个几百万行的 C++ 项目；
- 协议解析逻辑本质上是"读一段字节，翻译成有意义的字段"，这是纯计算、无副作用（除了往 Wireshark 的树结构里追加节点）的逻辑，天然适合用一个可以被 Wireshark 主循环安全 `pcall` 的解释器承载——脚本写错顶多这个协议解析失败，不会让 Wireshark 崩溃。

## 3. Architecture

```text
Wireshark Core
      |
      v
Dissector Framework
      |
      v
Lua Proto
      |
      v
dissector()
      |
      +-- Tvb
      +-- Pinfo
      +-- TreeItem
```

- `Wireshark Core`：捕获/读取原始数据包的机制层（对应 Host 的 Mechanism/Resource）；
- `Dissector Framework`：Wireshark 内部"根据端口/协议特征把数据包分发给某个 dissector"的调度层，这一层本身是 C++ 写的，是 Host 的 Stable Core API 的具体形态；
- `Lua Proto`：`mini_extension.lua` 里用 `Proto("myproto", ...)` 创建的对象，代表"MyProto 这个协议"在 Wireshark 里的注册身份；
- `dissector()`：真正执行解析逻辑的函数，被 Dissector Framework 在匹配到相应数据时调用；
- `Tvb` / `Pinfo` / `TreeItem`：Host 传给 `dissector()` 的三个句柄，分别代表"只读字节视图"、"包元信息（可写）"、"Packet Details 树节点（可写）"——这三个正是 Lua 可以访问、但不能直接访问底层 C++ 内存的边界代理。

## 4. Host responsibilities

- 维护整个 `tcp.port` 分发表（`DissectorTable`），根据目的/源端口号决定把一个 TCP 包交给谁解析；
- 在识别到某个 dissector 已注册到某端口后，把该包的字节数据包装成 `Tvb`、元信息包装成 `Pinfo`、把当前 Packet Details 树的根节点作为 `tree` 参数，调用 Lua 的 `dissector()` 函数；
- 提供 `Proto`、`ProtoField`、`DissectorTable` 等一整套稳定的 Lua API，让 Lua 侧能声明字段、注册协议、挂载到分发表；
- 把 `dissector()` 返回的"消费字节数"用于后续分包/多重协议叠加的处理；
- 用 `pcall` 语义包裹对 Lua `dissector()` 的调用，脚本运行时报错时只在状态栏/日志里提示，不会导致 Wireshark 进程崩溃。

## 5. Lua responsibilities

- 用 `Proto(...)` 声明协议本身，用 `ProtoField.*` 声明每个字段的名字、显示格式（`base.HEX`/`base.DEC`）、以及 Filter 表达式名（`myproto.magic` 等）；
- 实现 `dissector(tvb, pinfo, tree)`：读取 `tvb` 里的字节、判断 magic 是否匹配、把各字段的解析结果通过 `tree:add(...)` 写进 Packet Details 树、通过 `pinfo.cols.*` 设置包列表里的列显示；
- 用 `DissectorTable.get("tcp.port"):add(12345, my_proto)` 把自己主动挂载到 Wireshark 已有的分发表上，声明"TCP 端口 12345 上的流量请交给我解析"；
- 返回实际消费的字节数，告诉 Host 这次解析处理了多少数据。

## 6. Lifecycle

```text
Wireshark 启动
  |
  v
扫描 Lua 脚本目录 / -X lua_script 参数
  |
  v
加载并执行 mini_extension.lua（一次）
  |
  v
Proto("myproto", ...) 创建协议对象
  |
  v
DissectorTable:add(12345, my_proto)  <-- 注册进分发表
  |
  v
Wireshark 进入正常抓包/读取 pcap 流程
  |
  v
遇到 TCP 目的或源端口 = 12345 的包
  |
  v
Dissector Framework 调用 my_proto.dissector(tvb, pinfo, tree)
  |
  v
Lua 解析并写入 Packet Details 树
  |
  v
Wireshark 关闭 / 脚本被移除时卸载
```

## 7. Data Flow（C -> Lua）

Wireshark 核心（C/C++）在识别到一个包命中 TCP 端口 12345 后：

1. 把该包的原始字节封装成一个 `Tvb` 对象（这是对底层字节 buffer 的**只读代理**，Lua 拿不到裸指针，只能通过 `tvb(offset, len)` 这样的接口切片读取）；
2. 把包的元信息（时间戳、地址、端口、当前协议列等）封装成 `Pinfo`；
3. 把当前正在构建的 Packet Details 树的根节点作为 `tree`（一个 `TreeItem`）；
4. 用这三个参数调用 Lua 的 `my_proto.dissector(tvb, pinfo, tree)`。

在 `dissector()` 内部：

```lua
local magic = tvb(0, 2):uint()
local msg_type = tvb(2, 1):uint()
local payload_len = tvb(3, 2):uint()
```

这几行就是"C 的字节数据流向 Lua"的具体体现：`tvb(0,2):uint()` 把原始字节按大端读成一个 Lua number，Lua 完全不需要知道这段数据在 C++ 内存里是怎么存储的。

## 8. Control Flow（Lua -> C）

Lua 反过来影响 Host（Wireshark）的展示，通过下面这几类调用：

- `pinfo.cols.protocol = my_proto.name`：修改包列表 Protocol 列的显示文本；
- `pinfo.cols.info = "..."`：修改包列表 Info 列的显示文本；
- `tree:add(my_proto, tvb(), "...")`：在 Packet Details 树里新增一个子树节点，返回值 `subtree` 可以继续往下挂子节点；
- `subtree:add(fields.magic, tvb(0, 2))`：把某个字段的解析结果，绑定进树节点，同时这个字段会自动出现在 Filter 语法里（`myproto.magic`）；
- `return HEADER_LEN + actual_payload_len`：这是 Lua 唯一的"返回值"通道，告诉 Host 这次一共消费了多少字节，供后续可能的多协议叠加或分片判断使用。

**注意**：`tree`/`pinfo`/`tvb` 都是 Host 传入的对象引用，Lua 通过调用它们的方法间接修改 Host 内部状态，而不是"返回一份数据让 Host 自己去改"——这是一种"传引用、原地修改"式的 Control Flow，和示例 1（C 命令行）里"返回值 + pcall"的模式略有不同，但本质仍然是 Lua 在被调用期间通过 Host 暴露的 API 影响 Host。

## 9. State

- 本示例的 `dissector()` 是无状态的：每次调用都从 `tvb` 重新读取，不依赖任何跨包保留的 Lua 变量；
- 如果要处理跨包状态（例如统计"MyProto 一共出现了多少次 REQUEST"），可以在脚本顶层加一个局部变量当作计数器，它的生命周期跟随整个 Lua 解释器实例，从 Wireshark 启动加载脚本开始，一直存活到脚本被重新加载或 Wireshark 关闭；
- `TYPE_NAMES` 这张编号到名称的映射表，是 Extension 自己维护的静态数据，不属于 Wireshark 核心，体现"Lua 侧维护自己需要的辅助数据"。

## 10. Failure

- **不是 MyProto 报文**：`magic ~= MAGIC_VALUE` 时，`dissector()` `return 0`，明确告诉 Dissector Framework"这个包我解析不了/不归我管"，Wireshark 会尝试其他候选 dissector 或标记为未识别数据，不是错误，是正常的"拒绝接管"；
- **数据不足一个完整头部**：`tvb:len() < HEADER_LEN` 时同样 `return 0`，简化点：真实实现通常会用 `DESEGMENT_ONE_MORE_SEGMENT` 之类的机制请求 Wireshark 等待更多字节做分片重组，本示例为了聚焦 Extension Pattern 本身，选择直接放弃解析；
- **脚本本身有语法/运行时错误**：Wireshark 加载 Lua 脚本和调用 `dissector()` 都在其内部的 Lua 引擎 `pcall` 保护之下，脚本报错只会在状态栏或 Lua 控制台里出现红色错误提示，不会导致 Wireshark 进程崩溃，但那个包会解析失败/显示为 Malformed。

## 11. Complete Host Code

本示例没有独立的"Host 源码"——Host 就是 Wireshark 本体（闭源/超大型 C++ 项目），我们不重新贴出它的源码。Host 侧的角色由 Wireshark 内置的 Dissector Framework 承担，其行为已经在第 3~8 节完整描述。

## 12. Complete Lua Code

与 [code/02-wireshark-dissector/mini_extension.lua](code/02-wireshark-dissector/mini_extension.lua) 完全一致：

```lua
-- mini_extension.lua
--
-- 一个简化但真实可加载的 Wireshark Lua Dissector Extension。
-- 目标：给 Wireshark 增加"能解析 MyProto 协议"这个全新能力。
--
-- 玩具协议 MyProto 的报文格式（大端）：
--
--   +----------------+----------------+------------------+-----------------+
--   | magic (2 bytes)| type (1 byte)  | length (2 bytes)  | payload (N)     |
--   +----------------+----------------+------------------+-----------------+
--
-- magic   : 固定值 0xCAFE，用来快速识别这是不是 MyProto 报文
-- type    : 消息类型，1 = REQUEST, 2 = RESPONSE，其余 = UNKNOWN
-- length  : payload 部分的字节长度
-- payload : 长度为 length 的数据体，这里简单显示为字节流

-- 简化点：真实协议解析往往要处理分片重组（TCP desegmentation）、
-- 校验和验证、多种子类型的递归解析；这里为了聚焦 Extension Pattern
-- 本身，只做最基础的单包、定长头部解析，不处理跨包重组。

------------------------------------------------------------
-- 1. 声明协议：Proto 是 Extension 挂进 Wireshark 的"入口对象"
------------------------------------------------------------
local my_proto = Proto("myproto", "My Toy Protocol")

------------------------------------------------------------
-- 2. 声明字段：ProtoField 决定 Packet Details 树里显示什么、
--    Filter 栏里可以用什么表达式过滤（例如 myproto.type == 1）
------------------------------------------------------------
local fields = {
    magic   = ProtoField.uint16("myproto.magic",   "Magic",   base.HEX),
    type    = ProtoField.uint8("myproto.type",     "Type",    base.DEC),
    length  = ProtoField.uint16("myproto.length",  "Length",  base.DEC),
    payload = ProtoField.bytes("myproto.payload",  "Payload"),
}
my_proto.fields = fields

-- 消息类型编号 -> 可读名称，仅用于 Packet Details 里显示得更友好
local TYPE_NAMES = {
    [1] = "REQUEST",
    [2] = "RESPONSE",
}

local MAGIC_VALUE = 0xCAFE
local HEADER_LEN = 5 -- magic(2) + type(1) + length(2)

------------------------------------------------------------
-- 3. dissector 函数：Wireshark Dissector Framework 对每一个
--    命中该协议的包都会调用它一次。
--
--    参数:
--      tvb   - Tvb，对当前包原始字节的只读视图
--      pinfo - Pinfo，包的元信息（可读写，例如设置 Column 显示文本）
--      tree  - TreeItem，Packet Details 树的根节点，dissector 往里
--              添加子节点来展示解析出的字段
------------------------------------------------------------
function my_proto.dissector(tvb, pinfo, tree)
    local len = tvb:len()
    if len < HEADER_LEN then
        -- 数据不够一个完整头部，交回给 Wireshark 判断（这里简单地不处理）
        return 0
    end

    local magic = tvb(0, 2):uint()
    if magic ~= MAGIC_VALUE then
        -- 不是 MyProto 报文，返回 0 告诉框架"我没能识别这个包"
        return 0
    end

    -- 3.1 设置 Column 里的 Protocol 列显示为 MYPROTO
    pinfo.cols.protocol = my_proto.name

    local msg_type = tvb(2, 1):uint()
    local payload_len = tvb(3, 2):uint()
    local type_name = TYPE_NAMES[msg_type] or "UNKNOWN"

    -- 3.2 设置 Info 列，方便在包列表里一眼看到这是什么消息
    pinfo.cols.info = string.format("MyProto %s (type=%d, len=%d)", type_name, msg_type, payload_len)

    -- 3.3 往 Packet Details 树里添加一个 "My Toy Protocol" 子树
    local subtree = tree:add(my_proto, tvb(), "My Toy Protocol, Type: " .. type_name)

    subtree:add(fields.magic, tvb(0, 2))
    subtree:add(fields.type, tvb(2, 1)):append_text(" (" .. type_name .. ")")
    subtree:add(fields.length, tvb(3, 2))

    -- 3.4 payload：按 length 字段声明的长度截取剩余字节
    local available = len - HEADER_LEN
    local actual_payload_len = math.min(payload_len, available)
    if actual_payload_len > 0 then
        subtree:add(fields.payload, tvb(HEADER_LEN, actual_payload_len))
    end

    -- 返回本 dissector 实际消费的字节数
    return HEADER_LEN + actual_payload_len
end

------------------------------------------------------------
-- 4. 挂载：把 my_proto 注册进已有的 DissectorTable，
--    告诉 Wireshark "TCP 端口 12345 上的数据请交给 my_proto 解析"。
--
--    这一步就是 Extension Pattern 里"Extension 主动注册进 Host"
--    的体现：Host（Wireshark）本身完全不知道 MyProto 的存在，
--    是这段 Lua 脚本主动把自己挂进了 tcp.port 这张分发表。
------------------------------------------------------------
local tcp_port_table = DissectorTable.get("tcp.port")
tcp_port_table:add(12345, my_proto)

print("[mini_extension.lua] MyProto dissector 已注册到 TCP 端口 12345")
```

## 13. Execution

两种加载方式：

**方式一：临时加载（调试常用）**

```bash
wireshark -X lua_script:mini_extension.lua
```

**方式二：放进 Personal Lua Plugins 目录（长期生效）**

1. 打开 Wireshark，`Help -> About Wireshark -> Folders`，找到 "Personal Lua Plugins" 对应的目录；
2. 把 `mini_extension.lua` 复制进这个目录；
3. 重启 Wireshark，或用菜单 `Analyze -> Reload Lua Plugins`（部分版本是 `Ctrl+Shift+L`）即可重新加载脚本，不需要重启整个程序。

**构造测试字节流的思路**（不要求真的生成 pcap 文件）：

- 可以用 `text2pcap`（Wireshark 自带的小工具）把一段十六进制文本转换成 `.pcap` 文件，思路是：先手写一段十六进制字节，构造一个"IP+TCP 头部（目的端口 12345）+ MyProto 报文体"的完整帧，再用 `text2pcap` 转成 pcap；
- MyProto 报文体本身可以手写为：`CA FE 01 00 03 AA BB CC`（magic=0xCAFE，type=1=REQUEST，length=3，payload=`AA BB CC`）；
- 生成好 pcap 后用 `wireshark -X lua_script:mini_extension.lua sample.pcap` 打开；
- 验证方法：
  - 在 Packet Details 面板里应该能看到一个新的 "My Toy Protocol, Type: REQUEST" 子树，展开后能看到 Magic/Type/Length/Payload 四个字段；
  - 在包列表的 Protocol 列应显示 `MYPROTO`，Info 列显示形如 `MyProto REQUEST (type=1, len=3)`；
  - 在 Filter 栏输入 `myproto.type == 1` 应该能正确过滤出这个包（如果输入 `myproto.type == 2` 应该被过滤掉），这就验证了 `ProtoField` 声明的字段确实被注册进了 Wireshark 的过滤语法。

## 14. Step-by-step walkthrough

以一个字节序列 `CA FE 01 00 03 AA BB CC`（作为某个 TCP 端口 12345 报文的 payload）为例：

1. Wireshark Core 完成 IP/TCP 层解析后，发现该 TCP 段的端口是 12345，查 `tcp.port` 这张 `DissectorTable`，命中我们注册的 `my_proto`；
2. Dissector Framework 把这段字节封装成 `Tvb`，调用 `my_proto.dissector(tvb, pinfo, tree)`；
3. `tvb:len()` 返回 8，大于 `HEADER_LEN`(5)，继续；
4. `tvb(0,2):uint()` 读到 `0xCAFE`，等于 `MAGIC_VALUE`，确认是 MyProto；
5. `pinfo.cols.protocol` 被设置为 `"MYPROTO"`；
6. `tvb(2,1):uint()` 读到 `1`，`TYPE_NAMES[1]` = `"REQUEST"`；
7. `tvb(3,2):uint()` 读到 `3`（payload 声明长度为 3 字节）；
8. `pinfo.cols.info` 被设置为 `"MyProto REQUEST (type=1, len=3)"`；
9. `tree:add(...)` 创建子树 "My Toy Protocol, Type: REQUEST"，随后依次把 magic/type/length 三个字段挂进子树；
10. `available = 8 - 5 = 3`，`actual_payload_len = min(3, 3) = 3`，`tvb(5,3)` 恰好是 `AA BB CC`，作为 payload 字段挂进子树；
11. `return 5 + 3 = 8`，告诉 Host 这次一共消费了 8 字节（正好是整包）；
12. 用户在 Wireshark 界面里点开这个包，就能在 Packet Details 里看到完整的四个字段，在 Filter 栏输入 `myproto.type == 1` 能命中这个包。

## 15. Mental Model

```text
Host(Wireshark) = "一台海关分拣机 + 一堵挂满标签的墙(tcp.port 表)"
Lua(dissector)  = "一个懂某种特定包裹格式的检查员，把自己的名字贴上墙"

分拣机自己不懂 MyProto 格式，
它只负责："看到目的地是 12345 号窗口，就把包裹交给墙上登记的那个检查员。"
```

## 16. Why this is this Pattern

这是 Extension，而不是简单的 Configuration Pattern，因为：

- 如果这只是 Configuration，Lua 应该只是"提供参数"——比如告诉 Wireshark "TCP 端口 12345 叫 MyProto"，具体怎么解析字段仍然要 Wireshark 自带的通用逻辑去做。但这里完全不是这样：**是否存在这样一种协议、它的字段边界在哪、每个字节该被解释成什么，全部是 Lua 脚本里凭空定义出来的新知识**，Wireshark 原本对 MyProto 一无所知；
- `dissector()` 函数本身就是一段"新增的行为"——它新增了"Wireshark 能够读懂 MyProto 字节流"这个全新能力，而不是在 Wireshark 已有的、针对某个通用协议的解析行为之间做参数微调；
- `DissectorTable:add(12345, my_proto)` 这一步，正是 00 篇里"Extension 主动注册进 Host"的具体体现——Lua 主动把自己的名字贴进 Host 维护的分发表，而不是被动等待 Host 询问"你有什么配置参数"。

**如果 Lua 消失，Host 还能做什么？**
Wireshark 依然能捕获、能按 IP/TCP/UDP 等内置协议解析出网络层和传输层字段，依然能显示"端口 12345 上有一段不认识的数据"（标记为未解析的 TCP payload）——但它永远学不会"这段数据其实是 MyProto，字段应该这样切分"。协议解析这个具体能力完全随 Lua 消失而消失，Mechanism（抓包、TCP 重组框架）不受影响。

**如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？**
- 换成 JSON/YAML：不成立。协议解析是"按字节偏移做条件判断、做数值转换、做格式化字符串"的过程性逻辑，JSON/YAML 只能声明式地描述"字段名+偏移量+类型"这种规整的、无分支的结构（类似 Wireshark 的 `Decode As` 通用字段视图），一旦出现"根据 type 字段的值决定后续字段怎么解析"这种条件分支，JSON/YAML 就表达不了了，除非退化成"只做最简单的定长字段展示"，那已经不是完整意义上的 Extension；
- 换成纯 C：Pattern 本身成立（Wireshark 官方大多数协议 dissector 本来就是 C 写的插件），但代价是必须重新编译整个 Wireshark（或者至少重新编译并正确链接一个符合 ABI 的插件 `.so`），失去了"改一行 Lua、`Ctrl+Shift+L` 立即生效"的快速迭代能力，也失去了"脚本写错只影响这一个协议解析、不影响 Wireshark 进程稳定性"的安全边界。
