# 第六章:veth + bridge(联网)

## 1. 学习目标

亲手把上一章那个"网络孤岛" `ns1` 重新接回互联网,理解 Docker 默认桥接网络(`docker0`)背后到底是怎么把每个容器的 Network Namespace 连通到宿主机和外网的。

## 2. 前置知识

- 第五章:Network Namespace 默认是空的网络孤岛
- 知道什么是网卡、IP 地址、子网掩码、默认路由
- 知道 NAT(网络地址转换)的基本概念:把内网 IP 转换成宿主机 IP 再发出去

## 3. 原理简介

要让 `ns1` 里的进程能访问外网,需要解决两个问题:

1. **物理连通**:`ns1` 需要一根"网线"接到宿主机的网络上。
2. **地址转换**:`ns1` 里的 IP 大概率是私有地址(比如 `10.0.0.2`),外网不认识这个地址,需要在出宿主机之前换成宿主机自己的公网/局域网 IP。

Linux 提供的解法:

- **veth pair(虚拟以太网线对)**:创建出来永远是成对的两个虚拟网卡,一端塞进 `ns1`,另一端留在宿主机,数据包从一端进去,必然从另一端出来——完全模拟一根网线两头插在两台机器上。
- **bridge(网桥)**:一个软件二层交换机,宿主机上创建一个 bridge,把 veth 留在宿主机这一端,以及物理网卡(或者更多别的容器的 veth)都接上去,它们就处于同一个二层网络里,可以互相通信。
- **NAT(iptables MASQUERADE)+ IP 转发**:宿主机开启 `ip_forward`,并用 iptables 把从 bridge 网段出去的包做源地址转换,伪装成宿主机的 IP 发到外网,回包再转换回来发给原来的私有 IP。

```
外网
  |
宿主机物理网卡 (eth0, 192.168.1.10)
  |
[ iptables MASQUERADE + ip_forward ]
  |
br-mini (10.0.0.1)                <- 宿主机上的软件交换机
  |
veth-host  <---- veth pair ---->  veth-ns1 (10.0.0.2)
  |                                    |
宿主机 Network Namespace          ns1 (第五章创建的网络孤岛)
```

这正是 Docker 默认 bridge 网络模式(`docker0` + 每容器一对 veth + iptables MASQUERADE)的完整实现原理,只是这一章我们手工做一遍。

## 4. 实验步骤

> 沿用第五章创建的 `ns1`。如果已经删除,先执行 `sudo ip netns add ns1`。

### Step 1:创建 bridge

执行:

```bash
sudo ip link add br-mini type bridge
sudo ip addr add 10.0.0.1/24 dev br-mini
sudo ip link set br-mini up
```

**为什么这么做**:`br-mini` 相当于宿主机上多了一台"虚拟交换机",给它一个 IP(`10.0.0.1`)是为了让它同时兼任这个私有网段(`10.0.0.0/24`)的网关地址——后面 `ns1` 的默认路由会指向它。

### Step 2:创建 veth pair

执行:

```bash
sudo ip link add veth-host type veth peer name veth-ns1
```

观察:

```bash
ip link show | grep veth
```

会看到两个新网卡 `veth-host` 和 `veth-ns1`,此时它们都还在宿主机的 Network Namespace 里。

**为什么这么做**:`veth` 类型的设备天生成对出现,一端收到的包,另一端一定能收到,中间没有交换机、没有内核路由参与——纯粹是一根"虚拟网线"。

### Step 3:把 veth 一端丢进 `ns1`,另一端接上 bridge

执行:

```bash
sudo ip link set veth-ns1 netns ns1
sudo ip link set veth-host master br-mini
sudo ip link set veth-host up
```

**为什么这么做**:

- `ip link set veth-ns1 netns ns1`:把这张网卡从宿主机的 Network Namespace"移动"进 `ns1`。注意这不是复制,移动之后宿主机这边就再也看不到 `veth-ns1` 了(`ip link show` 会证实这一点)。
- `ip link set veth-host master br-mini`:把留在宿主机这端的 `veth-host` 接入 `br-mini` 这个网桥,相当于插了一根网线到交换机上。

### Step 4:配置 `ns1` 内部的网络

执行:

```bash
sudo ip netns exec ns1 ip link set lo up
sudo ip netns exec ns1 ip addr add 10.0.0.2/24 dev veth-ns1
sudo ip netns exec ns1 ip link set veth-ns1 up
sudo ip netns exec ns1 ip route add default via 10.0.0.1
```

观察:

```bash
sudo ip netns exec ns1 ping -c 2 10.0.0.1
```

这一步应该已经能 ping 通宿主机的网桥地址了。

**为什么这么做**:给 `ns1` 里那一端的 veth 分配一个和 `br-mini` 同网段的 IP(`10.0.0.2/24`),再加一条默认路由指向网桥地址(`10.0.0.1`),相当于告诉 `ns1`:"任何不认识的目的地,都先送到网关(网桥)去"。到这一步,`ns1` 已经不再是孤岛,而是接入了一个只有它自己和宿主机的小型局域网。

### Step 5:开启宿主机的 IP 转发

执行:

