# Python 文件读取（File Reading）完整总结：方法、应用场景、最佳实践与心智模式

> **一句话总结**

Python 中读取文件，本质上就是：

> **把外部的数据流（bytes / text）逐步转换为 Python 对象。**

整个流程可以抽象成：

```
Disk
 │
 ▼
File Object
 │
 ▼
read() / readline() / iteration
 │
 ▼
str / bytes
 │
 ▼
split / regex / json / yaml / csv ...
 │
 ▼
Python Object
```

所以真正需要思考的是：

> **我要最终得到什么对象？**

而不是：

> **我要调用哪个 read()。**

---

# 一、Python 文件读取方式总览

| 方法 | 返回值 | 是否一次读取 | 是否节省内存 | 最适合 |
|--------|---------|-------------|--------------|---------|
| read() | str / bytes | ✔ | ✘ | 小文件 |
| readline() | str | 一行 | ✔ | CLI交互 |
| readlines() | list[str] | ✔ | ✘ | 小文件逐行处理 |
| for line in file | str | 一行 | ✔✔✔ | 最大多数场景（推荐） |
| iter(file) | iterator | 一行 | ✔✔✔ | 本质就是for |
| iter(callable, sentinel) | iterator | 自定义 | ✔ | block读取 |
| file.read(size) | str / bytes | 固定大小 | ✔ | 大文件 |
| pathlib.Path.read_text() | str | ✔ | ✘ | 配置文件 |
| pathlib.Path.read_bytes() | bytes | ✔ | ✘ | 二进制 |
| mmap | memory view | 按需 | ✔✔ | 超大文件 |
| fileinput | iterator | 多文件 | ✔ | 多文件统一处理 |

---

# 二、最重要的心智模型

Python 文件对象实际上就是一个**Iterator**。

```
open()

↓

File Object

↓

__iter__()

↓

Iterator

↓

next()

↓

line
```

因此：

```python
with open("test.txt") as f:
    for line in f:
        print(line)
```

其实等价于

```python
it = iter(f)

while True:
    try:
        line = next(it)
    except StopIteration:
        break
```

所以：

> **文件就是一个按行产生字符串的 Generator。**

这是理解 Python 文件读取最重要的思想。

---

# 三、read()

## 一次全部读取

```python
with open("config.txt") as f:
    text = f.read()

print(text)
```

得到

```
str
```

例如

```
hello
world
python
```

整个字符串：

```
"hello\nworld\npython\n"
```

---

## 适合

- 配置文件
- JSON
- YAML
- SQL
- Markdown
- 小文本

例如

```
config.yaml
```

直接：

```python
yaml.safe_load(f.read())
```

---

## 心智模式

```
Disk

↓

Entire String

↓

Parser

↓

Object
```

---

# 四、read(size)

按固定长度读取。

例如：

```python
with open("big.log") as f:
    while True:
        chunk = f.read(1024)

        if not chunk:
            break

        process(chunk)
```

每次：

```
1024 chars
```

---

适合：

- 超大文本
- stream
- socket类似处理

---

心智模型：

```
File

↓

Chunk

↓

Chunk

↓

Chunk
```

而不是：

```
Entire File
```

---

# 五、readline()

一次读取一行。

```python
with open("data.txt") as f:
    line = f.readline()
```

继续：

```python
line2 = f.readline()
```

直到：

```
''
```

表示EOF。

---

适合：

CLI

例如：

```
Header

Data

Data

Data
```

先读Header：

```python
header = f.readline()

for line in f:
    ...
```

---

# 六、readlines()

返回：

```python
list[str]
```

例如：

```
a
b
c
```

得到：

```python
[
'a\n',
'b\n',
'c\n'
]
```

---

适合：

小文件。

例如：

```python
lines = f.readlines()

for line in lines:
    ...
```

---

缺点：

整个文件进入内存。

---

# 七、推荐：for line in file

这是Python官方推荐方式。

```python
with open("log.txt") as f:

    for line in f:
        print(line)
```

优点：

✔ 内存占用最低

✔ Lazy

✔ 自动缓冲

✔ 最Pythonic

---

例如日志：

```
10GB
```

也可以：

```
Line

↓

Process

↓

Discard

↓

Next Line
```

而不是：

```
10GB

↓

Memory
```

---

心智模式：

```
Iterator

↓

One Line

↓

Process

↓

Forget
```

---

# 八、enumerate()

读取行号。

```python
with open("log") as f:

    for lineno, line in enumerate(f, 1):

        print(lineno, line)
```

得到：

```
1 hello

2 world

3 python
```

非常适合：

日志分析。

---

# 九、Path.read_text()

```python
from pathlib import Path

text = Path("config.txt").read_text()
```

等价：

```python
with open(...) as f:
    text = f.read()
```

更现代。

---

适合：

配置文件。

例如：

```python
config = Path("config.yaml").read_text()
```

---

# 十、Path.read_bytes()

```python
data = Path("image.png").read_bytes()
```

得到：

```
bytes
```

---

适合：

图片

压缩包

二进制协议

---

# 十一、二进制读取

```python
with open("image.jpg","rb") as f:

    data = f.read()
```

返回：

```
bytes
```

而不是：

```
str
```

---

适合：

网络协议

DPDK

pcap

