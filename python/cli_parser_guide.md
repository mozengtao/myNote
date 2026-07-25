# Python 解析 CLI / Log / Table 文本的通用心智模式

> 核心思想：
>
> **不要把文本看成字符串，而要把它看成一种"序列化的数据结构(Data Serialization)"。**
>
> Parser 的目标不是处理字符串，而是**恢复(Recover)出原来的数据模型(Data Model)**。

---

# 一、统一的数据流模型

几乎所有 CLI、日志、文本表格都符合下面的数据流：

```text
          Raw Text
              │
              ▼
      Line Iterator
              │
              ▼
     Context Recognition
              │
              ▼
      Parse One Record
              │
              ▼
    Normalized Python Object
              │
              ▼
 Filter / Group / Aggregate
              │
              ▼
      Report / CSV / JSON
```

不要一边解析一边统计。

永远遵循：

> Parse First → Analyze Later

---

# 二、第一步：不要写代码，先识别 Pattern

先回答下面几个问题：

```
文本真正表示什么对象？
```

例如：

```
show cable modem
```

真正的数据其实是

```
CM
    ├── Channel
    ├── Channel
    └── Channel
```

而不是

```
字符串
字符串
字符串
```

这是最重要的第一步。

---

# 三、五种最常见的文本 Pattern

---

## Pattern 1：固定列 Table（Flat Table）

例如：

```
PID     USER     CPU
123     root     12
456     mysql    33
```

特点：

- 每一行都是独立对象
- 无上下文
- 无层级

数据模型：

```
Process
├── pid
├── user
└── cpu
```

Python：

```python
for line in lines:
    fields = line.split()

    process = {
        "pid": fields[0],
        "user": fields[1],
        "cpu": fields[2],
    }
```

心智模型：

```
一行 == 一个对象
```

无需状态机。

---

## Pattern 2：Context + Detail（父子结构）

例如：

```
CM MAC
38:ad...

    UCID 0/13
    UCID 0/12
    UCID 0/0

38:ad...

    UCID ...
```

特点：

父对象出现一次

子对象重复很多次

数据模型：

```
CM
 ├── Channel
 ├── Channel
 └── Channel
```

必须维护：

```
current_cm
```

流程：

```
发现新CM

↓

更新Context

↓

所有Channel继承Context
```

Python：

```python
current_cm = None

for line in file:

    if has_new_cm(line):
        current_cm = parse_cm(line)

    channel = parse_channel(line)

    if channel:
        records.append({
            "cm": current_cm,
            **channel
        })
```

心智模型：

```
Parser = State Machine
```

---

## Pattern 3：Section（章节结构）

例如：

```
Interface eth0

RX packets ...

TX packets ...

Interface eth1

RX packets ...

TX packets ...
```

数据模型：

```
Interface
    RX
    TX
```

维护：

```
current_interface
```

流程：

```
新Section

↓

更新状态

↓

解析内部内容
```

Python：

```python
current_if = None

for line in file:

    if line.startswith("Interface"):
        current_if = ...

    elif line.startswith("RX"):
        ...

    elif line.startswith("TX"):
        ...
```

心智模型：

```
Section = Context
```

---

## Pattern 4：Key-Value Log

例如：

```
time=...
host=...
level=INFO
msg=...
```

或者

```
INFO pid=123 cost=45
```

数据模型：

```
LogRecord
```

通常：

Regex

或者

split("=")

Python：

```python
record = {}

for token in line.split():

    if "=" in token:
        k, v = token.split("=")

        record[k] = v
```

心智模型：

```
一行

↓

Dictionary
```

---

## Pattern 5：Block（多行组成一个对象）

例如：

```
BEGIN

MAC
...

UCID
...

END
```

数据模型：

```
Block
```

维护：

```
current_block
```

Python：

```python
block = []

for line in file:

    if line == "BEGIN":
        block = []

    elif line == "END":
        parse(block)

    else:
        block.append(line)
```

心智模型：

```
多行

↓

一个对象
```

---

# 四、Parser 的三层抽象

建议永远分三层。

```
文本

↓

Parser

↓

Python Object

↓

Analyzer

↓

Result
```

例如：

```
show command

↓

parse()

↓

records[]

↓

filter()

↓

statistics()

↓

report()
```

