# FRR Kernel Interaction Architecture - Part 6: REUSE

## Lessons for Building Resilient Kernel-Facing Systems

This document extracts design principles from FRR's kernel interaction
layer that can be applied to any system that programs the Linux kernel.

---

## 1. Core Design Principles

```
+===========================================================================+
|                    RESILIENT KERNEL INTERACTION PRINCIPLES                |
+===========================================================================+

                    PRINCIPLE 1: KERNEL AS CACHE
                    ============================

  +-----------------------------------------------------------------------+
  |                                                                       |
  |   WRONG MENTAL MODEL:                                                 |
  |   +-------------------+                                               |
  |   |  Kernel           |  <-- Source of truth                          |
  |   |  FIB/State        |                                               |
  |   +-------------------+                                               |
  |           ^                                                           |
  |           | reads                                                     |
  |   +-------------------+                                               |
  |   |  User Application |  <-- Just a reader                            |
  |   +-------------------+                                               |
  |                                                                       |
  |                                                                       |
  |   CORRECT MENTAL MODEL:                                               |
  |   +-------------------+                                               |
  |   |  Application RIB  |  <-- Source of truth                          |
  |   |  (Intent Store)   |                                               |
  |   +-------------------+                                               |
  |           |                                                           |
  |           | writes                                                    |
  |           v                                                           |
  |   +-------------------+                                               |
  |   |  Kernel           |  <-- Cache/materialized view                  |
  |   |  FIB/State        |                                               |
  |   +-------------------+                                               |
  |                                                                       |
  +-----------------------------------------------------------------------+


                    PRINCIPLE 2: EVENTUAL CONSISTENCY
                    ==================================

  +-----------------------------------------------------------------------+
  |                                                                       |
  |  Time ---->                                                           |
  |                                                                       |
  |  Application State:   A  A  A  B  B  B  B  B  B  B                   |
  |                           |     ^                                     |
  |                           |     | intent changes                      |
  |                           v     |                                     |
  |  Kernel State:        ?  A  A  A  A  B  B  B  B  B                   |
  |                             ^        ^                                |
  |                             |        | kernel catches up              |
  |                             |                                         |
  |                             +-- temporary inconsistency               |
  |                                 (acceptable)                          |
  |                                                                       |
  |  Key insight:                                                         |
  |  - Brief inconsistency windows are normal                             |
  |  - Design for convergence, not instant consistency                    |
  |  - Monitor for prolonged divergence                                   |
  |                                                                       |
  +-----------------------------------------------------------------------+


                    PRINCIPLE 3: IDEMPOTENT OPERATIONS
                    ===================================

  +-----------------------------------------------------------------------+
  |                                                                       |
  |  OPERATION           | EXPECTED BEHAVIOR                              |
  +-----------------------------------------------------------------------+
  |  Install route A     | Route A in kernel                              |
  |  Install route A     | Route A in kernel (no error)                   |
  |  Install route A     | Route A in kernel (still no error)             |
  |                      |                                                |
  |  Delete route A      | Route A removed                                |
  |  Delete route A      | No change (no error)                           |
  |  Delete route A      | No change (still no error)                     |
  +-----------------------------------------------------------------------+
  |                                                                       |
  |  Implementation:                                                      |
  |  - EEXIST on install = success (already there)                        |
  |  - ESRCH on delete = success (already gone)                           |
  |  - Retry is always safe                                               |
  |                                                                       |
  +-----------------------------------------------------------------------+


                    PRINCIPLE 4: SEPARATION OF CONCERNS
                    ====================================

  +-----------------------------------------------------------------------+
  |                                                                       |
  |  +-------------------+     +-------------------+                      |
  |  |  DECISION LOGIC   |     |  EXECUTION LOGIC  |                      |
  |  +-------------------+     +-------------------+                      |
  |  |                   |     |                   |                      |
  |  | "What routes      |     | "How to program   |                      |
  |  |  should exist?"   |     |  the kernel?"     |                      |
  |  |                   |     |                   |                      |
  |  | - Protocol logic  |     | - Netlink format  |                      |
  |  | - Best path       |     | - Error handling  |                      |
  |  | - Policy          |     | - Batching        |                      |
  |  |                   |     | - Retry logic     |                      |
  |  +-------------------+     +-------------------+                      |
  |           |                         ^                                 |
  |           |    Abstract Queue       |                                 |
  |           +------------------------>+                                 |
  |                                                                       |
  |  Benefits:                                                            |
  |  - Test decision logic without kernel                                 |
  |  - Change kernel interface without affecting logic                    |
  |  - Async execution, better performance                                |
  |                                                                       |
  +-----------------------------------------------------------------------+


+===========================================================================+
|                    CONTROL-PLANE / KERNEL CONTRACT                        |
+===========================================================================+

                    THE CONTRACT
                    ============

  +-----------------------------------------------------------------------+
  |                                                                       |
  |  CONTROL PLANE PROMISES:                                              |
  |  +------------------------------------------------------------------+ |
  |  | 1. I will tell you my intent (install/delete operations)         | |
  |  | 2. I will handle your errors gracefully                          | |
  |  | 3. I will not rely on you remembering state across reboots       | |
  |  | 4. I will periodically verify our states match                   | |
  |  +------------------------------------------------------------------+ |
  |                                                                       |
  |  KERNEL PROMISES:                                                     |
  |  +------------------------------------------------------------------+ |
  |  | 1. I will try to execute your requests                           | |
  |  | 2. I will tell you if I succeed or fail (ACK/ERROR)              | |
  |  | 3. I will notify you of external changes (if you subscribe)      | |
  |  | 4. I make NO promises about state persistence                    | |
  |  +------------------------------------------------------------------+ |
  |                                                                       |
  |  NEITHER PROMISES:                                                    |
  |  +------------------------------------------------------------------+ |
  |  | - Atomicity across multiple operations                           | |
  |  | - Preventing external modifications                              | |
  |  | - Consistent ordering of notifications                           | |
  |  +------------------------------------------------------------------+ |
  |                                                                       |
  +-----------------------------------------------------------------------+


+===========================================================================+
|                    PRACTICAL IMPLEMENTATION PATTERNS                      |
+===========================================================================+

                    PATTERN: OPERATION QUEUE
                    ========================

  +-----------------------------------------------------------------------+
  |                                                                       |
  |  +---------+    +---------+    +---------+    +---------+            |
  |  | Route A |    | Route B |    | Route C |    | Route D |            |
  |  | INSTALL |    | DELETE  |    | INSTALL |    | INSTALL |            |
  |  +---------+    +---------+    +---------+    +---------+            |
  |       |              |              |              |                  |
  |       v              v              v              v                  |
  |  +--------------------------------------------------------+          |
  |  |                 OPERATION QUEUE                        |          |
  |  |  - FIFO order                                          |          |
  |  |  - Batching possible                                   |          |
  |  |  - Async processing                                    |          |
  |  +--------------------------------------------------------+          |
  |                           |                                           |
  |                           v                                           |
  |  +--------------------------------------------------------+          |
  |  |                 KERNEL EXECUTOR                        |          |
  |  |  - Dequeue operations                                  |          |
  |  |  - Send netlink messages                               |          |
  |  |  - Handle responses                                    |          |
  |  |  - Report status back                                  |          |
  |  +--------------------------------------------------------+          |
  |                                                                       |
  +-----------------------------------------------------------------------+


                    PATTERN: RECONCILIATION LOOP
                    ============================

  +-----------------------------------------------------------------------+
  |                                                                       |
  |  +---> [Sleep for interval]                                          |
  |  |                                                                    |
  |  |     [Get desired state from RIB]                                   |
  |  |              |                                                     |
  |  |              v                                                     |
  |  |     [Get actual state from kernel]                                 |
  |  |              |                                                     |
  |  |              v                                                     |
  |  |     [Compare states]                                               |
  |  |              |                                                     |
  |  |              v                                                     |
  |  |     [Generate delta operations]                                    |
  |  |              |                                                     |
  |  |              v                                                     |
  |  |     [Apply operations to kernel]                                   |
  |  |              |                                                     |
  |  +----<--------+                                                      |
  |                                                                       |
  |  Frequency considerations:                                            |
  |  - Full reconciliation: every few minutes (expensive)                 |
  |  - Targeted reconciliation: on specific events                        |
  |  - Health check: lightweight verification                             |
  |                                                                       |
  +-----------------------------------------------------------------------+


                    PATTERN: ERROR CLASSIFICATION
                    =============================

  +-----------------------------------------------------------------------+
  |                                                                       |
  |  ERROR CATEGORY     | ACTION              | EXAMPLE                   |
  +-----------------------------------------------------------------------+
  |  TRANSIENT          | Retry with backoff  | ENOBUFS, EAGAIN           |
  |  EXPECTED_RACE      | Log debug, continue | EEXIST, ESRCH             |
  |  PERMANENT          | Log error, skip     | EINVAL, EPERM             |
  |  FATAL              | Crash/restart       | ENOMEM (prolonged)        |
  +-----------------------------------------------------------------------+
  |                                                                       |
  |  Implementation:                                                      |
  |                                                                       |
  |  switch (-err->error) {                                               |
  |  case EEXIST:                                                         |
  |  case ESRCH:                                                          |
  |      log_debug(...);                                                  |
  |      return SUCCESS;  // Idempotent success                           |
  |                                                                       |
  |  case EAGAIN:                                                         |
  |  case ENOBUFS:                                                        |
  |      schedule_retry(...);                                             |
  |      return RETRY;                                                    |
  |                                                                       |
  |  case EINVAL:                                                         |
  |  case EPERM:                                                          |
  |      log_error(...);                                                  |
  |      return FAILURE;                                                  |
  |  }                                                                    |
  |                                                                       |
  +-----------------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 6.1 核心设计原则总结

**原则1：内核是缓存，不是数据库**

```
关键思想：
- 应用程序维护"意图状态"（我想要什么路由）
- 内核维护"物化视图"（实际安装的路由）
- 意图状态是权威的，内核状态是派生的

