# Section 3: PostgreSQL High-Level Architecture

Understanding PostgreSQL's architecture is essential before diving into
specific subsystems. This section provides the big picture.

---

## 3.1 Big Picture Overview

### PostgreSQL is Process-Based

Unlike MySQL (threaded), Oracle (threaded), or SQL Server (threaded),
PostgreSQL uses a **process-per-connection** model.

```
                        +---------------------+
                        |    Client App 1     |
                        +----------+----------+
                                   |
                                   | TCP/IP or Unix Socket
                                   v
+------------------------------------------------------------------+
|                         PostgreSQL Server                        |
|                                                                  |
|  +------------------+                                            |
|  |    postmaster    |  (Main process, forks backends)            |
|  +--------+---------+                                            |
|           |                                                      |
|           | fork()                                               |
|           v                                                      |
|  +------------------+   +------------------+   +--------------+  |
|  | Backend Process  |   | Backend Process  |   | Background   |  |
|  | (for Client 1)   |   | (for Client 2)   |   | Workers      |  |
|  +--------+---------+   +--------+---------+   +------+-------+  |
|           |                      |                    |          |
|           +----------------------+--------------------+          |
|                                  |                               |
|                                  v                               |
|           +------------------------------------------+           |
|           |           Shared Memory                  |           |
|           |  +-------------+  +----------------+     |           |
|           |  | Buffer Pool |  | Lock Tables    |     |           |
|           |  +-------------+  +----------------+     |           |
|           |  +-------------+  +----------------+     |           |
|           |  | WAL Buffers |  | Proc Array     |     |           |
|           |  +-------------+  +----------------+     |           |
|           +------------------------------------------+           |
|                                  |                               |
+----------------------------------+-------------------------------+
                                   |
                                   v
                        +----------+----------+
                        |     Disk Storage    |
                        |  +-------+ +------+ |
                        |  | Data  | | WAL  | |
                        |  | Files | | Files| |
                        |  +-------+ +------+ |
                        +---------------------+

PostgreSQL进程架构：

1. postmaster - 主进程，负责：
   - 监听连接请求
   - 为每个客户端fork一个后端进程
   - 管理后台工作进程

2. Backend Process - 后端进程，每个客户端连接对应一个：
   - 解析SQL
   - 执行查询
   - 返回结果

3. Background Workers - 后台工作进程：
   - autovacuum（自动清理）
   - checkpointer（检查点）
   - wal writer（WAL写入）
   - bgwriter（后台写入）

4. Shared Memory - 共享内存：
   - Buffer Pool（缓冲池）：缓存数据页
   - WAL Buffers（WAL缓冲）：缓存WAL记录
   - Lock Tables（锁表）：管理锁
   - Proc Array（进程数组）：跟踪活跃事务

5. Disk Storage - 磁盘存储：
   - Data Files：存储表和索引数据
   - WAL Files：存储预写日志
```

### Why Process-Per-Connection?

**Historical reasons:**
- PostgreSQL was developed on BSD Unix in the 1980s-90s
- Unix `fork()` was the standard way to handle concurrency
- Threads were not portable or reliable across Unix variants

**Technical reasons:**

```
+------------------------+-------------------------+
|     Process Model      |      Thread Model       |
+------------------------+-------------------------+
| Memory isolation       | Shared address space    |
| Crash = one client     | Crash = all clients     |
| Simple programming     | Complex synchronization |
| fork() overhead        | Lower creation overhead |
| OS handles scheduling  | Need userspace sched.   |
+------------------------+-------------------------+

进程模型 vs 线程模型对比：

进程模型优点：
- 内存隔离：一个后端崩溃不影响其他
- 编程简单：无需担心线程安全
- 操作系统调度：利用成熟的OS调度器

线程模型优点：
- 创建开销低
- 内存共享更高效

PostgreSQL选择进程模型是因为：
1. 更好的稳定性（故障隔离）
2. 更简单的代码（无需到处加锁）
3. 历史原因（早期Unix的最佳实践）
```

