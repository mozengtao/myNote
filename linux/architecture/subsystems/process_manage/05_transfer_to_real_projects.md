# TRANSFER｜将思想应用到实际项目

## 1. 如何在用户空间系统中应用调度器风格的抽象

```
APPLYING SCHEDULER-STYLE ABSTRACTIONS IN USER-SPACE
+=============================================================================+
|                                                                              |
|  PATTERN 1: PLUGGABLE POLICY VIA OPS-TABLE (可插拔策略通过操作表)             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux kernel:                                                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct sched_class {                                            │    │ |
|  │  │      void (*enqueue_task)(rq, task);                             │    │ |
|  │  │      task_struct *(*pick_next_task)(rq);                         │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  task->sched_class->pick_next_task(rq);  /* Dispatch */          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  User-space equivalent (C):                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Task scheduler for a thread pool or job queue */             │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct scheduler_ops {                                          │    │ |
|  │  │      void (*enqueue)(struct scheduler *s, struct job *j);        │    │ |
|  │  │      struct job *(*pick_next)(struct scheduler *s);              │    │ |
|  │  │      void (*job_complete)(struct scheduler *s, struct job *j);   │    │ |
|  │  │      const char *name;                                           │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* FIFO implementation */                                       │    │ |
|  │  │  static struct scheduler_ops fifo_ops = {                        │    │ |
|  │  │      .enqueue = fifo_enqueue,                                    │    │ |
|  │  │      .pick_next = fifo_pick_next,                                │    │ |
|  │  │      .job_complete = fifo_complete,                              │    │ |
|  │  │      .name = "fifo",                                             │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Priority-based implementation */                             │    │ |
|  │  │  static struct scheduler_ops priority_ops = {                    │    │ |
|  │  │      .enqueue = priority_enqueue,                                │    │ |
|  │  │      .pick_next = priority_pick_next,                            │    │ |
|  │  │      .job_complete = priority_complete,                          │    │ |
|  │  │      .name = "priority",                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Scheduler uses ops table */                                  │    │ |
|  │  │  struct scheduler {                                              │    │ |
|  │  │      const struct scheduler_ops *ops;                            │    │ |
|  │  │      void *private_data;  /* Implementation-specific */          │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct job *scheduler_get_next(struct scheduler *s) {           │    │ |
|  │  │      return s->ops->pick_next(s);                                │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  C++ equivalent:                                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  class IScheduler {                                              │    │ |
|  │  │  public:                                                         │    │ |
|  │  │      virtual void enqueue(Job* job) = 0;                         │    │ |
|  │  │      virtual Job* pick_next() = 0;                               │    │ |
|  │  │      virtual void job_complete(Job* job) = 0;                    │    │ |
|  │  │      virtual ~IScheduler() = default;                            │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  class FifoScheduler : public IScheduler { ... };                │    │ |
|  │  │  class PriorityScheduler : public IScheduler { ... };            │    │ |
|  │  │  class FairScheduler : public IScheduler { ... };                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 2: EMBEDDED SCHEDULING ENTITY (嵌入式调度实体)                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux kernel:                                                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct task_struct {                                            │    │ |
|  │  │      ...                                                         │    │ |
|  │  │      struct sched_entity se;    /* Embedded, not pointer */      │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Can get task from entity */                                  │    │ |
|  │  │  #define task_of(se) container_of(se, struct task_struct, se)    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  User-space equivalent:                                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* The scheduling entity - scheduler only sees this */          │    │ |
|  │  │  struct sched_node {                                             │    │ |
|  │  │      struct rb_node rb;        /* For red-black tree */          │    │ |
|  │  │      uint64_t vruntime;        /* Virtual runtime */             │    │ |
|  │  │      uint64_t weight;          /* Priority weight */             │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* The actual job - application sees this */                    │    │ |
|  │  │  struct my_job {                                                 │    │ |
|  │  │      char *name;                                                 │    │ |
|  │  │      void (*execute)(struct my_job *);                           │    │ |
|  │  │      void *user_data;                                            │    │ |
|  │  │                                                                  │    │ |
|  │  │      struct sched_node sched;  /* Embedded scheduling info */    │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Scheduler works with sched_node, gets job via container_of */│    │ |
|  │  │  #define job_of(node) container_of(node, struct my_job, sched)   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Benefit: Separation of concerns                                         │ |
|  │  • Scheduler only knows about sched_node                                │ |
|  │  • Job structure can vary without changing scheduler                    │ |
|  │  • No extra allocation for scheduling metadata                          │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 3: PER-WORKER DATA (per-Worker 数据)                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux kernel: per-CPU run queues                                        │ |
|  │                                                                          │ |
|  │  User-space: per-thread/per-worker job queues                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct worker {                                                 │    │ |
|  │  │      pthread_t thread;                                           │    │ |
|  │  │      struct job_queue local_queue;  /* Per-worker queue */       │    │ |
|  │  │      pthread_mutex_t lock;          /* Local lock only */        │    │ |
|  │  │      int id;                                                     │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct thread_pool {                                            │    │ |
|  │  │      struct worker workers[NUM_WORKERS];                         │    │ |
|  │  │      struct job_queue global_queue;  /* Overflow/stealing */     │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Worker primarily works on local queue */                     │    │ |
|  │  │  void worker_run(struct worker *w) {                             │    │ |
|  │  │      while (running) {                                           │    │ |
|  │  │          struct job *j = queue_pop(&w->local_queue);             │    │ |
|  │  │          if (!j) j = try_steal_from_others();                    │    │ |
|  │  │          if (j) execute(j);                                      │    │ |
|  │  │      }                                                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Benefits:                                                               │ |
|  │  • Minimal lock contention (each worker has own lock)                   │ |
|  │  • Cache locality (worker keeps working on same jobs)                   │ |
|  │  • Work stealing handles imbalance                                      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**模式 1：通过操作表实现可插拔策略**
- Linux 内核使用 `sched_class` 结构定义调度策略接口
- 用户空间等价：定义 `scheduler_ops` 结构，包含函数指针
- C++ 等价：使用虚函数接口 `IScheduler`
- 好处：可以在运行时切换策略，无需修改核心逻辑

**模式 2：嵌入式调度实体**
- Linux 内核：`task_struct` 嵌入 `sched_entity`，通过 `container_of` 获取任务
- 用户空间：将 `sched_node` 嵌入到 `my_job` 结构中
- 好处：关注点分离，无需为调度元数据额外分配内存

**模式 3：per-Worker 数据**
- Linux 内核：per-CPU 运行队列
- 用户空间：per-线程/per-worker 作业队列
- 好处：最小锁争用、缓存局部性、工作窃取处理不平衡

---

## 2. 基于公平性的调度适用场景

```
WHERE FAIRNESS-BASED SCHEDULING WORKS WELL
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  SCENARIO 1: MULTI-TENANT SYSTEMS (多租户系统)                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem: Multiple tenants share same infrastructure             │    │ |
|  │  │           One tenant shouldn't starve others                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Tenant A ─────────────┐                                  │   │    │ |
|  │  │  │  (weight: 100)         │                                  │   │    │ |
|  │  │  │                        ▼                                  │   │    │ |
|  │  │  │  Tenant B ───────► FAIR SCHEDULER ───► Resources          │   │    │ |
|  │  │  │  (weight: 200)         ▲                                  │   │    │ |
|  │  │  │                        │                                  │   │    │ |
|  │  │  │  Tenant C ─────────────┘                                  │   │    │ |
|  │  │  │  (weight: 100)                                            │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Result: B gets 50%, A and C get 25% each                 │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Examples:                                                       │    │ |
|  │  │  • Cloud providers (AWS, GCP) - fair share of compute           │    │ |
|  │  │  • SaaS platforms - fair access to API rate limits              │    │ |
|  │  │  • Database systems - fair query processing time                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  SCENARIO 2: REQUEST PROCESSING WITH MIXED WORKLOADS                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem: Some requests are CPU-heavy, some are quick            │    │ |
|  │  │           Don't want heavy requests to block quick ones          │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Heavy request (runs 100ms) ─────┐                        │   │    │ |
|  │  │  │       │                           │                       │   │    │ |
|  │  │  │       │ vruntime grows fast       ▼                       │   │    │ |
|  │  │  │       │                      FAIR SCHEDULER               │   │    │ |
|  │  │  │       │                           ▲                       │   │    │ |
|  │  │  │       │                           │                       │   │    │ |
|  │  │  │  Quick request (runs 1ms) ────────┘                       │   │    │ |
|  │  │  │       │                                                   │   │    │ |
|  │  │  │       │ vruntime grows slowly                             │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Result: Quick requests get scheduled frequently          │   │    │ |
|  │  │  │          Heavy requests make progress but don't block     │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Examples:                                                       │    │ |
|  │  │  • Web servers with mixed API endpoints                          │    │ |
|  │  │  • Message queues with varying message sizes                     │    │ |
|  │  │  • Search engines with simple vs complex queries                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  SCENARIO 3: BACKGROUND VS FOREGROUND WORK                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem: Background jobs shouldn't impact user-facing work      │    │ |
|  │  │           But background jobs still need to make progress        │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  User requests (weight: 1000) ──────┐                     │   │    │ |
|  │  │  │       │                              │                    │   │    │ |
|  │  │  │       │ Low vruntime growth          ▼                    │   │    │ |
|  │  │  │       │ (high priority)        FAIR SCHEDULER             │   │    │ |
|  │  │  │       │                              ▲                    │   │    │ |
|  │  │  │       │                              │                    │   │    │ |
|  │  │  │  Background jobs (weight: 100) ──────┘                    │   │    │ |
|  │  │  │       │                                                   │   │    │ |
|  │  │  │       │ High vruntime growth                              │   │    │ |
|  │  │  │       │ (low priority)                                    │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Result: User requests get 90% of resources               │   │    │ |
|  │  │  │          Background gets 10% but never starves            │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Examples:                                                       │    │ |
|  │  │  • Index rebuilding vs search queries                           │    │ |
|  │  │  • Log processing vs API requests                               │    │ |
|  │  │  • Data analytics vs real-time processing                       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  IMPLEMENTATION SKETCH:                                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct fair_scheduler {                                         │    │ |
|  │  │      struct rb_root tasks;       /* Red-black tree */            │    │ |
|  │  │      uint64_t min_vruntime;      /* Monotonic clock */           │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  void update_vruntime(struct job *j, uint64_t runtime_ns) {      │    │ |
|  │  │      /* Weight 1000 = baseline, higher = slower vruntime */      │    │ |
|  │  │      j->vruntime += runtime_ns * 1000 / j->weight;               │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct job *pick_next(struct fair_scheduler *s) {               │    │ |
|  │  │      /* Leftmost node has smallest vruntime */                   │    │ |
|  │  │      return rb_entry(rb_first(&s->tasks), struct job, rb_node);  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**基于公平性的调度适用场景**：

**场景 1：多租户系统**
- 问题：多个租户共享同一基础设施，一个租户不应饿死其他租户
- 解决方案：使用权重分配资源（如租户 B 权重 200 获得 50%，A 和 C 各 25%）
- 例子：云提供商、SaaS 平台、数据库系统

**场景 2：混合工作负载的请求处理**
- 问题：重型请求（100ms）不应阻塞快速请求（1ms）
- 解决方案：重型请求 vruntime 增长快，快速请求经常被调度
- 例子：Web 服务器、消息队列、搜索引擎

**场景 3：后台 vs 前台工作**
- 问题：后台作业不应影响用户请求，但仍需进展
- 解决方案：用户请求高权重（1000），后台作业低权重（100）
- 例子：索引重建 vs 搜索查询，日志处理 vs API 请求

---

## 3. 何时简单队列更好

```
WHEN SIMPLER QUEUES ARE BETTER
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  CASE 1: HOMOGENEOUS WORK UNITS (同质工作单元)                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  If all jobs are similar in cost, FIFO is often best:            │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Job A (10ms) ─┐                                          │   │    │ |
|  │  │  │  Job B (10ms) ─┼──► FIFO Queue ──► Worker                 │   │    │ |
|  │  │  │  Job C (10ms) ─┘                                          │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  No scheduling overhead - just dequeue and run            │   │    │ |
|  │  │  │  Perfect for: image thumbnailing, video transcoding       │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  CASE 2: STRICT ORDERING REQUIREMENTS (严格顺序要求)                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  If jobs MUST complete in order:                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Event 1 ──► Event 2 ──► Event 3 ──► ...                  │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Each event depends on previous one                       │   │    │ |
|  │  │  │  Reordering would break causality                         │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Perfect for: event sourcing, log processing              │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  CASE 3: LOW OVERHEAD REQUIREMENTS (低开销要求)                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Comparison:                                                     │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  FIFO Queue:                                              │   │    │ |
|  │  │  │  • Enqueue: O(1), ~10ns                                   │   │    │ |
|  │  │  │  • Dequeue: O(1), ~10ns                                   │   │    │ |
|  │  │  │  • Simple atomic operations                               │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Red-Black Tree (CFS-style):                              │   │    │ |
|  │  │  │  • Insert: O(log n), ~100-500ns                           │   │    │ |
|  │  │  │  • Delete: O(log n), ~100-500ns                           │   │    │ |
|  │  │  │  • Tree rebalancing overhead                              │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  When jobs are very short (<1µs), scheduling overhead     │   │    │ |
|  │  │  │  can dominate execution time                              │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  CASE 4: REAL-TIME WITH HARD DEADLINES (硬截止时间的实时)                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Priority queue often better than fair scheduler:                │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Priority 0 (highest): Emergency stop                     │   │    │ |
|  │  │  │  Priority 1: Safety checks                                │   │    │ |
|  │  │  │  Priority 2: Control loop                                 │   │    │ |
|  │  │  │  Priority 3: Logging                                      │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Fairness would be WRONG here - safety must preempt       │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Perfect for: robotics, industrial control, audio         │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  DECISION GUIDE:                                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Use FIFO when:                                                  │    │ |
|  │  │  • Jobs are homogeneous                                          │    │ |
|  │  │  • Order matters                                                 │    │ |
|  │  │  • Minimum overhead needed                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Use Priority Queue when:                                        │    │ |
|  │  │  • Clear priority levels exist                                   │    │ |
|  │  │  • Starvation of low priority is acceptable                      │    │ |
|  │  │  • Hard real-time requirements                                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Use Fair Scheduler when:                                        │    │ |
|  │  │  • Multiple independent users/tenants                            │    │ |
|  │  │  • Jobs vary in cost                                             │    │ |
|  │  │  • No starvation allowed                                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**何时简单队列更好**：

**情况 1：同质工作单元**
- 如果所有作业成本相似，FIFO 通常最好
- 无调度开销 - 只需出队并运行
- 适用于：图像缩略图、视频转码

**情况 2：严格顺序要求**
- 如果作业必须按顺序完成
- 重新排序会破坏因果关系
- 适用于：事件溯源、日志处理

**情况 3：低开销要求**
- FIFO：入队/出队 O(1)，~10ns
- 红黑树（CFS 风格）：插入/删除 O(log n)，~100-500ns
- 当作业非常短（<1µs）时，调度开销可能占主导

**情况 4：硬截止时间的实时**
- 优先级队列通常比公平调度器更好
- 公平性在这里是错误的 - 安全必须抢占
- 适用于：机器人、工业控制、音频

**决策指南**：
- FIFO：同质作业、顺序重要、最小开销
- 优先级队列：明确的优先级级别、可接受低优先级饥饿、硬实时
- 公平调度器：多租户、作业成本变化、不允许饥饿

---

## 4. 复制内核调度思想时的常见错误

```
COMMON MISTAKES WHEN COPYING KERNEL SCHEDULING IDEAS
+=============================================================================+
|                                                                              |
|  MISTAKE 1: OVER-ENGINEERING FOR SIMPLE WORKLOADS (过度工程)                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  BAD: Implementing full CFS for a simple task queue              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // 500 lines of red-black tree, vruntime calculation, etc.      │    │ |
|  │  │  struct cfs_scheduler *create_cfs_scheduler() { ... }            │    │ |
|  │  │                                                                  │    │ |
|  │  │  // When all you needed was:                                     │    │ |
|  │  │  while (job = queue_pop(&jobs)) {                                │    │ |
|  │  │      process(job);                                               │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  ─────────────────────────────────────────────────────────────   │    │ |
|  │  │                                                                  │    │ |
|  │  │  GOOD: Match complexity to actual problem                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  • 10 jobs/second? → FIFO queue                                  │    │ |
|  │  │  • 1000 jobs/second with priorities? → Priority queue           │    │ |
|  │  │  • 100K jobs/second, multi-tenant? → Consider CFS-style          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MISTAKE 2: IGNORING THE COST OF CONTEXT SWITCHING (忽略上下文切换成本)      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  BAD: Very short time slices in user-space scheduler             │    │ |
|  │  │                                                                  │    │ |
|  │  │  // "I'll preempt every 100µs like the kernel does"              │    │ |
|  │  │  while (1) {                                                     │    │ |
|  │  │      job = pick_next_job();                                      │    │ |
|  │  │      run_for_microseconds(job, 100);  // VERY short              │    │ |
|  │  │      save_context(job);                                          │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem: Context save/restore in user-space is EXPENSIVE        │    │ |
|  │  │           Kernel has hardware support, you don't                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  ─────────────────────────────────────────────────────────────   │    │ |
|  │  │                                                                  │    │ |
|  │  │  GOOD: Run jobs to completion or natural yield points            │    │ |
|  │  │                                                                  │    │ |
|  │  │  while (1) {                                                     │    │ |
|  │  │      job = pick_next_job();                                      │    │ |
|  │  │      job->execute();  // Run to completion                       │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Or use cooperative scheduling:                                  │    │ |
|  │  │  void job_execute(struct job *j) {                               │    │ |
|  │  │      for (int i = 0; i < 1000; i++) {                            │    │ |
|  │  │          do_work();                                              │    │ |
|  │  │          if (should_yield()) yield_to_scheduler();               │    │ |
|  │  │      }                                                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MISTAKE 3: MISUNDERSTANDING PREEMPTION (误解抢占)                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  BAD: Trying to implement true preemption in user-space          │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Using signals to "preempt" threads                           │    │ |
|  │  │  void timer_handler(int sig) {                                   │    │ |
|  │  │      // Try to switch contexts here                              │    │ |
|  │  │      longjmp(scheduler_context, 1);  // DANGER!                  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem:                                                        │    │ |
|  │  │  • Signal handlers have severe restrictions                      │    │ |
|  │  │  • longjmp from signal handler is undefined behavior             │    │ |
|  │  │  • Can corrupt locks, I/O, memory allocators                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  ─────────────────────────────────────────────────────────────   │    │ |
|  │  │                                                                  │    │ |
|  │  │  GOOD: Use cooperative yielding or real OS threads               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Cooperative: job voluntarily yields                          │    │ |
|  │  │  void expensive_operation() {                                    │    │ |
|  │  │      for (int i = 0; i < N; i++) {                               │    │ |
|  │  │          work();                                                 │    │ |
|  │  │          if (i % 1000 == 0) scheduler_yield();                   │    │ |
|  │  │      }                                                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // Or: Use actual OS threads, let kernel handle preemption      │    │ |
|  │  │  for (int i = 0; i < NUM_WORKERS; i++) {                         │    │ |
|  │  │      pthread_create(&workers[i], NULL, worker_func, NULL);       │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MISTAKE 4: COPYING COMPLEXITY WITHOUT UNDERSTANDING WHY (盲目复制复杂性)    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  WHY does CFS use a red-black tree?                              │    │ |
|  │  │  • 1000s of tasks                                                │    │ |
|  │  │  • Frequent insert/delete                                        │    │ |
|  │  │  • Need O(log n) worst case                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  Your situation:                                                 │    │ |
|  │  │  • 10 jobs?  → Linear scan is faster (cache-friendly)            │    │ |
|  │  │  • 100 jobs? → Heap might be simpler                             │    │ |
|  │  │  • Rarely reorder? → Sorted list might work                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  ─────────────────────────────────────────────────────────────   │    │ |
|  │  │                                                                  │    │ |
|  │  │  WHY does the kernel have per-CPU run queues?                    │    │ |
|  │  │  • 64+ CPUs                                                      │    │ |
|  │  │  • Global lock would be contended                                │    │ |
|  │  │  • NUMA locality matters                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  Your situation:                                                 │    │ |
|  │  │  • 4 threads? → Global queue with lock is probably fine          │    │ |
|  │  │  • Lock-free queue might be simpler than work-stealing           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MISTAKE 5: FORGETTING USER-SPACE LIMITATIONS (忘记用户空间限制)             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Things kernel can do that you CANNOT:                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  ✗ Disable interrupts                                            │    │ |
|  │  │  ✗ True preemption at any point                                  │    │ |
|  │  │  ✗ Direct CPU affinity control                                   │    │ |
|  │  │  ✗ Low-level timer interrupts (sub-ms)                           │    │ |
|  │  │  ✗ Atomic context switches (hardware support)                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Things you CAN do:                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  ✓ Cooperative scheduling (yield points)                         │    │ |
|  │  │  ✓ Thread pools with job queues                                  │    │ |
|  │  │  ✓ Priority-based dispatch                                       │    │ |
|  │  │  ✓ Fair scheduling algorithms                                    │    │ |
|  │  │  ✓ Load balancing between threads                                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SUMMARY: WHAT TO COPY, WHAT TO ADAPT                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  COPY DIRECTLY:                                                          │ |
|  │  ✓ Ops-table pattern for pluggable policies                             │ |
|  │  ✓ Embedded scheduling entity pattern                                   │ |
|  │  ✓ Per-worker data structures concept                                   │ |
|  │  ✓ Separation of mechanism and policy                                   │ |
|  │  ✓ vruntime concept for fairness                                        │ |
|  │                                                                          │ |
|  │  ADAPT TO YOUR SCALE:                                                    │ |
|  │  ~ Data structures (RB-tree → heap → array)                             │ |
|  │  ~ Time slice granularity (µs in kernel → ms or job-completion)         │ |
|  │  ~ Number of queues (per-CPU → per-thread → global)                     │ |
|  │                                                                          │ |
|  │  DON'T COPY:                                                             │ |
|  │  ✗ Signal-based preemption hacks                                        │ |
|  │  ✗ Complex load balancing for small thread counts                       │ |
|  │  ✗ Kernel-specific optimizations (lazy TLB, etc.)                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**复制内核调度思想时的常见错误**：

**错误 1：过度工程**
- 错误：为简单任务队列实现完整 CFS
- 正确：匹配复杂度与实际问题（10 作业/秒 → FIFO，100K 作业/秒多租户 → 考虑 CFS）

**错误 2：忽略上下文切换成本**
- 错误：在用户空间调度器中使用非常短的时间片（100µs）
- 问题：用户空间的上下文保存/恢复非常昂贵，内核有硬件支持
- 正确：运行作业到完成或使用协作调度

**错误 3：误解抢占**
- 错误：尝试在用户空间实现真正的抢占（使用信号）
- 问题：信号处理程序有严格限制，longjmp 是未定义行为
- 正确：使用协作让出或真正的 OS 线程

**错误 4：盲目复制复杂性**
- 为什么 CFS 使用红黑树？1000+ 任务，频繁插入/删除
- 你的情况：10 作业？线性扫描更快；100 作业？堆可能更简单

**错误 5：忘记用户空间限制**
- 内核能做而你不能：禁用中断、真正抢占、直接 CPU 亲和性控制
- 你能做的：协作调度、线程池、优先级分发、公平调度算法

**总结**：
- **直接复制**：操作表模式、嵌入式调度实体、per-worker 数据结构、机制与策略分离、vruntime 概念
- **适应你的规模**：数据结构、时间片粒度、队列数量
- **不要复制**：基于信号的抢占、小线程数的复杂负载均衡、内核特定优化