实践意义：
1. 任何时候都可以从意图状态重建内核状态
2. 不需要备份内核路由表
3. 重启后，应用重新同步即可

代码体现：
// FRR 中，RIB 是意图状态
// 即使内核路由表清空，RIB 仍然知道应该有什么路由
// 重启时，RIB 驱动内核安装所有路由
```

**原则2：最终一致性**

```
关键思想：
- 短暂的不一致是可接受的
- 系统会收敛到一致状态
- 监控是否长时间不收敛

实践意义：
1. 不需要分布式事务
2. 重试是正常操作
3. 偶发错误不是灾难

代码体现：
// FRR 允许短暂的 RIB/FIB 不一致
// 通过定期调和或事件驱动收敛
// 长时间不一致会触发告警
```

**原则3：幂等操作**

```
关键思想：
- 重复执行相同操作，结果相同
- 安装已存在的路由 = 成功
- 删除不存在的路由 = 成功

实践意义：
1. 重试安全
2. 简化错误处理
3. 支持"声明式"配置

代码体现：
// FRR 将 EEXIST 视为成功
// 将 ESRCH 视为成功
// 不需要先查询再操作
```

**原则4：关注点分离**

```
关键思想：
- 决策逻辑（选择路由）与执行逻辑（安装路由）分离
- 通过队列解耦

