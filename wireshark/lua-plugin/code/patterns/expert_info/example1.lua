-- Pattern: Expert Pattern（单一类别 ProtoExpert：声明长度不足 -> Error）
-- 场景：解析过程中发现"声明的长度超过实际可用字节"，这是一种明确的畸形包，
-- 预先用 ProtoExpert.new() 声明一个专门的 Expert（而不是临时拼一句字符串堆进 Tree），
-- 这样用户可以用 Filter（比如 expert.severity == "Error"）快速筛出所有这种包。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.11节阅读）。
--
-- 帧格式（SecureMsg）：
--   offset 0  1字节  declaredLen (后面 payload 应该有的字节数)
--   offset 1.. N字节  payload
--
-- 加载测试：
--   wireshark -X lua_script:example1.lua
-- 正常包（declaredLen=3，实际也给3字节）：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([3, 1, 2, 3]), ('127.0.0.1', 40022))"
-- 触发 Expert Error（declaredLen=100，实际只有2字节）：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([100, 1, 2]), ('127.0.0.1', 40022))"
-- 加载后可以在 Wireshark 的 Expert Information 面板/Filter `expert.severity == "Error"`
-- 里直接看到、筛出触发过这个分支的包。

local p_expert1 = Proto("expertsingle", "Expert Pattern Example 1: Single Category")
p_expert1.prefs.port = Pref.uint("UDP Port", 40022, "UDP port this dissector listens on")

-- 预先按"错误类型"声明专门的 ProtoExpert（这里只有一种类型，但依然独立声明，
-- 不要偷懒只声明一个通用的"出错了"，未来要加新类型时才不用改这一个 Expert 的语义）。
local ef_badlen = ProtoExpert.new("expertsingle.badlen.expert",
    "Declared length exceeds available bytes",
    expert.group.MALFORMED, expert.severity.ERROR)

local f_len     = ProtoField.uint8("expertsingle.len", "Declared Length", base.DEC)
local f_payload = ProtoField.bytes("expertsingle.payload", "Payload")
p_expert1.fields = { f_len, f_payload }
p_expert1.experts = { ef_badlen }

function p_expert1.dissector(buffer, pinfo, tree)
    if buffer:len() < 1 then return 0 end

    pinfo.cols.protocol = p_expert1.name
    local subtree = tree:add(p_expert1, buffer())

    local declared_len = buffer(0, 1):uint()
    local len_item = subtree:add(f_len, buffer(0, 1))

    if buffer:len() < 1 + declared_len then
        -- 把 Expert Info 挂在"声明长度"这个字段上，而不是挂在整个 subtree 上，
        -- 用户在 Tree 里点开就能立刻定位到"是哪个字段声明的值有问题"。
        len_item:add_proto_expert_info(ef_badlen,
            string.format("declared %d bytes but only %d available", declared_len, buffer:len() - 1))
        pinfo.cols.info:set("SecureMsg malformed (declared len too large)")
        return buffer:len()
    end

    subtree:add(f_payload, buffer(1, declared_len))
    pinfo.cols.info:set("SecureMsg len=" .. declared_len)
    return 1 + declared_len
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_expert1.prefs.port, p_expert1)