不要：

```
for line:

    parse

    if ...

    sum

    print

    write csv
```

否则 Parser 和业务逻辑会越来越难维护。

---

# 五、推荐的数据结构

永远尽量统一 Record。

例如：

```python
{
    "cm": "...",
    "ucid": "...",
    "uncorrectable": 47,
    "no_burst": 1530,
}
```

而不是：

```
变量1
变量2
变量3
```

统一对象以后：

```
CSV

JSON

排序

统计

过滤

全部统一
```

---

# 六、Parser 的标准框架

推荐固定模板：

```python
def parse(filename):

    context = {}

    records = []

    with open(filename) as f:

        for line in f:

            #
            # Step1
            # 更新 Context
            #
            update_context(context, line)

            #
            # Step2
            # 解析 Record
            #
            record = parse_record(context, line)

            if record:
                records.append(record)

    return records
```

业务逻辑：

```python
records = parse(file)

for r in records:

    if need_report(r):

        print(r)
```

---

# 七、状态机(State Machine)心智模型

Parser 本质就是：

```
          当前状态
               │
               ▼
        读取一行文本
               │
      ┌────────┴────────┐
      │                 │
 更新状态          产生Record
      │                 │
      └────────┬────────┘
               │
            下一行
```

状态通常只有几个：

```
current_cm

current_interface

current_host

current_session

current_time
```

状态永远保存在：

```
for循环外
```

而不是：

```
for循环里面
```

---

# 八、CLI / Log Parser 的黄金法则

## 黄金法则一

不要解析字符串。

而是恢复数据模型。

```
Text

↓

Object
```

---

## 黄金法则二

Parser 与业务逻辑完全分离。

```
Parser

↓

Python Object

↓

Business Logic
```

---

## 黄金法则三

Context 永远比 Regex 更重要。

很多 CLI 输出：

```
MAC

↓

Channel

↓

Channel

↓

Channel
```

真正需要维护的是：

```
current_mac
```

而不是写越来越复杂的 Regex。

---

## 黄金法则四

Record 尽量扁平化（Normalize）。

例如：

```python
{
    "cm": "...",
    "ucid": "...",
    "corrected": ...,
    "uncorrectable": ...
}
```

后续：

```
filter()

groupby()

sort()

json

csv
```

全部复用。

---

## 黄金法则五

Parser 应该只做两件事：

```
① 更新 Context

② 产生 Record
```

不要：

```
统计

打印

写文件

发送网络

数据库
```

---

# 九、统一心智模型（推荐牢记）

```
                Raw Text
                    │
                    ▼
         识别 Pattern（Pattern Recognition）
                    │
                    ▼
        恢复数据模型（Recover Data Model）
                    │
                    ▼
       Context + State Machine Parser
                    │
                    ▼
      Normalized Python Records (list[dict])
                    │
                    ▼
       Filter / Sort / Group / Aggregate
                    │
                    ▼
          Report / CSV / JSON / Database
```

一句话总结：

> **CLI、日志、表格等半结构化文本解析，本质上都是"模式识别（Pattern Recognition）→ 上下文维护（Context Management）→ 状态机解析（State Machine Parsing）→ 数据模型恢复（Recover Data Model）→ 业务分析（Analysis）"的过程。不要把 Parser 当作字符串处理程序，而要把它看成一个把文本恢复为 Python 数据对象的反序列化器（Deserializer）。**

---

# 十、10个典型示例：从文本到数据模型

下面用 10 组"输入文本 + Python代码"，把前面九节讲的心智模型逐一落地成可以直接照抄的代码范式。

---

## 示例 1：定长表格（Flat Table，split）

输入文本：

```text
MAC Address      IP Address      State      Channel
0018.6811.1234   10.1.1.5        online     3
0018.6811.5678   10.1.1.6        online     5
0018.6811.9abc   10.1.1.7        offline    -
```

Python：

```python
records = []

for line in lines[1:]:
    fields = line.split()

    records.append({
        "mac": fields[0],
        "ip": fields[1],
        "state": fields[2],
        "channel": fields[3],
    })
```

心智模型：一行文本 == 一条扁平记录，`split()` 就是最简单的反序列化器。

---

## 示例 2：含空格字段的表格（maxsplit）