**The key insight**: Process isolation means a bug in one connection cannot
corrupt another connection's memory. This improves stability at the cost of
context switching overhead.

Modern PostgreSQL mitigates the overhead through:
- Connection pooling (PgBouncer, PgPool-II)
- Shared buffers (most data lives in shared memory anyway)
- Efficient fork() on modern Linux (copy-on-write)

### Client-Server Communication

```
Client                                         Server
  |                                              |
  |  --- Startup Message ----------------------> |
  |                                              |
  |  <-- Authentication Request ---------------- |
  |                                              |
  |  --- Password Response -------------------> |
  |                                              |
  |  <-- Authentication OK --------------------- |
  |  <-- Parameter Status (server_version, etc.) |
  |  <-- Ready for Query ----------------------- |
  |                                              |
  |  --- Query ("SELECT * FROM t") -----------> |
  |                                              |
  |  <-- Row Description (column metadata) ----- |
  |  <-- Data Row ------------------------------ |
  |  <-- Data Row ------------------------------ |
  |  <-- Command Complete ("SELECT 2") --------- |
  |  <-- Ready for Query ----------------------- |
  |                                              |
  |  --- Terminate --------------------------->  |
  v                                              v

客户端-服务器通信协议：

1. 连接建立阶段：
   - 客户端发送启动消息
   - 服务器请求认证
   - 客户端提供密码
   - 服务器确认认证成功
   - 服务器发送参数状态和就绪信号

2. 查询执行阶段：
   - 客户端发送SQL查询
   - 服务器返回列元数据
   - 服务器返回数据行
   - 服务器返回命令完成消息
   - 服务器发送就绪信号

3. 连接关闭：
   - 客户端发送终止消息

这是PostgreSQL的文本协议，还有一个扩展查询协议用于预处理语句。
```

The protocol is defined in `src/backend/libpq/` and documented in the official
documentation. Key files:
- `src/backend/libpq/pqcomm.c` - Communication primitives
- `src/backend/libpq/auth.c` - Authentication
- `src/interfaces/libpq/` - Client library (libpq)

---

## 3.2 Major Subsystems

PostgreSQL can be understood as several cooperating subsystems:

```
+------------------------------------------------------------------+
|                         SQL Query                                |
+------------------------------------------------------------------+
                               |
                               v
+------------------------------------------------------------------+
|                          Parser                                  |
|  src/backend/parser/                                             |
|  - Lexical analysis (scan.l)                                     |
|  - Grammar (gram.y)                                              |
|  - Parse tree construction                                       |
+------------------------------------------------------------------+
                               |
                               v
+------------------------------------------------------------------+
|                     Analyzer / Rewriter                          |
|  src/backend/parser/analyze.c                                    |
|  src/backend/rewrite/                                            |
|  - Semantic analysis                                             |
|  - View expansion                                                |
|  - Rule application                                              |
+------------------------------------------------------------------+
                               |
                               v
+------------------------------------------------------------------+
|                    Planner / Optimizer                           |
|  src/backend/optimizer/                                          |
|  - Generate possible plans                                       |
|  - Estimate costs                                                |
|  - Choose cheapest plan                                          |
+------------------------------------------------------------------+
                               |
                               v
+------------------------------------------------------------------+
|                         Executor                                 |
|  src/backend/executor/                                           |
|  - Execute the plan                                              |
|  - Tuple-at-a-time processing                                    |
|  - Return results to client                                      |
+------------------------------------------------------------------+
                               |
                               v
+------------------------------------------------------------------+
|                     Access Methods                               |
|  src/backend/access/                                             |
|  - Heap (table storage)                                          |
|  - Index (B-tree, Hash, GiST, GIN, BRIN)                         |
|  - Transaction management                                        |
+------------------------------------------------------------------+
                               |
                               v
+------------------------------------------------------------------+
|                    Storage Manager                               |
|  src/backend/storage/                                            |
|  - Buffer management                                             |
|  - File management                                               |
|  - Lock management                                               |
+------------------------------------------------------------------+
                               |
                               v
+------------------------------------------------------------------+
|                     Operating System                             |
|  - File I/O                                                      |
|  - Memory (mmap, shared memory)                                  |
|  - Process management                                            |
+------------------------------------------------------------------+

PostgreSQL主要子系统（从上到下的数据流）：

1. Parser（解析器）src/backend/parser/
   - 词法分析：将SQL文本分解为标记
   - 语法分析：根据语法规则构建解析树
   - 输出：原始解析树

2. Analyzer/Rewriter（分析器/重写器）
   - 语义分析：检查表名、列名是否存在
   - 视图展开：将视图替换为其定义
   - 规则应用：应用用户定义的规则
   - 输出：查询树

3. Planner/Optimizer（计划器/优化器）src/backend/optimizer/
   - 生成可能的执行计划
   - 估算每个计划的代价
   - 选择代价最低的计划
   - 输出：执行计划

4. Executor（执行器）src/backend/executor/
   - 执行计划
   - 逐元组处理
   - 返回结果给客户端

5. Access Methods（访问方法）src/backend/access/
   - 堆表存储
   - 索引访问（B-tree, Hash, GiST, GIN, BRIN）
   - 事务管理

6. Storage Manager（存储管理器）src/backend/storage/
   - 缓冲区管理
   - 文件管理
   - 锁管理
```

