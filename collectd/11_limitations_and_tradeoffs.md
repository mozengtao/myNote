# collectd Limitations and Design Trade-offs

## Part 11: When NOT to Use collectd

### 11.1 Scaling Limits

```
collectd Scaling Characteristics:

Single Instance:
+---------------+     +----------------+     +---------------+
| Read Threads  | --> | Write Queue    | --> | Write Threads |
| (default: 5)  |     | (unbounded*)   |     | (default: 5)  |
+---------------+     +----------------+     +---------------+

* WriteQueueLimitHigh/Low can bound the queue

Bottlenecks:
1. CPU-bound read plugins: Limited by ReadThreads
2. Network-bound writes: Limited by WriteThreads  
3. Very high metric cardinality: Memory for cache
4. Very short intervals: Scheduling overhead
```

| Dimension | Practical Limit | Bottleneck |
|-----------|-----------------|------------|
| Metrics per second | ~100,000 | Write queue/threads |
| Unique time series | ~1,000,000 | Cache memory |
| Minimum interval | ~0.1 seconds | Scheduler overhead |
| Plugins per instance | ~100 | Configuration complexity |
| Backend connections | ~10-20 | Write thread contention |

**扩展性限制（中文解释）：**

collectd 的设计适合**中等规模**的指标采集：

1. **高吞吐场景**：如果需要每秒处理百万级指标，需要多实例或其他方案
2. **高基数场景**：过多唯一时间序列会导致缓存内存膨胀
3. **亚秒级采集**：调度开销在极短间隔下变得显著
4. **大量后端**：写线程共享会导致延迟增加

---

### 11.2 When Prometheus Exporters Are Better

```
collectd Model:                  Prometheus Exporter Model:
+--------+      +--------+       +--------+      +-----------+
| Server | ---> | Writer | --->  | Server | <--- | Prometheus|
+--------+      +--------+       +--------+      +-----------+
   Push model                       Pull model

Prometheus is better when:
1. Targets are ephemeral (containers, serverless)
2. Service discovery is needed
3. PromQL queries are required
4. Existing Prometheus ecosystem
```

| Scenario | Better Choice | Reason |
|----------|---------------|--------|
| Kubernetes monitoring | Prometheus | Service discovery, pod labels |
| Ephemeral containers | Prometheus | /metrics endpoint survives restarts |
| High-cardinality labels | Prometheus | Label-based data model |
| Long-term storage | collectd → RRD | Fixed storage, automatic aggregation |
| Legacy systems | collectd | Mature plugins, no agent required |
| Network equipment | collectd | SNMP plugin, passive collection |

**何时选择 Prometheus（中文解释）：**

Prometheus 更适合的场景：
1. **动态基础设施**：容器、Kubernetes、云函数
2. **服务发现**：自动发现新目标
3. **需要 PromQL**：复杂查询、告警规则
4. **生态系统**：Grafana、Alertmanager 集成

collectd 更适合的场景：
1. **静态基础设施**：物理服务器、网络设备
2. **长期存储**：RRD 自动聚合
3. **低资源开销**：C 语言，极低内存
4. **被动采集**：SNMP、网络流

---

### 11.3 Embedded Agent Limitations

```
collectd as Embedded Agent:
+-----------------------+
| Application Process   |
|  +------------------+ |
|  | libcollectd.so   | |   <-- NOT a typical use case!
|  +------------------+ |
|  | read callbacks   | |   <-- Runs in app threads
|  | write callbacks  | |   <-- May block app
|  +------------------+ |
+-----------------------+

Problems:
1. No library API (designed as daemon)
2. Global state conflicts
3. Signal handler interference
4. Threading model mismatch
5. No isolation (crash affects app)
```

**Why collectd is NOT designed for embedding**:
- Single global configuration
- Global callback lists
- Shared thread pools
- No namespace isolation
- Hardcoded logging

**Alternative approaches**:
1. Run collectd as sidecar process
2. Use application-native libraries (OpenTelemetry)
3. Expose `/metrics` endpoint for Prometheus

**作为嵌入式 Agent 的限制（中文解释）：**

collectd 不适合嵌入应用程序的原因：
1. **全局状态**：插件列表、缓存都是全局的
2. **信号处理**：可能与应用冲突
3. **线程模型**：固定的读写线程池
4. **无隔离**：插件崩溃会影响整个进程

替代方案：
- 作为 sidecar 容器运行
- 使用 OpenTelemetry SDK
- 实现自己的 `/metrics` 端点

---

### 11.4 High-Cardinality Metrics

```
Fixed-Size Identifier Problem:
+---------------------------+
| host[64]/plugin[64]-plugin_instance[64]/type[64]-type_instance[64]
+---------------------------+

Cannot express:
- Dynamic labels (user_id, request_id)
- High-cardinality dimensions
- Unbounded enumeration values

Example that doesn't fit:
  http_request_duration{method="GET", path="/api/users/12345", status="200"}
                                              ^^^^^^^^
                                        Unique per user!
```

| Cardinality | Impact |
|-------------|--------|
| 1,000 series | OK |
| 10,000 series | Noticeable memory |
| 100,000 series | Cache becomes bottleneck |
| 1,000,000+ series | Not recommended |

**Mitigation**:
- Aggregate at source (histogram buckets)
- Use metadata instead of type_instance
- Filter out high-cardinality dimensions

**高基数指标限制（中文解释）：**

collectd 的标识符模型不适合高基数场景：
- 固定长度字段（64 字节）
- 五个固定维度
- 无标签支持

