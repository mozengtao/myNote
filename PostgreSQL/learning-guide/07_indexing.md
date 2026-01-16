# Section 7: Indexing in PostgreSQL

Indexes are critical for query performance. This section explains how PostgreSQL
indexes work internally and when to use each type.

---

## 7.1 Why Indexes Exist

### The Problem: Full Table Scans

```
Without Index:
SELECT * FROM users WHERE email = 'alice@example.com';

+------------------------------------------------------------------+
|  Table: users (1,000,000 rows)                                   |
|                                                                  |
|  +------+------+------+------+------+------+------+------+       |
|  | Row1 | Row2 | Row3 | Row4 | ... | ...  | ...  | RowN |       |
|  +------+------+------+------+------+------+------+------+       |
|    ^                                                    ^        |
|    |_____________ Scan ALL rows _______________________|         |
|                                                                  |
|  Time Complexity: O(n)                                           |
|  For 1M rows: ~1,000,000 comparisons                             |
|  Disk I/O: Read every page (~125,000 pages at 8KB)               |
+------------------------------------------------------------------+

没有索引时的问题：

对于100万行的表，查找一个email需要：
- 扫描所有100万行
- 读取约125,000个页面（每页8KB）
- 即使只返回1行，也要扫描全表

这就是全表扫描（Sequential Scan）的代价。
```

### The Solution: Indexes

```
With B-tree Index on email:

+------------------------------------------------------------------+
|                     B-tree Index                                 |
|                                                                  |
|                      [d-m]                                       |
|                     /     \                                      |
|                    /       \                                     |
|               [a-c]         [n-z]                                |
|              /  |  \       /  |  \                               |
|             /   |   \     /   |   \                              |
|          [a-b][c-c][d-d][n-p][q-s][t-z]                          |
|            |                                                     |
|            v                                                     |
|      [alice@..., TID(5,3)]  --> Go directly to page 5, slot 3    |
|                                                                  |
|  Time Complexity: O(log n)                                       |
|  For 1M rows: ~20 comparisons                                    |
|  Disk I/O: ~3-4 pages (tree depth) + 1 heap page                 |
+------------------------------------------------------------------+

使用索引：

B-tree索引将查找复杂度从O(n)降到O(log n)：
- 100万行：约20次比较
- 读取约3-4个索引页 + 1个数据页
- 总共约4-5次磁盘I/O

索引条目包含：
- 索引键值（email）
- TID（页号, 行指针号）

通过TID可以直接定位到数据行。
```

### Trade-offs of Indexes

```
+------------------------------------------------------------------+
|                    Index Trade-offs                              |
+------------------------------------------------------------------+
|                                                                  |
|  Benefit              |  Cost                                    |
|  ---------------------|----------------------------------------  |
|  Fast lookups         |  Storage space (copy of indexed data)    |
|  Fast range scans     |  Write overhead (maintain on INSERT)     |
|  Sort elimination     |  Write overhead (maintain on UPDATE)     |
|  Index-only scans     |  Write overhead (maintain on DELETE)     |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  Rule of Thumb:                                                  |
|  - Indexes help reads, hurt writes                               |
|  - Don't over-index                                              |
|  - Index columns used in WHERE, JOIN, ORDER BY                   |
|  - Consider partial indexes for selective queries                |
|                                                                  |
+------------------------------------------------------------------+

索引的权衡：

优点：
- 快速查找和范围扫描
- 可以消除排序
- 支持仅索引扫描

代价：
- 存储空间（索引是数据副本）
- INSERT开销（需要更新索引）
- UPDATE开销（如果更新索引列）
- DELETE开销（需要标记索引条目）

经验法则：
- 索引帮助读取，损害写入
- 不要过度索引
- 索引WHERE、JOIN、ORDER BY中使用的列
- 考虑部分索引用于选择性查询
```

---

## 7.2 B-tree Index (Default)

B-tree is the default and most commonly used index type in PostgreSQL.

### B-tree Structure

