## Linux v3.2 Kernel 调试指南（Ubuntu Server + QEMU + GDB）

本指南将以下两份笔记整理合并为**一份结构化、可复用的调试手册**：

- `evc_cli/gdb_qemu_kernel.md`（环境搭建 + QEMU/GDB 启动 + 网络栈断点地图）
- `evc_cli/TCP_sendmsg_walkthrough.md`（以 `tcp_sendmsg` 为主线的源码级调试 walkthrough）

---

## 目录

- [总体架构](#arch)
- [环境准备](#prereq)
- [获取与编译 Linux v3.2（带调试符号）](#build-kernel)
- [构建最小 rootfs（BusyBox initramfs）](#rootfs)
- [启动 QEMU（开启 gdbstub）](#qemu-run)
- [使用 GDB 连接与常用命令](#gdb)
- [Kernel 网络方向调试 Playbook（断点地图 + 对象观察）](#net-playbook)
- [案例：tcp_sendmsg 源码级 debug walkthrough（v3.2 思路）](#case-tcp-sendmsg)
- [案例：tcp_v4_rcv 接收路径源码级 debug walkthrough（v3.2 思路）](#case-tcp-v4-rcv)
- [附录：GDB 提效片段（可选）](#appendix-gdb)

---

<a id="arch"></a>
## 总体架构

```text
Local Machine (optional)
    |
    | gdb (remote attach)
    v
+----------------------------------+
| Remote Ubuntu Server             |
|                                  |
|   +--------------------------+   |
|   | QEMU VM                  |   |
|   |  (Linux v3.2 Kernel)     |   |
|   +--------------------------+   |
|           ^                      |
|           | gdbstub (:1234)      |
|           v                      |
|   +--------------------------+   |
|   | gdb (local or remote)    |   |
|   +--------------------------+   |
+----------------------------------+
```

说明：

- **QEMU**：运行自编译的 Linux v3.2 内核 + initramfs rootfs
- **gdbstub**：QEMU 内置调试端口，默认 `:1234`
- **GDB**：加载 `vmlinux`（带符号）后连接 gdbstub，实现断点、单步、回溯、查看结构体等

---

<a id="prereq"></a>
## 环境准备

### 安装依赖

```bash
sudo apt update

sudo apt install -y \
  build-essential \
  qemu-system-x86 \
  qemu-kvm \
  gdb \
  git \
  flex bison \
  libssl-dev \
  libelf-dev \
  bc \
  cpio
```

### 检查 KVM 支持（强烈建议）

```bash
egrep -c '(vmx|svm)' /proc/cpuinfo
```

- 返回 `> 0`：通常表示 CPU 支持虚拟化（KVM 可能可用）
- 返回 `= 0`：只能用 TCG（会慢很多，但仍能调试）

---

<a id="build-kernel"></a>
## 获取与编译 Linux v3.2（带调试符号）

### 获取源码

```bash
mkdir -p ~/kernel-debug
cd ~/kernel-debug

git clone https://github.com/torvalds/linux.git
cd linux
git checkout v3.2
```

### 配置与开启调试选项

```bash
make defconfig
make menuconfig
```

建议启用（路径可能随菜单略有差异）：

- `Kernel hacking` →
  - `[*] Compile the kernel with debug info`
  - `[*] KGDB: kernel debugger`
  - `[*] KGDB: use kgdb over the serial console`

### 编译

```bash
make -j"$(nproc)"
```

你会得到：

- `vmlinux`：**GDB 使用**（带符号信息）
- `arch/x86/boot/bzImage`：**QEMU 启动**使用

---

<a id="rootfs"></a>
## 构建最小 rootfs（BusyBox initramfs）

### 编译 BusyBox

```bash
cd ~/kernel-debug
git clone https://busybox.net/git/busybox.git
cd busybox

make defconfig
make -j"$(nproc)"
make install
```

### 创建 init 脚本与目录

```bash
cd _install
mkdir -p proc sys dev etc

cat > init << 'EOF'
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
echo "Boot OK"
exec /bin/sh
EOF

chmod +x init
```

### 打包 initramfs

```bash
find . | cpio -o --format=newc > ../initramfs.cpio
```

---

<a id="qemu-run"></a>
## 启动 QEMU（开启 gdbstub）

在内核目录启动（确保 `bzImage` 路径正确，`initramfs.cpio` 指向 busybox 打包输出）：

```bash
cd ~/kernel-debug/linux

qemu-system-x86_64 \
  -enable-kvm \
  -cpu host \
  -kernel arch/x86/boot/bzImage \
  -initrd ../busybox/initramfs.cpio \
  -append "console=ttyS0 nokaslr" \
  -nographic \
  -s -S
```

关键参数速查：

- `-s`：开启 gdbstub（监听 `tcp::1234`）
- `-S`：CPU 上电后**先暂停**，等待 GDB 连接后再继续
- `nokaslr`：禁用地址随机化，断点与符号更稳定
- `-nographic` + `console=ttyS0`：串口输出到当前终端

---

<a id="gdb"></a>
## 使用 GDB 连接与常用命令

### 连接流程

在另一个终端（或同机另一个会话）：

```bash
cd ~/kernel-debug/linux
gdb vmlinux
```

在 gdb 里连接 QEMU：

```gdb
target remote :1234
```

建议先打一个“早期入口”的断点（例如 `start_kernel`）：

```gdb
b start_kernel
c
```

### 常用命令速查

```gdb
c                    # continue
si                   # step instruction (into)
ni                   # next instruction
bt                   # backtrace
info registers       # registers
x/10i $rip           # disassemble around RIP
```

---

<a id="net-playbook"></a>
## Kernel 网络方向调试 Playbook（断点地图 + 对象观察）

### 网络栈整体结构（心智模型）

```text
User Space
    ↓
syscall
    ↓
socket layer
    ↓
TCP/IP stack
    ↓
net core
    ↓
driver
```

### 三条核心路径（建议先背下来）

发送路径（TX）：

```text
send()
 → sys_sendto
 → sock_sendmsg
 → tcp_sendmsg
 → ip_queue_xmit
 → dev_queue_xmit
 → driver
```

接收路径（RX）：

```text
interrupt
 → netif_receive_skb
 → ip_rcv
 → tcp_v4_rcv
 → recv()
```

softirq / NAPI（理解“中断减压”机制）：

```text
interrupt
 → napi_schedule
 → NET_RX_SOFTIRQ
 → net_rx_action
 → netif_receive_skb
```

### 断点地图（一次性设好，边走边删）

TX：

```gdb
b sys_sendto
b sock_sendmsg
b inet_sendmsg
b tcp_sendmsg
b tcp_write_xmit
b ip_queue_xmit
b dev_queue_xmit
```

RX：

```gdb
b netif_receive_skb
b ip_rcv
b tcp_v4_rcv
b sock_def_readable
```

softirq：

```gdb
b net_rx_action
b __do_softirq
b napi_poll
```

驱动层（按网卡类型二选一/按实际符号调整）：

```gdb
b e1000_xmit_frame
b e1000_clean_rx_irq
```

或 virtio：

```gdb
b virtnet_poll
b virtio_net_xmit
```

### 关键数据结构（建议优先盯这三个）

- `struct sk_buff *skb`：数据载体（payload、协议头、关联设备）
- `struct sock *sk`：socket 状态（发送队列、窗口、拥塞）
- `struct net_device *dev`：网卡对象

常用查看：

```gdb
p *skb
p *sk
p *dev
```

`skb` 重点字段方向（实际字段名随版本略有变化）：

- `len`
- `data`
- `protocol`
- `dev`

---

<a id="case-tcp-sendmsg"></a>
## 案例：tcp_sendmsg 源码级 debug walkthrough（v3.2 思路）

### 先建立 tcp_sendmsg 的“执行骨架”

```text
send()
 → sys_sendto
 → sock_sendmsg
 → inet_sendmsg
 → tcp_sendmsg
    ↓
    tcp_push / tcp_write_xmit
    ↓
    ip_queue_xmit
    ↓
    dev_queue_xmit
```

### tcp_sendmsg 的结构化伪代码（便于对照断点）

```c
int tcp_sendmsg(struct kiocb *iocb, struct socket *sock,
                struct msghdr *msg, size_t size)
{
    struct sock *sk = sock->sk;
    int copied = 0;

    lock_sock(sk);

    while (size > 0) {
        skb = tcp_write_queue_tail(sk);
        if (!skb || /* not enough room */)
            skb = sk_stream_alloc_skb(sk);

        /* copy user data -> skb */
        copy_from_user_to_skb(skb);

        /* update tcp/skb state */
        skb->len += copy;
        copied += copy;
        size -= copy;

        if (should_push)
            tcp_push(sk);
    }

    release_sock(sk);
    return copied;
}
```

### Step 0：准备断点（建议照抄）

```gdb
b sys_sendto
b sock_sendmsg
b inet_sendmsg
b tcp_sendmsg
b tcp_write_xmit
b ip_queue_xmit
b dev_queue_xmit
```

### Step 1：在 QEMU 里触发一次发送

示例（任选其一）：

```bash
echo "hello" | nc <ip> 12345
```

或你自己的最小 client（核心是调用 `send(fd, "hello", 5, 0)`）。

### Step 2：命中 tcp_sendmsg 后先看调用栈

```gdb
c
bt
```

期望看到类似：

```text
tcp_sendmsg
inet_sendmsg
sock_sendmsg
sys_sendto
```

### Step 3：逐点观察“锁 → skb → 拷贝 → push”

1) **lock_sock（串行化）**

```gdb
n
```

2) **获取/分配 skb**

```gdb
n
p skb
```

如为空或空间不够，继续单步到分配处：

```gdb
n
p *skb
```

重点看 `skb->len/data/tail/end`（用来理解缓冲区与已写入长度）。

3) **数据拷贝（用户态 → 内核 skb）**

在拷贝后观察 payload：

```gdb
x/16bx skb->data
```

你应该能看到 `hello` 的字节模式。

4) **len 更新**

```gdb
p skb->len
```

### Step 4：进入 tcp_write_xmit（真正“发出去”的地方）

```gdb
b tcp_write_xmit
c
```

建议盯这些变量（理解流控/拥塞控制）：

```gdb
p sk->snd_una
p sk->snd_nxt
p sk->snd_wnd
```

### Step 5：进入 IP/设备层（封装与发射）

```gdb
b ip_queue_xmit
b dev_queue_xmit
c
```

看设备名：

```gdb
p skb->dev->name
```

---

<a id="case-tcp-v4-rcv"></a>
## 案例：tcp_v4_rcv 接收路径源码级 debug walkthrough（v3.2 思路）

这一节参考 `evc_cli/tcp_v4_rcv_walkthough.md`，目标是把 **RX 路径（收包 → 入 socket 队列 → 唤醒 → recv 拷贝）** 在 GDB 里走通。

### 先建立 RX 全路径骨架（必须记住）

```text
[硬件/虚拟网卡]
    ↓ interrupt
driver (e1000/virtio)
    ↓
napi_poll
    ↓
net_rx_action (softirq)
    ↓
netif_receive_skb
    ↓
ip_rcv
    ↓
tcp_v4_rcv
    ↓
tcp_rcv_established
    ↓
sock queue
    ↓
recv()
```

关键心智模型：

- **收包处理主要发生在 softirq（`net_rx_action`）里**，不是在硬中断里“完整处理完”
- `tcp_v4_rcv` 的本质是：**连接查找（4 元组）+ 分发到状态机**
- 真正“数据进入 socket”的关键点通常在 `tcp_rcv_established` 里把 skb 入 `sk_receive_queue`

### Step 0：一次性打好断点（强烈建议照抄）

```gdb
# 中断/softirq
b net_rx_action
b napi_poll
b netif_receive_skb

# IP 层
b ip_rcv

# TCP 核心
b tcp_v4_rcv
b tcp_rcv_established

# socket 层
b sock_def_readable
b tcp_recvmsg
```

### Step 1：触发流量（建议先用本地回环）

在 QEMU 里（任选其一）：

```bash
ping 127.0.0.1
```

或用 TCP 应用流量（推荐，方便对照 `tcp_recvmsg`）：

```bash
nc -l 12345
```

另开一个终端（同在 QEMU 里）：

```bash
nc 127.0.0.1 12345
```

### Step 2：从 softirq 入口开始（`net_rx_action`）

继续运行到断点：

```gdb
c
```

命中 `net_rx_action` 后，建议观察一次 softirq 的“处理预算”：

```gdb
p budget
```

这通常决定了一轮最多处理多少包（有助于理解“为什么有时候一次只走一部分”）。

### Step 3：进入 NAPI/驱动 poll（`napi_poll` → 驱动收包）

从 `net_rx_action` 单步进入 `napi_poll`：

```gdb
s
```

在 `napi_poll` 内部会调用设备 poll，常见会进入：

- `e1000_clean_rx_irq`（e1000）
- `virtnet_poll`（virtio-net）

驱动侧关键动作是：**从 ring 取 packet → 构造 skb → 上交内核**。

命中/拿到 `skb` 后优先看：

```gdb
p *skb
```

重点字段方向：

- `skb->data`
- `skb->len`
- `skb->protocol`

### Step 4：L2/L3 分发入口（`netif_receive_skb`）

跑到 `netif_receive_skb`：

```gdb
c
```

确认协议类型（IPv4 应该是 `0x0800`）：

```gdb
p skb->protocol
```

### Step 5：进入 IP 层（`ip_rcv`）

```gdb
c
```

在 `ip_rcv`，建议直接把 IP 头按结构体看一眼（便于确认“确实是你期望的包”）：

```gdb
p *(struct iphdr *)skb->data
```

### Step 6：进入 TCP 核心入口（`tcp_v4_rcv`）

```gdb
c
```

这里最重要的观察点：

1) **是否找到了对应 socket**

```gdb
p sk
```

如果 `sk == NULL`，通常意味着没匹配到连接（包会被丢弃/走别的分支）。

2) **TCP header 是否合理**

在多数路径里 TCP header 会在 IP header 之后（偏移可用“IP 头长度”概念理解）：

```gdb
p *(struct tcphdr *)(skb->data + iphdr_len)
```

注：`iphdr_len` 在不同版本/上下文里可能不是直接可用变量；核心目的是“把 TCP 头拿出来看一眼”。

### Step 7：真正的数据进入点（`tcp_rcv_established`）

从 `tcp_v4_rcv` 继续到 `tcp_rcv_established`，并重点盯两类东西：

1) **序列号检查**

```gdb
p TCP_SKB_CB(skb)->seq
p tp->rcv_nxt
```

它们体现了“包是否按序/是否可接收”。

2) **入 receive queue（进入 socket）**

通常会看到类似把 skb 入队：

```text
__skb_queue_tail(&sk->sk_receive_queue, skb);
```

验证队列变化：

```gdb
p sk->sk_receive_queue
```

（很多时候你更关心 `qlen` 增长，但字段表现随内核版本/调试符号不同可能略有差异。）

3) **唤醒用户态**

接着会调用：

```text
sk->sk_data_ready(sk, 0);
```

默认实现通常是 `sock_def_readable`，你可以用断点命中它来确认“唤醒发生了”。

### Step 8：用户态 `recv()` 取数据（`tcp_recvmsg`）

当用户态调用 `recv()`，会命中 `tcp_recvmsg`：

```gdb
c
```

它的核心就是：**从 `sk_receive_queue` 取 skb → `copy_to_user`**。

验证点：

```gdb
p skb->len
```

### 训练建议（可选，但很有效）

观察 seq 流动：

```gdb
display TCP_SKB_CB(skb)->seq
display ((struct tcp_sock *)sk)->rcv_nxt
```

观察 receive queue 增长：

```gdb
p sk->sk_receive_queue.qlen
```

制造丢包/乱序（在 QEMU 里）：

```bash
tc qdisc add dev lo root netem loss 10%
```

---

<a id="appendix-gdb"></a>
## 附录：GDB 提效片段（可选）

### 停下自动回溯

```gdb
define hook-stop
  bt
end
```

### 快速打印 skb（简化手工输入）

```gdb
define pskb
  p *$arg0
end
```

