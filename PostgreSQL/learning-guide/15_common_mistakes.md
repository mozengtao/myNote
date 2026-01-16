# Section 15: Common Beginner Mistakes (Critical)

These are the mistakes I see most often. Understanding WHY they are mistakes
(connecting to internals) helps you avoid them.

---

## 15.1 Ignoring Transactions

```
Mistake: Auto-commit by default
+------------------------------------------------------------------+
|                                                                  |
|  -- Many drivers auto-commit each statement                      |
|  UPDATE accounts SET balance = balance - 100 WHERE id = 1;       |
|  -- COMMIT (implicit)                                            |
|  UPDATE accounts SET balance = balance + 100 WHERE id = 2;       |
|  -- COMMIT (implicit)                                            |
|                                                                  |
|  Problem: If second UPDATE fails, first is already committed!    |
|  Money vanished from the system.                                 |
+------------------------------------------------------------------+

Fix: Explicit transactions
+------------------------------------------------------------------+
|                                                                  |
|  BEGIN;                                                          |
|  UPDATE accounts SET balance = balance - 100 WHERE id = 1;       |
|  UPDATE accounts SET balance = balance + 100 WHERE id = 2;       |
|  COMMIT;                                                         |
|                                                                  |
|  If any statement fails, ROLLBACK everything.                    |
+------------------------------------------------------------------+

忽略事务的错误：

问题：
- 许多驱动默认自动提交
- 多语句操作不是原子的
- 失败时数据不一致

解决：
- 明确使用BEGIN/COMMIT
- 相关操作放在同一事务
- 使用ORM的事务支持
```

---

## 15.2 Overusing ORMs Blindly

```
ORM N+1 Query Problem:
+------------------------------------------------------------------+
|                                                                  |
|  # Python/SQLAlchemy example                                     |
|  orders = session.query(Order).all()  # 1 query                  |
|  for order in orders:                                            |
|      print(order.customer.name)       # N queries!               |
|                                                                  |
|  SQL Generated:                                                  |
|  SELECT * FROM orders;                                           |
|  SELECT * FROM customers WHERE id = 1;                           |
|  SELECT * FROM customers WHERE id = 2;                           |
|  SELECT * FROM customers WHERE id = 3;                           |
|  ... (100 orders = 101 queries!)                                 |
+------------------------------------------------------------------+

Fix: Eager loading
+------------------------------------------------------------------+
|                                                                  |
|  orders = session.query(Order).options(                          |
|      joinedload(Order.customer)                                  |
|  ).all()                                                         |
|                                                                  |
|  SQL Generated:                                                  |
|  SELECT orders.*, customers.*                                    |
|  FROM orders                                                     |
|  LEFT JOIN customers ON orders.customer_id = customers.id;       |
|                                                                  |
|  1 query instead of 101!                                         |
+------------------------------------------------------------------+

ORM的N+1问题：

问题：
- 遍历关联对象时产生大量查询
- 100个订单 = 101次查询
- 性能极差

解决：
- 使用eager loading (joinedload, selectinload)
- 监控实际生成的SQL
- 不要盲目信任ORM
```

---

## 15.3 Missing Indexes

```
Mistake: Foreign keys without indexes
+------------------------------------------------------------------+
|                                                                  |
|  CREATE TABLE orders (                                           |
|      id SERIAL PRIMARY KEY,                                      |
|      customer_id INTEGER REFERENCES customers(id)                |
|      -- No index on customer_id!                                 |
|  );                                                              |
|                                                                  |
|  Problem 1: Slow lookups                                         |
|  SELECT * FROM orders WHERE customer_id = 100;                   |
|  -- Full table scan! (no index)                                  |
|                                                                  |
|  Problem 2: Slow deletes from parent                             |
|  DELETE FROM customers WHERE id = 100;                           |
|  -- Must scan ALL orders to check FK constraint!                 |
+------------------------------------------------------------------+

Fix: Index foreign key columns
+------------------------------------------------------------------+
|                                                                  |
|  CREATE INDEX idx_orders_customer_id ON orders(customer_id);     |
|                                                                  |
|  Now both operations use index scan.                             |
+------------------------------------------------------------------+

缺少索引：

问题：
- PostgreSQL不自动为外键创建索引
- 查询外键列需要全表扫描
- 删除父表记录需要扫描子表检查约束

解决：
- 为外键列创建索引
- 为WHERE子句中的列创建索引
- 使用EXPLAIN检查索引使用情况
```

