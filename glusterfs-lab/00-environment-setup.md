# 00 - 实验环境搭建

本课程的所有实验都跑在同一套三节点环境里，从 Lesson 1 到 Lesson 10 状态持续累积、不重建。这份文档记录环境是怎么来的，后面每一课不再重复这些内容。

## 拓扑

```
Windows
  |
  WSL2 Ubuntu 22.04
  |
  Docker (user-defined bridge network: glusterlab-net, 172.28.0.0/24)
  |
  +-- node1  (172.28.0.11)  glusterd + glusterfs-server 10.1
  +-- node2  (172.28.0.12)  glusterd + glusterfs-server 10.1
  +-- node3  (172.28.0.13)  glusterd + glusterfs-server 10.1
```

三个容器地位完全对等，没有一个是"主节点"——这是第一课要验证的核心结论的伏笔。

## 为什么这样设计

| 决策 | 原因 |
|---|---|
| 用自定义 bridge network，而不是默认 bridge | 默认 bridge 网络没有内置 DNS，容器间只能用 IP 互相访问；自定义网络下 Docker 会给每个容器注册域名（`node1`/`node2`/`node3`），既能测 IP 通信也能测主机名通信，贴近真实多机部署 |
| 固定 IP（`--ip`） | 后面抓包、看 `gluster peer status` 输出、对照 `/etc/hosts` 时，IP 不会变，实验可重复 |
| 用 Ubuntu 22.04 官方源的 `glusterfs-server`（10.1），不用 PPA | 已验证 `apt-cache policy glusterfs-server` 直接有候选版本，无需额外第三方源，构建更快更稳定 |
| `glusterd` 以 `-N`（no-daemon）前台方式作为容器 PID 1 | 容器没有 systemd，PID 1 必须是一个前台进程；这样 `docker stop` 的信号能直接送达 `glusterd`，容器生命周期等于 `glusterd` 生命周期 |
| `--cap-add SYS_ADMIN --device /dev/fuse` | 后面 Lesson 4 要在容器内做 FUSE 挂载，普通容器权限做不到 |
| `--cap-add NET_ADMIN --cap-add NET_RAW` | 后面 Lesson 4/7/9 要在容器内跑 `tcpdump` 抓包 |
| 用裸 `docker network create` + `docker run`，不用 docker-compose | 这台机器没有 `docker compose` 插件；更重要的是，一条条命令跑更容易讲清楚"每个参数是为了解决什么问题"，而 compose 文件会把这些决策隐藏起来 |

## 踩的一个坑（也是证据）

最初的 Dockerfile 用 `apt-get install -y --no-install-recommends glusterfs-server glusterfs-client`，装完后容器里 **没有 `gluster` 命令**：

```
$ docker exec node1 gluster --version
OCI runtime exec failed: exec failed: unable to start container process:
exec: "gluster": executable file not found in $PATH
```

排查过程：

```
$ docker exec node1 dpkg -L glusterfs-server | grep -i bin
/usr/sbin/gluster-eventsapi
/usr/sbin/glusterd
/usr/sbin/glustereventsd
```

`glusterfs-server` 包里只有 `glusterd`（管理进程），并没有 `gluster`（命令行客户端）。原因：在 Debian/Ubuntu 的打包方式里，CLI 工具被拆成了独立的 `glusterfs-cli` 包，正常 `apt install glusterfs-server` 会把它作为 recommends 自动带上，但 `--no-install-recommends` 把它跳过了。加上 `glusterfs-cli` 后问题解决：

```
$ docker exec node1 gluster --version
glusterfs 10.1
Repository revision: git://git.gluster.org/glusterfs.git
```

这个小插曲本身就是一个有用的知识点：**`glusterd`（管理面守护进程）和 `gluster`（管理面的命令行客户端）是两个独立的二进制**，这个分离在后面 Lesson 9（RPC）分析"控制面 vs 数据面"时会再次用到——先记住这个事实。

## 验证

```
$ docker ps --format "table {{.Names}}\t{{.Image}}\t{{.Status}}"
NAMES     IMAGE             STATUS
node3     glusterlab:10.1   Up 16 seconds
node2     glusterlab:10.1   Up 17 seconds
node1     glusterlab:10.1   Up 17 seconds

$ docker exec node1 ps aux | grep glusterd
root   1  2.2  0.2 265016 16764 ?  SLsl 03:28  0:00 /usr/sbin/glusterd -N --log-level INFO

$ docker exec node1 hostname -I
172.28.0.11

$ docker exec node1 ping -c 2 node2
64 bytes from node2.glusterlab-net (172.28.0.12): icmp_seq=1 ttl=64 time=1.64 ms
64 bytes from node2.glusterlab-net (172.28.0.12): icmp_seq=2 ttl=64 time=0.063 ms
```

三个节点：`glusterd` 都以 PID 1 在跑，互相之间三层网络（ping）和名字解析（DNS）都通。环境就绪。

原始命令输出保存在 [evidence/](evidence/)：
- `01-docker-ps.txt`
- `02-network-inspect.json`
- `03-glusterd-status.txt`
- `04-gluster-version.txt`
- `05-ip-addr.txt`

## 后续课程会用到的操作速查

```bash
# 进入某节点
docker exec -it node1 bash

# 从宿主机直接对某节点跑单条命令
docker exec node1 <command>

# 观察某节点日志
docker exec node1 tail -f /var/log/glusterfs/glusterd.log

# 停/起某节点（模拟节点下线/恢复，后面 Lesson 4/8 会用）
docker stop node2 && docker start node2
```

下一步：[Lesson 1 - 认识 Peer](lesson01-peer/README.md)
