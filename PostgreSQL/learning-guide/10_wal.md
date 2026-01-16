# Section 10: Write-Ahead Logging (WAL)

WAL is the foundation of PostgreSQL's durability and crash recovery.
Understanding WAL is essential for understanding PostgreSQL's reliability
guarantees.

---

## 10.1 Why WAL Exists

### The Fundamental Problem

```
Problem: Disk writes are not atomic
+------------------------------------------------------------------+
|                                                                  |
|  An 8KB page write can be interrupted:                           |
|                                                                  |
|  +------------------+                                            |
|  | Page in memory   |  (consistent)                              |
|  | [header][data...]|                                            |
|  +------------------+                                            |
|          |                                                       |
|          | write()                                               |
|          v                                                       |
|  +------------------+                                            |
|  | Page on disk     |  (partially written = TORN PAGE)           |
|  | [header][????...]|                                            |
|  +------------------+                                            |
|          ^                                                       |
|          | CRASH during write!                                   |
|                                                                  |
|  Result: Data page is corrupted                                  |
+------------------------------------------------------------------+

问题：磁盘写入不是原子的

8KB页面写入可能被中断：
- 只写入部分数据
- 产生"撕裂页"（torn page）
- 页面损坏，无法恢复

更严重的问题：
- 事务可能修改多个页面
- 崩溃时可能只有部分页面写入
- 数据库状态不一致
```

### The WAL Solution

```
WAL Principle: "Write log before data"
+------------------------------------------------------------------+
|                                                                  |
|  Without WAL:                                                    |
|  1. Modify page A in memory                                      |
|  2. Write page A to disk                                         |
|  3. Modify page B in memory                                      |
|  4. Write page B to disk  <-- CRASH HERE                         |
|  Result: Page A updated, page B not = INCONSISTENT               |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  With WAL:                                                       |
|  1. Modify page A in memory                                      |
|  2. Write WAL record for A to WAL buffer                         |
|  3. Modify page B in memory                                      |
|  4. Write WAL record for B to WAL buffer                         |
|  5. COMMIT: Flush WAL buffer to disk (fsync)                     |
|  6. Return success to client                                     |
|  7. Later: Write data pages to disk (checkpoint)                 |
|                                                                  |
|  If crash after step 5:                                          |
|  - WAL is safe on disk                                           |
|  - Recovery replays WAL to reconstruct pages A and B             |
|                                                                  |
|  If crash before step 5:                                         |
|  - Transaction not committed (WAL not flushed)                   |
|  - Changes are lost, but that's OK (transaction was uncommitted) |
|                                                                  |
+------------------------------------------------------------------+

WAL原理："先写日志，后写数据"

关键洞察：
- 顺序写入（日志）比随机写入（数据页）快
- 只要日志持久化，数据可以重建
- COMMIT时只需要刷写日志

WAL保证：
- 已提交事务的修改不会丢失
- 未提交事务的修改可以回滚
- 崩溃后能恢复到一致状态
```

---

## 10.2 WAL Mechanics

### WAL Record Structure

```
WAL Record:
+------------------------------------------------------------------+
|  +------------------+                                            |
|  | XLogRecord       |  Header                                    |
|  |   xl_tot_len     |  Total record length                       |
|  |   xl_xid         |  Transaction ID                            |
|  |   xl_prev        |  Position of previous record               |
|  |   xl_info        |  Resource manager specific info            |
|  |   xl_rmid        |  Resource manager ID                       |
|  |   xl_crc         |  CRC checksum                              |
|  +------------------+                                            |
|  +------------------+                                            |
|  | XLogRecordBlock  |  Block reference (if any)                  |
|  |   file/block#    |  Which page was modified                   |
|  +------------------+                                            |
|  +------------------+                                            |
|  | Data             |  Actual modification data                  |
|  |   (varies)       |  Could be tuple data, index entry, etc.    |
|  +------------------+                                            |
+------------------------------------------------------------------+

WAL记录结构：

XLogRecord头部：
- xl_tot_len: 记录总长度
- xl_xid: 事务ID
- xl_prev: 前一条记录位置（链表）
- xl_rmid: 资源管理器ID（heap, btree等）
- xl_crc: CRC校验和

块引用：
- 修改了哪个文件的哪个块
- 可能包含完整页面镜像（full page write）

数据：
- 实际的修改内容
- 格式取决于操作类型
```

### WAL Segments and LSN

