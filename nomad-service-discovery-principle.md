# Nomad Service Discovery 工作原理

## 核心概念

Nomad Service Discovery 是 HashiCorp Nomad 内置的服务注册与发现机制。它解决的核心问题是：**在容器化环境中，服务的 IP 和端口是动态分配的，消费者如何找到生产者？**

```
传统方式 (硬编码):
  Grafana 配置: prometheus_url = "192.168.1.100:9090"
  问题: Prometheus 重启后端口变了，地址也可能变

Nomad SD 方式 (动态发现):
  Grafana 配置: prometheus_url = nomadService("prom-http")  --> 自动解析
  结果: 无论 Prometheus 在哪运行、端口是什么，都能自动找到
```

---

## 整体架构

```
+------------------------------------------------------------------+
|                      Nomad Server (Leader)                       |
|                                                                  |
|  +----------------------------------------------------------+    |
|  |              Service Registry (内存)                     |    |
|  |                                                          |    |
|  |  "prom-http"       -> [{IP:64.110.0.213, Port:20773}]    |    |
|  |  "grafana"         -> [{IP:64.110.0.50,  Port:31245}]    |    |
|  |  "prom-node-exporter" -> [{IP:64.110.0.213, Port:28801}, |    |
|  |                          {IP:64.110.0.214, Port:28802},  |    |
|  |                          {IP:64.110.0.215, Port:28803}]  |    |
|  |  "prom-collectd"   -> [{IP:..., Port:...}, ...]          |    |
|  |  "kafka-bootstrap-server" -> [{...}, {...}, {...}]       |    |
|  |  "loki-http"       -> [{IP:..., Port:...}]               |    |
|  |  ...                                                     |    |
|  +----------------------------------------------------------+    |
+------------------------------------------------------------------+
         ^                    ^                    |
         | 注册               | 注册               | 查询
         |                    |                    v
+--------+------- +  +--------+-------+  +--------+-------+
| Nomad Client 1  |  | Nomad Client 2 |  | Nomad Client 3 |
| (Node 1)        |  | (Node 2)       |  | (Node 3)       |
|                 |  |                |  |                |
| [node-exporter] |  | [node-exporter]|  | [node-exporter]|
| [prometheus]    |  | [vmc]          |  | [grafana]      |
| [kafka]         |  | [kafka]        |  | [kafka]        |
+-----------------+  +----------------+  +---------------+
```

---

## 工作流程：三个阶段

### 阶段 1：服务注册 (Registration)

当 Nomad 启动一个 job 的 task 时，如果 task 中定义了 `service` 块，Nomad 会自动将服务信息注册到 Service Registry。

**HCL 中的定义：**

```hcl
# node-exporter_container.nomad.hcl
service {
  provider = "nomad"          # 使用 Nomad 内置 SD（不是 Consul）
  name = "prom-node-exporter" # 服务名称（全局唯一标识）
  port = "exporter-data-port" # 引用 network 块中定义的端口
  address_mode = "host"       # 使用宿主机 IP（而非容器内部 IP）
}
```

**注册时发生了什么：**

```
1. Nomad 调度器决定在 Node-1 上运行 node-exporter
2. Docker 启动容器，Nomad 分配宿主机端口 28801 -> 容器 9100
3. Nomad Client 向 Nomad Server 报告:
   "prom-node-exporter" = {
     Address: "64.110.0.213"    # Node-1 的 IP
     Port: 28801                # 动态分配的宿主机端口
     Tags: []
     JobID: "node-exporter"
     AllocID: "abc123"
   }
4. Nomad Server 将此条目写入 Service Registry
```

由于 node-exporter 是 `type = "system"` job，每个节点都会运行一个实例，所以 Registry 中会有 N 条记录（N = 节点数）。

### 阶段 2：健康检查 (Health Checking)

注册的服务可以配置健康检查，不健康的实例会从查询结果中排除：

```hcl
# prometheus_container.nomad.hcl
service {
  provider = "nomad"
  name = "prom-http"
  check {
    type = "http"
    protocol = "https"
    tls_skip_verify = "true"
    path = "/-/healthy"       # Prometheus 的健康检查端点
    interval = "10s"          # 每 10 秒检查一次
    timeout = "3s"
  }
}
```

```hcl
# node-exporter_container.nomad.hcl
service {
  check {
    type = "tcp"              # TCP 端口可达性检查
    port = "exporter-data-port"
    interval = "10s"
    timeout = "3s"
    check_restart {
      limit = 3               # 连续 3 次失败则重启容器
      grace = "90s"            # 启动后 90 秒内不检查
    }
  }
}
```

