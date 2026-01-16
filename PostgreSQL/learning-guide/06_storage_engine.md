# Section 6: Storage Engine Fundamentals

Understanding how PostgreSQL stores data on disk is fundamental to understanding
performance, maintenance operations, and debugging.

---

## 6.1 Tables as Heap Files

### What is a Heap?

In PostgreSQL, a "heap" is not the heap data structure (priority queue).
It means **unordered storage** - rows are stored wherever there is space.

```
Heap vs Clustered Index (comparison with other databases):

+------------------------------------------------------------------+
|                    PostgreSQL (Heap)                             |
+------------------------------------------------------------------+
|  Table: users                                                    |
|  +---------------------------+                                   |
|  | Page 0 | Page 1 | Page 2 |   <-- Data stored in any order    |
|  +---------------------------+                                   |
|                                                                  |
|  Row insertion: Find page with free space, insert there          |
|  Row retrieval by PK: Use index to find (page, offset)           |
+------------------------------------------------------------------+

+------------------------------------------------------------------+
|                  MySQL InnoDB (Clustered)                        |
+------------------------------------------------------------------+
|  Table: users                                                    |
|  +---------------------------+                                   |
|  | id=1-10 | id=11-20 | ... |   <-- Data stored in PK order     |
|  +---------------------------+                                   |
|                                                                  |
|  Row insertion: Insert in sorted position (may cause splits)     |
|  Row retrieval by PK: Binary search on pages                     |
+------------------------------------------------------------------+

堆存储 vs 聚簇索引：

PostgreSQL使用堆存储：
- 数据按插入顺序存储，无特定排序
- 插入快（找到有空间的页即可）
- 主键查询需要索引间接访问

MySQL InnoDB使用聚簇索引：
- 数据按主键顺序存储
- 插入可能导致页分裂
- 主键查询直接定位数据

PostgreSQL选择堆存储的原因：
1. 插入性能更稳定
2. 无需维护物理排序
3. MVCC实现更简单
```

### Physical File Layout

```
Data Directory ($PGDATA):
+------------------------------------------------------------------+
|  base/                                                           |
|    +-- 1/           (template1 database)                         |
|    +-- 12345/       (user database, OID = 12345)                 |
|          +-- 16384         (table file, relfilenode = 16384)     |
|          +-- 16384.1       (extension if > 1GB)                  |
|          +-- 16384_fsm     (free space map)                      |
|          +-- 16384_vm      (visibility map)                      |
|          +-- 16385         (index file)                          |
|                                                                  |
|  pg_wal/                   (Write-Ahead Log)                     |
|    +-- 000000010000000000000001                                  |
|    +-- 000000010000000000000002                                  |
|                                                                  |
|  pg_xact/                  (Transaction status, formerly pg_clog)|
|  pg_multixact/             (Multi-transaction status)            |
+------------------------------------------------------------------+

物理文件布局：

base/目录：
- 每个数据库一个子目录（以OID命名）
- 每个表/索引一个文件（以relfilenode命名）
- 文件超过1GB时自动分段（.1, .2, ...）

辅助文件：
- _fsm: 空闲空间映射（记录每页空闲空间）
- _vm: 可见性映射（记录哪些页全可见）

pg_wal/目录：
- WAL段文件（预写日志）
- 默认16MB一个段

pg_xact/目录：
- 事务提交状态
```

### Finding a Table's Files

```sql
-- Find the physical file for a table
SELECT pg_relation_filepath('users');
-- Returns: base/12345/16384

-- Get more details
SELECT relname, relfilenode, relpages, reltuples
FROM pg_class
WHERE relname = 'users';

-- Examine actual file sizes
SELECT pg_size_pretty(pg_relation_size('users')) AS table_size,
       pg_size_pretty(pg_indexes_size('users')) AS indexes_size,
       pg_size_pretty(pg_total_relation_size('users')) AS total_size;

查找表的物理文件：

pg_relation_filepath('users') 返回相对路径
pg_relation_size('users') 返回表大小（不含索引）
pg_total_relation_size('users') 返回总大小（含索引）

这些函数对于理解存储使用很重要。
```

---

