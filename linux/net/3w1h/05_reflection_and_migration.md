# 反思与迁移（Reflection and Migration）

## 1. 这个设计在什么情况下会失效

```
DESIGN FAILURE MODES
+=============================================================================+
|                                                                              |
|  FAILURE MODE 1: HIGH CONCURRENCY LOCK CONTENTION (高并发锁竞争)             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Scenario:                                                               │ |
|  │  • Many connections sharing same hash bucket                             │ |
|  │  • Many threads operating on same socket                                 │ |
|  │  • Global data structures under high update rate                         │ |
|  │                                                                          │ |
|  │  Problem manifestation:                                                  │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  CPU0         CPU1         CPU2         CPU3                     │    │ |
|  │  │    │            │            │            │                      │    │ |
|  │  │    ▼            ▼            ▼            ▼                      │    │ |
|  │  │  lock_sock   lock_sock    lock_sock    lock_sock                 │    │ |
|  │  │    │            │            │            │                      │    │ |
|  │  │    ▼         (blocked)   (blocked)   (blocked)                   │    │ |
|  │  │  [work]         .            .            .                      │    │ |
|  │  │    │            .            .            .                      │    │ |
|  │  │  release     acquire      (wait)       (wait)                    │    │ |
|  │  │               [work]         .            .                      │    │ |
|  │  │              release      acquire      (wait)                    │    │ |
|  │  │                           [work]          .                      │    │ |
|  │  │                          release       acquire                   │    │ |
|  │  │                                        [work]                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Total time = 4x single operation time                           │    │ |
|  │  │  Parallelism = 0%                                                │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Linux mitigations:                                                      │ |
|  │  • Per-socket locks (not global)                                        │ |
|  │  • RCU for read-heavy structures                                        │ |
|  │  • Per-CPU statistics                                                   │ |
|  │  • Hash table to distribute connections                                  │ |
|  │                                                                          │ |
|  │  When these fail:                                                        │ |
|  │  • Single socket with many threads                                      │ |
|  │  • Short connections causing hash table churn                            │ |
|  │  • High rate of socket creation/destruction                              │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  FAILURE MODE 2: EXTREME NETWORK LOAD NAPI LATENCY (极端负载下 NAPI 延迟)    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Scenario:                                                               │ |
|  │  • 100 Gbps traffic with minimum size packets                           │ |
|  │  • 148 million packets per second                                       │ |
|  │  • Per-packet budget: ~7ns                                              │ |
|  │                                                                          │ |
|  │  Problem:                                                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  NAPI poll cycle:                                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  Budget = 64 packets                                             │    │ |
|  │  │  Processing time per packet ≈ 100-500 ns                         │    │ |
|  │  │  Total poll time ≈ 6-32 μs                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  At 100 Gbps:                                                    │    │ |
|  │  │  Packets arriving per μs ≈ 148                                   │    │ |
|  │  │  Packets arriving in 32 μs ≈ 4,736                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Result: Ring buffer overflow!                                   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Solution at this scale:                                                 │ |
|  │  • Hardware offload (checksum, segmentation)                            │ |
|  │  • Multi-queue NICs with RSS                                            │ |
|  │  • XDP/eBPF for early packet processing                                 │ |
|  │  • Kernel bypass (DPDK, netmap)                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  FAILURE MODE 3: NEW HARDWARE INCOMPATIBILITY (新硬件不兼容)                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Scenario:                                                               │ |
|  │  • New NIC with novel offload capabilities                              │ |
|  │  • New transport protocols (QUIC, etc.)                                 │ |
|  │  • New addressing schemes                                               │ |
|  │                                                                          │ |
|  │  Problems:                                                               │ |
|  │                                                                          │ |
|  │  1. net_device_ops may not have needed operations                       │ |
|  │     ┌──────────────────────────────────────────────────────────────┐   │ |
|  │     │  struct net_device_ops {                                      │   │ |
|  │     │      .ndo_start_xmit   // exists                              │   │ |
|  │     │      .ndo_select_queue // exists                              │   │ |
|  │     │      .ndo_new_feature  // DOESN'T EXIST!                      │   │ |
|  │     │  };                                                           │   │ |
|  │     │                                                                │   │ |
|  │     │  Solutions:                                                    │   │ |
|  │     │  • Add new ops (requires kernel patch)                        │   │ |
|  │     │  • Use ethtool/ioctl extensions                               │   │ |
|  │     │  • Vendor-specific sysfs entries                              │   │ |
|  │     └──────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  2. sk_buff assumptions may not hold                                    │ |
|  │     ┌──────────────────────────────────────────────────────────────┐   │ |
|  │     │  sk_buff assumes:                                             │   │ |
|  │     │  • Linear buffer or scatter-gather                            │   │ |
|  │     │  • Standard header layout                                     │   │ |
|  │     │  • Single packet per skb                                      │   │ |
|  │     │                                                                │   │ |
|  │     │  Novel hardware might need:                                   │   │ |
|  │     │  • Completely different buffer format                         │   │ |
|  │     │  • Hardware-specific metadata                                 │   │ |
|  │     │  • Batched operations                                         │   │ |
|  │     └──────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  FAILURE MODE 4: SECURITY VULNERABILITIES (安全漏洞)                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  TCP state machine complexity creates attack surface:                    │ |
|  │                                                                          │ |
|  │  1. SYN flood attacks                                                   │ |
|  │     • Half-open connections exhaust resources                           │ |
|  │     • Mitigation: SYN cookies                                           │ |
|  │                                                                          │ |
|  │  2. RST attacks                                                         │ |
|  │     • Blind RST can terminate connections                               │ |
|  │     • Mitigation: Sequence number validation                            │ |
|  │                                                                          │ |
|  │  3. TIME_WAIT assassination                                             │ |
|  │     • Attack lingering connections                                      │ |
|  │     • Mitigation: Strict state validation                               │ |
|  │                                                                          │ |
|  │  4. Side-channel leaks                                                  │ |
|  │     • Timing attacks reveal internal state                              │ |
|  │     • Mitigation: Constant-time operations (not always done)            │ |
|  │                                                                          │ |
|  │  Fundamental issue: The abstraction exposes more state than necessary   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

设计失效的场景：

1. **高并发锁竞争**：
   - 多线程操作同一 socket 时，锁成为串行化瓶颈
   - Linux 通过 per-socket 锁和 RCU 缓解，但无法完全消除

2. **极端网络负载**：
   - 100Gbps 场景下，NAPI 无法处理所有数据包
   - 需要硬件卸载、XDP 或内核旁路

3. **新硬件不兼容**：
   - `net_device_ops` 可能缺少新硬件需要的操作
   - `sk_buff` 的假设可能不适用于新协议

4. **安全漏洞**：
   - 状态机复杂性增加攻击面
   - SYN flood、RST 攻击、TIME_WAIT 攻击等

---

## 2. 哪些部分可以直接借鉴，哪些必须调整

```
LESSONS FOR YOUR OWN PROJECTS
+=============================================================================+
|                                                                              |
|  DIRECTLY APPLICABLE (可以直接借鉴)                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. LAYERED ARCHITECTURE (分层设计)                                      │ |
|  │     ┌──────────────────────────────────────────────────────────────┐   │ |
|  │     │                                                                │   │ |
|  │     │  Applicable to:                                                │   │ |
|  │     │  • Any system with multiple abstraction levels                 │   │ |
|  │     │  • API gateways                                                │   │ |
|  │     │  • Plugin architectures                                        │   │ |
|  │     │                                                                │   │ |
|  │     │  Key takeaways:                                                │   │ |
|  │     │  • Define clear interfaces between layers                      │   │ |
|  │     │  • Each layer hides complexity from upper layers               │   │ |
|  │     │  • Allow independent layer evolution                           │   │ |
|  │     │                                                                │   │ |
|  │     │  Example pattern:                                              │   │ |
|  │     │  struct layer_ops {                                            │   │ |
|  │     │      int (*process)(struct context *ctx, struct data *d);      │   │ |
|  │     │      int (*configure)(struct context *ctx, void *config);      │   │ |
|  │     │  };                                                            │   │ |
|  │     │                                                                │   │ |
|  │     └──────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  2. BUFFER MANAGEMENT (缓冲区管理 - sk_buff 模式)                        │ |
|  │     ┌──────────────────────────────────────────────────────────────┐   │ |
|  │     │                                                                │   │ |
|  │     │  Applicable to:                                                │   │ |
|  │     │  • Message passing systems                                     │   │ |
|  │     │  • Protocol parsers                                            │   │ |
|  │     │  • Data pipeline processors                                    │   │ |
|  │     │                                                                │   │ |
|  │     │  Key takeaways:                                                │   │ |
|  │     │  • Single buffer travels through all layers                    │   │ |
|  │     │  • Metadata separate from data                                 │   │ |
|  │     │  • Head/tail pointers for header manipulation                  │   │ |
|  │     │  • Reference counting for sharing                              │   │ |
|  │     │                                                                │   │ |
|  │     │  Example pattern:                                              │   │ |
|  │     │  struct message_buffer {                                       │   │ |
|  │     │      void *head;           // buffer start                     │   │ |
|  │     │      void *data;           // payload start                    │   │ |
|  │     │      size_t len;           // payload length                   │   │ |
|  │     │      size_t capacity;      // total capacity                   │   │ |
|  │     │      atomic_t refcount;    // for sharing                      │   │ |
|  │     │      struct metadata meta; // processing state                 │   │ |
|  │     │  };                                                            │   │ |
|  │     │                                                                │   │ |
|  │     └──────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  3. CALLBACK/EVENT-DRIVEN DESIGN (回调/事件驱动)                         │ |
|  │     ┌──────────────────────────────────────────────────────────────┐   │ |
|  │     │                                                                │   │ |
|  │     │  Applicable to:                                                │   │ |
|  │     │  • Async I/O systems                                           │   │ |
|  │     │  • Event loops                                                 │   │ |
|  │     │  • Plugin systems                                              │   │ |
|  │     │                                                                │   │ |
|  │     │  Key takeaways:                                                │   │ |
|  │     │  • Register handlers, don't poll                               │   │ |
|  │     │  • Clear callback interfaces                                   │   │ |
|  │     │  • Context passing to callbacks                                │   │ |
|  │     │                                                                │   │ |
|  │     │  Example pattern:                                              │   │ |
|  │     │  struct handler_ops {                                          │   │ |
|  │     │      void (*on_data)(void *ctx, struct buffer *buf);           │   │ |
|  │     │      void (*on_error)(void *ctx, int err);                     │   │ |
|  │     │      void (*on_close)(void *ctx);                              │   │ |
|  │     │  };                                                            │   │ |
|  │     │                                                                │   │ |
|  │     │  void register_handler(int id, struct handler_ops *ops,        │   │ |
|  │     │                        void *ctx);                             │   │ |
|  │     │                                                                │   │ |
|  │     └──────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  4. FINITE STATE MACHINE (有限状态机)                                    │ |
|  │     ┌──────────────────────────────────────────────────────────────┐   │ |
|  │     │                                                                │   │ |
|  │     │  Applicable to:                                                │   │ |
|  │     │  • Protocol implementations                                    │   │ |
|  │     │  • Connection management                                       │   │ |
|  │     │  • Workflow engines                                            │   │ |
|  │     │                                                                │   │ |
|  │     │  Key takeaways:                                                │   │ |
|  │     │  • Centralize state in one field                               │   │ |
|  │     │  • Single function for state transitions                       │   │ |
|  │     │  • State transition table for validation                       │   │ |
|  │     │                                                                │   │ |
|  │     │  Example pattern (from tcp.c):                                 │   │ |
|  │     │  static const int valid_transitions[NUM_STATES][NUM_EVENTS] = {│   │ |
|  │     │      [STATE_INIT][EVENT_START] = STATE_RUNNING,                │   │ |
|  │     │      [STATE_RUNNING][EVENT_PAUSE] = STATE_PAUSED,              │   │ |
|  │     │      [STATE_RUNNING][EVENT_STOP] = STATE_STOPPED,              │   │ |
|  │     │      // ... etc                                                │   │ |
|  │     │  };                                                            │   │ |
|  │     │                                                                │   │ |
|  │     │  int set_state(struct obj *o, int event) {                     │   │ |
|  │     │      int new_state = valid_transitions[o->state][event];       │   │ |
|  │     │      if (new_state == INVALID) return -EINVAL;                 │   │ |
|  │     │      o->state = new_state;                                     │   │ |
|  │     │      return 0;                                                 │   │ |
|  │     │  }                                                             │   │ |
|  │     │                                                                │   │ |
|  │     └──────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  5. PLUGIN/REGISTRY PATTERN (插件/注册模式)                              │ |
|  │     ┌──────────────────────────────────────────────────────────────┐   │ |
|  │     │                                                                │   │ |
|  │     │  Applicable to:                                                │   │ |
|  │     │  • Extensible systems                                          │   │ |
|  │     │  • Algorithm selection                                         │   │ |
|  │     │  • Driver/adapter patterns                                     │   │ |
|  │     │                                                                │   │ |
|  │     │  Key takeaways:                                                │   │ |
|  │     │  • Define interface struct with function pointers              │   │ |
|  │     │  • Maintain registry of implementations                        │   │ |
|  │     │  • Lookup by name/type at runtime                              │   │ |
|  │     │                                                                │   │ |
|  │     │  Example pattern:                                              │   │ |
|  │     │  struct algorithm_ops {                                        │   │ |
|  │     │      const char *name;                                         │   │ |
|  │     │      int (*init)(void *ctx);                                   │   │ |
|  │     │      int (*process)(void *ctx, void *data);                    │   │ |
|  │     │      void (*cleanup)(void *ctx);                               │   │ |
|  │     │  };                                                            │   │ |
|  │     │                                                                │   │ |
|  │     │  void register_algorithm(struct algorithm_ops *ops);           │   │ |
|  │     │  struct algorithm_ops *find_algorithm(const char *name);       │   │ |
|  │     │                                                                │   │ |
|  │     └──────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