```bash
sudo sysctl -w net.ipv4.ip_forward=1
```

**为什么这么做**:默认情况下,Linux 主机收到"不是发给自己的包"会直接丢弃,不会帮忙转发(除非明确配置成路由器角色)。这里我们需要宿主机在"来自 `br-mini` 网段"和"物理网卡"之间转发数据包,所以必须打开这个内核开关。

### Step 6:配置 NAT(MASQUERADE)

执行(把 `eth0` 换成你机器实际连外网的网卡名,用 `ip route get 8.8.8.8` 可以查到):

```bash
sudo iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -o eth0 -j MASQUERADE
```

**为什么这么做**:`10.0.0.2` 这个地址只在这台宿主机内部有意义,外网路由器根本不知道该往哪儿回它的包。MASQUERADE 规则的作用是:凡是源地址属于 `10.0.0.0/24`、要从 `eth0` 出去的包,把源地址临时替换成 `eth0` 的地址;收到回包时再反向替换回来发给 `10.0.0.2`。这一步执行完,理论上出去的包已经能被外网正确路由和回应了。

### Step 7:验证能上网了

执行:

```bash
sudo ip netns exec ns1 ping -c 3 8.8.8.8
```

观察:这次应该能 ping 通(前提是宿主机本身能访问外网,且没有额外防火墙拦截)。

**为什么会这样**:数据包路径是——`ns1` 内的进程发包 → `veth-ns1` → `veth-host`(对端)→ `br-mini` → 内核路由决定走 `eth0` → 经过 NAT 换源地址 → 到达外网 → 回包原路返回并被 NAT 换回目的地址 → 送回 `ns1`。整条链路里的每一环,都是这一章亲手搭出来的。

## 5. 实验结果分析

- Network Namespace 的"孤岛"状态,是通过 veth pair 这种"虚拟网线"打破的——一端插进孤岛,一端插上宿主机的交换机(bridge)。
- bridge 让多个 veth(未来可以是多个容器)处于同一个二层网段,天然支持"容器之间互相通信"。
- 真正让孤岛能访问外网的,是宿主机层面的路由转发 + NAT,这两者都发生在**宿主机默认 Network Namespace**,不属于任何容器,这也是为什么容器网络配置改动往往需要 root 权限操作宿主机。

## 6. Docker 对应关系

```
今天实验:

ip link add br-mini type bridge
ip link add veth-host type veth peer name veth-ns1
ip link set veth-ns1 netns ns1
ip link set veth-host master br-mini
iptables -t nat -A POSTROUTING ... -j MASQUERADE

        |
        v

Docker 默认 bridge 网络:

docker network create / dockerd 启动时自动创建的 docker0 网桥
docker run 时,dockerd 自动:
  - 创建一对 veth
  - 一端放入容器的 Network Namespace,命名为 eth0
  - 另一端接入 docker0 网桥
  - 分配一个 172.17.0.0/16 段内的 IP 给容器
  - 维护 iptables 的 MASQUERADE / DNAT(用于 -p 端口映射)规则
```

`docker run -p 8080:80` 里的端口映射,本质上就是在 `PREROUTING`/`DOCKER` 链上加一条 DNAT 规则,把宿主机 `8080` 端口的流量转发到容器的 `172.17.x.x:80`——原理和这一章的 MASQUERADE 是同一套 iptables NAT 机制,只是方向和规则链不同。

## 7. 心智模型

```
veth + bridge + NAT
    =
用软件网线和软件交换机
把一个"网络孤岛"重新接入现实世界

不是:
Docker 内置了什么特殊的虚拟网络协议
```

## 8. 思考题

1. 为什么 Docker 给每个容器分配独立的 IP 网段(比如 `172.17.0.0/16`),而不是直接让容器共用宿主机 IP?这跟本章"端口不冲突"的结论有什么联系,又带来了什么新问题(提示:外部怎么访问容器)?
2. 如果两个容器都连在同一个 `docker0` 网桥上,它们之间互相 `ping` 通信,数据包需不需要经过 NAT、需不需要经过宿主机的物理网卡?
3. `docker network create mynet` 创建的自定义网络,和这一章手工创建的 `br-mini`,在实现原理上是同一个东西吗?自定义网络额外多提供了什么(提示:容器名 DNS 解析)?

## 9. 延伸阅读

- `man 8 ip-link`(veth 部分)
- `man 8 iptables`(MASQUERADE target)
- Kernel 文档:`Documentation/networking/bridge.rst`
- Docker 官方文档:[Bridge network driver](https://docs.docker.com/network/drivers/bridge/)

---

## 清理本章创建的资源(可选)

```bash
sudo ip netns del ns1
sudo ip link del br-mini
sudo iptables -t nat -D POSTROUTING -s 10.0.0.0/24 -o eth0 -j MASQUERADE
```

（`veth-host` 会随着 `ns1` 被删除、`veth-ns1` 消失而自动一起消失，因为 veth pair 是成对生灭的。）

---

下一章:[第七章:OverlayFS](07-overlayfs.md) —— 到目前为止我们隔离的是"进程视角",接下来隔离"文件系统内容"。
