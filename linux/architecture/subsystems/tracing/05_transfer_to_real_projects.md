# TRANSFER｜应用到实际项目

## 1. 大型系统中的可观测性设计

```
OBSERVABILITY DESIGN IN LARGE SYSTEMS
+=============================================================================+
|                                                                              |
|  THE THREE PILLARS                                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                    OBSERVABILITY                          │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │     LOGS          METRICS          TRACES                 │   │    │ |
|  │  │  │     ┌───┐         ┌───┐           ┌───┐                   │   │    │ |
|  │  │  │     │   │         │   │           │   │                   │   │    │ |
|  │  │  │     │ W │         │ W │           │ W │                   │   │    │ |
|  │  │  │     │ H │         │ H │           │ H │                   │   │    │ |
|  │  │  │     │ A │         │ O │           │ E │                   │   │    │ |
|  │  │  │     │ T │         │ W │           │ R │                   │   │    │ |
|  │  │  │     │   │         │   │           │ E │                   │   │    │ |
|  │  │  │     │   │         │ M │           │   │                   │   │    │ |
|  │  │  │     │   │         │ U │           │   │                   │   │    │ |
|  │  │  │     │   │         │ C │           │   │                   │   │    │ |
|  │  │  │     │   │         │ H │           │   │                   │   │    │ |
|  │  │  │     └───┘         └───┘           └───┘                   │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Events with      Aggregated       Request flow           │   │    │ |
|  │  │  │  context          numbers          across services        │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Linux kernel teaches us:                                        │    │ |
|  │  │  • Logs = printk (expensive, persistent)                         │    │ |
|  │  │  • Metrics = perf counters (aggregated, cheap)                   │    │ |
|  │  │  • Traces = ftrace/BPF (detailed, on-demand)                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 1: ALWAYS-ON METRICS, ON-DEMAND TRACES                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Like kernel's perf counters + tracepoints                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  class RequestHandler {                                          │    │ |
|  │  │      // Always on: cheap counters                                │    │ |
|  │  │      static Counter requests_total;                              │    │ |
|  │  │      static Histogram latency_ms;                                │    │ |
|  │  │                                                                  │    │ |
|  │  │      // On-demand: detailed tracing                              │    │ |
|  │  │      static bool trace_enabled = false;  // toggle dynamically   │    │ |
|  │  │                                                                  │    │ |
|  │  │      Response handle(Request req) {                              │    │ |
|  │  │          auto start = now();                                     │    │ |
|  │  │          requests_total.inc();                                   │    │ |
|  │  │                                                                  │    │ |
|  │  │          // Like tracepoint: check if enabled                    │    │ |
|  │  │          if (unlikely(trace_enabled)) {                          │    │ |
|  │  │              trace_request_start(req);                           │    │ |
|  │  │          }                                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │          Response resp = process(req);                           │    │ |
|  │  │                                                                  │    │ |
|  │  │          latency_ms.observe(now() - start);                      │    │ |
|  │  │                                                                  │    │ |
|  │  │          if (unlikely(trace_enabled)) {                          │    │ |
|  │  │              trace_request_end(req, resp);                       │    │ |
|  │  │          }                                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │          return resp;                                            │    │ |
|  │  │      }                                                           │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 2: RING BUFFER FOR EVENT COLLECTION                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Like kernel's per-CPU ring buffer                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  class TraceBuffer {                                             │    │ |
|  │  │      // Fixed-size, pre-allocated                                │    │ |
|  │  │      alignas(64) Event buffer[BUFFER_SIZE];                      │    │ |
|  │  │      atomic<size_t> write_pos{0};                                │    │ |
|  │  │      size_t read_pos{0};                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  public:                                                         │    │ |
|  │  │      // Lock-free write (producer side)                          │    │ |
|  │  │      void write(Event e) {                                       │    │ |
|  │  │          size_t pos = write_pos.fetch_add(1, relaxed);           │    │ |
|  │  │          buffer[pos % BUFFER_SIZE] = e;  // may overwrite old    │    │ |
|  │  │      }                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Consumer reads (single reader)                           │    │ |
|  │  │      optional<Event> read() {                                    │    │ |
|  │  │          if (read_pos >= write_pos.load(acquire))                │    │ |
|  │  │              return nullopt;                                     │    │ |
|  │  │          return buffer[read_pos++ % BUFFER_SIZE];                │    │ |
|  │  │      }                                                           │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Per-thread buffers (like per-CPU)                            │    │ |
|  │  │  thread_local TraceBuffer local_buffer;                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 3: STRUCTURED EVENTS                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Like TRACE_EVENT macro - define format once                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Bad: ad-hoc strings                                          │    │ |
|  │  │  log("Request " + req.id + " took " + duration + "ms");          │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Good: structured events (like trace_event_call)              │    │ |
|  │  │  struct RequestEvent {                                           │    │ |
|  │  │      uint64_t timestamp;                                         │    │ |
|  │  │      uint32_t request_id;                                        │    │ |
|  │  │      uint32_t duration_us;                                       │    │ |
|  │  │      uint16_t status_code;                                       │    │ |
|  │  │      char endpoint[32];                                          │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Benefits:                                                    │    │ |
|  │  │  // • Fixed size (no allocation)                                 │    │ |
|  │  │  // • Binary format (fast to write)                              │    │ |
|  │  │  // • Schema known (easy to parse)                               │    │ |
|  │  │  // • Can be aggregated efficiently                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**大型系统中的可观测性设计**：

**三大支柱**：
- **日志**：带上下文的事件（WHAT）
- **指标**：聚合数字（HOW MUCH）
- **追踪**：跨服务请求流（WHERE）

Linux 内核教导我们：
- 日志 = printk（昂贵、持久）
- 指标 = perf counters（聚合、便宜）
- 追踪 = ftrace/BPF（详细、按需）

**模式 1：始终在线指标 + 按需追踪**
- 始终在线：便宜的计数器（requests_total、latency_ms）
- 按需：详细追踪（trace_enabled 动态切换）
- 像 tracepoint：检查是否启用

**模式 2：事件收集环形缓冲区**
- 像内核 per-CPU 环形缓冲区
- 固定大小，预分配
- 无锁写入（生产者侧）
- Per-thread 缓冲区

**模式 3：结构化事件**
- 像 TRACE_EVENT 宏 - 定义一次格式
- 坏：临时字符串
- 好：结构化事件（固定大小、二进制格式、模式已知）

---

## 2. 何时追踪值得代价

```
WHEN TRACING IS WORTH THE COST
+=============================================================================+
|                                                                              |
|  DECISION FRAMEWORK                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  USE TRACING WHEN:                                               │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✓ You can't reproduce issues locally                       │ │    │ |
|  │  │  │    "Only happens in production under load"                  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✓ You need to understand timing/latency                    │ │    │ |
|  │  │  │    "Where is the time going?"                               │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✓ You need to understand causality                         │ │    │ |
|  │  │  │    "What triggered this?"                                   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✓ You need to debug without restart                        │ │    │ |
|  │  │  │    "Can't take down production"                             │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✓ You need deep visibility into internals                  │ │    │ |
|  │  │  │    "What's happening inside this library?"                  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  DON'T USE TRACING WHEN:                                         │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✗ Simple bugs reproducible with logging                    │ │    │ |
|  │  │  │    Just add a log statement                                 │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✗ Need persistent record of all events                     │ │    │ |
|  │  │  │    Use logging/audit instead                                │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✗ Simple aggregate metrics suffice                         │ │    │ |
|  │  │  │    Use counters/histograms                                  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ✗ Development environment with debugger                    │ │    │ |
|  │  │  │    Just use gdb/lldb                                        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  OVERHEAD BUDGET                                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Scenario               Acceptable overhead                      │    │ |
|  │  │  ───────────────────────────────────────────────────────────────│    │ |
|  │  │  Always-on monitoring   <1%    (like static tracepoints off)     │    │ |
|  │  │  Production debugging   <5%    (like enabled tracepoints)        │    │ |
|  │  │  Deep investigation     <20%   (like kprobes + BPF)              │    │ |
|  │  │  Development profiling  Any    (like function_graph tracer)      │    │ |
|  │  │                                                                  │    │ |
|  │  │  Rule: Cost should match value                                   │    │ |
|  │  │  • High-value insights justify higher cost                       │    │ |
|  │  │  • Baseline monitoring must be nearly free                       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SAMPLING VS FULL TRACING                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Like perf sampling: don't capture everything                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Full tracing:                                                   │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │  Every request → full trace                                 │ │    │ |
|  │  │  │  1M req/sec × 1KB trace = 1GB/sec data!                     │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Sampled tracing:                                                │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │  1% of requests → full trace                                │ │    │ |
|  │  │  │  10K req/sec × 1KB = 10MB/sec (manageable)                  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Sampling strategies:                                       │ │    │ |
|  │  │  │  • Random: 1 in N requests                                  │ │    │ |
|  │  │  │  • Head-based: decide at request start                      │ │    │ |
|  │  │  │  • Tail-based: keep if slow/error                           │ │    │ |
|  │  │  │  • Adaptive: increase on anomalies                          │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**何时追踪值得代价**：