```
MUST ADJUST FOR SCALE (必须根据规模调整)
+=============================================================================+
|                                                                              |
|  1. CONCURRENCY STRATEGY (并发策略)                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux kernel approach:                                                  │ |
|  │  • RCU for read-heavy, write-rare data                                  │ |
|  │  • Per-CPU data to avoid sharing                                        │ |
|  │  • Fine-grained spinlocks                                               │ |
|  │  • Lock-free algorithms where possible                                  │ |
|  │                                                                          │ |
|  │  Adjustment needed for:                                                  │ |
|  │                                                                          │ |
|  │  ┌──────────────────────┬────────────────────────────────────────────┐ │ |
|  │  │ Your Scale           │ Recommended Approach                        │ │ |
|  │  ├──────────────────────┼────────────────────────────────────────────┤ │ |
|  │  │ Single-threaded      │ No locks needed                             │ │ |
|  │  │                      │ Simple state management                     │ │ |
|  │  ├──────────────────────┼────────────────────────────────────────────┤ │ |
|  │  │ Few threads          │ Single mutex per resource                   │ │ |
|  │  │ (<8 cores)           │ Maybe reader-writer locks                   │ │ |
|  │  ├──────────────────────┼────────────────────────────────────────────┤ │ |
|  │  │ Many threads         │ Fine-grained locking                        │ │ |
|  │  │ (8-64 cores)         │ Consider lock-free structures              │ │ |
|  │  │                      │ Per-thread caching                          │ │ |
|  │  ├──────────────────────┼────────────────────────────────────────────┤ │ |
|  │  │ Extreme scale        │ Full kernel approach needed                 │ │ |
|  │  │ (64+ cores, millions │ RCU, per-CPU, lock-free                    │ │ |
|  │  │  of connections)     │ Consider kernel-space implementation       │ │ |
|  │  └──────────────────────┴────────────────────────────────────────────┘ │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  2. QUEUE LENGTHS (队列长度)                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux kernel defaults:                                                  │ |
|  │  • TCP receive buffer: 87380-6291456 bytes (adjustable)                 │ |
|  │  • TCP send buffer: 16384-4194304 bytes (adjustable)                    │ |
|  │  • Device TX queue: ~1000 packets                                       │ |
|  │  • Listen backlog: up to 128 (SOMAXCONN)                                │ |
|  │                                                                          │ |
|  │  Adjustment needed based on:                                             │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  Memory constraint:                                              │    │ |
|  │  │  • Embedded: Small buffers, aggressive dropping                  │    │ |
|  │  │  • Server: Large buffers, buffer bloat concerns                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  Latency requirement:                                            │    │ |
|  │  │  • Low latency: Small queues (< 100 packets)                     │    │ |
|  │  │  • High throughput: Large queues (1000+ packets)                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  Traffic pattern:                                                │    │ |
|  │  │  • Bursty: Larger queues to absorb bursts                        │    │ |
|  │  │  • Steady: Smaller queues sufficient                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  Application type:                                               │    │ |
|  │  │  • File transfer: Large buffers                                  │    │ |
|  │  │  • Gaming/VoIP: Small buffers                                    │    │ |
|  │  │  • Web server: Medium, adaptive                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  3. EXTENSION INTERFACES (扩展接口)                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux kernel approach:                                                  │ |
|  │  • Many extension points (Netfilter, qdisc, protocols, drivers)        │ |
|  │  • Comprehensive ops structures with many callbacks                     │ |
|  │  • Module system for runtime loading                                    │ |
|  │                                                                          │ |
|  │  Adjustment for smaller systems:                                         │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  YAGNI principle: Only add extension points you need             │    │ |
|  │  │                                                                  │    │ |
|  │  │  Instead of:                                                     │    │ |
|  │  │  struct ops {                                                    │    │ |
|  │  │      void (*on_event1)();                                        │    │ |
|  │  │      void (*on_event2)();                                        │    │ |
|  │  │      ... 50 more callbacks ...                                   │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Start with:                                                     │    │ |
|  │  │  struct ops {                                                    │    │ |
|  │  │      void (*process)(void *ctx, struct data *d);                 │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Add more only when proven necessary                             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**可直接借鉴的设计：**
1. **分层架构**：清晰的接口定义，层间独立演进
2. **缓冲区管理**：sk_buff 模式，单一缓冲区穿越所有层
3. **回调/事件驱动**：注册处理器而非轮询
4. **有限状态机**：集中状态管理，状态转换表验证
5. **插件/注册模式**：运行时扩展能力

**必须根据规模调整：**
1. **并发策略**：从简单互斥锁到 RCU，根据核心数选择
2. **队列长度**：根据内存、延迟、流量模式调整
3. **扩展接口**：YAGNI 原则，不要过度设计

---

## 3. 常见误用与反模式

```
COMMON MISUSE AND ANTI-PATTERNS
+=============================================================================+
|                                                                              |
|  ANTI-PATTERN 1: BYPASSING ABSTRACTION LAYERS (绕过抽象层)                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD EXAMPLE:                                                            │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  // 直接操作 sk_buff 内部，绕过 API                               │  │ |
|  │  │  skb->data = skb->head + offset;  // WRONG!                      │  │ |
|  │  │  skb->len -= removed_bytes;       // WRONG!                      │  │ |
|  │  │                                                                    │  │ |
|  │  │  // 直接访问 tcp_sock 字段而非使用 API                            │  │ |
|  │  │  struct tcp_sock *tp = tcp_sk(sk);                                │  │ |
|  │  │  tp->snd_cwnd = new_value;  // WRONG! No locking!                │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  CORRECT APPROACH:                                                       │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  // 使用提供的 API                                                │  │ |
|  │  │  skb_pull(skb, removed_bytes);    // Proper API                  │  │ |
|  │  │  skb_push(skb, header_size);      // Proper API                  │  │ |
|  │  │                                                                    │  │ |
|  │  │  // 使用 socket 选项 API                                          │  │ |
|  │  │  setsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, "cubic", 5);        │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  Why it matters:                                                         │ |
|  │  • Internal details may change between kernel versions                  │ |
|  │  • API enforces invariants (e.g., checksum update)                      │ |
|  │  • Missing side effects (e.g., statistics, events)                      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 2: IGNORING REFERENCE COUNTING (忽略引用计数)                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD EXAMPLE:                                                            │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  struct sk_buff *my_skb = skb;    // Just copy pointer           │  │ |
|  │  │  schedule_work(&my_work);         // Use it later                │  │ |
|  │  │                                                                    │  │ |
|  │  │  // Later, in work callback:                                      │  │ |
|  │  │  process(my_skb);  // CRASH! skb was freed!                      │  │ |
|  │  │                                                                    │  │ |
|  │  │  ---                                                              │  │ |
|  │  │                                                                    │  │ |
|  │  │  sock_hold(sk);                                                   │  │ |
|  │  │  // ... use sk ...                                                │  │ |
|  │  │  // Forgot sock_put(sk)!  // MEMORY LEAK!                        │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  CORRECT APPROACH:                                                       │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  skb_get(skb);                    // Increment refcount          │  │ |
|  │  │  my_skb = skb;                                                    │  │ |
|  │  │  schedule_work(&my_work);                                         │  │ |
|  │  │                                                                    │  │ |
|  │  │  // Later:                                                        │  │ |
|  │  │  process(my_skb);                                                 │  │ |
|  │  │  kfree_skb(my_skb);               // Decrement refcount          │  │ |
|  │  │                                                                    │  │ |
|  │  │  ---                                                              │  │ |
|  │  │                                                                    │  │ |
|  │  │  sock_hold(sk);                                                   │  │ |
|  │  │  // ... use sk ...                                                │  │ |
|  │  │  sock_put(sk);                    // Always balance!             │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  General rule: Every _get() needs a _put(), every _hold() needs a _put()│ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 3: BLOCKING IN WRONG CONTEXT (在错误上下文中阻塞)              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD EXAMPLE:                                                            │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  // In NAPI poll context (softirq):                               │  │ |
|  │  │  static int my_poll(struct napi_struct *napi, int budget)        │  │ |
|  │  │  {                                                                │  │ |
|  │  │      mutex_lock(&global_mutex);  // WRONG! Can't sleep!          │  │ |
|  │  │      msleep(100);                // WRONG! Can't sleep!          │  │ |
|  │  │      kmalloc(size, GFP_KERNEL);  // WRONG! Can sleep!            │  │ |
|  │  │  }                                                                │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  CORRECT APPROACH:                                                       │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  // In softirq context:                                           │  │ |
|  │  │  static int my_poll(struct napi_struct *napi, int budget)        │  │ |
|  │  │  {                                                                │  │ |
|  │  │      spin_lock(&my_spinlock);    // OK, doesn't sleep            │  │ |
|  │  │      kmalloc(size, GFP_ATOMIC);  // OK, doesn't sleep            │  │ |
|  │  │  }                                                                │  │ |
|  │  │                                                                    │  │ |
|  │  │  // In process context:                                           │  │ |
|  │  │  int my_ioctl(...)                                                │  │ |
|  │  │  {                                                                │  │ |
|  │  │      mutex_lock(&global_mutex);  // OK, can sleep                │  │ |
|  │  │      kmalloc(size, GFP_KERNEL);  // OK, can sleep                │  │ |
|  │  │  }                                                                │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  Context rules:                                                          │ |
|  │  • Hardirq: Can't sleep, very limited work                              │ |
|  │  • Softirq/BH: Can't sleep, spinlocks only                              │ |
|  │  • Process: Can sleep, mutex OK                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 4: ABUSING GLOBAL LOCKS (滥用全局锁)                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD EXAMPLE:                                                            │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  static DEFINE_MUTEX(global_lock);  // One lock for everything   │  │ |
|  │  │                                                                    │  │ |
|  │  │  void send_packet(struct socket *sk, ...)                         │  │ |
|  │  │  {                                                                │  │ |
|  │  │      mutex_lock(&global_lock);  // All sends serialized!         │  │ |
|  │  │      do_send(sk, ...);                                            │  │ |
|  │  │      mutex_unlock(&global_lock);                                  │  │ |
|  │  │  }                                                                │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  CORRECT APPROACH (like Linux kernel):                                   │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  // Per-socket lock                                               │  │ |
|  │  │  void send_packet(struct sock *sk, ...)                           │  │ |
|  │  │  {                                                                │  │ |
|  │  │      lock_sock(sk);             // Only this socket locked       │  │ |
|  │  │      do_send(sk, ...);                                            │  │ |
|  │  │      release_sock(sk);                                            │  │ |
|  │  │  }                                                                │  │ |
|  │  │                                                                    │  │ |
|  │  │  // Different sockets can proceed in parallel                     │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  Principle: Lock the minimum scope necessary                             │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 5: MODIFYING SHARED STRUCTURES DIRECTLY (直接修改共享结构)     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD EXAMPLE:                                                            │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  // Modifying routing table entry directly                        │  │ |
|  │  │  struct rtable *rt = get_route(...);                              │  │ |
|  │  │  rt->rt_gateway = new_gateway;  // WRONG! Other CPUs reading!    │  │ |
|  │  │                                                                    │  │ |
|  │  │  // Modifying protocol handler list                               │  │ |
|  │  │  ptype->func = new_handler;  // WRONG! Race condition!           │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  CORRECT APPROACH (RCU pattern):                                         │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  // Create new version                                            │  │ |
|  │  │  struct rtable *new_rt = kmalloc(...);                            │  │ |
|  │  │  memcpy(new_rt, old_rt, sizeof(*new_rt));                         │  │ |
|  │  │  new_rt->rt_gateway = new_gateway;                                │  │ |
|  │  │                                                                    │  │ |
|  │  │  // Atomically swap pointers                                      │  │ |
|  │  │  rcu_assign_pointer(route_table[idx], new_rt);                    │  │ |
|  │  │                                                                    │  │ |
|  │  │  // Wait for readers to finish                                    │  │ |
|  │  │  synchronize_rcu();                                                │  │ |
|  │  │                                                                    │  │ |
|  │  │  // Free old version                                              │  │ |
|  │  │  kfree(old_rt);                                                   │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  Principle: Copy-on-write for shared data                                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ANTI-PATTERN 6: IGNORING ERROR PATHS (忽略错误路径)                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  BAD EXAMPLE:                                                            │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  void process_packet(struct sk_buff *skb)                         │  │ |
|  │  │  {                                                                │  │ |
|  │  │      struct header *hdr = parse_header(skb);                      │  │ |
|  │  │      if (!hdr)                                                    │  │ |
|  │  │          return;  // WRONG! skb leaked!                          │  │ |
|  │  │                                                                    │  │ |
|  │  │      lock_resource();                                             │  │ |
|  │  │      if (error_condition())                                       │  │ |
|  │  │          return;  // WRONG! lock held, skb leaked!               │  │ |
|  │  │  }                                                                │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  CORRECT APPROACH:                                                       │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  void process_packet(struct sk_buff *skb)                         │  │ |
|  │  │  {                                                                │  │ |
|  │  │      int err = 0;                                                 │  │ |
|  │  │                                                                    │  │ |
|  │  │      struct header *hdr = parse_header(skb);                      │  │ |
|  │  │      if (!hdr) {                                                  │  │ |
|  │  │          err = -EINVAL;                                           │  │ |
|  │  │          goto drop;                                               │  │ |
|  │  │      }                                                            │  │ |
|  │  │                                                                    │  │ |
|  │  │      lock_resource();                                             │  │ |
|  │  │      if (error_condition()) {                                     │  │ |
|  │  │          err = -EIO;                                              │  │ |
|  │  │          goto unlock;                                             │  │ |
|  │  │      }                                                            │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Success path                                              │  │ |
|  │  │                                                                    │  │ |
|  │  │  unlock:                                                          │  │ |
|  │  │      unlock_resource();                                           │  │ |
|  │  │  drop:                                                            │  │ |
|  │  │      if (err)                                                     │  │ |
|  │  │          kfree_skb(skb);                                          │  │ |
|  │  │  }                                                                │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  Principle: Cleanup in reverse order of acquisition                      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**常见反模式：**

