-- Pattern: Table-Driven Parsing（固定顺序字段描述表）
-- 场景：协议头部是"一串固定长度字段的顺序排列"，字段之间没有条件依赖，
-- 用一张 { {field=f, len=n}, ... } 的表 + 一个通用循环去消费，而不是手写4行 tree:add。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.3节阅读）。
--
-- 帧格式（SensorFrameHeader，固定7字节）：
--   offset 0    1字节  type
--   offset 1..2 2字节  seq (big-endian uint16)
--   offset 3..4 2字节  length (big-endian uint16)
--   offset 5..6 2字节  reserved
--
-- 加载测试：
--   wireshark -X lua_script:example1.lua
-- 构造 type=7 seq=100 length=0 reserved=0 的包，发到 UDP 40005：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([7, 0,100, 0,0, 0,0]), ('127.0.0.1', 40005))"

local p_tdheader = Proto("tdheader", "Table-Driven Parsing Example 1: Fixed Header")
p_tdheader.prefs.port = Pref.uint("UDP Port", 40005, "UDP port this dissector listens on")

local f_type     = ProtoField.uint8("tdheader.type", "Type", base.DEC)
local f_seq      = ProtoField.uint16("tdheader.seq", "Sequence", base.DEC)
local f_length   = ProtoField.uint16("tdheader.length", "Length", base.DEC)
local f_reserved = ProtoField.uint16("tdheader.reserved", "Reserved", base.HEX)
p_tdheader.fields = { f_type, f_seq, f_length, f_reserved }

-- 描述表：每一项只说"用哪个 field、占几个字节"，offset 由循环自己累加。
-- 新增/调整字段只改这张表，不用碰 dissector() 里的逻辑；
-- 而且这张表本身就是协议头部布局的文档，一眼能看出总共有几个字段、各自多长。
local layout = {
    { field = f_type,     len = 1 },
    { field = f_seq,      len = 2 },
    { field = f_length,   len = 2 },
    { field = f_reserved, len = 2 },
}

function p_tdheader.dissector(buffer, pinfo, tree)
    local total_header_len = 0
    for _, item in ipairs(layout) do
        total_header_len = total_header_len + item.len
    end

    if buffer:len() < total_header_len then return 0 end

    pinfo.cols.protocol = p_tdheader.name
    local subtree = tree:add(p_tdheader, buffer())

    local offset = 0
    for _, item in ipairs(layout) do
        subtree:add(item.field, buffer(offset, item.len))
        offset = offset + item.len
    end

    pinfo.cols.info:set(string.format("SensorFrameHeader type=%d seq=%d",
        buffer(0, 1):uint(), buffer(1, 2):uint()))

    return offset
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_tdheader.prefs.port, p_tdheader)
