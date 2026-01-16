# Section 16: Mental Model Summary

This final section synthesizes everything into a coherent mental model for
thinking about PostgreSQL.

---

## The PostgreSQL Mental Model

```
+------------------------------------------------------------------+
|                 PostgreSQL: A Mental Model                       |
+------------------------------------------------------------------+
|                                                                  |
|  PostgreSQL is:                                                  |
|                                                                  |
|  1. A DURABLE DATA STRUCTURE                                     |
|     - Data survives crashes (WAL guarantees this)                |
|     - Writes go to log first, data pages later                   |
|     - Recovery replays log to reconstruct state                  |
|                                                                  |
|  2. A CONCURRENCY CONTROL SYSTEM                                 |
|     - MVCC: Multiple versions of rows coexist                    |
|     - Readers see snapshots, never blocked by writers            |
|     - Writers block only other writers (on same row)             |
|                                                                  |
|  3. A QUERY EXECUTION ENGINE                                     |
|     - SQL -> Parse -> Optimize -> Execute                        |
|     - Cost-based optimizer chooses execution plan                |
|     - Multiple index types for different access patterns         |
|                                                                  |
|  4. A CRASH RECOVERY SYSTEM                                      |
|     - WAL enables point-in-time recovery                         |
|     - Checkpoints limit recovery time                            |
|     - Full page writes protect against torn pages                |
|                                                                  |
+------------------------------------------------------------------+

PostgreSQL心智模型：

1. 持久数据结构
   - WAL保证崩溃不丢数据
   - 先写日志，后写数据

2. 并发控制系统
   - MVCC实现读写不阻塞
   - 每个事务看到一致快照

3. 查询执行引擎
   - 基于代价的优化器
   - 多种索引类型

4. 崩溃恢复系统
   - WAL支持时间点恢复
   - 检查点限制恢复时间
```

---

## How PostgreSQL Differs From Other Databases

```
+------------------------------------------------------------------+
|                   PostgreSQL vs Others                           |
+------------------------------------------------------------------+
|                                                                  |
|  vs MySQL (InnoDB):                                              |
|  +------------------------+----------------------------------+   |
|  | PostgreSQL             | MySQL                            |   |
|  +------------------------+----------------------------------+   |
|  | Process per connection | Thread per connection            |   |
|  | Heap storage           | Clustered index (PK order)       |   |
|  | MVCC via xmin/xmax     | MVCC via undo log                |   |
|  | Rich data types        | More limited types               |   |
|  | Extensions first-class | Plugins less integrated          |   |
|  | REPEATABLE READ = SI   | REPEATABLE READ = partial SI     |   |
|  +------------------------+----------------------------------+   |
|                                                                  |
|  vs Oracle:                                                      |
|  - PostgreSQL: Open source, no licensing costs                   |
|  - Oracle: More mature RAC, better partitioning historically     |
|  - PostgreSQL catching up rapidly                                |
|                                                                  |
|  vs SQLite:                                                      |
|  - PostgreSQL: Full server, multi-user, networked                |
|  - SQLite: Embedded, single-writer, file-based                   |
|  - Different use cases entirely                                  |
|                                                                  |
+------------------------------------------------------------------+

PostgreSQL与其他数据库的区别：

vs MySQL：
- 进程模型 vs 线程模型
- 堆存储 vs 聚簇索引
- 更丰富的数据类型
- 扩展是一等公民

vs Oracle：
- 开源，无许可费用
- 功能逐渐追平

vs SQLite：
- 完整服务器 vs 嵌入式
- 多用户 vs 单写入者
- 不同的使用场景
```

---

## How to Think About PostgreSQL

```
When writing queries, think:
+------------------------------------------------------------------+
|                                                                  |
|  1. "How will this be executed?"                                 |
|     - What indexes exist?                                        |
|     - Will it use them?                                          |
|     - What's the estimated row count?                            |
|     -> Use EXPLAIN to verify                                     |
|                                                                  |
|  2. "How does this affect storage?"                              |
|     - UPDATE creates new row version (MVCC)                      |
|     - DELETE marks row, doesn't remove                           |
|     - Need VACUUM to reclaim space                               |
|                                                                  |
|  3. "What happens with concurrency?"                             |
|     - What locks are acquired?                                   |
|     - Can this deadlock?                                         |
|     - What isolation level do I need?                            |
|                                                                  |
|  4. "What if it crashes?"                                        |
|     - Are my changes in a transaction?                           |
|     - Will WAL protect them?                                     |
|     - Is synchronous_commit appropriate?                         |
|                                                                  |
+------------------------------------------------------------------+

思考PostgreSQL的方式：

写查询时思考：
1. 如何执行？用什么索引？
2. 如何影响存储？UPDATE创建新版本
3. 并发如何处理？需要什么锁？
4. 崩溃时怎样？事务保护了吗？
```

---

## When PostgreSQL is the Wrong Tool

```
Consider alternatives when:
+------------------------------------------------------------------+
|                                                                  |
|  Use Case                     | Better Alternative               |
|  -----------------------------|--------------------------------  |
|  Pure key-value, ultra-fast   | Redis, Memcached                |
|  Billions of time-series rows | TimescaleDB, InfluxDB           |
|  Full-text search at scale    | Elasticsearch                   |
|  Graph traversals             | Neo4j, Neptune                  |
|  Analytical warehouse (PB)    | ClickHouse, BigQuery            |
|  Embedded in application      | SQLite                          |
|  Document-first design        | MongoDB (but consider JSONB!)   |
|                                                                  |
+------------------------------------------------------------------+

何时PostgreSQL不是最佳选择：

- 纯键值、超高速：Redis
- 海量时序数据：TimescaleDB
- 大规模全文搜索：Elasticsearch
- 图遍历：Neo4j
- PB级分析：ClickHouse
- 嵌入式：SQLite
- 文档优先：考虑JSONB，或MongoDB

但是：PostgreSQL的覆盖范围非常广，
许多场景都可以用PostgreSQL + 扩展解决。
```