1. **绕过抽象层**：
   - 直接操作内部字段而非使用 API
   - 破坏封装，导致版本不兼容

2. **忽略引用计数**：
   - 使用对象后未释放 → 内存泄漏
   - 未增加引用就异步使用 → 使用后释放

3. **在错误上下文中阻塞**：
   - 软中断中使用互斥锁
   - 硬中断中做耗时操作

4. **滥用全局锁**：
   - 所有操作共用一把锁
   - 完全串行化，无法扩展

5. **直接修改共享结构**：
   - 不使用 RCU/copy-on-write
   - 导致读者看到不一致状态

6. **忽略错误路径**：
   - 错误返回时未释放资源
   - 资源泄漏累积导致系统崩溃

---

## 总结

```
KEY LESSONS SUMMARY
+=============================================================================+
|                                                                              |
|  FROM LINUX NETWORK SUBSYSTEM TO YOUR PROJECTS                               |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  DO (应该做的):                                                          │ |
|  │                                                                          │ |
|  │  ✓ Design in layers with clear interfaces                               │ |
|  │  ✓ Use callbacks for extension and decoupling                           │ |
|  │  ✓ Implement state machines with transition tables                       │ |
|  │  ✓ Use reference counting for shared resources                          │ |
|  │  ✓ Protect concurrent access at appropriate granularity                  │ |
|  │  ✓ Register plugins through well-defined interfaces                      │ |
|  │  ✓ Follow cleanup patterns (goto chains or RAII)                        │ |
|  │                                                                          │ |
|  │  DON'T (不应该做的):                                                     │ |
|  │                                                                          │ |
|  │  ✗ Bypass abstraction layers for "performance"                          │ |
|  │  ✗ Ignore reference counting rules                                      │ |
|  │  ✗ Use global locks when per-resource locks work                        │ |
|  │  ✗ Modify shared structures without proper synchronization               │ |
|  │  ✗ Block in interrupt/softirq context                                   │ |
|  │  ✗ Leak resources on error paths                                        │ |
|  │                                                                          │ |
|  │  ADJUST FOR YOUR SCALE (根据规模调整):                                   │ |
|  │                                                                          │ |
|  │  Small system → Simple locks, fewer extension points                     │ |
|  │  Medium system → Fine-grained locks, key extension points                │ |
|  │  Large system → Full kernel-style approach                               │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**核心经验总结：**

**应该做的：**
- 清晰的分层接口设计
- 使用回调实现解耦和扩展
- 用状态转换表实现状态机
- 使用引用计数管理共享资源
- 在合适粒度保护并发访问
- 通过注册机制支持插件
- 遵循清理模式（goto 链或 RAII）

**不应该做的：**
- 为了"性能"绕过抽象层
- 忽略引用计数规则
- 在可以用细粒度锁时使用全局锁
- 不同步地修改共享结构
- 在中断/软中断上下文中阻塞
- 在错误路径泄漏资源

**根据规模调整：**
- 小系统：简单锁，少量扩展点
- 中型系统：细粒度锁，关键扩展点
- 大型系统：完整的内核风格方案
