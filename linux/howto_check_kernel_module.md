# Linux Kernel Module 查看方式总结 —— 一套完整的心智模型

很多人会混淆 **"模块存在"**、**"模块已编译"**、**"模块已加载"**、**"设备已创建"**、**"驱动已工作"** 这几个概念。

实际上，可以把 Linux Kernel Module 看成下面几个阶段：

```text
                Kernel Config
                     │
                     ▼
          是否编译进Kernel？
        ┌────────────┴────────────┐
        │                         │
   Built-in(y)              Module(m)
        │                         │
        │                  是否存在ko文件？
        │                         │
        ▼                         ▼
 Kernel启动时加载            modprobe/insmod
        │                         │
        └────────────┬────────────┘
                     ▼
               Module Loaded?
                     │
                     ▼
             Driver 初始化(init)
                     │
        ┌────────────┼────────────┐
        ▼            ▼            ▼
   注册字符设备    注册网络设备    注册文件系统...
        │
        ▼
   创建设备节点(/dev)
        │
        ▼
     用户空间可使用
```

因此，不同命令其实是在回答不同的问题。

---

# 一、当前是否已经加载(Module Loaded?)

## lsmod

```bash
lsmod | grep fuse
```

### 本质

读取：

```
/proc/modules
```

然后格式化输出。

### 回答的问题

> 这个模块现在是否已经加载？

例如：

```
fuse 172032 3
```

说明

- 已加载
- 大小
- 被引用次数

### 优点

最常用。

### 局限

对于 **Built-in Driver** 永远看不到。

例如

```
CONFIG_FUSE_FS=y
```

那么：

```
lsmod
```

没有 fuse。

因为它不是 module。

---

## cat /proc/modules

```bash
cat /proc/modules | grep fuse
```

### 本质

Kernel 导出的真实数据。

```
lsmod
```

就是解析它。

### 与 lsmod 差异

几乎没有。

只是：

```
lsmod
```

更容易阅读。

---

# 二、Kernel 是否支持这个模块？

## zcat /proc/config.gz

```bash
zcat /proc/config.gz | grep CONFIG_FUSE
```

回答：

> 当前运行内核编译时是否启用了它？

可能得到：

```
CONFIG_FUSE_FS=m
```

表示：

编译成模块。

或者

```
CONFIG_FUSE_FS=y
```

表示：

Built-in。

或者

```
# CONFIG_FUSE_FS is not set
```

表示：

根本没编译。

### 注意

这里只说明：

**Kernel Capability**

不是：

> 是否已经加载。

---

# 三、模块文件是否存在？

```bash
find /lib/modules/$(uname -r)
```

或者：

```
modinfo fuse
```

或者：

```
ls /lib/modules/$(uname -r)
```

回答：

> 系统是否提供这个 ko？

例如：

```
kernel/fs/fuse/fuse.ko
```

说明：

可以 modprobe。

但是：

```
不存在 ≠ Kernel 不支持
```

因为：

可能 Built-in。

---

# 四、是否 Built-in？

```bash
cat /lib/modules/$(uname -r)/modules.builtin
```

例如：

```
cat /lib/modules/$(uname -r)/modules.builtin | grep fuse
```

回答：

> 这个驱动是不是编译进 kernel image？

例如：

```
kernel/fs/fuse/fuse.ko
```

虽然写的是：

```
fuse.ko
```

实际上：

没有这个文件。

这里只是：

Builtin 列表。

### 为什么叫 builtin？

modprobe 会查询：

```
modules.builtin
```

如果发现：

```
builtin
```

就不会去找 ko。

---

# 五、Driver 是否初始化成功？

## dmesg

```bash
dmesg | grep fuse
```

回答：

> Driver init() 是否执行？

例如：

```
fuse: init (API version 7.39)
```

说明：

初始化成功。

如果：

```
Unknown symbol
```

```
probe failed
```

```
registration failed
```

说明：

驱动加载失败。

所以：

```
lsmod 有
```

不一定：

```
Driver 正常工作。
```

还要看：

```
dmesg
```

---

# 六、字符设备是否已经注册？

很多 Driver：

```
misc_register()
```

例如：

```
fuse
```

查看：

