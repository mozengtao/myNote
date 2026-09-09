-- Pattern: Expert Pattern（多类别 ProtoExpert 分级：Warn / Note / Error）
-- 场景：跟案例1 DNS dissector 一样，按"值得标记的原因"分类声明多个 ProtoExpert，
-- 而不是把所有异常都标成同一个 severity——否则 Expert Info 面板会失去信号意义
-- （这正是笔记里提到的这个模式的"缺点"）。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.11节阅读）。
--
-- 帧格式（MiniLogin）：
--   offset 0  1字节  version   (< 2 认为是过期版本，只是提醒，不阻断解析 -> Warn)
--   offset 1  1字节  status    (0=OK, 1=AuthFailed -> 有安全含义但不是协议错误 -> Note；
--                               其它取值认为是畸形包 -> Error)
--
-- 加载测试：
--   wireshark -X lua_script:example2.lua
-- 触发 Warn（过期版本）：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([1, 0]), ('127.0.0.1', 40023))"
-- 触发 Note（认证失败）：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([2, 1]), ('127.0.0.1', 40023))"
-- 触发 Error（status 取值非法，畸形包）：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([2, 9]), ('127.0.0.1', 40023))"
-- 可以用 Filter `expert.severity >= "Warn"` 一次性筛出前两类，跳过完全正常的 status=0 包。

local p_expert2 = Proto("expertmulti", "Expert Pattern Example 2: Multi-Category")
p_expert2.prefs.port = Pref.uint("UDP Port", 40023, "UDP port this dissector listens on")

-- 按"错误/异常的性质"分三类声明，而不是共用一个 ef_generic：
--   过期版本：值得提醒，但完全不影响后续解析 -> WARN + DEPRECATED
--   认证失败：有安全含义，但这是业务语义上的失败，不是包本身畸形 -> NOTE + SECURITY
--   非法 status：协议层面的畸形数据 -> ERROR + MALFORMED
local ef_deprecated = ProtoExpert.new("expertmulti.deprecated.expert",
    "Deprecated protocol version", expert.group.DEPRECATED, expert.severity.WARN)
local ef_authfailed = ProtoExpert.new("expertmulti.authfailed.expert",
    "Authentication failed", expert.group.SECURITY, expert.severity.NOTE)
local ef_badstatus = ProtoExpert.new("expertmulti.badstatus.expert",
    "Unknown status value", expert.group.MALFORMED, expert.severity.ERROR)

local status_names = { [0] = "OK", [1] = "AuthFailed" }
local f_version = ProtoField.uint8("expertmulti.version", "Version", base.DEC)
local f_status  = ProtoField.uint8("expertmulti.status", "Status", base.DEC, status_names)
p_expert2.fields = { f_version, f_status }
p_expert2.experts = { ef_deprecated, ef_authfailed, ef_badstatus }

function p_expert2.dissector(buffer, pinfo, tree)
    if buffer:len() < 2 then return 0 end

    pinfo.cols.protocol = p_expert2.name
    local subtree = tree:add(p_expert2, buffer())

    local version = buffer(0, 1):uint()
    local version_item = subtree:add(f_version, buffer(0, 1))
    if version < 2 then
        version_item:add_proto_expert_info(ef_deprecated, "client is using a deprecated version, please upgrade")
    end

    local status = buffer(1, 1):uint()
    local status_item = subtree:add(f_status, buffer(1, 1))
    if status == 0 then
        pinfo.cols.info:set("MiniLogin OK")
    elseif status == 1 then
        status_item:add_proto_expert_info(ef_authfailed, "authentication failed")
        pinfo.cols.info:set("MiniLogin AuthFailed")
    else
        status_item:add_proto_expert_info(ef_badstatus, "unrecognized status value " .. status)
        pinfo.cols.info:set("MiniLogin malformed status=" .. status)
    end

    return buffer:len()
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_expert2.prefs.port, p_expert2)
