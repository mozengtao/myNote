# Extracting FRR Architecture - Part 6: VERIFY

## Self-Check and Architecture Validation

This document provides checklists and verification tools to ensure
your FRR-inspired architecture is correctly implemented.

---

## 1. Architecture Sanity Checklist

```
+===========================================================================+
|                    ARCHITECTURE SANITY CHECKLIST                          |
+===========================================================================+

                    STATE OWNERSHIP
                    ===============

  +------------------------------------------------------------------+
  | QUESTION                                          | PASS? | FIX  |
  +------------------------------------------------------------------+
  | Is there exactly ONE owner for each piece of     |       |      |
  | authoritative state?                              | [ ]   |      |
  +------------------------------------------------------------------+
  | Can you point to a single file/struct that owns  |       |      |
  | each major data type?                            | [ ]   |      |
  +------------------------------------------------------------------+
  | Are there any global variables that multiple     |       |      |
  | modules modify directly?                         | [ ]   |      |
  | (Should be NO)                                   |       |      |
  +------------------------------------------------------------------+
  | Is state modification always through explicit    |       |      |
  | requests/functions?                              | [ ]   |      |
  +------------------------------------------------------------------+


                    DECISION/EXECUTION SEPARATION
                    =============================

  +------------------------------------------------------------------+
  | QUESTION                                          | PASS? | FIX  |
  +------------------------------------------------------------------+
  | Can you test decision logic without real I/O?    | [ ]   |      |
  +------------------------------------------------------------------+
  | Are there any send()/write()/recv() calls in     |       |      |
  | decision code?                                   | [ ]   |      |
  | (Should be NO)                                   |       |      |
  +------------------------------------------------------------------+
  | Does decision code return data structures        |       |      |
  | (decisions) rather than perform actions?         | [ ]   |      |
  +------------------------------------------------------------------+
  | Is I/O concentrated in a separate module/thread? | [ ]   |      |
  +------------------------------------------------------------------+


                    REPLAYABILITY
                    =============

  +------------------------------------------------------------------+
  | QUESTION                                          | PASS? | FIX  |
  +------------------------------------------------------------------+
  | Can you replay a sequence of inputs and get the  |       |      |
  | same state?                                      | [ ]   |      |
  +------------------------------------------------------------------+
  | Is configuration stored separately from runtime  |       |      |
  | state?                                           | [ ]   |      |
  +------------------------------------------------------------------+
  | Can you rebuild state from config + intent log?  | [ ]   |      |
  +------------------------------------------------------------------+
  | Is there hidden state that affects behavior but  |       |      |
  | isn't logged?                                    | [ ]   |      |
  | (Should be NO)                                   |       |      |
  +------------------------------------------------------------------+


                    EXPLICIT CONCURRENCY
                    ====================

  +------------------------------------------------------------------+
  | QUESTION                                          | PASS? | FIX  |
  +------------------------------------------------------------------+
  | Can you draw a diagram of all threads and what   |       |      |
  | data each accesses?                              | [ ]   |      |
  +------------------------------------------------------------------+
  | Is shared state protected by explicit mechanisms |       |      |
  | (locks, RCU, atomics)?                           | [ ]   |      |
  +------------------------------------------------------------------+
  | Are there any ad-hoc pthread_create() calls?     |       |      |
  | (Should be minimal and justified)               |       |      |
  +------------------------------------------------------------------+
  | Is the event loop single-threaded for core logic?| [ ]   |      |
  +------------------------------------------------------------------+


                    FAILURE ISOLATION
                    =================

  +------------------------------------------------------------------+
  | QUESTION                                          | PASS? | FIX  |
  +------------------------------------------------------------------+
  | If engine A crashes/fails, do other engines      |       |      |
  | continue working?                                | [ ]   |      |
  +------------------------------------------------------------------+
  | Is there a way to restart a failed component     |       |      |
  | without restarting everything?                   | [ ]   |      |
  +------------------------------------------------------------------+
  | Are errors from one subsystem contained to that  |       |      |
  | subsystem?                                       | [ ]   |      |
  +------------------------------------------------------------------+
  | Is there health monitoring for each component?   | [ ]   |      |
  +------------------------------------------------------------------+


                    OBSERVABILITY
                    =============

  +------------------------------------------------------------------+
  | QUESTION                                          | PASS? | FIX  |
  +------------------------------------------------------------------+
  | Can you query current state at any time?         | [ ]   |      |
  +------------------------------------------------------------------+
  | Can you see the state of all FSMs?               | [ ]   |      |
  +------------------------------------------------------------------+
  | Are important events logged with enough context? | [ ]   |      |
  +------------------------------------------------------------------+
  | Can you reconstruct what happened from logs?     | [ ]   |      |
  +------------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 6.1 架构完整性检查清单

**状态所有权检查：**

```
□ 每个权威状态是否只有一个所有者？
  - 可以指出哪个结构体拥有哪个数据
  - 没有多个模块直接修改同一数据