### Parser

The parser transforms SQL text into a parse tree:

```
Input:  "SELECT name FROM users WHERE id = 1"

                    +-------------+
                    | SelectStmt  |
                    +------+------+
                           |
          +----------------+----------------+
          |                |                |
    +-----+-----+    +-----+-----+    +-----+-----+
    | targetList|    | fromClause|    | whereClause|
    +-----------+    +-----------+    +-----------+
          |                |                |
    +-----+-----+    +-----+-----+    +-----+-----+
    | ColumnRef |    | RangeVar  |    |  A_Expr   |
    |  "name"   |    |  "users"  |    |  id = 1   |
    +-----------+    +-----------+    +-----------+

解析树示例：

对于SQL: SELECT name FROM users WHERE id = 1

解析器生成一个SelectStmt节点，包含：
- targetList: 要选择的列（name）
- fromClause: 要查询的表（users）
- whereClause: 过滤条件（id = 1）

这个树形结构是后续处理的基础。
解析器只检查语法，不检查表或列是否存在。
```

Key files:
- `src/backend/parser/scan.l` - Lexer (flex)
- `src/backend/parser/gram.y` - Grammar (bison)
- `src/backend/parser/analyze.c` - Semantic analysis

### Planner / Optimizer

The planner converts a query tree into an execution plan:

```
Query: SELECT * FROM orders o JOIN customers c ON o.cust_id = c.id
       WHERE c.country = 'US'

Possible Plans:

Plan A: Hash Join                    Plan B: Nested Loop
+------------------+                 +------------------+
|    Hash Join     |                 |   Nested Loop    |
+--------+---------+                 +--------+---------+
         |                                    |
    +----+----+                          +----+----+
    |         |                          |         |
+---+---+ +---+---+                  +---+---+ +---+---+
|  Seq  | | Hash  |                  | Index | |  Seq  |
| Scan  | |       |                  | Scan  | | Scan  |
|orders | |       |                  |orders | |custom.|
+-------+ +---+---+                  +-------+ +-------+
              |
          +---+---+
          |  Seq  |
          | Scan  |
          |custom.|
          +-------+

Estimated Costs:
Plan A: 1500 (chosen if tables are large)
Plan B: 800 (chosen if customers table is small with good index)

执行计划示例：

对于一个JOIN查询，优化器考虑多种执行方式：

计划A: Hash Join
- 扫描customers表，构建哈希表
- 扫描orders表，用哈希表查找匹配行
- 适合：两个大表连接

计划B: Nested Loop
- 扫描customers表
- 对每一行，用索引在orders表中查找匹配
- 适合：一个小表，另一个表有好的索引

优化器为每个计划估算代价，选择代价最低的。
代价基于：磁盘I/O次数、CPU操作次数、结果行数估计等。
```

