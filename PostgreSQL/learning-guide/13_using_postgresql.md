# Section 13: Using PostgreSQL as a User (Practical Layer)

Now that you understand the internals, let's connect them to practical usage.

---

## 13.1 Basic SQL Usage

### Tables and Rows (Connected to Internals)

```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    email TEXT UNIQUE,
    created_at TIMESTAMPTZ DEFAULT now()
);

-- What happens internally:
-- 1. Parser: Validates syntax
-- 2. Analyzer: Checks types, resolves names
-- 3. Executor: Creates heap file, creates system catalog entries
-- 4. Creates B-tree index for PRIMARY KEY (users_pkey)
-- 5. Creates B-tree index for UNIQUE (users_email_key)
```

```
Internal Storage After CREATE TABLE:
+------------------------------------------------------------------+
|                                                                  |
|  System Catalogs Updated:                                        |
|  - pg_class: New entry for 'users'                               |
|  - pg_attribute: Entries for id, name, email, created_at         |
|  - pg_type: Uses existing types (int4, text, timestamptz)        |
|  - pg_index: Entries for primary key and unique indexes          |
|                                                                  |
|  Files Created:                                                  |
|  - base/<dboid>/<relfilenode>      (heap file)                   |
|  - base/<dboid>/<relfilenode>_fsm  (free space map)              |
|  - base/<dboid>/<relfilenode>_vm   (visibility map)              |
|  - base/<dboid>/<idx_relfilenode>  (primary key index)           |
|  - base/<dboid>/<idx_relfilenode>  (unique index)                |
|                                                                  |
+------------------------------------------------------------------+

CREATE TABLE内部过程：

系统目录更新：
- pg_class: 表的元数据
- pg_attribute: 列定义
- pg_index: 索引信息

创建的文件：
- 堆文件：存储表数据
- FSM：空闲空间映射
- VM：可见性映射
- 索引文件：B-tree索引
```

### INSERT and MVCC

```sql
INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com');
```

```
INSERT Internal Flow:
+------------------------------------------------------------------+
|                                                                  |
|  1. Get new transaction ID (xid = 1000)                          |
|  2. Find page with free space (FSM lookup)                       |
|  3. Lock buffer                                                  |
|  4. Write WAL record                                             |
|  5. Insert tuple with:                                           |
|     - xmin = 1000 (current transaction)                          |
|     - xmax = 0 (not deleted)                                     |
|     - id = nextval('users_id_seq')                               |
|  6. Update indexes (B-tree insert)                               |
|  7. Mark buffer dirty                                            |
|  8. Release buffer lock                                          |
|                                                                  |
|  Page After INSERT:                                              |
|  +----------------------------------------------------------+    |
|  | [Header] [LP1] ... [free space] ... [Tuple: xmin=1000...] |   |
|  +----------------------------------------------------------+    |
|                                                                  |
+------------------------------------------------------------------+

INSERT内部流程：

1. 获取事务ID
2. 查找有空闲空间的页（通过FSM）
3. 锁定缓冲区
4. 写入WAL记录
5. 插入元组（包含xmin）
6. 更新索引
7. 标记缓冲区为脏
8. 释放锁
```

### SELECT and Visibility

```sql
SELECT * FROM users WHERE id = 1;
```

```
SELECT Internal Flow:
+------------------------------------------------------------------+
|                                                                  |
|  1. Parser: SELECT -> parse tree                                 |
|  2. Analyzer: Resolve 'users', 'id', check types                 |
|  3. Planner: Choose Index Scan on users_pkey                     |
|  4. Executor:                                                    |
|     a. Get current snapshot (list of active transactions)        |
|     b. Descend B-tree index to find id=1 -> TID(0,1)             |
|     c. Fetch tuple from heap page 0, line pointer 1              |
|     d. Visibility check:                                         |
|        - Is xmin committed? (check CLOG or hint bits)            |
|        - Is xmin in snapshot? (was it active when we started?)   |
|        - Is xmax set? Is it committed?                           |
|     e. If visible: return tuple                                  |
|     f. If not visible: skip (MVCC at work!)                      |
|                                                                  |
+------------------------------------------------------------------+

SELECT内部流程：

1-3. SQL处理流水线
4. 执行器：
   - 获取快照
   - 通过索引找到TID
   - 从堆获取元组
   - 可见性检查（MVCC核心）
   - 返回可见元组
```

