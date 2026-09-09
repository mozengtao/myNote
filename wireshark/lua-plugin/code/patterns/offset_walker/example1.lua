-- Pattern: Offset Walker（基础游标：read(n) 自动前进 + 越界检查）
-- 场景：把"当前读到哪里了"包成一个 Cursor 对象，而不是裸的局部变量 offset，
-- offset 算术集中到 Cursor:read() 内部一处，不用在每个字段后面都手写 offset = offset + n。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.5节阅读）。
--
-- 帧格式（CursorFrame，固定6字节）：
--   offset 0..1  2字节  a (uint16)
--   offset 2..5  4字节  b (uint32)
--
-- 加载测试：
--   wireshark -X lua_script:example1.lua
--   python3 -c "import socket,struct; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(struct.pack('>HI', 1000, 70000), ('127.0.0.1', 40009))"

local Cursor = {}
Cursor.__index = Cursor

function Cursor.new(buffer)
    return setmetatable({ buf = buffer, pos = 0 }, Cursor)
end

-- 读 n 字节的 TvbRange 并让游标前进 n；越界时抛出 Lua error，
-- 调用方应该用 pcall 包住整个 dissector 主体来兜底（见 12-common-pitfalls.md）。
function Cursor:read(n)
    if self.pos + n > self.buf:len() then
        error(string.format("Cursor:read out of bounds: pos=%d n=%d len=%d",
            self.pos, n, self.buf:len()))
    end
    local range = self.buf(self.pos, n)
    self.pos = self.pos + n
    return range
end

function Cursor:remaining()
    return self.buf:len() - self.pos
end

local p_cursor = Proto("cursorframe", "Offset Walker Example 1: Basic Cursor")
p_cursor.prefs.port = Pref.uint("UDP Port", 40009, "UDP port this dissector listens on")

local f_a = ProtoField.uint16("cursorframe.a", "A", base.DEC)
local f_b = ProtoField.uint32("cursorframe.b", "B", base.DEC)
p_cursor.fields = { f_a, f_b }

function p_cursor.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = p_cursor.name
    local subtree = tree:add(p_cursor, buffer())

    local ok, result_or_err = pcall(function()
        local cur = Cursor.new(buffer)
        subtree:add(f_a, cur:read(2))
        subtree:add(f_b, cur:read(4))
        return cur.pos
    end)

    if not ok then
        subtree:add_proto_expert_info(nil, "malformed CursorFrame: " .. tostring(result_or_err))
        return buffer:len()
    end

    pinfo.cols.info:set("CursorFrame")
    return result_or_err
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_cursor.prefs.port, p_cursor)
