-- Pattern: Length-Prefixed Parser（先读一个长度字段，再据此切出后续内容）
-- 场景：变长 payload 前面带一个4字节长度字段，读长度、检查剩余字节是否够，
-- 再切出对应长度的内容——这也是 TCP 层做 dissect_tcp_pdus() 判断"是否需要更多字节"的理论基础。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.6节阅读）。
--
-- 帧格式（LenPrefixed）：
--   offset 0..3  4字节  len (big-endian uint32，后面 payload 的字节数)
--   offset 4..   N字节  payload
--
-- 加载测试：
--   wireshark -X lua_script:example1.lua
--   python3 -c "import socket,struct; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       payload=b'hello world'; \
--       s.sendto(struct.pack('>I', len(payload)) + payload, ('127.0.0.1', 40011))"
-- 故意发一个"声明长度比实际数据长"的畸形包，测 Expert Info 分支：
--   python3 -c "import socket,struct; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(struct.pack('>I', 100) + b'short', ('127.0.0.1', 40011))"

local p_lp = Proto("lenprefixed", "Length-Prefixed Parser Example 1: Single Field")
p_lp.prefs.port = Pref.uint("UDP Port", 40011, "UDP port this dissector listens on")

local f_len  = ProtoField.uint32("lenprefixed.len",  "Payload Length", base.DEC)
local f_data = ProtoField.string("lenprefixed.data", "Payload")
p_lp.fields = { f_len, f_data }

function p_lp.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = p_lp.name
    local subtree = tree:add(p_lp, buffer())

    if buffer:len() < 4 then
        subtree:add_proto_expert_info(nil, "not enough bytes for length prefix")
        return 0
    end

    local len = buffer(0, 4):uint()  -- 长度字段本身的字节序要跟协议规范对齐
    subtree:add(f_len, buffer(0, 4))

    if buffer:len() < 4 + len then
        -- 这正是 TCP 场景下 desegment 需要处理的情况：数据还没到齐。
        -- 如果是走 dissect_tcp_pdus()，应返回 DESEGMENT_ONE_MORE_SEGMENT 让框架去要更多字节，
        -- 而不是自己截断解析；这里是 UDP，直接标记为畸形。
        subtree:add_proto_expert_info(nil, "payload shorter than declared length")
        return 4
    end

    subtree:add(f_data, buffer(4, len))
    pinfo.cols.info:set(string.format("LenPrefixed len=%d", len))
    return 4 + len
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_lp.prefs.port, p_lp)