---

## 13.2 Schema Design

### Normalization (Why It Matters for PostgreSQL)

```
Bad Design (Denormalized):
+------------------------------------------------------------------+
|  orders                                                          |
|  +----+-------------+-------------+--------------+               |
|  | id | customer_name| customer_email| product_name|              |
|  +----+-------------+-------------+--------------+               |
|  | 1  | Alice       | alice@x.com | Widget       |               |
|  | 2  | Alice       | alice@x.com | Gadget       |               |
|  | 3  | Bob         | bob@y.com   | Widget       |               |
|  +----+-------------+-------------+--------------+               |
|                                                                  |
|  Problems:                                                       |
|  - Redundant data (Alice's info stored twice)                    |
|  - Update anomaly (change Alice's email = update 2 rows)         |
|  - Larger tuples = fewer per page = more I/O                     |
|  - TOAST may kick in for large text columns                      |
+------------------------------------------------------------------+

Good Design (Normalized):
+------------------------------------------------------------------+
|  customers                   orders                              |
|  +----+-------+-----------+  +----+-------------+------------+   |
|  | id | name  | email     |  | id | customer_id | product_id |   |
|  +----+-------+-----------+  +----+-------------+------------+   |
|  | 1  | Alice | alice@x.com| | 1  | 1           | 1          |   |
|  | 2  | Bob   | bob@y.com |  | 2  | 1           | 2          |   |
|  +----+-------+-----------+  | 3  | 2           | 1          |   |
|                              +----+-------------+------------+   |
|                                                                  |
|  Benefits:                                                       |
|  - No redundancy                                                 |
|  - Single point of update                                        |
|  - Smaller tuples = more per page                                |
|  - Better cache utilization                                      |
+------------------------------------------------------------------+

规范化设计：

坏设计的问题：
- 数据冗余
- 更新异常
- 元组大，每页存储少
- 可能触发TOAST

好设计的优点：
- 无冗余
- 单点更新
- 元组小，每页存储多
- 更好的缓存利用
```

### Data Types (Choosing Wisely)

```
+------------------------------------------------------------------+
|  Type Choice Impact on Storage                                   |
+------------------------------------------------------------------+
|                                                                  |
|  Type        | Size    | Notes                                   |
|  ------------|---------|---------------------------------------- |
|  SMALLINT    | 2 bytes | Range: -32768 to 32767                  |
|  INTEGER     | 4 bytes | Range: -2B to 2B                        |
|  BIGINT      | 8 bytes | Range: -9.2e18 to 9.2e18                |
|  SERIAL      | 4 bytes | Auto-increment integer                  |
|  BIGSERIAL   | 8 bytes | Auto-increment bigint                   |
|  TEXT        | variable| No length limit, TOAST if > ~2KB        |
|  VARCHAR(n)  | variable| Same as TEXT internally!                |
|  CHAR(n)     | n bytes | Padded with spaces (rarely useful)      |
|  BOOLEAN     | 1 byte  | true/false/null                         |
|  TIMESTAMPTZ | 8 bytes | Timestamp with time zone (preferred)    |
|  TIMESTAMP   | 8 bytes | Without time zone (avoid!)              |
|  UUID        | 16 bytes| Good for distributed IDs                |
|  JSONB       | variable| Binary JSON, indexable                  |
|  JSON        | variable| Text JSON (use JSONB instead)           |
|                                                                  |
+------------------------------------------------------------------+

数据类型选择：

整数：
- 不要默认用BIGINT，INTEGER通常够用
- 节省空间 = 更多行per页 = 更少I/O

文本：
- TEXT和VARCHAR(n)内部一样
- VARCHAR(n)只是添加检查约束
- CHAR(n)会填充空格，通常不需要

时间：
- 始终使用TIMESTAMPTZ（带时区）
- 避免TIMESTAMP（不带时区容易出错）

JSON：
- 使用JSONB而非JSON
- JSONB可以建索引，查询更快
```

