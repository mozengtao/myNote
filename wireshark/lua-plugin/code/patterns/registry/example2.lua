-- Pattern: Registry Pattern（示例2，不依赖 DissectorTable 的轻量注册表）
-- 场景：示例1用的是 Wireshark 提供的 DissectorTable 基础设施；这里演示同样的
-- "登记 + 转发"思路也可以完全不依赖它——自己维护一张 Lua table 作为注册表，
-- 各"模块"调用 Registry.register(key, name, handlerFn) 把自己挂进去。
-- 适合"不需要暴露给其它 dissector 复用、只是本插件内部想解耦"的场景。
--
-- 这是一个完整、可独立加载运行的单文件插件（配合笔记 11-design-patterns.md 8.10节阅读）。
-- 真实项目里，下面 Registry.register(...) 的调用通常会分散在多个独立文件里，
-- 这里为了保持"单文件可直接运行"而写在一起，但入口 dissector 的逻辑完全不知道
-- 也不关心到底注册了几个 handler。
--
-- 帧格式：offset 0 是1字节 msgType，用来查 Registry，剩下字节转发给对应 handler。
--
-- 加载测试：
--   wireshark -X lua_script:example2.lua
-- 测 msgType=1（Ping）：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([1, 0x01, 0x02]), ('127.0.0.1', 40021))"
-- 测 msgType=2（Echo）：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([2]) + b'hello', ('127.0.0.1', 40021))"

-- 轻量注册表：跟 DissectorTable 概念上是一回事，只是完全用 Lua 原生 table 实现，
-- 不需要 DissectorTable.new()，也不用管 ftypes/base 这些 Wireshark 特定的类型描述。
local Registry = { handlers = {} }

function Registry.register(msgType, name, handlerFn)
    Registry.handlers[msgType] = { name = name, fn = handlerFn }
end

function Registry.lookup(msgType)
    return Registry.handlers[msgType]
end

local p_lightreg = Proto("lightreg", "Registry Pattern Example 2: In-Process Handler Registry")
p_lightreg.prefs.port = Pref.uint("UDP Port", 40021, "UDP port this dissector listens on")

local f_msgtype = ProtoField.uint8("lightreg.msgtype", "Message Type", base.DEC)
p_lightreg.fields = { f_msgtype }

function p_lightreg.dissector(buffer, pinfo, tree)
    if buffer:len() < 1 then return 0 end

    pinfo.cols.protocol = p_lightreg.name
    local subtree = tree:add(p_lightreg, buffer(0, 1))
    local msgtype = buffer(0, 1):uint()
    subtree:add(f_msgtype, buffer(0, 1))

    -- 入口只做"查表 + 转发"，完全不知道 Registry 里到底注册了哪些 handler。
    local entry = Registry.lookup(msgtype)
    if entry then
        entry.fn(buffer(1):tvb(), pinfo, subtree)
        pinfo.cols.info:set(entry.name)
    else
        subtree:add_proto_expert_info(nil, "no handler registered for msgtype " .. msgtype)
        pinfo.cols.info:set("LightReg: unknown msgtype " .. msgtype)
    end

    return buffer:len()
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_lightreg.prefs.port, p_lightreg)

-- "模块"各自把自己登记进 Registry；在多文件项目里，这两段通常各自在独立文件的
-- 顶层代码里执行（只要在第一个包到达之前跑过就行），跟入口文件完全解耦。
Registry.register(1, "Ping Module", function(tvb, pinfo, tree)
    tree:add(tvb(), string.format("Ping payload (%d bytes)", tvb:len()))
end)

Registry.register(2, "Echo Module", function(tvb, pinfo, tree)
    tree:add(tvb(), "Echo payload: " .. tvb:string())
end)
