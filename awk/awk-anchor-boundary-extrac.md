# AWK 核心心智模型：Anchor → Boundary → Extract

> **一句话总结：**
>
> **几乎所有复杂 AWK 脚本都可以抽象成三个步骤：**
>
> ```text
>         找锚点（Anchor）
>                │
>                ▼
>      找稳定的边界（Boundary）
>                │
>                ▼
>       提取目标数据（Extract）
> ```
>
> 其中：
>
> - **Anchor**：决定"从哪里开始看"
> - **Boundary**：决定"数据的范围在哪里"
> - **Extract**：决定"真正要拿什么"

整个过程类似于 GPS 定位：

```
先找到城市（Anchor）

↓

找到街道（Boundary）

↓

找到具体门牌号（Extract）
```

---

# 一、第一步：找锚点（Anchor）

## 心智模式

Anchor 的作用只有一个：

> **唯一、稳定地定位到目标区域。**

不要想着：

```
第18行
```

而应该想着：

```
看到 "Interface"

↓

我知道我要开始工作了
```

所以：

```
Anchor
=
事件(Event)
=
入口(Entry Point)
```

---

# Anchor 方法一：正则匹配（最常用）

例如：

```text
Interface eth0
```

```awk
/^Interface/
```

或者：

```awk
/Interface/
```

示例：

```awk
/^Interface/ {
    iface = $2
}
```

适用于：

- CLI 输出
- Log
- Config

★★★★★

---

# Anchor 方法二：字符串完全相等

例如：

```text
Status
```

```awk
$1 == "Status"
```

示例：

```awk
$1 == "Status" {
    print $2
}
```

适用于：

字段格式固定。

★★★★★

---

# Anchor 方法三：多个条件组合

例如：

```
Interface eth0
```

要求：

```
第一列是 Interface

第二列不是 lo
```

```awk
$1=="Interface" && $2!="lo"
```

示例：

```awk
$1=="Interface" && $2!="lo" {
    print $2
}
```

★★★★★

---

# Anchor 方法四：字段匹配

例如：

```
admin
```

```
root
```

```
daemon
```

取：

```
admin
```

```awk
$3=="admin"
```

---

# Anchor 方法五：正则表达式匹配字段

例如：

```
eth0

eth1

lo
```

```awk
$1 ~ /^eth/
```

★★★★★

---

# Anchor 方法六：范围 Pattern

```
BEGIN CONFIG

...

END CONFIG
```

```awk
/BEGIN CONFIG/,/END CONFIG/
```

其实：

Range Pattern 本身就是：

Anchor。

★★★★★

---

# Anchor 方法七：BEGIN / END

```
BEGIN

END
```

也是特殊 Anchor。

```
程序开始

程序结束
```

---

# Anchor 最佳实践

| 方法 | 推荐指数 | 场景 |
|-------|----------|------|
| `/regex/` | ★★★★★ | CLI、日志 |
| `$1=="xxx"` | ★★★★★ | 固定字段 |
| `$1~/regex/` | ★★★★★ | 字段过滤 |
| `&&` 多条件 | ★★★★★ | 精确定位 |
| Range Pattern | ★★★★★ | 配置块 |
| BEGIN/END | ★★★★★ | 初始化/统计 |

---

# 二、第二步：找稳定边界（Boundary）

## 心智模式

Anchor 只是：

```
知道到了哪里
```

Boundary 才决定：

```
数据到哪里结束

真正的数据在哪里
```

Boundary：

```
决定数据范围
```

例如：

```
Interface eth0

Status

MAC

Speed

Interface eth1
```

Boundary：

```
Interface

↓

直到下一次 Interface
```

---

# Boundary 方法一：getline（行偏移）

★★★★★

例如：

```
CPU

-----

core0
```

```awk
/^CPU/ {

    getline
    getline

    print
}
```

适合：

```
下一行

后两行
```

---

# Boundary 方法二：flag 状态机

★★★★★

```
Interfaces

...

Routes
```

```awk
/^Interfaces/ {

    flag=1

    next
}

/^Routes/ {

    flag=0
}

flag
```

这是：

最经典的方法。

---

# Boundary 方法三：Range Pattern

★★★★★

```
/start/,/end/
```

例如：

```awk
/BEGIN/,/END/
```

AWK 自动维护状态。

---

# Boundary 方法四：next

★★★★★

```
跳过后面的 Rule
```

例如：

```awk
/^#/ {

    next
}
```

适合：

```
过滤

提前结束
```

---

# Boundary 方法五：sub()/gsub() 删除边界

★★★★★

例如：

```
MAC : aa:bb
```

```awk
sub(/^.*:[ ]*/, "")
```

得到：

```
aa:bb
```

Boundary：

```
:
```

之前全部删除。

---

# Boundary 方法六：split()

例如：

```
name=value
```

```awk
split($0,a,"=")

print a[2]
```

Boundary：

```
=
```

---

# Boundary 方法七：FS

例如：

```
name:value
```

```awk
BEGIN{

FS=":"
}
```

Boundary：

```
:
```

直接变成字段。

---

# Boundary 方法八：RS