---

## 13.3 Constraints and Their Impact

```sql
-- Constraints and their internal implementation
CREATE TABLE orders (
    id SERIAL PRIMARY KEY,           -- Creates unique B-tree index
    customer_id INTEGER NOT NULL     -- Null check on insert/update
        REFERENCES customers(id),    -- Foreign key (triggers check)
    amount NUMERIC CHECK (amount > 0), -- Check constraint
    created_at TIMESTAMPTZ DEFAULT now()
);

-- FOREIGN KEY internally:
-- 1. Creates trigger on orders: check customer exists on INSERT/UPDATE
-- 2. Creates trigger on customers: check no orders on DELETE/UPDATE
-- 3. May create index on customer_id (recommended, not automatic)
```

```
Constraint Enforcement:
+------------------------------------------------------------------+
|                                                                  |
|  INSERT INTO orders (customer_id, amount) VALUES (999, 100);     |
|                                                                  |
|  Checks performed:                                               |
|  1. NOT NULL: customer_id != NULL? Yes                           |
|  2. CHECK: amount > 0? Yes (100 > 0)                             |
|  3. FOREIGN KEY: Does customer 999 exist?                        |
|     - Query: SELECT 1 FROM customers WHERE id = 999              |
|     - If no: ERROR: violates foreign key constraint              |
|  4. PRIMARY KEY: Is new id unique?                               |
|     - Check B-tree index for duplicate                           |
|                                                                  |
|  If all pass: INSERT proceeds                                    |
|  If any fail: Transaction aborts (Atomicity!)                    |
|                                                                  |
+------------------------------------------------------------------+

约束检查：

NOT NULL：插入/更新时检查
CHECK：插入/更新时评估表达式
PRIMARY KEY：通过唯一索引检查
FOREIGN KEY：查询引用表（可能很慢！）

外键性能提示：
- 在外键列上创建索引
- 否则DELETE父表需要全表扫描子表
```

---

## Summary

```
+------------------------------------------------------------------+
|              Using PostgreSQL Summary                            |
+------------------------------------------------------------------+
|                                                                  |
|  Always Think About:                                             |
|  - How query translates to execution plan                        |
|  - How data is stored (pages, tuples, TOAST)                     |
|  - How MVCC affects visibility                                   |
|  - How indexes are used and maintained                           |
|                                                                  |
|  Schema Design:                                                  |
|  - Normalize to reduce redundancy                                |
|  - Choose appropriate data types                                 |
|  - Use constraints for data integrity                            |
|  - Index foreign key columns                                     |
|                                                                  |
|  Best Practices:                                                 |
|  - Use TIMESTAMPTZ not TIMESTAMP                                 |
|  - Use JSONB not JSON                                            |
|  - Index columns used in WHERE and JOIN                          |
|  - Don't over-index (hurts writes)                               |
|                                                                  |
+------------------------------------------------------------------+

使用PostgreSQL总结：

始终考虑：
- 查询如何转换为执行计划
- 数据如何存储
- MVCC如何影响可见性
- 索引如何使用和维护

Schema设计：
- 规范化减少冗余
- 选择合适的数据类型
- 使用约束保证数据完整性
- 索引外键列

最佳实践：
- 使用TIMESTAMPTZ
- 使用JSONB
- 索引WHERE和JOIN中的列
- 不要过度索引
```