Key files:
- `src/backend/optimizer/path/` - Path generation
- `src/backend/optimizer/plan/` - Plan creation
- `src/backend/optimizer/util/` - Cost estimation

### Executor

The executor runs the plan and produces results:

```
+------------------+
|   ExecutorStart  |  Initialize state
+--------+---------+
         |
         v
+------------------+
|   ExecutorRun    |  Main execution loop
+--------+---------+
         |
         | (for each tuple)
         v
    +----+----+
    | Fetch   |
    | Tuple   |-----> Return to client
    +---------+
         |
         v
+------------------+
|   ExecutorEnd    |  Clean up
+------------------+

执行器工作流程：

1. ExecutorStart - 初始化执行状态
   - 分配内存
   - 打开表和索引
   - 初始化节点状态

2. ExecutorRun - 主执行循环
   - 反复获取元组
   - 应用过滤条件
   - 返回结果给客户端

3. ExecutorEnd - 清理
   - 关闭表和索引
   - 释放内存

PostgreSQL使用"火山模型"（Volcano model）执行器：
每个计划节点实现一个迭代器接口，按需产生元组。
```

The executor uses the "Volcano" or "iterator" model: each plan node implements
a simple interface (`Init`, `GetNext`, `Close`), and tuples flow up the tree
one at a time.

Key files:
- `src/backend/executor/execMain.c` - Main execution
- `src/backend/executor/execProcnode.c` - Node dispatch
- `src/backend/executor/nodeSeqscan.c` - Sequential scan (example node)

### Storage Engine

The storage subsystem manages data on disk:

```
+------------------------------------------------------------------+
|                      Storage Architecture                         |
+------------------------------------------------------------------+
|                                                                  |
|  +--------------------+                                          |
|  |   Buffer Manager   |  src/backend/storage/buffer/             |
|  |  +--------------+  |                                          |
|  |  | Shared       |  |  Caches disk pages in memory             |
|  |  | Buffer Pool  |  |                                          |
|  |  +--------------+  |                                          |
|  +--------------------+                                          |
|            |                                                     |
|            v                                                     |
|  +--------------------+                                          |
|  |   SMGR Layer       |  src/backend/storage/smgr/               |
|  |  (Storage Manager) |  Abstract interface to storage           |
|  +--------------------+                                          |
|            |                                                     |
|            v                                                     |
|  +--------------------+                                          |
|  |   MD (Magnetic     |  src/backend/storage/smgr/md.c           |
|  |       Disk)        |  Actual file operations                  |
|  +--------------------+                                          |
|            |                                                     |
|            v                                                     |
|  +--------------------+                                          |
|  |   File System      |                                          |
|  |   (ext4, xfs, etc) |                                          |
|  +--------------------+                                          |
|                                                                  |
+------------------------------------------------------------------+

存储架构层次：

1. Buffer Manager（缓冲管理器）
   - 在内存中缓存磁盘页
   - 管理页的读入和写出
   - 实现LRU等替换策略

2. SMGR Layer（存储管理器层）
   - 提供抽象的存储接口
   - 支持不同的存储后端

3. MD（Magnetic Disk）
   - 实际的文件操作
   - 将页面映射到文件

4. File System
   - 操作系统的文件系统
```

### Transaction Manager

Handles ACID properties:

