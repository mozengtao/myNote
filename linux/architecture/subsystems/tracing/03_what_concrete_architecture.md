# WHAT｜具体架构

## 1. 模式：事件驱动追踪

```
PATTERNS: EVENT-DRIVEN TRACING
+=============================================================================+
|                                                                              |
|  THE OBSERVER PATTERN IN KERNEL                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Classic Observer Pattern:                                       │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Subject (Observable)        Observers                      │ │    │ |
|  │  │  │  ┌──────────────┐           ┌──────────────┐                │ │    │ |
|  │  │  │  │              │           │  Observer 1  │                │ │    │ |
|  │  │  │  │  notify() ───┼──────────►│  update()    │                │ │    │ |
|  │  │  │  │              │           └──────────────┘                │ │    │ |
|  │  │  │  │              │           ┌──────────────┐                │ │    │ |
|  │  │  │  │              │──────────►│  Observer 2  │                │ │    │ |
|  │  │  │  │              │           │  update()    │                │ │    │ |
|  │  │  │  └──────────────┘           └──────────────┘                │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Linux Tracing Implementation:                                   │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Tracepoint (Subject)        Probes (Observers)             │ │    │ |
|  │  │  │  ┌──────────────────┐       ┌──────────────────┐            │ │    │ |
|  │  │  │  │                  │       │  ftrace_probe    │            │ │    │ |
|  │  │  │  │ sched_switch ────┼──────►│  write_to_ring   │            │ │    │ |
|  │  │  │  │                  │       └──────────────────┘            │ │    │ |
|  │  │  │  │                  │       ┌──────────────────┐            │ │    │ |
|  │  │  │  │                  │──────►│  perf_probe      │            │ │    │ |
|  │  │  │  │                  │       │  sample_event    │            │ │    │ |
|  │  │  │  │                  │       └──────────────────┘            │ │    │ |
|  │  │  │  │                  │       ┌──────────────────┐            │ │    │ |
|  │  │  │  │                  │──────►│  bpf_prog        │            │ │    │ |
|  │  │  │  │                  │       │  run_program     │            │ │    │ |
|  │  │  │  └──────────────────┘       └──────────────────┘            │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PUBLISH-SUBSCRIBE ARCHITECTURE                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Publishers (Kernel Code)                                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │ │    │ |
|  │  │  │  │  Scheduler  │  │   VFS       │  │   Network   │         │ │    │ |
|  │  │  │  │             │  │             │  │             │         │ │    │ |
|  │  │  │  │ trace_      │  │ trace_      │  │ trace_      │         │ │    │ |
|  │  │  │  │ sched_*()   │  │ vfs_*()     │  │ net_*()     │         │ │    │ |
|  │  │  │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘         │ │    │ |
|  │  │  │         │                │                │                │ │    │ |
|  │  │  └─────────┼────────────────┼────────────────┼────────────────┘ │    │ |
|  │  │            │                │                │                  │    │ |
|  │  │            ▼                ▼                ▼                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                    EVENT BUS                                │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ┌─────────────────────────────────────────────────────┐   │ │    │ |
|  │  │  │  │              Ring Buffer (per-CPU)                   │   │ │    │ |
|  │  │  │  └─────────────────────────────────────────────────────┘   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └───────────────────────────┬────────────────────────────────┘ │    │ |
|  │  │                              │                                  │    │ |
|  │  │            ┌─────────────────┼─────────────────┐                │    │ |
|  │  │            ▼                 ▼                 ▼                │    │ |
|  │  │  Subscribers (Consumers)                                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │ │    │ |
|  │  │  │  │  trace-cmd  │  │    perf     │  │  bpftrace   │         │ │    │ |
|  │  │  │  │             │  │             │  │             │         │ │    │ |
|  │  │  │  │ read via    │  │ mmap ring   │  │ BPF maps    │         │ │    │ |
|  │  │  │  │ tracefs     │  │ buffer      │  │ perf buf    │         │ │    │ |
|  │  │  │  └─────────────┘  └─────────────┘  └─────────────┘         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**模式：事件驱动追踪**

**内核中的观察者模式**：
- 经典观察者模式：Subject 通知多个 Observer
- Linux 追踪实现：Tracepoint（Subject）→ 多个 Probe（Observers）
  - ftrace_probe：写入环形缓冲区
  - perf_probe：采样事件
  - bpf_prog：运行程序

**发布-订阅架构**：

**发布者（内核代码）**：
- 调度器：trace_sched_*()
- VFS：trace_vfs_*()
- 网络：trace_net_*()

**事件总线**：
- 环形缓冲区（Per-CPU）

**订阅者（消费者）**：
- trace-cmd：通过 tracefs 读取
- perf：mmap 环形缓冲区
- bpftrace：BPF maps、perf 缓冲区

---

## 2. 核心结构：trace_event

```
CORE STRUCTURES: TRACE_EVENT
+=============================================================================+
|                                                                              |
|  STRUCT TRACE_EVENT_CALL                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // include/linux/trace_events.h                                 │    │ |
|  │  │  struct trace_event_call {                                       │    │ |
|  │  │      struct list_head    list;      // linked list of events     │    │ |
|  │  │      struct trace_event_class *class;                            │    │ |
|  │  │      union {                                                     │    │ |
|  │  │          char *name;                // event name                │    │ |
|  │  │          struct tracepoint *tp;     // if tracepoint-based       │    │ |
|  │  │      };                                                          │    │ |
|  │  │      struct trace_event event;      // actual event              │    │ |
|  │  │      char *print_fmt;               // format string             │    │ |
|  │  │      struct event_filter *filter;   // optional filter           │    │ |
|  │  │      int flags;                     // TRACE_EVENT_FL_*          │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT TRACEPOINT                                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // include/linux/tracepoint.h                                   │    │ |
|  │  │  struct tracepoint {                                             │    │ |
|  │  │      const char *name;              // tracepoint name           │    │ |
|  │  │      struct static_key key;         // for jump label            │    │ |
|  │  │      struct tracepoint_func __rcu *funcs;  // registered probes  │    │ |
|  │  │      int (*regfunc)(void);          // called on register        │    │ |
|  │  │      void (*unregfunc)(void);       // called on unregister      │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct tracepoint_func {                                        │    │ |
|  │  │      void *func;                    // probe function            │    │ |
|  │  │      void *data;                    // private data              │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  RING BUFFER                                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // kernel/trace/ring_buffer.c                                   │    │ |
|  │  │  struct trace_buffer {                                           │    │ |
|  │  │      struct ring_buffer_per_cpu **buffers;  // per-CPU buffers   │    │ |
|  │  │      unsigned long size;                    // total size        │    │ |
|  │  │      atomic_t record_disabled;              // disable flag      │    │ |
|  │  │      cpumask_var_t cpumask;                 // which CPUs        │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct ring_buffer_per_cpu {                                    │    │ |
|  │  │      struct buffer_page *head_page;     // reader position       │    │ |
|  │  │      struct buffer_page *tail_page;     // writer position       │    │ |
|  │  │      struct buffer_page *commit_page;   // committed data        │    │ |
|  │  │      local_t entries;                   // number of entries     │    │ |
|  │  │      local_t overrun;                   // overwritten count     │    │ |
|  │  │      raw_spinlock_t reader_lock;        // for reading           │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Event entry:                                                    │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │  ┌───────────┬──────────┬──────────────────────────────┐   │ │    │ |
|  │  │  │  │ type_len  │ time_δ   │         payload              │   │ │    │ |
|  │  │  │  │ (4 bits)  │ (28 bits)│   (event-specific data)      │   │ │    │ |
|  │  │  │  └───────────┴──────────┴──────────────────────────────┘   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Compact header: only 32 bits for timestamp delta           │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  BPF STRUCTURES                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct bpf_prog {                                               │    │ |
|  │  │      u32 len;                       // insn count                │    │ |
|  │  │      enum bpf_prog_type type;       // kprobe, tracepoint, etc   │    │ |
|  │  │      struct bpf_insn *insnsi;       // BPF bytecode              │    │ |
|  │  │      bpf_func_t bpf_func;           // JIT'd function            │    │ |
|  │  │      struct bpf_prog_aux *aux;      // metadata                  │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct bpf_map {                                                │    │ |
|  │  │      const struct bpf_map_ops *ops; // operations                │    │ |
|  │  │      enum bpf_map_type map_type;    // hash, array, perf, etc    │    │ |
|  │  │      u32 key_size;                                               │    │ |
|  │  │      u32 value_size;                                             │    │ |
|  │  │      u32 max_entries;                                            │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Map types for tracing:                                          │    │ |
|  │  │  • BPF_MAP_TYPE_HASH         - key/value storage                 │    │ |
|  │  │  • BPF_MAP_TYPE_PERF_EVENT_ARRAY - per-CPU event buffers         │    │ |
|  │  │  • BPF_MAP_TYPE_RINGBUF      - efficient ring buffer             │    │ |
|  │  │  • BPF_MAP_TYPE_STACK_TRACE  - stack trace storage               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**核心结构：trace_event**

**struct trace_event_call**：
- list：事件链表
- class：事件类
- name/tp：事件名或 tracepoint
- event：实际事件
- print_fmt：格式字符串
- filter：可选过滤器

**struct tracepoint**：
- name：tracepoint 名称
- key：用于 jump label
- funcs：注册的探针
- regfunc/unregfunc：注册/注销回调

**Ring Buffer**：
- trace_buffer：per-CPU 缓冲区数组
- ring_buffer_per_cpu：head_page（读）、tail_page（写）、commit_page
- 事件条目：type_len（4 位）+ time_δ（28 位）+ payload

**BPF 结构**：
- bpf_prog：len、type、insnsi（字节码）、bpf_func（JIT 函数）
- bpf_map：ops、map_type、key_size、value_size
- 追踪的 Map 类型：HASH、PERF_EVENT_ARRAY、RINGBUF、STACK_TRACE

---

## 3. 控制流：探针执行

```
CONTROL FLOW: PROBE EXECUTION
+=============================================================================+
|                                                                              |
|  TRACEPOINT EXECUTION PATH                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Kernel code hits tracepoint                                  │    │ |
|  │  │  void __sched schedule(void) {                                   │    │ |
|  │  │      ...                                                         │    │ |
|  │  │      trace_sched_switch(preempt, prev, next);                    │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  }                                                               │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  // Macro expansion (include/trace/events/sched.h)               │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  static inline void trace_sched_switch(...) {               │ │    │ |
|  │  │  │      if (static_key_false(&__tracepoint_sched_switch.key)) {│ │    │ |
|  │  │  │          // Only if enabled                                 │ │    │ |
|  │  │  │          __traceiter_sched_switch(...);                     │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼ (if enabled)                                      │    │ |
|  │  │  // Iterator calls each registered probe                         │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  void __traceiter_sched_switch(...) {                       │ │    │ |
|  │  │  │      struct tracepoint_func *it_func;                       │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │      it_func = rcu_dereference_raw(                         │ │    │ |
|  │  │  │          __tracepoint_sched_switch.funcs);                  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │      if (it_func) {                                         │ │    │ |
|  │  │  │          do {                                               │ │    │ |
|  │  │  │              // Call each probe                             │ │    │ |
|  │  │  │              ((void(*)(...))(it_func->func))(...);          │ │    │ |
|  │  │  │          } while ((++it_func)->func);                       │ │    │ |
|  │  │  │      }                                                      │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  // Probe writes to ring buffer                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  void trace_event_buffer_commit(...) {                      │ │    │ |
|  │  │  │      ring_buffer_write(buffer, event, length);              │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  KPROBE EXECUTION PATH                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. Code executes, hits int3 (breakpoint)                        │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  2. CPU generates debug exception (#BP, vector 3)                │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  3. do_int3() [arch/x86/kernel/traps.c]                          │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  4. kprobe_int3_handler() [kernel/kprobes.c]                     │    │ |
|  │  │     ┌────────────────────────────────────────────────────────┐  │    │ |
|  │  │     │                                                         │  │    │ |
|  │  │     │  p = get_kprobe(regs->ip - 1);  // find kprobe          │  │    │ |
|  │  │     │  if (p->pre_handler)                                    │  │    │ |
|  │  │     │      p->pre_handler(p, regs);   // your handler!        │  │    │ |
|  │  │     │                                                         │  │    │ |
|  │  │     └────────────────────────────────────────────────────────┘  │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  5. Single-step original instruction                             │    │ |
|  │  │     (executed out-of-line or via trap)                           │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  6. Continue normal execution                                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  BPF EXECUTION PATH                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Probe triggers (tracepoint, kprobe, uprobe)                     │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  BPF trampoline / call site                                      │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  1. Save registers                                          │ │    │ |
|  │  │  │  2. Build BPF context (pt_regs, etc)                        │ │    │ |
|  │  │  │  3. Call JIT'd BPF function                                 │ │    │ |
|  │  │  │         │                                                   │ │    │ |
|  │  │  │         ▼                                                   │ │    │ |
|  │  │  │     ┌────────────────────────────────────────────────────┐ │ │    │ |
|  │  │  │     │  BPF Program (JIT-compiled x86_64)                  │ │ │    │ |
|  │  │  │     │                                                      │ │ │    │ |
|  │  │  │     │  // Access context                                   │ │ │    │ |
|  │  │  │     │  pid = bpf_get_current_pid_tgid();                   │ │ │    │ |
|  │  │  │     │                                                      │ │ │    │ |
|  │  │  │     │  // Update map                                       │ │ │    │ |
|  │  │  │     │  bpf_map_update_elem(&map, &key, &val, 0);           │ │ │    │ |
|  │  │  │     │                                                      │ │ │    │ |
|  │  │  │     │  // Send event to user                               │ │ │    │ |
|  │  │  │     │  bpf_perf_event_output(ctx, &perf_map, ...);         │ │ │    │ |
|  │  │  │     │                                                      │ │ │    │ |
|  │  │  │     └────────────────────────────────────────────────────┘ │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  4. Restore registers                                       │ │    │ |
|  │  │  │  5. Return to kernel code                                   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**控制流：探针执行**

**Tracepoint 执行路径**：
1. 内核代码命中 tracepoint：`trace_sched_switch()`
2. 宏展开检查静态键是否启用
3. 如果启用，迭代器调用每个注册的探针
4. 探针写入环形缓冲区

**Kprobe 执行路径**：
1. 代码执行，命中 int3（断点）
2. CPU 生成调试异常
3. do_int3() → kprobe_int3_handler()
4. 查找 kprobe，调用 pre_handler
5. 单步执行原始指令
6. 继续正常执行

**BPF 执行路径**：
1. 探针触发（tracepoint、kprobe、uprobe）
2. BPF 蹦床：保存寄存器，构建 BPF 上下文
3. 调用 JIT 编译的 BPF 函数
4. BPF 程序可以：访问上下文、更新 map、发送事件
5. 恢复寄存器，返回内核代码

---

## 4. 扩展点

```
EXTENSION POINTS
+=============================================================================+
|                                                                              |
|  ADDING NEW TRACEPOINTS                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // 1. Define in header (include/trace/events/mysubsys.h)        │    │ |
|  │  │  TRACE_EVENT(mysubsys_myevent,                                   │    │ |
|  │  │      TP_PROTO(int arg1, char *arg2),                             │    │ |
|  │  │      TP_ARGS(arg1, arg2),                                        │    │ |
|  │  │      TP_STRUCT__entry(                                           │    │ |
|  │  │          __field(int, arg1)                                      │    │ |
|  │  │          __string(arg2, arg2)                                    │    │ |
|  │  │      ),                                                          │    │ |
|  │  │      TP_fast_assign(                                             │    │ |
|  │  │          __entry->arg1 = arg1;                                   │    │ |
|  │  │          __assign_str(arg2, arg2);                               │    │ |
|  │  │      ),                                                          │    │ |
|  │  │      TP_printk("arg1=%d arg2=%s", __entry->arg1,                 │    │ |
|  │  │                __get_str(arg2))                                  │    │ |
|  │  │  );                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // 2. Use in code                                               │    │ |
|  │  │  #include <trace/events/mysubsys.h>                              │    │ |
|  │  │  trace_mysubsys_myevent(123, "hello");                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  // 3. Appears in /sys/kernel/debug/tracing/events/mysubsys/     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  CUSTOM TRACERS                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Built-in tracers in kernel/trace/                            │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  • trace_nop.c      - no-op tracer (baseline)               │ │    │ |
|  │  │  │  • trace_functions.c - function call tracer                 │ │    │ |
|  │  │  │  • trace_irqsoff.c  - interrupt latency tracer              │ │    │ |
|  │  │  │  • trace_sched_wakeup.c - wakeup latency                    │ │    │ |
|  │  │  │  • trace_branch.c   - branch profiling                      │ │    │ |
|  │  │  │  • trace_hwlat.c    - hardware latency detector             │ │    │ |
|  │  │  │  • trace_osnoise.c  - OS noise tracer                       │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Register custom tracer                                       │    │ |
|  │  │  struct tracer my_tracer = {                                     │    │ |
|  │  │      .name = "mytracer",                                         │    │ |
|  │  │      .init = my_tracer_init,                                     │    │ |
|  │  │      .reset = my_tracer_reset,                                   │    │ |
|  │  │      .start = my_tracer_start,                                   │    │ |
|  │  │      .stop = my_tracer_stop,                                     │    │ |
|  │  │  };                                                              │    │ |
|  │  │  register_tracer(&my_tracer);                                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  BPF PROGRAMS                                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  BPF program types for tracing:                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  BPF_PROG_TYPE_KPROBE        - attach to kprobes            │ │    │ |
|  │  │  │  BPF_PROG_TYPE_TRACEPOINT    - attach to tracepoints        │ │    │ |
|  │  │  │  BPF_PROG_TYPE_RAW_TRACEPOINT - raw access to args          │ │    │ |
|  │  │  │  BPF_PROG_TYPE_PERF_EVENT    - attach to perf events        │ │    │ |
|  │  │  │  BPF_PROG_TYPE_TRACING       - fentry/fexit/fmod_ret        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Example: bpftrace one-liner                                     │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  # Count syscalls by process                                │ │    │ |
|  │  │  │  bpftrace -e 'tracepoint:raw_syscalls:sys_enter {           │ │    │ |
|  │  │  │      @[comm] = count();                                     │ │    │ |
|  │  │  │  }'                                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  # Histogram of read sizes                                  │ │    │ |
|  │  │  │  bpftrace -e 'tracepoint:syscalls:sys_exit_read {           │ │    │ |
|  │  │  │      @bytes = hist(args->ret);                              │ │    │ |
|  │  │  │  }'                                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**扩展点**：

**添加新 Tracepoints**：
1. 在头文件中用 TRACE_EVENT 宏定义
2. 在代码中使用 trace_mysubsys_myevent()
3. 出现在 /sys/kernel/debug/tracing/events/mysubsys/

**自定义 Tracers**：
- 内置 tracers：nop、functions、irqsoff、sched_wakeup、hwlat、osnoise
- 注册自定义 tracer：register_tracer()

**BPF 程序**：
- 追踪的程序类型：KPROBE、TRACEPOINT、RAW_TRACEPOINT、PERF_EVENT、TRACING
- bpftrace 示例：
  - 按进程计数系统调用
  - 读取大小直方图

---

## 5. 代价：性能影响

```
COSTS: PERFORMANCE IMPACT
+=============================================================================+
|                                                                              |
|  OVERHEAD BREAKDOWN                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  TRACEPOINT OVERHEAD:                                            │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  State           Per-event cost    Example impact           │ │    │ |
|  │  │  │  ──────────────────────────────────────────────────────────│ │    │ |
|  │  │  │  Disabled        ~0 ns             0% (NOP instruction)     │ │    │ |
|  │  │  │  Enabled         100-300 ns        1-5% on hot paths        │ │    │ |
|  │  │  │  + filtering     +50-100 ns        Add for each filter      │ │    │ |
|  │  │  │  + stack trace   +500-2000 ns      Expensive!               │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  KPROBE OVERHEAD:                                                │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Operation           Cost                                   │ │    │ |
|  │  │  │  ──────────────────────────────────────────────────────────│ │    │ |
|  │  │  │  int3 trap           ~100-200 ns                            │ │    │ |
|  │  │  │  Handler dispatch    ~100 ns                                │ │    │ |
|  │  │  │  Single-step         ~300-500 ns                            │ │    │ |
|  │  │  │  ──────────────────────────────────────────────────────────│ │    │ |
|  │  │  │  Total per probe     500-1000 ns                            │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Note: Optimized kprobes can avoid single-step (~300 ns)    │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  BPF OVERHEAD:                                                   │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Component           Cost                                   │ │    │ |
|  │  │  │  ──────────────────────────────────────────────────────────│ │    │ |
|  │  │  │  Program dispatch    ~50-100 ns                             │ │    │ |
|  │  │  │  Simple BPF program  ~100-500 ns (depends on size)          │ │    │ |
|  │  │  │  Map lookup          ~50-100 ns per lookup                  │ │    │ |
|  │  │  │  Map update          ~100-200 ns per update                 │ │    │ |
|  │  │  │  perf_event_output   ~200-500 ns                            │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Complex program with maps: 500-2000 ns                     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MEMORY OVERHEAD                                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Ring buffer:                                                    │    │ |
|  │  │  • Default: 1MB per CPU (configurable)                           │    │ |
|  │  │  • 8 CPUs = 8MB of kernel memory                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  BPF maps:                                                       │    │ |
|  │  │  • Hash map: ~50 bytes per entry overhead                        │    │ |
|  │  │  • 100K entries × 50 bytes = 5MB                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  Perf ring buffer:                                               │    │ |
|  │  │  • Usually 8-64 pages per CPU                                    │    │ |
|  │  │  • 8 CPUs × 32 pages × 4KB = 1MB                                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  REAL-WORLD BENCHMARKS                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Scenario: HTTP server under load                                │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Baseline (no tracing):      100K requests/sec              │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  With tracing enabled:                                      │ │    │ |
|  │  │  │  • Basic tracepoints         98K req/sec  (-2%)             │ │    │ |
|  │  │  │  • + BPF aggregation         95K req/sec  (-5%)             │ │    │ |
|  │  │  │  • + stack traces            85K req/sec  (-15%)            │ │    │ |
|  │  │  │  • + full function tracing   50K req/sec  (-50%)            │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  GUIDELINES:                                                     │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  • Always-on tracing: aim for <1% overhead                  │ │    │ |
|  │  │  │  • Investigation: accept 5-10% during debugging             │ │    │ |
|  │  │  │  • Development: 50%+ acceptable for deep analysis           │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  • Avoid stack traces on hot paths                          │ │    │ |
|  │  │  │  • Use filtering aggressively                               │ │    │ |
|  │  │  │  • Prefer tracepoints over kprobes                          │ │    │ |
|  │  │  │  • Aggregate in kernel (BPF) not userspace                  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**代价：性能影响**

**开销分解**：

**Tracepoint 开销**：
- 禁用：~0 ns（NOP 指令）
- 启用：100-300 ns
- + 过滤：+50-100 ns
- + 堆栈跟踪：+500-2000 ns（昂贵！）

**Kprobe 开销**：
- int3 陷阱：~100-200 ns
- 处理器分发：~100 ns
- 单步执行：~300-500 ns
- 总计：500-1000 ns

**BPF 开销**：
- 程序分发：~50-100 ns
- 简单 BPF 程序：~100-500 ns
- Map 查找：~50-100 ns
- 复杂程序：500-2000 ns

**内存开销**：
- 环形缓冲区：默认每 CPU 1MB
- BPF maps：每条目 ~50 字节开销
- Perf 环形缓冲区：每 CPU 8-64 页

**真实基准**（HTTP 服务器负载）：
- 基线：100K 请求/秒
- 基本 tracepoints：98K (-2%)
- + BPF 聚合：95K (-5%)
- + 堆栈跟踪：85K (-15%)
- + 完整函数追踪：50K (-50%)

**指南**：
- 始终在线追踪：目标 <1% 开销
- 调查：调试期间接受 5-10%
- 避免热路径上的堆栈跟踪
- 积极使用过滤
- 优先使用 tracepoints 而非 kprobes
- 在内核中聚合（BPF）而非用户空间
