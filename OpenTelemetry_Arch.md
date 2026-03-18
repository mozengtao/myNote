# OpenTelemetry 架构与核心概念（系统化总结）

## 1. OpenTelemetry 概述

**OpenTelemetry（OTel）** 是一个开源的可观测性（Observability）框架，用于统一采集、处理和导出应用程序的遥测数据（Telemetry Data）。

OTel 的核心目标：

- 提供 **统一的可观测性标准**
- 支持 **Trace、Metrics、Logs 三类数据**
- 实现 **Vendor-neutral（厂商无关）**
- 提供 **跨语言 SDK**
- 构建 **统一 Telemetry 数据管道**

OTel 由以下两个项目合并而来：

- **OpenTracing**（分布式追踪）
- **OpenCensus**（Metrics + Trace）

并由 CNCF（Cloud Native Computing Foundation）维护。

---

## 2. OpenTelemetry 总体架构

OpenTelemetry 的整体系统架构如下图所示，数据从应用层逐层向下流动，最终到达可视化分析层：

```
Application                    <-- 业务应用（数据源头）
    |
    | Instrumentation           <-- 埋点层（自动/手动）
    v
OpenTelemetry API              <-- 标准接口层（Tracer / Meter / Logger）
    |
    v
OpenTelemetry SDK              <-- SDK 实现层（采集、采样、批处理）
    |
    | OTLP Protocol             <-- 标准传输协议（gRPC / HTTP）
    v
OpenTelemetry Collector        <-- 独立 Pipeline 服务（接收、处理、导出）
    |
    v
Observability Backend          <-- 存储后端（Jaeger / Prometheus / Loki）
    |
    v
Visualization / Analysis       <-- 可视化面板（Grafana 等）
```

完整架构层次（简化视图）：

| 层级 | 名称 | 职责 |
|------|------|------|
| 1 | Application Layer | 业务代码运行的地方 |
| 2 | Instrumentation Layer | 自动/手动埋点采集遥测数据 |
| 3 | OpenTelemetry API | 定义标准编程接口 |
| 4 | OpenTelemetry SDK | API 的具体实现，负责采集与导出 |
| 5 | OpenTelemetry Collector | 独立中转服务，处理并路由遥测数据 |
| 6 | Observability Backend | 后端存储与查询引擎 |

---

## 3. 完整系统架构图

> 下图展示了从应用代码到可视化的完整数据流。每一层之间通过标准协议通信，层与层之间解耦。

```
+---------------------------------------------------------------+
| Application Layer                                             |
|                                                               |
|  Business Code                                                |
|  +----------------------------------------------------------+ |
|  | HTTP Handler / RPC / DB / Business Logic                 | |
|  +----------------------------------------------------------+ |
|       |                          |                            |
|       | instrumentation          |                            |
|       v                          v                            |
|  Manual Instrumentation    Auto Instrumentation               |
|  (开发者手动调用 API)       (框架自动注入探针)                   |
+-------------------------------+-------------------------------+
                                |
                                v
+---------------------------------------------------------------+
| OpenTelemetry API                                             |
|                                                               |
|  Tracing API        Metrics API        Logs API               |
|  +------------+     +------------+     +------------+         |
|  |   Tracer   |     |   Meter    |     |   Logger   |         |
|  +-----+------+     +-----+------+     +-----+------+         |
|        |                  |                   |               |
|        v                  v                   v               |
|    Span API          Metric API           Log API             |
|  (创建/管理Span)    (记录指标数据)      (记录日志事件)           |
+-------------------------------+-------------------------------+
                                |
                                v
+---------------------------------------------------------------+
| OpenTelemetry SDK                                             |
|                                                               |
|  Telemetry Providers                                          |
|                                                               |
|  +------------------------------------------------------+     |
|  | Tracer Provider                                       |    |
|  |   Sampler -> SpanProcessor -> BatchProcessor          |    |
|  |                                    |                  |    |
|  |                                    v                  |    |
|  |                                Exporter               |    |
|  +------------------------------------------------------+     |
|                                                               |
|  +------------------------------------------------------+     |
|  | Meter Provider                                        |    |
|  |   Aggregator -> MetricReader -> MetricExporter        |    |
|  +------------------------------------------------------+     |
|                                                               |
|  +------------------------------------------------------+     |
|  | Logger Provider                                       |    |
|  |   LogProcessor -> LogExporter                         |    |
|  +------------------------------------------------------+     |
+-------------------------------+-------------------------------+
                                |
                                | OTLP (gRPC / HTTP)
                                v
+---------------------------------------------------------------+
| OpenTelemetry Collector                                       |
|                                                               |
|  Telemetry Processing Pipeline                                |
|                                                               |
|  +-----------+     +-------------+     +-------------+        |
|  | Receiver  | --> | Processor   | --> | Exporter    |        |
|  |           |     |             |     |             |        |
|  | OTLP      |     | batch       |     | Jaeger      |        |
|  | Jaeger    |     | sampling    |     | Prometheus  |        |
|  | Zipkin    |     | transform   |     | OTLP        |        |
|  | Prometheus|     | attributes  |     | Kafka       |        |
|  +-----------+     +-------------+     +-------------+        |
+-------------------------------+-------------------------------+
                                |
                                v
+---------------------------------------------------------------+
| Observability Backends                                        |
|                                                               |
|  Trace Backend      Metric Backend      Log Backend           |
|    Jaeger             Prometheus           Loki               |
|                                                               |
|                    Visualization                              |
|                      Grafana                                  |
+---------------------------------------------------------------+
```

