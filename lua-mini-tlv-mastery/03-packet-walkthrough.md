# 03 · Packet A / Packet B 逐步对照

> 先读 [code/mini_tlv.lua](code/mini_tlv.lua) 里两包的 Expected 注释，再对照本节。算法见 [02-walk-algorithm.md](02-walk-algorithm.md)。

跑一遍：

```text
lua5.4 lua-mini-tlv-mastery/code/mini_tlv.lua
```

打印树必须和下面两节的 Expected 一致。不一致时先查 Length 有没有把沙箱圈错，再查当时用的是哪本词典。

## 1. Packet A：走进子馆

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

### 1.1 字节怎么切

```text
offset  hex        含义
1       01         Type = 1
2-3     00 17      Length = 23
4-26    ...        Sequence 的 Value（23 字节）

  4     02         Type = 2
  5-6   00 02      Length = 2
  7-8   00 07      SeqNum = 7

  9     03         Type = 3
  10-11 00 01      Length = 1
  12    02         Operation = 2

  13    04         Type = 4
  14-15 00 0B      Length = 11
  16-26 ...        Device 的 Value（11 字节）

    16    01       Type = 1   <-- 这里不再是 Sequence
    17-18 00 04    Length = 4
    19-22 72 70 64 30   "rpd0"

    23    02       Type = 2   <-- 这里不再是 SeqNum
    24-25 00 01    Length = 1
    26    03       Id = 3
```

23 = 5 (SeqNum TLV) + 4 (Operation TLV) + 14 (Device TLV 头 3 + Value 11)。

### 1.2 词典怎么换

入口：`walk(buf, 1, 26, schema, 0)`。

1. Type 1，`schema[1]` 是 Sequence，`type = {}`。打印 `TLV 1 (23 bytes) - Sequence`，递归 `walk(buf, 4, 26, schema, 1)`——**沙箱变了，词典仍是根表**。
2. Type 2，根表里是 SeqNum / UINT16。叶子，打印 `= 7`，不递归。
3. Type 3，Operation / UINT8，`valuestring[2] = "Write"`。打印 `= 2 (Write)`。
4. Type 4，Device，`type` 是非空表。打印 `TLV 4 (11 bytes) - Device`，递归 `walk(buf, 16, 26, spec.type, 2)`——**沙箱和词典都换了**。
5. 子馆里 Type 1 是 Name，Type 2 是 Id。同是 1 和 2，意思已经不是 Sequence / SeqNum。

### 1.3 Expected

```text
TLV 1 (23 bytes) - Sequence
  TLV 2 (2 bytes) - SeqNum = 7
  TLV 3 (1 bytes) - Operation = 2 (Write)
  TLV 4 (11 bytes) - Device
    TLV 1 (4 bytes) - Name = rpd0
    TLV 2 (1 bytes) - Id = 3
```

词典里 1 / 2 / 3 / 4 同级；树上 2 / 3 / 4 低于 1，因为它们的字节在 Sequence 的 23 字节窗口里。

## 2. Packet B：空表回大厅

```203:211:lua-mini-tlv-mastery/code/mini_tlv.lua
local packet_b = hex_to_bytes([[
    01 00 1A
       02 00 02 00 07
       05 00 12
          06 00 01 02
          04 00 0B
             01 00 04 72 70 64 30
             02 00 01 03
]])
```

### 2.1 字节怎么切

```text
01 00 1A                 Sequence，Length = 26
   02 00 02 00 07          SeqNum = 7
   05 00 12                Channel，Length = 18
      06 00 01 02            ChanIndex = 2
      04 00 0B               Device，Length = 11
         01 00 04 72 70 64 30  Name = rpd0
         02 00 01 03           Id = 3
```

26 = 5 (SeqNum) + 21 (Channel 头 3 + Value 18)。
18 = 4 (ChanIndex) + 14 (Device 头 3 + Value 11)。

### 2.2 Channel 为什么能装 Device

`schema[5]` 是 Channel，`type = {}`。走进 Channel 的 18 字节窗口时，词典被设回**根** `schema`，不是一张空的「Channel 专用表」。

所以窗口里的 Type 6 仍是 `ChanIndex`，Type 4 仍是 `Device`。`Device` 自己再换到局部子馆，Name / Id 的门牌重新从 1 数。

若误把 `type = {}` 读成「没有孩子」，Channel 就会被当成叶子或未知容器，Packet B 整棵树会塌。

### 2.3 Expected

```text
TLV 1 (26 bytes) - Sequence
  TLV 2 (2 bytes) - SeqNum = 7
  TLV 5 (18 bytes) - Channel
    TLV 6 (1 bytes) - ChanIndex = 2
    TLV 4 (11 bytes) - Device
      TLV 1 (4 bytes) - Name = rpd0
      TLV 2 (1 bytes) - Id = 3
```

## 3. 两包对照

```text
Packet A 练的：走进子馆。
  根层 Type 1 / 2 = Sequence / SeqNum
  Device 里 Type 1 / 2 = Name / Id

Packet B 练的：空表回大厅。
  Channel 没有自己的子词典
  Device(4) 仍按根表解释
```

```text
              Packet A                         Packet B
                 |                                |
                 v                                v
            Sequence {}                      Sequence {}
           /    |     \                      /         \
      SeqNum Operation Device           SeqNum       Channel {}
                       |                             /        \
                    子馆 {1,2}                 ChanIndex    Device
                    Name / Id                              子馆 {1,2}
```

两包的 Device 叶子完全一样（`rpd0` / `Id=3`）。差别只在 Device 外面套的是谁，以及套它的那个容器用的是哪本词典。

下一步只改表和 hex，不改 `walk`：[04-exercises.md](04-exercises.md)。