输入文本：

```text
USER   PID  CPU  COMMAND
root   1    0.0  /sbin/init splash
mysql  842  1.2  /usr/sbin/mysqld --datadir=/var/lib/mysql
```

Python：

```python
records = []

for line in lines[1:]:
    user, pid, cpu, command = line.split(maxsplit=3)

    records.append({
        "user": user,
        "pid": int(pid),
        "cpu": float(cpu),
        "command": command,
    })
```

心智模型：当最后一列本身含有空格时，`split(maxsplit=N)` 才能保留它的完整性，否则字段会被错误切碎。

---

## 示例 3：父子上下文（Context + State）

输入文本：

```text
RPD rpd-001

    MD-US 0    locked
    MD-US 1    locked

RPD rpd-002

    MD-US 0    unlocked
```

Python：

```python
current_rpd = None
records = []

for raw in lines:
    line = raw.rstrip()

    if line.startswith("RPD"):
        current_rpd = line.split()[1]
        continue

    if line.strip().startswith("MD-US"):
        _, md_id, state = line.split()

        records.append({
            "rpd": current_rpd,
            "md_us": md_id,
            "state": state,
        })
```

心智模型：`current_rpd` 是循环外维护的状态，子记录必须继承它才有完整含义。

---

## 示例 4：Section 分节 + 正则提取

输入文本：

```text
Interface eth0

RX packets 102934 bytes 88291029
TX packets 88123 bytes 71203981

Interface eth1

RX packets 552 bytes 44192
TX packets 601 bytes 51203
```

Python：

```python
import re

current_if = None
records = {}

for raw in lines:
    line = raw.strip()

    m = re.match(r"Interface (\S+)", line)
    if m:
        current_if = m.group(1)
        records[current_if] = {}
        continue

    m = re.match(r"(RX|TX) packets (\d+) bytes (\d+)", line)
    if m and current_if:
        direction, packets, byte_count = m.groups()

        records[current_if][direction.lower()] = {
            "packets": int(packets),
            "bytes": int(byte_count),
        }
```

心智模型：Section 标题只是切换 Context，真正的数据在后续行里靠正则捕获。

---

## 示例 5：Key=Value 日志（正则批量捕获）

输入文本：

```text
time=2026-07-24T10:00:01 host=cmts-01 level=INFO msg=link_up
time=2026-07-24T10:00:05 host=cmts-01 level=ERROR msg=carrier_loss
```

Python：

```python
import re

records = []

for line in lines:
    fields = dict(re.findall(r"(\w+)=(\S+)", line))
    records.append(fields)
```

心智模型：`key=value` 序列本质上就是字典的线性编码，`re.findall` 可以直接把它还原成 dict。

---

## 示例 6：BEGIN/END 块（多行拼成一个对象）

输入文本：

```text
BEGIN

mac 0018.6811.1234
ucid 0/13
power 45.2

END

BEGIN

mac 0018.6811.5678
ucid 0/12
power 43.9

END
```

Python：

```python
blocks = []
current_block = None

for raw in lines:
    line = raw.strip()

    if line == "BEGIN":
        current_block = {}
    elif line == "END":
        blocks.append(current_block)
        current_block = None
    elif line and current_block is not None:
        key, value = line.split(maxsplit=1)
        current_block[key] = value
```

心智模型：`BEGIN`/`END` 只是记录的边界符，块内部本身仍然是一组 key-value，可以复用示例 5 的解析方式。

---

## 示例 7：空行分隔的多行记录（Blank-line-delimited Records）

输入文本：

```text
name: eth0
ip: 10.1.1.5
mac: 00:11:22:33:44:55

name: eth1
ip: 10.1.1.6
mac: 00:11:22:33:44:56
```

Python：

```python
records = []
current = {}

for raw in lines:
    line = raw.strip()

    if line == "":
        if current:
            records.append(current)
            current = {}
        continue

    key, value = line.split(":", 1)
    current[key.strip()] = value.strip()

if current:
    records.append(current)
```

心智模型：空行是记录之间的分隔符，遇到空行就把当前累积的字典 flush 出去；循环结束后别忘了再 flush 最后一条。

---

## 示例 8：缩进层级表示树形结构（Indentation as Hierarchy）

输入文本：

