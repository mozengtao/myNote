# collectd Architecture Overview

## Part 1: High-Level Architecture and Design Philosophy

### 1.1 What is collectd?

collectd is a **time-series data processing pipeline** implemented as a Unix daemon. It follows a classic producer-consumer architecture where:

- **Read plugins** produce metrics (data acquisition)
- **Core daemon** dispatches, caches, and routes data
- **Write plugins** consume metrics (data persistence/forwarding)

```
+------------------+     +------------------+     +------------------+
|   Read Plugins   | --> |   Core Daemon    | --> |  Write Plugins   |
+------------------+     +------------------+     +------------------+
|  cpu.c           |     | plugin.c         |     | write_rrd.c      |
|  memory.c        |     | utils_cache.c    |     | write_prometheus |
|  load.c          |     | filter_chain.c   |     | network.c        |
|  disk.c          |     | dispatch logic   |     | write_graphite.c |
+------------------+     +------------------+     +------------------+
       ^                        ^                        |
       |                        |                        v
   [Data Sources]          [Configuration]         [Backends]
   /proc, sysfs,           collectd.conf           RRD, InfluxDB,
   APIs, etc.              types.db                Prometheus, etc.
```

**架构概述（中文解释）：**
collectd 是一个基于拉取模式的时间序列数据处理管道。其核心设计理念是将数据采集（读插件）、数据处理（核心守护进程）和数据存储（写插件）三者解耦。读插件负责从各种系统资源（如 /proc 文件系统、系统 API 等）采集指标数据；核心守护进程负责调度、缓存和路由这些数据；写插件则负责将数据持久化到各种后端存储（如 RRD、Prometheus、InfluxDB 等）。这种架构使得 collectd 具有高度的可扩展性和灵活性。

---

### 1.2 Why Pull-Based Scheduling?

collectd uses a **pull-based model** where the daemon actively schedules and calls read plugins at configured intervals. This differs from push-based systems where data sources push metrics.

```
Pull-Based (collectd):                 Push-Based (alternative):
+--------+    timer     +--------+     +--------+           +--------+
| Daemon | -----------> | Plugin |     | Source | --------> | Server |
+--------+   "read now" +--------+     +--------+  "here's  +--------+
    ^                       |                       data"
    |                       |
    +--- controls timing ---+
    
Advantages:                            Disadvantages:
- Consistent sampling                  - Source controls timing
- Rate limiting built-in               - Unpredictable load
- Backoff on failures                  - No natural rate limit
- Predictable resource use             - Complex buffering needed
```

**为什么采用拉取模式（中文解释）：**
collectd 采用拉取模式的核心原因是为了实现**一致的采样间隔**。在监控场景中，时间序列数据的价值很大程度上取决于采样的规律性。拉取模式让守护进程完全控制采样时机，可以：
1. 确保固定的采样间隔（对 RRD 等时间序列数据库至关重要）
2. 内置速率限制，防止系统过载
3. 实现智能退避策略（当插件失败时自动增加间隔）
4. 提供可预测的资源使用模式

---

### 1.3 Core Design Decisions

#### 1.3.1 Central Daemon vs. Library

collectd is a **daemon**, not a library. This is intentional:

| Daemon Approach | Library Approach |
|-----------------|------------------|
| Single config point | Config per-application |
| Cross-app aggregation | Siloed metrics |
| Dedicated scheduling | App-managed timing |
| Unified type system | Ad-hoc schemas |
| Resource isolation | Shared memory space |

**为什么是守护进程而非库（中文解释）：**
collectd 选择守护进程模式而非库模式，主要考虑：
1. **统一配置**：所有监控配置集中管理
2. **跨应用聚合**：可以汇总来自不同应用的指标
3. **独立调度**：不依赖应用程序的生命周期
4. **资源隔离**：监控代码与业务代码分离，避免相互影响

#### 1.3.2 What collectd Solves

1. **Uniform metric collection** across diverse sources
2. **Time-aligned sampling** for consistent time series
3. **Type-safe value semantics** (counter, gauge, derive, absolute)
4. **Pluggable backends** without code changes
5. **Filter/transform pipeline** for data manipulation

#### 1.3.3 What collectd Does NOT Solve

1. **Real-time alerting** (use Nagios, Prometheus Alertmanager)
2. **Log aggregation** (use ELK, Loki)
3. **Distributed tracing** (use Jaeger, Zipkin)
4. **High-cardinality metrics** (limited by fixed-length identifier fields)
5. **Dynamic service discovery** (static configuration model)

---

### 1.4 Key Source Files Reference

| File | Purpose |
|------|---------|
| `src/daemon/collectd.c` | Main entry point, event loop |
| `src/daemon/plugin.c` | Plugin registration, scheduling, dispatch |
| `src/daemon/plugin.h` | Core data structures and APIs |
| `src/daemon/configfile.c` | Configuration parsing |
| `src/daemon/utils_cache.c` | Value cache (rate calculation) |
| `src/daemon/filter_chain.c` | Match/target filter chains |
| `src/daemon/types_list.c` | types.db parsing |

---

### 1.5 Learning Outcomes

After reading this section, you should be able to:

- [ ] Explain why collectd uses a pull-based model
- [ ] Describe the read → dispatch → write pipeline
- [ ] Articulate trade-offs of daemon vs. library approach
- [ ] Identify which problems collectd solves vs. doesn't solve
- [ ] Navigate to core source files for each subsystem