---

## 15.4 Using SERIAL Incorrectly

```
Mistake: Assuming SERIAL is always sequential
+------------------------------------------------------------------+
|                                                                  |
|  CREATE TABLE orders (id SERIAL PRIMARY KEY);                    |
|                                                                  |
|  BEGIN; INSERT INTO orders DEFAULT VALUES; ROLLBACK;             |
|  -- Sequence incremented but rolled back!                        |
|                                                                  |
|  BEGIN; INSERT INTO orders DEFAULT VALUES; COMMIT;               |
|  -- id = 2, not 1!                                               |
|                                                                  |
|  SERIAL values:                                                  |
|  - Not guaranteed sequential (gaps from rollbacks)               |
|  - Not guaranteed in order (concurrent inserts)                  |
|  - Generated BEFORE insert (can fail after)                      |
+------------------------------------------------------------------+

Mistake: Using SERIAL for distributed systems
+------------------------------------------------------------------+
|                                                                  |
|  In distributed systems, SERIAL causes:                          |
|  - Single point of contention (sequence)                         |
|  - Collisions when merging databases                             |
|                                                                  |
|  Better: UUID or ULID                                            |
|  CREATE TABLE orders (                                           |
|      id UUID DEFAULT gen_random_uuid() PRIMARY KEY               |
|  );                                                              |
|                                                                  |
|  Or use a distributed ID generator                               |
+------------------------------------------------------------------+

SERIAL的误解：

误解1：SERIAL是连续的
- 回滚会跳过ID
- 并发插入顺序不确定

误解2：SERIAL适合分布式系统
- 序列是单点
- 合并数据库时冲突

何时使用什么：
- 单机、无需连续：SERIAL
- 分布式：UUID
- 需要排序：ULID或类似
```

---

## 15.5 Misunderstanding NULL

```
NULL Gotchas:
+------------------------------------------------------------------+
|                                                                  |
|  -- NULL is not equal to anything, including NULL!               |
|  SELECT NULL = NULL;   -- Returns NULL, not TRUE!                |
|  SELECT NULL <> NULL;  -- Returns NULL, not TRUE!                |
|                                                                  |
|  -- Must use IS NULL / IS NOT NULL                               |
|  SELECT * FROM users WHERE email IS NULL;                        |
|                                                                  |
|  -- NULL in comparisons                                          |
|  SELECT * FROM users WHERE email = 'test@example.com';           |
|  -- Does NOT return rows where email IS NULL                     |
|                                                                  |
|  -- NULL in aggregates                                           |
|  SELECT COUNT(*) FROM users;      -- Counts all rows             |
|  SELECT COUNT(email) FROM users;  -- Excludes NULL emails        |
|                                                                  |
|  -- NULL in UNIQUE constraints                                   |
|  CREATE TABLE t (email TEXT UNIQUE);                             |
|  INSERT INTO t VALUES (NULL);  -- OK                             |
|  INSERT INTO t VALUES (NULL);  -- Also OK! (NULL != NULL)        |
+------------------------------------------------------------------+

NULL的陷阱：

1. NULL的比较
   - NULL = NULL 返回NULL，不是TRUE
   - 使用IS NULL / IS NOT NULL

2. WHERE子句
   - column = 'value' 不包含NULL行
   - 需要显式: column = 'value' OR column IS NULL

3. 聚合函数
   - COUNT(*) 计算所有行
   - COUNT(column) 排除NULL

4. UNIQUE约束
   - NULL不等于NULL
   - 多个NULL不违反UNIQUE
   - 如需禁止多个NULL: 添加partial unique index
```

---

## 15.6 Not Using EXPLAIN

