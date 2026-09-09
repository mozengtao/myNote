-- Pattern: Subdissector Chain（按类型号在多个内置协议间转发，带回退兜底）
-- 场景：一个"迷你复用协议"，1字节 protoId 决定剩下的字节该转发给哪个内置协议；
-- 如果 protoId 未知，或者对应的下游协议在当前 Wireshark 编译版本里不存在，
-- 统一回退给总是存在的 "data" dissector，而不是让整个解析崩掉。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.4节阅读）。
--
-- 帧格式（MiniMux）：
--   offset 0   1字节  protoId (1 = 内层是 IPv4 包，2 = 内层是 HTTP 请求/响应文本，其它 = 未知)
--   offset 1.. N字节  内层协议的完整字节
--
-- 加载测试：
--   wireshark -X lua_script:example2.lua
-- 测 protoId=2（HTTP）分支：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([2]) + b'GET / HTTP/1.1\\r\\nHost: example.com\\r\\n\\r\\n', ('127.0.0.1', 40008))"
-- 测 protoId=9（未知，触发回退到 data）分支：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([9, 0xDE, 0xAD, 0xBE, 0xEF]), ('127.0.0.1', 40008))"

local p_mux = Proto("minimux", "Subdissector Chain Example 2: Protocol Mux")
p_mux.prefs.port = Pref.uint("UDP Port", 40008, "UDP port this dissector listens on")

local proto_id_names = { [1] = "IPv4", [2] = "HTTP" }
-- protoId -> Wireshark 内置 dissector 短名的映射；转发目标是数据而不是硬编码逻辑，
-- 新增一种 protoId 只需要在这张表里加一行。
local proto_id_targets = { [1] = "ip", [2] = "http" }

local f_protoid = ProtoField.uint8("minimux.protoid", "Protocol Id", base.DEC, proto_id_names)
p_mux.fields = { f_protoid }

-- 统一的"安全获取 dissector"封装：找不到就返回 nil，调用方决定怎么兜底。
local function tryGetDissector(name)
    if not name then return nil end
    local ok, d = pcall(Dissector.get, name)
    if ok then return d end
    return nil
end

function p_mux.dissector(buffer, pinfo, tree)
    if buffer:len() < 1 then return 0 end

    pinfo.cols.protocol = p_mux.name
    local subtree = tree:add(p_mux, buffer(0, 1))
    local protoid = buffer(0, 1):uint()
    subtree:add(f_protoid, buffer(0, 1))

    local inner = buffer(1):tvb()
    local target = tryGetDissector(proto_id_targets[protoid])

    if not target then
        -- 目标协议未知，或者在当前 Wireshark 编译版本里没有这个 dissector：
        -- 统一回退到 "data"，它保证在任何版本里都存在。
        target = tryGetDissector("data")
        if protoid and not proto_id_targets[protoid] then
            subtree:add_proto_expert_info(nil, "unknown protoId " .. protoid .. ", falling back to data")
        end
    end

    if target then
        target:call(inner, pinfo, tree)
    else
        -- 理论上不会走到这里（data 总是存在），留作最后一道防线
        tree:add(p_mux, inner(), "Undissected payload")
    end

    return buffer:len()
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_mux.prefs.port, p_mux)
