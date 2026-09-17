# 01 · 从 mini_tlv 抽出的 Lua 语言点

> 对照 [code/mini_tlv.lua](code/mini_tlv.lua)。这里不按官方手册列 API，只钉这份实现真正用到的惯用法。心智模型见 [00-two-hierarchies.md](00-two-hierarchies.md)。

## 1. 未定义的全局是 nil：环境探测

```16:18:lua-mini-tlv-mastery/code/mini_tlv.lua
if Proto ~= nil then
    return
end
```

Lua 里读一个从未赋值的全局，得到的是 `nil`，不是异常。Wireshark 加载脚本时会注入 `Proto`；在普通 `lua5.4` 里 `Proto` 不存在，条件为假，文件继续往下跑。

这是「探测宿主」的最小写法。仓库若被软链进 Wireshark 插件目录，这个防护避免把教学 walker 误登记成协议。

## 2. 长字符串 `[[...]]` 当 hex dump

```183:190:lua-mini-tlv-mastery/code/mini_tlv.lua
local packet_a = hex_to_bytes([[
    01 00 17
       02 00 02 00 07
       03 00 01 02
       04 00 0B
          01 00 04 72 70 64 30
          02 00 01 03
]])
```

`[[...]]` 是原始长字符串：里面的换行、空格、缩进都保留，不必写 `"\n"`。教学包把报文写成可对齐的 hex，比一长串 `"010017..."` 好读。

文件末尾的说明也是同一语法：

```220:225:lua-mini-tlv-mastery/code/mini_tlv.lua
print([[
词典（schema）: 1 Sequence / 2 SeqNum / 3 Operation / 4 Device 在根表同级。
报文树（hex） : 谁的字节在谁的 Length 窗口里，打印就低一层。
                01 00 17 => Type=1, Length=0x0017=23，后面 23 字节属于 Sequence。
type={}       : 人还在袋子的 Value 里，查门牌却回大厅。
]])
```

## 3. `gsub` 去注释 / 空白，再按两个十六进制字符切开

```67:75:lua-mini-tlv-mastery/code/mini_tlv.lua
local function hex_to_bytes(hex)
    hex = hex:gsub("%-%-[^\n]*", ""):gsub("%s+", "")
    if #hex % 2 ~= 0 then
        error("odd number of hex digits")
    end
    return (hex:gsub("..", function(byte)
        return string.char(tonumber(byte, 16))
    end))
end
```

三件事叠在一次清洗 + 一次替换里：

1. `"%-%-[^\n]*"` 吃掉 `--` 到行尾的注释。模式里 `-` 要写成 `%-`，否则被当成「前一元素可重复 0 次以上」（Lua pattern 的 `-` 是非贪婪重复，不是减号）。
2. `"%s+"` 丢掉空白，于是缩进和对齐不再进入字节流。
3. `".."` 每次吃两个字符。替换值不是字符串，而是 **function**：`tonumber(byte, 16)` 把 `"72"` 变成 `0x72`，`string.char` 再变成一字节。`gsub` 把每次返回的字符拼成新串。

外层括号 `return (hex:gsub(...))` 是为了只留下第一个返回值。`gsub` 本来返回 `(新串, 替换次数)`；包一层之后，调用方拿到的是纯字符串，不会把次数泄漏给后面的赋值。

`#hex` 是清洗后的字符个数，不是字节语义上的「元素个数」——此时 hex 还是 ASCII 文本。奇数个十六进制数字说明 dump 写坏了，直接 `error`。

## 4. `string` 就是只读字节缓冲

Lua 没有独立的 `bytes` 类型。一份二进制报文就是普通 `string`：

```77:83:lua-mini-tlv-mastery/code/mini_tlv.lua
local function u8(buf, i)
    return string.byte(buf, i)
end

local function u16(buf, i)
    return u8(buf, i) * 256 + u8(buf, i + 1)
end
```

要点：

- 下标从 **1** 开始。`walk(buf, 1, #buf, schema, 0)` 的 `1` 不是「跳过第 0 字节」，而是缓冲的第一字节。
- `string.byte(buf, i)` 返回 `0..255` 的整数。
- `buf:sub(first, last)` 切片，两端都包含。`last - first + 1` 才是长度。
- `#buf` 是字节数（Lua 把 string 当字节序列，不是 UTF-8 码点序列）。
- `u16` 手写大端：高字节 `* 256` + 低字节。对应线格式 Length 的 2 字节 BE。

`decode_leaf` 读 STRING 时直接 `buf:sub(first, last)`，因为 Value 窗口里的字节已经是 ASCII。`72 70 64 30` 就是 `"rpd0"`。

## 5. 数值键 schema：门牌号是 table 的 key

