-- Pattern: Factory Pattern（批量生成结构相似的版本化 Proto）
-- 场景：多个协议版本之间高度相似（同样的字段命名规则、同样的基础字段集合），
-- 跟案例3 rls_v3_1.lua / rls_v3_2.lua 之间的关系一样。用一个工厂函数
-- makeVersionedProto(version, ...) 统一生成，而不是每个版本手写一遍几乎一样的代码。
--
-- 这是一个完整、可独立加载运行的插件，一次加载会同时注册两个版本化协议
-- （配合笔记 11-design-patterns.md 8.9节阅读）。
--
-- 帧格式（每个版本相同）：
--   offset 0   1字节  msgType
--   offset 1.. N字节  payload（剩余全部字节）
--
-- 加载测试：
--   wireshark -X lua_script:example1.lua
-- 测 v1（UDP 40017）：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([1, 0xAA, 0xBB]), ('127.0.0.1', 40017))"
-- 测 v2（UDP 40018，字段命名规则跟 v1 完全一样，只是端口/Proto名不同）：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([2, 0xCC]), ('127.0.0.1', 40018))"

-- 工厂函数：统一约束"每个版本的 Proto 长什么样"——统一的字段命名前缀
-- （protoName .. ".xxx"）、统一的字段集合、统一的注册方式。
-- 新增一个版本只需要多调用一次这个函数，不需要复制粘贴整段 dissector 逻辑。
local function makeVersionedProto(version, protoName, displayName, defaultPort)
    local proto = Proto(protoName, displayName)
    proto.prefs.port = Pref.uint("UDP Port", defaultPort, "UDP port this dissector listens on")

    local f_version = ProtoField.uint8(protoName .. ".version", "Version", base.DEC)
    local f_msgtype = ProtoField.uint8(protoName .. ".msgtype", "Message Type", base.DEC)
    local f_payload = ProtoField.bytes(protoName .. ".payload", "Payload")
    proto.fields = { f_version, f_msgtype, f_payload }

    function proto.dissector(buffer, pinfo, tree)
        if buffer:len() < 1 then return 0 end

        pinfo.cols.protocol = proto.name
        local subtree = tree:add(proto, buffer())

        -- version 是协议本身固定的常量（工厂参数带进来的），不是从字节里读出来的，
        -- 所以用 2 参数形式 + set_generated() 标记为"生成的"字段，而不是从 buffer 切片。
        subtree:add(f_version, version):set_generated()

        local msgtype = buffer(0, 1):uint()
        subtree:add(f_msgtype, buffer(0, 1))

        if buffer:len() > 1 then
            subtree:add(f_payload, buffer(1))
        end

        pinfo.cols.info:set(string.format("%s msgtype=%d", displayName, msgtype))
        return buffer:len()
    end

    -- 统一的注册方式：所有版本都按同样的规则挂到 udp.port
    local udp_port = DissectorTable.get("udp.port")
    udp_port:add(proto.prefs.port, proto)

    return proto
end

-- 用同一个工厂批量生成两个版本；两者字段命名规则、注册方式完全一致，
-- 唯一的差异（version 值、Proto 名字、默认端口）通过参数传入。
local proto_v1 = makeVersionedProto(1, "factoryproto_v1", "Factory Pattern Example 1: Protocol v1", 40017)
local proto_v2 = makeVersionedProto(2, "factoryproto_v2", "Factory Pattern Example 1: Protocol v2", 40018)