```
WAL Organization:
+------------------------------------------------------------------+
|                                                                  |
|  WAL Segment Files (16MB each by default):                       |
|                                                                  |
|  pg_wal/                                                         |
|    000000010000000000000001  (first segment)                     |
|    000000010000000000000002  (second segment)                    |
|    000000010000000000000003  (third segment)                     |
|    ...                                                           |
|                                                                  |
|  LSN (Log Sequence Number):                                      |
|  +------------------+------------------+                         |
|  |  Segment Number  |  Offset in Segment|                        |
|  |     (32 bit)     |     (32 bit)      |                        |
|  +------------------+------------------+                         |
|                                                                  |
|  Example: LSN = 0/16B3A80                                        |
|  - Segment: 0x00000001 = segment 1                               |
|  - Offset: 0x6B3A80 in that segment                              |
|                                                                  |
+------------------------------------------------------------------+

WAL段文件：
- 默认16MB一个段
- 文件名编码段号
- 按顺序追加写入

LSN（日志序列号）：
- 64位数字
- 唯一标识WAL中的位置
- 高32位：段号
- 低32位：段内偏移

LSN的用途：
- 页面的pd_lsn记录最后修改的LSN
- 恢复时比较LSN决定是否需要重放
- 复制时跟踪复制进度
```

### WAL Buffer and Flushing

```
WAL Write Path:
+------------------------------------------------------------------+
|                                                                  |
|  Backend Process                                                 |
|  +--------------+                                                |
|  | Transaction  |                                                |
|  |   INSERT     |                                                |
|  +------+-------+                                                |
|         |                                                        |
|         v                                                        |
|  +--------------+                                                |
|  | XLogInsert() |  Create WAL record                             |
|  +------+-------+                                                |
|         |                                                        |
|         v                                                        |
|  +------+-------+                                                |
|  | WAL Buffers  |  (shared memory)                               |
|  | [rec1][rec2] |  Append record to buffer                       |
|  +------+-------+                                                |
|         |                                                        |
|         | At COMMIT (or buffer full)                             |
|         v                                                        |
|  +--------------+                                                |
|  | XLogFlush()  |  Write + fsync to disk                         |
|  +------+-------+                                                |
|         |                                                        |
|         v                                                        |
|  +------+-------+                                                |
|  | pg_wal/      |  WAL segment files                             |
|  | 0000000100.. |                                                |
|  +--------------+                                                |
|                                                                  |
+------------------------------------------------------------------+

WAL写入路径：

1. XLogInsert()
   - 构造WAL记录
   - 复制到WAL缓冲区
   - 返回LSN

2. WAL缓冲区
   - 共享内存区域
   - 多个后端并发写入
   - 使用WAL插入锁协调

3. XLogFlush()
   - 将缓冲区内容写入磁盘
   - 调用fsync确保持久化
   - COMMIT时必须完成

关键参数：
- wal_buffers: WAL缓冲区大小
- synchronous_commit: 是否同步提交
- wal_sync_method: fsync方法
```

---

## 10.3 Checkpoints

```
Checkpoint: Ensuring recovery starts from a known point
+------------------------------------------------------------------+
|                                                                  |
|  Time --->                                                       |
|                                                                  |
|  WAL:  [records...]  [records...]  [records...]  [records...]    |
|                           ^                                      |
|                           |                                      |
|                      Checkpoint                                  |
|                                                                  |
|  At Checkpoint:                                                  |
|  1. Flush all dirty pages to disk                                |
|  2. Write checkpoint record to WAL                               |
|  3. Update pg_control with checkpoint location                   |
|                                                                  |
|  Recovery:                                                       |
|  - Start from last checkpoint                                    |
|  - Replay WAL records after checkpoint                           |
|  - Don't need to replay everything from beginning                |
|                                                                  |
+------------------------------------------------------------------+

检查点：

目的：
- 限制恢复时间
- 允许回收旧WAL段
- 确保数据页与WAL一致

检查点操作：
1. 将所有脏页刷写到磁盘
2. 在WAL中写入检查点记录
3. 更新pg_control文件

检查点后：
- 检查点之前的WAL可以删除
- 恢复只需从检查点开始

配置参数：
- checkpoint_timeout: 检查点间隔（默认5分钟）
- max_wal_size: 触发检查点的WAL大小
- checkpoint_completion_target: 检查点完成比例
```

### Full Page Writes