□ 全局变量是否通过函数访问？
  - 不是 global_var = value;
  - 而是 set_value(global, value);

□ 状态修改是否通过请求？
  - 不是直接修改
  - 而是提交请求让状态管理器处理
```

**决策/执行分离检查：**

```
□ 决策代码能否单独测试？
  - 给假输入
  - 检查输出
  - 不需要真实 I/O

□ 决策代码中是否有 I/O？
  - 搜索 send、write、recv
  - 应该没有
  
□ I/O 是否集中？
  - 在专门的模块
  - 或专门的线程
```

**可重放性检查：**

```
□ 相同输入是否产生相同状态？
  - 重放事件序列
  - 验证最终状态

□ 配置和运行时状态是否分离？
  - 配置是意图
  - 状态是实际
  - 可以重新调和

□ 是否有隐藏状态？
  - 影响行为但未记录
  - 应该没有
```

---

## 2. Red Flags

```
+===========================================================================+
|                    ARCHITECTURAL RED FLAGS                                |
+===========================================================================+

                    GLOBAL MUTABLE STATE
                    ====================

  RED FLAG:
  +------------------------------------------------------------------+
  |  // Multiple modules directly access and modify                  |
  |  extern int global_counter;  // Declared in header               |
  |                                                                  |
  |  // In module_a.c                                                |
  |  global_counter++;                                               |
  |                                                                  |
  |  // In module_b.c                                                |
  |  global_counter--;                                               |
  |                                                                  |
  |  // In module_c.c                                                |
  |  if (global_counter > 0) ...                                     |
  +------------------------------------------------------------------+

  FIX:
  +------------------------------------------------------------------+
  |  // Centralize in state manager                                  |
  |  struct state_manager {                                          |
  |      int counter;                                                |
  |  };                                                              |
  |                                                                  |
  |  int state_increment_counter(struct state_manager *s);           |
  |  int state_decrement_counter(struct state_manager *s);           |
  |  int state_get_counter(const struct state_manager *s);           |
  +------------------------------------------------------------------+


                    EXECUTION INSIDE DECISION LOGIC
                    ================================

  RED FLAG:
  +------------------------------------------------------------------+
  |  // Computation mixed with I/O                                   |
  |  int calculate_and_send(struct data *d) {                        |
  |      int result = complex_calculation(d);                        |
  |      send(socket, &result, sizeof(result), 0);  // <-- RED FLAG  |
  |      return result;                                              |
  |  }                                                               |
  +------------------------------------------------------------------+

  FIX:
  +------------------------------------------------------------------+
  |  // Separate calculation and sending                             |
  |  int calculate(struct data *d) {                                 |
  |      return complex_calculation(d);                              |
  |  }                                                               |
  |                                                                  |
  |  void send_result(int socket, int result) {                      |
  |      send(socket, &result, sizeof(result), 0);                   |
  |  }                                                               |
  |                                                                  |
  |  // Usage: decision then execution                               |
  |  int result = calculate(d);                                      |
  |  send_result(socket, result);                                    |
  +------------------------------------------------------------------+


                    IMPLICIT LIFETIMES
                    ==================

  RED FLAG:
  +------------------------------------------------------------------+
  |  struct item *get_item(int id) {                                 |
  |      // Caller has no idea when this becomes invalid             |
  |      return &global_items[id];                                   |
  |  }                                                               |
  |                                                                  |
  |  void some_function() {                                          |
  |      struct item *i = get_item(5);                               |
  |      // ... other code ...                                       |
  |      // Is i still valid? Who knows!                             |
  |      use(i);                                                     |
  |  }                                                               |
  +------------------------------------------------------------------+

  FIX:
  +------------------------------------------------------------------+
  |  // Option 1: Reference counting                                 |
  |  struct item *get_item_ref(int id) {                             |
  |      struct item *i = &global_items[id];                         |
  |      ref_get(&i->refcount);                                      |
  |      return i;                                                   |
  |  }                                                               |
  |  void put_item_ref(struct item *i) {                             |
  |      ref_put(&i->refcount);                                      |
  |  }                                                               |
  |                                                                  |
  |  // Option 2: RCU (like frrcu.h)                                |
  |  void some_function() {                                          |
  |      rcu_read_lock();                                            |
  |      struct item *i = get_item(5);                               |
  |      // i is valid while rcu_read_lock held                      |
  |      use(i);                                                     |
  |      rcu_read_unlock();                                          |
  |  }                                                               |
  +------------------------------------------------------------------+


                    UNBOUNDED CALLBACKS
                    ===================

  RED FLAG:
  +------------------------------------------------------------------+
  |  // Callback can do anything, including mess up state            |
  |  typedef void (*callback_t)(void *);                             |
  |                                                                  |
  |  void process(callback_t cb, void *arg) {                        |
  |      // Before callback: state is X                              |
  |      cb(arg);  // Who knows what cb does!                        |
  |      // After callback: state is ???                             |
  |  }                                                               |
  +------------------------------------------------------------------+

  FIX:
  +------------------------------------------------------------------+
  |  // Callbacks return decisions, don't modify state               |
  |  typedef struct decision (*callback_t)(const struct event *);    |
  |                                                                  |
  |  void process(callback_t cb, const struct event *e) {            |
  |      struct decision d = cb(e);  // Pure function                |
  |      apply_decision(&d);         // Controlled state change      |
  |  }                                                               |
  +------------------------------------------------------------------+


                    HIDDEN DEPENDENCIES
                    ===================

  RED FLAG:
  +------------------------------------------------------------------+
  |  void initialize() {                                             |
  |      // Order matters, but it's not obvious                      |
  |      init_A();  // Must be first                                 |
  |      init_B();  // Depends on A                                  |
  |      init_C();  // Depends on A and B                            |
  |      init_D();  // Who knows?                                    |
  |  }                                                               |
  +------------------------------------------------------------------+

  FIX:
  +------------------------------------------------------------------+
  |  // Explicit dependencies                                        |
  |  struct module_A *init_A(void);                                  |
  |  struct module_B *init_B(struct module_A *a);                    |
  |  struct module_C *init_C(struct module_A *a, struct module_B *b);|
  |                                                                  |
  |  void initialize() {                                             |
  |      struct module_A *a = init_A();                              |
  |      struct module_B *b = init_B(a);      // Needs A             |
  |      struct module_C *c = init_C(a, b);   // Needs A and B       |
  |  }                                                               |
  +------------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 6.2 危险信号

