# Section 8: Transactions and MVCC (Core Topic)

MVCC (Multi-Version Concurrency Control) is the heart of PostgreSQL's
concurrency model. Understanding MVCC is essential for understanding
PostgreSQL's behavior.

---

## 8.1 ACID Properties

Every database claims ACID compliance. Here's what it actually means:

```
+------------------------------------------------------------------+
|                        ACID Properties                           |
+------------------------------------------------------------------+
|                                                                  |
|  A - Atomicity                                                   |
|      "All or nothing"                                            |
|      A transaction either completes entirely or has no effect    |
|                                                                  |
|  C - Consistency                                                 |
|      "Valid state to valid state"                                |
|      Transactions preserve database invariants (constraints)     |
|                                                                  |
|  I - Isolation                                                   |
|      "Concurrent transactions don't interfere"                   |
|      Each transaction sees a consistent view of data             |
|                                                                  |
|  D - Durability                                                  |
|      "Committed data survives crashes"                           |
|      Once committed, data is permanently stored                  |
|                                                                  |
+------------------------------------------------------------------+

ACID属性详解：

A - 原子性：
  要么全做，要么全不做
  转账：扣款和加款必须一起成功或一起失败
  PostgreSQL实现：WAL + 回滚

C - 一致性：
  从一个有效状态到另一个有效状态
  约束（主键、外键、CHECK）始终满足
  PostgreSQL实现：约束检查 + 事务回滚

I - 隔离性：
  并发事务互不干扰
  每个事务看到一致的数据视图
  PostgreSQL实现：MVCC + 快照

D - 持久性：
  已提交的数据不会丢失
  即使立即崩溃，数据也能恢复
  PostgreSQL实现：WAL + fsync
```

### How PostgreSQL Enforces ACID

```
Atomicity:
+------------------------------------------------------------------+
| BEGIN;                                                           |
| UPDATE accounts SET balance = balance - 100 WHERE id = 1;        |
| UPDATE accounts SET balance = balance + 100 WHERE id = 2;        |
| -- If crash here, both updates are rolled back via WAL           |
| COMMIT;                                                          |
| -- If crash here, both updates are recovered via WAL             |
+------------------------------------------------------------------+

原子性实现：
- 事务内的修改先写入WAL
- 只有COMMIT时才标记事务为已提交
- 崩溃恢复时，未提交事务的修改被回滚

Isolation (MVCC):
+------------------------------------------------------------------+
|  Transaction 1         | Transaction 2                          |
|  xid = 100             | xid = 101                               |
| -----------------------|---------------------------------------- |
|  BEGIN;                |                                         |
|  SELECT * FROM t;      |                                         |
|  -- sees version A     |                                         |
|                        | BEGIN;                                  |
|                        | UPDATE t SET x = 'B';                   |
|                        | COMMIT;                                 |
|  SELECT * FROM t;      |                                         |
|  -- STILL sees A!      |                                         |
|  -- T1's snapshot was  |                                         |
|  -- taken at BEGIN     |                                         |
|  COMMIT;               |                                         |
+------------------------------------------------------------------+

隔离性实现（MVCC）：
- 每个事务获得一个快照
- 快照记录事务开始时的活跃事务
- 事务只能看到快照之前已提交的数据
- 即使其他事务提交了新版本，当前事务仍看旧版本

Durability (WAL):
+------------------------------------------------------------------+
|  1. Write WAL record to WAL buffer                               |
|  2. At COMMIT: flush WAL buffer to disk (fsync)                  |
|  3. Return success to client                                     |
|  4. Data pages written to disk later (checkpoint)                |
|                                                                  |
|  If crash before step 4:                                         |
|  - WAL is intact (flushed at commit)                             |
|  - Recovery replays WAL to reconstruct data pages                |
+------------------------------------------------------------------+

持久性实现（WAL）：
- 修改先写入WAL缓冲
- COMMIT时WAL强制刷盘（fsync）
- 数据页稍后异步写入
- 崩溃时通过WAL重放恢复数据
```

---

## 8.2 MVCC Design

### The Problem with Locking

