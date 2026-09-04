# SSHFS 使用 FUSE 的完整操作流程（从 `vim main.c` 到远程文件写入）

## 一、整体心智模型

可以先记住一句话：

> **SSHFS 本身不是文件系统，它只是一个实现了 FUSE 接口的普通用户程序。**

Linux 真正认识的是：

```
   VFS
    ↓
FUSE Driver
```

而不是 SSHFS。

因此整个架构如下：

```
                用户空间(User Space)
+------------------------------------------------------------+
|                                                            |
|    vim        ls        cp        gcc                      |
|      │         │         │         │                       |
|      └─────────┴─────────┴─────────┘                       |
|                    │                                       |
|                系统调用(open/read/write)                    |
+--------------------┼---------------------------------------+
                     │
=====================│========================================
                     │
                 Linux Kernel
                     │
                Virtual File System(VFS)
                     │
              判断该目录属于 FUSE
                     │
              FUSE Kernel Driver
                     │
                /dev/fuse 字符设备
=====================│========================================
                     │
                用户空间(User Space)
                     │
               sshfs 后台进程
                     │
                  SSH Client
                     │
              加密 TCP 连接
                     │
                Remote sshd
                     │
             Linux VFS (远程)
                     │
                  ext4/xfs
                     │
                  磁盘文件
```

整个过程中：

**内核不知道 SSH 的存在。**

内核只知道：

> "有人实现了一个 FUSE 文件系统，我把请求交给它即可。"

---

# 二、第一步：执行 sshfs 挂载

例如：

```bash
mkdir ~/remote

sshfs dev5:/mnt/DATA/morrism/testbed ~/remote
```

SSHFS 会做三件事情。

---

## 第一步：启动 sshfs 进程

Linux 中实际上多了一个后台进程：

```
$ ps

sshfs
```

它会一直等待 FUSE 请求。

可以理解成：

```
sshfs
│
├── 已连接 dev5
│
└── 正监听 /dev/fuse
```

---

## 第二步：通知内核

SSHFS 调用：

```
mount()
```

告诉内核：

```
~/remote

不是 ext4

不是 xfs

而是：

FUSE 文件系统
```

于是：

```
mount

↓

Kernel

↓

建立：

~/remote

↓

FUSE
```

以后：

```
~/remote
```

对应：

```
sshfs
```

而不是磁盘。

---

## 第三步：建立 SSH 连接

SSHFS 内部执行：

```
SSH Login
```

例如：

```
admin@10.254.25.106
```

于是：

```
sshfs
    │
    ▼
SSH
    │
    ▼
Remote sshd
```

一直保持连接。

因此：

```
mount
```

完成以后：

```
sshfs
        │
        ▼
SSH 已连接
        │
        ▼
等待文件请求
```

---

# 三、第二步：执行 ls

例如：

```bash
ls ~/remote
```

发生什么？

---

## 用户程序

```
ls
```

调用：

```
readdir()
```

---

## Linux VFS

VFS 查询：

```
~/remote
```

属于：

```
FUSE
```

于是：

```
Kernel

↓

发送：

READDIR
```

---

## FUSE Driver

FUSE Driver 不知道远程目录。

它只是：

```
把请求发给：

sshfs
```

通过：

```
/dev/fuse
```

通信。

```
Kernel

↓

/dev/fuse

↓

sshfs
```

---

## sshfs

收到：

```
READDIR
```

于是：

```
发送 SSH 请求
```

例如：

```
SFTP READDIR
```

---

## Remote

远程：

```
ext4

↓

读取目录

↓

返回：

main.c

Makefile

README
```

---

## 返回

一路返回：

```
Remote

↓

SSH

↓

sshfs

↓

FUSE

↓

Kernel

↓

ls
```

最后：

```
main.c
Makefile
README
```

显示出来。

---

# 四、第三步：vim main.c

例如：

```
vim main.c
```

整个过程。

---

## vim

调用：

```
open()

↓

read()
```

---