```40:61:lua-mini-tlv-mastery/code/mini_tlv.lua
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
```

必须写 `[1] = ...`。若写成 `{ { name = "Sequence", ... }, ... }`，Lua 也会从 1 编号，但意图被「数组字面量」淹没；门牌号是协议规定的 Type，不是「第几个写进表」。`walk` 用 `dict[t]` 按报文里读到的整数去查，和书写顺序无关。

`Device.type` 是另一张同样形状的表。子馆里 Type 重新从 1 数：这里的 `[1]` 是 Name，不是 Sequence。

## 6. `type()` 三分派，`next(t) == nil` 判空表

```153:164:lua-mini-tlv-mastery/code/mini_tlv.lua
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
```

`type(x)` 返回类型名字符串：`"nil"` / `"boolean"` / `"number"` / `"string"` / `"table"` / `"function"` / `"userdata"` / `"thread"`。叶子的 `spec.type` 是 `"UINT8"` 这类字符串，所以先用 `type(spec.type) == "string"` 切开。

剩下的都是 table。空表 `{}` 和非空表 `{ [1]=... }` 在 `type()` 看来都是 `"table"`，必须再测「有没有第一个键」：

- `next(t)` 返回表里的一对 `k, v`；空表返回 `nil`。
- `next(t) == nil` 因此是「这张表一个字段都没有」。

**不要用 `#t == 0`。** `#` 只对数组部分有定义。`{ [1] = ..., [2] = ... }` 的 `#` 碰巧是 2，但 `{ name = "x" }` 或将来某张只用哈希键的子表，`#` 是 0。`type = {}` 要表达的是「没有子门牌」，不是「数组长度为 0」。`next` 不区分数组键和哈希键，空就是空。

未知 Type：`dict[t]` 为 `nil`。`spec and spec.name or "Undefined"` 是短路默认值——`spec` 为假时不碰 `.name`，避免对 `nil` 取字段。此时只打印一行，**不递归**，整个 Value 窗口被 `pos = val_last + 1` 跳过。

## 7. 闭包：大厅必须能摸到根 schema

```160:160:lua-mini-tlv-mastery/code/mini_tlv.lua
            walk(buf, val_first, val_last, schema, depth + 1)
```

`walk` 是 `local function`，参数列表里没有「根表」。`type = {}` 回大厅时，它直接写外层的 `schema`。这是闭包：内层函数记住定义处的那个 upvalue。

若把根表也做成参数，每次调用都得往下传 `root`，大厅和当前词典就容易被传混。现在的形状是：当前词典走参数 `dict`，根词典走闭包。两根指针的职责在调用约定上就分开了。

## 8. `valuestring` 查表，`or` 给默认名

```107:116:lua-mini-tlv-mastery/code/mini_tlv.lua
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
```

`Operation = 2` 打印成 `2 (Write)`，因为 `valuestring[2] == "Write"`。缺枚举时保持纯数字，不报错。

名字一侧：

```lua
local name = spec and spec.name or "Undefined"
```

`and` / `or` 返回操作数本身，不是布尔。`spec` 存在则取 `spec.name`；否则整条 `and` 为假，`or` 落到 `"Undefined"`。

## 9. `string.rep` 缩进，`string.format` 行输出

```131:132:lua-mini-tlv-mastery/code/mini_tlv.lua
    local pos = first
    local indent = string.rep("  ", depth)
```

`string.rep("  ", depth)` 按报文树深度重复两个空格。`depth` 只跟沙箱套层有关，跟当前词典无关。

```lua
string.format("%sTLV %d (%d bytes) - %s", indent, t, len, name)
```

`%s` / `%d` 与 C `printf` 同类。标题下划线用 `string.rep("-", #title)`，长度跟标题字符数对齐。

## 10. 对照速查

| 语言点 | 在本文件里做什么 |
|---|---|
| 未赋值全局是 `nil` | `Proto ~= nil` 探测 Wireshark |
| `[[...]]` | hex dump 和多行说明 |
| `gsub` + function | hex 文本 -> 二进制 string |
| `string.byte` / `sub` / `#` | 1-based 字节缓冲 |
| `[n] = { ... }` | Type 号当 key 的 schema |
| `type()` + `next(t)` | 叶子 / 大厅 / 子馆三分派 |
| 闭包 upvalue | `type={}` 时回到根 `schema` |
| `and` / `or` | `Undefined` 默认名 |
| `valuestring[raw]` | 枚举数字 -> 可读标签 |
| `string.rep` / `format` | 缩进和一行 TLV |

下一步按调用顺序把这些点串成算法：[02-walk-algorithm.md](02-walk-algorithm.md)。