实践意义：
1. 决策逻辑可以独立测试
2. 执行逻辑可以替换（Netlink vs DPDK）
3. 异步执行提高性能

代码体现：
// FRR 的 dataplane 抽象层
// RIB 线程做决策，dataplane 线程做执行
// 通过 dplane_ctx 传递信息
```

### 6.2 控制面/内核契约

**控制面的承诺：**

```
1. 告知意图
   - 发送 install/delete 请求
   - 不假设内核当前状态

2. 优雅处理错误
   - 分类错误
   - 合理响应

3. 不依赖内核持久化
   - 重启后重新同步
   - 自己持久化意图状态

4. 定期验证
   - 主动检查一致性
   - 发现并修复偏差
```

**内核的承诺：**

```
1. 尽力执行请求
   - 成功或失败
   - 返回结果

2. 报告结果
   - ACK 表示成功
   - ERROR 表示失败，带原因

3. 通知变化
   - 如果订阅了相应组
   - 不保证顺序

4. 不保证持久化
   - 重启后清空
   - 外部修改可能发生
```

### 6.3 实践实现模式

**模式1：操作队列**

```
为什么需要队列？

问题：
- 路由协议产生大量路由更新
- 同步安装每条路由太慢
- 批量处理更高效

解决方案：
1. 协议产生路由更新 -> 入队
2. Executor 线程批量出队 -> 批量发送
3. 处理响应 -> 更新状态

