# Section 8: Docker Networking

## 8.1 Default Bridge Network

```
DEFAULT BRIDGE NETWORK:
+==================================================================+
||                                                                ||
||  When you install Docker, it creates docker0 bridge:           ||
||                                                                ||
||  Host Machine                                                  ||
||  +----------------------------------------------------------+  ||
||  |                                                          |  ||
||  |  eth0: 192.168.1.100 (physical)                          |  ||
||  |                                                          |  ||
||  |  docker0: 172.17.0.1 (bridge)                            |  ||
||  |      |                                                   |  ||
||  |      +--- veth123 --- Container A (172.17.0.2)           |  ||
||  |      |                                                   |  ||
||  |      +--- veth456 --- Container B (172.17.0.3)           |  ||
||  |                                                          |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  Each container gets:                                          ||
||  - IP on 172.17.0.0/16 subnet                                  ||
||  - Virtual ethernet pair (veth) to bridge                      ||
||  - NAT for outbound traffic                                    ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

默认Bridge网络：安装Docker时创建docker0桥接。

主机有物理网卡eth0(192.168.1.100)和docker0桥接(172.17.0.1)。每个容器通过虚拟以太网对(veth)连接到桥接，获得172.17.0.0/16子网的IP。

每个容器获得：172.17.0.0/16子网上的IP、到桥接的虚拟以太网对(veth)、出站流量的NAT。

### How NAT Works

```
OUTBOUND TRAFFIC (Container to Internet):
+------------------------------------------------------------------+
|                                                                  |
|  Container A (172.17.0.2) wants to reach google.com              |
|                                                                  |
|  1. Packet: src=172.17.0.2, dst=google.com                       |
|        |                                                         |
|        v                                                         |
|  2. docker0 bridge receives packet                               |
|        |                                                         |
|        v                                                         |
|  3. iptables NAT rule translates:                                |
|     src=172.17.0.2 -> src=192.168.1.100 (host IP)                |
|        |                                                         |
|        v                                                         |
|  4. Packet sent to internet via eth0                             |
|        |                                                         |
|        v                                                         |
|  5. Response comes back to host                                  |
|        |                                                         |
|        v                                                         |
|  6. NAT translates back, routes to container                     |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

出站流量（容器到互联网）：容器A(172.17.0.2)想访问google.com。

1）数据包src=172.17.0.2, dst=google.com，2）docker0桥接收到数据包，3）iptables NAT规则转换src=172.17.0.2为src=192.168.1.100（主机IP），4）数据包通过eth0发送到互联网，5）响应返回主机，6）NAT反向转换，路由到容器。

### Port Mapping

```
PORT MAPPING (Inbound Traffic):
+==================================================================+
||                                                                ||
||  docker run -p 8080:80 nginx                                   ||
||                                                                ||
||  Creates iptables rule:                                        ||
||  Host:8080 -> Container:80                                     ||
||                                                                ||
||  External Traffic Flow:                                        ||
||  +----------------------------------------------------------+  ||
||  |  Internet                                                |  ||
||  |      |                                                   |  ||
||  |      v                                                   |  ||
||  |  Host eth0 (192.168.1.100:8080)                          |  ||
||  |      |                                                   |  ||
||  |      v iptables DNAT                                     |  ||
||  |  docker0 bridge                                          |  ||
||  |      |                                                   |  ||
||  |      v                                                   |  ||
||  |  Container (172.17.0.2:80)                               |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  PORT MAPPING OPTIONS:                                         ||
||  +----------------------------------------------------------+  ||
||  |  -p 8080:80        Host:8080 -> Container:80             |  ||
||  |  -p 80:80          Host:80 -> Container:80               |  ||
||  |  -p 8080:80/udp    UDP instead of TCP                    |  ||
||  |  -p 127.0.0.1:8080:80  Only localhost                    |  ||
||  |  -P                Auto-map all EXPOSEd ports            |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

端口映射（入站流量）：docker run -p 8080:80 nginx创建iptables规则：Host:8080 -> Container:80。

外部流量流：互联网 -> 主机eth0(192.168.1.100:8080) -> iptables DNAT -> docker0桥接 -> 容器(172.17.0.2:80)。

端口映射选项：-p 8080:80（Host:8080到Container:80）、-p 80:80、-p 8080:80/udp（UDP）、-p 127.0.0.1:8080:80（仅localhost）、-P（自动映射所有EXPOSE端口）。

---

## 8.2 Container-to-Container Communication

### Why localhost Doesn't Work

```
LOCALHOST MISUNDERSTANDING:
+==================================================================+
||                                                                ||
||  Container A                    Container B                    ||
||  +---------------------+        +---------------------+        ||
||  |                     |        |                     |        ||
||  | 127.0.0.1 = THIS    |        | 127.0.0.1 = THIS    |        ||
||  | container's lo      |        | container's lo      |        ||
||  |                     |        |                     |        ||
||  | 172.17.0.2 = eth0   |        | 172.17.0.3 = eth0   |        ||
||  +---------------------+        +---------------------+        ||
||                                                                ||
||  In Container A:                                               ||
||  curl http://localhost:80     -> Tries Container A's port 80   ||
||  curl http://127.0.0.1:80     -> Same, Container A's loopback  ||
||                                                                ||
||  To reach Container B from A:                                  ||
||  curl http://172.17.0.3:80    -> Works! Uses network namespace ||
||                                                                ||
||  BUT: IP addresses are dynamic! Container B might get          ||
||       different IP next time.                                  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

