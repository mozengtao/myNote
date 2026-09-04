# FUSE（Filesystem in Userspace）详解

[FUSE介绍](https://linux.laoqinren.net/linux/fuse/)
[]()
[]()

## 一、一句话理解

> **FUSE 是 Linux 提供的一套机制，让普通用户程序也能实现一个文件系统，而不需要编写内核驱动。**

也就是说：

以前：

```
实现文件系统
        │
        ▼
必须写 Linux Kernel Driver
```

现在：

```
实现文件系统
        │
        ▼
写普通用户程序
        │
        ▼
FUSE 帮你与内核交互
```

---

# 二、FUSE 出现之前的问题

Linux 中，一切文件操作都会经过内核：

```
vim hello.txt

        │
        ▼
   open()
   read()
   write()
        │
        ▼
Linux Kernel
        │
        ▼
Ext4 Driver
        │
        ▼
Disk
```

假设你想支持一个新的文件系统，例如：

```
DropboxFS
GitFS
S3FS
SSHFS
```

在没有 FUSE 时，你只能：

```
自己写 Kernel Module
```

这意味着：

- 学习 VFS（Virtual File System）
- 学习 inode
- 学习 dentry
- 学习 page cache
- 学习内核同步机制
- 内核崩溃可能导致整个系统崩溃
- 调试困难（gdb 基本用不了）

开发成本非常高。

---

# 三、FUSE 的解决方案

FUSE 引入了一层"翻译器"：

```
                用户程序
                     │
                     ▼
               open("a.txt")
                     │
                     ▼
                  Linux VFS
                     │
                     ▼
                FUSE Kernel
                     │
          /dev/fuse 通信接口
                     │
                     ▼
        用户态 FUSE Server（你写的程序）
                     │
                     ▼
      可以访问任何数据来源
```

核心思想：

> **把文件系统逻辑放到用户空间，把内核只保留最小的数据转发能力。**

---

# 四、FUSE 的工作流程

例如：

```
cat hello.txt
```

发生的过程：

```
cat
 │
 │ open()
 ▼
Linux Kernel
 │
 ▼
FUSE Driver
 │
 ▼
/dev/fuse
 │
 ▼
SSHFS
 │
 ▼
SSH
 │
 ▼
Remote Server
 │
 ▼
读取文件
 │
 ▲
一路返回数据
```

整个流程可以理解为：

```
应用程序
     │
     ▼
Linux 内核
     │
     ▼
FUSE
     │
     ▼
你的程序
```

因此：

> **FUSE 本身不是文件系统，而是一个"文件系统框架"。**

---

# 五、为什么 SSHFS 能工作？

SSHFS 本质上只是一个 FUSE 程序。

例如：

```
sshfs dev5:/mnt/DATA ~/remote
```

实际上启动了一个后台进程：

```
sshfs
```

它一直运行：

```
            Linux
              │
              ▼
      read("a.c")
              │
              ▼
            FUSE
              │
              ▼
           sshfs
              │
              ▼
            ssh
              │
              ▼
        dev5:/mnt/DATA
```

所以：

```
vim main.c
```

实际上变成：

```
vim

↓

FUSE

↓

sshfs

↓

ssh

↓

Remote write()
```

---

# 六、FUSE 可以连接任何东西

很多人以为：

```
FUSE == SSHFS
```

其实：

```
SSHFS
      │
      ▼
    FUSE
```

只是其中一种。

任何东西都可以伪装成文件系统。

例如：

```
            FUSE
              │
 ┌────────────┼────────────┐
 │            │            │
 ▼            ▼            ▼
SSHFS       S3FS        EncFS
SSH         Amazon S3   加密目录
```

还有：

```
FTP
HTTP
Git
Zip
数据库
对象存储
云盘
```

都可以做成文件系统。

例如：

```
ls
```

实际上：

```
HTTP GET
```

例如：

```
cat object.txt
```

实际上：

```
S3 GET Object
```

例如：

```
mkdir
```

实际上：

```
Git Commit
```

所以：

> **FUSE 的核心价值就是：把"任何数据源"伪装成普通目录。**

---

# 七、典型应用

### SSHFS

```
本地目录
      │
      ▼
Remote Linux
```

---

### S3FS

```
本地目录
      │
      ▼
Amazon S3
```

---

### EncFS

```
本地目录
      │
      ▼
自动加密
      │
      ▼
磁盘
```

---

### gocryptfs

```
明文目录
      │
      ▼
FUSE
      │
      ▼
加密目录
```

---

### rclone mount

```
Google Drive
OneDrive
Dropbox
阿里云OSS
Azure Blob
```

都可以：

```
mount
```

以后：

```
cp
ls
vim
```

全部正常使用。

---

# 八、FUSE 与内核文件系统对比

| 对比项 | 内核文件系统 | FUSE 文件系统 |
|---------|-------------|---------------|
| 运行位置 | Kernel Space | User Space |
| 开发难度 | 很高 | 较低 |
| 调试 | 困难 | 可用 gdb、strace |
| 崩溃影响 | 可能导致系统崩溃 | 只影响对应进程 |
| 性能 | 更高 | 略低（有用户态/内核态切换开销） |
| 适用场景 | ext4、xfs、btrfs 等本地文件系统 | SSHFS、云存储、加密、网络文件系统等 |

---

# 九、心智模型总结

把 Linux 文件系统想象成一个"翻译器"：

```
           用户程序
        vim / cp / rm / ls
                │
                ▼
             Linux VFS
                │
     ┌──────────┴──────────┐
     ▼                     ▼
 ext4/xfs                FUSE
 (内核实现)                │
                          ▼
                   用户态文件系统程序
              SSHFS / S3FS / gocryptfs ...
                           │
                           ▼
            SSH、S3、云盘、数据库等数据源
```

牢记三个层次：

1. **VFS（Virtual File System）**：Linux 统一的文件系统接口，应用程序只与 VFS 交互。
2. **FUSE**：连接 VFS 与用户态文件系统程序的桥梁，把文件操作请求转发到用户空间。
3. **具体文件系统程序**：如 SSHFS、S3FS、gocryptfs，它们负责将文件操作翻译成 SSH、HTTP、加密等具体协议。

**一句话总结：**

> **FUSE 并不是一种文件系统，而是一种让"普通用户程序也能实现文件系统"的框架；SSHFS 只是基于 FUSE 的一个具体实现，它把远程 SSH 目录映射成本地文件系统。**