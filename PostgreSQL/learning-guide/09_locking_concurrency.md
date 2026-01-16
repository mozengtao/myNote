# Section 9: Locking and Concurrency

While MVCC eliminates most read-write conflicts, PostgreSQL still needs locks
for certain operations. This section explains what locks exist and when they
are used.

---

## 9.1 What MVCC Solves vs What Locks Are Needed For

```
+------------------------------------------------------------------+
|                    MVCC Handles:                                 |
+------------------------------------------------------------------+
|  - Read-write conflicts (readers see old version)                |
|  - Consistent snapshots for queries                              |
|  - Non-blocking reads                                            |
+------------------------------------------------------------------+

+------------------------------------------------------------------+
|                    Locks Still Needed For:                       |
+------------------------------------------------------------------+
|  - Write-write conflicts (two writers on same row)               |
|  - DDL operations (ALTER TABLE, DROP TABLE)                      |
|  - Explicit locking (SELECT FOR UPDATE)                          |
|  - Index structure modifications                                 |
|  - Internal data structure protection                            |
+------------------------------------------------------------------+

MVCC处理的问题：
- 读写冲突（读者看到旧版本）
- 查询的一致性快照
- 非阻塞读取

仍然需要锁的场景：
- 写写冲突（两个写者修改同一行）
- DDL操作（修改表结构）
- 显式锁定（SELECT FOR UPDATE）
- 索引结构修改
- 内部数据结构保护
```

---

## 9.2 Lock Types in PostgreSQL

### Table-Level Locks

```
Table Lock Modes (from weakest to strongest):
+------------------------------------------------------------------+
|                                                                  |
|  Mode                  | Conflicts With                         |
|  ----------------------|----------------------------------------|
|  ACCESS SHARE          | ACCESS EXCLUSIVE                       |
|  ROW SHARE             | EXCLUSIVE, ACCESS EXCLUSIVE            |
|  ROW EXCLUSIVE         | SHARE, SHARE ROW EXCLUSIVE,            |
|                        | EXCLUSIVE, ACCESS EXCLUSIVE            |
|  SHARE UPDATE EXCLUSIVE| SHARE UPDATE EXCLUSIVE, SHARE,         |
|                        | SHARE ROW EXCLUSIVE, EXCLUSIVE,        |
|                        | ACCESS EXCLUSIVE                       |
|  SHARE                 | ROW EXCLUSIVE, SHARE UPDATE EXCLUSIVE, |
|                        | SHARE ROW EXCLUSIVE, EXCLUSIVE,        |
|                        | ACCESS EXCLUSIVE                       |
|  SHARE ROW EXCLUSIVE   | ROW EXCLUSIVE, SHARE UPDATE EXCLUSIVE, |
|                        | SHARE, SHARE ROW EXCLUSIVE,            |
|                        | EXCLUSIVE, ACCESS EXCLUSIVE            |
|  EXCLUSIVE             | ROW SHARE, ROW EXCLUSIVE,              |
|                        | SHARE UPDATE EXCLUSIVE, SHARE,         |
|                        | SHARE ROW EXCLUSIVE, EXCLUSIVE,        |
|                        | ACCESS EXCLUSIVE                       |
|  ACCESS EXCLUSIVE      | All modes                              |
|                                                                  |
+------------------------------------------------------------------+

表级锁模式（从弱到强）：

ACCESS SHARE (SELECT)
- 最弱的锁
- 只与ACCESS EXCLUSIVE冲突
- SELECT自动获取

ROW SHARE (SELECT FOR UPDATE)
- 准备修改行
- 与EXCLUSIVE和ACCESS EXCLUSIVE冲突

ROW EXCLUSIVE (INSERT/UPDATE/DELETE)
- 修改数据
- 与SHARE及以上冲突

SHARE (CREATE INDEX)
- 读取整个表
- 阻止并发修改

ACCESS EXCLUSIVE (ALTER TABLE, DROP TABLE)
- 最强的锁
- 与所有锁冲突
- 阻止所有访问
```

### Lock Mode Compatibility Matrix

