# Node Exporter 原理、作用与心智模型

---

# 1. 什么是 Node Exporter

Node Exporter 是 Prometheus 官方提供的主机监控 Exporter。

官方定义：

> Exporter for machine metrics.

其核心作用是：

```text
将 Linux 主机状态
转换为
Prometheus Metrics
```

本质上：

```text
Linux Kernel
      │
      ▼
/proc
/sys
system calls
      │
      ▼
node_exporter
      │
      ▼
Prometheus Metrics
```

Node Exporter 自身并不产生监控数据。

它只是：

```text
采集 Linux Kernel 已经存在的数据
        +
转换为 Prometheus 格式
```

因此：

```text
node_exporter
=
Linux → Prometheus Adapter
```

---

# 2. Node Exporter 在监控体系中的位置

## Prometheus 监控架构

```text
                     ┌──────────────┐
                     │   Grafana    │
                     └──────┬───────┘
                            │
                            ▼
                    ┌──────────────┐
                    │ Prometheus   │
                    └──────┬───────┘
                           │
                           │ HTTP Pull
                           │
     ┌─────────────────────┼─────────────────────┐
     │                     │                     │
     ▼                     ▼                     ▼

 node_exporter      node_exporter       node_exporter

    host-a             host-b              host-c
```

职责划分：

| 组件 | 职责 |
|--------|--------|
| node_exporter | 采集主机指标 |
| Prometheus | 抓取并存储指标 |
| Alertmanager | 告警 |
| Grafana | 展示 |

---

# 3. 为什么需要 Node Exporter

Linux 的监控数据分散在多个地方：

```text
/proc
/sys
/dev
cgroup
kernel statistics
```

例如：

```text
CPU
    └── /proc/stat

Memory
    └── /proc/meminfo

Network
    └── /proc/net/dev

Filesystem
    └── statfs()

Disk
    └── /proc/diskstats
```

如果没有 Node Exporter：

```text
自己写程序

    read()

        ↓

    parse()

        ↓

 metric format

        ↓

 HTTP server
```

Node Exporter 已经实现了这些功能。

---

# 4. Node Exporter 的核心工作流程

## 整体流程

```text
Prometheus
      │
      │ GET /metrics
      ▼
node_exporter
      │
      ├── CPU Collector
      ├── Memory Collector
      ├── Network Collector
      ├── Filesystem Collector
      └── ...
      │
      ▼
读取 Linux 信息
      │
      ▼
转换为 Metrics
      │
      ▼
返回 HTTP Response
```

---

# 5. Exporter 的心智模型

理解 Exporter 最简单的方法：

```text
Exporter
=
协议转换器
```

例如：

```text
Linux Statistics
       │
       ▼
 node_exporter
       │
       ▼
 Prometheus Metrics
```

类似网络协议栈中的：

```text
TCP
 │
 ▼
IP
 │
 ▼
Ethernet
```

只是这里转换的是：

```text
Linux 数据格式
        ↓
Prometheus 数据格式
```

---

# 6. Collector 架构

Node Exporter 采用插件化设计。

每个 Collector 负责一个领域。

```text
node_exporter
       │
       ├── cpu collector
       ├── mem collector
       ├── net collector
       ├── filesystem collector
       ├── loadavg collector
       ├── diskstats collector
       ├── uname collector
       └── ...
```

可以理解为：

```text
Collector
=
一个指标采集插件
```

---

# 7. Collector 与 Linux 数据源对应关系

```text
                     node_exporter
                           │
      ┌────────────┬───────┼────────────┬────────────┐
      │            │       │            │            │
      ▼            ▼       ▼            ▼            ▼

     CPU        Memory   Network     Filesystem    Disk

      │            │       │            │            │
      ▼            ▼       ▼            ▼            ▼

 /proc/stat
 /proc/meminfo
 /proc/net/dev
 statfs()
 /proc/diskstats
```

---

# 8. 一次 Scrape 的全过程

Prometheus 周期性执行：

```http
GET http://host:9100/metrics
```

随后：

```text
Prometheus
      │
      ▼
HTTP Request
      │
      ▼
node_exporter
      │
      ▼
collect()
```

然后所有 Collector 开始工作：

```text
              scrape request
                       │
                       ▼

              node_exporter
                       │
       ┌───────────────┼───────────────┐
       │               │               │
       ▼               ▼               ▼

 CPU Collector   MEM Collector   NET Collector

       │               │               │
       ▼               ▼               ▼

 /proc/stat     /proc/meminfo   /proc/net/dev
```

最后生成：

```text
node_cpu_seconds_total ...

node_memory_MemAvailable_bytes ...

node_network_receive_bytes_total ...
```

返回给 Prometheus。

---

# 9. Pull 模型心智模式

很多人误认为：

```text
node_exporter
主动上报数据
```

实际上：

```text
Prometheus
主动抓取
```

即：

```text
           Pull Model

Prometheus
      │
      │ GET /metrics
      ▼
node_exporter
```

而不是：

```text
node_exporter
      │
      ▼
Prometheus
```

---

## 时间线

```text
每15秒

Prometheus
      │
      ▼
GET /metrics

      │
      ▼

node_exporter

      │
      ▼

实时采集

      │
      ▼

返回 Metrics
```

---

# 10. Linux 数据来源

