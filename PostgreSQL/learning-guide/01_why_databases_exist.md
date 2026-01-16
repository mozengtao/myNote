# Section 1: Why Databases Exist (First Principles)

Before understanding PostgreSQL, you must understand why databases exist at all.
This is not a rhetorical question. The answer reveals the fundamental constraints
that shape every database design decision.

---

## 1.1 Life Without a Database

Imagine building a banking application. You need to store account balances.
Without a database, what are your options?

### Option 1: Flat Files

```
accounts.txt:
user1,1000.00
user2,2500.00
user3,150.00
```

```
+------------------+
|  Application     |
+--------+---------+
         |
         v
+------------------+
|  accounts.txt    |
|  (plain text)    |
+------------------+
         |
         v
+------------------+
|      Disk        |
+------------------+
```

平面文件方案：应用程序直接将数据写入文本文件，然后存储到磁盘。
这是最简单的数据存储方式，但存在严重的并发和数据完整性问题。

**Problems:**

1. **No atomicity**: Transfer $100 from user1 to user2.
   ```
   read user1 balance -> 1000
   write user1 balance -> 900
   <-- CRASH HERE -->
   write user2 balance -> 2600   (never happens)
   ```
   User1 lost $100. The system is now inconsistent.

2. **No concurrency**: Two processes read user1's balance simultaneously.
   Both see 1000. Both subtract 100. Both write 900. User1 should have 800.
   You just created money.

3. **No query capability**: Find all users with balance > 1000.
   You must scan the entire file every time.

4. **No schema enforcement**: What stops someone from writing:
   ```
   user1,not_a_number
   ```

### Option 2: Custom Binary Formats

```
+------------------+
|  Application     |
+--------+---------+
         |
         v
+------------------+
|  Binary Format   |
|  (custom codec)  |
+------------------+
         |
         v
+------------------+
|      Disk        |
+------------------+
```

自定义二进制格式：应用程序使用自定义的编码/解码逻辑将数据序列化为二进制格式存储。
这种方式可以提高读写效率，但增加了实现复杂度，且仍然无法解决并发和原子性问题。

You design a compact binary format:
```c
struct Account {
    char user_id[32];
    int64_t balance_cents;
};
```

**Problems:**

1. Same atomicity and concurrency issues
2. You must write serialization/deserialization code
3. Schema changes require file format migrations
4. You must implement your own indexing

### Option 3: In-Memory Data Structures

```
+---------------------------+
|       Application         |
|  +---------------------+  |
|  |  HashMap<user,bal>  |  |
|  +---------------------+  |
+---------------------------+
         |
         | (periodic flush)
         v
+------------------+
|      Disk        |
+------------------+
```

内存数据结构方案：数据存储在内存中的哈希表等数据结构中，定期刷写到磁盘。
这种方式读写速度极快，但崩溃时会丢失未持久化的数据，且内存容量有限。

Store everything in a hash map. Fast reads and writes.

**Problems:**

1. **Durability**: Power goes out. All data gone.
2. **Memory limits**: Data grows larger than RAM. Now what?
3. **Startup time**: Reload everything from disk on restart.

### Why These Approaches Fail at Scale

```
                    Flat    Binary   In-Memory
                    Files   Format   Structures
+------------------+-------+--------+-----------+
| Persistence      |  Yes  |  Yes   |    No     |
| Concurrent Write |  No   |  No    |    No*    |
| Atomicity        |  No   |  No    |    No     |
| Query Capability |  Poor |  Poor  |   Good    |
| Memory Efficient |  Yes  |  Yes   |    No     |
+------------------+-------+--------+-----------+
* Without explicit locking

各方案对比表：
- 持久性(Persistence)：数据能否在重启后保留
- 并发写入(Concurrent Write)：能否安全地同时写入
- 原子性(Atomicity)：操作是否可以完整执行或完整回滚
- 查询能力(Query Capability)：复杂查询的支持程度
- 内存效率(Memory Efficient)：是否能处理超过内存容量的数据
```

The fundamental problems are:

1. **Persistence vs Speed**: Disk is slow but durable. Memory is fast but volatile.
2. **Concurrency**: Multiple writers must coordinate without corrupting data.
3. **Atomicity**: Multi-step operations must either complete entirely or not at all.
4. **Querying**: Ad-hoc data retrieval should not require scanning everything.

---

## 1.2 What a Database Actually Provides

A database is not "a place to store data." That is what a file is.

A database is a system that solves the fundamental problems above.

### A Database is a Durable Data Structure

```
+-------------------+
|    Application    |
+---------+---------+
          |
          v
+-------------------+
|   Database Layer  |
|  +--------------+ |
|  | Write-Ahead  | |
|  |    Log       | |
|  +--------------+ |
|  +--------------+ |
|  | Buffer Pool  | |
|  +--------------+ |
|  +--------------+ |
|  | Data Files   | |
|  +--------------+ |
+-------------------+
          |
          v
+-------------------+
|       Disk        |
+-------------------+
```

数据库作为持久化数据结构：
- 预写日志(WAL)确保在数据实际写入前，操作记录已经持久化
- 缓冲池(Buffer Pool)在内存中缓存热数据，减少磁盘访问
- 数据文件(Data Files)是实际存储数据的地方
这种分层设计同时解决了持久性和性能的矛盾。

A hash map provides O(1) lookups but lives in memory.
A B-tree provides O(log n) lookups and can be disk-resident.
A database provides both, with durability guarantees.

**Key insight**: A database writes changes to a sequential log (fast) before
updating the actual data structures (slow). If the system crashes, it replays
the log to recover.

