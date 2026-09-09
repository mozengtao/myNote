-- Pattern: Factory Pattern（批量生成同模式的 N 个 ProtoField）
-- 场景：Factory 不只能用来生成 Proto，也能用来生成一批"命名规则相同、类型相同"的
-- ProtoField——比如一个帧里连续出现 N 个格式一样的传感器读数，手写 N 个
-- ProtoField.uint16(...) 容易漏改编号，用工厂函数 makeFieldSet(prefix, count) 批量生成。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.9节阅读）。
--
-- 帧格式（MultiSensor，固定 4 个 uint16 读数）：
--   offset 0..1   reading1
--   offset 2..3   reading2
--   offset 4..5   reading3
--   offset 6..7   reading4
--
-- 加载测试：
--   wireshark -X lua_script:example2.lua
--   python3 -c "import socket,struct; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(struct.pack('>HHHH', 10, 20, 30, 40), ('127.0.0.1', 40019))"

local READING_COUNT = 4

-- 工厂函数：按统一命名规则（prefix .. ".readingN"）批量生成 count 个 ProtoField。
-- 好处：调整字段个数只改 READING_COUNT 这一个常量，不用逐个手写/删除 ProtoField 声明；
-- 命名规则集中在这一处，不会出现"漏改编号导致两个字段用了同一个 filter 名"的问题。
local function makeFieldSet(prefix, count)
    local fields = {}
    for i = 1, count do
        fields[i] = ProtoField.uint16(prefix .. ".reading" .. i, "Reading " .. i, base.DEC)
    end
    return fields
end

local p_multisensor = Proto("multisensor", "Factory Pattern Example 2: Field Set Factory")
p_multisensor.prefs.port = Pref.uint("UDP Port", 40019, "UDP port this dissector listens on")

local reading_fields = makeFieldSet("multisensor", READING_COUNT)
p_multisensor.fields = reading_fields

function p_multisensor.dissector(buffer, pinfo, tree)
    local needed = READING_COUNT * 2
    if buffer:len() < needed then return 0 end

    pinfo.cols.protocol = p_multisensor.name
    local subtree = tree:add(p_multisensor, buffer())

    local offset = 0
    local values = {}
    for i = 1, READING_COUNT do
        subtree:add(reading_fields[i], buffer(offset, 2))
        values[i] = tostring(buffer(offset, 2):uint())
        offset = offset + 2
    end

    pinfo.cols.info:set("MultiSensor [" .. table.concat(values, ",") .. "]")
    return offset
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_multisensor.prefs.port, p_multisensor)
