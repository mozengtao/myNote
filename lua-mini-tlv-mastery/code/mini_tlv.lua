-- Source: /home/morrism/code_extra/wireshark-plugins/examples/mini_tlv/mini_tlv.lua
-- Teaching copy for lua-mini-tlv-mastery. Keep in sync with the original.
--
-- Mini TLV walker: the algorithm inside rcp.lua, without Wireshark.
--
-- Wire format matches RCP: Type(1) + Length(2) + Value(Length).
-- Type is a door number in the CURRENT dictionary, not a global ID.
-- spec.type has three meanings:
--   string      leaf: decode this value and stop
--   { [n]=... } child gallery: swap dictionary to spec.type and recurse
--   {}          hall: keep using the ROOT schema and recurse
-- Length is a sandbox: recursion may not walk past this Value window.
--
-- If Wireshark loads this file from a repo symlink, do nothing.

if Proto ~= nil then
    return
end

--------------------------------------------------------------------------------
-- 两套「层级」，不要叠成一套
--
-- 1) 词典（从 schema 看）
--    schema 是门牌本，不是报文。根表里 1/2/3/4/5/6 并列，只表示：
--    当前词典 = 大厅时，这些 Type 都能查到。
--    type = {}     孩子仍用这本根词典（袋子），不是“没填完”。
--    type = {..}   换一本局部词典（子馆），门牌号在这里重新计数。
--    type = "UINT16" 叶子，没有下一层词典。
--
-- 2) 报文树（从报文看）
--    线格式: Type(1) + Length(2, 大端) + Value(Length 字节)。
--    Length 只计 Value，不含 Type/Length 自己。
--    01 00 17 = Type 1, Length 0x0017 = 23，后面 23 字节都在 Sequence 肚子里。
--    打印缩进跟的是这层沙箱：谁在谁的 Value 里，谁就低一层。
--
-- type = {} 的精确含义：人还在我的 Value 里，门牌却拿到大厅去对。
-- 所以 Packet A 里 SeqNum 和 Sequence 在词典里同级，在树上 SeqNum 低于 Sequence。
--------------------------------------------------------------------------------

local schema = {
    [1] = { name = "Sequence", type = {} },
    [2] = { name = "SeqNum", type = "UINT16" },
    [3] = {
        name = "Operation",
        type = "UINT8",
        valuestring = {
            [1] = "Read",
            [2] = "Write",
            [3] = "Delete",
        },
    },
    [4] = {
        name = "Device",
        type = {
            [1] = { name = "Name", type = "STRING" },
            [2] = { name = "Id", type = "UINT8" },
        },
    },
    [5] = { name = "Channel", type = {} },
    [6] = { name = "ChanIndex", type = "UINT8" },
}

--------------------------------------------------------------------------------
-- Bytes
--------------------------------------------------------------------------------

local function hex_to_bytes(hex)
    hex = hex:gsub("%-%-[^\n]*", ""):gsub("%s+", "")
    if #hex % 2 ~= 0 then
        error("odd number of hex digits")
    end
    return (hex:gsub("..", function(byte)
        return string.char(tonumber(byte, 16))
    end))
end

local function u8(buf, i)
    return string.byte(buf, i)
end

local function u16(buf, i)
    return u8(buf, i) * 256 + u8(buf, i + 1)
end

local function decode_leaf(buf, first, last, spec)
    local kind = spec.type
    local n = last - first + 1

    if n < 1 then
        return "(empty)"
    end

    local text
    if kind == "UINT8" then
        text = tostring(u8(buf, first))
    elseif kind == "UINT16" then
        if n ~= 2 then
            return string.format("(bad UINT16 length %d)", n)
        end
        text = tostring(u16(buf, first))
    elseif kind == "STRING" then
        text = buf:sub(first, last)
    else
        return "(unknown codec " .. kind .. ")"
    end

    if spec.valuestring then
        local raw
        if kind == "UINT8" then
            raw = u8(buf, first)
        elseif kind == "UINT16" then
            raw = u16(buf, first)
        end
        if raw ~= nil and spec.valuestring[raw] then
            text = text .. " (" .. spec.valuestring[raw] .. ")"
        end
    end
    return text
