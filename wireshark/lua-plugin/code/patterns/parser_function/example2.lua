-- Pattern: Parser Pattern（独立解析函数，处理变长字段并可重复调用）
-- 场景：一种"Pascal 字符串"（1字节长度 + N字节内容）在同一个协议里出现了两次
-- （姓名 + 头衔）。把"怎么解析一个 Pascal 字符串"拆成独立函数，两处都复用它，
-- 而不是写两遍几乎一样的代码。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.2节阅读）。
--
-- 帧格式（BusinessCard）：
--   offset 0     1字节  name 的长度 Lname
--   offset 1..   Lname 字节  name 内容
--   紧接着       1字节  title 的长度 Ltitle
--   紧接着       Ltitle 字节  title 内容
--
-- 加载测试：
--   wireshark -X lua_script:example2.lua
-- 构造 name="Ada" title="Engineer" 的包，发到 UDP 40004：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       name=b'Ada'; title=b'Engineer'; \
--       s.sendto(bytes([len(name)])+name+bytes([len(title)])+title, ('127.0.0.1', 40004))"

local p_bizcard = Proto("bizcard", "Parser Pattern Example 2: Business Card")
p_bizcard.prefs.port = Pref.uint("UDP Port", 40004, "UDP port this dissector listens on")

local f_name  = ProtoField.string("bizcard.name", "Name")
local f_title = ProtoField.string("bizcard.title", "Title")
p_bizcard.fields = { f_name, f_title }

-- 独立解析函数：解析"1字节长度 + N字节内容"这一种可重复出现的结构，
-- 返回 (字符串内容, 消费的字节数)，调用方不需要知道内部怎么切片。
-- 如果调用方传入的 field 不同，还能用同一份逻辑生成不同的 ProtoField 展示。
local function parsePascalString(buffer, offset, subtree, field)
    if offset + 1 > buffer:len() then
        error("parsePascalString: not enough bytes for length prefix at offset " .. offset)
    end

    local len = buffer(offset, 1):uint()
    if offset + 1 + len > buffer:len() then
        error(string.format("parsePascalString: declared len=%d overruns buffer at offset %d", len, offset))
    end

    local str = buffer(offset + 1, len):string()
    subtree:add(field, buffer(offset, 1 + len), str)
    return str, 1 + len
end

function p_bizcard.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = p_bizcard.name
    local subtree = tree:add(p_bizcard, buffer())

    -- 用 pcall 兜底：畸形长度会让 parsePascalString 主动 error()，
    -- 不用 pcall 包住的话会导致整个 dissector 崩溃退出。
    local ok, err = pcall(function()
        local offset = 0
        local name, consumed1 = parsePascalString(buffer, offset, subtree, f_name)
        offset = offset + consumed1

        local title, consumed2 = parsePascalString(buffer, offset, subtree, f_title)
        offset = offset + consumed2

        pinfo.cols.info:set(string.format("%s, %s", name, title))
    end)

    if not ok then
        subtree:add_proto_expert_info(nil, "malformed business card: " .. tostring(err))
    end

    return buffer:len()
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_bizcard.prefs.port, p_bizcard)