```
+------------------------------------------------------------------+
|                   Transaction Manager                             |
+------------------------------------------------------------------+
|                                                                  |
|  src/backend/access/transam/                                     |
|                                                                  |
|  +--------------------+  +--------------------+                  |
|  |     CLOG           |  |     WAL            |                  |
|  | (Commit Log)       |  | (Write-Ahead Log)  |                  |
|  |                    |  |                    |                  |
|  | Tracks which XIDs  |  | Ensures durability |                  |
|  | committed/aborted  |  | before data pages  |                  |
|  +--------------------+  +--------------------+                  |
|                                                                  |
|  +--------------------+  +--------------------+                  |
|  |   Snapshot Mgr     |  |   Lock Manager     |                  |
|  |                    |  |                    |                  |
|  | Which tuples are   |  | Row, table, and    |                  |
|  | visible to whom    |  | advisory locks     |                  |
|  +--------------------+  +--------------------+                  |
|                                                                  |
+------------------------------------------------------------------+

事务管理器组件：

1. CLOG (Commit Log)
   - 记录每个事务ID的状态
   - 状态：进行中、已提交、已中止
   - 用于判断元组可见性

2. WAL (Write-Ahead Log)
   - 确保持久性
   - 在修改数据页之前先写日志
   - 崩溃恢复的基础

3. Snapshot Manager（快照管理器）
   - 确定哪些元组对哪些事务可见
   - 实现MVCC的关键

4. Lock Manager（锁管理器）
   - 行级锁、表级锁
   - 咨询锁
   - 死锁检测
```

### WAL (Write-Ahead Logging)

Ensures durability and enables recovery:

```
Write Path:
+----------+     +------------+     +------------+
|  Modify  | --> | WAL Buffer | --> | WAL on     |
|  Data    |     |            |     | Disk       |
+----------+     +------------+     +------------+
     |                                    |
     v                                    |
+----------+     +------------+           |
|  Buffer  | --> | Data on    | <---------+
|  Pool    |     | Disk       |   (recovery replays WAL)
+----------+     +------------+

WAL写入路径：

1. 修改数据 -> 将变更记录写入WAL缓冲
2. WAL缓冲 -> 刷写到WAL磁盘文件（先于数据页）
3. 同时修改缓冲池中的数据页
4. 数据页稍后由checkpoint刷写到磁盘
5. 崩溃时：重放WAL恢复数据页

关键原则："先写日志"
- 任何数据修改，必须先将日志写入磁盘
- 只有日志持久化后，事务才算提交
- 这保证了即使崩溃也不会丢失已提交事务
```

Key files:
- `src/backend/access/transam/xlog.c` - WAL core
- `src/backend/access/transam/xloginsert.c` - WAL record creation
- `src/backend/access/transam/xlogrecovery.c` - Recovery

---

## Summary

```
+------------------------------------------------------------------+
|              PostgreSQL Architecture Summary                      |
+------------------------------------------------------------------+
|                                                                  |
|  Process Model:                                                  |
|  - postmaster forks backend per connection                       |
|  - Backends share memory (buffers, locks, etc.)                  |
|  - Background workers for maintenance tasks                      |
|                                                                  |
|  Query Flow:                                                     |
|  SQL -> Parser -> Analyzer -> Planner -> Executor -> Storage     |
|                                                                  |
|  Key Subsystems:                                                 |
|  - Parser: SQL text to parse tree                                |
|  - Optimizer: Choose best execution plan                         |
|  - Executor: Run plan, return results                            |
|  - Storage: Buffer pages, manage files                           |
|  - Transaction: ACID, WAL, MVCC                                  |
|                                                                  |
+------------------------------------------------------------------+

PostgreSQL架构总结：

进程模型：
- postmaster为每个连接fork一个后端进程
- 后端进程通过共享内存通信
- 后台工作进程负责维护任务

查询流程：
SQL -> 解析 -> 分析 -> 计划 -> 执行 -> 存储

关键子系统：
- 解析器：SQL文本转解析树
- 优化器：选择最佳执行计划
- 执行器：运行计划，返回结果
- 存储：缓冲页面，管理文件
- 事务：ACID、WAL、MVCC

下一节我们将详细探索源代码目录结构。
```

In the next section, we will tour the PostgreSQL source tree and see where
each subsystem lives in the code.