健康检查类型：

| 类型 | 方式 | 适用场景 |
|------|------|---------|
| `tcp` | 尝试建立 TCP 连接 | 简单的端口存活检查 |
| `http` | 发送 HTTP GET 请求，检查返回码 | Web 服务、REST API |
| `script` | 在容器内执行脚本 | 复杂的自定义检查 |

### 阶段 3：服务发现 (Discovery)

其他服务通过 Nomad 的**模板引擎**查询 Service Registry，获得目标服务的地址和端口。

**模板引擎工作原理：**

Nomad job 中的 `template` 块使用 Go template 语法，其中 `nomadService` 函数是查询 Service Registry 的入口：

```hcl
template {
  data = <<EOH
  PROMETHEUS_URL={{ range nomadService "prom-http" }}{{ .Address }}:{{ .Port }}{{ end }}
  EOH
  destination = "local/config.env"
  change_mode = "restart"      # 地址变化时重启容器
}
```

**渲染过程：**

```
Go Template 输入:
  {{ range nomadService "prom-http" }}{{ .Address }}:{{ .Port }}{{ end }}

Nomad 查询 Service Registry:
  "prom-http" -> [{Address: "64.110.0.213", Port: 20773, Tags: ["https"]}]

渲染输出:
  PROMETHEUS_URL=64.110.0.213:20773
```

---

## `nomadService` 函数的三种调用形式

在 vCMTS 代码库中可以看到三种不同的用法：

### 形式 1：全局查询（返回所有实例）

```hcl
{{ range nomadService "prom-node-exporter" }}
  {{ .Address }}:{{ .Port }}
{{ end }}
```

返回所有注册了 `prom-node-exporter` 的实例。用于 Prometheus 发现所有 scrape 目标。

### 形式 2：带 AllocID 的本地查询（只返回同 alloc 的实例）

```hcl
{{ range nomadService 1 "${NOMAD_ALLOC_ID}" "webdav-service" }}
  WEBDAV_PORT={{ .Port }}
  WEBDAV_IP={{ .Address }}
{{ end }}
```

`nomadService 1 "${NOMAD_ALLOC_ID}" "name"` 表示只查询**同一 allocation** 内的服务。用于同一 job group 中多个 task 之间的通信。

### 形式 3：带 Tags 过滤

```hcl
{{- with nomadService "prom-http" -}}
  {{- with index . 0 -}}
    {{- range .Tags -}}
      {{- if (eq . "https") -}}
        {{- $promProtocol = "https" -}}
      {{- end -}}
    {{- end -}}
  {{- end -}}
{{- end }}
```

通过 `.Tags` 获取额外的元数据（如协议类型 `https`/`tls`）。

---

## 动态更新机制

Nomad 模板引擎会**持续监听** Service Registry 的变化：

```
时间 T0: Prometheus 运行在 Node-1:20773
         Grafana 模板渲染: url = https://64.110.0.213:20773

时间 T1: Node-1 故障，Nomad 将 Prometheus 重调度到 Node-2:21456
         Service Registry 更新: prom-http -> Node-2:21456

时间 T2: Nomad 检测到模板输入变化
         重新渲染: url = https://64.110.0.214:21456
         根据 change_mode 执行操作:
           - "restart": 重启 Grafana 容器（加载新配置）
           - "signal": 发送信号（如 SIGHUP）让进程重新加载
           - "script": 执行自定义脚本
```

vCMTS 中的实际例子：

```hcl
# prometheus_container.nomad.hcl - Prometheus 配置变化时热重载
template {
  data = file("./configs/prometheus.yml")
  destination = "local/config/prometheus.yml"
  change_mode = "script"
  change_script {
    command = "/etc/init.d/prometheus"
    args = ["reload"]                     # 不重启，只重载配置
  }
}
```

```hcl
# grafana_container.nomad.hcl - 数据源配置变化时重启
template {
  data = file("./configs/grafana.yml")
  change_mode = "restart"                 # 完全重启容器
  destination = "datasources/grafana.yml"
}
```

---

## vCMTS 中的服务注册全景

以下是从 `nomad-jobs/tmpl/docker/` 中收集到的所有已注册服务：