## 6.2 Page Layout (Critical)

PostgreSQL stores data in fixed-size pages (default 8KB).

### Page Structure

```
+------------------------------------------------------------------+
|                         Page (8KB default)                       |
+------------------------------------------------------------------+
| PageHeaderData (24 bytes)                                        |
|   pd_lsn:      LSN of last WAL record affecting this page        |
|   pd_checksum: Page checksum (if enabled)                        |
|   pd_flags:    Flag bits                                         |
|   pd_lower:    Offset to start of free space                     |
|   pd_upper:    Offset to end of free space                       |
|   pd_special:  Offset to special space (for indexes)             |
|   pd_pagesize_version: Page size and layout version              |
|   pd_prune_xid: Oldest XID for pruning                           |
+------------------------------------------------------------------+
| Line Pointers (ItemIdData array)                                 |
|   [lp1] [lp2] [lp3] [lp4] ...                                    |
|   Each: 4 bytes (offset, length, flags)                          |
|                                        |                         |
|                                        v pd_lower                |
|   ~~~~~~~~~~~~~ FREE SPACE ~~~~~~~~~~~~                          |
|                                        ^ pd_upper                |
|                                        |                         |
+------------------------------------------------------------------+
| Tuple Data (grows downward from end)                             |
|   [tuple4] [tuple3] [tuple2] [tuple1]                            |
+------------------------------------------------------------------+
| Special Space (index-specific, at end of page)                   |
|   (heap pages have no special space)                             |
+------------------------------------------------------------------+

页面结构详解（8KB默认）：

页头（PageHeaderData, 24字节）：
- pd_lsn: 最后修改该页的WAL记录位置
  用于恢复时判断是否需要重放
- pd_checksum: 页面校验和（可选）
  启用后可检测存储损坏
- pd_lower: 空闲空间起始位置
  行指针数组向下增长到这里
- pd_upper: 空闲空间结束位置
  元组数据向上增长到这里
- pd_special: 特殊空间起始位置
  索引页用于存储索引特定数据

行指针数组（Line Pointers）：
- 每个指针4字节
- 包含：偏移量(15位)、长度(15位)、标志(2位)
- 从页头之后开始，向下增长

元组数据（Tuple Data）：
- 从页尾向上增长
- 通过行指针间接访问
- 这种设计允许元组在页内移动而不影响外部引用

当pd_lower和pd_upper相遇时，页面已满。
```

Source: `src/include/storage/bufpage.h`

### Why Line Pointers (Indirection)?

```
Direct Storage (bad):
+------------------------------------------------------------------+
| External Reference: Page 5, Byte 100                             |
|                                                                  |
| If tuple moves (compaction), reference becomes invalid!          |
+------------------------------------------------------------------+

Indirect Storage (PostgreSQL):
+------------------------------------------------------------------+
| External Reference: Page 5, Line Pointer 3                       |
|                                                                  |
| Page 5:                                                          |
| +------+------+------+------+                                    |
| | LP1  | LP2  | LP3  | LP4  |  Line Pointer 3 -> offset 7500     |
| +------+------+------+------+                                    |
|                       |                                          |
| ... free space ...    |                                          |
|                       v                                          |
| +--------------+-------------+                                   |
| | Tuple at 7500| Other tuples|                                   |
| +--------------+-------------+                                   |
|                                                                  |
| If tuple moves to offset 7200:                                   |
| Just update LP3 -> 7200                                          |
| External reference remains valid!                                |
+------------------------------------------------------------------+

为什么使用行指针（间接寻址）：

直接存储的问题：
- 外部引用直接指向字节偏移
- 如果元组因压缩而移动，引用失效
- 需要更新所有引用该元组的索引

间接存储的优点：
- 外部引用指向行指针编号
- 元组移动只需更新行指针
- 索引不需要修改
- 支持页面内部压缩（碎片整理）

这就是为什么PostgreSQL的TID是(page, line_pointer)
而不是(page, byte_offset)。
```

### Tuple Structure

