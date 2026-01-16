# Section 14: Using PostgreSQL in Real Projects

This section covers practical patterns for production PostgreSQL usage.

---

## 14.1 Typical Application Architecture

```
Application Stack with PostgreSQL:
+------------------------------------------------------------------+
|                                                                  |
|  +------------------+                                            |
|  |  Load Balancer   |                                            |
|  +--------+---------+                                            |
|           |                                                      |
|     +-----+-----+                                                |
|     |           |                                                |
|  +--+---+   +---+--+                                             |
|  | App1 |   | App2 |   (Stateless application servers)           |
|  +--+---+   +---+--+                                             |
|     |           |                                                |
|     +-----+-----+                                                |
|           |                                                      |
|  +--------+---------+                                            |
|  | Connection Pool  |  (PgBouncer or similar)                    |
|  +--------+---------+                                            |
|           |                                                      |
|  +--------+---------+                                            |
|  |   PostgreSQL     |                                            |
|  |   Primary        |                                            |
|  +--------+---------+                                            |
|           |                                                      |
|           | Streaming Replication                                |
|           |                                                      |
|  +--------+---------+                                            |
|  |   PostgreSQL     |  (Read replicas)                           |
|  |   Standby        |                                            |
|  +------------------+                                            |
|                                                                  |
+------------------------------------------------------------------+

典型应用架构：

应用层：
- 无状态应用服务器
- 可水平扩展
- 不维护连接状态

连接池：
- PgBouncer最常用
- 复用数据库连接
- 解决"连接过多"问题

PostgreSQL：
- 主库处理写操作
- 备库处理读操作（可选）
- 流复制保持同步
```

### Connection Pooling (Critical for Production)

```
Why Connection Pooling is Essential:
+------------------------------------------------------------------+
|                                                                  |
|  Without Pool:                                                   |
|  100 app instances x 10 connections each = 1000 PostgreSQL       |
|  connections = 1000 backend processes = HIGH MEMORY USAGE        |
|                                                                  |
|  With PgBouncer:                                                 |
|  100 app instances -> PgBouncer (100 connections) -> PostgreSQL  |
|                                          (50 connections)        |
|                                                                  |
|  PgBouncer Pooling Modes:                                        |
|  +-------------+---------------------------------------------+   |
|  | session     | Connection bound to client session (safest) |   |
|  | transaction | Connection bound to transaction (recommended)|  |
|  | statement   | Connection released after each statement    |   |
|  +-------------+---------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+

连接池的必要性：

问题：
- PostgreSQL每连接一个进程
- 进程占用内存（约10MB/连接）
- 100应用 x 10连接 = 1000进程 = 10GB内存

解决方案：PgBouncer
- 复用连接
- 100应用连接 -> 50数据库连接
- 显著减少资源使用

池化模式：
- session: 连接绑定到会话（最安全，效率低）
- transaction: 连接绑定到事务（推荐）
- statement: 每条语句后释放（有限制）

注意：transaction模式不支持：
- 预处理语句（跨事务）
- LISTEN/NOTIFY
- 临时表
```

---

## 14.2 Performance Practices

### Using EXPLAIN ANALYZE

```sql
-- Basic EXPLAIN
EXPLAIN SELECT * FROM orders WHERE customer_id = 100;

-- With actual execution statistics
EXPLAIN ANALYZE SELECT * FROM orders WHERE customer_id = 100;

-- With buffer usage
EXPLAIN (ANALYZE, BUFFERS) SELECT * FROM orders WHERE customer_id = 100;

-- Full verbose output
EXPLAIN (ANALYZE, BUFFERS, FORMAT TEXT)
SELECT * FROM orders WHERE customer_id = 100;
```

```
Reading EXPLAIN Output:
+------------------------------------------------------------------+
|                                                                  |
|  Index Scan using orders_customer_id_idx on orders               |
|    (cost=0.43..8.45 rows=10 width=40)                            |
|    (actual time=0.025..0.031 rows=8 loops=1)                     |
|    Index Cond: (customer_id = 100)                               |
|    Buffers: shared hit=4                                         |
|  Planning Time: 0.152 ms                                         |
|  Execution Time: 0.058 ms                                        |
|                                                                  |
|  Reading:                                                        |
|  - Index Scan: Using index (good!)                               |
|  - cost=0.43..8.45: Estimated cost (startup..total)              |
|  - rows=10: Estimated rows                                       |
|  - actual rows=8: Real rows returned                             |
|  - Buffers: shared hit=4: 4 pages from buffer cache              |
|  - No shared read: All pages were cached (good!)                 |
|                                                                  |
+------------------------------------------------------------------+

EXPLAIN输出解读：

Index Scan：使用索引（好）
Seq Scan：全表扫描（大表时注意）

cost=0.43..8.45：
- 0.43: 启动代价
- 8.45: 总代价

rows=10 vs actual rows=8：
- 估计10行，实际8行
- 差距大时需要ANALYZE更新统计

Buffers:
- shared hit: 从缓存读取
- shared read: 从磁盘读取
- hit多read少 = 缓存效果好
```