| 服务名 | Job | 用途 |
|--------|-----|------|
| `prom-node-exporter` | node-exporter | 节点指标采集 (linux_bond_*, linux_phys_*) |
| `prom-http` | prometheus | Prometheus 查询 API |
| `grafana` | grafana | Grafana Web UI |
| `prom-collectd` | vmc, ptp | collectd 指标 (VMC/PTP) |
| `loki-http` | loki | 日志查询 API |
| `loki-grpc` | loki | 日志 gRPC 接口 |
| `otlp-http` | (OTLP) | OpenTelemetry 日志收集 |
| `alert-http` | alertmanager | 告警管理 |
| `kafka-bootstrap-server` | kafka | Kafka 消息队列 |
| `kafka-bootstrap-server-tls` | kafka | Kafka TLS 端口 |
| `kafka-controller-quorum-voter` | kafka | Kafka KRaft 选举 |
| `kafka-exporter` | kafka | Kafka 指标导出 |
| `vmc-serf` | vmc | VMC 集群 gossip 协议 |
| `vmc-statemgr` | vmc | VMC 状态管理 |
| `dhcpv4-client-port` | vmc | DHCPv4 客户端 |
| `dhcpv6-client-port` | vmc | DHCPv6 客户端 |
| `dhcpv4-proxy` | router | DHCPv4 代理 |
| `dhcpv6-proxy` | router | DHCPv6 代理 |
| `bgp-grpc` | router | BGP gRPC 接口 |
| `router-grpc` | router | 路由器 gRPC |
| `fpm-grpc` | router | FPM gRPC |
| `prom-bgp` | router | BGP 指标导出 |
| `li-grpc` | li | 合法监听 gRPC |
| `prom-li` | li | 合法监听指标导出 |
| `pktcbl-grpc` | pktcbl | PacketCable gRPC |
| `prom-pktcbl` | pktcbl | PacketCable 指标导出 |
| `snmp-nsi` | snmp | SNMP NSI 端口 |
| `rq-netconff-port` | remote-query | NETCONF 端口 |
| `kepler-ossi-metrics` | remote-query | Kepler OSSI 指标 |
| `evc-restconf` | evc | NSO RESTCONF API |
| `evc-raft` | evc | EVC 高可用 Raft |
| `epmd` | epmd | Erlang 端口映射 |

---

## 与 Consul 的对比

Nomad 支持两种服务发现后端：

| 特性 | Nomad 内置 SD | Consul SD |
|------|--------------|-----------|
| 部署 | 零依赖，Nomad 自带 | 需要额外部署 Consul 集群 |
| 配置 | `provider = "nomad"` | `provider = "consul"` |
| 健康检查 | TCP / HTTP / Script | 更丰富（gRPC, TTL 等） |
| DNS 查询 | 不支持 | 支持 (service.consul) |
| KV 存储 | 不支持 | 支持 |
| 跨数据中心 | 不支持 | 支持 |
| 模板函数 | `nomadService` | `service` |
| 适用场景 | 单集群、简单场景 | 多集群、复杂服务网格 |

vCMTS 选择了 **Nomad 内置 SD**（所有 service 块都是 `provider = "nomad"`），因为整个系统运行在单个 Nomad 集群内，不需要 Consul 的额外复杂度。

---

## 数据流总结

```
+-------------------+     +-------------------+     +-------------------+
|    服务注册        |     |    Registry 存储   |     |    服务发现        |
+-------------------+     +-------------------+     +-------------------+
|                   |     |                   |     |                   |
| Job HCL 定义:     |     | Nomad Server 内存: |     | 模板引擎查询:      |
|   service {       |---->|   name -> [       |---->| nomadService(name)|
|     provider =    |     |     {IP, Port,    |     |   -> [{IP, Port}] |
|       "nomad"     |     |      Tags,        |     |                   |
|     name = "x"    |     |      JobID,       |     | 渲染为配置文件     |
|     port = "y"    |     |      AllocID,     |     | 注入到容器中       |
|     tags = [...]  |     |      Health}      |     |                   |
|     check {...}   |     |   ]               |     | change_mode:      |
|   }               |     |                   |     |   restart/script  |
+-------------------+     +-------------------+     +-------------------+
        ^                         |                         |
        |                         | 健康检查                 | 持续监听
        |                         v                         v
+-------------------+     +-------------------+     +------------------+
| 容器启动           |     | 不健康实例         |    | Registry 变化     |
| -> 动态端口分配     |     | -> 自动从查询结果  |    | -> 重新渲染模板    |
| -> 向 Server 注册  |     |    中移除          |    | -> 触发更新操作    |
+-------------------+     +-------------------+     +-------------------+
```

本质上，Nomad Service Discovery 就是一个**内置的服务注册表 + 模板引擎**的组合，通过声明式配置实现了服务间的自动发现和动态连接，消除了所有硬编码的地址依赖。