```
Traditional Locking:
+------------------------------------------------------------------+
|  Writer                 | Reader                                 |
| ------------------------|----------------------------------------|
|  BEGIN;                 |                                        |
|  UPDATE users SET ...   |                                        |
|  -- Acquires X lock     |                                        |
|                         | BEGIN;                                 |
|                         | SELECT * FROM users;                   |
|                         | -- BLOCKED! Waiting for X lock         |
|                         | -- (cannot read while write in progress)|
|  ...                    |                                        |
|  (slow operation)       |                                        |
|  ...                    |                                        |
|  COMMIT;                |                                        |
|  -- Releases X lock     |                                        |
|                         | -- NOW can proceed                     |
|                         | -- Returns data                        |
+------------------------------------------------------------------+

传统锁的问题：

读-写冲突：
- 写者获取排他锁
- 读者必须等待写者完成
- 写操作越慢，阻塞越严重

写-写冲突：
- 两个写者必须串行执行
- 吞吐量受限

这种方式简单但不适合高并发场景。
```

### MVCC: Multi-Version Concurrency Control

```
MVCC Approach:
+------------------------------------------------------------------+
|  Writer                 | Reader                                 |
| ------------------------|----------------------------------------|
|  BEGIN; (xid=100)       |                                        |
|  UPDATE users SET ...   |                                        |
|  -- Creates NEW version |                                        |
|  -- Old version remains |                                        |
|                         | BEGIN; (xid=101)                       |
|                         | SELECT * FROM users;                   |
|                         | -- NOT blocked!                        |
|                         | -- Reads OLD version (xmax not set)    |
|                         | -- Returns immediately                 |
|  ...                    |                                        |
|  COMMIT;                |                                        |
|                         | SELECT * FROM users;                   |
|                         | -- Still sees old version!             |
|                         | -- (snapshot taken at BEGIN)           |
|                         | COMMIT;                                |
+------------------------------------------------------------------+

MVCC方法：

关键思想：
- 不修改旧版本，创建新版本
- 旧版本保留供并发读者使用
- 读者不阻塞写者，写者不阻塞读者

工作原理：
1. UPDATE创建新元组，设置旧元组的xmax
2. 读者检查元组的xmin/xmax判断可见性
3. 如果xmin已提交且xmax未设置/未提交，元组可见
4. VACUUM清理不再需要的旧版本
```

### Tuple Versioning

```
Initial State:
+------------------------------------------------------------------+
|  users table, Row id=1                                           |
|                                                                  |
|  +-------+-------+-------+------+-------------------------------+|
|  | xmin  | xmax  | ctid  | id   | name                          ||
|  |-------+-------+-------+------+-------------------------------||
|  | 50    | 0     | (0,1) | 1    | 'Alice'                       ||
|  +-------+-------+-------+------+-------------------------------+|
|                                                                  |
|  xmin=50: Inserted by transaction 50 (committed)                 |
|  xmax=0: Not deleted                                             |
+------------------------------------------------------------------+

After UPDATE by Transaction 100:
+------------------------------------------------------------------+
|  UPDATE users SET name = 'Alicia' WHERE id = 1;                  |
|                                                                  |
|  Old tuple (marked deleted):                                     |
|  +-------+-------+-------+------+-------------------------------+|
|  | xmin  | xmax  | ctid  | id   | name                          ||
|  |-------+-------+-------+------+-------------------------------||
|  | 50    | 100   | (0,2) | 1    | 'Alice'                       ||
|  +-------+-------+-------+------+-------------------------------+|
|      ^      ^       ^                                            |
|      |      |       +-- Points to new version                    |
|      |      +-- Deleted by transaction 100                       |
|      +-- Originally inserted by transaction 50                   |
|                                                                  |
|  New tuple (new version):                                        |
|  +-------+-------+-------+------+-------------------------------+|
|  | xmin  | xmax  | ctid  | id   | name                          ||
|  |-------+-------+-------+------+-------------------------------||
|  | 100   | 0     | (0,2) | 1    | 'Alicia'                      ||
|  +-------+-------+-------+------+-------------------------------+|
|      ^                                                           |
|      +-- Inserted by transaction 100                             |
+------------------------------------------------------------------+

元组版本控制：

UPDATE操作：
1. 不修改原元组
2. 创建新元组，xmin = 当前事务ID
3. 将原元组的xmax设为当前事务ID
4. 原元组的ctid指向新元组

DELETE操作：
1. 只设置xmax为当前事务ID
2. 不创建新版本
3. 元组物理上仍存在

INSERT操作：
1. 创建新元组
2. xmin = 当前事务ID
3. xmax = 0

这就是为什么PostgreSQL的UPDATE是"DELETE + INSERT"，
会增加表大小，需要VACUUM清理。
```

