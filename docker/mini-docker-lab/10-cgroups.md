# 第十章:cgroups

## 1. 学习目标

理解 cgroups(control groups)如何让**内核**强制限制一组进程能用多少 CPU、内存、能创建多少个进程,并且不需要修改这些进程自身的代码。这一章分三个子实验:CPU 限制、Memory 限制(触发 OOM)、PIDs 限制(挡住 fork 炸弹)。

## 2. 前置知识

- 前面九章建立的心智模型:Namespace 解决的是"看到什么"(视图),cgroups 解决的是完全不同的问题:"能用多少"(配额)
- 知道 `/sys/fs/cgroup` 是一个特殊的虚拟文件系统(cgroup v2 下是统一的单一层级,本课程只讲 v2)
- 会看 `top`/`ps`、会看 `dmesg`

## 3. 原理简介

前面九章解决的都是"隔离"——让一个进程看不到不该看到的东西。但"看不到"不等于"用不了太多"。如果没有资源限制:

```
一个"看起来隔离"的容器进程

可以:
- 无限占用 CPU,饿死宿主机上其他进程
- 无限吃内存,直到把整台机器的内存耗尽,触发全局 OOM,可能杀掉不相关的关键进程
- 无限 fork 子进程(fork 炸弹),耗尽整台机器的 PID 资源,导致任何新进程都创建失败
```

cgroups 解决的正是这个问题,思路和 Namespace 完全不同:

```
Namespace:
   给进程一个"假的、独立的视图"
   进程本身感觉不到被隔离

cgroups:
   内核在进程真正申请资源(CPU 时间片、内存页、新 PID)的那一刻
   直接在内核态检查这个进程属于哪个 cgroup
   如果超过这个 cgroup 的配额,直接拒绝/限流/杀死
   进程自己完全不需要知道这件事,也无法绕过
```

这也是为什么本章标题强调:**cgroups 不是"容器管好自己",而是"内核在背后管着这个进程"**。容器进程里的程序完全不需要写任何"资源感知"的代码。

cgroup v2 把所有资源控制器(controller)统一挂在一棵树下(`/sys/fs/cgroup`),每个子目录就是一个"分组",往这个分组的 `cgroup.procs` 文件里写入 PID,就相当于把这个进程"放进"了这个分组,该分组下所有的限制文件（`cpu.max`、`memory.max`、`pids.max` 等）立刻对它生效。

## 4. 实验步骤

### Step 0:在根 cgroup 上启用需要的 controller(只做一次)

执行:

```bash
cat /sys/fs/cgroup/cgroup.controllers
echo "+cpu +memory +pids" | sudo tee /sys/fs/cgroup/cgroup.subtree_control
cat /sys/fs/cgroup/cgroup.subtree_control
```

**为什么这么做**:cgroup v2 要求"父分组显式把某个 controller 下放给子分组",不能默认子分组随便用父分组还没启用的 controller。这一步只需要做一次,后面创建的子 cgroup 才能使用 `cpu.max`、`memory.max`、`pids.max` 这些文件。

---

### 子实验 A:CPU 限制

#### Step A1:创建 cgroup 并设置 CPU 配额

```bash
sudo mkdir /sys/fs/cgroup/mini-cpu
echo "50000 100000" | sudo tee /sys/fs/cgroup/mini-cpu/cpu.max
```

**为什么这么做**:`cpu.max` 的格式是 `<quota> <period>`,单位是微秒。`50000 100000` 表示"每 100000 微秒(100ms)的周期里,这个 cgroup 里所有进程加起来最多只能用 50000 微秒(50ms)的 CPU 时间"——也就是限制为**半个 CPU 核心**的算力,不管这台机器实际有多少核。

#### Step A2:找一个能吃满 CPU 的进程,扔进这个 cgroup

```bash
yes > /dev/null &
YES_PID=$!
echo $YES_PID | sudo tee /sys/fs/cgroup/mini-cpu/cgroup.procs
```

**为什么这么做**:`yes > /dev/null` 是一个经典的"死循环打印"命令,天然会用满一个 CPU 核心,很适合当限流的观测对象。把它的 PID 写进 `cgroup.procs`,就是把这个已经在运行的进程"移动"进了 `mini-cpu` 这个分组。

#### Step A3:观察限流效果

```bash
top -p $YES_PID
```

**观察**:正常不受限时 `yes` 通常会占满 100%(单核)CPU,加入 `mini-cpu` 之后,`top` 显示的 CPU 占用会稳定在大约 **50%** 左右。