## Kernel

发现：

```
FUSE
```

于是：

```
OPEN

↓

READ
```

发给：

```
sshfs
```

---

## sshfs

收到：

```
READ
```

于是：

```
SSH

↓

Remote

↓

read()
```

读取：

```
main.c
```

返回内容。

于是：

```
vim
```

得到：

```
int main()
{
}
```

---

# 五、保存文件

例如：

```
:w
```

vim 调用：

```
write()
```

发生：

```
vim

↓

Kernel

↓

FUSE

↓

sshfs

↓

SSH

↓

Remote write()

↓

磁盘
```

真正写的是：

```
dev5:/mnt/DATA/morrism/testbed/main.c
```

不是本地。

---

# 六、删除文件

例如：

```
rm hello.c
```

流程：

```
rm

↓

unlink()

↓

Kernel

↓

FUSE

↓

sshfs

↓

SSH

↓

Remote unlink()

↓

删除成功
```

---

# 七、复制文件

例如：

```
cp a.c b.c
```

实际上：

```
open(a)

↓

read()

↓

sshfs

↓

Remote read()
```

然后：

```
write(b)

↓

sshfs

↓

Remote write()
```

可以发现：

**整个复制过程都是远程完成的数据传输，而不是本地磁盘操作。**

---

# 八、为什么会比本地慢？

因为一次读取：

```
read()
```

变成：

```
read()

↓

Kernel

↓

FUSE

↓

Context Switch

↓

sshfs

↓

SSH

↓

TCP

↓

Remote

↓

Disk

↓

返回
```

相比：

```
Kernel

↓

Disk
```

多了：

- 用户态切换
- SSH 加密
- 网络传输
- 远程磁盘访问

所以：

> **SSHFS 的便利性来自于"像本地一样访问远程文件"，代价是额外的上下文切换和网络开销。**

---

# 九、为什么应用程序感觉不到远程？

因为：

```
vim
```

根本不知道：

```
SSH
```

它只知道：

```
open()

read()

write()
```

Linux VFS 屏蔽了所有细节。

所以：

```
vim
```

既可以编辑：

```
ext4
```

也可以编辑：

```
NFS

SSHFS

S3FS

gocryptfs
```

代码完全不用修改。

这就是 **VFS（Virtual File System）抽象层** 的价值。

---

# 十、完整时序图

下面用一个简单的 `vim main.c` 示例总结整个调用链：

```
用户
 │
 │ vim main.c
 ▼
vim
 │
 │ open()
 ▼
Linux VFS
 │
 │ 发现该目录属于 FUSE
 ▼
FUSE Kernel Driver
 │
 │ 通过 /dev/fuse 转发请求
 ▼
sshfs 用户态进程
 │
 │ 转换为 SSH/SFTP 请求
 ▼
SSH Client
 │
 │ TCP
 ▼
Remote sshd
 │
 ▼
Remote Linux VFS
 │
 ▼
ext4/xfs
 │
 ▼
磁盘文件 main.c
 │
 ▲
 │ 返回文件内容
 └───────────────────────────────────────────────
```

---

# 十一、最终心智模型

牢记下面这张图即可：

```
            应用程序
      vim、gcc、cp、rm、git
                 │
                 ▼
          POSIX 文件接口
     open/read/write/readdir
                 │
                 ▼
          Linux VFS（统一接口）
                 │
        ┌────────┴────────┐
        ▼                 ▼
   ext4/xfs           FUSE Driver
 (内核文件系统)            │
                           ▼
                    sshfs（用户态）
                           │
                           ▼
                      SSH/SFTP 协议
                           │
                           ▼
                     远程 Linux 文件系统
```

**一句话总结：**

> **SSHFS 利用 FUSE 将 Linux VFS 发出的文件操作（open、read、write 等）转发给用户态的 sshfs 进程；sshfs 再将这些操作翻译为 SSH/SFTP 请求发送到远程服务器，最终让远程目录在本地表现得像一个普通目录。**