**图例说明：**

- **Receiver**：数据接收器，支持多种协议接入（OTLP、Jaeger、Zipkin、Prometheus）
- **Processor**：数据处理器，可做批量聚合（batch）、采样（sampling）、属性变换（transform/attributes）
- **Exporter**：数据导出器，将处理后的数据发送到指定后端

---

## 4. OpenTelemetry 的三层核心架构

OpenTelemetry 可以拆分为三个核心层：

| 层级 | 说明 |
|------|------|
| **API Layer** | 定义标准埋点接口，不包含实现 |
| **SDK Layer** | API 的运行时实现，负责采集、处理和导出 |
| **Collector Layer** | 独立部署的遥测数据中转与处理服务 |

---

## 5. API 层（Instrumentation Interface）

API 层定义 **标准埋点接口**。

特点：

- 只定义接口，不负责实现
- 不依赖具体 backend
- 应用代码只依赖 API，不依赖 SDK 具体实现

主要 API：

| API | 作用 | 说明 |
|-----|------|------|
| Tracer | 创建 Trace | 用于创建和管理 Span，追踪请求链路 |
| Meter | 记录 Metrics | 用于记录 Counter、Gauge、Histogram 等指标 |
| Logger | 记录 Logs | 用于记录结构化日志事件 |

示例（Go 语言）：

```go
span := tracer.Start(ctx, "http_request")
defer span.End()
```

> API 的角色类比：OTel API 相当于可观测性领域的 **编程接口（Observability Programming Interface）**，类似操作系统的系统调用。

---

## 6. SDK 层（Telemetry Runtime）

SDK 是 API 的具体实现，负责遥测数据的完整生命周期管理。

主要职责：**collect → process → export**

SDK 内部结构：

```
API
 |
 v
SDK
 +-- Tracer Provider    <-- 管理 Tracer 实例
 +-- Meter Provider     <-- 管理 Meter 实例
 +-- Logger Provider    <-- 管理 Logger 实例
 |
 +-- Sampler            <-- 决定是否采样（降低开销）
 +-- Span Processor     <-- 处理生成的 Span 数据
 +-- Exporter           <-- 将数据导出到 Collector 或 Backend
```

---

## 7. SDK 内部架构

> 下图详细展示了 SDK 中 **Trace 数据** 从创建到导出的完整路径。