**全局可变状态：**

```
警告信号：
- extern 全局变量
- 多个模块直接修改
- 难以追踪谁改了什么

修复：
- 集中到状态管理器
- 通过函数访问
- 记录修改历史
```

**决策中执行：**

```
警告信号：
- 计算和 I/O 混在一起
- 难以测试计算逻辑
- 难以复用计算

修复：
- 分离计算和执行
- 计算函数是纯函数
- 执行单独调用
```

**隐式生命周期：**

```
警告信号：
- 返回指针但不清楚何时失效
- 调用者不知道如何释放
- 可能出现悬空引用

修复：
- 引用计数：get_ref/put_ref
- RCU：rcu_read_lock/unlock
- 明确所有权规则
```

**无界回调：**

```
警告信号：
- 回调可以做任何事
- 调用回调后状态不确定
- 难以推理程序行为

修复：
- 回调返回决策，不修改状态
- 主逻辑控制状态变更
- 回调是纯函数
```

---

## 3. Knowledge Transfer Outcome

```
+===========================================================================+
|                    LEARNING VERIFICATION                                  |
+===========================================================================+

  After studying this guide, you should be able to:

  +------------------------------------------------------------------+
  | SKILL                                           | SELF-CHECK     |
  +------------------------------------------------------------------+
  |                                                                  |
  | DESIGN:                                                          |
  | [ ] Design FRR-inspired architectures from scratch               |
  | [ ] Identify the "Zebra", "engines", "backend" in any system     |
  | [ ] Draw clear module boundaries                                 |
  | [ ] Choose appropriate level of isolation (process/thread/none)  |
  |                                                                  |
  +------------------------------------------------------------------+
  |                                                                  |
  | REFACTOR:                                                        |
  | [ ] Extract state from scattered locations                       |
  | [ ] Introduce request-based state modification                   |
  | [ ] Separate decision from execution                             |
  | [ ] Convert implicit state to FSMs                               |
  |                                                                  |
  +------------------------------------------------------------------+
  |                                                                  |
  | APPLY:                                                           |
  | [ ] Identify when FRR-style is appropriate                       |
  | [ ] Identify when simpler approaches are better                  |
  | [ ] Avoid over-engineering and cargo-culting                     |
  | [ ] Incrementally migrate existing code                          |
  |                                                                  |
  +------------------------------------------------------------------+
  |                                                                  |
  | EXPLAIN:                                                         |
  | [ ] Why state needs a single authority                           |
  | [ ] Why decision must be separate from execution                 |
  | [ ] Why event-driven beats ad-hoc threading                      |
  | [ ] Why FSMs are better than implicit state                      |
  | [ ] Why RCU solves lifetime problems                             |
  |                                                                  |
  +------------------------------------------------------------------+


  FINAL TEST: Can you answer these questions?
  ============================================

  1. You're designing a database cache manager. What is your "Zebra"?
     Answer: The cache state manager (owns cache entries, eviction policy)

  2. You have a monitoring system with 5 data sources. Are they "engines"?
     Answer: Yes, if they make independent decisions about what to collect

  3. Someone proposes using 10 threads for parallelism. Red flag?
     Answer: Maybe. Ask: what state is shared? Are locks explicit?

  4. A function both calculates optimal placement AND calls the API.
     Problem?
     Answer: Yes. Split into pure calculation + separate API call.

  5. Code uses `extern int counter` modified from 3 files. Fix?
     Answer: Create counter_manager with increment/decrement functions.
```