```
Full Page Write: Protection against torn pages
+------------------------------------------------------------------+
|                                                                  |
|  Problem: What if data page is torn during write?                |
|                                                                  |
|  Solution: Write full page image on first modification           |
|            after checkpoint                                      |
|                                                                  |
|  Checkpoint                                                      |
|      |                                                           |
|      v                                                           |
|  Page X first modified:                                          |
|  +------------------+                                            |
|  | WAL Record       |                                            |
|  | [full 8KB page]  |  <-- Complete copy of page                 |
|  | [modification]   |                                            |
|  +------------------+                                            |
|                                                                  |
|  Page X modified again:                                          |
|  +------------------+                                            |
|  | WAL Record       |                                            |
|  | [modification]   |  <-- Only the change (no full page)        |
|  +------------------+                                            |
|                                                                  |
|  Recovery: If page is torn, restore from full page image         |
|                                                                  |
+------------------------------------------------------------------+

全页写入（Full Page Write）：

问题：如果数据页写入时撕裂怎么办？

解决方案：
- 检查点后首次修改页面时，WAL包含完整页面镜像
- 后续修改只记录增量
- 恢复时，撕裂页可以从WAL恢复

配置：
- full_page_writes: 启用/禁用（默认开启）
- 禁用可提高性能，但有数据损坏风险

代价：
- WAL体积增大（尤其是检查点后）
- 这就是为什么checkpoint_completion_target有用
```

---

## 10.4 Crash Recovery Flow

```
Recovery Process:
+------------------------------------------------------------------+
|                                                                  |
|  1. Startup: Read pg_control                                     |
|     - Find last checkpoint location                              |
|     - Determine if clean shutdown or crash                       |
|                                                                  |
|  2. If crash recovery needed:                                    |
|     +--------------------------------------------------+         |
|     |                                                  |         |
|     |  Read WAL from checkpoint                        |         |
|     |          |                                       |         |
|     |          v                                       |         |
|     |  For each WAL record:                            |         |
|     |    - Read the page it references                 |         |
|     |    - If page LSN < record LSN:                   |         |
|     |        Apply the record (redo)                   |         |
|     |    - If page LSN >= record LSN:                  |         |
|     |        Skip (already applied)                    |         |
|     |          |                                       |         |
|     |          v                                       |         |
|     |  Continue until end of WAL                       |         |
|     |          |                                       |         |
|     |          v                                       |         |
|     |  Database is now consistent                      |         |
|     +--------------------------------------------------+         |
|                                                                  |
|  3. Mark recovery complete                                       |
|  4. Begin accepting connections                                  |
|                                                                  |
+------------------------------------------------------------------+

崩溃恢复流程：

1. 启动时读取pg_control
   - 获取最后检查点位置
   - 判断是否需要恢复

2. 如果需要恢复：
   - 从检查点开始读取WAL
   - 对每条WAL记录：
     - 如果页面LSN < 记录LSN：应用记录
     - 如果页面LSN >= 记录LSN：跳过
   - 直到WAL结束

3. 标记恢复完成
4. 开始接受连接

LSN比较的重要性：
- 确保幂等性
- 已应用的记录不会重复应用
- 即使恢复中断也能继续
```

Source: `src/backend/access/transam/xlogrecovery.c`

---

## Summary

```
+------------------------------------------------------------------+
|                      WAL Summary                                 |
+------------------------------------------------------------------+
|                                                                  |
|  Core Principle:                                                 |
|  - Write log before data                                         |
|  - Log flush at commit guarantees durability                     |
|  - Data pages written lazily (checkpoint)                        |
|                                                                  |
|  WAL Components:                                                 |
|  - WAL records: Individual change records                        |
|  - WAL segments: 16MB files in pg_wal/                           |
|  - LSN: Log Sequence Number (position in WAL)                    |
|  - WAL buffers: Shared memory buffer for WAL writes              |
|                                                                  |
|  Checkpoints:                                                    |
|  - Flush dirty pages to disk                                     |
|  - Allow recovery to start from checkpoint                       |
|  - Enable WAL segment recycling                                  |
|                                                                  |
|  Full Page Writes:                                               |
|  - Protect against torn pages                                    |
|  - Store complete page image on first modification               |
|                                                                  |
|  Recovery:                                                       |
|  - Read WAL from last checkpoint                                 |
|  - Apply records to pages (if needed)                            |
|  - Compare page LSN with record LSN                              |
|                                                                  |
+------------------------------------------------------------------+

WAL总结：

核心原则：
- 先写日志，后写数据
- 提交时刷写日志保证持久性
- 数据页延迟写入（检查点）

WAL组件：
- WAL记录：单个变更记录
- WAL段：pg_wal/中的16MB文件
- LSN：日志序列号
- WAL缓冲：共享内存缓冲区

检查点：
- 刷写脏页到磁盘
- 限制恢复起点
- 允许回收旧WAL段

全页写入：
- 防止撕裂页
- 检查点后首次修改存储完整页面

恢复：
- 从最后检查点读取WAL
- 按需应用记录到页面
- 使用LSN比较确保幂等

关键源文件：
- src/backend/access/transam/xlog.c - WAL核心
- src/backend/access/transam/xloginsert.c - WAL插入
- src/backend/access/transam/xlogrecovery.c - 恢复

下一节我们将学习崩溃恢复和复制。
```