**使用追踪当**：
- ✓ 无法在本地重现问题
- ✓ 需要理解时序/延迟
- ✓ 需要理解因果关系
- ✓ 需要不重启调试
- ✓ 需要深入了解内部

**不使用追踪当**：
- ✗ 简单 bug 用日志可重现
- ✗ 需要所有事件的持久记录
- ✗ 简单聚合指标足够
- ✗ 有调试器的开发环境

**开销预算**：
- 始终在线监控：<1%
- 生产调试：<5%
- 深入调查：<20%
- 开发分析：任何

**采样 vs 完整追踪**：
- 完整追踪：1M 请求/秒 × 1KB = 1GB/秒数据！
- 采样追踪：1% 请求 → 10MB/秒（可管理）
- 采样策略：随机、头部、尾部、自适应

---

## 3. 常见追踪反模式

```
COMMON TRACING ANTI-PATTERNS
+=============================================================================+
|                                                                              |
|  ANTI-PATTERN 1: ALWAYS-ON VERBOSE TRACING                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem:                                                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // BAD: Always tracing everything                          │ │    │ |
|  │  │  │  void process(Request req) {                                │ │    │ |
|  │  │  │      trace("Starting process");                             │ │    │ |
|  │  │  │      trace("Step 1 complete");                              │ │    │ |
|  │  │  │      trace("Step 2 complete");                              │ │    │ |
|  │  │  │      // 20 more trace calls...                              │ │    │ |
|  │  │  │      trace("Process complete");                             │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  → CPU overhead even when not debugging                     │ │    │ |
|  │  │  │  → Storage costs for unused data                            │ │    │ |
|  │  │  │  → Signal lost in noise                                     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution (like kernel tracepoints):                             │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // GOOD: Disabled by default, enable when needed           │ │    │ |
|  │  │  │  void process(Request req) {                                │ │    │ |
|  │  │  │      if (unlikely(tracing_enabled)) {                       │ │    │ |
|  │  │  │          trace_start(req);                                  │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │      // ... work ...                                        │ │    │ |
|  │  │  │      if (unlikely(tracing_enabled)) {                       │ │    │ |
|  │  │  │          trace_end(req);                                    │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 2: SYNCHRONOUS TRACE EXPORT                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem:                                                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // BAD: Blocking I/O in hot path                           │ │    │ |
|  │  │  │  void trace(Event e) {                                      │ │    │ |
|  │  │  │      // Blocks on network!                                  │ │    │ |
|  │  │  │      http_post("https://trace-server/api/events", e);       │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  → P99 latency spikes when server slow                      │ │    │ |
|  │  │  │  → Request fails if trace server down                       │ │    │ |
|  │  │  │  → Adds 1-10ms to every request                             │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution (like kernel ring buffer):                             │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // GOOD: Write to buffer, async export                     │ │    │ |
|  │  │  │  void trace(Event e) {                                      │ │    │ |
|  │  │  │      ring_buffer_write(&local_buffer, e);  // ~100ns        │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // Background thread exports                               │ │    │ |
|  │  │  │  void exporter_thread() {                                   │ │    │ |
|  │  │  │      while (running) {                                      │ │    │ |
|  │  │  │          batch = collect_from_all_buffers();                │ │    │ |
|  │  │  │          http_post(trace_server, batch);                    │ │    │ |
|  │  │  │          sleep(1s);                                         │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 3: NO FILTERING                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem:                                                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // BAD: Collect all traces, filter later                   │ │    │ |
|  │  │  │  - Collect 100% of traces                                   │ │    │ |
|  │  │  │  - Send to central server                                   │ │    │ |
|  │  │  │  - Query for what you need                                  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  → Massive data transfer                                    │ │    │ |
|  │  │  │  → Storage explosion                                        │ │    │ |
|  │  │  │  → Most data never used                                     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution (like BPF in-kernel filtering):                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // GOOD: Filter at source                                  │ │    │ |
|  │  │  │  void trace(Event e) {                                      │ │    │ |
|  │  │  │      // Only trace if interesting                           │ │    │ |
|  │  │  │      if (e.duration > threshold ||                          │ │    │ |
|  │  │  │          e.status >= 500 ||                                 │ │    │ |
|  │  │  │          e.request_id == debug_request_id) {                │ │    │ |
|  │  │  │          ring_buffer_write(&buffer, e);                     │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // Like BPF: aggregate in kernel, export summary           │ │    │ |
|  │  │  │  void bpf_style() {                                         │ │    │ |
|  │  │  │      histogram[bucket(e.latency)]++;  // aggregate locally  │ │    │ |
|  │  │  │      // Export histogram, not raw events                    │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 4: IGNORING CARDINALITY                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem:                                                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // BAD: High cardinality labels                            │ │    │ |
|  │  │  │  counter.WithLabels(user_id, request_id, timestamp).Inc()   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  user_id:     millions of unique values                     │ │    │ |
|  │  │  │  request_id:  billions of unique values                     │ │    │ |
|  │  │  │  timestamp:   infinite unique values                        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  → Memory explosion for metric storage                      │ │    │ |
|  │  │  │  → Query becomes slow/impossible                            │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution:                                                       │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // GOOD: Low cardinality for metrics                       │ │    │ |
|  │  │  │  counter.WithLabels(endpoint, status_class).Inc()           │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  endpoint:     ~100 values                                  │ │    │ |
|  │  │  │  status_class: 5 values (1xx, 2xx, 3xx, 4xx, 5xx)           │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // For high-cardinality: use traces, not metrics           │ │    │ |
|  │  │  │  // user_id, request_id → belong in trace span              │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  KEY LESSONS FROM KERNEL                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. OFF BY DEFAULT                                                       │ |
|  │     Tracing should cost nothing when not used                            │ |
|  │                                                                          │ |
|  │  2. ASYNC EXPORT                                                         │ |
|  │     Never block hot path on trace collection                             │ |
|  │                                                                          │ |
|  │  3. FILTER AT SOURCE                                                     │ |
|  │     Don't collect what you won't use                                     │ |
|  │                                                                          │ |
|  │  4. AGGREGATE LOCALLY                                                    │ |
|  │     Summarize in producer, export summaries                              │ |
|  │                                                                          │ |
|  │  5. FIXED MEMORY                                                         │ |
|  │     Use ring buffers, don't allocate per event                           │ |
|  │                                                                          │ |
|  │  6. BINARY FORMAT                                                        │ |
|  │     Structured events > formatted strings                                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**常见追踪反模式**：

**反模式 1：始终开启的详细追踪**
- 问题：即使不调试也有 CPU 开销，信号淹没在噪音中
- 解决方案：默认禁用，需要时启用（像内核 tracepoints）

**反模式 2：同步追踪导出**
- 问题：热路径阻塞 I/O，P99 延迟尖峰
- 解决方案：写入缓冲区，异步导出（像内核环形缓冲区）

**反模式 3：无过滤**
- 问题：收集所有追踪，后过滤 → 大量数据传输
- 解决方案：在源头过滤（像 BPF 内核过滤）
- 本地聚合，导出摘要

**反模式 4：忽略基数**
- 问题：高基数标签（user_id、request_id）→ 内存爆炸
- 解决方案：低基数用于指标，高基数用于追踪

**内核关键教训**：
1. **默认关闭**：不使用时追踪应无成本
2. **异步导出**：永不在热路径阻塞追踪收集
3. **源头过滤**：不收集不会用的
4. **本地聚合**：在生产者中总结，导出摘要
5. **固定内存**：使用环形缓冲区，不按事件分配
6. **二进制格式**：结构化事件 > 格式化字符串
