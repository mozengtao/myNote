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
