# Section 11: Crash Recovery and Replication

WAL enables both crash recovery and replication. This section explains how
PostgreSQL recovers from crashes and how it replicates data to standby servers.

---

## 11.1 Crash Recovery

### Recovery Process Detail

```
Server Crash and Recovery Timeline:
+------------------------------------------------------------------+
|                                                                  |
|  Normal Operation:                                               |
|  [checkpoint]--[txns]--[txns]--[txns]--[CRASH]                   |
|       ^                                   |                      |
|       |                                   |                      |
|  Last known                          Incomplete                  |
|  good state                          transactions                |
|                                                                  |
|  Recovery:                                                       |
|  1. Find last checkpoint in pg_control                           |
|  2. Read WAL from checkpoint forward                             |
|  3. Redo all committed transactions                              |
|  4. Rollback all uncommitted transactions                        |
|  5. Database is consistent                                       |
|                                                                  |
+------------------------------------------------------------------+

崩溃恢复时间线：

正常运行：
- 定期检查点
- 事务处理
- 突然崩溃

恢复过程：
1. 从pg_control找到最后检查点
2. 从检查点开始读取WAL
3. 重做所有已提交事务
4. 回滚所有未提交事务
5. 数据库恢复到一致状态
```

### REDO and Recovery Manager

```
Resource Manager (rmgr) Architecture:
+------------------------------------------------------------------+
|                                                                  |
|  Each resource manager handles its own WAL records:              |
|                                                                  |
|  +------------+  +------------+  +------------+                  |
|  | Heap RM    |  | B-tree RM  |  | XLOG RM    |                  |
|  | (tables)   |  | (indexes)  |  | (control)  |                  |
|  +-----+------+  +-----+------+  +-----+------+                  |
|        |               |               |                         |
|        v               v               v                         |
|  +------------+  +------------+  +------------+                  |
|  | heap_redo  |  | btree_redo |  | xlog_redo  |                  |
|  +------------+  +------------+  +------------+                  |
|                                                                  |
|  During recovery, each WAL record is routed to                   |
|  the appropriate resource manager's redo function                |
|                                                                  |
+------------------------------------------------------------------+

资源管理器架构：

每种数据类型有自己的资源管理器：
- Heap RM: 处理表操作
- B-tree RM: 处理B-tree索引
- Hash RM: 处理哈希索引
- XLOG RM: 处理控制信息

恢复时：
- 读取WAL记录
- 根据rmid分发到对应的redo函数
- 各redo函数知道如何重放该类型的操作
```

Source: `src/backend/access/transam/rmgr.c`

---

## 11.2 Physical Replication

### Streaming Replication Architecture

```
Streaming Replication:
+------------------------------------------------------------------+
|                                                                  |
|   Primary Server                      Standby Server             |
|  +----------------+                  +----------------+          |
|  |   Application  |                  |                |          |
|  |    Writes      |                  |  (Read-only)   |          |
|  +-------+--------+                  +----------------+          |
|          |                                   ^                   |
|          v                                   |                   |
|  +-------+--------+                  +-------+--------+          |
|  |   WAL Writer   |                  | Startup/       |          |
|  |                |                  | Recovery       |          |
|  +-------+--------+                  +-------+--------+          |
|          |                                   ^                   |
|          v                                   |                   |
|  +-------+--------+     WAL          +-------+--------+          |
|  |   WAL Sender   |----------------->| WAL Receiver   |          |
|  |   Process      |   (streaming)    | Process        |          |
|  +----------------+                  +----------------+          |
|                                                                  |
+------------------------------------------------------------------+

流复制架构：

主服务器：
- 处理所有写操作
- WAL Sender进程发送WAL

备服务器：
- WAL Receiver进程接收WAL
- Startup进程应用WAL（持续恢复模式）
- 可以处理只读查询

复制流程：
1. 主服务器生成WAL
2. WAL Sender发送给Standby
3. WAL Receiver写入standby的pg_wal
4. Startup进程应用WAL
```

### Synchronous vs Asynchronous Replication

