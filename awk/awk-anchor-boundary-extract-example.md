# 利用 AWK 解析 `ip a s`：Anchor → Boundary → Extract 心智模式

## 输入数据

```text
$ ip a s
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
    inet 10.255.255.254/32 brd 10.255.255.254 scope global lo
       valid_lft forever preferred_lft forever
    inet6 ::1/128 scope host
       valid_lft forever preferred_lft forever
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP group default qlen 1000
    link/ether 00:15:5d:9d:04:e9 brd ff:ff:ff:ff:ff:ff
    inet 172.19.14.5/20 brd 172.19.15.255 scope global eth0
       valid_lft forever preferred_lft forever
    inet6 fe80::215:5dff:fe9d:4e9/64 scope link
       valid_lft forever preferred_lft forever
3: docker0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc noqueue state DOWN group default
    link/ether 16:8d:fe:ec:54:6f brd ff:ff:ff:ff:ff:ff
    inet 172.17.0.1/16 brd 172.17.255.255 scope global docker0
       valid_lft forever preferred_lft forever
```

目标：

- 忽略 `lo`
- 输出每个接口
- 输出 IPv4
- 输出 IPv6（如果存在）

最终输出：

```text
eth0       IPv4=172.19.14.5      IPv6=fe80::215:5dff:fe9d:4e9
docker0    IPv4=172.17.0.1       IPv6=-
```

---

# 一、第一步：Anchor（寻找锚点）

## 为什么这里选择 Interface Header？

观察输入可以发现：

```text
1: lo:
2: eth0:
3: docker0:
```

每一个 Interface 都是以：

```
数字:
```

开始。

因此：

```
^[0-9]+:
```

就是最稳定的 Anchor。

ASCII Diagram：

```text
1: lo:
^^^^^^^^
 Anchor

↓

2: eth0:
^^^^^^^^^^
 Anchor

↓

3: docker0:
^^^^^^^^^^^^^
 Anchor
```

AWK：

```awk
/^[0-9]+:/ {

}
```

---

## 提取 Interface 名称

当前 Record：

```text
2: eth0: <BROADCAST,...>
```

字段：

| 字段 | 内容 |
|------|------|
| $1 | 2: |
| $2 | eth0: |
| $3 | <BROADCAST,...> |

因此：

```awk
iface = $2
sub(/:$/, "", iface)
```

得到：

```
eth0
```

---

## 为什么不用 getline？

很多初学者第一反应：

```
getline

getline
```

实际上：

完全不需要。

因为：

```
每个 Interface Header

天然就是一个新的 Anchor
```

---

# 二、第二步：Boundary（确定数据边界）

Anchor 只是告诉我们：

```
新的 Interface 开始了
```

真正的数据范围是：

```
当前 Header

↓

inet

↓

inet6

↓

......

↓

下一 Header
```

ASCII：

```text
Interface Header
^^^^^^^^^^^^^^^^
Anchor

↓

link

↓

inet

↓

inet6

↓

......

↓

Next Interface Header
^^^^^^^^^^^^^^^^^^^^^
Boundary
```

所以：

Boundary：

```
当前 Interface

↓

直到

下一 Interface
```

---

## 如何实现 Boundary？

并没有写：

```
getline

getline
```

而是：

```
一直扫描

↓

直到新的 Anchor 出现
```

当新的 Header 出现：

```awk
/^[0-9]+:/ {

    flush()

    ...

}
```

说明：

```
上一块结束了
```

于是：

```
flush()

↓

开始新的 Block
```

这是：

**Streaming Parser（流式解析器）**

不是：

```
随机访问
```

---

# 三、第三步：Extract（提取目标数据）

进入当前 Interface Block 后：

只需要关心：

```
inet

inet6
```

即可。

---

## 提取 IPv4

输入：

```text
inet 172.19.14.5/20 brd ...
```

字段：

| 字段 | 内容 |
|------|------|
| $1 | inet |
| $2 | 172.19.14.5/20 |

AWK：

```awk
$1=="inet" {

    ipv4=$2
}
```

---

## 提取 IPv6

输入：

```text
inet6 fe80::215:...
```

AWK：

```awk
$1=="inet6" {

    ipv6=$2
}
```

---

## 为什么这里只取 `$2`？

因为：

这里真正的 Anchor 是：

```
inet

↓

第二列
```

即：

```text
inet      172.19.14.5/20

^^^^

Anchor

↓

Field +1
```

这就是：