```
                        B-tree Structure (order 3)
+------------------------------------------------------------------+
|                                                                  |
|                           Root                                   |
|                        +--------+                                |
|                        | 50 |100|                                |
|                        +--------+                                |
|                       /    |    \                                |
|                      /     |     \                               |
|                     v      v      v                              |
|              +--------+ +--------+ +--------+                    |
|    Internal  | 20 |40 | | 70 |90 | |120|150|                     |
|    Nodes     +--------+ +--------+ +--------+                    |
|              /  |  \    /  |  \    /  |  \                       |
|             v   v   v  v   v   v  v   v   v                      |
|           +--+ +--+ +--+ +--+ +--+ +--+ +--+ +--+ +--+           |
|    Leaf   |10| |25| |45| |55| |75| |95| |105| |130| |160|        |
|    Nodes  |15| |30| |48| |60| |80| |98| |110| |140| |170|        |
|           |18| |35| |  | |65| |85| |  | |115| |145| |   |        |
|           +--+ +--+ +--+ +--+ +--+ +--+ +---+ +---+ +---+        |
|                                                                  |
|    Each leaf entry: (key, TID)                                   |
|    Leaves linked: leaf1 <-> leaf2 <-> leaf3 ...                  |
+------------------------------------------------------------------+

B-tree结构：

特点：
- 所有叶子节点在同一层（平衡树）
- 内部节点只存储分隔键
- 叶子节点存储实际的(键, TID)对
- 叶子节点之间双向链接（支持范围扫描）

搜索过程（查找65）：
1. 从根开始，65 >= 50，走右分支
2. 65 < 100，走中间分支
3. 在70-90节点，65 < 70，走左分支
4. 在叶子节点找到65

复杂度：O(log n)，树高通常3-4层

PostgreSQL的B-tree实现基于Lehman-Yao算法，
支持高并发读写。
```

Source: `src/backend/access/nbtree/`

### B-tree Operations

```
Search (key = 65):
+------------------------------------------------------------------+
| 1. Start at root                                                 |
| 2. Binary search within node to find child pointer               |
| 3. Follow pointer to child                                       |
| 4. Repeat until leaf node                                        |
| 5. Binary search in leaf to find key                             |
| 6. Return TID(s) for matching keys                               |
+------------------------------------------------------------------+

Range Scan (key BETWEEN 30 AND 80):
+------------------------------------------------------------------+
| 1. Search for lower bound (30)                                   |
| 2. Scan right through leaf nodes                                 |
| 3. Follow leaf links: leaf1 -> leaf2 -> leaf3                    |
| 4. Stop when key > upper bound (80)                              |
+------------------------------------------------------------------+

Insert (key = 42):
+------------------------------------------------------------------+
| 1. Search to find correct leaf                                   |
| 2. If leaf has space: insert key                                 |
| 3. If leaf is full: split leaf                                   |
|    - Create new leaf with half the keys                          |
|    - Insert separator in parent                                  |
|    - May cascade splits up the tree                              |
+------------------------------------------------------------------+

B-tree操作：

搜索：
- 从根到叶的路径遍历
- 每层二分查找确定子节点
- O(log n)

范围扫描：
- 先搜索到下界
- 沿叶子节点链表扫描
- 直到超过上界

插入：
- 搜索到目标叶子
- 有空间则直接插入
- 无空间则分裂：创建新叶子，将一半键移过去，更新父节点

删除：
- PostgreSQL的B-tree使用延迟删除
- 删除只标记条目
- VACUUM清理标记的条目
```

### When to Use B-tree

```sql
-- Good use cases for B-tree:
-- 1. Equality queries
SELECT * FROM users WHERE id = 1000;

-- 2. Range queries
SELECT * FROM orders WHERE created_at BETWEEN '2024-01-01' AND '2024-12-31';

-- 3. Sorting
SELECT * FROM products ORDER BY price;

-- 4. Prefix matching (LIKE 'foo%')
SELECT * FROM users WHERE name LIKE 'Alice%';

-- Bad use cases for B-tree:
-- 1. Suffix matching (LIKE '%foo')
SELECT * FROM users WHERE name LIKE '%smith';  -- Cannot use B-tree

-- 2. Low selectivity columns
SELECT * FROM users WHERE gender = 'M';  -- If 50% are 'M', full scan may be faster
```

---

## 7.3 Hash Index