### A Database is a Concurrency Control System

```
Transaction 1                 Transaction 2
     |                              |
     v                              v
+------------------------------------------+
|            Concurrency Control           |
|  +--------+  +--------+  +--------+      |
|  | Locks  |  |  MVCC  |  | Snapshots|    |
|  +--------+  +--------+  +--------+      |
+------------------------------------------+
                    |
                    v
+------------------------------------------+
|              Shared Data                 |
+------------------------------------------+
```

数据库作为并发控制系统：
- 锁(Locks)：防止多个事务同时修改同一数据
- MVCC(多版本并发控制)：让读操作看到一致的数据快照
- 快照(Snapshots)：记录事务开始时数据的状态
这些机制协同工作，使得多个事务可以安全地并发执行。

Without concurrency control:
```
Time    T1              T2              Balance
----    --------        --------        -------
0       read bal                        1000
1                       read bal        1000
2       bal -= 100                      1000
3                       bal -= 100      1000
4       write bal                       900
5                       write bal       900  <-- WRONG! Should be 800
```

With proper concurrency control:
```
Time    T1              T2              Balance
----    --------        --------        -------
0       read bal                        1000
1       bal -= 100                      1000
2       write bal                       900
3       COMMIT                          900
4                       read bal        900
5                       bal -= 100      900
6                       write bal       800  <-- CORRECT
```

### A Database is a Query Execution Engine

```
      "SELECT * FROM users WHERE age > 30"
                    |
                    v
            +-------+-------+
            |    Parser     |
            +-------+-------+
                    |
                    v
            +-------+-------+
            |   Optimizer   |
            +-------+-------+
                    |
                    v
            +-------+-------+
            |   Executor    |
            +-------+-------+
                    |
                    v
            +-------+-------+
            |  Storage AM   |
            +-------+-------+
                    |
                    v
                 Results
```

数据库作为查询执行引擎：
- 解析器(Parser)：将SQL文本转换为内部表示
- 优化器(Optimizer)：选择最高效的执行计划
- 执行器(Executor)：按计划获取和处理数据
- 存储访问方法(Storage AM)：从物理存储中读取数据
这一流水线将声明式查询转换为高效的数据访问操作。

You say WHAT you want, not HOW to get it. The database:
1. Parses your query into a tree structure
2. Optimizes it (chooses indexes, join order, etc.)
3. Executes it efficiently
4. Returns results

### A Database is a Crash Recovery System

```
Normal Operation:
+--------+     +--------+     +--------+
|  App   | --> |  WAL   | --> |  Data  |
+--------+     +--------+     +--------+

崩溃时(Crash):
+--------+     +--------+     +--------+
|  App   |  X  |  WAL   |     |  Data  |
+--------+     +--------+     +--------+
                   |          (incomplete)
                   v
             (durable)

恢复过程(Recovery):
+--------+     +--------+     +--------+
|  WAL   | --> | Replay | --> |  Data  |
+--------+     +--------+     +--------+
                              (consistent)

崩溃恢复机制：
- 正常操作时，变更先写入WAL，再更新数据文件
- 崩溃时，WAL已持久化，但数据文件可能不完整
- 恢复时，重放WAL日志，将数据文件恢复到一致状态
这就是为什么数据库能在崩溃后保证数据不丢失。
```

The fundamental problem: disk writes are not atomic. A 4KB write might be
interrupted halfway through.

The solution: Write-Ahead Logging (WAL).
1. Before modifying any data page, write the intended change to a log.
2. The log is append-only and can be flushed sequentially (fast).
3. If the system crashes, replay the log to reconstruct the last consistent state.

---

## Why "Just Using Files" is Insufficient

Consider implementing the above four properties yourself:

1. **Durability**: Implement WAL, fsync correctly, handle partial writes
2. **Concurrency**: Implement locking or MVCC, handle deadlocks
3. **Query execution**: Implement parser, optimizer, multiple join algorithms
4. **Crash recovery**: Implement checkpoint, redo, undo, handle torn pages

Each of these is a multi-year engineering effort. Combining them into a correct,
performant system is decades of work.

PostgreSQL represents 30+ years of continuous development by hundreds of
engineers. When you use PostgreSQL, you get:

- Battle-tested durability guarantees
- Sophisticated concurrency control (MVCC)
- A mature query optimizer
- Proven crash recovery

The alternative is re-inventing all of this, badly.

---

## Summary

```
+------------------------------------------+
|           Why Databases Exist            |
+------------------------------------------+
|                                          |
|  Problem             Database Solution   |
|  ----------------    ------------------  |
|  Data Persistence -> WAL + Checkpoints   |
|  Concurrent Access-> MVCC + Locking      |
|  Data Corruption  -> ACID Transactions   |
|  Query Complexity -> SQL + Optimizer     |
|                                          |
+------------------------------------------+

数据库存在的原因总结：
- 数据持久化问题 -> 通过WAL和检查点解决
- 并发访问问题 -> 通过MVCC和锁机制解决
- 数据损坏问题 -> 通过ACID事务解决
- 查询复杂度问题 -> 通过SQL和优化器解决

数据库不仅仅是"存储数据的地方"，而是一个解决分布式系统中
最困难问题的完整系统：持久性、并发性、原子性和查询能力。
```

A database exists because the four fundamental problems of data storage
(persistence, concurrency, atomicity, querying) are individually hard and
interact in complex ways. A database is a unified solution to these problems.

In the next section, we will see why PostgreSQL specifically exists, and what
design choices it made to address these problems.