---

## 中文解释 (Chinese Explanation)

### 6.3 学习成果验证

**设计能力：**

```
□ 能从零设计 FRR 式架构
□ 能识别系统中的 "Zebra"、"引擎"、"后端"
□ 能画清晰的模块边界
□ 能选择适当的隔离级别
```

**重构能力：**

```
□ 能从分散位置提取状态
□ 能引入请求式状态修改
□ 能分离决策和执行
□ 能把隐式状态转换为 FSM
```

**应用判断：**

```
□ 知道何时适合 FRR 式架构
□ 知道何时简单方法更好
□ 避免过度工程
□ 能增量迁移现有代码
```

**解释能力：**

```
□ 能解释为什么状态需要单一权威
□ 能解释为什么决策必须与执行分离
□ 能解释为什么事件驱动优于 ad-hoc 线程
□ 能解释为什么 FSM 比隐式状态好
□ 能解释 RCU 如何解决生命周期问题
```

---

## 4. Summary

### Key Principles Extracted from FRR

| Principle | FRR Implementation | Generic Application |
|-----------|-------------------|---------------------|
| Single Source of Truth | Zebra owns RIB | State Manager owns authoritative state |
| Decision/Execution Split | Protocol/Kernel | Logic/Backend separation |
| Event-Driven Core | lib/event.c | Single-threaded event loop |
| Explicit FSMs | Protocol state machines | Any complex state transitions |
| Safe Concurrent Access | frrcu.h (RCU) | Reference counting or RCU |
| Request-based Changes | ZAPI messages | Intent/Request pattern |
| Failure Isolation | Separate daemons | Engine isolation |
| Reconciliation | Graceful restart | Config vs Runtime sync |

### Next Steps

1. **Audit your current codebase** using the checklist
2. **Identify red flags** and prioritize fixes
3. **Start small**: Extract one state authority first
4. **Iterate**: Add separation incrementally
5. **Verify**: Re-check after each change

---

## End of FRR Concepts Extraction Documentation

This series covers:
1. **WHY**: Why these concepts matter
2. **HOW**: Translating FRR to generic patterns
3. **WHAT**: Concrete building blocks
4. **WHERE**: Mapping to your codebase
5. **APPLY**: Practical application
6. **VERIFY**: Validation and self-check