```
HeapTupleHeaderData (23 bytes minimum):
+------------------------------------------------------------------+
| t_xmin (4 bytes)      | Transaction ID that inserted this tuple  |
| t_xmax (4 bytes)      | Transaction ID that deleted/locked tuple |
| t_cid (4 bytes)       | Command ID within transaction            |
| t_ctid (6 bytes)      | Current TID (or pointer to newer version)|
| t_infomask2 (2 bytes) | Number of attributes + flags             |
| t_infomask (2 bytes)  | Various flag bits                        |
| t_hoff (1 byte)       | Offset to user data                      |
+------------------------------------------------------------------+
| Null Bitmap (variable)| 1 bit per column if HEAP_HASNULL         |
+------------------------------------------------------------------+
| User Data             | Actual column values                     |
+------------------------------------------------------------------+

元组结构详解（HeapTupleHeaderData）：

MVCC相关字段：
- t_xmin: 插入该元组的事务ID
  事务提交后，该元组对后续事务可见
- t_xmax: 删除/锁定该元组的事务ID
  如果为0或无效事务，元组仍然有效
- t_cid: 事务内的命令ID
  用于判断同一事务内的可见性
- t_ctid: 当前元组ID或新版本位置
  UPDATE时指向新版本，形成版本链

标志字段：
- t_infomask2: 属性数量 + 各种标志
- t_infomask: 状态标志
  HEAP_XMIN_COMMITTED: 插入事务已提交
  HEAP_XMAX_INVALID: 删除事务无效（未删除）

数据字段：
- t_hoff: 用户数据开始的偏移量
- Null Bitmap: NULL值位图
- User Data: 实际列值（按列顺序存储）

元组头部最小23字节，这是每行的固定开销。
小表（如只有一个int列）的存储效率较低。
```

Source: `src/include/access/htup_details.h`

---

## 6.3 Free Space Management

### Free Space Map (FSM)

```
When inserting a row, PostgreSQL must find a page with enough space.
Scanning all pages would be O(n) - too slow.

Solution: Free Space Map (_fsm file)

FSM Structure (simplified):
+------------------------------------------------------------------+
|                    FSM Tree Structure                            |
|                                                                  |
|              [max_free_in_tree]                                  |
|                    / \                                           |
|                   /   \                                          |
|      [max_left_subtree] [max_right_subtree]                      |
|            / \                / \                                |
|           /   \              /   \                               |
|        [pg0] [pg1]        [pg2] [pg3]                            |
|                                                                  |
| Leaf nodes: Free space category (0-255) for each heap page       |
| Internal nodes: Max of children (for quick search)               |
+------------------------------------------------------------------+

空闲空间映射（FSM）：

问题：插入行时如何快速找到有足够空间的页？
天真方法：扫描所有页 -> O(n)太慢

解决方案：FSM树结构
- 叶子节点：每个堆页的空闲空间类别（0-255）
- 内部节点：子树的最大值
- 查找O(log n)

空闲空间类别：
- 0 = 页满
- 255 = 页空
- 每个类别代表约32字节（8KB/256）

当需要N字节空间时：
1. 从根开始
2. 如果左子树的最大值 >= N，进入左子树
3. 否则进入右子树
4. 找到第一个满足条件的叶子节点
```

### Visibility Map (VM)

```
Purpose: Track which pages contain only "all-visible" tuples

+------------------------------------------------------------------+
|  Visibility Map: 2 bits per heap page                            |
|                                                                  |
|  Page 0: [1,0]  - All visible, not all frozen                    |
|  Page 1: [1,1]  - All visible AND all frozen                     |
|  Page 2: [0,0]  - Has invisible tuples                           |
|  Page 3: [1,0]  - All visible, not all frozen                    |
+------------------------------------------------------------------+

Uses:
1. Index-Only Scans: If page is all-visible, skip heap fetch
2. VACUUM: Only process pages that are not all-visible
3. Freezing: Track which pages need freezing

可见性映射（VM）：

每个堆页2位：
- 位1：全可见标志
  如果设置，页内所有元组对所有事务可见
- 位2：全冻结标志
  如果设置，页内所有元组已冻结（无需再冻结）

用途：
1. 仅索引扫描：如果页全可见，无需访问堆
   显著提升某些查询性能
2. VACUUM：只处理非全可见的页
   大大加速VACUUM
3. 冻结：跟踪哪些页需要冻结
   防止事务ID回卷
```