```
Mistake: Guessing performance issues
+------------------------------------------------------------------+
|                                                                  |
|  "The query is slow, let me add more indexes"                    |
|  -- Wrong approach!                                              |
|                                                                  |
|  Right approach:                                                 |
|  1. EXPLAIN ANALYZE the query                                    |
|  2. Identify the actual bottleneck                               |
|  3. Fix the specific issue                                       |
|                                                                  |
+------------------------------------------------------------------+

Common EXPLAIN findings:
+------------------------------------------------------------------+
|                                                                  |
|  Symptom                    | Cause              | Fix           |
|  ---------------------------|--------------------| --------------|
|  Seq Scan on large table    | Missing index      | Add index     |
|  rows=1000 actual=1000000   | Stale statistics   | ANALYZE       |
|  Sort, very slow            | work_mem too low   | Increase it   |
|  Nested Loop, many loops    | Bad join order     | Check stats   |
|  Bitmap Heap Scan slow      | Many matching rows | Different idx |
|                                                                  |
+------------------------------------------------------------------+

不使用EXPLAIN的错误：

错误做法：
- 猜测性能问题
- 随便添加索引
- 忽略实际执行计划

正确做法：
1. EXPLAIN ANALYZE查询
2. 找到真正的瓶颈
3. 针对性修复

常见发现：
- Seq Scan: 可能需要索引
- 估计行数与实际差距大: 需要ANALYZE
- Sort太慢: work_mem太小
- Nested Loop循环多: 连接顺序问题
```

---

## 15.7 Forgetting VACUUM

```
Mistake: Disabling autovacuum
+------------------------------------------------------------------+
|                                                                  |
|  "VACUUM is slowing down my queries, let me disable it"          |
|                                                                  |
|  Consequences:                                                   |
|  1. Table bloat: Dead tuples accumulate, table grows             |
|  2. Index bloat: Indexes point to dead tuples                    |
|  3. Slow queries: Must scan more pages                           |
|  4. Transaction ID wraparound: DATABASE SHUTDOWN!                |
|                                                                  |
+------------------------------------------------------------------+

Transaction ID Wraparound:
+------------------------------------------------------------------+
|                                                                  |
|  PostgreSQL uses 32-bit transaction IDs (2 billion)              |
|  IDs wrap around eventually                                      |
|                                                                  |
|  VACUUM freezes old transactions:                                |
|  - Replaces XID with FrozenTransactionId                         |
|  - Tuple always visible to future transactions                   |
|                                                                  |
|  Without VACUUM:                                                 |
|  - XID approaches wraparound limit                               |
|  - PostgreSQL enters "read-only mode"                            |
|  - Must run VACUUM to recover                                    |
|                                                                  |
+------------------------------------------------------------------+

忘记VACUUM的错误：

错误做法：
- 禁用autovacuum
- 忽视表膨胀警告

后果：
1. 表膨胀：死元组累积
2. 索引膨胀：索引指向死元组
3. 查询变慢：需要扫描更多页
4. XID回卷：数据库只读！

正确做法：
- 保持autovacuum开启
- 监控pg_stat_user_tables
- 高更新表可能需要更激进的autovacuum设置
```

---

## Summary

```
+------------------------------------------------------------------+
|              Common Mistakes Summary                             |
+------------------------------------------------------------------+
|                                                                  |
|  1. Ignoring Transactions                                        |
|     - Use explicit BEGIN/COMMIT for related operations           |
|                                                                  |
|  2. Overusing ORMs                                               |
|     - Watch for N+1 queries                                      |
|     - Use eager loading                                          |
|                                                                  |
|  3. Missing Indexes                                              |
|     - Index foreign key columns                                  |
|     - Index WHERE clause columns                                 |
|                                                                  |
|  4. SERIAL Misuse                                                |
|     - Gaps are normal                                            |
|     - Use UUID for distributed systems                           |
|                                                                  |
|  5. NULL Misunderstanding                                        |
|     - Use IS NULL, not = NULL                                    |
|     - Understand NULL in aggregates                              |
|                                                                  |
|  6. Not Using EXPLAIN                                            |
|     - Always EXPLAIN before optimizing                           |
|     - Use EXPLAIN ANALYZE for actual stats                       |
|                                                                  |
|  7. Forgetting VACUUM                                            |
|     - Never disable autovacuum                                   |
|     - Monitor table bloat                                        |
|                                                                  |
+------------------------------------------------------------------+

常见错误总结：

1. 忽略事务 - 使用显式事务
2. 盲目使用ORM - 注意N+1问题
3. 缺少索引 - 索引外键和WHERE列
4. SERIAL误用 - 分布式用UUID
5. NULL误解 - 使用IS NULL
6. 不用EXPLAIN - 优化前先分析
7. 忘记VACUUM - 不要禁用autovacuum
```
