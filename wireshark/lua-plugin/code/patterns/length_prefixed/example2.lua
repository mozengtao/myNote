-- Pattern: Length-Prefixed Parser（重复的长度前缀数组：count + N个[len+string]）
-- 场景：Length-Prefixed 不止用于单个字段，也常用于"变长数组"：先读一个 count，
-- 再循环 count 次，每次都是"读1字节长度 -> 据此读内容"，跟 8.7 TLV 的区别是
-- 这里没有 Type，纯粹是"变长字符串的重复出现"。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.6节阅读）。
--
-- 帧格式（MultiString）：
--   offset 0    1字节  count (字符串条数)
--   重复 count 次：
--     1字节  len
--     len 字节  字符串内容
--
-- 加载测试：
--   wireshark -X lua_script:example2.lua
-- 发送 count=2 的两条字符串 "foo","bar"：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       payload = bytes([2, 3])+b'foo'+bytes([3])+b'bar'; \
--       s.sendto(payload, ('127.0.0.1', 40012))"
-- 故意把 count 写成比实际条数多，测越界检测分支：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([5, 3])+b'foo', ('127.0.0.1', 40012))"

local p_multi = Proto("multistring", "Length-Prefixed Parser Example 2: Repeated Array")
p_multi.prefs.port = Pref.uint("UDP Port", 40012, "UDP port this dissector listens on")

local f_count = ProtoField.uint8("multistring.count", "Count", base.DEC)
local f_item  = ProtoField.string("multistring.item", "Item")
p_multi.fields = { f_count, f_item }

function p_multi.dissector(buffer, pinfo, tree)
    if buffer:len() < 1 then return 0 end

    pinfo.cols.protocol = p_multi.name
    local subtree = tree:add(p_multi, buffer())

    local count = buffer(0, 1):uint()
    subtree:add(f_count, buffer(0, 1))

    local offset = 1
    local items = {}

    for i = 1, count do
        -- 每一轮都是标准的 Length-Prefixed 步骤：先确认长度字段本身够不够，再确认内容够不够。
        if buffer:len() < offset + 1 then
            subtree:add_proto_expert_info(nil,
                string.format("truncated before item %d/%d length byte", i, count))
            break
        end

        local len = buffer(offset, 1):uint()
        if buffer:len() < offset + 1 + len then
            subtree:add_proto_expert_info(nil,
                string.format("item %d/%d declared len=%d overruns packet", i, count, len))
            break
        end

        local item_range = buffer(offset, 1 + len)
        local str = buffer(offset + 1, len):string()
        subtree:add(f_item, item_range, str)
        items[#items + 1] = str

        offset = offset + 1 + len
    end

    pinfo.cols.info:set("MultiString [" .. table.concat(items, ", ") .. "]")
    return offset
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_multi.prefs.port, p_multi)
