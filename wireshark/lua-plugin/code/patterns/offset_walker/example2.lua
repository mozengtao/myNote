-- Pattern: Offset Walker（增强游标：peek(n) 非消费前瞻 + skip(n)）
-- 场景：某个可选字段"是否存在"要靠往前看一个 marker 字节才能判断，
-- 但判断的那一刻还不确定要不要消费它——这时裸 offset 写法很容易在"要不要 +1"上出错。
-- peek(n) 让你先看不动 offset，确认真的是 marker 之后再用 skip(n)/read(n) 正式消费。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.5节阅读）。
--
-- 帧格式（MiniOptional）：
--   offset 0     1字节  type (固定存在)
--   offset 1     可选：如果这个字节恰好是 marker 0xEE，说明后面跟了一个2字节 extension，
--                 消费掉 marker 之后紧跟 extension；否则说明没有 extension，
--                 直接进入下一个固定字段。
--   紧接着       2字节  id (固定存在，无论前面有没有 extension 都在)
--
-- 加载测试：
--   wireshark -X lua_script:example2.lua
-- 不带 extension（type=1, id=999）：
--   python3 -c "import socket,struct; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([1]) + struct.pack('>H', 999), ('127.0.0.1', 40010))"
-- 带 extension（type=1, marker=0xEE, extension=1234, id=999）：
--   python3 -c "import socket,struct; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([1, 0xEE]) + struct.pack('>HH', 1234, 999), ('127.0.0.1', 40010))"

local Cursor = {}
Cursor.__index = Cursor

function Cursor.new(buffer)
    return setmetatable({ buf = buffer, pos = 0 }, Cursor)
end

function Cursor:read(n)
    if self.pos + n > self.buf:len() then
        error(string.format("Cursor:read out of bounds: pos=%d n=%d len=%d", self.pos, n, self.buf:len()))
    end
    local range = self.buf(self.pos, n)
    self.pos = self.pos + n
    return range
end

-- 非消费的前瞻：只看接下来 n 字节是什么，不移动 pos。
-- 用来实现"先看一眼再决定怎么解析"，而不用手写"读了发现不对再退回去"的回滚逻辑。
function Cursor:peek(n)
    if self.pos + n > self.buf:len() then
        error(string.format("Cursor:peek out of bounds: pos=%d n=%d len=%d", self.pos, n, self.buf:len()))
    end
    return self.buf(self.pos, n)
end

-- 跳过 n 字节但不返回 TvbRange，用于"确认要消费但不需要单独展示"的场景（比如 marker 字节）。
function Cursor:skip(n)
    if self.pos + n > self.buf:len() then
        error(string.format("Cursor:skip out of bounds: pos=%d n=%d len=%d", self.pos, n, self.buf:len()))
    end
    self.pos = self.pos + n
end

function Cursor:remaining()
    return self.buf:len() - self.pos
end

local p_optional = Proto("minioptional", "Offset Walker Example 2: Peek/Skip Cursor")
p_optional.prefs.port = Pref.uint("UDP Port", 40010, "UDP port this dissector listens on")

local MARKER = 0xEE
local f_type      = ProtoField.uint8("minioptional.type", "Type", base.DEC)
local f_extension = ProtoField.uint16("minioptional.extension", "Extension", base.DEC)
local f_id        = ProtoField.uint16("minioptional.id", "Id", base.DEC)
p_optional.fields = { f_type, f_extension, f_id }

function p_optional.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = p_optional.name
    local subtree = tree:add(p_optional, buffer())

    local ok, consumed, has_extension = pcall(function()
        local cur = Cursor.new(buffer)
        subtree:add(f_type, cur:read(1))

        local extension_present = cur:remaining() >= 1 and cur:peek(1):uint() == MARKER
        if extension_present then
            cur:skip(1) -- 消费掉 marker 字节本身，不单独建字段展示
            subtree:add(f_extension, cur:read(2))
        end

        subtree:add(f_id, cur:read(2))
        return cur.pos, extension_present
    end)

    if not ok then
        subtree:add_proto_expert_info(nil, "malformed MiniOptional: " .. tostring(consumed))
        return buffer:len()
    end

    pinfo.cols.info:set(has_extension and "MiniOptional (with extension)" or "MiniOptional")
    return consumed
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_optional.prefs.port, p_optional)