```
Hash Index Structure:
+------------------------------------------------------------------+
|                                                                  |
|  Key: email = 'alice@example.com'                                |
|                                                                  |
|  hash('alice@example.com') = 0x7A3B --> Bucket 0x7A3B            |
|                                                                  |
|  Bucket Array:                                                   |
|  +------+------+------+------+------+------+------+------+       |
|  |      |      |      | ...  | 7A3B | ...  |      |      |       |
|  +------+------+------+------+------+------+------+------+       |
|                                |                                 |
|                                v                                 |
|                         +----------------+                       |
|                         | alice@...      |                       |
|                         | TID(5,3)       |                       |
|                         +----------------+                       |
|                                |                                 |
|                                v                                 |
|                         +----------------+                       |
|                         | (collision)    |                       |
|                         | another@...    |                       |
|                         | TID(8,1)       |                       |
|                         +----------------+                       |
+------------------------------------------------------------------+

哈希索引结构：

原理：
- 对键值进行哈希得到桶号
- 在对应桶中查找条目
- 处理哈希冲突（链表或溢出页）

优点：
- 等值查询O(1)平均复杂度
- 比B-tree更快的等值查找（理论上）

缺点：
- 不支持范围查询
- 不支持排序
- 历史上有WAL支持问题（已在PG10修复）

使用场景：
- 只需要等值查询
- 键值分布均匀
- 实践中B-tree通常足够好
```

```sql
-- Create hash index
CREATE INDEX users_email_hash ON users USING hash (email);

-- Only supports equality:
SELECT * FROM users WHERE email = 'alice@example.com';  -- Uses hash index

-- Does NOT support:
SELECT * FROM users WHERE email LIKE 'alice%';  -- Cannot use hash index
SELECT * FROM users ORDER BY email;  -- Cannot use hash index
```

---

## 7.4 GiST (Generalized Search Tree)

```
GiST: A framework for building custom indexes

+------------------------------------------------------------------+
|  GiST can index:                                                 |
|  - Geometric shapes (PostGIS)                                    |
|  - Full-text search documents                                    |
|  - Ranges (int4range, tsrange)                                   |
|  - Any data type with definable "containment"                    |
+------------------------------------------------------------------+

Example: R-tree for 2D points (GiST implementation)

                    Bounding Box Tree
                    
                    +------------------+
                    | World            |
                    | (0,0)-(100,100)  |
                    +------------------+
                   /                    \
                  /                      \
    +------------------+        +------------------+
    | West Region      |        | East Region      |
    | (0,0)-(50,100)   |        | (50,0)-(100,100) |
    +------------------+        +------------------+
         /       \                   /       \
        /         \                 /         \
    +-------+  +-------+       +-------+  +-------+
    |SW     |  |NW     |       |SE     |  |NE     |
    |(0,0)- |  |(0,50)-|       |(50,0)-|  |(50,50)|
    |(50,50)|  |(50,100|       |(100,50)| |(100,100)
    +-------+  +-------+       +-------+  +-------+
        |                           |
        v                           v
    Points in                  Points in
    this region                this region

GiST（通用搜索树）：

GiST是一个索引框架，可用于构建自定义索引。

特点：
- 支持任意数据类型
- 用户定义的一致性谓词
- 支持范围查询、最近邻查询

典型应用：
- 几何数据（R-tree for PostGIS）
- 全文搜索
- 范围类型（时间范围、数值范围）

示例：空间索引
- 每个节点是一个边界框
- 搜索时排除不相交的子树
- 支持"包含"、"相交"、"最近邻"查询
```

```sql
-- GiST index for geometric data (PostGIS)
CREATE INDEX locations_geom_gist ON locations USING gist (geom);

-- Find all points within a box
SELECT * FROM locations
WHERE geom && ST_MakeBox2D(ST_Point(0,0), ST_Point(10,10));

-- GiST index for ranges
CREATE INDEX reservations_during_gist ON reservations USING gist (during);

-- Find overlapping reservations
SELECT * FROM reservations
WHERE during && '[2024-01-01, 2024-01-31]'::daterange;
```

Source: `src/backend/access/gist/`

---

## 7.5 GIN (Generalized Inverted Index)

