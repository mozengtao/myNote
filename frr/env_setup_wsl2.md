# FRR 学习环境搭建指南（WSL2）

本文档介绍在 Windows WSL2 环境下搭建 FRR（Free Range Routing）学习实验环境的三种方案。

---

## 目录

- [前置准备](#前置准备)
- [方案一：Docker 多容器拓扑（推荐）](#方案一docker-多容器拓扑推荐)
- [方案二：Linux Network Namespace（无需 Docker）](#方案二linux-network-namespace无需-docker)
- [方案三：WSL2 直装 FRR（单节点学习）](#方案三wsl2-直装-frr单节点学习)
- [FRR 基础操作速查](#frr-基础操作速查)
- [实验一：OSPF 两路由器互联](#实验一ospf-两路由器互联)
- [实验二：BGP 对等体建立](#实验二bgp-对等体建立)
- [实验三：三节点 OSPF + BGP 混合组网](#实验三三节点-ospf--bgp-混合组网)
- [故障排查命令](#故障排查命令)
- [清理环境](#清理环境)

---

## 前置准备

### 1. 确认 WSL2 版本

```bash
uname -r
# 应该看到类似 5.15.x 或 6.x.x-microsoft-standard-WSL2
```

### 2. 启动 Docker 守护进程

WSL2 中 Docker 不会自动启动，需要手动启动：

```bash
# 启动 Docker
sudo service docker start

# 验证
docker info | head -10

# 如果要开机自启（可选），在 ~/.bashrc 中添加：
# [ -z "$(service docker status 2>/dev/null | grep running)" ] && sudo service docker start > /dev/null 2>&1
```

### 3. 安装 Docker Compose 插件

```bash
# 安装 docker-compose-plugin
sudo apt update
sudo apt install docker-compose-plugin -y

# 验证
docker compose version
```

### 4. 拉取 FRR 镜像

```bash
docker pull quay.io/frrouting/frr:latest

# 验证
docker images | grep frr
```

---

## 方案一：Docker 多容器拓扑（推荐）

这是最实用的方案——每个 Docker 容器模拟一台路由器，通过 Docker network 互联。

### 目标拓扑

```
    10.1.1.0/24         10.2.2.0/24
R1 ──────────── R2 ──────────── R3
.1             .2  .2           .3
```

### 创建项目目录

```bash
mkdir -p ~/frr-lab/{r1,r2,r3}
cd ~/frr-lab
```

### 创建 FRR 守护进程配置

每个路由器都需要一个 `daemons` 文件来控制启用哪些协议守护进程：

```bash
# 通用 daemons 文件（启用 zebra、ospfd、bgpd）
cat > daemons <<'EOF'
zebra=yes
bgpd=yes
ospfd=yes
ospf6d=no
ripd=no
ripngd=no
isisd=no
pimd=no
ldpd=no
nhrpd=no
eigrpd=no
babeld=no
sharpd=no
pbrd=no
bfdd=no
fabricd=no
vrrpd=no

vtysh_enable=yes
zebra_options="  -A 127.0.0.1 -s 90000000"
bgpd_options="   -A 127.0.0.1"
ospfd_options="  -A 127.0.0.1"
EOF

# 复制到每个路由器目录
cp daemons r1/daemons
cp daemons r2/daemons
cp daemons r3/daemons
```

### R1 的 FRR 配置

```bash
cat > r1/frr.conf <<'EOF'
frr version 10.0
frr defaults traditional
hostname R1
log syslog informational
!
interface eth1
 ip address 10.1.1.1/24
exit
!
router ospf
 ospf router-id 1.1.1.1
 network 10.1.1.0/24 area 0
exit
!
line vty
!
EOF
```

### R2 的 FRR 配置

```bash
cat > r2/frr.conf <<'EOF'
frr version 10.0
frr defaults traditional
hostname R2
log syslog informational
!
interface eth1
 ip address 10.1.1.2/24
exit
!
interface eth2
 ip address 10.2.2.2/24
exit
!
router ospf
 ospf router-id 2.2.2.2
 network 10.1.1.0/24 area 0
 network 10.2.2.0/24 area 0
exit
!
line vty
!
EOF
```

### R3 的 FRR 配置

```bash
cat > r3/frr.conf <<'EOF'
frr version 10.0
frr defaults traditional
hostname R3
log syslog informational
!
interface eth1
 ip address 10.2.2.3/24
exit
!
router ospf
 ospf router-id 3.3.3.3
 network 10.2.2.0/24 area 0
exit
!
line vty
!
EOF
```

### docker-compose.yml

```bash
cat > docker-compose.yml <<'YAML'
services:
  r1:
    image: quay.io/frrouting/frr:latest
    container_name: r1
    hostname: R1
    privileged: true
    volumes:
      - ./r1/frr.conf:/etc/frr/frr.conf
      - ./r1/daemons:/etc/frr/daemons
    networks:
      link_r1_r2:
        ipv4_address: 10.1.1.1
    cap_add:
      - NET_ADMIN
      - SYS_ADMIN
    sysctls:
      - net.ipv4.ip_forward=1

  r2:
    image: quay.io/frrouting/frr:latest
    container_name: r2
    hostname: R2
    privileged: true
    volumes:
      - ./r2/frr.conf:/etc/frr/frr.conf
      - ./r2/daemons:/etc/frr/daemons
    networks:
      link_r1_r2:
        ipv4_address: 10.1.1.2
      link_r2_r3:
        ipv4_address: 10.2.2.2
    cap_add:
      - NET_ADMIN
      - SYS_ADMIN
    sysctls:
      - net.ipv4.ip_forward=1

  r3:
    image: quay.io/frrouting/frr:latest
    container_name: r3
    hostname: R3
    privileged: true
    volumes:
      - ./r3/frr.conf:/etc/frr/frr.conf
      - ./r3/daemons:/etc/frr/daemons
    networks:
      link_r2_r3:
        ipv4_address: 10.2.2.3
    cap_add:
      - NET_ADMIN
      - SYS_ADMIN
    sysctls:
      - net.ipv4.ip_forward=1

networks:
  link_r1_r2:
    driver: bridge
    ipam:
      config:
        - subnet: 10.1.1.0/24
  link_r2_r3:
    driver: bridge
    ipam:
      config:
        - subnet: 10.2.2.0/24
YAML
```

### 启动与使用

```bash
# 启动所有路由器
cd ~/frr-lab
docker compose up -d

# 查看状态
docker compose ps

# 进入 R1 的 FRR CLI
docker exec -it r1 vtysh

# 进入 R2 的 FRR CLI
docker exec -it r2 vtysh

# 进入 R3 的 FRR CLI
docker exec -it r3 vtysh
```

---

## 方案二：Linux Network Namespace（无需 Docker）

使用内核的 network namespace 功能模拟多台路由器，轻量且直接。

### 前提：安装 FRR

```bash
# 添加 FRR 官方源
curl -s https://deb.frrouting.org/frr/keys.gpg | sudo tee /usr/share/keyrings/frrouting.gpg > /dev/null
FRRVER="frr-stable"
echo "deb [signed-by=/usr/share/keyrings/frrouting.gpg] https://deb.frrouting.org/frr $(lsb_release -s -c) $FRRVER" | \
  sudo tee /etc/apt/sources.list.d/frr.list
sudo apt update
sudo apt install frr frr-pythontools -y
```

### 创建拓扑脚本

```bash
cat > ~/frr-lab/setup_ns.sh <<'BASH'
#!/bin/bash
set -e

# 清理旧环境
cleanup() {
    for ns in R1 R2; do
        sudo ip netns del "$ns" 2>/dev/null || true
    done
    sudo ip link del veth-r1 2>/dev/null || true
}

cleanup

echo "=== 创建 namespace ==="
sudo ip netns add R1
sudo ip netns add R2

echo "=== 创建 veth pair 连接 R1 <-> R2 ==="
sudo ip link add veth-r1 type veth peer name veth-r2

# 将 veth 端口分配到各 namespace
sudo ip link set veth-r1 netns R1
sudo ip link set veth-r2 netns R2

echo "=== 配置 R1 ==="
sudo ip netns exec R1 ip addr add 10.0.0.1/24 dev veth-r1
sudo ip netns exec R1 ip link set veth-r1 up
sudo ip netns exec R1 ip link set lo up
sudo ip netns exec R1 sysctl -w net.ipv4.ip_forward=1

echo "=== 配置 R2 ==="
sudo ip netns exec R2 ip addr add 10.0.0.2/24 dev veth-r2
sudo ip netns exec R2 ip link set veth-r2 up
sudo ip netns exec R2 ip link set lo up
sudo ip netns exec R2 sysctl -w net.ipv4.ip_forward=1

echo "=== 验证连通性 ==="
sudo ip netns exec R1 ping -c 2 10.0.0.2

echo "=== 拓扑就绪 ==="
echo "R1 (10.0.0.1) <---veth---> R2 (10.0.0.2)"
echo ""
echo "在 R1 中启动 FRR:"
echo "  sudo ip netns exec R1 /usr/lib/frr/zebra -d -N R1"
echo "  sudo ip netns exec R1 /usr/lib/frr/ospfd -d -N R1"
echo "  sudo ip netns exec R1 vtysh -N R1"
echo ""
echo "在 R2 中启动 FRR:"
echo "  sudo ip netns exec R2 /usr/lib/frr/zebra -d -N R2"
echo "  sudo ip netns exec R2 /usr/lib/frr/ospfd -d -N R2"
echo "  sudo ip netns exec R2 vtysh -N R2"
BASH

chmod +x ~/frr-lab/setup_ns.sh
```

### 使用方法

```bash
# 创建拓扑
sudo ~/frr-lab/setup_ns.sh

# 在 R1 namespace 中启动 FRR 并进入 CLI
sudo ip netns exec R1 /usr/lib/frr/zebra -d -N R1
sudo ip netns exec R1 /usr/lib/frr/ospfd -d -N R1
sudo ip netns exec R1 vtysh -N R1

# 在 R2 namespace 中启动 FRR 并进入 CLI（另开终端）
sudo ip netns exec R2 /usr/lib/frr/zebra -d -N R2
sudo ip netns exec R2 /usr/lib/frr/ospfd -d -N R2
sudo ip netns exec R2 vtysh -N R2
```

---

## 方案三：WSL2 直装 FRR（单节点学习）

最简方案，适合学习 FRR CLI 和基本配置语法，但无法做多路由器实验。

```bash
# 安装 FRR（同方案二的安装步骤）
curl -s https://deb.frrouting.org/frr/keys.gpg | sudo tee /usr/share/keyrings/frrouting.gpg > /dev/null
FRRVER="frr-stable"
echo "deb [signed-by=/usr/share/keyrings/frrouting.gpg] https://deb.frrouting.org/frr $(lsb_release -s -c) $FRRVER" | \
  sudo tee /etc/apt/sources.list.d/frr.list
sudo apt update
sudo apt install frr frr-pythontools -y

# 启用需要的守护进程
sudo sed -i 's/ospfd=no/ospfd=yes/' /etc/frr/daemons
sudo sed -i 's/bgpd=no/bgpd=yes/' /etc/frr/daemons

# 启动 FRR
sudo service frr start

# 进入 CLI
sudo vtysh
```

---

## FRR 基础操作速查

### vtysh CLI 模式

```
用户模式          R1>               只读，show 命令
特权模式          R1#               show/debug/clear 命令
全局配置模式      R1(config)#       全局配置
接口配置模式      R1(config-if)#    接口配置
路由器配置模式    R1(config-router)# 路由协议配置
```

### 常用命令

```
# 进入/退出模式
enable                        进入特权模式
configure terminal            进入全局配置
exit / end                    退出当前/所有配置模式

# 查看信息
show ip route                 查看路由表
show ip ospf neighbor         OSPF 邻居状态
show ip ospf database         OSPF 链路状态数据库
show ip bgp summary           BGP 邻居摘要
show ip bgp                   BGP 路由表
show interface brief          接口摘要
show running-config           当前运行配置

# 保存配置
write memory                  保存配置到文件（持久化）
copy running-config startup-config  同上
```

---

## 实验一：OSPF 两路由器互联

使用方案一的 Docker 环境（只用 R1 和 R2）。

### 验证步骤

```bash
# 进入 R1
docker exec -it r1 vtysh

R1# show ip ospf neighbor
# 应该看到 R2 (2.2.2.2) 状态为 Full

R1# show ip route
# 应该看到通过 OSPF 学到的 10.2.2.0/24 路由

R1# show ip ospf database
# 查看 LSDB（链路状态数据库）
```

### 动手实验：断开链路观察收敛

```bash
# 在宿主机上断开 R2 的 eth1 接口
docker exec r2 ip link set eth1 down

# 在 R1 上观察 OSPF 邻居消失
docker exec -it r1 vtysh -c "show ip ospf neighbor"

# 恢复
docker exec r2 ip link set eth1 up

# 观察邻居重新建立
docker exec -it r1 vtysh -c "show ip ospf neighbor"
```

---

## 实验二：BGP 对等体建立

在 Docker 环境中，修改 R1 和 R2 的配置来建立 eBGP 对等。

### R1 BGP 配置

```bash
docker exec -it r1 vtysh

R1# configure terminal
R1(config)# router bgp 65001
R1(config-router)# bgp router-id 1.1.1.1
R1(config-router)# neighbor 10.1.1.2 remote-as 65002
R1(config-router)# address-family ipv4 unicast
R1(config-router-af)# network 10.1.1.0/24
R1(config-router-af)# end
R1# write memory
```

### R2 BGP 配置

```bash
docker exec -it r2 vtysh

R2# configure terminal
R2(config)# router bgp 65002
R2(config-router)# bgp router-id 2.2.2.2
R2(config-router)# neighbor 10.1.1.1 remote-as 65001
R2(config-router)# address-family ipv4 unicast
R2(config-router-af)# network 10.2.2.0/24
R2(config-router-af)# end
R2# write memory
```

### 验证

```bash
# 在 R1 上查看 BGP 邻居
R1# show ip bgp summary

# 应该看到:
# Neighbor     V  AS   MsgRcvd  MsgSent  Up/Down   State/PfxRcd
# 10.1.1.2     4  65002  ...      ...     00:xx:xx  1

# 查看 BGP 路由
R1# show ip bgp

# 查看路由表（应该有 BGP 和 OSPF 路由）
R1# show ip route
```

---

## 实验三：三节点 OSPF + BGP 混合组网

```
      AS 65001              AS 65002              AS 65003
  ┌───────────┐  eBGP   ┌───────────┐  eBGP   ┌───────────┐
  │    R1     ├─────────┤    R2     ├─────────┤    R3     │
  │ 10.1.1.1  │10.1.1.0 │ 10.1.1.2  │10.2.2.0 │ 10.2.2.3  │
  │           │ /24     │ 10.2.2.2  │ /24     │           │
  │ OSPF+BGP  │         │ OSPF+BGP  │         │ OSPF+BGP  │
  └───────────┘         └───────────┘         └───────────┘
```

### R1 完整配置

```
configure terminal
!
router ospf
 ospf router-id 1.1.1.1
 network 10.1.1.0/24 area 0
exit
!
router bgp 65001
 bgp router-id 1.1.1.1
 neighbor 10.1.1.2 remote-as 65002
 address-family ipv4 unicast
  network 10.1.1.0/24
  redistribute ospf
 exit-address-family
exit
!
end
write memory
```

### R2 完整配置

```
configure terminal
!
router ospf
 ospf router-id 2.2.2.2
 network 10.1.1.0/24 area 0
 network 10.2.2.0/24 area 0
exit
!
router bgp 65002
 bgp router-id 2.2.2.2
 neighbor 10.1.1.1 remote-as 65001
 neighbor 10.2.2.3 remote-as 65003
 address-family ipv4 unicast
  network 10.1.1.0/24
  network 10.2.2.0/24
  redistribute ospf
 exit-address-family
exit
!
end
write memory
```

### R3 完整配置

```
configure terminal
!
router ospf
 ospf router-id 3.3.3.3
 network 10.2.2.0/24 area 0
exit
!
router bgp 65003
 bgp router-id 3.3.3.3
 neighbor 10.2.2.2 remote-as 65002
 address-family ipv4 unicast
  network 10.2.2.0/24
  redistribute ospf
 exit-address-family
exit
!
end
write memory
```

### 验证全网互通

```bash
# 从 R1 ping R3（跨两跳）
docker exec r1 ping -c 3 10.2.2.3

# 在 R1 上查看完整路由表
docker exec -it r1 vtysh -c "show ip route"

# 在 R2 上查看 BGP 路由（应有两个邻居）
docker exec -it r2 vtysh -c "show ip bgp summary"

# 追踪路径
docker exec r1 traceroute 10.2.2.3
```

---

## 故障排查命令

```bash
# --- 在 vtysh 中 ---

# OSPF 排查
show ip ospf neighbor              # 邻居是否 Full？
show ip ospf interface             # 接口是否参与 OSPF？
show ip ospf database              # LSDB 是否完整？
debug ospf event                   # 开启 OSPF 事件调试
debug ospf packet all              # 开启 OSPF 报文调试

# BGP 排查
show ip bgp summary                # 邻居状态？PfxRcd 是否 > 0？
show ip bgp neighbors 10.1.1.2    # 详细邻居信息
show ip bgp neighbors 10.1.1.2 advertised-routes   # 发出了哪些路由？
show ip bgp neighbors 10.1.1.2 received-routes     # 收到了哪些路由？
debug bgp updates                  # BGP 更新调试
debug bgp neighbor-events          # BGP 邻居事件调试

# 通用
show ip route                      # 最终路由表
show interface brief               # 接口状态
show log                           # 日志

# --- 在容器 shell 中 ---
docker exec r1 ip route            # 查看 Linux 内核路由表
docker exec r1 ip addr             # 查看 IP 地址
docker exec r1 ip link             # 查看接口状态
```

---

## 清理环境

### Docker 方案清理

```bash
cd ~/frr-lab
docker compose down        # 停止并删除容器和网络
docker compose down -v     # 同上，并删除卷
```

### Namespace 方案清理

```bash
sudo ip netns del R1
sudo ip netns del R2
sudo ip link del veth-r1 2>/dev/null
```

---

## 推荐学习路径

1. **第一步**：方案一搭好环境，跑通实验一（OSPF），理解邻居建立、路由学习
2. **第二步**：实验二（BGP），理解 AS、eBGP/iBGP 区别
3. **第三步**：实验三，理解路由重分发（redistribute）、多协议协作
4. **进阶**：
   - 添加 route-map 做路由过滤和策略
   - 配置 BFD（快速故障检测）
   - 尝试 EVPN-VXLAN（需要更复杂拓扑）
   - 了解 Prefix List、Community、AS-Path Filter

## 参考资源

- FRR 官方文档: https://docs.frrouting.org/
- FRR Docker 镜像: https://quay.io/repository/frrouting/frr
- FRR GitHub: https://github.com/FRRouting/frr
