# Linux Container Networking 知识体系（Knowledge Map）

> 学习目标：
>
> 建立 Linux Container Networking 的完整心智模型，而不是零散记忆 Docker 命令。

---

# 总体架构

```text
                      Internet / LAN
                             │
                        Physical NIC
                             │
                     Linux Network Stack
                             │
                Routing / Netfilter / NAT
                             │
                     Linux Bridge (L2)
                  ┌──────────┴──────────┐
                  │                     │
               veth                 veth
                  │                     │
        Network Namespace      Network Namespace
          (Container A)          (Container B)
```

Docker Networking 本质上就是：

Linux Networking
+
Namespace
+
Virtual Device
+
Bridge
+
Routing
+
Netfilter

---

# 一、Network Namespace

## 作用

提供独立网络栈。

每个 Namespace 都拥有自己的：

- Network Device
- IP Address
- Routing Table
- ARP Table
- Firewall
- Socket
- TCP/IP Stack

可以理解为：

```text
Namespace == 一台独立 Linux 主机
```

---

# 二、Virtual Network Device

Linux 可以创建很多"虚拟网卡"。

## 1. veth pair

作用：

连接两个 Namespace。

心智模型：

```text
Virtual Ethernet Cable

vethA <=================> vethB
```

职责：

仅负责：

```
skb Forward
```

不会：

- Routing
- NAT
- Switching

类比：

```
现实世界：

网线
```

---

## 2. loopback

每个 Namespace 都有：

```
lo
```

作用：

本机通信。

例如：

```
127.0.0.1
```

---

## 3. dummy

没有任何实际连接。

主要：

测试。

---

## 4. macvlan

作用：

给每个 Container 一个真实 MAC。

拓扑：

```text
          Switch
             │
          eth0
             │
        macvlan driver
        ├─────────────┐
        │             │
Container A      Container B
 MAC A             MAC B
```

特点：

Container 看起来像局域网中的真实主机。

无需 Bridge。

无需 NAT。

适合：

- 网络设备模拟
- DHCP
- Router
- Firewall

---

## 5. ipvlan

与 macvlan 类似。

区别：

多个 Container 共用一个 MAC。

每个拥有不同 IP。

适合：

减少 MAC 数量。

---

## 6. TUN/TAP

用于：

VPN

QEMU

WireGuard

OpenVPN

TAP：

二层设备。

TUN：

三层设备。

---

# 三、Linux Bridge

作用：

Linux Software Switch。

拓扑：

```text
         docker0

     +--------------+
     |              |
     +--------------+
      │      │
    veth1  veth2
      │      │
 Container Container
```

职责：

根据：

```
MAC Address
```

决定：

数据发往哪个端口。

类比：

```
Bridge == Switch
```

---

# 四、Routing

作用：

不同 Network 通信。

例如：

```
172.17.0.0/16

↓

192.168.1.0/24
```

Routing 决定：

下一跳。

Linux：

```
ip route
```

---

# 五、ARP / Neighbor

作用：

IP

↓

MAC

转换。

Linux：

```
ip neigh
```

Bridge 转发之前：

必须知道：

Destination MAC。

---

# 六、Netfilter

Linux 数据包过滤框架。

负责：

- Firewall
- NAT
- Conntrack

整个数据流：

```text
        PREROUTING

             │

         Routing

        │          │

 INPUT        FORWARD

        │          │

      OUTPUT

             │

       POSTROUTING
```

---

# 七、iptables

Netfilter 用户接口。

主要 Table：

```
filter

nat

mangle

raw
```

Docker 最常用：

nat。

---

# 八、NAT

作用：

地址转换。

分：

## SNAT

Source NAT

例如：

```
172.17.0.2

↓

192.168.1.100
```

用于：

Container

↓

Internet

---

## DNAT

Destination NAT

例如：

```
Host:8080

↓

Container:80
```

用于：

Port Mapping。

---

## MASQUERADE

SNAT 特殊形式。

Docker 默认：

```
MASQUERADE
```

---

# 九、Conntrack

作用：

连接跟踪。

保存：

```
TCP State

UDP Session
```

保证：

NAT

知道：

响应应该返回哪里。

---

# 十、Overlay Network

单机：

Bridge。

跨主机：

Overlay。

例如：

```text
Host A                  Host B

Container A

      │

VXLAN Tunnel

      │

Container B
```

常见：

- VXLAN
- Geneve
- GRE

---

# 十一、VXLAN

作用：

二层网络

封装

UDP

实现：

跨主机 Layer2。

Docker Overlay：

就是：

VXLAN。

---

# 十二、Docker Bridge

Docker 默认：

```
docker0
```

实际上就是：

```
Linux Bridge
```

Docker 自动：

- 创建 Bridge
- 创建 veth
- 分配 IP
- 配置 iptables

---

# 十三、Host Network

Container：

直接使用：

Host Namespace。

没有：

```
veth

Bridge

NAT
```

性能最好。

---

# 十四、None Network

只有：

```
lo
```

没有：

任何网络。

---

# 十五、Macvlan Network

Container：

直接出现在：

LAN。

拓扑：

```text
Switch

   │

Host eth0

   │

macvlan

├──────┐

C1      C2
```

---

# 十六、Container Network Interface（CNI）

Kubernetes：

不会自己配置网络。

调用：

CNI Plugin。

例如：

- bridge
- macvlan
- ipvlan
- flannel
- calico
- cilium

---

# 十七、完整数据流

Container A

↓

eth0

↓

veth

↓

Linux Bridge

↓

Routing

↓

Netfilter

↓

Host eth0

↓

Internet

响应：

Internet

↓

Host

↓

Netfilter

↓

Bridge

↓

veth

↓

Container

---

# 十八、最终心智模型

```text
                     Internet
                         │
                    Physical NIC
                         │
                Linux Routing Engine
                         │
             Netfilter / NAT / Conntrack
                         │
                  Linux Bridge (L2)
          ┌──────────────┼──────────────┐
          │              │              │
       veth pair      veth pair      veth pair
          │              │              │
    Network NS      Network NS      Network NS
   (Container A)   (Container B)   (Container C)
          │              │              │
      TCP/IP Stack   TCP/IP Stack   TCP/IP Stack
```

---

# 核心类比（牢记）

| Linux 网络组件 | 类比现实网络 |
|---------------|-------------|
| Network Namespace | 一台独立主机 |
| veth pair | 一根网线 |
| Linux Bridge | 二层交换机 |
| Routing Table | 路由器 |
| ARP Table | 通讯录（IP→MAC） |
| Netfilter | 防火墙 |
| NAT | 地址转换器 |
| Conntrack | 会话记录本 |
| Overlay/VXLAN | 跨机房虚拟交换网络 |
| Docker | 自动化网络管理员 |
| CNI | Kubernetes 网络安装接口 |

> **最终心智模型：Docker 并没有发明新的网络协议，它只是自动化地组合了 Linux Kernel 已有的网络能力。掌握 Linux 网络组件，就掌握了 Docker Networking；掌握 Docker Networking，又为 Kubernetes Networking 打下了坚实基础。**