---

## The Database Engineer's Mindset

```
+------------------------------------------------------------------+
|              The Database Engineer's Mindset                     |
+------------------------------------------------------------------+
|                                                                  |
|  1. DATA INTEGRITY FIRST                                         |
|     - Constraints are features, not obstacles                    |
|     - Transactions protect invariants                            |
|     - If the database says no, listen                            |
|                                                                  |
|  2. UNDERSTAND THE COST                                          |
|     - Every index has write overhead                             |
|     - Every join has a cost                                      |
|     - Disk I/O is the bottleneck                                 |
|                                                                  |
|  3. MEASURE, DON'T GUESS                                         |
|     - EXPLAIN before optimizing                                  |
|     - Monitor production queries                                 |
|     - Profile before changing                                    |
|                                                                  |
|  4. DESIGN FOR FAILURE                                           |
|     - Assume crashes will happen                                 |
|     - Assume disks will fail                                     |
|     - Assume queries will be slow sometimes                      |
|                                                                  |
|  5. SIMPLICITY OVER CLEVERNESS                                   |
|     - Normalized schema first                                    |
|     - Obvious queries over clever ones                           |
|     - Standard patterns over custom solutions                    |
|                                                                  |
+------------------------------------------------------------------+

数据库工程师的思维方式：

1. 数据完整性优先
   - 约束是特性，不是障碍
   - 事务保护不变量

2. 理解代价
   - 每个索引有写开销
   - 每个连接有代价
   - 磁盘I/O是瓶颈

3. 测量而非猜测
   - 优化前先EXPLAIN
   - 监控生产查询

4. 为故障设计
   - 假设会崩溃
   - 假设磁盘会坏
   - 假设查询会变慢

5. 简单胜于聪明
   - 先规范化
   - 明显的查询
   - 标准模式
```

---

## Final Summary

```
+------------------------------------------------------------------+
|                    What You've Learned                           |
+------------------------------------------------------------------+
|                                                                  |
|  Foundations:                                                    |
|  - Why databases exist (persistence, concurrency, recovery)      |
|  - Why PostgreSQL chose MVCC, process model, extensibility       |
|                                                                  |
|  Internals:                                                      |
|  - Source code organization (backend, include, etc.)             |
|  - SQL processing pipeline (parse, plan, execute)                |
|  - Storage (heap files, pages, tuples)                           |
|  - Indexing (B-tree, GiST, GIN, BRIN)                            |
|  - MVCC (xmin/xmax, visibility, snapshots)                       |
|  - Locking (table locks, row locks, deadlocks)                   |
|  - WAL (durability, recovery, replication)                       |
|  - Memory (shared buffers, memory contexts)                      |
|                                                                  |
|  Practice:                                                       |
|  - Schema design connected to internals                          |
|  - Production patterns (pooling, EXPLAIN, transactions)          |
|  - Common mistakes and how to avoid them                         |
|                                                                  |
|  Mindset:                                                        |
|  - Think about execution plans                                   |
|  - Think about storage implications                              |
|  - Think about concurrency                                       |
|  - Think about failure modes                                     |
|                                                                  |
+------------------------------------------------------------------+

你学到了什么：

基础：
- 数据库存在的原因
- PostgreSQL的设计选择

内部机制：
- 源代码组织
- SQL处理流水线
- 存储引擎
- 索引类型
- MVCC
- 锁机制
- WAL
- 内存管理

实践：
- Schema设计
- 生产环境模式
- 常见错误

思维方式：
- 思考执行计划
- 思考存储影响
- 思考并发
- 思考故障模式

这不是终点，而是开始。
继续阅读源代码，继续实践，继续深入。
PostgreSQL的文档是你最好的朋友。
```

---

## Next Steps

```
+------------------------------------------------------------------+
|                      Where to Go From Here                       |
+------------------------------------------------------------------+
|                                                                  |
|  1. Read the PostgreSQL documentation                            |
|     https://www.postgresql.org/docs/                             |
|     - Best database documentation available                      |
|                                                                  |
|  2. Explore the source code                                      |
|     - Start with README files in each directory                  |
|     - Trace a simple query through the code                      |
|     - Read the comments (they're excellent)                      |
|                                                                  |
|  3. Join the community                                           |
|     - pgsql-hackers mailing list                                 |
|     - PostgreSQL Slack                                           |
|     - Local user groups                                          |
|                                                                  |
|  4. Build something                                              |
|     - Nothing teaches like real problems                         |
|     - Monitor your queries                                       |
|     - Optimize based on data                                     |
|                                                                  |
+------------------------------------------------------------------+

下一步：

1. 阅读PostgreSQL官方文档
   - 最好的数据库文档

2. 探索源代码
   - 从README文件开始
   - 跟踪简单查询的执行
   - 阅读注释（非常好）

3. 加入社区
   - pgsql-hackers邮件列表
   - PostgreSQL Slack

4. 构建项目
   - 实际问题是最好的老师
   - 监控你的查询
   - 基于数据优化

祝你学习PostgreSQL愉快！
```