### Visibility Rules

```
Visibility Check (simplified):
+------------------------------------------------------------------+
|                                                                  |
|  For a tuple to be visible to transaction T:                     |
|                                                                  |
|  1. xmin must be committed (or be T itself)                      |
|     AND                                                          |
|  2. xmin must be < T's snapshot                                  |
|     AND                                                          |
|  3. xmax must be 0 OR uncommitted OR >= T's snapshot             |
|                                                                  |
+------------------------------------------------------------------+

Example:
+------------------------------------------------------------------+
|  Current Transaction: xid = 150                                  |
|  Snapshot: xmin=140 (oldest active when T started)               |
|                                                                  |
|  Tuple A: xmin=100, xmax=0                                       |
|  -> xmin 100 committed? Yes                                      |
|  -> xmin 100 < 140? Yes                                          |
|  -> xmax=0? Yes                                                  |
|  -> VISIBLE                                                      |
|                                                                  |
|  Tuple B: xmin=145, xmax=0                                       |
|  -> xmin 145 committed? Yes                                      |
|  -> xmin 145 < 140? No (145 >= 140)                              |
|  -> NOT VISIBLE (created after our snapshot)                     |
|                                                                  |
|  Tuple C: xmin=100, xmax=130                                     |
|  -> xmin 100 committed? Yes                                      |
|  -> xmin 100 < 140? Yes                                          |
|  -> xmax 130 committed? Yes                                      |
|  -> xmax 130 < 140? Yes                                          |
|  -> NOT VISIBLE (deleted before our snapshot)                    |
|                                                                  |
|  Tuple D: xmin=100, xmax=145                                     |
|  -> xmin 100 committed? Yes                                      |
|  -> xmax 145 committed? Yes                                      |
|  -> xmax 145 >= 140? Yes                                         |
|  -> VISIBLE (deletion not yet visible to us)                     |
+------------------------------------------------------------------+

可见性规则：

对于事务T，元组可见的条件：
1. xmin已提交（或xmin是T自己）
2. xmin在T的快照之前
3. xmax为0，或未提交，或在T的快照之后

快照的关键概念：
- 快照记录事务开始时的状态
- 包含当时所有活跃事务的列表
- 活跃事务的修改不可见

这就是"事务隔离"的实现机制。
```

Source: `src/backend/access/heap/heapam_visibility.c`

---

## 8.3 Transaction Isolation Levels

```
SQL Standard Isolation Levels:
+------------------------------------------------------------------+
|                                                                  |
|  Level            | Dirty  | Non-Repeatable | Phantom           |
|                   | Read   | Read           | Read              |
|  -----------------|--------|----------------|-------------------|
|  READ UNCOMMITTED | Yes    | Yes            | Yes               |
|  READ COMMITTED   | No     | Yes            | Yes               |
|  REPEATABLE READ  | No     | No             | Yes               |
|  SERIALIZABLE     | No     | No             | No                |
|                                                                  |
+------------------------------------------------------------------+

PostgreSQL Implementation:
+------------------------------------------------------------------+
|                                                                  |
|  Level            | PostgreSQL Behavior                         |
|  -----------------|---------------------------------------------|
|  READ UNCOMMITTED | Same as READ COMMITTED (no dirty reads!)    |
|  READ COMMITTED   | Snapshot per statement                      |
|  REPEATABLE READ  | Snapshot per transaction                    |
|  SERIALIZABLE     | True serializability (SSI)                  |
|                                                                  |
+------------------------------------------------------------------+

隔离级别：

READ UNCOMMITTED：
- SQL标准允许脏读
- PostgreSQL实现为READ COMMITTED（不允许脏读）

READ COMMITTED（默认）：
- 每条语句一个新快照
- 两次SELECT可能看到不同结果
- 如果之间有其他事务提交

REPEATABLE READ：
- 整个事务一个快照
- 事务内看到一致的数据视图
- 可能遇到序列化失败错误

SERIALIZABLE：
- 真正的可序列化隔离
- 使用SSI（Serializable Snapshot Isolation）
- 可能需要重试事务
```

