-- Pattern: TLV Parser（Type-Length-Value 通用解析循环，扁平结构）
-- 场景：协议由"重复出现的 [Type][Length][Value]"组成，事先不知道会出现多少个，
-- 用一个通用的 while 循环处理任意数量的 TLV 项，新增一种 Type 只需要加一个分支。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.7节阅读）。
--
-- 帧格式：重复的 [1字节 Type][1字节 Length][Length字节 Value]
--   Type 1 = Option-A（Value 是任意字节）
--   Type 2 = Option-B（Value 是任意字节）
--
-- 加载测试：
--   wireshark -X lua_script:example1.lua
-- 发送两个 TLV：Type=1 Len=2 Value=[0xAA,0xBB]，Type=2 Len=1 Value=[0x01]
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([1,2,0xAA,0xBB, 2,1,0x01]), ('127.0.0.1', 40013))"
-- 故意把某个 Length 写得比剩余字节还大，测越界检测分支：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([1,200,0xAA,0xBB]), ('127.0.0.1', 40013))"

local p_tlv = Proto("tlvflat", "TLV Parser Example 1: Flat TLV")
p_tlv.prefs.port = Pref.uint("UDP Port", 40013, "UDP port this dissector listens on")

local f_type = ProtoField.uint8("tlvflat.type",  "Type",  base.DEC)
local f_len  = ProtoField.uint8("tlvflat.len",   "Length", base.DEC)
local f_val  = ProtoField.bytes("tlvflat.value",  "Value")
p_tlv.fields = { f_type, f_len, f_val }

-- type_names: 把数字类型翻译成可读名字，用于 tree:add 里的展示
local type_names = { [1] = "Option-A", [2] = "Option-B" }

function p_tlv.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = p_tlv.name
    local subtree = tree:add(p_tlv, buffer())
    local offset = 0
    local total = buffer:len()
    local count = 0

    while offset < total do
        -- 至少需要 2 字节(Type+Length)才能继续解析，否则就是畸形包
        if total - offset < 2 then
            subtree:add_proto_expert_info(nil, "TLV truncated at offset " .. offset)
            break
        end

        local t = buffer(offset, 1):uint()
        local l = buffer(offset + 1, 1):uint()

        -- 声明的长度超过剩余 buffer，说明包是坏的，不能继续往下读，否则会抛 Tvb 越界异常
        if offset + 2 + l > total then
            subtree:add_proto_expert_info(nil, "TLV length overruns packet at offset " .. offset)
            break
        end

        local item = subtree:add(buffer(offset, 2 + l),
            string.format("%s (type=%d len=%d)", type_names[t] or "Unknown", t, l))
        item:add(f_type, buffer(offset, 1))
        item:add(f_len,  buffer(offset + 1, 1))
        if l > 0 then
            item:add(f_val, buffer(offset + 2, l))
        end

        offset = offset + 2 + l  -- 关键：offset 必须严格按"已消费字节数"前进
        count = count + 1
    end

    pinfo.cols.info:set(string.format("TLV Flat (%d items)", count))
    return offset
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_tlv.prefs.port, p_tlv)
