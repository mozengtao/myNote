-- Pattern: Subdissector Chain（转发给内置协议）
-- 场景：一个"迷你隧道协议"，1字节 innerType 之后紧跟一个完整的内层 IP 包。
-- 自己不重新实现 IP 解析，而是把剩下的字节连同 pinfo/tree 一起转发给 Wireshark
-- 内置的 "ip" dissector（Dissector.get("ip"):call(...)）。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.4节阅读）。
--
-- 帧格式（MiniTunnel）：
--   offset 0   1字节  innerType (0x04 = 内层是 IPv4 包)
--   offset 1.. N字节  内层协议的完整字节（这里是一个 IPv4 包）
--
-- 加载测试（需要本机装了 scapy 来构造合法的 IPv4 头部）：
--   wireshark -X lua_script:example1.lua
--   python3 -c "
--   from scapy.all import IP, raw
--   import socket
--   inner = raw(IP(dst='8.8.8.8')/b'hello')
--   payload = bytes([0x04]) + inner
--   s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
--   s.sendto(payload, ('127.0.0.1', 40007))
--   "
-- 预期效果：Tree 里除了 MiniTunnel 自己的 innerType 字段，还会出现一整棵
-- 由内置 "ip" dissector 生成的 Internet Protocol 子树——这就是 Subdissector Chain
-- "不用重新实现下游协议，还能保证 Tree 层级正确嵌套"的效果。

local p_tunnel = Proto("minitunnel", "Subdissector Chain Example 1: IP Tunnel")
p_tunnel.prefs.port = Pref.uint("UDP Port", 40007, "UDP port this dissector listens on")

local innertype_names = { [0x04] = "IPv4" }
local f_innertype = ProtoField.uint8("minitunnel.innertype", "Inner Type", base.HEX, innertype_names)
p_tunnel.fields = { f_innertype }

function p_tunnel.dissector(buffer, pinfo, tree)
    if buffer:len() < 1 then return 0 end

    pinfo.cols.protocol = p_tunnel.name
    local subtree = tree:add(p_tunnel, buffer(0, 1))
    local innertype = buffer(0, 1):uint()
    subtree:add(f_innertype, buffer(0, 1))

    local inner = buffer(1):tvb()

    if innertype == 0x04 then
        -- Dissector.get() 在目标协议不存在的运行环境里会抛错，必须用 pcall 兜底，
        -- 否则一旦下游协议缺失，整个 dissector 都会崩溃退出。
        local ok, ip_dissector = pcall(Dissector.get, "ip")
        if ok and ip_dissector then
            pinfo.cols.info:set("MiniTunnel -> IPv4")
            ip_dissector:call(inner, pinfo, tree)
        else
            subtree:add_proto_expert_info(nil, "ip dissector unavailable in this build")
            tree:add(p_tunnel, inner(), "Undissected inner IPv4 packet")
        end
    else
        subtree:add_proto_expert_info(nil, "unknown inner type " .. innertype)
    end

    return buffer:len()
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_tunnel.prefs.port, p_tunnel)