**为什么会这样**:内核调度器每隔一个 period(这里是 100ms)就检查这个 cgroup 累计用了多少 CPU 时间,一旦用满配额(这里是 50ms),就会把这个 cgroup 里的进程**限流**(throttle)到下一个周期,不会真的杀掉进程,只是不让它继续占用 CPU,直到新的周期开始。

清理:

```bash
kill $YES_PID
sudo rmdir /sys/fs/cgroup/mini-cpu
```

---

### 子实验 B:Memory 限制与 OOM

#### Step B1:创建 cgroup 并设置内存上限

```bash
sudo mkdir /sys/fs/cgroup/mini-mem
echo "50M" | sudo tee /sys/fs/cgroup/mini-mem/memory.max
```

**为什么这么做**:`memory.max` 是这个 cgroup 里所有进程加起来能用的物理内存硬上限,超过就会触发该 cgroup 范围内的 OOM Killer(注意:是"这个 cgroup 范围内",不是全局 OOM,这正是 cgroups 相比"什么都不限制"的关键改进——爆内存的影响被关在了这个分组里)。

#### Step B2:写一个会不断吃内存的小程序

```bash
python3 -c "
chunks = []
while True:
    chunks.append('x' * 10 * 1024 * 1024)  # 每次多吃 10MB
    print('allocated:', len(chunks) * 10, 'MB')
" &
PY_PID=$!
echo $PY_PID | sudo tee /sys/fs/cgroup/mini-mem/cgroup.procs
```

**为什么这么做**:这个 Python 脚本会不停地往一个列表里追加 10MB 的字符串,持续增长内存占用,是一个方便观察"内存被限制"效果的负载。同样是先启动进程,再把它的 PID 写进 cgroup 完成"分组"。

#### Step B3:观察 OOM 发生

```bash
wait $PY_PID 2>&1
cat /sys/fs/cgroup/mini-mem/memory.events
sudo dmesg | tail -20
```

**观察**:Python 进程打印到大约 `40~50 MB` 左右就会被**强制杀死**(输出会突然中断,`wait` 会报告非正常退出);`memory.events` 文件里 `oom_kill` 这一项计数会变成 `1`(或更多);`dmesg` 里能看到类似 `Memory cgroup out of memory: Killed process ... python3` 的日志。

**为什么会这样**:一旦这个 cgroup 里所有进程的内存总量摸到 `memory.max` 设置的上限,内核的 memory controller 会在这个 cgroup 范围内触发 OOM Killer,挑一个进程杀掉来回收内存——**这个杀戮被限制在 `mini-mem` 这个分组内**,宿主机上其他不属于这个分组的进程完全不受影响,哪怕宿主机整体内存还很充裕。

清理:

```bash
sudo rmdir /sys/fs/cgroup/mini-mem
```

---

### 子实验 C:PIDs 限制(挡住 fork 炸弹)

#### Step C1:创建 cgroup 并设置最大进程数

```bash
sudo mkdir /sys/fs/cgroup/mini-pids
echo 20 | sudo tee /sys/fs/cgroup/mini-pids/pids.max
```

**为什么这么做**:`pids.max` 限制这个 cgroup 里**累计能存在的进程/线程数量**上限。这里设成 20,方便快速看到效果,不需要等太久。

#### Step C2:开一个新 shell,把它自己放进这个 cgroup,再执行 fork 炸弹

```bash
sudo bash -c '
echo $$ | tee /sys/fs/cgroup/mini-pids/cgroup.procs
:(){ :|:& };:
'
```

**为什么这么做**:`echo $$ | tee cgroup.procs` 是把**当前这个 bash 自己**加入 `mini-pids` 分组——因为子进程会自动继承父进程所属的 cgroup,所以只要把这个 shell 放进去,它之后 fork 出的所有子孙进程也都跑不出这个分组的限制。`:(){ :|:& };:` 是经典的 bash fork 炸弹,会不停地自我复制。

**安全提示**:一定要先确认 `pids.max` 已经设置成功、且确实把当前 shell 加入了这个 cgroup,再执行 fork 炸弹这一步,否则会拖垮整台机器。如果不放心,可以先用 `cat /sys/fs/cgroup/mini-pids/cgroup.procs` 确认当前 shell 的 PID 已经在里面,再执行 fork 炸弹命令。

#### Step C3:观察结果