```
+----------------------------------------------------------------+
|           AS   RS   RX  SUX    S  SRX    X   AX                |
+----------------------------------------------------------------+
| AS        OK   OK   OK   OK   OK   OK   OK   --                |
| RS        OK   OK   OK   OK   OK   OK   --   --                |
| RX        OK   OK   OK   OK   --   --   --   --                |
| SUX       OK   OK   OK   --   --   --   --   --                |
| S         OK   OK   --   --   OK   --   --   --                |
| SRX       OK   OK   --   --   --   --   --   --                |
| X         OK   --   --   --   --   --   --   --                |
| AX        --   --   --   --   --   --   --   --                |
+----------------------------------------------------------------+
AS = ACCESS SHARE        SUX = SHARE UPDATE EXCLUSIVE
RS = ROW SHARE           S = SHARE
RX = ROW EXCLUSIVE       SRX = SHARE ROW EXCLUSIVE
X = EXCLUSIVE            AX = ACCESS EXCLUSIVE

OK = Compatible (both can proceed)
-- = Conflict (one must wait)

锁兼容性矩阵：

OK = 兼容（两者可同时持有）
-- = 冲突（一方必须等待）

关键点：
- SELECT只与DDL冲突
- INSERT/UPDATE/DELETE与CREATE INDEX冲突
- DDL与一切冲突

这解释了为什么：
- 高并发下DDL会造成阻塞
- VACUUM不阻塞SELECT
- CREATE INDEX阻塞写入
```

### Common Operations and Their Locks

```sql
-- Operation                     | Lock Acquired
-- ------------------------------|------------------------
SELECT                           -- ACCESS SHARE
SELECT FOR UPDATE                -- ROW SHARE (table) + row lock
SELECT FOR SHARE                 -- ROW SHARE (table) + row lock
INSERT                           -- ROW EXCLUSIVE
UPDATE                           -- ROW EXCLUSIVE + row lock
DELETE                           -- ROW EXCLUSIVE + row lock
VACUUM                           -- SHARE UPDATE EXCLUSIVE
CREATE INDEX CONCURRENTLY        -- SHARE UPDATE EXCLUSIVE
CREATE INDEX                     -- SHARE
ALTER TABLE                      -- ACCESS EXCLUSIVE
DROP TABLE                       -- ACCESS EXCLUSIVE
TRUNCATE                         -- ACCESS EXCLUSIVE
REINDEX                          -- SHARE (or ACCESS EXCLUSIVE)

常见操作的锁：

读操作：
- SELECT: ACCESS SHARE（最弱）
- SELECT FOR UPDATE: ROW SHARE + 行锁

写操作：
- INSERT/UPDATE/DELETE: ROW EXCLUSIVE

维护操作：
- VACUUM: SHARE UPDATE EXCLUSIVE
- CREATE INDEX CONCURRENTLY: SHARE UPDATE EXCLUSIVE
- CREATE INDEX: SHARE（阻塞写）

DDL操作：
- ALTER TABLE/DROP TABLE/TRUNCATE: ACCESS EXCLUSIVE（阻塞一切）
```

---

## 9.3 Row-Level Locks

```
Row Lock Modes:
+------------------------------------------------------------------+
|                                                                  |
|  Mode          | Acquired By            | Conflicts With         |
|  --------------|------------------------|------------------------|
|  FOR KEY SHARE | Foreign key check      | FOR UPDATE             |
|  FOR SHARE     | SELECT FOR SHARE       | FOR UPDATE,            |
|                |                        | FOR NO KEY UPDATE      |
|  FOR NO KEY    | UPDATE (no key change) | FOR SHARE,             |
|  UPDATE        |                        | FOR NO KEY UPDATE,     |
|                |                        | FOR UPDATE             |
|  FOR UPDATE    | SELECT FOR UPDATE,     | All row lock modes     |
|                | UPDATE (key change),   |                        |
|                | DELETE                 |                        |
|                                                                  |
+------------------------------------------------------------------+

行级锁模式：

FOR KEY SHARE：
- 外键检查时获取
- 最弱的行锁
- 只阻止行被删除或主键被修改

FOR SHARE：
- SELECT FOR SHARE获取
- 阻止行被修改

FOR NO KEY UPDATE：
- 普通UPDATE获取（不修改主键）
- 阻止SELECT FOR SHARE和更强的锁

FOR UPDATE：
- 最强的行锁
- SELECT FOR UPDATE、DELETE获取
- 与所有行锁冲突
```

### Row Lock Implementation