Source: `src/backend/storage/freespace/` and `src/backend/access/heap/visibilitymap.c`

---

## 6.4 TOAST (The Oversized Attribute Storage Technique)

Large values cannot fit in a single page. PostgreSQL handles this with TOAST.

```
TOAST Strategies:
+------------------------------------------------------------------+
|                                                                  |
| Strategy    | Description                                        |
| ------------|--------------------------------------------------- |
| PLAIN       | No TOAST, must fit in page                         |
| EXTENDED    | Compress first, then store out-of-line if needed  |
| EXTERNAL    | Store out-of-line without compression              |
| MAIN        | Compress first, try to keep in-line               |
+------------------------------------------------------------------+

When does TOAST kick in?
- Tuple size > ~2KB (TOAST_TUPLE_THRESHOLD)
- PostgreSQL tries to get tuple below ~2KB

TOAST Storage:
+------------------------------------------------------------------+
|  Main Table                          TOAST Table                 |
|  +------------------------+          +------------------------+  |
|  | id | name | big_text   |          | chunk_id | chunk_seq  |  |
|  |----|------|------------|          | chunk_data             |  |
|  | 1  | foo  | [TOAST ptr]|--------->| 12345, 0, <2KB chunk>  |  |
|  |    |      |            |          | 12345, 1, <2KB chunk>  |  |
|  |    |      |            |          | 12345, 2, <2KB chunk>  |  |
|  +------------------------+          +------------------------+  |
+------------------------------------------------------------------+

TOAST（超大属性存储技术）：

问题：单个页面8KB，如何存储大于8KB的值？

TOAST策略：
- PLAIN: 不使用TOAST，必须能放入页面
- EXTENDED: 先压缩，必要时外部存储（默认）
- EXTERNAL: 不压缩，直接外部存储
- MAIN: 先压缩，尽量保持内联

TOAST触发条件：
- 元组大小超过约2KB
- PostgreSQL尝试将元组压缩到约2KB以下

存储方式：
- 主表中存储TOAST指针
- 大值切分为约2KB的块
- 块存储在关联的TOAST表中
- 读取时自动重组

每个有TOAST列的表都有一个隐藏的TOAST表。
```

```sql
-- See TOAST table for a table
SELECT reltoastrelid::regclass
FROM pg_class
WHERE relname = 'your_table';

-- TOAST storage statistics
SELECT
    schemaname,
    relname,
    pg_size_pretty(pg_relation_size(relid)) AS table_size,
    pg_size_pretty(pg_relation_size(reltoastrelid)) AS toast_size
FROM pg_stat_user_tables
WHERE reltoastrelid != 0;
```

---

## 6.5 Reading and Writing Data

### Read Path

```
SELECT * FROM users WHERE id = 1;

+------------------------------------------------------------------+
|  1. Executor calls heap_fetch() or heapgettup()                  |
|                                                                  |
|  2. Buffer Manager:                                              |
|     a. Hash TID to find buffer                                   |
|     b. If in buffer pool: pin and return                         |
|     c. If not: evict old page, read from disk                    |
|                                                                  |
|  3. Storage Manager (smgr):                                      |
|     a. Map relation + block number to file + offset              |
|     b. Call mdread() to read from file                           |
|                                                                  |
|  4. File System:                                                 |
|     a. OS read() system call                                     |
|     b. May hit OS page cache or actual disk                      |
+------------------------------------------------------------------+

读取路径：

1. 执行器调用heap_fetch()
   - 提供TID (page, offset)

2. 缓冲管理器：
   - 检查页面是否在缓冲池中
   - 如果在：增加pin计数，返回
   - 如果不在：选择一个缓冲区，驱逐旧页

3. 存储管理器：
   - 将(关系, 块号)映射到(文件, 偏移)
   - 调用mdread()读取

4. 文件系统：
   - OS的read()系统调用
   - 可能命中OS页缓存

关键点：
- 缓冲池是PostgreSQL和磁盘之间的缓存层
- OS也有页缓存（双重缓存）
- 配置shared_buffers时要考虑这一点
```