例如：

```
一段配置

空行

下一段配置
```

```
RS=""
```

Boundary：

```
空行
```

一段就是一个 Record。

---

# Boundary 最佳实践

| 方法 | 推荐指数 | 场景 |
|-------|----------|------|
| getline | ★★★★★ | 后续几行 |
| flag | ★★★★★ | 区域扫描 |
| `/A/,/B/` | ★★★★★ | 配置块 |
| next | ★★★★★ | 提前结束 |
| sub/gsub | ★★★★★ | 去掉前缀 |
| split | ★★★★☆ | KV |
| FS | ★★★★★ | 字段分隔 |
| RS | ★★★★★ | Record 分隔 |

---

# 三、第三步：提取目标数据（Extract）

## 心智模式

经过前两步：

```
已经知道：

位置

范围
```

最后一步：

```
真正拿数据。
```

Extract：

```
Data Projection
```

---

# Extract 方法一：字段

★★★★★

```
print $2

print $NF
```

最常见。

---

# Extract 方法二：match()

★★★★★

```
MAC : aa:bb
```

```awk
match($0,/([0-9a-f]{2}:){5}[0-9a-f]{2}/)

print substr($0,RSTART,RLENGTH)
```

非常适合：

日志

MAC

IP

Version

---

# Extract 方法三：substr()

★★★★★

```
substr($0,10)
```

取固定位置。

---

# Extract 方法四：split()

★★★★★

```
split($0,a,":")
```

然后：

```
a[1]

a[2]
```

---

# Extract 方法五：gensub()（gawk）

★★★★★

```
gensub(...)
```

直接返回替换结果。

不用修改原字符串。

---

# Extract 方法六：sub()+print

★★★★★

```
sub(/^.*=/,"")

print
```

---

# Extract 方法七：数组

★★★★★

```
iface=$2

mac=$4

print iface,mac
```

上下文关联。

---

# Extract 方法八：printf()

★★★★★

```
printf("%s -> %s\n",iface,mac)
```

结构化输出。

---

# Extract 方法九：NF、$NF

★★★★★

例如：

```
Route via 1.1.1.1
```

```
print $NF
```

最后一列。

---

# Extract 最佳实践

| 方法 | 推荐指数 | 场景 |
|------|----------|------|
| `$N` | ★★★★★ | 固定字段 |
| `$NF` | ★★★★★ | 最后一列 |
| match | ★★★★★ | 正则提取 |
| substr | ★★★★☆ | 固定偏移 |
| split | ★★★★★ | KV |
| gensub | ★★★★★ | gawk |
| sub | ★★★★★ | 删除前缀 |
| printf | ★★★★★ | 输出格式 |
| 数组 | ★★★★★ | 上下文保存 |

---

# 四、完整示例

输入：

```text
Interface eth0

    MAC Address : aa:bb:cc:dd:ee:ff

    Status      : UP

Interface eth1

    MAC Address : 11:22:33:44:55:66

    Status      : DOWN
```

AWK：

```awk
/^Interface/ {

    iface = $2
}

/MAC Address/ {

    sub(/^.*:[[:space:]]*/, "")

    mac = $0

    printf("%-8s -> %s\n", iface, mac)
}
```

处理流程：

```text
扫描数据流
      │
      ▼
┌─────────────────────┐
│ Anchor              │
│ /^Interface/        │
└─────────┬───────────┘
          │ 保存 iface
          ▼
┌─────────────────────┐
│ Boundary            │
│ sub(/^.*:/,"")      │
│ 去掉前缀             │
└─────────┬───────────┘
          │
          ▼
┌─────────────────────┐
│ Extract             │
│ mac=$0              │
│ printf(...)         │
└─────────────────────┘
```

输出：

```text
eth0     -> aa:bb:cc:dd:ee:ff
eth1     -> 11:22:33:44:55:66
```

---

# 五、最终心智模型（Architecture）

```text
                 输入数据流（Record Stream）
                           │
                           ▼
               ┌────────────────────┐
               │ ① Anchor           │
               │ 找到事件/入口        │
               │ /regex/、$1=="x"   │
               └─────────┬──────────┘
                         │
                         ▼
               ┌────────────────────┐
               │ ② Boundary         │
               │ 确定数据范围         │
               │ getline、flag、FS   │
               │ sub()、Range Pattern│
               └─────────┬──────────┘
                         │
                         ▼
               ┌────────────────────┐
               │ ③ Extract          │
               │ 提取目标数据         │
               │ $N、match()        │
               │ split()、substr()  │
               └─────────┬──────────┘
                         │
                         ▼
                    输出结果
```

> **黄金心智模式：**
>
> 不要把 AWK 看成一个"按列取值工具"，而要把它看成一个**流式数据解析器（Streaming Parser）**。它的工作流程始终是：**利用 Anchor 在数据流中定位目标区域，利用 Boundary 建立稳定的数据边界，最后利用 Extract 提取真正需要的数据**。这三个阶段相互独立又相互配合，使得复杂的 CLI 输出、日志文件和配置文件都能够以一种健壮、可维护的方式进行解析。