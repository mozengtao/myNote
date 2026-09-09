-- Pattern: Table-Driven Parsing（表项自带 decode 函数，做展示层的换算）
-- 场景：字段依然是固定顺序排列，但原始字节需要经过换算才有可读的物理意义
-- （温度原始值/10 = 摄氏度，电池原始值*3.3/1024 = 电压）。
-- 给每个表项加一个可选的 decode 函数，循环里统一调用，而不是在每个分支里各写一遍换算代码。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.3节阅读）。
--
-- 帧格式（SensorReading，固定4字节）：
--   offset 0..1  2字节  raw_temp    (int16，实际温度 = raw_temp / 10.0 摄氏度)
--   offset 2..3  2字节  raw_battery (uint16，实际电压 = raw_battery * 3.3 / 1024 V)
--
-- 加载测试：
--   wireshark -X lua_script:example2.lua
-- 构造 raw_temp=235(23.5C) raw_battery=800 的包，发到 UDP 40006：
--   python3 -c "import socket,struct; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(struct.pack('>hH', 235, 800), ('127.0.0.1', 40006))"

local p_tdreading = Proto("tdreading", "Table-Driven Parsing Example 2: Reading With Decode")
p_tdreading.prefs.port = Pref.uint("UDP Port", 40006, "UDP port this dissector listens on")

local f_temp    = ProtoField.int16("tdreading.temperature", "Temperature", base.DEC)
local f_battery = ProtoField.uint16("tdreading.battery", "Battery Voltage", base.DEC)
p_tdreading.fields = { f_temp, f_battery }

-- 描述表：除了 field/len，多两个可选函数：
--   read(tvbr)      把字节读成跟 field 类型匹配的原始数值（int16 用 :int()，uint16 用 :uint()）
--   format(rawvalue) 把原始数值换算成带单位的可读字符串，用于 Info 列和 Tree 里的附加说明
-- 换算规则跟"这个字段在哪、多长"写在同一张表里，不用去 dissector() 主体里找。
local layout = {
    {
        field = f_temp,
        len = 2,
        read = function(tvbr) return tvbr:int() end,
        format = function(raw) return string.format("%.1f C (raw=%d)", raw / 10.0, raw) end,
    },
    {
        field = f_battery,
        len = 2,
        read = function(tvbr) return tvbr:uint() end,
        format = function(raw) return string.format("%.2f V (raw=%d)", raw * 3.3 / 1024, raw) end,
    },
}

function p_tdreading.dissector(buffer, pinfo, tree)
    local total_len = 0
    for _, item in ipairs(layout) do
        total_len = total_len + item.len
    end
    if buffer:len() < total_len then return 0 end

    pinfo.cols.protocol = p_tdreading.name
    local subtree = tree:add(p_tdreading, buffer())

    local offset = 0
    local info_parts = {}
    for _, item in ipairs(layout) do
        local range = buffer(offset, item.len)
        local raw = item.read(range)
        local text = item.format(raw)
        -- tree:add(field, tvbrange, value) 三参数形式：value 是我们自己算出来的原始数值，
        -- 保证跟 field 声明的类型一致；再用 append_text 把换算后的可读文本附加在后面。
        subtree:add(item.field, range, raw):append_text(" (" .. text .. ")")
        info_parts[#info_parts + 1] = text
        offset = offset + item.len
    end

    pinfo.cols.info:set(table.concat(info_parts, ", "))
    return offset
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_tdreading.prefs.port, p_tdreading)
