-- Pattern: Dispatcher（数值型分派）
-- 场景：一个"传感器上报帧"协议，第1字节是 msgType（1=Temperature 2=Humidity 3=Battery），
-- 后面2字节是该类型对应的原始数值。用一张 { [msgType]=handler } 表代替 if/elseif 链。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.1节阅读）。
--
-- 帧格式：
--   offset 0      1字节  msgType (1/2/3，未知类型会走 Expert Info)
--   offset 1..2   2字节  value (big-endian uint16)
--
-- 加载测试：
--   wireshark -X lua_script:example1.lua
-- 构造一个 msgType=1(Temperature) value=235 的测试包并发到 UDP 40001：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([1, 0, 235]), ('127.0.0.1', 40001))"
-- 换成 bytes([2, 0, 60]) 测 Humidity，bytes([9, 0, 0]) 测"未知类型"分支。

local p_dispsensor = Proto("dispsensor", "Dispatcher Pattern Example 1: Sensor Frame")
p_dispsensor.prefs.port = Pref.uint("UDP Port", 40001, "UDP port this dissector listens on")

local msgtype_names = { [1] = "Temperature", [2] = "Humidity", [3] = "Battery" }

local f_msgtype  = ProtoField.uint8("dispsensor.msgtype", "Message Type", base.DEC, msgtype_names)
local f_temp     = ProtoField.uint16("dispsensor.temperature", "Temperature (raw)", base.DEC)
local f_humidity = ProtoField.uint16("dispsensor.humidity", "Humidity (raw)", base.DEC)
local f_battery  = ProtoField.uint16("dispsensor.battery", "Battery (raw, mV)", base.DEC)
p_dispsensor.fields = { f_msgtype, f_temp, f_humidity, f_battery }

-- 分派表：key 是 msgType 的取值，value 是"如何解析剩余字节 + 更新 Info 列"的处理函数。
-- 新增一种传感器类型，只需要在这里加一行，不用改 dissector() 主体。
local handlers = {}

handlers[1] = function(buffer, pinfo, subtree)
    local raw = buffer(1, 2):uint()
    subtree:add(f_temp, buffer(1, 2))
    pinfo.cols.info:set(string.format("Temperature raw=%d", raw))
end

handlers[2] = function(buffer, pinfo, subtree)
    local raw = buffer(1, 2):uint()
    subtree:add(f_humidity, buffer(1, 2))
    pinfo.cols.info:set(string.format("Humidity raw=%d", raw))
end

handlers[3] = function(buffer, pinfo, subtree)
    local raw = buffer(1, 2):uint()
    subtree:add(f_battery, buffer(1, 2))
    pinfo.cols.info:set(string.format("Battery raw=%d mV", raw))
end

function p_dispsensor.dissector(buffer, pinfo, tree)
    if buffer:len() < 3 then return 0 end

    pinfo.cols.protocol = p_dispsensor.name
    local subtree = tree:add(p_dispsensor, buffer())
    local msgtype = buffer(0, 1):uint()
    subtree:add(f_msgtype, buffer(0, 1))

    local handler = handlers[msgtype]
    if handler then
        -- "这是什么类型"和"这个类型该怎么解析"彼此解耦，handler 内部完全不需要知道分派逻辑
        handler(buffer, pinfo, subtree)
    else
        subtree:add_proto_expert_info(nil, "unknown message type " .. msgtype)
        pinfo.cols.info:set("Unknown message type " .. msgtype)
    end

    return buffer:len()
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_dispsensor.prefs.port, p_dispsensor)
