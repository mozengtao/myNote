# WHERE｜源代码地图

## 1. kernel/trace/ 目录结构

```
KERNEL/TRACE/ DIRECTORY STRUCTURE
+=============================================================================+
|                                                                              |
|  TRACING SUBSYSTEM LAYOUT                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  kernel/trace/                                                           │ |
|  │  │                                                                       │ |
|  │  │  CORE INFRASTRUCTURE                                                  │ |
|  │  ├── trace.c              ◄── Main tracing logic                        │ |
|  │  │                             tracer registration                       │ |
|  │  │                             tracing_init()                            │ |
|  │  │                                                                       │ |
|  │  ├── trace.h              ◄── Internal headers                          │ |
|  │  │                             struct tracer                             │ |
|  │  │                             struct trace_array                        │ |
|  │  │                                                                       │ |
|  │  ├── ring_buffer.c        ◄── Per-CPU ring buffer                       │ |
|  │  │                             ring_buffer_write()                       │ |
|  │  │                             ring_buffer_read()                        │ |
|  │  │                                                                       │ |
|  │  ├── trace_events.c       ◄── Trace event infrastructure                │ |
|  │  │                             event_trace_init()                        │ |
|  │  │                             trace_event_reg()                         │ |
|  │  │                                                                       │ |
|  │  ├── trace_output.c       ◄── Event formatting/printing                 │ |
|  │  │                             print_trace_line()                        │ |
|  │  │                                                                       │ |
|  │  │  TRACERS                                                              │ |
|  │  ├── trace_nop.c          ◄── No-op tracer (baseline)                   │ |
|  │  ├── trace_functions.c    ◄── Function tracer                           │ |
|  │  │                             function_trace_call()                     │ |
|  │  ├── trace_functions_graph.c  ◄── Function graph tracer                 │ |
|  │  │                                 function_graph_enter()                │ |
|  │  │                                 function_graph_return()               │ |
|  │  ├── trace_irqsoff.c      ◄── IRQ latency tracer                        │ |
|  │  ├── trace_sched_wakeup.c ◄── Wakeup latency tracer                     │ |
|  │  ├── trace_hwlat.c        ◄── Hardware latency detector                 │ |
|  │  ├── trace_osnoise.c      ◄── OS noise tracer                           │ |
|  │  │                                                                       │ |
|  │  │  FILTERS                                                              │ |
|  │  ├── trace_events_filter.c  ◄── Event filtering                         │ |
|  │  │                               parse_filter_string()                   │ |
|  │  │                               apply_event_filter()                    │ |
|  │  ├── trace_events_trigger.c ◄── Event triggers                          │ |
|  │  │                               trigger_on_match()                      │ |
|  │  │                                                                       │ |
|  │  │  DYNAMIC PROBES                                                       │ |
|  │  ├── trace_kprobe.c       ◄── Kprobe-based events                       │ |
|  │  │                             kprobe_dispatcher()                       │ |
|  │  ├── trace_uprobe.c       ◄── Uprobe-based events                       │ |
|  │  │                             uprobe_dispatcher()                       │ |
|  │  ├── trace_dynevent.c     ◄── Dynamic event management                  │ |
|  │  │                                                                       │ |
|  │  │  USER INTERFACE                                                       │ |
|  │  ├── ftrace.c             ◄── Ftrace core + mcount handling             │ |
|  │  │                             ftrace_init()                             │ |
|  │  │                             register_ftrace_function()                │ |
|  │  ├── trace_stat.c         ◄── /sys/kernel/debug/tracing/trace_stat/     │ |
|  │  ├── trace_printk.c       ◄── trace_printk() support                    │ |
|  │  │                                                                       │ |
|  │  │  BPF INTEGRATION                                                      │ |
|  │  ├── bpf_trace.c          ◄── BPF tracing integration                   │ |
|  │  │                             bpf_probe_register()                      │ |
|  │  │                                                                       │ |
|  │  │  PERF INTEGRATION                                                     │ |
|  │  ├── trace_event_perf.c   ◄── Perf event integration                    │ |
|  │  │                             perf_trace_init()                         │ |
|  │  │                                                                       │ |
|  │  └── Makefile                                                            │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  RELATED DIRECTORIES                                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  kernel/                                                                 │ |
|  │  ├── kprobes.c            ◄── Kprobes core                              │ |
|  │  │                             register_kprobe()                         │ |
|  │  │                             kprobe_handler()                          │ |
|  │  └── tracepoint.c         ◄── Tracepoint infrastructure                 │ |
|  │                               tracepoint_probe_register()                │ |
|  │                                                                          │ |
|  │  kernel/events/           ◄── Perf events subsystem                     │ |
|  │  ├── core.c               ◄── Perf core                                 │ |
|  │  └── ring_buffer.c        ◄── Perf ring buffer                          │ |
|  │                                                                          │ |
|  │  kernel/bpf/              ◄── BPF subsystem                             │ |
|  │  ├── core.c               ◄── BPF core                                  │ |
|  │  ├── verifier.c           ◄── BPF verifier                              │ |
|  │  ├── syscall.c            ◄── BPF syscall handling                      │ |
|  │  └── trampoline.c         ◄── BPF trampolines                           │ |
|  │                                                                          │ |
|  │  include/trace/events/    ◄── Tracepoint definitions                    │ |
|  │  ├── sched.h              ◄── Scheduler tracepoints                     │ |
|  │  ├── block.h              ◄── Block layer tracepoints                   │ |
|  │  ├── net.h                ◄── Network tracepoints                       │ |
|  │  ├── kmem.h               ◄── Memory tracepoints                        │ |
|  │  └── ...                                                                 │ |
|  │                                                                          │ |
|  │  include/linux/                                                          │ |
|  │  ├── tracepoint.h         ◄── Tracepoint macros                         │ |
|  │  ├── trace_events.h       ◄── Trace event structures                    │ |
|  │  ├── ftrace.h             ◄── Ftrace interface                          │ |
|  │  └── bpf.h                ◄── BPF definitions                           │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**kernel/trace/ 目录结构**：

**核心基础设施**：
- `trace.c`：主追踪逻辑、tracer 注册
- `ring_buffer.c`：Per-CPU 环形缓冲区
- `trace_events.c`：追踪事件基础设施
- `trace_output.c`：事件格式化/打印

**Tracers**：
- `trace_nop.c`：No-op tracer（基线）
- `trace_functions.c`：函数 tracer
- `trace_functions_graph.c`：函数图 tracer
- `trace_irqsoff.c`：IRQ 延迟 tracer
- `trace_hwlat.c`：硬件延迟检测器

**过滤器**：
- `trace_events_filter.c`：事件过滤
- `trace_events_trigger.c`：事件触发器

**动态探针**：
- `trace_kprobe.c`：基于 Kprobe 的事件
- `trace_uprobe.c`：基于 Uprobe 的事件

**相关目录**：
- `kernel/kprobes.c`：Kprobes 核心
- `kernel/tracepoint.c`：Tracepoint 基础设施
- `kernel/events/`：Perf 事件子系统
- `kernel/bpf/`：BPF 子系统
- `include/trace/events/`：Tracepoint 定义

---

## 2. 架构锚点：struct trace_event

```
ARCHITECTURAL ANCHORS
+=============================================================================+
|                                                                              |
|  STRUCT TRACE_ARRAY (kernel/trace/trace.h)                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // The main tracing context                                     │    │ |
|  │  │  struct trace_array {                                            │    │ |
|  │  │      struct list_head    list;        // list of trace arrays    │    │ |
|  │  │      char               *name;         // instance name          │    │ |
|  │  │      struct trace_buffer *array_buffer; // main trace buffer     │    │ |
|  │  │      struct trace_buffer *max_buffer;   // max latency buffer    │    │ |
|  │  │      struct tracer      *current_tracer; // active tracer        │    │ |
|  │  │      int                 buffer_disabled;                        │    │ |
|  │  │      struct trace_options *options;                              │    │ |
|  │  │      struct dentry      *dir;          // tracefs dir            │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Global trace array (default instance)                        │    │ |
|  │  │  static struct trace_array global_trace;                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT TRACER (kernel/trace/trace.h)                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct tracer {                                                 │    │ |
|  │  │      const char         *name;        // "function", "nop", etc  │    │ |
|  │  │      int (*init)(struct trace_array *tr);                        │    │ |
|  │  │      void (*reset)(struct trace_array *tr);                      │    │ |
|  │  │      void (*start)(struct trace_array *tr);                      │    │ |
|  │  │      void (*stop)(struct trace_array *tr);                       │    │ |
|  │  │      int (*update_thresh)(struct trace_array *tr);               │    │ |
|  │  │      ssize_t (*read)(struct trace_iterator *iter,                │    │ |
|  │  │                      struct file *filp, char __user *ubuf, ...); │    │ |
|  │  │      struct tracer    *next;          // linked list             │    │ |
|  │  │      struct tracer_flags *flags;                                 │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Available tracers linked list                                │    │ |
|  │  │  static struct tracer *trace_types;                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT TRACE_EVENT_CALL (include/linux/trace_events.h)                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // Represents a single trace event type                         │    │ |
|  │  │  struct trace_event_call {                                       │    │ |
|  │  │      struct list_head  list;                                     │    │ |
|  │  │      struct trace_event_class *class;                            │    │ |
|  │  │      union {                                                     │    │ |
|  │  │          char            *name;        // event name             │    │ |
|  │  │          struct tracepoint *tp;        // for tp events          │    │ |
|  │  │      };                                                          │    │ |
|  │  │      struct trace_event event;                                   │    │ |
|  │  │      char              *print_fmt;     // printf format          │    │ |
|  │  │      struct event_filter *filter;      // filter expression      │    │ |
|  │  │      void              *mod;           // module owner           │    │ |
|  │  │      int                flags;                                   │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Event flags                                                  │    │ |
|  │  │  #define TRACE_EVENT_FL_FILTERED     (1 << 0)                    │    │ |
|  │  │  #define TRACE_EVENT_FL_TRACEPOINT   (1 << 1)                    │    │ |
|  │  │  #define TRACE_EVENT_FL_KPROBE       (1 << 2)                    │    │ |
|  │  │  #define TRACE_EVENT_FL_UPROBE       (1 << 3)                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT TRACEPOINT (include/linux/tracepoint.h)                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct tracepoint {                                             │    │ |
|  │  │      const char      *name;            // tp name                │    │ |
|  │  │      struct static_key key;            // for NOP→JMP patching   │    │ |
|  │  │      struct tracepoint_func __rcu *funcs; // registered probes   │    │ |
|  │  │      int (*regfunc)(void);             // register callback      │    │ |
|  │  │      void (*unregfunc)(void);          // unregister callback    │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Probe function holder                                        │    │ |
|  │  │  struct tracepoint_func {                                        │    │ |
|  │  │      void *func;                       // the probe function     │    │ |
|  │  │      void *data;                       // private data           │    │ |
|  │  │      int prio;                         // priority               │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**架构锚点**：

**struct trace_array**：
- 主追踪上下文
- list、name、array_buffer、current_tracer、dir
- 全局追踪数组：global_trace

**struct tracer**：
- name（"function"、"nop" 等）
- init、reset、start、stop、read 回调
- trace_types：可用 tracers 链表

**struct trace_event_call**：
- 表示单个追踪事件类型
- list、class、name/tp、event、print_fmt、filter
- 标志：FILTERED、TRACEPOINT、KPROBE、UPROBE

**struct tracepoint**：
- name、key（用于 NOP→JMP 修补）
- funcs（注册的探针）
- regfunc、unregfunc 回调

---

## 3. 控制中心：tracepoint_probe_register()

```
CONTROL HUBS
+=============================================================================+
|                                                                              |
|  TRACEPOINT_PROBE_REGISTER (kernel/tracepoint.c)                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  int tracepoint_probe_register(struct tracepoint *tp,            │    │ |
|  │  │                                void *probe, void *data) {        │    │ |
|  │  │      // 1. Allocate new func array with room for probe           │    │ |
|  │  │      // 2. Copy existing probes                                  │    │ |
|  │  │      // 3. Add new probe                                         │    │ |
|  │  │      // 4. RCU-swap old array with new                           │    │ |
|  │  │      // 5. If first probe, enable tracepoint                     │    │ |
|  │  │      //    → static_key_enable()                                 │    │ |
|  │  │      //    → patches NOP to JMP                                  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Called by:                                                      │    │ |
|  │  │  • ftrace when enabling tracepoints                              │    │ |
|  │  │  • perf when attaching to tracepoints                            │    │ |
|  │  │  • BPF when loading tracepoint programs                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  RING_BUFFER_WRITE (kernel/trace/ring_buffer.c)                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  int ring_buffer_write(struct trace_buffer *buffer,              │    │ |
|  │  │                        unsigned long length,                     │    │ |
|  │  │                        void *data) {                             │    │ |
|  │  │      // Hot path - called for every event                        │    │ |
|  │  │                                                                  │    │ |
|  │  │      cpu_buffer = per_cpu_ptr(buffer, cpu);                      │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Reserve space (lockless for this CPU)                    │    │ |
|  │  │      event = rb_reserve_next_event(cpu_buffer, length);          │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Copy data                                                │    │ |
|  │  │      memcpy(rb_event_data(event), data, length);                 │    │ |
|  │  │                                                                  │    │ |
|  │  │      // Commit (makes visible to readers)                        │    │ |
|  │  │      rb_commit(cpu_buffer, event);                               │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Called by:                                                      │    │ |
|  │  │  • Every tracepoint when enabled                                 │    │ |
|  │  │  • Function tracer                                               │    │ |
|  │  │  • All tracers that write events                                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  OTHER KEY CONTROL HUBS                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  REGISTER_KPROBE (kernel/kprobes.c)                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  int register_kprobe(struct kprobe *p) {                         │    │ |
|  │  │      // 1. Verify probe location is valid                        │    │ |
|  │  │      // 2. Save original instruction                             │    │ |
|  │  │      // 3. Replace with int3 (breakpoint)                        │    │ |
|  │  │      // 4. Add to kprobe hash table                              │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Called when:                                                    │    │ |
|  │  │  • bpftrace attaches to kernel function                          │    │ |
|  │  │  • perf probe creates new probe                                  │    │ |
|  │  │  • trace_kprobe.c creates kprobe event                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  REGISTER_FTRACE_FUNCTION (kernel/trace/ftrace.c)                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  int register_ftrace_function(struct ftrace_ops *ops) {          │    │ |
|  │  │      // Register callback for function tracing                   │    │ |
|  │  │      // Modifies mcount call sites                               │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Called when:                                                    │    │ |
|  │  │  • Function tracer enabled                                       │    │ |
|  │  │  • Function graph tracer enabled                                 │    │ |
|  │  │  • BPF fentry/fexit programs attached                            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  BPF_PROG_LOAD (kernel/bpf/syscall.c)                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  static int bpf_prog_load(union bpf_attr *attr) {                │    │ |
|  │  │      // 1. Parse BPF bytecode                                    │    │ |
|  │  │      // 2. Run verifier                                          │    │ |
|  │  │      // 3. JIT compile to native                                 │    │ |
|  │  │      // 4. Create file descriptor                                │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Then attach via:                                                │    │ |
|  │  │  • bpf_program__attach_kprobe()                                  │    │ |
|  │  │  • bpf_program__attach_tracepoint()                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**控制中心**：

**tracepoint_probe_register()**：
- 分配新 func 数组
- 复制现有探针，添加新探针
- RCU 交换旧数组
- 如果是第一个探针，启用 tracepoint（NOP→JMP）
- 调用者：ftrace、perf、BPF

**ring_buffer_write()**：
- 热路径 - 每个事件都调用
- Per-CPU 无锁保留空间
- 复制数据，提交
- 调用者：所有写事件的 tracers

**register_kprobe()**：
- 验证探针位置
- 保存原始指令
- 替换为 int3
- 添加到 kprobe 哈希表

**register_ftrace_function()**：
- 注册函数追踪回调
- 修改 mcount 调用站点

**bpf_prog_load()**：
- 解析 BPF 字节码
- 运行验证器
- JIT 编译为本机代码

---

## 4. 阅读策略

```
READING STRATEGY
+=============================================================================+
|                                                                              |
|  RECOMMENDED READING ORDER                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  LEVEL 1: TRACEPOINT BASICS                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. include/linux/tracepoint.h                                   │    │ |
|  │  │     • struct tracepoint                                          │    │ |
|  │  │     • DECLARE_TRACE, DEFINE_TRACE macros                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. include/trace/events/sched.h                                 │    │ |
|  │  │     • Example: TRACE_EVENT(sched_switch, ...)                    │    │ |
|  │  │     • See how events are defined                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. kernel/tracepoint.c                                          │    │ |
|  │  │     • tracepoint_probe_register()                                │    │ |
|  │  │     • How probes are attached                                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LEVEL 2: RING BUFFER                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  4. kernel/trace/ring_buffer.c (first 500 lines)                 │    │ |
|  │  │     • struct ring_buffer_per_cpu                                 │    │ |
|  │  │     • ring_buffer_write()                                        │    │ |
|  │  │     • Lockless design                                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  5. kernel/trace/trace.c (trace_init, basic flow)                │    │ |
|  │  │     • struct trace_array                                         │    │ |
|  │  │     • How tracers are registered                                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LEVEL 3: TRACERS                                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  6. kernel/trace/trace_functions.c                               │    │ |
|  │  │     • Simple tracer example                                      │    │ |
|  │  │     • function_trace_call()                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  7. kernel/trace/ftrace.c                                        │    │ |
|  │  │     • How mcount works                                           │    │ |
|  │  │     • Dynamic patching                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  8. kernel/trace/trace_kprobe.c                                  │    │ |
|  │  │     • Dynamic kprobe events                                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LEVEL 4: DYNAMIC PROBES                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  9. kernel/kprobes.c                                             │    │ |
|  │  │      • register_kprobe()                                         │    │ |
|  │  │      • kprobe_handler()                                          │    │ |
|  │  │      • int3 handling                                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  10. kernel/events/uprobes.c                                     │    │ |
|  │  │      • User-space probing                                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LEVEL 5: BPF INTEGRATION                                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  11. kernel/trace/bpf_trace.c                                    │    │ |
|  │  │      • BPF tracing integration                                   │    │ |
|  │  │      • How BPF attaches to tracepoints                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  12. kernel/bpf/verifier.c (overview)                            │    │ |
|  │  │      • How programs are verified                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  13. kernel/bpf/trampoline.c                                     │    │ |
|  │  │      • BPF trampolines for fentry/fexit                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**推荐阅读顺序**：

**第 1 层：Tracepoint 基础**
1. `include/linux/tracepoint.h`：struct tracepoint、宏
2. `include/trace/events/sched.h`：事件定义示例
3. `kernel/tracepoint.c`：探针如何附加

**第 2 层：环形缓冲区**
4. `kernel/trace/ring_buffer.c`：无锁设计
5. `kernel/trace/trace.c`：tracer 注册

**第 3 层：Tracers**
6. `kernel/trace/trace_functions.c`：简单 tracer 示例
7. `kernel/trace/ftrace.c`：mcount 如何工作
8. `kernel/trace/trace_kprobe.c`：动态 kprobe 事件

**第 4 层：动态探针**
9. `kernel/kprobes.c`：register_kprobe()、int3 处理
10. `kernel/events/uprobes.c`：用户空间探测

**第 5 层：BPF 集成**
11. `kernel/trace/bpf_trace.c`：BPF 追踪集成
12. `kernel/bpf/verifier.c`：程序如何验证
13. `kernel/bpf/trampoline.c`：fentry/fexit 蹦床

---

## 5. 验证方法

```
VALIDATION APPROACH
+=============================================================================+
|                                                                              |
|  METHOD 1: TRACEFS INTERFACE                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Mount tracefs (usually auto-mounted)                                  │ |
|  │  mount -t tracefs nodev /sys/kernel/debug/tracing                        │ |
|  │                                                                          │ |
|  │  # List available tracers                                                │ |
|  │  cat /sys/kernel/debug/tracing/available_tracers                         │ |
|  │  → function function_graph nop                                           │ |
|  │                                                                          │ |
|  │  # List available events                                                 │ |
|  │  ls /sys/kernel/debug/tracing/events/                                    │ |
|  │  → sched/ block/ net/ syscalls/ ...                                      │ |
|  │                                                                          │ |
|  │  # Enable a tracepoint                                                   │ |
|  │  echo 1 > /sys/kernel/debug/tracing/events/sched/sched_switch/enable     │ |
|  │                                                                          │ |
|  │  # Read trace output                                                     │ |
|  │  cat /sys/kernel/debug/tracing/trace_pipe                                │ |
|  │                                                                          │ |
|  │  # See event format                                                      │ |
|  │  cat /sys/kernel/debug/tracing/events/sched/sched_switch/format          │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 2: TRACE-CMD                                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Record scheduler events                                               │ |
|  │  trace-cmd record -e sched                                               │ |
|  │                                                                          │ |
|  │  # Record with function tracing                                          │ |
|  │  trace-cmd record -p function -l 'vfs_*'                                 │ |
|  │                                                                          │ |
|  │  # View recorded data                                                    │ |
|  │  trace-cmd report                                                        │ |
|  │                                                                          │ |
|  │  # Function graph                                                        │ |
|  │  trace-cmd record -p function_graph -g do_sys_open                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 3: PERF                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # List tracepoints                                                      │ |
|  │  perf list tracepoint                                                    │ |
|  │                                                                          │ |
|  │  # Record events                                                         │ |
|  │  perf record -e sched:sched_switch -a sleep 5                            │ |
|  │                                                                          │ |
|  │  # View recorded data                                                    │ |
|  │  perf script                                                             │ |
|  │                                                                          │ |
|  │  # Profile with stack traces                                             │ |
|  │  perf record -g -e sched:sched_switch                                    │ |
|  │  perf report                                                             │ |
|  │                                                                          │ |
|  │  # Create dynamic kprobe                                                 │ |
|  │  perf probe --add 'do_sys_open filename:string'                          │ |
|  │  perf record -e probe:do_sys_open -a sleep 5                             │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 4: BPFTRACE                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # One-liner: count syscalls by process                                  │ |
|  │  bpftrace -e 'tracepoint:raw_syscalls:sys_enter { @[comm] = count(); }' │ |
|  │                                                                          │ |
|  │  # Kprobe: trace vfs_read with arguments                                 │ |
|  │  bpftrace -e 'kprobe:vfs_read { printf("%s read %d bytes\n",             │ |
|  │               comm, arg2); }'                                            │ |
|  │                                                                          │ |
|  │  # Histogram of read latency                                             │ |
|  │  bpftrace -e 'kprobe:vfs_read { @start[tid] = nsecs; }                   │ |
|  │               kretprobe:vfs_read /@start[tid]/ {                         │ |
|  │                   @ns = hist(nsecs - @start[tid]);                       │ |
|  │                   delete(@start[tid]);                                   │ |
|  │               }'                                                         │ |
|  │                                                                          │ |
|  │  # List available probes                                                 │ |
|  │  bpftrace -l 'tracepoint:sched:*'                                        │ |
|  │  bpftrace -l 'kprobe:vfs_*'                                              │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 5: VERIFY TRACING OVERHEAD                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Check buffer size                                                     │ |
|  │  cat /sys/kernel/debug/tracing/buffer_size_kb                            │ |
|  │                                                                          │ |
|  │  # Check for dropped events                                              │ |
|  │  cat /sys/kernel/debug/tracing/stats                                     │ |
|  │                                                                          │ |
|  │  # Check per-CPU stats                                                   │ |
|  │  cat /sys/kernel/debug/tracing/per_cpu/cpu0/stats                        │ |
|  │  → entries: 1234                                                         │ |
|  │  → overrun: 0                                                            │ |
|  │  → commit overrun: 0                                                     │ |
|  │                                                                          │ |
|  │  # Benchmark with perf                                                   │ |
|  │  perf stat -e cycles -- ./my_workload                                    │ |
|  │  # Then enable tracing and compare                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**验证方法**：

**方法 1：tracefs 接口**
- 列出可用 tracers 和事件
- 启用 tracepoint
- 读取 trace 输出
- 查看事件格式

**方法 2：trace-cmd**
- 记录调度器事件
- 函数追踪
- 函数图

**方法 3：perf**
- 列出 tracepoints
- 记录事件
- 带堆栈跟踪的分析
- 创建动态 kprobe

**方法 4：bpftrace**
- 一行命令：按进程计数系统调用
- kprobe：追踪 vfs_read 带参数
- 读取延迟直方图
- 列出可用探针

**方法 5：验证追踪开销**
- 检查缓冲区大小
- 检查丢弃的事件
- per-CPU 统计
- 用 perf 基准测试
