# 第五章:Network Namespace(隔离)

## 1. 学习目标

理解 Network Namespace 如何给一组进程一整套"独立的网络栈"(网卡、路由表、iptables 规则、端口空间),并亲眼验证:一个新建的 Network Namespace 默认**完全没有网络**,连本机回环都要单独看。

本章只做"隔离"，先不联网——先把问题看清楚,下一章再解决它。

## 2. 前置知识

- 前面几章的心智模型:Namespace = 改变 View
- 知道网卡(interface)、IP 地址、路由表是什么
- 会用 `ip addr`、`ip link`、`ping` 基本命令

## 3. 原理简介

正常情况下,一台机器只有一套网络栈:

```
Host

网卡: eth0 (192.168.1.10), lo (127.0.0.1)
路由表: 一份
iptables 规则: 一份
端口: 0-65535,所有进程共享同一个端口空间
```

这带来两个明显问题:

1. **端口冲突**:两个容器都想监听 `80` 端口,在同一套网络栈里天然冲突。
2. **网络策略隔离**:你没法只对某一个容器生效一条 iptables 规则,除非用一堆条件去匹配它的流量。

Network Namespace 解决的问题:**给一组进程一整套从零开始的网络栈**——没有网卡(除了系统自动给的 `lo`,而且默认是 down 的)、没有路由、没有 iptables 规则、有自己独立的 0-65535 端口空间。这也是为什么"不同容器都监听 80 端口"完全不冲突的根本原因。

```
Host Network Namespace

eth0 (192.168.1.10)
lo   (127.0.0.1, UP)

              |
              | ip netns add ns1
              v

Network Namespace "ns1"

lo   (127.0.0.1, DOWN)   <- 只有这一个,且默认关闭
没有 eth0
没有路由表项
没有和外界的任何连接
```

## 4. 实验步骤

### Step 1:创建一个具名的 Network Namespace

执行:

```bash
sudo ip netns add ns1
sudo ip netns list
```

**为什么这么做**:和前面用 `unshare` 不同,这里用 `ip netns add`。它做的事情本质上也是创建一个 Network Namespace,但额外做了一件事:把这个 Namespace 通过 bind mount 挂到 `/var/run/netns/ns1` 这个文件上,让它在**没有任何进程存活**的情况下也能被引用、被之后的命令找到(回忆第三章:普通 Namespace 会随最后一个引用进程的退出而消失,这里通过挂载点人为制造了一个"引用")。

### Step 2:在这个 Namespace 里看网络情况

执行:

```bash
sudo ip netns exec ns1 ip addr
sudo ip netns exec ns1 ip link
```

观察:只有 `lo` 一个接口,状态是 `DOWN`,没有 IP 地址。

**为什么这么做**:`ip netns exec ns1 <command>` 相当于"进入 ns1 这个 Network Namespace 后执行 `<command>`"。它内部做的事情跟 `unshare --net` 类似,只是从一个已经存在、有名字的 Namespace 里进去,而不是新建一个。

### Step 3:验证连不通网

执行:

```bash
sudo ip netns exec ns1 ping -c 2 127.0.0.1
sudo ip netns exec ns1 ping -c 2 8.8.8.8
```

观察:第一条 `ping 127.0.0.1` 大概率直接失败(网络不可达或者连不上),因为 `lo` 接口是 `DOWN` 的;第二条 `ping 8.8.8.8` 必然失败(没有默认路由,也没有出口网卡)。

**为什么会这样**:这个 Namespace 里除了一个"关着的" `lo`,什么都没有。既没有能发包的网卡,也没有告诉内核"包该往哪儿送"的路由表。这是完全符合预期的隔离效果——本章的目的就是让你直观感受到这种"一穷二白"的状态。

### Step 4:把 `lo` 打开,验证本地回环级别的网络是可以恢复的

执行:

```bash
sudo ip netns exec ns1 ip link set lo up
sudo ip netns exec ns1 ping -c 2 127.0.0.1
```

观察:这次 `ping` 能成功。

**为什么会这样**:`lo` 接口是存在的,只是初始状态是 down。打开它之后,这个 Namespace 内部的"自己跟自己通信"(比如同一个 Namespace 里两个进程通过 `127.0.0.1` 互相访问)是可以工作的,但依然完全无法访问外部世界——因为还没有任何一张能通往外部的网卡。这个"外部连通性"正是下一章要解决的问题。

## 5. 实验结果分析

- 新 Network Namespace 默认是完全孤立的网络孤岛:没有网卡、没有路由、没有和宿主机或外网的任何联系。
- `ip netns add` 相比 `unshare --net` 多做的"挂载持久化"这一步,是 Docker 得以脱离具体进程、提前准备好容器网络环境的关键机制(下一章会看到 Docker 是如何借助类似机制,在容器进程真正启动前就能把 veth 一端配置好的)。
- 端口、路由、iptables 规则都是"每个 Network Namespace 各一份"的资源,这就是为什么多个容器可以各自监听 80 端口而不冲突。

## 6. Docker 对应关系

```
今天实验:

ip netns add ns1
ip netns exec ns1 ip addr

        |
        v

Docker:

docker run -d nginx

        |
        v

runc 内部调用:

clone(CLONE_NEWNET | ...)
容器一启动,就已经处在一个全新的、空的 Network Namespace 里
```

Docker 默认给每个容器创建的 Network Namespace,初始状态和这一章看到的一模一样——什么都没有。容器之所以最终能上网、能被 `-p` 映射端口访问,靠的是 Docker 在容器启动前后,用下一章要学的 veth + bridge 技术把这个"网络孤岛"重新接上了宿主机的网络。

## 7. 心智模型

```
Network Namespace
    =
一整套独立的网络栈(网卡/路由表/iptables/端口空间)
默认状态是"网络孤岛",什么都没有

不是:
自动帮你配好网络的黑箱
```

## 8. 思考题

1. 为什么 `ip netns add` 要通过 bind mount 一个文件来"固定住"这个 Namespace,而不是像 `unshare` 那样只依赖进程存活?这跟 Docker 需要"提前准备好容器网络、容器进程还没启动就能操作它的网卡"这个需求有什么关系?
2. 两个不同的容器(两个不同的 Network Namespace)都监听 TCP 80 端口,操作系统的端口占用检测(`bind()` 失败判断)为什么不会互相冲突?
3. 如果一个 Network Namespace 里只有 `lo`,而且默认是 down 的,那 Docker 容器里执行 `curl localhost:8080` 之所以能生效(如果容器自己监听了 8080),依赖的是这一章的哪一个观察结果?

## 9. 延伸阅读

- `man 7 network_namespaces`
- `man 8 ip-netns`
- `man 8 ip-link`

---

下一章:[第六章:veth + bridge(联网)](06-network-veth-bridge.md) —— 把这个网络孤岛重新接回互联网,这正是 Docker 默认桥接网络的真实实现方式。