```
GIN: Inverted index for multi-valued columns

+------------------------------------------------------------------+
|  Problem: Indexing arrays or documents                           |
|                                                                  |
|  Row 1: tags = ['postgres', 'database', 'sql']                   |
|  Row 2: tags = ['python', 'postgres', 'orm']                     |
|  Row 3: tags = ['database', 'nosql', 'mongodb']                  |
|                                                                  |
|  How to efficiently find rows containing 'postgres'?             |
+------------------------------------------------------------------+

GIN Structure:
+------------------------------------------------------------------+
|                                                                  |
|  Key (token)      |  Posting List (TIDs)                         |
|  -----------------|------------------------------------------    |
|  'database'       |  [Row1, Row3]                                |
|  'mongodb'        |  [Row3]                                      |
|  'nosql'          |  [Row3]                                      |
|  'orm'            |  [Row2]                                      |
|  'postgres'       |  [Row1, Row2]    <-- Query: tags @> 'postgres'
|  'python'         |  [Row2]                                      |
|  'sql'            |  [Row1]                                      |
|                                                                  |
|  Keys stored in B-tree                                           |
|  Posting lists stored separately                                 |
+------------------------------------------------------------------+

GIN（通用倒排索引）：

适用场景：
- 数组列（包含某元素的数组）
- 全文搜索（包含某词的文档）
- JSONB（包含某键/值的JSON）

结构：
- 键（token）存储在B-tree中
- 每个键关联一个posting list（包含该键的行的TID列表）
- posting list可能很长，使用压缩

查询"postgres"：
1. 在键的B-tree中找到'postgres'
2. 获取posting list: [Row1, Row2]
3. 返回这些行

优点：
- 极快的"包含"查询
- 支持复杂的布尔组合

缺点：
- 更新开销大（每个元素一个条目）
- 占用更多空间
```

```sql
-- GIN index for array columns
CREATE INDEX articles_tags_gin ON articles USING gin (tags);

-- Find articles with specific tag
SELECT * FROM articles WHERE tags @> ARRAY['postgres'];

-- GIN index for full-text search
CREATE INDEX articles_fts_gin ON articles USING gin (to_tsvector('english', body));

-- Full-text search
SELECT * FROM articles
WHERE to_tsvector('english', body) @@ to_tsquery('postgres & performance');

-- GIN index for JSONB
CREATE INDEX data_gin ON documents USING gin (data);

-- Find documents with specific key/value
SELECT * FROM documents WHERE data @> '{"status": "active"}';
```

Source: `src/backend/access/gin/`

---

## 7.6 BRIN (Block Range Index)

```
BRIN: Compact index for naturally ordered data

+------------------------------------------------------------------+
|  Problem: Huge tables where data is naturally ordered            |
|                                                                  |
|  Example: Time-series data                                       |
|  - Events inserted in time order                                 |
|  - Table has 1 billion rows                                      |
|  - B-tree index would be huge                                    |
+------------------------------------------------------------------+

BRIN Structure:
+------------------------------------------------------------------+
|                                                                  |
|  Block Range    |  Min Timestamp    |  Max Timestamp             |
|  ---------------|-------------------|---------------------------  |
|  Pages 0-127    |  2024-01-01 00:00 |  2024-01-01 05:00          |
|  Pages 128-255  |  2024-01-01 05:01 |  2024-01-01 10:00          |
|  Pages 256-383  |  2024-01-01 10:01 |  2024-01-01 15:00          |
|  Pages 384-511  |  2024-01-01 15:01 |  2024-01-01 20:00          |
|  ...                                                             |
|                                                                  |
|  Query: WHERE timestamp BETWEEN '2024-01-01 06:00'               |
|                           AND '2024-01-01 08:00'                 |
|                                                                  |
|  BRIN says: Only need to scan pages 128-255                      |
|  Skip: Pages 0-127, 256+                                         |
+------------------------------------------------------------------+

BRIN（块范围索引）：

适用场景：
- 数据按某列自然排序（如时间序列）
- 表非常大
- 可以接受一些误判（需要扫描整个块范围）

结构：
- 将表分成块范围（默认128页一个范围）
- 每个范围存储汇总信息（最小值、最大值）
- 索引非常小

查询过程：
1. 查看哪些块范围可能包含目标值
2. 只扫描这些块范围
3. 跳过不可能包含目标的块范围

优点：
- 极小的索引大小（可能只有B-tree的0.1%）
- 适合超大表

缺点：
- 只有数据物理排序时才有效
- 不精确（需要扫描整个块范围）
- 不支持精确查找
```