```text
eth0
    vlan10
        10.1.10.1/24
    vlan20
        10.1.20.1/24
eth1
    10.1.30.1/24
```

Python：

```python
def indent_of(line):
    return len(line) - len(line.lstrip(" "))

root = []
stack = [(-1, root)]

for raw in lines:
    if not raw.strip():
        continue

    depth = indent_of(raw)
    name = raw.strip()

    while stack and stack[-1][0] >= depth:
        stack.pop()

    node = {"name": name, "children": []}
    stack[-1][1].append(node)
    stack.append((depth, node["children"]))
```

心智模型：缩进量就是树的深度，用一个 `(depth, parent_children)` 栈就能把平铺的行还原成任意层级的树。

---

## 示例 9：定宽字段（Fixed-width columns，切片而非分隔符）

输入文本：

```text
NAME      STATE   FREQ_MHZ
US0       LOCKED     35.6
US1       LOCKED     40.2
US2     UNLOCKED      0.0
```

Python：

```python
COLUMNS = [
    ("name", 0, 10),
    ("state", 10, 18),
    ("freq_mhz", 18, 28),
]

records = []

for line in lines[1:]:
    record = {}

    for key, start, end in COLUMNS:
        record[key] = line[start:end].strip()

    records.append(record)
```

心智模型：没有分隔符时，列的位置本身就是 schema，只能靠固定的字符区间切片还原字段。

---

## 示例 10：多状态机解析配置文件（Cisco 风格 running-config）

输入文本：

```text
hostname cmts-01
!
interface Cable1/0/0
 cable upstream 0 frequency 35600000
 cable upstream 0 channel-width 3200000
!
interface Cable1/0/1
 cable upstream 0 frequency 40200000
!
```

Python：

```python
state = "GLOBAL"
current_if = None
interfaces = {}

for raw in lines:
    line = raw.rstrip()

    if line == "!":
        state = "GLOBAL"
        current_if = None
        continue

    if state == "GLOBAL" and line.startswith("interface "):
        current_if = line.split()[1]
        interfaces[current_if] = []
        state = "INTERFACE"
        continue

    if state == "INTERFACE" and line.startswith(" "):
        interfaces[current_if].append(line.strip())
```

心智模型：`state` 变量决定了同一行文本的含义（全局命令 vs 接口子命令），`!` 是状态复位的信号，而不是无意义的分隔符。

---

## 十个示例的共同主线

| 示例 | Pattern | 关键机制 |
| --- | --- | --- |
| 1 | 定长表格 | `split()` |
| 2 | 含空格字段表格 | `split(maxsplit=N)` |
| 3 | 父子上下文 | 循环外变量 `current_xxx` |
| 4 | Section 分节 | Context 切换 + 正则 |
| 5 | Key=Value 日志 | `re.findall` + `dict()` |
| 6 | BEGIN/END 块 | 边界符触发 flush |
| 7 | 空行分隔记录 | 空行触发 flush |
| 8 | 缩进树形结构 | 缩进量 + 栈 |
| 9 | 定宽字段 | 按位置切片 |
| 10 | 多状态配置 | `state` 变量 + 状态迁移 |

一句话归纳：

> 这 10 个例子表面上用了不同的技巧（`split`、正则、栈、切片、状态变量），但都遵循同一条主线：**先识别 Pattern，再决定要不要维护 Context，再用最少的状态跑一次 State Machine，最终把文本恢复成 Normalized 的 Python 对象——后续的过滤、统计、导出都只是在这份 Python 对象上做事，与原始文本再无关系。**

---

# 十一、Parser 六步法（Step1 ~ Step6）

把前面十节的心智模型收敛成一份写 Parser 时可以照着执行的步骤清单。动手写代码之前，按顺序回答这 6 个问题。

---

## Step1：不要先写代码

先回答：

```
真正的数据对象（Object）是什么？
```

不是"这是几行文本"，而是"这几行文本还原出来应该是什么结构"。

---

## Step2：找 Context

问自己：

```
哪些字段不会每一行都重复？
```

不会每行重复的字段，就是 Context。循环过程中必须维护：

```
current_xxx
```

会每行都变的字段，才是这一行自己的数据，不需要维护成状态。

---

## Step3：一行只做一件事

对于每一行，只做以下两件事之一：