**观察**:屏幕上会很快开始刷 `bash: fork: retry: Resource temporarily unavailable` 或者 `bash: fork: Resource temporarily unavailable` 这样的报错,进程数量被死死摁在 20 左右,不会继续暴涨。此时切到另一个终端(没加入这个 cgroup 的)执行:

```bash
uptime
ps aux | wc -l
```

**观察**:宿主机整体依然响应正常,系统总进程数没有失控暴涨。

**为什么会这样**:内核在每次 `fork()`/`clone()` 系统调用发生时,都会检查发起调用的进程属于哪个 cgroup,以及这个 cgroup 当前的进程数是否已经达到 `pids.max`。一旦达到,`fork()` 系统调用本身直接返回失败(`EAGAIN`),新进程根本不会被创建出来——**这不是"创建了很多进程再杀掉",而是"从源头上不让你创建"**。这正是为什么这个防护措施能够有效防止一个失控的容器(比如程序 bug 导致的意外 fork 炸弹,或者被利用的安全漏洞)拖垮整台宿主机。

清理(需要先清掉里面还活着的进程,再删除 cgroup):

```bash
sudo pkill -9 -f ':(){ :|:& };:'
sudo rmdir /sys/fs/cgroup/mini-pids
```

## 5. 实验结果分析

- 三个子实验虽然限制的资源不同(CPU 时间、内存页、PID 数量),但触发限制的**方式完全一致**:进程本身对限制毫无感知,是内核在它申请资源的那一刻直接介入(限流/杀死/拒绝创建)。
- OOM 和 PIDs 限制都体现了"故障隔离"(fault isolation)的价值:失控的负载被关在自己的 cgroup 分组里,不会波及宿主机上其他不相关的进程。
- cgroups 和 Namespace 是两个完全正交的机制:一个进程可以只被限制资源、看到完整的宿主机视图;也可以只被隔离视图、完全不受资源限制(第九章就是这种情况)。Docker 容器同时用了两者,才既"看不到别人",又"用不爆资源"。

## 6. Docker 对应关系

```
今天实验:

mkdir /sys/fs/cgroup/mini-cpu
echo "50000 100000" > .../cpu.max
echo $PID > .../cgroup.procs

        |
        v

Docker:

docker run --cpus="0.5" --memory="50m" --pids-limit=20 myapp

        |
        v

runc 根据 OCI config.json 里的 linux.resources 字段,
在容器进程启动前后为它创建/配置对应的 cgroup v2 分组,
并把容器主进程的 PID 写入该分组的 cgroup.procs
```

`docker stats` 命令展示的 CPU%、内存使用量,读取的正是各容器所在 cgroup 目录下 `cpu.stat`、`memory.current` 这些统计文件。

## 7. 心智模型

```
cgroups
    =
内核在资源分配的那一刻,强制收口一组进程能用多少

不是:
容器"自觉"控制自己用多少资源
```

## 8. 思考题

1. 为什么 CPU 超限是"限流"(throttle,进程还活着,只是变慢),而内存超限是"杀死"(kill)?这跟 CPU 时间片和内存页在内核里的性质有什么本质区别(提示:CPU 可以"晚一点再给你",内存页被别的进程占用后能"晚一点再还给你"吗)?
2. `pids.max` 限制的是"进程数量",不限制"每个进程能用多少内存/CPU"。如果一个容器只设置了 `pids-limit` 而没设置 `memory` 限制,一个失控的多进程程序(不是fork炸弹,而是每个子进程都吃很多内存)还是可能拖垮宿主机吗?
3. 结合这一章"进程对限制无感知"的结论,以及第一章"容器只是一个普通进程"的结论,你能不能推断出:如果直接对一个进程的 PID 手动执行 `echo $PID > /sys/fs/cgroup/somegroup/cgroup.procs`,而这个进程完全不是通过 `docker run` 启动的,cgroups 限制是否依然会对它生效?

## 9. 延伸阅读

- `man 7 cgroups`
- Kernel 文档:`Documentation/admin-guide/cgroup-v2.rst`(重点看 `cpu.max`、`memory.max`、`pids.max`、`memory.events` 几节)
- runc 源码:`libcontainer/cgroups/fs2/`(cgroup v2 driver 的具体实现,终章会再提到)

---

下一章:[第十一章:组合成 Mini Container](11-mini-container.md) —— 把 Namespace + OverlayFS + chroot + cgroups 全部串成一个可以直接运行的脚本。