```sql
-- BRIN index for time-series data
CREATE INDEX events_created_brin ON events USING brin (created_at);

-- Query benefits from BRIN
SELECT * FROM events
WHERE created_at BETWEEN '2024-01-01' AND '2024-01-02';

-- Check index size comparison
SELECT pg_size_pretty(pg_relation_size('events_created_brin')) AS brin_size;
-- Might be just a few MB for a 100GB table!
```

Source: `src/backend/access/brin/`

---

## 7.7 Choosing the Right Index

```
+------------------------------------------------------------------+
|                    Index Selection Guide                         |
+------------------------------------------------------------------+
|                                                                  |
|  Query Pattern              |  Recommended Index                 |
|  ---------------------------|------------------------------------
|  Equality (=)               |  B-tree (default) or Hash          |
|  Range (<, >, BETWEEN)      |  B-tree                            |
|  Sorting (ORDER BY)         |  B-tree                            |
|  Pattern (LIKE 'foo%')      |  B-tree (with text_pattern_ops)    |
|  Pattern (LIKE '%foo%')     |  GIN (pg_trgm extension)           |
|  Full-text search           |  GIN                               |
|  Array containment          |  GIN                               |
|  JSONB queries              |  GIN                               |
|  Geometric (near, within)   |  GiST (PostGIS)                    |
|  Range overlap              |  GiST                              |
|  Time-series (ordered)      |  BRIN                              |
|  Very large tables          |  BRIN (if data is ordered)         |
+------------------------------------------------------------------+

索引选择指南：

等值查询(=)            -> B-tree或Hash
范围查询(<, >, BETWEEN) -> B-tree
排序(ORDER BY)         -> B-tree
前缀匹配(LIKE 'foo%')  -> B-tree
中缀匹配(LIKE '%foo%') -> GIN + pg_trgm
全文搜索               -> GIN
数组包含               -> GIN
JSONB查询              -> GIN
几何查询               -> GiST
范围重叠               -> GiST
时间序列               -> BRIN（如果数据有序）
超大表                 -> BRIN（如果数据有序）

记住：
- B-tree是默认选择，覆盖大多数场景
- GIN用于"包含"查询（数组、全文、JSON）
- GiST用于几何和范围数据
- BRIN用于超大有序数据
- Hash很少使用（B-tree通常足够）
```

---

## Summary

```
+------------------------------------------------------------------+
|                    Indexing Summary                              |
+------------------------------------------------------------------+
|                                                                  |
|  Index Type | Structure        | Best For                       |
|  -----------|------------------|-------------------------------- |
|  B-tree     | Balanced tree    | Equality, range, sorting       |
|  Hash       | Hash table       | Equality only                  |
|  GiST       | Search tree      | Geometric, range types         |
|  GIN        | Inverted index   | Arrays, full-text, JSONB       |
|  BRIN       | Block summaries  | Large ordered tables           |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  Key Principles:                                                 |
|  1. Indexes trade write speed for read speed                     |
|  2. Choose index type based on query pattern                     |
|  3. B-tree covers 90% of use cases                               |
|  4. Use EXPLAIN to verify index usage                            |
|  5. Partial indexes for selective queries                        |
|  6. Don't over-index                                             |
|                                                                  |
+------------------------------------------------------------------+

索引总结：

各类型特点：
- B-tree: 平衡树，等值/范围/排序，最通用
- Hash: 哈希表，仅等值，理论上更快
- GiST: 通用搜索树，几何/范围数据
- GIN: 倒排索引，数组/全文/JSONB
- BRIN: 块范围汇总，大型有序表

关键原则：
1. 索引用读取速度换写入速度
2. 根据查询模式选择索引类型
3. B-tree覆盖90%的场景
4. 用EXPLAIN验证索引使用
5. 选择性查询考虑部分索引
6. 不要过度索引

源代码位置：
- src/backend/access/nbtree/ - B-tree
- src/backend/access/hash/ - Hash
- src/backend/access/gist/ - GiST
- src/backend/access/gin/ - GIN
- src/backend/access/brin/ - BRIN

下一节我们将学习事务和MVCC，这是PostgreSQL并发控制的核心。
```