问题示例：
- 每用户指标
- 每请求追踪
- 每连接统计

解决方案：
- 在源头聚合
- 使用直方图桶
- 使用 Prometheus（支持任意标签）

---

### 11.5 Real-Time Requirements

```
collectd Timing:
+--------+  Interval  +--------+  Queue  +--------+
| Read   | ---------> | Queue  | ------> | Write  |
+--------+            +--------+         +--------+
  10s interval          variable           variable
                        latency            latency

Total latency = Interval + Queue wait + Write time
              ≈ 10s + 0-5s + 0-1s
              ≈ 10-16 seconds typical

NOT suitable for:
- Sub-second alerting
- Real-time dashboards
- Trading systems
- Industrial control
```

**Real-time alternatives**:
1. Stream processing (Kafka, Flink)
2. In-process instrumentation
3. Direct push to time-series DB

**实时性限制（中文解释）：**

collectd 不适合实时场景：
- 间隔采样（默认 10 秒）
- 队列缓冲（可变延迟）
- 批量写入（额外延迟）

总延迟公式：`采集间隔 + 队列等待 + 写入时间`

不适合的场景：
- 亚秒级告警
- 实时仪表盘
- 交易系统
- 工业控制

---

### 11.6 Configuration Model Limitations

```
Static Configuration:
+----------------+                 +----------------+
| collectd.conf  |  --- reload --> | collectd.conf' |
+----------------+                 +----------------+
      |                                   |
      | SIGHUP                            | restart
      v                                   v
  [Partial reload]                  [Full restart]

What CAN'T change at runtime:
- Adding/removing plugins
- Changing intervals
- Modifying filter chains
- Updating types.db

What CAN change at runtime:
- Threshold values (via thresholds.conf reload)
- Some plugin-specific options (plugin-dependent)
```

**Configuration anti-patterns in collectd**:
1. ❌ Dynamic metric discovery (needs restart)
2. ❌ Auto-configuration from service registry
3. ❌ Hot-swapping backends
4. ❌ Per-request routing rules

**配置模型限制（中文解释）：**

collectd 采用**静态配置**模型：
- 启动时加载配置
- 大多数更改需要重启
- 不支持动态发现

限制的场景：
- 自动发现新服务
- 根据服务注册表配置
- 运行时切换后端
- 动态路由规则

---

### 11.7 Decision Matrix

```
Should you use collectd?

                      YES                          NO
                       |                            |
    +------------------+------------------+         |
    |                                     |         |
+---v---+  +-------+  +-------+  +-------+    +----v----+
|Legacy |  |Static |  |Long-  |  |Low    |    |Dynamic  |
|Infra  |  |Config |  |term   |  |Resource|    |Infra   |
+-------+  +-------+  |Storage|  |Budget |    +---------+
                      +-------+  +-------+         |
                                                   |
                                    +----+---------+----+
                                    |    |         |    |
                               +----v-+  |    +----v-+  |
                               |K8s   |  |    |High  |  |
                               |Pods  |  |    |Card. |  |
                               +------+  |    +------+  |
                                         |              |
                                    +----v----+    +----v----+
                                    |Serverless|   |Real-time|
                                    +----------+   +---------+
```

| Criterion | Use collectd | Use Alternative |
|-----------|--------------|-----------------|
| Infrastructure | Static servers | Dynamic containers |
| Cardinality | Low (<100K series) | High (>1M series) |
| Latency | Seconds OK | Sub-second required |
| Configuration | Static | Dynamic discovery |
| Storage | RRD (fixed retention) | Prometheus/InfluxDB |
| Ecosystem | Mature plugins | Cloud-native |

**决策矩阵（中文解释）：**

选择 collectd 的条件：
- ✅ 静态基础设施（物理服务器）
- ✅ 低基数指标
- ✅ 秒级延迟可接受
- ✅ 静态配置
- ✅ 需要 RRD 长期存储
- ✅ 使用成熟的 SNMP/系统插件

选择其他方案的条件：
- ❌ Kubernetes/容器
- ❌ 高基数（每用户指标）
- ❌ 亚秒级实时性
- ❌ 动态服务发现
- ❌ 云原生生态

---

### 11.8 Summary: Design Trade-offs

| Trade-off | collectd Choice | Consequence |
|-----------|-----------------|-------------|
| Memory vs. Flexibility | Fixed-size buffers | Can't handle arbitrary labels |
| Simplicity vs. Dynamism | Static config | No hot-reload |
| Efficiency vs. Generality | C, no GC | Plugin bugs can crash daemon |
| Sampling vs. Events | Interval-based | Not suitable for event streams |
| Push vs. Pull | Push (mostly) | Harder service discovery |
| Daemon vs. Library | Daemon | Can't embed in apps |

**设计权衡总结（中文解释）：**

collectd 的每个设计选择都有代价：

1. **内存 vs 灵活性**：固定大小换来内存效率
2. **简单 vs 动态**：静态配置换来确定性
3. **效率 vs 通用性**：C 语言换来性能
4. **采样 vs 事件**：间隔采样换来可预测负载
5. **推送 vs 拉取**：主动推送换来控制权
6. **守护进程 vs 库**：进程隔离换来资源独立

---

### 11.9 Learning Outcomes

After reading this section, you should be able to:

- [ ] Identify scaling limits of collectd
- [ ] Explain when Prometheus is a better choice
- [ ] Describe why collectd can't be embedded
- [ ] Understand high-cardinality limitations
- [ ] Recognize real-time requirement mismatches
- [ ] Make informed decisions about when to use collectd
