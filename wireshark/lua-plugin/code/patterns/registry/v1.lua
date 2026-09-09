-- Pattern: Registry Pattern（示例1，子协议文件：v1 版本，自己把自己挂进注册表）
-- 必须跟 entry.lua、v2.lua 一起加载才构成完整插件，见 entry.lua 顶部注释。

local v1Proto = Proto("patternreg_v1", "PatternReg v1")

local f_payload = ProtoField.bytes("patternreg_v1.payload", "Payload")
v1Proto.fields = { f_payload }

function v1Proto.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = v1Proto.name
    local subtree = tree:add(v1Proto, buffer())
    if buffer:len() > 0 then
        subtree:add(f_payload, buffer())
    end
    pinfo.cols.info:set(string.format("PatternReg v1 (%d bytes payload)", buffer:len()))
    return buffer:len()
end

-- 关键一步：v1 自己把自己 add 进 entry.lua 创建的共享表，entry.lua 完全不需要改动。
-- 新增一个版本只需要新增一个类似 v1.lua/v2.lua 的文件，这正是"对扩展开放，对修改关闭"。
function v1Proto.init()
    DissectorTable.get("patternreg.version"):add(1, v1Proto)
end