```
Anchor

↓

Relative Field Offset
```

---

# 四、输出（Flush）

为什么需要：

```
flush()
```

因为：

```
解析

↓

输出
```

最好解耦。

定义：

```awk
function flush()
{
    if (iface != "" && iface != "lo") {

        printf("%-10s IPv4=%-18s IPv6=%s\n",
               iface,
               (ipv4 ? ipv4 : "-"),
               (ipv6 ? ipv6 : "-"))
    }
}
```

什么时候调用？

```
新的 Header

↓

说明上一块结束

↓

flush()
```

最后：

```
END

↓

flush()
```

输出最后一个 Interface。

---

# 五、完整 AWK

```awk
#!/usr/bin/awk -f

##############################################################################
# Print current interface
##############################################################################
function flush()
{
    if (iface != "" && iface != "lo") {
        printf("%-10s IPv4=%-18s IPv6=%s\n",
               iface,
               (ipv4 ? ipv4 : "-"),
               (ipv6 ? ipv6 : "-"))
    }
}

##############################################################################
# Interface Header (Anchor)
##############################################################################
/^[0-9]+:/ {

    flush()

    iface = $2
    sub(/:$/, "", iface)

    ipv4 = ""
    ipv6 = ""

    next
}

##############################################################################
# IPv4
##############################################################################
$1 == "inet" {

    ipv4 = $2

    sub(/\/.*/, "", ipv4)

    next
}

##############################################################################
# IPv6
##############################################################################
$1 == "inet6" {

    ipv6 = $2

    sub(/\/.*/, "", ipv6)

    next
}

##############################################################################
# END
##############################################################################
END {

    flush()
}
```

运行：

```bash
ip a s | awk -f ipaddr.awk
```

---

# 六、脚本执行过程（Step by Step）

```text
读取：

1: lo:
        │
        ▼
Anchor
        │
        ▼
iface="lo"

↓

inet

↓

保存 IPv4

↓

inet6

↓

保存 IPv6

↓

读取：

2: eth0:
        │
        ▼
新的 Anchor

↓

flush(lo)

↓

忽略

↓

iface="eth0"

↓

inet

↓

保存 IPv4

↓

inet6

↓

保存 IPv6

↓

读取：

3: docker0:
        │
        ▼
新的 Anchor

↓

flush(eth0)

↓

输出 eth0

↓

继续解析 docker0

↓

END

↓

flush(docker0)
```

整个过程始终只有：

```
单向扫描

没有回退

没有随机访问
```

---

# 七、核心心智模型

```text
                   ip a s 输出

                          │
                          ▼
             ┌────────────────────────┐
             │ Anchor                 │
             │ /^[0-9]+:/             │
             │ 找到新的 Interface      │
             └──────────┬─────────────┘
                        │
                        ▼
             ┌────────────────────────┐
             │ Boundary               │
             │ 当前 Interface Block    │
             │      │                 │
             │      ▼                 │
             │ 下一 Interface Header  │
             └──────────┬─────────────┘
                        │
                        ▼
             ┌────────────────────────┐
             │ Extract                │
             │ inet  → IPv4           │
             │ inet6 → IPv6           │
             └──────────┬─────────────┘
                        │
                        ▼
                    flush() 输出
```

---

# 八、最佳实践（Golden Rules）

| 阶段 | 推荐做法 | 原因 |
|------|----------|------|
| Anchor | 使用稳定的 Header（如 `^[0-9]+:`） | 不依赖固定行号，格式稳定 |
| Boundary | 以"下一次 Anchor"作为结束标志 | 适合所有块状 CLI 输出，避免使用 `getline` |
| Extract | 仅提取当前 Block 内关心的数据（如 `inet`、`inet6`） | 逻辑清晰、职责单一 |
| 状态管理 | 使用变量保存当前 Block 的状态 | 易于扩展更多字段（MAC、MTU、State 等） |
| 输出 | 使用 `flush()` 统一输出 | 解析与输出解耦，便于维护和复用 |

> **最终心智模式：**  
> 把 `ip a s` 看成一个由多个 **Interface Block** 组成的数据流，而不是一组独立的行。AWK 的任务不是"按第几行、第几列取值"，而是**在流中不断寻找新的 Interface Header（Anchor），以两个 Header 之间形成一个 Block（Boundary），在 Block 内提取需要的信息（Extract），最后统一输出结果。这种"Anchor → Boundary → Extract"模式几乎适用于所有块状 CLI 输出和日志解析。**