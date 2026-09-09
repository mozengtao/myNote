-- Pattern: Recursive Parser（循环模拟递归：类 DNS 标签序列）
-- 场景：域名式的"标签序列"（1字节长度 + 标签内容，重复出现，长度为0表示结束）
-- 是自相似结构的经典例子。用 repeat...until label_len==0 的循环写法实现，
-- 本质上是"用循环模拟递归"——跟案例1 DNS dissector 里 getQueryName() 是同一手法。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.8节阅读）。
--
-- 帧格式（DomainName）：重复的 [1字节 label_len][label_len字节 内容]，label_len=0 结束
--   例如 "www.example.com" 编码为：
--   3 'w' 'w' 'w'  7 'e' 'x' 'a' 'm' 'p' 'l' 'e'  3 'c' 'o' 'm'  0
--
-- 加载测试：
--   wireshark -X lua_script:example1.lua
--   python3 -c "
--   import socket
--   def label(s): return bytes([len(s)]) + s.encode()
--   payload = label('www') + label('example') + label('com') + bytes([0])
--   s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
--   s.sendto(payload, ('127.0.0.1', 40015))
--   "

local p_domain = Proto("minidomain", "Recursive Parser Example 1: Label Sequence")
p_domain.prefs.port = Pref.uint("UDP Port", 40015, "UDP port this dissector listens on")

local f_label = ProtoField.string("minidomain.label", "Label")
local f_name  = ProtoField.string("minidomain.name", "Full Name")
p_domain.fields = { f_label, f_name }

local MAX_LABELS = 64 -- 深度/次数上限：防止畸形包（label_len 永远不为0）导致死循环

-- 独立解析函数：用 repeat...until 循环模拟"标签序列是自相似结构"的递归语义，
-- 每次读一个标签长度，长度为0则终止；返回拼好的完整名字和消费的字节数。
local function parseLabelSequence(buffer, offset, subtree)
    local labels = {}
    local start_offset = offset
    local count = 0

    repeat
        if offset >= buffer:len() then
            error("parseLabelSequence: truncated before label length byte at offset " .. offset)
        end

        local label_len = buffer(offset, 1):uint()
        count = count + 1
        if count > MAX_LABELS then
            error("parseLabelSequence: too many labels (possible malformed/malicious packet)")
        end

        if label_len > 0 then
            if offset + 1 + label_len > buffer:len() then
                error("parseLabelSequence: label overruns packet at offset " .. offset)
            end
            local label = buffer(offset + 1, label_len):string()
            subtree:add(f_label, buffer(offset, 1 + label_len), label)
            labels[#labels + 1] = label
            offset = offset + 1 + label_len
        else
            offset = offset + 1 -- 消费掉终止字节 (label_len == 0)
        end
    until label_len == 0

    return table.concat(labels, "."), offset - start_offset
end

function p_domain.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = p_domain.name
    local subtree = tree:add(p_domain, buffer())

    local ok, name_or_err, consumed = pcall(parseLabelSequence, buffer, 0, subtree)
    if not ok then
        subtree:add_proto_expert_info(nil, "malformed DomainName: " .. tostring(name_or_err))
        return buffer:len()
    end

    subtree:add(f_name, buffer(0, consumed), name_or_err)
    pinfo.cols.info:set("DomainName " .. name_or_err)
    return consumed
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_domain.prefs.port, p_domain)