```
Asynchronous (default):
+------------------------------------------------------------------+
|                                                                  |
|  Primary:                                                        |
|  1. COMMIT                                                       |
|  2. Write WAL to disk                                            |
|  3. Return success to client  <-- Immediate                      |
|                                                                  |
|  Standby:                                                        |
|  4. Receive WAL (some time later)                                |
|  5. Apply WAL                                                    |
|                                                                  |
|  Risk: If primary crashes before step 4, data may be lost        |
|        on failover to standby                                    |
+------------------------------------------------------------------+

Synchronous:
+------------------------------------------------------------------+
|                                                                  |
|  Primary:                                                        |
|  1. COMMIT                                                       |
|  2. Write WAL to disk                                            |
|  3. Send WAL to standby                                          |
|  4. Wait for standby acknowledgment  <-- Blocks here             |
|  5. Return success to client                                     |
|                                                                  |
|  Standby:                                                        |
|  3. Receive WAL                                                  |
|  4. Write to disk (or apply)                                     |
|  5. Send acknowledgment                                          |
|                                                                  |
|  Guarantee: Committed data exists on at least 2 servers          |
+------------------------------------------------------------------+

异步复制（默认）：
- 主库提交后立即返回
- 备库稍后接收WAL
- 性能好，但可能丢失数据

同步复制：
- 主库等待备库确认
- 保证数据在多个服务器上
- 延迟增加，但更安全

配置：
synchronous_commit = on/off/remote_write/remote_apply
synchronous_standby_names = 'standby1, standby2'
```

---

## 11.3 Logical Replication

```
Logical vs Physical Replication:
+------------------------------------------------------------------+
|                                                                  |
|  Physical Replication:                                           |
|  - Replicates WAL bytes                                          |
|  - Standby is exact copy                                         |
|  - Same PostgreSQL version required                              |
|  - Cannot replicate to different schema                          |
|                                                                  |
|  Logical Replication:                                            |
|  - Replicates logical changes (INSERT, UPDATE, DELETE)           |
|  - Can replicate specific tables                                 |
|  - Can replicate across versions                                 |
|  - Can replicate to different schema                             |
|                                                                  |
+------------------------------------------------------------------+

Logical Replication Architecture:
+------------------------------------------------------------------+
|                                                                  |
|   Publisher                           Subscriber                 |
|  +----------------+                  +----------------+          |
|  |   Application  |                  |   Application  |          |
|  |    Writes      |                  | Reads & Writes |          |
|  +-------+--------+                  +-------+--------+          |
|          |                                   ^                   |
|          v                                   |                   |
|  +-------+--------+                  +-------+--------+          |
|  | WAL Decoder    |                  | Apply Worker   |          |
|  | (logical)      |                  |                |          |
|  +-------+--------+                  +-------+--------+          |
|          |          Logical            |                         |
|          +--------> Changes ---------->+                         |
|                    (SQL-like)                                    |
+------------------------------------------------------------------+

物理复制 vs 逻辑复制：

物理复制：
- 复制WAL字节
- 备库是精确副本
- 需要相同PostgreSQL版本
- 不能选择性复制

逻辑复制：
- 复制逻辑变更（INSERT等）
- 可以选择特定表
- 可以跨版本
- 可以复制到不同schema
- 订阅端可写

使用场景：
- 物理复制：高可用、灾难恢复
- 逻辑复制：数据迁移、多主、数据分发
```

---

## Summary

```
+------------------------------------------------------------------+
|           Crash Recovery and Replication Summary                 |
+------------------------------------------------------------------+
|                                                                  |
|  Crash Recovery:                                                 |
|  - Based on WAL replay                                           |
|  - Start from last checkpoint                                    |
|  - Redo committed, undo uncommitted                              |
|  - Resource managers handle their own records                    |
|                                                                  |
|  Physical Replication:                                           |
|  - Ship WAL to standby                                           |
|  - Standby continuously applies WAL                              |
|  - Synchronous or asynchronous                                   |
|  - Exact byte-level copy                                         |
|                                                                  |
|  Logical Replication:                                            |
|  - Decode WAL into logical changes                               |
|  - Replicate specific tables                                     |
|  - Cross-version compatible                                      |
|  - Subscriber can have different schema                          |
|                                                                  |
+------------------------------------------------------------------+

总结：

崩溃恢复：
- 基于WAL重放
- 从最后检查点开始
- 重做已提交，回滚未提交

物理复制：
- 传输WAL到备库
- 备库持续应用WAL
- 同步或异步
- 字节级精确副本

逻辑复制：
- 将WAL解码为逻辑变更
- 可以选择特定表
- 跨版本兼容
- 订阅端可以有不同schema

源代码：
- src/backend/access/transam/xlogrecovery.c - 恢复
- src/backend/replication/ - 复制
```