```
              OpenTelemetry SDK

               +--------------+
               |   Resource   |    <-- 标识服务的元数据
               | service.name |        (服务名、实例 ID 等)
               | service.id   |
               +------+-------+
                      |
                      v
              +---------------+
              | TracerProvider |    <-- 管理所有 Tracer 实例
              +------+--------+
                     |
                     v
                  Tracer           <-- 由 Provider 创建
                     |
                     v
                   Span            <-- Tracer.Start() 创建的追踪单元
                     |
                     v
             +-----------------+
             |  SpanProcessor  |   <-- 对 Span 进行处理
             |                 |
             | SimpleProcessor |       SimpleProcessor: 逐条导出（适合开发调试）
             | BatchProcessor  |       BatchProcessor:  批量导出（适合生产环境）
             +--------+--------+
                      |
                      v
                  Exporter         <-- 负责序列化并发送数据
                      |
                      v
                    OTLP           <-- 标准传输协议
```

**关键组件说明：**

- **Resource**：附加到所有遥测数据上的服务元信息（如 `service.name`），用于在后端区分不同服务
- **TracerProvider**：Tracer 工厂，负责配置采样策略和处理管道
- **SpanProcessor**：
  - `SimpleProcessor` — 同步逐条导出，延迟低但吞吐有限，适合开发调试
  - `BatchProcessor` — 异步批量导出，减少网络开销，**生产环境推荐**
- **Exporter**：将 Span 数据序列化为 OTLP 格式并发送至 Collector 或直接发送到后端

---

## 8. 三种 Telemetry 数据模型

OTel 定义三种核心遥测数据：

```
Telemetry
 +-- Trace      <-- 请求链路追踪
 +-- Metrics    <-- 时间序列指标
 +-- Logs       <-- 离散事件日志
```

### 8.1 Trace（分布式追踪）

Trace 表示 **请求跨服务的完整调用链**。一个 Trace 由多个 Span 组成，Span 之间通过 ParentSpanID 形成树形结构。

数据结构：

```
Trace
 +-- TraceID                   <-- 全局唯一的追踪 ID（串联整个请求链）
 +-- Span
      +-- SpanID              <-- 当前 Span 的唯一 ID
      +-- ParentSpanID        <-- 父 Span ID（构成调用树）
      +-- Name                <-- 操作名称（如 "GET /api/users"）
      +-- StartTime           <-- 开始时间
      +-- EndTime             <-- 结束时间
      +-- Attributes          <-- 键值对属性（如 http.method, http.status_code）
      +-- Events              <-- Span 内的离散事件（如异常、日志）
      +-- Status              <-- 状态码（OK / ERROR / UNSET）
```

调用链示例（每个节点对应一个 Span）：

```
HTTP Request
     |
     v
API Gateway          Span 1 (root span)
     |
     v
Service A            Span 2 (child of Span 1)
     |
     v
Service B            Span 3 (child of Span 2)
     |
     v
Database             Span 4 (child of Span 3)
```

### 8.2 Metrics（指标）

Metrics 是 **时间序列数据**，用于衡量系统运行状态。

常见类型：

| 类型 | 说明 | 典型用途 |
|------|------|----------|
| **Counter** | 单调递增计数器 | 请求总数、错误总数 |
| **Gauge** | 可增可减的当前值 | CPU 使用率、内存占用、连接数 |
| **Histogram** | 值的分布统计 | 请求延迟分布、响应体大小分布 |

示例指标：

```
http_request_total           <-- Counter: HTTP 请求总数
cpu_usage                    <-- Gauge: CPU 使用率
request_latency              <-- Histogram: 请求延迟分布
```

### 8.3 Logs（日志）

Logs 是 **离散事件记录**，每条日志包含时间戳、级别、消息和可选的追踪上下文。

结构：

| 字段 | 说明 |
|------|------|
| `timestamp` | 事件发生时间 |
| `severity` | 日志级别（INFO / WARN / ERROR 等） |
| `message` | 日志内容 |
| `attributes` | 附加键值对属性 |
| `trace_id` | 关联的 Trace ID |
| `span_id` | 关联的 Span ID |

> **关键特性**：Logs 可以通过 `trace_id` 和 `span_id` 与 Trace 关联，实现 **Trace-Log 联动查询**。这意味着可以从一条 Trace 直接跳转到相关日志，或从一条错误日志反向定位到完整请求链路。

三者的关联关系：

