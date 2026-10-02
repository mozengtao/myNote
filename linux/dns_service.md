# BIND DNS Zone 管理命令与 Mental Model

`named` 是 **BIND（Berkeley Internet Name Domain）** 的主程序，是 Linux 上最主流的 DNS 服务器软件，监听 53 端口（UDP 为主，TCP 用于大报文 / 区域传输），用来实现域名 ↔ IP 的解析

## 1. 整体 Mental Model

这一组命令完整体现了：

> **修改 BIND DNS zone → 检查 zone file → 修复 Linux/SELinux 权限 → reload zone → 清理缓存 → DNS 查询验证**

整体架构：

```text
                         BIND DNS Server
                              │
              ┌───────────────┴───────────────┐
              │                               │
       Configuration                    Runtime State
       named.conf                       loaded zones
       named.rfc1912.zones              DNS cache
              │                               │
              ▼                               ▼
       zone file                       rndc reload
       /var/named/...                  rndc flushname
              │
       ┌──────┼─────────┐
       │      │         │
     Unix   SELinux   DNS syntax
 permission context    check
       │      │         │
       ▼      ▼         ▼
    chown   restorecon named-checkzone
                              │
                              ▼
                             dig
                              │
                              ▼
                     DNS query verification
```

---

# 2. `ls -lZ`

```bash
ls -lZ /var/named/dukekunshan.edu.cn
```

作用：

> 查看 zone file 的 **Linux 文件权限、owner/group 和 SELinux context**。

---

## `-l`

例如：

```text
-rw-r-----. 1 root named 856 Sep 29 13:00 dukekunshan.edu.cn
```

表示：

```text
owner = root
group = named

root  → read + write
named → read
other → nothing
```

对应：

```text
640
│││
││└── other = 0
│└─── group = 4 (read)
└──── owner = 6 (read + write)
```

---

## `-Z`

`-Z` 查看 SELinux security context。

例如：

```text
system_u:object_r:named_zone_t:s0
```

其中：

```text
system_u:object_r:named_zone_t:s0
                       │
                       └── SELinux type
```

对于 BIND zone file，常见的重要类型是：

```text
named_zone_t
```

所以：

```text
Linux Permission
       +
SELinux Context
       ↓
named 是否能够访问 zone file
```

---

# 3. `named-checkzone`

```bash
named-checkzone dukekunshan.edu.cn \
    /var/named/dukekunshan.edu.cn
```

作用：

> 检查 DNS zone file 的语法和语义是否正确。

它：

* 读取 zone file
* 解析 DNS records
* 检查 zone structure
* 检查 SOA
* 检查 NS
* 检查 record 格式
* 检查 TTL
* 检查 DNS name
* 检查 serial 等

它**不会修改运行中的 named**。

可以把它理解成：

```text
named-checkzone
       ≈
DNS zone file 的 syntax checker
```

类似：

```bash
gcc -fsyntax-only foo.c
```

---

## 成功示例

```text
zone dukekunshan.edu.cn/IN: loaded serial 2026092901
OK
```

---

## 重要区别

```text
named-checkzone
        │
        ▼
只检查磁盘上的 zone file
```

它不会自动让正在运行的 BIND 使用新的内容。

---

# 4. `chown`

```bash
chown root:named /var/named/dukekunshan.edu.cn
```

作用：

> 修改文件的 owner 和 group。

最终：

```text
owner = root
group = named
```

即：

```text
root:named
```

为什么通常使用 `named` group？

因为 BIND 的 `named` daemon 通常以：

```text
named
```

用户运行。

于是形成：

```text
root
 │
 │ 管理 zone file
 ▼
/var/named/dukekunshan.edu.cn
 │
 │ group read access
 ▼
named process
 │
 ▼
读取 zone file
```

这是 Linux 最小权限模型的一种典型应用。

---

# 5. `chmod 640`

```bash
chmod 640 /var/named/dukekunshan.edu.cn
```

作用：

> 设置 Unix 文件权限。

`640`：

```text
     owner group other

6       4     0
│       │     │
│       │     └── ---
│       └──────── r--
└──────────────── rw-
```

因此：

```text
root   → rw-
named  → r--
other  → ---
```

即：

```text
root:named
    +
  0640
    ↓
root 可以读写
named 可以读取
其他用户不能访问
```

---

# 6. `restorecon`

```bash
restorecon -v /var/named/dukekunshan.edu.cn
```

