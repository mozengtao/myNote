# SSHFS 使用指南（从心智模型到实践）

## 一、SSHFS 的心智模型

先记住一句话：

> **SSHFS = SSH + FUSE = 把远程目录映射成本地目录。**

它并不会复制文件。

它只是：

```
                ssh
+---------+ <-----------> +----------------+
| 本地目录 |               | 远程目录       |
| ~/dev5   |               | /mnt/DATA/... |
+---------+               +----------------+
      ↑
      │
  所有读写都经过 SSH
```

因此：

- 本地 `vim` 修改
- 本地 `cp`
- 本地 `rm`
- IDE 打开

实际上都是：

```
本地程序
      │
      ▼
SSHFS(FUSE)
      │
      ▼
SSH
      │
      ▼
远程文件系统
```

所以：

```
vim test.c
```

实际上修改的是：

```
dev5:/mnt/DATA/morrism/testbed/test.c
```

而不是本地副本。

---

# 二、什么时候使用 SSHFS

适合：

✅ 修改代码

```
vim
VSCode
CLion
QtCreator
```

✅ 浏览日志

```
less
tail
grep
```

✅ 上传下载

```
cp
mv
rsync
```

✅ 调试

例如：

```
GDB
Python
Shell Script
```

都可以直接操作远程文件。

---

不适合：

❌ 大量随机IO

例如：

```
数据库
SQLite
VM镜像
Docker OverlayFS
```

因为所有IO都经过SSH。

---

# 三、安装

Ubuntu

```bash
sudo apt install sshfs
```

CentOS

```bash
sudo yum install sshfs
```

Fedora

```bash
sudo dnf install sshfs
```

---

# 四、最常用命令

## 1）挂载目录

```
sshfs user@host:/remote/path local_dir
```

例如：

```bash
mkdir -p ~/remote/dev5

sshfs \
admin@10.254.25.106:/mnt/DATA/morrism/testbed \
~/remote/dev5
```

以后：

```
~/remote/dev5
```

就是远程目录。

---

## 2）利用 ~/.ssh/config

假设：

```
Host dev5
    HostName 10.254.25.106
    User admin
```

那么：

```bash
sshfs dev5:/mnt/DATA/morrism/testbed \
~/remote/dev5
```

推荐这样写。

优点：

- 不需要IP
- 自动使用密钥
- 自动使用ProxyJump
- 自动使用端口

---

## 3）卸载

Linux：

```bash
fusermount -u ~/remote/dev5
```

或者：

```bash
umount ~/remote/dev5
```

---

## 4）查看是否已经挂载

```bash
mount | grep sshfs
```

或者

```bash
df -h
```

例如：

```
admin@dev5:/mnt/DATA
```

说明已经挂载。

---

## 5）重新连接

SSH断开以后：

```
Transport endpoint is not connected
```

解决：

```bash
fusermount -u ~/remote/dev5

sshfs dev5:/mnt/DATA/morrism/testbed \
~/remote/dev5
```

---

# 五、最常见参数

## reconnect

自动重连

```bash
sshfs \
-o reconnect \
dev5:/mnt/DATA \
~/remote/dev5
```

推荐一直加。

---

## ServerAliveInterval

保持SSH连接

```bash
-o ServerAliveInterval=15
```

每15秒发一个keepalive。

---

## ServerAliveCountMax

允许连续失败次数

```bash
-o ServerAliveCountMax=3
```

---

## cache=yes

开启缓存

```bash
-o cache=yes
```

读取大量文件速度更快。

---

## follow_symlinks

跟随软链接

```bash
-o follow_symlinks
```

---

## allow_other

允许其它用户访问

```bash
-o allow_other
```

需要：

```
/etc/fuse.conf
```

开启：

```
user_allow_other
```

---

## IdentityFile

指定密钥

```bash
-o IdentityFile=~/.ssh/id_ed25519
```

---

## port

指定端口

```bash
-p 2222
```

或者：

```
-o port=2222
```

---

# 六、推荐挂载命令

日常开发：

```bash
sshfs \
-o reconnect \
-o ServerAliveInterval=15 \
-o ServerAliveCountMax=3 \
dev5:/mnt/DATA/morrism/testbed \
~/remote/dev5
```

这是开发中最常见的配置。

---

# 七、典型应用场景

## 场景1：Linux开发

远程：

```
/mnt/DATA/project
```

本地：

```
~/remote/project
```

然后：

```
vim
make
grep
git
```

全部直接操作。

---

## 场景2：VSCode

```
VSCode
      │
Open Folder
      │
~/remote/dev5
```

不用Remote SSH。

---

## 场景3：同步文件

例如：

```
cp test.c ~/remote/dev5/
```

就是：

```
scp test.c dev5:/mnt/DATA/...
```

但是体验更自然。

---

## 场景4：查看日志

例如：

```
tail -f ~/remote/dev5/log.txt
```

就是：

```
tail -f dev5:/mnt/DATA/log.txt
```

---

## 场景5：图片查看

例如：

```
eog ~/remote/dev5/result.png
```

图片直接打开。

---

# 八、SSHFS 与其它方案对比

| 方案 | 是否挂载 | 是否复制 | 适合 |
|------|----------|----------|------|
| ssh | × | × | 登录服务器 |
| scp | × | √ | 单次传文件 |
| rsync | × | √ | 同步目录 |
| sshfs | √ | × | 日常开发 |
| NFS | √ | × | 局域网高性能共享 |
| Samba | √ | × | Windows/Linux共享 |

一句话记忆：

```
ssh     -> 登录
scp     -> 拷贝一次
rsync   -> 增量同步
sshfs   -> 当成本地目录
NFS     -> 高性能共享
```

---

# 九、SSHFS 的心智模型总结

```
             本地程序
      vim  gcc  git  cp  rm
                │
                ▼
           Linux VFS
                │
                ▼
             FUSE层
                │
                ▼
             SSHFS驱动
                │
          SSH 加密通道
                │
                ▼
      dev5:/mnt/DATA/morrism/testbed
```

关键理解：

- **SSHFS 不是文件同步工具，而是文件系统映射工具。**
- **本地目录只是远程目录的"窗口"，所有读写实时发生在远程。**
- **开发时优先使用 SSHFS；需要长期高性能共享时再考虑 NFS；需要复制或备份时使用 rsync。**