```
Trace  <--- trace_id ---> Logs
  |                         |
  +--- 时间维度聚合 --->  Metrics
```

---

## 9. OpenTelemetry Collector

Collector 是一个 **独立运行的 Telemetry Pipeline 服务**，部署在应用和后端之间。

数据流向：

```
Application --> Collector --> Backend
```

Collector 的核心优势：

| 优势 | 说明 |
|------|------|
| 减少应用负担 | 应用只需将数据发给 Collector，不需要直接对接多个后端 |
| 支持多后端 | 一份数据可同时导出到 Jaeger、Prometheus、Loki 等 |
| 统一出口 | 所有服务的遥测数据汇聚到统一入口 |
| 数据处理 | 支持采样、过滤、属性变换等中间处理逻辑 |

---

## 10. Collector Pipeline 架构

Collector 内部使用 **Pipeline 结构**：`Receiver → Processor → Exporter`

```
           OpenTelemetry Collector

              Telemetry Pipeline

           +--------------------+
           |      Receiver      |    <-- 数据入口，接收各协议数据
           |--------------------|
           | OTLP               |        标准 OTel 协议
           | Jaeger             |        Jaeger 原生协议
           | Zipkin             |        Zipkin 协议
           | Prometheus         |        Prometheus 拉取模式
           +---------+----------+
                     |
                     v
           +--------------------+
           |      Processor     |    <-- 数据处理，中间逻辑
           |--------------------|
           | batch              |        批量聚合，减少网络调用
           | sampling           |        尾部采样，降低数据量
           | filter             |        过滤不需要的数据
           | attributes         |        添加/修改/删除属性
           | transform          |        数据格式转换
           +---------+----------+
                     |
                     v
           +--------------------+
           |      Exporter      |    <-- 数据出口，发送到后端
           |--------------------|
           | jaeger             |        导出到 Jaeger
           | prometheus         |        导出到 Prometheus
           | kafka              |        导出到 Kafka（缓冲层）
           | otlp               |        导出到另一个 Collector 或后端
           +--------------------+
```

---

## 11. OTLP 协议

**OTLP（OpenTelemetry Protocol）** 是 OTel 定义的标准通信协议，用于在各组件之间传输遥测数据。

数据流向：

```
SDK ---OTLP---> Collector ---OTLP---> Backend
```

协议支持两种传输方式：

| 传输方式 | 说明 |
|----------|------|
| **OTLP/gRPC** | 基于 gRPC 的二进制传输，性能高，**推荐用于生产环境** |
| **OTLP/HTTP** | 基于 HTTP/JSON 或 HTTP/protobuf，兼容性好，适合防火墙受限环境 |

数据编码格式：**Protocol Buffers (protobuf)**

---

## 12. 自动埋点 vs 手动埋点

### 自动埋点（Auto Instrumentation）

**无需修改业务代码**，通过 Agent 或运行时注入自动捕获遥测数据。

自动捕获的范围：

- HTTP 请求/响应
- SQL 数据库查询
- RPC 远程调用
- 常见框架（Spring、Express、Django 等）

常见实现方式：

| 语言 | 方式 |
|------|------|
| Java | Java Agent（字节码注入） |
| Python | Python Agent（猴子补丁） |
| .NET | .NET Agent（DiagnosticListener） |
| Go | 需使用带 OTel 支持的库封装 |

### 手动埋点（Manual Instrumentation）

开发者 **主动调用 OTel API** 在业务代码中创建 Span 和记录数据。

```go
span := tracer.Start(ctx, "db.query")
defer span.End()
```

适用于：

- 自定义业务逻辑追踪
- 自定义 Metrics 记录
- 自动埋点无法覆盖的场景

> **最佳实践**：通常将自动埋点与手动埋点结合使用——自动埋点覆盖基础设施层（HTTP、DB、RPC），手动埋点补充业务层的关键路径。

---

## 13. 完整 Trace 生成流程

> 以下展示一次 HTTP 请求从进入应用到最终可视化的完整 Trace 生成过程。