作用：

> 根据 SELinux policy，将文件恢复为正确的 SELinux security context。

例如错误状态：

```text
system_u:object_r:default_t:s0
```

期望：

```text
system_u:object_r:named_zone_t:s0
```

`restorecon` 会根据系统配置的默认 SELinux context 规则进行恢复。

---

## `chown`、`chmod`、`restorecon` 的区别

这三个命令分别解决三个不同层次的问题：

```text
                     zone file
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
           chown       chmod      restorecon
             │           │           │
             ▼           ▼           ▼
        owner/group   Unix mode   SELinux context
             │           │           │
             └───────────┼───────────┘
                         ▼
                 named 能否访问文件
```

可以简单记成：

```text
chown
→ 谁拥有这个文件？

chmod
→ Unix 层面谁能读/写？

restorecon
→ SELinux 是否允许这个进程访问？
```

---

# 7. `rndc reload`

```bash
rndc reload dukekunshan.edu.cn
```

`rndc`：

```text
Remote Name Daemon Control
```

它是 BIND 提供的运行时控制接口。

---

## 为什么需要 `reload`？

假设你修改：

```text
/var/named/dukekunshan.edu.cn
```

磁盘上的文件已经变化：

```text
zone file
    │
    ▼
new DNS records
```

但是正在运行的：

```text
named process
```

并不会因为你编辑了文件，就一定立即重新加载这个 zone。

所以：

```bash
rndc reload dukekunshan.edu.cn
```

相当于：

> 告诉正在运行的 named：重新读取这个 zone。

流程：

```text
修改 zone file
      │
      ▼
named-checkzone
      │
      ▼
zone file OK
      │
      ▼
rndc reload
      │
      ▼
named 重新读取 zone
      │
      ▼
新的 zone data 进入运行状态
```

---

# 8. `rndc flushname`

```bash
rndc flushname speedtest.dukekunshan.edu.cn
```

作用：

> 清除指定 DNS name 对应的缓存。

---

## 为什么需要清缓存？

假设之前：

```text
speedtest.dukekunshan.edu.cn
        ↓
172.25.2.100
```

BIND 把结果缓存下来。

后来 zone file 改成：

```text
speedtest.dukekunshan.edu.cn
        ↓
172.25.2.200
```

此时可能出现：

```text
Authoritative zone
        ↓
172.25.2.200

DNS cache
        ↓
172.25.2.100
```

于是缓存中的旧数据可能影响查询结果。

执行：

```bash
rndc flushname speedtest.dukekunshan.edu.cn
```

就是：

> 删除这个 DNS name 的缓存记录。

下一次查询时，需要重新获取数据。

---

# 9. `reload` 和 `flushname` 的区别

这是理解 BIND runtime 的关键。

```text
                         BIND
                          │
              ┌───────────┴───────────┐
              ▼                       ▼
      Authoritative Zone            Cache
              │                       │
              │                       │
         zone file               cached answer
              │                       │
              ▼                       ▼
        rndc reload             rndc flushname
```

因此：

```bash
rndc reload dukekunshan.edu.cn
```

解决的是：

> zone file 更新了，但是 named 还没有重新加载。

而：

```bash
rndc flushname speedtest.dukekunshan.edu.cn
```

解决的是：

> DNS cache 中可能还存在旧的 answer。

---

# 10. `dig`

```bash
dig @172.25.2.5 speedtest.dukekunshan.edu.cn A
```

作用：

> 向指定 DNS server 发起实际的 DNS 查询，用于最终验证。

拆解：

```text
dig
 │
 ├── @172.25.2.5
 │       │
 │       └── 指定 DNS Server
 │
 ├── speedtest.dukekunshan.edu.cn
 │       │
 │       └── 查询的 DNS name
 │
 └── A
         │
         └── 查询 IPv4 address record
```

相当于：

> 请向 `172.25.2.5` 这个 DNS server 查询 `speedtest.dukekunshan.edu.cn` 的 A record。

---

## 典型结果

```text
;; ANSWER SECTION:

speedtest.dukekunshan.edu.cn. 3600 IN A 172.25.2.100
```

表示：

```text
speedtest.dukekunshan.edu.cn
             │
             ▼
        172.25.2.100
```

DNS server 最终返回了这个 IPv4 地址。

---

# 11. 把所有命令串起来

完整的 DNS zone deployment / troubleshooting pipeline：

