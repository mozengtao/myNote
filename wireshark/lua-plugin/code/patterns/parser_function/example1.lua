-- Pattern: Parser Pattern（独立解析函数，返回多个值）
-- 场景：帧头本身不复杂，但把"怎么解析头部"从主 dissector() 里拆出来，
-- 独立成一个函数、一次性返回多个解析结果 + 消费的字节数，方便复用/单独阅读。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.2节阅读）。
--
-- 帧格式（MiniFrame）：
--   offset 0    1字节  version
--   offset 1    1字节  flags
--   offset 2..3 2字节  payload length (big-endian uint16)
--   offset 4..  N字节  payload（长度由上面的 length 字段决定）
--
-- 加载测试：
--   wireshark -X lua_script:example1.lua
-- 构造 version=1 flags=0x02 length=3 payload="abc" 的包，发到 UDP 40003：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([1, 2, 0, 3]) + b'abc', ('127.0.0.1', 40003))"

local p_miniframe = Proto("miniframe", "Parser Pattern Example 1: MiniFrame Header")
p_miniframe.prefs.port = Pref.uint("UDP Port", 40003, "UDP port this dissector listens on")

local f_version = ProtoField.uint8("miniframe.version", "Version", base.DEC)
local f_flags   = ProtoField.uint8("miniframe.flags", "Flags", base.HEX)
local f_length  = ProtoField.uint16("miniframe.length", "Payload Length", base.DEC)
local f_payload = ProtoField.bytes("miniframe.payload", "Payload")
p_miniframe.fields = { f_version, f_flags, f_length, f_payload }

-- 独立解析函数：只负责"帧头怎么解析"，不关心调用方是谁、payload 怎么处理。
-- 返回多个值（version, flags, length, consumed），主 dissector 拿到这些值后
-- 决定接下来做什么——这正是 Parser Pattern 跟 Table-Driven 的区别：
-- 这里字段之间有"length 决定 payload 该读多长"的依赖，没法只靠一张静态表描述。
local function parseHeader(buffer, subtree)
    local version = buffer(0, 1):uint()
    local flags   = buffer(1, 1):uint()
    local length  = buffer(2, 2):uint()

    subtree:add(f_version, buffer(0, 1))
    subtree:add(f_flags, buffer(1, 1))
    subtree:add(f_length, buffer(2, 2))

    return version, flags, length, 4
end

function p_miniframe.dissector(buffer, pinfo, tree)
    if buffer:len() < 4 then return 0 end

    pinfo.cols.protocol = p_miniframe.name
    local subtree = tree:add(p_miniframe, buffer())

    local version, flags, length, consumed = parseHeader(buffer, subtree)
    pinfo.cols.info:set(string.format("MiniFrame v%d flags=0x%02x len=%d", version, flags, length))

    if buffer:len() < consumed + length then
        subtree:add_proto_expert_info(nil, "payload shorter than declared length")
        return consumed
    end

    if length > 0 then
        subtree:add(f_payload, buffer(consumed, length))
    end

    return consumed + length
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_miniframe.prefs.port, p_miniframe)