end

--------------------------------------------------------------------------------
-- walk
--
-- depth / indent  = 报文树：当前 Value 窗口套了几层。
-- dict            = 词典：这一层用哪本门牌本（根表或 spec.type）。
-- 两根指针一起走，但不是同一件事。
--------------------------------------------------------------------------------

local function walk(buf, first, last, dict, depth)
    local pos = first
    local indent = string.rep("  ", depth)

    while pos <= last do
        if pos + 2 > last then
            print(indent .. "truncated TLV header")
            return
        end

        local t = u8(buf, pos)
        local len = u16(buf, pos + 1)
        local val_first = pos + 3
        local val_last = pos + 2 + len

        if val_last > last then
            print(string.format("%sTLV %d length %d walks past sandbox", indent, t, len))
            return
        end

        local spec = dict[t]
        local name = spec and spec.name or "Undefined"
        local line = string.format("%sTLV %d (%d bytes) - %s", indent, t, len, name)

        if spec == nil then
            print(line)
        elseif type(spec.type) == "string" then
            -- leaf: value belongs on this node, not as a child
            print(line .. " = " .. decode_leaf(buf, val_first, val_last, spec))
        elseif next(spec.type) == nil then
            print(line)
            walk(buf, val_first, val_last, schema, depth + 1)
        else
            print(line)
            walk(buf, val_first, val_last, spec.type, depth + 1)
        end

        pos = val_last + 1
    end
end

--------------------------------------------------------------------------------
-- Packets. Read the expected tree in the comment, then run and compare.
--------------------------------------------------------------------------------

-- Packet A: 词典里 1/2/3/4 同级；报文里 2/3/4 在 01 00 17 的 23 字节 Value 里。
--
-- Expected（缩进 = 报文树，不是 schema 并列）:
-- TLV 1 (23 bytes) - Sequence
--   TLV 2 (2 bytes) - SeqNum = 7
--   TLV 3 (1 bytes) - Operation = 2 (Write)
--   TLV 4 (11 bytes) - Device
--     TLV 1 (4 bytes) - Name = rpd0   <-- Type 1 is Name here, not Sequence
--     TLV 2 (1 bytes) - Id = 3        <-- Type 2 is Id here, not SeqNum
local packet_a = hex_to_bytes([[
    01 00 17
       02 00 02 00 07
       03 00 01 02
       04 00 0B
          01 00 04 72 70 64 30
          02 00 01 03
]])

-- Packet B: Sequence { SeqNum=7, Channel { ChanIndex=2, Device { Name=rpd0, Id=3 } } }
--
-- Channel.type = {} so Device(4) is looked up in the ROOT schema.
-- Expected:
-- TLV 1 (26 bytes) - Sequence
--   TLV 2 (2 bytes) - SeqNum = 7
--   TLV 5 (18 bytes) - Channel
--     TLV 6 (1 bytes) - ChanIndex = 2
--     TLV 4 (11 bytes) - Device
--       TLV 1 (4 bytes) - Name = rpd0
--       TLV 2 (1 bytes) - Id = 3
local packet_b = hex_to_bytes([[
    01 00 1A
       02 00 02 00 07
       05 00 12
          06 00 01 02
          04 00 0B
             01 00 04 72 70 64 30
             02 00 01 03
]])

local function dump(title, buf)
    print(title)
    print(string.rep("-", #title))
    walk(buf, 1, #buf, schema, 0)
    print()
end

print([[
词典（schema）: 1 Sequence / 2 SeqNum / 3 Operation / 4 Device 在根表同级。
报文树（hex） : 谁的字节在谁的 Length 窗口里，打印就低一层。
                01 00 17 => Type=1, Length=0x0017=23，后面 23 字节属于 Sequence。
type={}       : 人还在袋子的 Value 里，查门牌却回大厅。
]])
dump("Packet A: nested Device (local dictionary)", packet_a)
dump("Packet B: Channel bag reopens the root schema", packet_b)