### Read Committed vs Repeatable Read

```
READ COMMITTED (default):
+------------------------------------------------------------------+
|  Transaction 1             | Transaction 2                       |
| ---------------------------|-------------------------------------|
|  BEGIN;                    |                                     |
|  SELECT count(*) FROM t;   |                                     |
|  -- Returns 100            |                                     |
|                            | BEGIN;                              |
|                            | INSERT INTO t VALUES (...);         |
|                            | COMMIT;                             |
|  SELECT count(*) FROM t;   |                                     |
|  -- Returns 101!           |                                     |
|  -- (new snapshot)         |                                     |
|  COMMIT;                   |                                     |
+------------------------------------------------------------------+

REPEATABLE READ:
+------------------------------------------------------------------+
|  Transaction 1             | Transaction 2                       |
| ---------------------------|-------------------------------------|
|  BEGIN ISOLATION LEVEL     |                                     |
|    REPEATABLE READ;        |                                     |
|  SELECT count(*) FROM t;   |                                     |
|  -- Returns 100            |                                     |
|                            | BEGIN;                              |
|                            | INSERT INTO t VALUES (...);         |
|                            | COMMIT;                             |
|  SELECT count(*) FROM t;   |                                     |
|  -- Still returns 100!     |                                     |
|  -- (same snapshot)        |                                     |
|  COMMIT;                   |                                     |
+------------------------------------------------------------------+

READ COMMITTED vs REPEATABLE READ：

READ COMMITTED：
- 每条语句获取新快照
- 能看到其他已提交事务的修改
- 两次查询可能结果不同
- 适合大多数应用场景

REPEATABLE READ：
- 事务开始时获取快照
- 整个事务看到一致的数据
- 不受其他事务提交影响
- 适合需要一致性视图的场景

选择建议：
- 默认使用READ COMMITTED
- 需要事务内一致性时用REPEATABLE READ
- 需要真正序列化时用SERIALIZABLE
```

### Serialization Failures

```
REPEATABLE READ can fail:
+------------------------------------------------------------------+
|  Transaction 1             | Transaction 2                       |
| ---------------------------|-------------------------------------|
|  BEGIN ISOLATION LEVEL     | BEGIN ISOLATION LEVEL               |
|    REPEATABLE READ;        |   REPEATABLE READ;                  |
|  SELECT * FROM accounts    | SELECT * FROM accounts              |
|    WHERE id = 1;           |   WHERE id = 1;                     |
|  -- balance = 100          | -- balance = 100                    |
|  UPDATE accounts           |                                     |
|    SET balance = 50        |                                     |
|    WHERE id = 1;           |                                     |
|  COMMIT;                   |                                     |
|                            | UPDATE accounts                     |
|                            |   SET balance = 80                  |
|                            |   WHERE id = 1;                     |
|                            | -- ERROR: could not serialize       |
|                            | --   access due to concurrent       |
|                            | --   update                         |
+------------------------------------------------------------------+

序列化失败：

当两个REPEATABLE READ事务尝试修改同一行时：
- 第一个成功提交
- 第二个得到序列化错误

应用必须准备好重试事务：

while (true) {
    try {
        execute_transaction();
        break;
    } catch (SerializationError) {
        // Retry the transaction
        continue;
    }
}

这是MVCC的正确行为，不是bug。
```

---

## 8.4 VACUUM: Cleaning Up Old Versions

MVCC creates multiple versions. Old versions must be cleaned up.