```
HTTP Request
     |
     v
Auto Instrumentation (HTTP middleware)     <-- 1. HTTP 中间件自动拦截请求
     |
     v
Create Span                                <-- 2. 创建 root Span（携带 TraceID）
     |
     v
Tracer (API)                               <-- 3. 通过 API 层记录 Span 数据
     |
     v
TracerProvider (SDK)                       <-- 4. SDK 管理 Tracer，应用采样策略
     |
     v
SpanProcessor                             <-- 5. 处理 Span（Simple 或 Batch）
     |
     v
Batch Exporter                             <-- 6. 批量序列化并发送
     |
     v
OTLP Protocol                             <-- 7. 通过 OTLP 传输到 Collector
     |
     v
OpenTelemetry Collector                    <-- 8. Collector 接收、处理、路由
     |
     v
Trace Backend (Jaeger)                     <-- 9. 存入 Jaeger 后端
     |
     v
Visualization (Grafana)                    <-- 10. 在 Grafana 中查看火焰图/甘特图
```

---

## 14. 典型 Observability 技术栈

生产环境常见的完整可观测性架构：

```
Application
   |
   v
OpenTelemetry SDK
   |
   v
OpenTelemetry Collector
   |
   +-- Trace   --> Jaeger        <-- 分布式追踪存储与查询
   +-- Metric  --> Prometheus    <-- 时间序列指标存储与告警
   +-- Logs    --> Loki          <-- 日志聚合与查询
   |
   v
Grafana Visualization            <-- 统一可视化面板
```

**技术栈角色说明：**

| 组件 | 角色 | 说明 |
|------|------|------|
| **Jaeger** | Trace 后端 | 存储和查询分布式追踪数据，支持依赖图分析 |
| **Prometheus** | Metric 后端 | 时间序列数据库，支持 PromQL 查询和告警规则 |
| **Loki** | Log 后端 | 轻量日志聚合系统，仅索引标签不索引内容 |
| **Grafana** | 可视化 | 统一 Dashboard，支持同时查询 Trace/Metric/Log |

---

## 15. OpenTelemetry 的设计哲学

### 15.1 Vendor Neutral（厂商无关）

不绑定任何厂商或后端，通过标准协议（OTLP）实现数据流通。可随时切换后端（如从 Jaeger 迁移到 Tempo）而无需修改应用代码。

### 15.2 Unified Telemetry（统一遥测）

将三种核心遥测数据统一在同一个框架下：

```
Trace + Metrics + Logs = 完整的可观测性
```

三者共享相同的上下文（TraceID、SpanID），实现跨信号关联查询。

### 15.3 Pipeline Architecture（管道架构）

数据处理采用管道模式：

```
Receiver --> Processor --> Exporter
```

类似 Unix Pipeline 哲学——每个组件职责单一、可自由组合。

---

## 16. 工程师理解模型

可以将 OTel 类比为一个 **Telemetry Operating System（遥测操作系统）**：

| OTel 组件 | 操作系统类比 | 说明 |
|-----------|------------|------|
| Telemetry API | System Call | 应用程序调用的标准接口 |
| Telemetry Runtime (SDK) | Kernel | 负责数据采集、处理的运行时 |
| Telemetry Router (Collector) | Network Stack | 负责数据路由和转发 |
| Telemetry Storage (Backend) | File System | 负责数据持久化存储 |

```
Application
     |
     v
Telemetry API (system call)        <-- 应用层调用接口
     |
     v
Telemetry Runtime (SDK)            <-- 运行时处理
     |
     v
Telemetry Router (Collector)       <-- 路由转发
     |
     v
Telemetry Storage (Backend)        <-- 持久化存储
```

---

## 17. 一句话总结

> **OpenTelemetry = 标准化的 Observability 数据采集 + 处理 + 分发系统**

OTel 负责三件事，且仅负责这三件事：

| 职责 | 说明 |
|------|------|
| **Telemetry 数据采集** | 通过 API + SDK 在应用中埋点，采集 Trace/Metrics/Logs |
| **Telemetry 数据处理** | 通过 Collector Pipeline 进行采样、过滤、转换 |
| **Telemetry 数据分发** | 通过 Exporter 将数据发送到各种 Backend，与后端完全解耦 |