```
Row locks are stored in the tuple header (xmax + infomask):
+------------------------------------------------------------------+
|                                                                  |
|  Tuple Header:                                                   |
|  +-------+-------+-----------+                                   |
|  | xmin  | xmax  | infomask  |                                   |
|  +-------+-------+-----------+                                   |
|                      |                                           |
|                      +-- HEAP_XMAX_LOCK_ONLY                     |
|                          HEAP_XMAX_KEYSHR_LOCK                   |
|                          HEAP_XMAX_EXCL_LOCK                     |
|                          etc.                                    |
|                                                                  |
|  When xmax is set for locking (not deletion):                    |
|  - HEAP_XMAX_LOCK_ONLY flag is set                               |
|  - Lock type indicated by other flags                            |
|  - xmax contains the locking transaction ID                      |
|                                                                  |
+------------------------------------------------------------------+

行锁实现：

行锁存储在元组头部：
- xmax字段存储锁定事务ID
- infomask标志位指示锁类型
- HEAP_XMAX_LOCK_ONLY表示这是锁，不是删除

这种设计意味着：
- 行锁信息存储在数据页中
- 不需要单独的锁表
- 但锁定的行会在页中修改（WAL开销）

MultiXact：
- 当多个事务锁定同一行时
- 使用MultiXact ID代替单个事务ID
- MultiXact是事务ID的集合
```

---

## 9.4 Deadlock Detection

```
Deadlock Example:
+------------------------------------------------------------------+
|  Transaction 1              | Transaction 2                      |
|  ---------------------------|----------------------------------- |
|  BEGIN;                     | BEGIN;                             |
|  UPDATE t SET ... WHERE     |                                    |
|    id = 1;                  |                                    |
|  -- Holds lock on row 1     |                                    |
|                             | UPDATE t SET ... WHERE             |
|                             |   id = 2;                          |
|                             | -- Holds lock on row 2             |
|  UPDATE t SET ... WHERE     |                                    |
|    id = 2;                  |                                    |
|  -- WAITING for T2          |                                    |
|                             | UPDATE t SET ... WHERE             |
|                             |   id = 1;                          |
|                             | -- WAITING for T1                  |
|                             |                                    |
|  -- DEADLOCK!               | -- DEADLOCK!                       |
|                             |                                    |
|  PostgreSQL detects this    |                                    |
|  and aborts one transaction |                                    |
+------------------------------------------------------------------+

死锁示例：

T1持有行1的锁，等待行2
T2持有行2的锁，等待行1
-> 死锁！

PostgreSQL的死锁检测：
1. 事务等待锁时启动计时器
2. deadlock_timeout（默认1秒）后检查
3. 构建等待图（wait-for graph）
4. 检测图中的环
5. 选择一个事务中止

被中止的事务收到错误：
ERROR: deadlock detected
DETAIL: Process X waits for ShareLock on transaction Y;
        Process Y waits for ShareLock on transaction X.
```

### Avoiding Deadlocks

```
Best Practices:
+------------------------------------------------------------------+
|                                                                  |
|  1. Access tables in consistent order                            |
|     - Always lock table A before table B                         |
|     - Document and enforce this convention                       |
|                                                                  |
|  2. Access rows in consistent order                              |
|     - Sort rows by primary key before locking                    |
|     - SELECT ... ORDER BY id FOR UPDATE                          |
|                                                                  |
|  3. Keep transactions short                                      |
|     - Less time holding locks = fewer conflicts                  |
|                                                                  |
|  4. Use appropriate isolation level                              |
|     - READ COMMITTED often sufficient                            |
|     - SERIALIZABLE may cause more conflicts                      |
|                                                                  |
|  5. Retry on deadlock                                            |
|     - Application should catch and retry                         |
|                                                                  |
+------------------------------------------------------------------+

避免死锁的最佳实践：

1. 按一致顺序访问表
   - 所有代码先锁表A再锁表B
   - 文档化并执行此约定

2. 按一致顺序访问行
   - 按主键排序后再锁定
   - SELECT ... ORDER BY id FOR UPDATE

3. 保持事务简短
   - 持锁时间短 = 冲突少

4. 使用适当的隔离级别
   - READ COMMITTED通常足够
   - SERIALIZABLE可能更多冲突

5. 死锁时重试
   - 应用层捕获错误并重试
```

Source: `src/backend/storage/lmgr/deadlock.c`

---

## 9.5 Advisory Locks

```
Advisory Locks: Application-defined locks
+------------------------------------------------------------------+
|                                                                  |
|  Regular Locks:                                                  |
|  - Automatically acquired/released by PostgreSQL                 |
|  - Based on database objects (tables, rows)                      |
|                                                                  |
|  Advisory Locks:                                                 |
|  - Explicitly acquired/released by application                   |
|  - Based on arbitrary integers                                   |
|  - PostgreSQL doesn't know what they mean                        |
|                                                                  |
+------------------------------------------------------------------+

咨询锁（Advisory Locks）：

用途：
- 应用层的分布式锁
- 保护非数据库资源
- 协调多个进程/服务器

特点：
- 锁ID是任意整数
- PostgreSQL不理解其含义
- 完全由应用控制
```