```
Why VACUUM is Necessary:
+------------------------------------------------------------------+
|                                                                  |
|  Without VACUUM:                                                 |
|  - Dead tuples accumulate                                        |
|  - Table grows without bound                                     |
|  - Scans become slower                                           |
|  - Transaction ID wraparound danger                              |
|                                                                  |
+------------------------------------------------------------------+

What VACUUM Does:
+------------------------------------------------------------------+
|                                                                  |
|  Before VACUUM:                                                  |
|  +-------+-------+-------+-------+-------+                       |
|  | Live  | Dead  | Live  | Dead  | Dead  |                       |
|  +-------+-------+-------+-------+-------+                       |
|                                                                  |
|  After VACUUM:                                                   |
|  +-------+-------+-------+-------+-------+                       |
|  | Live  | Free  | Live  | Free  | Free  |                       |
|  +-------+-------+-------+-------+-------+                       |
|           ^               ^       ^                              |
|           |               |       |                              |
|           +-- Space can be reused for new tuples                 |
|                                                                  |
|  Note: VACUUM does NOT shrink the table file!                    |
|  Use VACUUM FULL to reclaim disk space (locks table).            |
+------------------------------------------------------------------+

VACUUM的必要性：

问题：
- MVCC产生旧版本元组
- 旧版本占用空间
- 查询需要跳过旧版本
- 事务ID会耗尽（wraparound）

VACUUM的作用：
- 标记死元组空间为可重用
- 更新FSM（空闲空间映射）
- 更新VM（可见性映射）
- 冻结旧事务ID（防止wraparound）

VACUUM vs VACUUM FULL：
- VACUUM：不锁表，不缩小文件，空间可重用
- VACUUM FULL：锁表，重写表文件，真正回收空间
```

### Autovacuum

```
Autovacuum Configuration:
+------------------------------------------------------------------+
|                                                                  |
|  Parameter                 | Default | Description               |
|  --------------------------|---------|---------------------------|
|  autovacuum                | on      | Enable autovacuum         |
|  autovacuum_vacuum_threshold| 50     | Min dead tuples           |
|  autovacuum_vacuum_scale_factor| 0.2 | Fraction of table         |
|  autovacuum_naptime        | 1min    | Time between runs         |
|                                                                  |
|  Trigger condition:                                              |
|  dead_tuples > threshold + scale_factor * table_size             |
|                                                                  |
|  Example: 10,000 row table                                       |
|  Vacuum triggers when: dead_tuples > 50 + 0.2 * 10000 = 2050     |
|                                                                  |
+------------------------------------------------------------------+

Autovacuum配置：

触发条件：
死元组数 > threshold + scale_factor * 表行数

默认设置：
- 阈值：50
- 比例因子：0.2（20%）
- 检查间隔：1分钟

对于10000行的表：
- 当死元组超过2050时触发
- 即约20%的行被更新/删除

调优建议：
- 高更新表：降低scale_factor
- 大表：增加naptime
- 关键表：配置单独的autovacuum参数
```

---

## Summary

```
+------------------------------------------------------------------+
|              Transactions and MVCC Summary                       |
+------------------------------------------------------------------+
|                                                                  |
|  ACID Implementation:                                            |
|  - Atomicity: WAL + rollback                                     |
|  - Consistency: Constraints + transaction rollback               |
|  - Isolation: MVCC + snapshots                                   |
|  - Durability: WAL + fsync                                       |
|                                                                  |
|  MVCC Principles:                                                |
|  - Readers don't block writers                                   |
|  - Writers don't block readers                                   |
|  - Each transaction sees a consistent snapshot                   |
|  - Old versions kept until no longer needed                      |
|                                                                  |
|  Tuple Versioning:                                               |
|  - xmin: Transaction that created the tuple                      |
|  - xmax: Transaction that deleted/updated the tuple              |
|  - Visibility determined by comparing xmin/xmax with snapshot    |
|                                                                  |
|  VACUUM:                                                         |
|  - Cleans up dead tuples                                         |
|  - Autovacuum runs automatically                                 |
|  - Essential for preventing bloat and XID wraparound             |
|                                                                  |
+------------------------------------------------------------------+

事务和MVCC总结：

ACID实现：
- 原子性：WAL + 回滚
- 一致性：约束 + 事务回滚
- 隔离性：MVCC + 快照
- 持久性：WAL + fsync

MVCC原则：
- 读者不阻塞写者
- 写者不阻塞读者
- 每个事务看到一致的快照
- 旧版本保留直到不再需要

元组版本控制：
- xmin：创建元组的事务
- xmax：删除/更新元组的事务
- 可见性由xmin/xmax与快照比较决定

VACUUM：
- 清理死元组
- autovacuum自动运行
- 防止表膨胀和XID回卷

关键源文件：
- src/backend/access/heap/heapam_visibility.c - 可见性判断
- src/backend/utils/time/snapmgr.c - 快照管理
- src/backend/commands/vacuum.c - VACUUM实现
- src/backend/access/transam/xact.c - 事务管理

下一节我们将学习锁机制，了解MVCC之外还需要什么锁。
```