```bash
cat /proc/misc
```

例如：

```
229 fuse
```

说明：

Kernel 已经注册：

```
minor=229
```

对应：

```
/dev/fuse
```

### 回答的问题

> Driver 是否成功注册 misc device？

不是：

> Module 是否加载。

---

# 七、设备节点是否已经存在？

例如：

```bash
ls -l /dev/fuse
```

输出：

```
crw-rw-rw- 1 root root 10,229
```

说明：

用户空间已经可以：

```
open("/dev/fuse")
```

如果：

```
No such file
```

可能：

- Driver 未注册
- udev 未创建设备
- mknod 未执行

注意：

```
Module Loaded
```

≠

```
/dev 一定存在
```

---

# 八、各种检查方式之间的关系

```text
                 Kernel Config
                        │
                        ▼
              zcat /proc/config.gz
                        │
        ┌───────────────┴──────────────┐
        ▼                              ▼
 CONFIG_xxx=y                     CONFIG_xxx=m
        │                              │
        │                       是否存在ko文件？
        │                              │
        │                       /lib/modules/
        │                              │
        └───────────────┬──────────────┘
                        ▼
              Module 是否已加载？
          lsmod /proc/modules
                        │
                        ▼
               Driver init() 是否成功？
                     dmesg
                        │
        ┌───────────────┴──────────────┐
        ▼                              ▼
 注册字符设备                    注册其它Subsystem
 /proc/misc
        │
        ▼
 创建 /dev 节点
 ls -l /dev/fuse
        │
        ▼
 用户程序可以 open()
```

---

# 九、最容易混淆的几个概念

| 现象 | 是否说明 Driver 正常？ | 说明 |
|-------|----------------------|------|
| `/proc/config.gz` 有配置 | ❌ | 仅表示编译时启用了功能 |
| `/lib/modules` 有 `.ko` | ❌ | 模块文件存在，但可能未加载 |
| `modules.builtin` 有记录 | ❌ | 驱动编译进 Kernel，不代表运行正常 |
| `lsmod` 有模块 | ❌ | 模块已加载，但初始化可能失败 |
| `dmesg` 出现 init 成功 | ✅ 基本说明 | Driver 已完成初始化 |
| `/proc/misc` 有设备 | ✅ 更进一步 | 字符设备已注册 |
| `/dev/fuse` 存在 | ✅ 接近可用 | 用户态可以访问设备节点 |
| 应用程序成功使用设备 | ✅ 最终状态 | 驱动真正对外提供服务 |

---

# 十、一句话记忆（心智模型）

把整个生命周期记成七个连续问题：

```text
Kernel 有没有？
        │
        ▼
编译了吗？                → /proc/config.gz
        │
        ▼
Built-in 还是 Module？    → modules.builtin
        │
        ▼
Module 是否加载？         → lsmod /proc/modules
        │
        ▼
Driver 是否初始化？       → dmesg
        │
        ▼
是否注册 Kernel 接口？    → /proc/misc、/proc/devices、/sys、/proc/filesystems 等
        │
        ▼
是否创建设备节点？        → /dev/*
        │
        ▼
用户程序是否真正可用？
```

## 核心心智模式

将这些命令按层次理解，而不是孤立记忆：

| 层次 | 关注点 | 典型命令 |
|------|--------|----------|
| **配置层（Capability）** | 内核是否支持该功能 | `zcat /proc/config.gz` |
| **构建层（Build Artifact）** | 是 Built-in 还是 Module、模块文件是否存在 | `modules.builtin`、`find /lib/modules`、`modinfo` |
| **加载层（Runtime Load）** | 模块是否已进入内核 | `lsmod`、`cat /proc/modules` |
| **初始化层（Driver Init）** | 驱动 `init()` 是否成功执行 | `dmesg` |
| **注册层（Kernel Interface）** | 是否向内核注册接口（misc、block、net、filesystem 等） | `cat /proc/misc`、`/proc/devices`、`/proc/filesystems`、`/sys/class` |
| **设备层（Device Node）** | 是否创建了用户空间访问入口 | `ls -l /dev/*` |
| **应用层（Application）** | 用户程序是否真正能够正常使用 | `mount`、`open()`、业务程序验证 |