### Write Path

```
INSERT INTO users VALUES (1, 'alice');

+------------------------------------------------------------------+
|  1. Find page with free space (via FSM)                          |
|                                                                  |
|  2. Buffer Manager:                                              |
|     a. Read page into buffer (if not already)                    |
|     b. Pin buffer                                                |
|     c. Lock buffer (exclusive)                                   |
|                                                                  |
|  3. Write WAL record FIRST (Write-Ahead Logging)                 |
|     a. Construct WAL record                                      |
|     b. Insert into WAL buffer                                    |
|     c. May trigger WAL flush if synchronous_commit               |
|                                                                  |
|  4. Modify page in buffer                                        |
|     a. Add line pointer                                          |
|     b. Copy tuple data                                           |
|     c. Update pd_lower, pd_upper                                 |
|     d. Update pd_lsn to WAL position                             |
|     e. Mark buffer dirty                                         |
|                                                                  |
|  5. Release buffer lock and pin                                  |
|                                                                  |
|  6. Dirty page written to disk later by:                         |
|     - Background writer                                          |
|     - Checkpointer                                               |
|     - Backend (if buffer needed)                                 |
+------------------------------------------------------------------+

写入路径：

1. 通过FSM找到有空闲空间的页

2. 缓冲管理器：
   - 读取页面到缓冲区
   - 固定缓冲区（pin）
   - 排他锁定缓冲区

3. 先写WAL记录：
   - 构造WAL记录
   - 插入WAL缓冲区
   - 同步提交时可能触发WAL刷盘

4. 修改缓冲区中的页面：
   - 添加行指针
   - 复制元组数据
   - 更新pd_lower, pd_upper
   - 更新pd_lsn到WAL位置
   - 标记缓冲区为脏

5. 释放缓冲区锁和pin

6. 脏页稍后由以下进程写入磁盘：
   - 后台写入进程
   - 检查点进程
   - 后端进程（需要缓冲区时）

关键点：
- WAL先写，数据页后写
- 修改在内存中进行
- 实际磁盘写入是延迟的
```

---

## Summary

```
+------------------------------------------------------------------+
|              Storage Engine Summary                              |
+------------------------------------------------------------------+
|                                                                  |
|  Heap Files:                                                     |
|  - Tables stored as unordered heap files                         |
|  - Rows inserted wherever space exists                           |
|  - Files in base/<dboid>/<relfilenode>                           |
|                                                                  |
|  Page Layout (8KB):                                              |
|  - Header (24 bytes) + Line Pointers + Free Space + Tuples       |
|  - Line pointers provide indirection for tuple movement          |
|  - pd_lsn tracks WAL position for recovery                       |
|                                                                  |
|  Tuple Layout:                                                   |
|  - Header (23+ bytes) with xmin, xmax, ctid                      |
|  - MVCC information embedded in every tuple                      |
|                                                                  |
|  Support Structures:                                             |
|  - FSM: Quick find of pages with free space                      |
|  - VM: Track all-visible pages for optimization                  |
|  - TOAST: Handle large values (> ~2KB)                           |
|                                                                  |
+------------------------------------------------------------------+

存储引擎总结：

堆文件：
- 表存储为无序堆文件
- 行插入到有空间的任意位置
- 文件位置：base/<数据库OID>/<relfilenode>

页面布局（8KB）：
- 头部(24字节) + 行指针 + 空闲空间 + 元组
- 行指针提供间接寻址，支持元组移动
- pd_lsn跟踪WAL位置，用于恢复

元组布局：
- 头部(23+字节)包含xmin, xmax, ctid
- MVCC信息嵌入每个元组

辅助结构：
- FSM：快速找到有空闲空间的页
- VM：跟踪全可见页，优化查询
- TOAST：处理大值（>约2KB）

关键源文件：
- src/include/storage/bufpage.h - 页面结构
- src/include/access/htup_details.h - 元组结构
- src/backend/access/heap/ - 堆访问方法
- src/backend/storage/buffer/ - 缓冲管理

下一节我们将学习索引如何加速数据访问。
```