```sql
-- Session-level advisory lock (released at session end)
SELECT pg_advisory_lock(12345);    -- Acquire lock
SELECT pg_advisory_unlock(12345);  -- Release lock

-- Transaction-level advisory lock (released at transaction end)
SELECT pg_advisory_xact_lock(12345);

-- Non-blocking version (returns false if can't acquire)
SELECT pg_try_advisory_lock(12345);

-- Example: Ensure only one worker processes a job
SELECT pg_try_advisory_lock(job_id) FROM jobs WHERE status = 'pending' LIMIT 1;
-- If returns true, this worker got the job
-- If returns false, another worker is processing it
```

---

## 9.6 Monitoring Locks

```sql
-- View current locks
SELECT locktype, relation::regclass, mode, granted, pid
FROM pg_locks
WHERE NOT granted;

-- Find blocking queries
SELECT
    blocked_locks.pid AS blocked_pid,
    blocked_activity.usename AS blocked_user,
    blocking_locks.pid AS blocking_pid,
    blocking_activity.usename AS blocking_user,
    blocked_activity.query AS blocked_query,
    blocking_activity.query AS blocking_query
FROM pg_catalog.pg_locks blocked_locks
JOIN pg_catalog.pg_stat_activity blocked_activity
    ON blocked_activity.pid = blocked_locks.pid
JOIN pg_catalog.pg_locks blocking_locks
    ON blocking_locks.locktype = blocked_locks.locktype
    AND blocking_locks.database IS NOT DISTINCT FROM blocked_locks.database
    AND blocking_locks.relation IS NOT DISTINCT FROM blocked_locks.relation
    AND blocking_locks.page IS NOT DISTINCT FROM blocked_locks.page
    AND blocking_locks.tuple IS NOT DISTINCT FROM blocked_locks.tuple
    AND blocking_locks.virtualxid IS NOT DISTINCT FROM blocked_locks.virtualxid
    AND blocking_locks.transactionid IS NOT DISTINCT FROM blocked_locks.transactionid
    AND blocking_locks.classid IS NOT DISTINCT FROM blocked_locks.classid
    AND blocking_locks.objid IS NOT DISTINCT FROM blocked_locks.objid
    AND blocking_locks.objsubid IS NOT DISTINCT FROM blocked_locks.objsubid
    AND blocking_locks.pid != blocked_locks.pid
JOIN pg_catalog.pg_stat_activity blocking_activity
    ON blocking_activity.pid = blocking_locks.pid
WHERE NOT blocked_locks.granted;

-- View lock wait time
SELECT pid, wait_event_type, wait_event, state, query,
       now() - query_start AS duration
FROM pg_stat_activity
WHERE wait_event_type = 'Lock';
```

---

## Summary

```
+------------------------------------------------------------------+
|              Locking and Concurrency Summary                     |
+------------------------------------------------------------------+
|                                                                  |
|  Lock Hierarchy:                                                 |
|  - Table-level locks: Control access to entire tables            |
|  - Row-level locks: Control access to specific rows              |
|  - Advisory locks: Application-defined locks                     |
|                                                                  |
|  Key Points:                                                     |
|  - MVCC handles read-write conflicts                             |
|  - Locks handle write-write conflicts and DDL                    |
|  - Table locks: 8 modes from ACCESS SHARE to ACCESS EXCLUSIVE    |
|  - Row locks: 4 modes from FOR KEY SHARE to FOR UPDATE           |
|  - Deadlock detection runs after deadlock_timeout                |
|                                                                  |
|  Best Practices:                                                 |
|  - Keep transactions short                                       |
|  - Access resources in consistent order                          |
|  - Use appropriate lock modes                                    |
|  - Monitor with pg_locks and pg_stat_activity                    |
|  - Handle deadlocks in application code                          |
|                                                                  |
+------------------------------------------------------------------+

锁和并发总结：

锁层次：
- 表级锁：控制对整个表的访问
- 行级锁：控制对特定行的访问
- 咨询锁：应用定义的锁

关键点：
- MVCC处理读写冲突
- 锁处理写写冲突和DDL
- 表锁8种模式
- 行锁4种模式
- 死锁检测在deadlock_timeout后运行

最佳实践：
- 保持事务简短
- 按一致顺序访问资源
- 使用适当的锁模式
- 监控pg_locks和pg_stat_activity
- 应用层处理死锁

源代码位置：
- src/backend/storage/lmgr/lock.c - 锁管理器
- src/backend/storage/lmgr/deadlock.c - 死锁检测
- src/backend/storage/lmgr/proc.c - 进程和等待

下一节我们将学习WAL（预写日志），这是持久性的核心机制。
```
