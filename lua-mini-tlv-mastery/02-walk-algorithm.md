# 02 · walk 算法

> 代码：[code/mini_tlv.lua](code/mini_tlv.lua)。语言点已在 [01-lua-language-points.md](01-lua-language-points.md) 钉过，这里只讲控制流：字节怎么变成树。

## 1. 数据从哪来

```text
[[ hex dump ]]
      |
      v
 hex_to_bytes()     -- 去注释、去空白、每两字符变 1 字节
      |
      v
 Lua string buf     -- 1-based，#buf 是总字节数
      |
      v
 dump(title, buf)
      |
      v
 walk(buf, 1, #buf, schema, 0)
```

`dump` 只负责标题和一条横线，真正走路的是 `walk`：

```213:218:lua-mini-tlv-mastery/code/mini_tlv.lua
local function dump(title, buf)
    print(title)
    print(string.rep("-", #title))
    walk(buf, 1, #buf, schema, 0)
    print()
end
```

入口约定：整包是一层沙箱 `[1, #buf]`，词典是根 `schema`，深度 0。

## 2. 读头，圈沙箱

```129:143:lua-mini-tlv-mastery/code/mini_tlv.lua
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
```

一个 TLV 最少 3 字节：Type(1) + Length(2)。`pos + 2 > last` 表示连头都读不完。

索引（全部 1-based）：

```text
pos          Type
pos+1        Length 高字节
pos+2        Length 低字节
pos+3        Value 第一字节 = val_first
pos+2+len    Value 最后一字节 = val_last
pos+3+len    下一个 TLV 的 Type = val_last + 1
```

`len` 只计 Value。Value 为空时 `len == 0`，`val_last = pos + 2`，下一跳仍是 `val_last + 1 = pos + 3`，即跳过 3 字节的空 TLV。

沙箱检查：

```145:148:lua-mini-tlv-mastery/code/mini_tlv.lua
        if val_last > last then
            print(string.format("%sTLV %d length %d walks past sandbox", indent, t, len))
            return
        end
```

`last` 是**当前**窗口的右端，不一定是整包末尾。嵌套时，子 `walk` 的 `last` 是外层 Value 的 `val_last`。Length 声称走到窗口外面，立刻停，避免 `string.byte` 读出 `nil` 再做算术。

## 3. 查当前词典，走三扇门

```149:166:lua-mini-tlv-mastery/code/mini_tlv.lua
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
```

```text
                dict[t]
                   |
     +-------------+-------------+
     |             |             |
   nil          string         table
     |             |             |
  Undefined     decode         next(type)
  不递归         停在本行          |
                           +-----+-----+
                           |           |
                      empty {}     non-empty
                           |           |
                    递归, dict=    递归, dict=
                    根 schema      spec.type
```

四种出口的共同点：无论是否递归，本层 `pos` 都跳到 `val_last + 1`。递归消耗的是 **Value 窗口里的子 TLV**，不是本层的 `pos`。兄弟 TLV 靠这个加法排好队。

`spec == nil` 时不递归：整段 Value 当作不透明字节丢掉。练习 3 把 `Channel` 改成局部子表之后，里面的 `Device(4)` 就会走这条路。

## 4. decode_leaf：叶子的 Value 属于本节点

```85:105:lua-mini-tlv-mastery/code/mini_tlv.lua
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
```

叶子不打开下一层词典。Value 解码完写在同一行：`TLV 2 (2 bytes) - SeqNum = 7`。

`UINT16` 强制 `n == 2`，和线格式约定一致：类型说自己是 16 位整数，Length 却不是 2，就标坏长度，不瞎读。`valuestring` 只在 UINT8 / UINT16 上查，STRING 没有枚举。

## 5. 一次递归调用改了什么、没改什么

| 参数 | 叶子 | `type = {}` | `type = { ... }` |
|---|---|---|---|
| `buf` | 同一份 string | 同一份 | 同一份 |
| `first` / `last` | 不递归 | 换成 Value 窗口 | 换成 Value 窗口 |
| `dict` | 不递归 | **根 `schema`** | **`spec.type`** |
| `depth` | 不递归 | +1 | +1 |

`buf` 从不切开。Lua string 不可变，切片是新串，贵而且不必。walker 用闭区间下标表示窗口，和 C 里 `ptr + len` 是同一想法。

## 6. 和真实插件的算法对齐

| 本文件 | `rcp.lua` / `rcp_tlv_table.lua` |
|---|---|
| `schema` | `M.tlv_table` |
| `walk` | `dissect_tlv` |
| `first` / `last` | `cur_value_tvb` 的长度沙箱 |
| `dict` | 当前 TLV 表（根表或 sub-TLV 表） |
| `decode_leaf` | 按 field 类型读 tvb |
| `type = {}` | Sequence / Channel 一类袋子，孩子仍用顶层表 |

`_create_tlv_protofield` 是把同一棵规格树编成 Wireshark 字段，不是解析算法。学走路时可以忽略它。

下一步用两包报文把三扇门都走一遍：[03-packet-walkthrough.md](03-packet-walkthrough.md)。
