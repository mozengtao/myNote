-- Pattern: Registry Pattern（示例1，子协议文件：v2 版本，自己把自己挂进注册表）
-- 必须跟 entry.lua、v1.lua 一起加载才构成完整插件，见 entry.lua 顶部注释。
--
-- 跟 v1 的差异只是"解析规则不同"（这里多解析了1字节 flag），
-- 用来体现"注册表不关心每个子协议内部具体怎么解析，只负责按 key 转发"。

local v2Proto = Proto("patternreg_v2", "PatternReg v2")

local f_flag    = ProtoField.uint8("patternreg_v2.flag", "Flag", base.HEX)
local f_payload = ProtoField.bytes("patternreg_v2.payload", "Payload")
v2Proto.fields = { f_flag, f_payload }

function v2Proto.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = v2Proto.name
    local subtree = tree:add(v2Proto, buffer())

    if buffer:len() < 1 then
        subtree:add_proto_expert_info(nil, "PatternReg v2: missing flag byte")
        return buffer:len()
    end

    subtree:add(f_flag, buffer(0, 1))
    if buffer:len() > 1 then
        subtree:add(f_payload, buffer(1))
    end

    pinfo.cols.info:set(string.format("PatternReg v2 flag=0x%02x", buffer(0, 1):uint()))
    return buffer:len()
end

function v2Proto.init()
    DissectorTable.get("patternreg.version"):add(2, v2Proto)
end