```text
                 修改 zone file
                       │
                       ▼
       /var/named/dukekunshan.edu.cn
                       │
                       ▼
              ┌────────────────┐
              │ File Access    │
              └────────────────┘
                       │
                ┌──────┴──────┐
                ▼             ▼
             chown           chmod
          root:named          640
                │             │
                └──────┬──────┘
                       ▼
                  restorecon
                       │
                       ▼
             SELinux context OK
                       │
                       ▼
             named-checkzone
                       │
                       ▼
                zone syntax OK
                       │
                       ▼
                rndc reload
                       │
                       ▼
              named reloads zone
                       │
                       ▼
            flush old DNS cache
                       │
                       ▼
                 rndc flushname
                       │
                       ▼
                     dig
                       │
                       ▼
             实际 DNS 查询结果
```

---

# 12. DNS 排障的 4 层 Mental Model

以后遇到 BIND DNS 问题，可以把问题分成四层。

## Layer 1：File

首先确认：

```bash
ls -lZ /var/named/dukekunshan.edu.cn
```

关注：

```text
文件是否存在？
owner/group 是否正确？
Unix permission 是否正确？
SELinux context 是否正确？
```

---

## Layer 2：Zone Syntax

然后：

```bash
named-checkzone dukekunshan.edu.cn \
    /var/named/dukekunshan.edu.cn
```

关注：

```text
zone file 能否被 BIND 正确解析？
SOA 是否正确？
NS 是否正确？
A/CNAME/MX 等 record 是否正确？
serial 是否正确？
```

---

## Layer 3：BIND Runtime

zone file 正确之后：

```bash
rndc reload dukekunshan.edu.cn
```

关注：

```text
运行中的 named 是否已经加载最新 zone？
```

如果怀疑缓存：

```bash
rndc flushname speedtest.dukekunshan.edu.cn
```

关注：

```text
旧的 DNS answer 是否仍然存在于 cache？
```

---

## Layer 4：Actual DNS Result

最后：

```bash
dig @172.25.2.5 speedtest.dukekunshan.edu.cn A
```

关注：

```text
DNS server 实际返回什么？
```

---

# 13. 最实用的排查顺序

遇到：

> `speedtest.dukekunshan.edu.cn` 查询结果不对

建议按照：

```text
① 文件存在吗？
        │
        ▼
② owner/group 正确吗？
        │
        ▼
③ chmod 正确吗？
        │
        ▼
④ SELinux context 正确吗？
        │
        ▼
⑤ named-checkzone 是否 OK？
        │
        ▼
⑥ named 是否 reload？
        │
        ▼
⑦ cache 是否存在旧数据？
        │
        ▼
⑧ dig 实际返回什么？
```

对应命令：

```bash
# 1. 文件 / 权限 / SELinux
ls -lZ /var/named/dukekunshan.edu.cn

# 2. Zone syntax
named-checkzone dukekunshan.edu.cn \
    /var/named/dukekunshan.edu.cn

# 3. 修复 owner/group
chown root:named /var/named/dukekunshan.edu.cn

# 4. 修复 Unix permission
chmod 640 /var/named/dukekunshan.edu.cn

# 5. 修复 SELinux context
restorecon -v /var/named/dukekunshan.edu.cn

# 6. 重新加载 zone
rndc reload dukekunshan.edu.cn

# 7. 清除指定 name 的 cache
rndc flushname speedtest.dukekunshan.edu.cn

# 8. 最终验证
dig @172.25.2.5 speedtest.dukekunshan.edu.cn A
```

---

# 14. 一句话记忆

```text
ls -lZ
    → 文件现在是什么状态？

named-checkzone
    → zone file 内容对不对？

chown
    → 谁拥有文件？

chmod
    → Unix 权限是什么？

restorecon
    → SELinux context 对不对？

rndc reload
    → named 是否重新加载 zone？

rndc flushname
    → 是否清掉旧 cache？

dig
    → DNS 实际返回什么？
```

最终可以把整个过程浓缩成：

```text
File
 ↓
Permission
 ↓
SELinux
 ↓
Zone Syntax
 ↓
BIND Runtime
 ↓
Cache
 ↓
DNS Query
```

这也是排查 BIND DNS 问题时最重要的 **Mental Model：不要把“zone file”“named 的运行状态”和“DNS cache”看成同一个东西，它们是三个不同的状态层。**