```
① 更新 Context（如果这一行是 Context 行）

② 提取字段，生成 Record（如果这一行是数据行）
```

不要一边解析一边统计，也不要在同一行里既更新 Context 又做业务计算。

---

## Step4：Parser 与 Business 分离

Parser：

```
文本
  ↓
Python Object
```

Business：

```
Object
  ↓
Filter
  ↓
Aggregation
  ↓
Report
```

永远不要把这两层写在同一个循环里。

---

## Step5：统一 Record

最终所有数据都变成同样字段的 Record（比如统一的 dict schema）。

统一之后：

```
filter
sort
groupby
csv
json
excel
```

全部可以直接复用同一份 records，不用为每种输出单独适配字段。

---

## Step6：把 Parser 看成状态机（State Machine）

只维护少量状态。

```
遇到新的 Context：

    更新状态

否则：

    继承状态
```

---

## 结合新示例：解析 Cable Modem 的 Service Flow（QoS）

输入文本：

```text
Cable Modem: 0018.6811.1234

  SFID 101  Direction US  MaxRate 10000000
  SFID 102  Direction DS  MaxRate 50000000

Cable Modem: 0018.6811.5678

  SFID 201  Direction US  MaxRate 5000000
```

### Step1：数据对象是什么？

不是"一行行的 SFID"，而是：

```
CableModem
 └── ServiceFlow
      ├── sfid
      ├── direction
      └── max_rate
```

### Step2：找 Context

`Cable Modem: xxx` 这一行不是每行都有，只在换 CM 时出现一次 —— 它是 Context，必须维护 `current_cm`。

`SFID` / `Direction` / `MaxRate` 每一行都不同 —— 它们是数据行自己的字段，不是 Context。

### Step3：一行只做一件事

```python
import re

current_cm = None
records = []

for raw in lines:
    line = raw.strip()

    m = re.match(r"Cable Modem: (\S+)", line)
    if m:
        current_cm = m.group(1)
        continue

    m = re.match(r"SFID (\d+)\s+Direction (\S+)\s+MaxRate (\d+)", line)
    if m and current_cm:
        sfid, direction, max_rate = m.groups()

        records.append({
            "cm": current_cm,
            "sfid": sfid,
            "direction": direction,
            "max_rate": int(max_rate),
        })
```

每一次循环，要么走第一个 `if` 更新 `current_cm`，要么走第二个 `if` 生成一条 Record，两者不会同时发生。

### Step4：Parser 与 Business 分离

Parser 只负责把文本变成 records，不做任何统计或打印：

```python
def parse_qos(lines):
    current_cm = None
    records = []

    for raw in lines:
        line = raw.strip()

        m = re.match(r"Cable Modem: (\S+)", line)
        if m:
            current_cm = m.group(1)
            continue

        m = re.match(r"SFID (\d+)\s+Direction (\S+)\s+MaxRate (\d+)", line)
        if m and current_cm:
            sfid, direction, max_rate = m.groups()

            records.append({
                "cm": current_cm,
                "sfid": sfid,
                "direction": direction,
                "max_rate": int(max_rate),
            })

    return records
```

Business 逻辑放在完全独立的函数里，只依赖 `records`，不依赖原始文本：

```python
def upstream_total_rate(records):
    return sum(
        r["max_rate"]
        for r in records
        if r["direction"] == "US"
    )
```

### Step5：统一 Record

每条 Record 都是同样的 4 个字段：`cm` / `sfid` / `direction` / `max_rate`。因此导出可以直接复用，不用为 CSV 再单独适配一次字段：

```python
import csv

records = parse_qos(lines)

with open("qos.csv", "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=["cm", "sfid", "direction", "max_rate"])
    writer.writeheader()
    writer.writerows(records)
```

### Step6：把 Parser 看成状态机

这里只有一个状态：`current_cm`。

```
遇到 "Cable Modem:" 行 → 更新状态（current_cm）

遇到 "SFID" 行 → 继承当前状态，生成 Record
```

心智模型：

> Parser 从头到尾只做一件事——**一边走一边问"现在这一行，是在告诉我新的 Context，还是在告诉我一条属于当前 Context 的数据？"**，六个 Step 只是把这句话拆成了可以照抄的检查清单。