图片

压缩包

---

# 十二、固定Block读取（二进制）

```python
BLOCK = 4096

with open("disk.img","rb") as f:

    while True:

        block = f.read(BLOCK)

        if not block:
            break

        process(block)
```

适合：

超大二进制。

---

# 十三、iter(callable, sentinel)

一个经典技巧。

```python
with open("disk.img","rb") as f:

    for block in iter(lambda: f.read(4096), b''):
        process(block)
```

几乎等价：

```python
while True:

    block = f.read(4096)

    if block == b'':
        break
```

但是更加Pythonic。

---

# 十四、mmap（超大文件）

```python
import mmap

with open("huge.log") as f:

    mm = mmap.mmap(
        f.fileno(),
        0,
        access=mmap.ACCESS_READ
    )

    print(mm[:100])
```

特点：

```
Disk

↓

Virtual Memory

↓

Need

↓

Load
```

不是：

```
Entire File
```

适合：

GB日志。

---

# 十五、fileinput（多个文件）

例如：

```
a.log

b.log

c.log
```

统一：

```python
import fileinput

for line in fileinput.input():

    print(line)
```

执行：

```
python script.py a.log b.log
```

看起来：

```
一个连续文件
```

非常方便。

---

# 十六、不同文本格式的读取模式

## 普通文本

```python
with open("names.txt") as f:
    for line in f:
        name = line.strip()
```

得到：

```
str
```

---

## Key Value

```
eth0 10.0.0.1
eth1 10.0.0.2
```

```python
with open("ip.txt") as f:

    for line in f:

        iface, ip = line.split()
```

得到：

```
tuple
```

---

## CSV

```python
import csv

with open("user.csv") as f:

    reader = csv.DictReader(f)

    for row in reader:

        print(row["name"])
```

得到：

```
dict
```

---

## JSON

```python
import json

with open("config.json") as f:

    cfg = json.load(f)
```

得到：

```
dict

list
```

---

## YAML

```python
import yaml

with open("cfg.yaml") as f:

    cfg = yaml.safe_load(f)
```

得到：

```
Python Object
```

---

# 十七、CLI / Log 最佳读取模式

日志：

```
INFO ...

WARN ...

ERROR ...
```

推荐：

```python
with open("server.log") as f:

    for line in f:

        line = line.rstrip()

        if "ERROR" in line:
            ...
```

不要：

```python
text = f.read()

text.splitlines()
```

除非文件很小。

---

# 十八、最佳实践

## ① 永远使用 with（首选）

```python
with open("data.txt") as f:
    ...
```

优点：

自动关闭文件，即使发生异常也安全。

---

## ② 明确指定编码

推荐：

```python
with open(
    "data.txt",
    encoding="utf-8"
) as f:
    ...
```

避免不同平台默认编码导致的问题。

---

## ③ 大文件优先逐行遍历

推荐：

```python
for line in f:
```

而不是：

```python
f.readlines()
```

---

## ④ 使用 strip()/rstrip() 去除换行

```python
line = line.rstrip("\n")
```

如果还需要去除首尾空白：

```python
line = line.strip()
```

根据数据格式选择合适的方法。

---

## ⑤ 根据最终对象选择解析方式

不要停留在字符串层面，应尽快转换为结构化对象：

```python
# CSV
row = next(csv.DictReader(f))

# JSON
obj = json.load(f)

# YAML
obj = yaml.safe_load(f)

# Key-Value
key, value = line.split()
```

遵循：

```
Text
    ↓
Parser
    ↓
Normalized Python Object
```

---

## ⑥ pathlib 优先于字符串路径

```python
from pathlib import Path

path = Path("config.yaml")

text = path.read_text(encoding="utf-8")
```

可读性和跨平台性更好。

---

# 十九、文件读取心智模式（黄金法则）

## 模式一：小文件

```
File

↓

read()

↓

String

↓

Parser

↓

Object
```

适用于配置文件、JSON、YAML、Markdown 等。

---

## 模式二：日志流（最推荐）

```
File

↓

Iterator

↓

One Line

↓

Process

↓

Discard
```

适用于日志、CLI 输出、大文本文件。

---

## 模式三：超大文件

```
File

↓

Chunk

↓

Process

↓

Next Chunk
```

适用于镜像文件、PCAP、二进制协议、大型文本流。

---

## 模式四：结构化解析

```
Disk
    ↓
File Object
    ↓
Raw Text / Bytes
    ↓
Specialized Parser
    ├── split()      → tuple
    ├── csv          → dict
    ├── json.load()  → dict/list
    ├── yaml         → dict/list
    ├── xml          → ElementTree
    └── regex        → Match Object
            ↓
Normalized Python Object
            ↓
Business Logic
```

> **最终黄金法则：**
>
> - **小文件**：`read()` 一次读完。
> - **大多数文本（日志、CLI、配置列表）**：`for line in file`，这是最推荐、最 Pythonic 的方式。
> - **超大文件或流式数据**：按 `chunk`（`read(size)` 或 `iter(lambda: f.read(size), b"")`）读取。
> - **结构化数据**：读取后立即交给专用解析器（`csv`、`json`、`yaml`、`xml` 等），尽早转换为 **Normalized Python Object**，避免在业务逻辑中长期处理原始字符串。