Node Exporter 本质上是 Linux 数据读取器。

主要来源：

| 数据源 | 用途 |
|----------|----------|
| /proc/stat | CPU |
| /proc/meminfo | Memory |
| /proc/loadavg | Load |
| /proc/net/dev | Network |
| /proc/diskstats | Disk |
| /proc/mounts | Filesystem |
| /sys/class/net | NIC |
| /sys/block | Block Device |
| uname() | Kernel Version |

---

# 11. CPU Collector

## 数据来源

```text
/proc/stat
```

示例：

```text
cpu  100 200 300 400

cpu0 50 100 150 200

cpu1 50 100 150 200
```

---

## 导出指标

```text
node_cpu_seconds_total
```

例如：

```text
node_cpu_seconds_total{
    cpu="0",
    mode="user"
}
```

---

## CPU 心智模型

```text
/proc/stat
      │
      ▼
CPU累计时间
      │
      ▼
node_cpu_seconds_total
      │
      ▼
rate()
      │
      ▼
CPU利用率
```

注意：

Node Exporter 不计算利用率。

只提供：

```text
累计计数器(Counter)
```

Prometheus 负责计算：

```promql
rate(node_cpu_seconds_total[5m])
```

---

# 12. Memory Collector

## 数据来源

```text
/proc/meminfo
```

示例：

```text
MemTotal

MemFree

Buffers

Cached

MemAvailable
```

---

## 导出指标

```text
node_memory_MemTotal_bytes

node_memory_MemAvailable_bytes

node_memory_Cached_bytes
```

---

## Memory 心智模型

```text
Linux Memory
      │
      ▼
/proc/meminfo
      │
      ▼
node_exporter
      │
      ▼
node_memory_*
      │
      ▼
PromQL
      │
      ▼
Memory Usage
```

---

## 计算内存利用率

```promql
100 *
(
1 -
node_memory_MemAvailable_bytes
/
node_memory_MemTotal_bytes
)
```

---

# 13. Network Collector

对于网络工程师来说，这是最重要的部分之一。

---

## 数据来源

```text
/proc/net/dev
```

例如：

```text
eth0:

    rx_bytes
    rx_packets
    tx_bytes
    tx_packets
```

---

## 导出指标

```text
node_network_receive_bytes_total

node_network_transmit_bytes_total

node_network_receive_packets_total

node_network_transmit_packets_total
```

---

## 网络心智模型

```text
NIC Driver
      │
      ▼
Kernel Statistics
      │
      ▼
/proc/net/dev
      │
      ▼
node_exporter
      │
      ▼
node_network_*
```

---

## 计算带宽

```promql
rate(
    node_network_receive_bytes_total{
        device="eth0"
    }[5m]
)
```

结果：

```text
eth0 RX Bytes/s
```

若要换算为 bit/s：

```promql
rate(node_network_receive_bytes_total[5m]) * 8
```

---

# 14. Filesystem Collector

## 数据来源

```text
/proc/mounts

statfs()
```

---

## 导出指标

```text
node_filesystem_size_bytes

node_filesystem_free_bytes

node_filesystem_avail_bytes
```

---

## Filesystem 心智模型

```text
Filesystem
      │
      ▼
statfs()
      │
      ▼
node_exporter
      │
      ▼
node_filesystem_*
```

---

## 计算磁盘利用率

```promql
100 *
(
1 -
node_filesystem_avail_bytes
/
node_filesystem_size_bytes
)
```

---

# 15. 从 Linux 网络协议栈角度理解

对于熟悉 Linux 网络协议栈的人：

```text
                    Kernel
                       │
                       ▼
              Statistics Layer
                       │
                       ▼
               /proc/net/dev
                       │
                       ▼
                node_exporter
                       │
                       ▼
               Prometheus Metric
                       │
                       ▼
                   Grafana
```

其本质类似于：

```text
ip -s link

ethtool -S

ss -s

netstat -s

sar

vmstat

iostat
```

只是：

```text
人类可读
        ↓
机器可读

一次查询
        ↓
时间序列数据库

命令行输出
        ↓
PromQL 查询
```

---

# 16. Node Exporter 最重要的心智模型

## 心智模型一

```text
node_exporter
=
Linux Metrics Adapter
```

---

## 心智模型二

```text
node_exporter
=
HTTP版 cat /proc/*
```

即：

```text
cat /proc/stat
cat /proc/meminfo
cat /proc/net/dev
df
free
uptime
```

统一包装成：

```text
http://host:9100/metrics
```

---

## 心智模型三

```text
node_exporter
不负责

× 存储
× 告警
× 展示
× 聚合分析
```

只负责：

```text
采集
    +
转换
    +
暴露
```

---

## 心智模型四（推荐牢记）

```text
                    Linux Kernel
                           │
                           ▼
             /proc  /sys  statfs()
                           │
                           ▼
                    node_exporter
                           │
                           ▼
                Prometheus Metrics
                           │
                           ▼
                     Prometheus
                           │
                           ▼
                        Grafana
```

一句话总结：

Node Exporter 就是 Linux Kernel 与 Prometheus 之间的“协议转换层（Adapter）”，负责把分散在 `/proc`、`/sys`、系统调用中的主机状态信息，实时转换成标准 Prometheus Metrics 供 Prometheus 抓取和存储。