localhost误解：每个容器有自己的network namespace。

容器A的127.0.0.1是容器A的lo接口，容器B的127.0.0.1是容器B的lo接口。

在容器A中：curl http://localhost:80尝试容器A的端口80、curl http://127.0.0.1:80同样是容器A的回环。

从A到达容器B：curl http://172.17.0.3:80有效！使用网络namespace。

但是：IP地址是动态的！容器B下次可能获得不同IP。

### Docker DNS

```
USER-DEFINED NETWORKS WITH DNS:
+==================================================================+
||                                                                ||
||  # Create user-defined network                                 ||
||  docker network create mynet                                   ||
||                                                                ||
||  # Run containers on that network                              ||
||  docker run --name db --network mynet mysql                    ||
||  docker run --name web --network mynet nginx                   ||
||                                                                ||
||  USER-DEFINED NETWORK:                                         ||
||  +----------------------------------------------------------+  ||
||  |  mynet bridge (172.18.0.1)                               |  ||
||  |      |                                                   |  ||
||  |      +--- db (172.18.0.2)                                |  ||
||  |      |    DNS name: "db"                                 |  ||
||  |      |                                                   |  ||
||  |      +--- web (172.18.0.3)                               |  ||
||  |           DNS name: "web"                                |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
||  FROM WEB CONTAINER:                                           ||
||  +----------------------------------------------------------+  ||
||  |  ping db           -> Resolves to 172.18.0.2             |  ||
||  |  mysql -h db       -> Works! Uses DNS                    |  ||
||  |                                                          |  ||
||  |  Docker's embedded DNS (127.0.0.11) resolves             |  ||
||  |  container names to IPs                                  |  ||
||  +----------------------------------------------------------+  ||
||                                                                ||
+==================================================================+
```

**Chinese Explanation (中文说明):**

用户定义网络带DNS：docker network create mynet创建网络，docker run --name db --network mynet mysql和docker run --name web --network mynet nginx在该网络上运行容器。

用户定义网络：mynet桥接(172.18.0.1)，db(172.18.0.2)DNS名称"db"，web(172.18.0.3)DNS名称"web"。

从web容器：ping db解析为172.18.0.2、mysql -h db有效！使用DNS。Docker的嵌入式DNS(127.0.0.11)将容器名称解析为IP。

### Network Types

```
DOCKER NETWORK DRIVERS:
+------------------------------------------------------------------+
|                                                                  |
|  BRIDGE (default):                                               |
|  +------------------------------------------------------------+  |
|  |  - Isolated network on single host                        |  |
|  |  - Containers can communicate                             |  |
|  |  - NAT for external access                                |  |
|  |  - Use for: Most single-host scenarios                    |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  HOST:                                                           |
|  +------------------------------------------------------------+  |
|  |  docker run --network host nginx                          |  |
|  |  - Container uses host's network namespace                |  |
|  |  - No network isolation                                   |  |
|  |  - Best performance (no NAT overhead)                     |  |
|  |  - Use for: Performance-critical networking               |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  NONE:                                                           |
|  +------------------------------------------------------------+  |
|  |  docker run --network none myapp                          |  |
|  |  - No network access at all                               |  |
|  |  - Only loopback interface                                |  |
|  |  - Use for: Security-sensitive batch jobs                 |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  OVERLAY:                                                        |
|  +------------------------------------------------------------+  |
|  |  - Spans multiple Docker hosts                            |  |
|  |  - Used with Docker Swarm                                 |  |
|  |  - Use for: Multi-host deployments                        |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**Chinese Explanation (中文说明):**

Docker网络驱动：

BRIDGE（默认）：单主机上的隔离网络、容器可通信、外部访问NAT。用于：大多数单主机场景。

HOST：docker run --network host nginx，容器使用主机的网络namespace、无网络隔离、最佳性能（无NAT开销）。用于：性能关键的网络。

NONE：docker run --network none myapp，完全无网络访问、只有回环接口。用于：安全敏感的批处理作业。

OVERLAY：跨越多个Docker主机、与Docker Swarm使用。用于：多主机部署。

---

## Summary

```
DOCKER NETWORKING KEY INSIGHTS:
+==================================================================+
||                                                                ||
||  DEFAULT BRIDGE:                                               ||
||  - docker0 bridge at 172.17.0.1                                ||
||  - Containers get IPs in 172.17.0.0/16                         ||
||  - NAT for outbound, port mapping for inbound                  ||
||                                                                ||
||  CONTAINER COMMUNICATION:                                      ||
||  - localhost = container's own loopback                        ||
||  - Use container IPs or DNS names                              ||
||  - User-defined networks enable DNS by name                    ||
||                                                                ||
||  BEST PRACTICE:                                                ||
||  - Always use user-defined networks                            ||
||  - Reference containers by name, not IP                        ||
||  - Use --network host only when needed                         ||
||                                                                ||
+==================================================================+
```

**Next Section**: [Docker Volumes and Data Persistence](09-volumes.md)
