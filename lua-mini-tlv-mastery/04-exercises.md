# 04 · 练习（只改表和 hex，不要改 walk）

> 规则和原教学 README 相同：做完这些，就是在用和真实插件相同的方法加 TLV。主文件 [code/mini_tlv.lua](code/mini_tlv.lua) 保持与源示例一致，答案不要写回去。

每题先自己改一份副本、跑 `lua5.4`，再对预期树。`walk` / `decode_leaf` / `hex_to_bytes` 一行都不要动。

## 1. 根表加叶子 Note(7)

在根 `schema` 加：

```lua
[7] = { name = "Note", type = "STRING" },
```

给 Packet A 末尾——仍在 Sequence 的 Value 里——补一个 Note。字符串用 `"hi"`（`68 69`）。

Sequence 原 Length 是 `0x0017` = 23。新 TLV 是：

```text
07 00 02 68 69
```

5 字节（Type 1 + Length 2 + Value 2）。新 Length = 23 + 5 = 28 = `0x001C`。

```lua
local packet_a = hex_to_bytes([[
    01 00 1C
       02 00 02 00 07
       03 00 01 02
       04 00 0B
          01 00 04 72 70 64 30
          02 00 01 03
       07 00 02 68 69
]])
```

Expected：

```text
TLV 1 (28 bytes) - Sequence
  TLV 2 (2 bytes) - SeqNum = 7
  TLV 3 (1 bytes) - Operation = 2 (Write)
  TLV 4 (11 bytes) - Device
    TLV 1 (4 bytes) - Name = rpd0
    TLV 2 (1 bytes) - Id = 3
  TLV 7 (2 bytes) - Note = hi
```

Note 和 SeqNum / Device 一样走根词典，因为它写在 Sequence 的 Value 里，而 Sequence 的 `type = {}` 会把词典设回大厅。忘了改 `01 00 17` 的话，`walk` 会在沙箱右端截断，Note 进不去，或报 `walks past sandbox`。

## 2. Device 加子 TLV Role(3)

给 `Device.type` 加：

```lua
[3] = {
    name = "Role",
    type = "UINT8",
    valuestring = {
        [1] = "core",
        [2] = "rpd",
    },
},
```

只改表、不改 hex 时，Packet A / B 的树不变——多出来的门牌没有人敲门。把 Role=2 写进 Device 的 Value，才看得到枚举：

```text
03 00 01 02
```

4 字节。Device 原 Length `0x000B` = 11，新 Length = 15 = `0x000F`。Sequence 原 23，再加 4，变成 27 = `0x001B`。

```lua
local packet_a = hex_to_bytes([[
    01 00 1B
       02 00 02 00 07
       03 00 01 02
       04 00 0F
          01 00 04 72 70 64 30
          02 00 01 03
          03 00 01 02
]])
```

Expected：

```text
TLV 1 (27 bytes) - Sequence
  TLV 2 (2 bytes) - SeqNum = 7
  TLV 3 (1 bytes) - Operation = 2 (Write)
  TLV 4 (15 bytes) - Device
    TLV 1 (4 bytes) - Name = rpd0
    TLV 2 (1 bytes) - Id = 3
    TLV 3 (1 bytes) - Role = 2 (rpd)
```

这里的 Type 3 是 Role，不是根表的 Operation。门牌只在当前词典里有效。

## 3. 把 Channel 改成局部子表

预测：若把 `Channel` 从 `type = {}` 改成带局部子表、不再回大厅，Packet B 里的 `Device(4)` 会打印成什么？

改完再跑，核对预测。一种最小改法：

```lua
[5] = {
    name = "Channel",
    type = {
        [6] = { name = "ChanIndex", type = "UINT8" },
    },
},
```

hex 不动。走进 Channel 之后，`dict` 变成这张只有 `[6]` 的表。Type 6 仍是 ChanIndex。Type 4 在这本词典里不存在：`spec == nil`，名字是 `Undefined`，**不递归**，Device 肚子里的 Name / Id 整段被跳过。

Expected：

```text
TLV 1 (26 bytes) - Sequence
  TLV 2 (2 bytes) - SeqNum = 7
  TLV 5 (18 bytes) - Channel
    TLV 6 (1 bytes) - ChanIndex = 2
    TLV 4 (11 bytes) - Undefined
```

若希望 Device 在 Channel 里仍然合法，必须把 Device 写进 Channel 的局部表（或继续用 `type = {}` 回大厅）。真实插件里 Sequence / RfChannel / RfPort 用空表，就是为了让顶层对象能装在袋子里，而不给每个袋子抄一份根表。
