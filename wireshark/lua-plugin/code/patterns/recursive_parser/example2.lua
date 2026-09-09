-- Pattern: Recursive Parser（真正的自调用递归：Group 可以嵌套 Sub-Group）
-- 场景：跟例1的区别是——这里的自相似结构不是"一条链"（标签序列），而是一棵树：
-- 一个 Group 里的每一项，可能是 Leaf（叶子，直接有值），也可能是另一个完整的 Group
-- （子树，格式跟外层 Group 完全一样）。这种"树形自相似"必须用函数自调用来表达，
-- 循环写不出来——所以这个例子里 parseGroup() 会在遇到嵌套 Group 时调用它自己。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.8节阅读）。
--
-- 帧格式（Group，可递归嵌套）：
--   offset 0  1字节  itemCount
--   重复 itemCount 次，每项：
--     1字节  kind (0 = Leaf，1 = SubGroup)
--     如果 kind==0：紧跟 1字节 value
--     如果 kind==1：紧跟一个完整的 Group（格式跟外层一样，递归）
--
-- 加载测试（构造一个 Group{ Leaf=5, SubGroup{ Leaf=7, Leaf=9 } }）：
--   wireshark -X lua_script:example2.lua
--   python3 -c "
--   import socket
--   inner_group = bytes([2]) + bytes([0,7]) + bytes([0,9])       # itemCount=2, Leaf=7, Leaf=9
--   outer_group = bytes([2]) + bytes([0,5]) + bytes([1]) + inner_group  # itemCount=2, Leaf=5, SubGroup(...)
--   s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
--   s.sendto(outer_group, ('127.0.0.1', 40016))
--   "

local p_group = Proto("minigroup", "Recursive Parser Example 2: Nested Group (真递归)")
p_group.prefs.port = Pref.uint("UDP Port", 40016, "UDP port this dissector listens on")

local KIND_LEAF, KIND_SUBGROUP = 0, 1
local f_count = ProtoField.uint8("minigroup.count", "Item Count", base.DEC)
local f_kind  = ProtoField.uint8("minigroup.kind", "Kind", base.DEC, { [KIND_LEAF] = "Leaf", [KIND_SUBGROUP] = "SubGroup" })
local f_value = ProtoField.uint8("minigroup.value", "Value", base.DEC)
p_group.fields = { f_count, f_kind, f_value }

local MAX_DEPTH = 16 -- 明确的深度上限：畸形/恶意包可能无限嵌套 SubGroup，必须有终止条件

-- 真正的自调用递归：解析一个 Group，遇到 kind==SubGroup 就对紧跟着的内容
-- 递归调用 parseGroup 自己（depth+1），返回 (整个 Group 消费的字节数)。
-- 跟例1的循环写法不同：这里的调用栈跟数据结构的嵌套层级一一对应。
local function parseGroup(buffer, offset, subtree, depth)
    if depth > MAX_DEPTH then
        error("parseGroup: nesting too deep (>" .. MAX_DEPTH .. "), possible malformed packet")
    end

    if offset + 1 > buffer:len() then
        error("parseGroup: truncated before item count at offset " .. offset)
    end

    local count = buffer(offset, 1):uint()
    subtree:add(f_count, buffer(offset, 1))
    local pos = offset + 1

    for i = 1, count do
        if pos + 1 > buffer:len() then
            error(string.format("parseGroup: truncated before kind byte of item %d at offset %d", i, pos))
        end

        local kind = buffer(pos, 1):uint()
        local item = subtree:add(buffer(pos, 1), string.format("Item %d: %s", i, kind == KIND_LEAF and "Leaf" or "SubGroup"))
        item:add(f_kind, buffer(pos, 1))
        pos = pos + 1

        if kind == KIND_LEAF then
            if pos + 1 > buffer:len() then
                error("parseGroup: truncated before leaf value at offset " .. pos)
            end
            item:add(f_value, buffer(pos, 1))
            pos = pos + 1
        elseif kind == KIND_SUBGROUP then
            -- 递归：SubGroup 内部又是一个完整的 Group，格式跟外层完全一样
            local consumed = parseGroup(buffer, pos, item, depth + 1)
            pos = pos + consumed
        else
            error("parseGroup: unknown kind " .. kind .. " at offset " .. (pos - 1))
        end
    end

    return pos - offset
end

function p_group.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = p_group.name
    local subtree = tree:add(p_group, buffer())

    local ok, consumed_or_err = pcall(parseGroup, buffer, 0, subtree, 1)
    if not ok then
        subtree:add_proto_expert_info(nil, "malformed Group: " .. tostring(consumed_or_err))
        return buffer:len()
    end

    pinfo.cols.info:set("Group (nested)")
    return consumed_or_err
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_group.prefs.port, p_group)