好处：
- 解耦生产和消费
- 支持批量优化
- 异步不阻塞
```

**模式2：调和循环**

```
为什么需要调和？

问题：
- 内核状态可能被外部修改
- 消息可能丢失
- 需要周期性验证

解决方案：
定期执行：
1. 获取期望状态（从 RIB）
2. 获取实际状态（从内核）
3. 计算差异
4. 应用差异

权衡：
- 频率高 -> 更快发现偏差 -> 更大开销
- 频率低 -> 开销小 -> 偏差存在时间长

实践：
- 完整调和：几分钟一次
- 事件触发调和：特定事件后
- 轻量健康检查：频繁但简单
```

**模式3：错误分类**

```
不是所有错误都一样！

分类：
1. TRANSIENT（瞬态）
   - 例如：ENOBUFS, EAGAIN
   - 处理：重试

2. EXPECTED_RACE（预期竞态）
   - 例如：EEXIST, ESRCH
   - 处理：视为成功

3. PERMANENT（永久）
   - 例如：EINVAL, EPERM
   - 处理：记录错误，跳过

4. FATAL（致命）
   - 例如：持续的 ENOMEM
   - 处理：重启/告警
```

### 6.4 调试内核交互问题

**常见问题和诊断：**

```
问题1：路由没有安装到内核
诊断：
1. 检查 RIB：show ip route
2. 检查内核：ip route
3. 检查日志：debug zebra kernel

问题2：路由安装失败
诊断：
1. 检查接口状态：是否 up？
2. 检查下一跳可达性
3. 检查权限：Zebra 是否有 CAP_NET_ADMIN？

问题3：路由频繁变化
诊断：
1. 检查链路稳定性
2. 检查是否有外部程序修改路由
3. 检查协议收敛问题

问题4：内存持续增长
诊断：
1. 检查 Netlink 缓冲区大小
2. 检查是否有未处理的消息堆积
3. 检查是否有内存泄漏（valgrind）
```

### 6.5 设计自己的系统

**检查清单：**

```
□ 明确定义"意图状态"的存储位置
□ 设计幂等的操作接口
□ 实现错误分类和处理策略
□ 添加调和机制
□ 分离决策和执行逻辑
□ 添加充足的日志和监控
□ 考虑批量处理优化
□ 测试异常场景（权限、竞态、重启）
```

---

## 2. Common Pitfalls and Solutions

### 2.1 Pitfall: Assuming Kernel State Persists

**Problem**: Application expects kernel to remember state after reboot.

**Solution**: Always reconcile on startup.

### 2.2 Pitfall: Treating Errors as Fatal

**Problem**: Application crashes on ESRCH or EEXIST.

**Solution**: Classify errors and handle appropriately.

### 2.3 Pitfall: Synchronous Kernel Calls

**Problem**: Main thread blocked waiting for kernel.

**Solution**: Use async queue and dedicated worker thread.

### 2.4 Pitfall: No Monitoring

**Problem**: Divergence goes undetected.

**Solution**: Implement periodic reconciliation and health checks.

---

## 3. Summary Checklist

When building a kernel-facing system:

- [ ] Define clear "source of truth" for application state
- [ ] Treat kernel as a cache to be synchronized
- [ ] Design operations to be idempotent
- [ ] Classify and handle errors appropriately
- [ ] Implement reconciliation mechanism
- [ ] Use async operation patterns
- [ ] Add comprehensive logging
- [ ] Monitor for state divergence
- [ ] Test failure scenarios thoroughly
- [ ] Document the control-plane/kernel contract

---

## End of Kernel Interaction Architecture Documentation

This series covers:
1. **WHY**: Why kernel is not a database
2. **HOW**: Netlink strategy and error handling
3. **WHAT**: Message structures and formats
4. **WHERE**: Source code reading guide
5. **API**: Abstraction boundaries
6. **REUSE**: Design principles for your own systems
