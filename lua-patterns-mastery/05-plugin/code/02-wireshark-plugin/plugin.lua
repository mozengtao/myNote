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