### Common Performance Patterns

```sql
-- 1. Avoid SELECT * in production
-- Bad:
SELECT * FROM large_table WHERE status = 'active';
-- Good (only needed columns):
SELECT id, name FROM large_table WHERE status = 'active';

-- 2. Use covering indexes for index-only scans
CREATE INDEX idx_orders_customer_total
ON orders (customer_id) INCLUDE (total_amount);
-- Now can answer this without hitting heap:
SELECT customer_id, total_amount FROM orders WHERE customer_id = 100;

-- 3. Use partial indexes for filtered queries
CREATE INDEX idx_orders_pending
ON orders (created_at) WHERE status = 'pending';
-- Only indexes pending orders, much smaller

-- 4. Batch operations
-- Bad (1000 individual INSERTs):
INSERT INTO log VALUES (1, 'msg1');
INSERT INTO log VALUES (2, 'msg2');
-- ...

-- Good (batch INSERT):
INSERT INTO log VALUES (1, 'msg1'), (2, 'msg2'), ...;
-- Or use COPY for bulk loading
```

---

## 14.3 Safety and Correctness

### Transaction Patterns

```sql
-- Always use explicit transactions for multi-statement operations
BEGIN;
UPDATE accounts SET balance = balance - 100 WHERE id = 1;
UPDATE accounts SET balance = balance + 100 WHERE id = 2;
COMMIT;

-- Handle errors properly
BEGIN;
-- ... operations ...
-- If error occurs, ROLLBACK automatically (on connection close)
-- Or explicitly:
ROLLBACK;
```

### Avoiding Race Conditions

```sql
-- Problem: Read-Modify-Write race
-- Session 1:                      Session 2:
-- SELECT balance FROM accounts;   SELECT balance FROM accounts;
-- -- sees 100                     -- sees 100
-- UPDATE balance = 100 - 10;      UPDATE balance = 100 - 10;
-- -- Final: 90 (should be 80!)

-- Solution 1: SELECT FOR UPDATE
BEGIN;
SELECT balance FROM accounts WHERE id = 1 FOR UPDATE;
-- Row is locked, other sessions wait
UPDATE accounts SET balance = balance - 10 WHERE id = 1;
COMMIT;

-- Solution 2: Atomic UPDATE (preferred for simple cases)
UPDATE accounts SET balance = balance - 10 WHERE id = 1;
-- Single statement is atomic, no race condition

-- Solution 3: Optimistic locking with version column
UPDATE accounts
SET balance = balance - 10, version = version + 1
WHERE id = 1 AND version = 5;
-- If affected rows = 0, someone else modified, retry
```

### Choosing Isolation Levels

```sql
-- Default READ COMMITTED: Good for most cases
BEGIN;
-- Sees committed data, may see changes mid-transaction
COMMIT;

-- REPEATABLE READ: When you need consistent view
BEGIN ISOLATION LEVEL REPEATABLE READ;
-- Sees snapshot from transaction start
-- May get serialization error on conflict
COMMIT;

-- SERIALIZABLE: When correctness is critical
BEGIN ISOLATION LEVEL SERIALIZABLE;
-- True serializable isolation
-- Higher chance of serialization errors, must retry
COMMIT;

-- Retry pattern for REPEATABLE READ / SERIALIZABLE
-- Application code:
-- while True:
--     try:
--         begin transaction
--         execute operations
--         commit
--         break
--     except SerializationError:
--         rollback
--         continue  # retry
```

---

## Summary

```
+------------------------------------------------------------------+
|           Using PostgreSQL in Real Projects Summary              |
+------------------------------------------------------------------+
|                                                                  |
|  Architecture:                                                   |
|  - Use connection pooling (PgBouncer)                            |
|  - Consider read replicas for scaling reads                      |
|  - Stateless application servers                                 |
|                                                                  |
|  Performance:                                                    |
|  - Use EXPLAIN ANALYZE to understand queries                     |
|  - Create indexes for WHERE and JOIN columns                     |
|  - Use covering indexes for index-only scans                     |
|  - Batch operations where possible                               |
|  - Avoid SELECT *                                                |
|                                                                  |
|  Safety:                                                         |
|  - Always use transactions for related changes                   |
|  - Use SELECT FOR UPDATE or atomic updates for races             |
|  - Choose appropriate isolation level                            |
|  - Handle serialization errors in application                    |
|                                                                  |
+------------------------------------------------------------------+

生产环境使用总结：

架构：
- 使用连接池
- 考虑读副本
- 无状态应用服务器

性能：
- 使用EXPLAIN ANALYZE
- 为WHERE和JOIN列创建索引
- 使用覆盖索引
- 批量操作
- 避免SELECT *

安全性：
- 始终使用事务
- 处理竞态条件
- 选择合适的隔离级别
- 处理序列化错误
```
