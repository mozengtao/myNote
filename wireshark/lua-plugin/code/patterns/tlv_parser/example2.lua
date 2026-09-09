-- Pattern: TLV Parser（嵌套 TLV：某个 Type 的 Value 本身又是一段 TLV 序列）
-- 场景：TLV 的 Value 不一定是"原始字节"，也可以是另一段 TLV 序列（Container 类型）。
-- 把"解析一段 TLV 序列"写成一个独立函数 parseTlvSequence()，遇到 Container 类型时
-- 对它的 Value 递归调用自己——这跟 8.8 Recursive Parser 是同一个原理，
-- 区别只是这里的"自相似结构"载体是 TLV 而不是标签序列。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.7节阅读）。
--
-- 帧格式：重复的 [1字节 Type][1字节 Length][Length字节 Value]
--   Type 1 = Name（Value 是字符串）
--   Type 2 = Container（Value 本身又是一段 TLV 序列，可以嵌套 Container）
--   Type 3 = Number（Value 是1字节数字）
--
-- 加载测试：
--   wireshark -X lua_script:example2.lua
-- 构造 Name="A" + Container[ Name="B" + Number=9 ] 两层结构：
--   python3 -c "
--   import socket
--   inner = bytes([1,1])+b'B' + bytes([3,1,9])
--   payload = bytes([1,1])+b'A' + bytes([2,len(inner)])+inner
--   s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
--   s.sendto(payload, ('127.0.0.1', 40014))
--   "

local p_tlv2 = Proto("tlvnested", "TLV Parser Example 2: Nested TLV")
p_tlv2.prefs.port = Pref.uint("UDP Port", 40014, "UDP port this dissector listens on")

local TYPE_NAME, TYPE_CONTAINER, TYPE_NUMBER = 1, 2, 3
local type_names = { [TYPE_NAME] = "Name", [TYPE_CONTAINER] = "Container", [TYPE_NUMBER] = "Number" }

local f_type = ProtoField.uint8("tlvnested.type", "Type", base.DEC)
local f_len  = ProtoField.uint8("tlvnested.len",  "Length", base.DEC)
local f_name = ProtoField.string("tlvnested.name", "Name")
local f_num  = ProtoField.uint8("tlvnested.number", "Number", base.DEC)
p_tlv2.fields = { f_type, f_len, f_name, f_num }

local MAX_DEPTH = 8 -- 递归深度上限，防止畸形/恶意包里的自嵌套 Container 导致栈溢出

-- 独立解析函数：解析 [offset, offset+total) 范围内的一段 TLV 序列，
-- 遇到 Container 就对它的 Value 递归调用自己（depth+1），并把结果挂到一个子 subtree 下。
local function parseTlvSequence(buffer, offset, total, subtree, depth)
    if depth > MAX_DEPTH then
        subtree:add_proto_expert_info(nil, "TLV nesting too deep (>" .. MAX_DEPTH .. ")")
        return
    end

    local end_offset = offset + total
    while offset < end_offset do
        if end_offset - offset < 2 then
            subtree:add_proto_expert_info(nil, "TLV truncated at offset " .. offset)
            return
        end

        local t = buffer(offset, 1):uint()
        local l = buffer(offset + 1, 1):uint()

        if offset + 2 + l > end_offset then
            subtree:add_proto_expert_info(nil, "TLV length overruns container at offset " .. offset)
            return
        end

        local item = subtree:add(buffer(offset, 2 + l),
            string.format("%s (type=%d len=%d)", type_names[t] or "Unknown", t, l))
        item:add(f_type, buffer(offset, 1))
        item:add(f_len,  buffer(offset + 1, 1))

        if t == TYPE_NAME then
            item:add(f_name, buffer(offset + 2, l))
        elseif t == TYPE_NUMBER and l >= 1 then
            item:add(f_num, buffer(offset + 2, 1))
        elseif t == TYPE_CONTAINER then
            -- 关键一步：Value 部分不是原始字节，而是另一段 TLV 序列，递归解析它
            parseTlvSequence(buffer, offset + 2, l, item, depth + 1)
        end

        offset = offset + 2 + l
    end
end

function p_tlv2.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = p_tlv2.name
    local subtree = tree:add(p_tlv2, buffer())

    local ok, err = pcall(parseTlvSequence, buffer, 0, buffer:len(), subtree, 1)
    if not ok then
        subtree:add_proto_expert_info(nil, "malformed nested TLV: " .. tostring(err))
    end

    pinfo.cols.info:set("Nested TLV")
    return buffer:len()
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_tlv2.prefs.port, p_tlv2)
