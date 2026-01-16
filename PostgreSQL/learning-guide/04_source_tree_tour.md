# Section 4: PostgreSQL Source Tree Tour (Critical Section)

This section teaches you to navigate the PostgreSQL source code. Understanding
where code lives is prerequisite to understanding how it works.

---

## 4.1 Top-Level Structure

```
postgres/
|
+-- src/                    # All source code
|   +-- backend/            # The PostgreSQL server
|   +-- include/            # Header files
|   +-- bin/                # Client tools (psql, pg_dump, etc.)
|   +-- interfaces/         # Client libraries (libpq)
|   +-- common/             # Code shared between frontend and backend
|   +-- port/               # Platform-specific code
|   +-- pl/                 # Procedural languages (PL/pgSQL, etc.)
|   +-- test/               # Test suites
|   +-- tools/              # Development tools
|   +-- tutorial/           # SQL tutorial examples
|
+-- doc/                    # Documentation
|   +-- src/                # SGML documentation source
|
+-- contrib/                # Extensions and contrib modules
|
+-- config/                 # Build configuration files
|
+-- configure               # Build configuration script
+-- meson.build             # Meson build system (modern)
+-- GNUmakefile.in          # Make build system (traditional)

PostgreSQL顶层目录结构：

src/ - 所有源代码
  backend/ - 服务器核心代码
  include/ - 头文件（数据结构定义）
  bin/ - 客户端工具
  interfaces/ - 客户端库
  common/ - 前后端共享代码
  port/ - 平台特定代码
  pl/ - 过程语言实现
  test/ - 测试套件
  tools/ - 开发工具
  tutorial/ - SQL教程示例

doc/ - 文档源文件
contrib/ - 扩展模块
config/ - 构建配置
```

---

## 4.2 src/backend - The Server Core

This is where PostgreSQL's main functionality lives.

```
src/backend/
|
+-- access/           # Storage access methods
|   +-- brin/         #   BRIN index
|   +-- common/       #   Common access code
|   +-- gin/          #   GIN index
|   +-- gist/         #   GiST index
|   +-- hash/         #   Hash index
|   +-- heap/         #   Heap (table) access
|   +-- index/        #   Index common code
|   +-- nbtree/       #   B-tree index
|   +-- spgist/       #   SP-GiST index
|   +-- table/        #   Table access layer
|   +-- transam/      #   Transaction manager + WAL
|
+-- catalog/          # System catalog management
|
+-- commands/         # SQL command implementations
|
+-- executor/         # Query executor
|
+-- foreign/          # Foreign data wrappers
|
+-- lib/              # Internal libraries
|
+-- libpq/            # Backend side of client protocol
|
+-- main/             # Server entry point
|
+-- nodes/            # Node types for parse/plan trees
|
+-- optimizer/        # Query planner/optimizer
|   +-- geqo/         #   Genetic query optimizer
|   +-- path/         #   Access path generation
|   +-- plan/         #   Plan generation
|   +-- prep/         #   Preprocessing
|   +-- util/         #   Utilities and cost estimation
|
+-- parser/           # SQL parser
|
+-- partitioning/     # Table partitioning
|
+-- postmaster/       # Main process and process management
|
+-- replication/      # Streaming replication
|
+-- rewrite/          # Query rewriter (views, rules)
|
+-- statistics/       # Statistics collection
|
+-- storage/          # Storage subsystem
|   +-- buffer/       #   Buffer pool management
|   +-- file/         #   File management
|   +-- freespace/    #   Free space map
|   +-- ipc/          #   Inter-process communication
|   +-- large_object/ #   Large object storage
|   +-- lmgr/         #   Lock manager
|   +-- page/         #   Page-level operations
|   +-- smgr/         #   Storage manager
|   +-- sync/         #   Synchronization primitives
|
+-- tcop/             # Traffic cop (query dispatch)
|
+-- tsearch/          # Full-text search
|
+-- utils/            # Utility functions
    +-- adt/          #   Built-in data types
    +-- cache/        #   System caches
    +-- error/        #   Error handling
    +-- fmgr/         #   Function manager
    +-- hash/         #   Hashing utilities
    +-- init/         #   Backend initialization
    +-- mb/           #   Multi-byte/encoding support
    +-- misc/         #   Miscellaneous utilities
    +-- mmgr/         #   Memory management
    +-- sort/         #   Sorting algorithms
    +-- time/         #   Date/time utilities

src/backend/ 目录详解：

access/ - 存储访问方法
  这里实现了PostgreSQL的所有数据访问方式：
  - heap/: 堆表（普通表）的读写
  - nbtree/: B-tree索引实现
  - hash/: 哈希索引
  - gin/: 倒排索引（全文搜索用）
  - gist/: 通用搜索树（地理数据等）
  - brin/: 块范围索引（大表）
  - transam/: 事务管理和WAL

optimizer/ - 查询优化器
  - path/: 生成可能的访问路径
  - plan/: 将最优路径转换为执行计划
  - prep/: 查询预处理
  - util/: 代价估算

storage/ - 存储子系统
  - buffer/: 缓冲池管理（内存中的页缓存）
  - lmgr/: 锁管理器
  - smgr/: 存储管理器抽象层

executor/ - 查询执行器
  每个node*.c文件实现一种执行节点

parser/ - SQL解析器
  - scan.l: 词法分析（Flex）
  - gram.y: 语法分析（Bison）

utils/ - 工具函数
  - adt/: 内置数据类型实现（int, text, date等）
  - mmgr/: 内存上下文管理
```

### Key Files to Know

```
File                                    Purpose
------------------------------------    ----------------------------------
src/backend/main/main.c                 Server entry point
src/backend/postmaster/postmaster.c     Main process, forks backends
src/backend/tcop/postgres.c             Backend main loop
src/backend/parser/gram.y               SQL grammar
src/backend/parser/scan.l               SQL lexer
src/backend/optimizer/plan/planner.c    Query planner entry point
src/backend/executor/execMain.c         Executor entry point
src/backend/access/heap/heapam.c        Heap (table) access methods
src/backend/access/nbtree/nbtree.c      B-tree index operations
src/backend/storage/buffer/bufmgr.c     Buffer pool management
src/backend/storage/lmgr/lock.c         Lock manager
src/backend/access/transam/xlog.c       WAL core implementation

重要文件索引：

main/main.c
  程序入口点，启动postmaster或单用户后端

postmaster/postmaster.c
  主进程，监听连接，fork后端进程

tcop/postgres.c
  后端主循环，接收并处理客户端请求

parser/gram.y
  SQL语法定义，bison格式
  想理解SQL语法？从这里开始

optimizer/plan/planner.c
  优化器入口，将查询树转换为执行计划

executor/execMain.c
  执行器入口，运行执行计划

access/heap/heapam.c
  堆表访问方法（INSERT/UPDATE/DELETE/SELECT如何操作表）

access/nbtree/nbtree.c
  B-tree索引实现

storage/buffer/bufmgr.c
  缓冲池管理（PostgreSQL与磁盘之间的缓存层）

storage/lmgr/lock.c
  锁管理器实现

access/transam/xlog.c
  WAL核心实现
```

---

## 4.3 src/include - Header Files

The include directory mirrors the backend structure and contains:
- Data structure definitions
- Function declarations
- Macros
- Constants

```
src/include/
|
+-- access/           # Access method headers
|   +-- htup.h        #   Heap tuple definition
|   +-- htup_details.h#   Detailed tuple operations
|   +-- xlog.h        #   WAL definitions
|   +-- transam.h     #   Transaction definitions
|
+-- catalog/          # System catalog definitions
|   +-- pg_class.h    #   pg_class system table
|   +-- pg_type.h     #   pg_type system table
|
+-- executor/         # Executor headers
|
+-- nodes/            # Parse and plan tree nodes
|   +-- parsenodes.h  #   Parse tree node definitions
|   +-- plannodes.h   #   Plan tree node definitions
|   +-- execnodes.h   #   Executor state nodes
|
+-- optimizer/        # Optimizer headers
|
+-- parser/           # Parser headers
|
+-- storage/          # Storage headers
|   +-- block.h       #   Block number definitions
|   +-- buf.h         #   Buffer definitions
|   +-- bufpage.h     #   Page layout
|   +-- itemid.h      #   Item identifier (line pointer)
|   +-- lock.h        #   Lock definitions
|
+-- utils/            # Utility headers
|   +-- memutils.h    #   Memory context definitions
|   +-- palloc.h      #   Memory allocation
|
+-- postgres.h        # Main include file
+-- c.h               # Fundamental C definitions

src/include/ 关键头文件：

postgres.h
  主头文件，几乎所有后端代码都包含它

c.h
  基础C定义：类型别名、宏、平台抽象

storage/bufpage.h
  页面布局定义
  理解PostgreSQL存储的关键文件

storage/itemid.h
  行指针定义

access/htup_details.h
  堆元组（行）的详细结构
  包含xmin/xmax等MVCC字段

nodes/parsenodes.h
  解析树节点定义
  每种SQL语句对应一种节点类型

nodes/plannodes.h
  执行计划节点定义
  每种执行操作对应一种节点类型

这些头文件定义了PostgreSQL的核心数据结构。
理解它们是理解源代码的基础。
```

### Critical Data Structures

```c
// From src/include/storage/bufpage.h - Page Layout

/*
 * +----------------+---------------------------------+
 * | PageHeaderData | linp1 linp2 linp3 ...           |
 * +-----------+----+---------------------------------+
 * | ... linpN |                                      |
 * +-----------+--------------------------------------+
 * |           ^ pd_lower                             |
 * |                                                  |
 * |             v pd_upper                           |
 * +-------------+------------------------------------+
 * |             | tupleN ...                         |
 * +-------------+------------------+-----------------+
 * |    ... tuple3 tuple2 tuple1   | "special space" |
 * +-------------------------------+-----------------+
 *                                  ^ pd_special
 */

typedef struct PageHeaderData
{
    PageXLogRecPtr pd_lsn;      /* LSN of last change */
    uint16      pd_checksum;    /* Page checksum */
    uint16      pd_flags;       /* Flag bits */
    LocationIndex pd_lower;     /* Offset to start of free space */
    LocationIndex pd_upper;     /* Offset to end of free space */
    LocationIndex pd_special;   /* Offset to special space */
    uint16      pd_pagesize_version;
    TransactionId pd_prune_xid; /* Oldest prunable XID */
    ItemIdData  pd_linp[FLEXIBLE_ARRAY_MEMBER]; /* Line pointers */
} PageHeaderData;

页面结构（PageHeaderData）：

pd_lsn: 页面最后修改的WAL位置
  用于崩溃恢复时判断页面是否需要重放

pd_checksum: 页面校验和
  检测存储损坏

pd_lower: 空闲空间起始位置
  行指针数组向下增长

pd_upper: 空闲空间结束位置
  元组数据向上增长

pd_special: 特殊空间起始位置
  索引页用于存储索引特有数据

pd_linp[]: 行指针数组
  每个指针指向页面内的一个元组
```

```c
// From src/include/access/htup_details.h - Heap Tuple Header

struct HeapTupleHeaderData
{
    union
    {
        HeapTupleFields t_heap;   /* For on-disk tuples */
        DatumTupleFields t_datum; /* For in-memory composites */
    }           t_choice;

    ItemPointerData t_ctid;      /* Current TID or newer version */

    uint16      t_infomask2;     /* Attributes + flags */
    uint16      t_infomask;      /* Various flag bits */
    uint8       t_hoff;          /* Header size */

    bits8       t_bits[FLEXIBLE_ARRAY_MEMBER]; /* Null bitmap */
};

typedef struct HeapTupleFields
{
    TransactionId t_xmin;        /* Inserting transaction ID */
    TransactionId t_xmax;        /* Deleting/locking transaction ID */
    /* ... */
} HeapTupleFields;

堆元组头部（HeapTupleHeaderData）：

t_xmin: 插入该元组的事务ID
  用于判断元组对哪些事务可见

t_xmax: 删除/锁定该元组的事务ID
  如果为0或无效，元组未被删除
  如果有效，需检查该事务状态

t_ctid: 当前元组ID或更新版本的位置
  UPDATE时指向新版本
  用于跟踪元组版本链

t_infomask: 标志位
  HEAP_XMIN_COMMITTED: xmin已提交
  HEAP_XMAX_INVALID: xmax无效（未删除）

t_hoff: 头部大小
  数据从t_hoff偏移处开始

t_bits[]: NULL位图
  标记哪些列为NULL

这些字段是MVCC的核心。
每个元组都携带版本信息，使得不同事务可以看到不同版本。
```

---

## 4.4 src/bin - Client Tools

```
src/bin/
|
+-- initdb/           # Database cluster initialization
+-- pg_ctl/           # Server control utility
+-- pg_dump/          # Backup utility
+-- pg_restore/       # Restore utility (implicit in pg_dump)
+-- psql/             # Interactive terminal
+-- pg_basebackup/    # Physical backup
+-- pg_resetwal/      # Reset WAL (dangerous!)
+-- pg_controldata/   # Display control file
+-- pg_waldump/       # Dump WAL contents
+-- pgbench/          # Benchmark tool

客户端工具：

initdb/
  初始化数据库集群
  创建目录结构、系统表、初始数据库

pg_ctl/
  服务器启停控制

pg_dump/ & pg_restore/
  逻辑备份和恢复
  导出SQL或自定义格式

psql/
  交互式SQL终端
  PostgreSQL最常用的客户端

pg_basebackup/
  物理备份（复制整个数据目录）

pg_waldump/
  查看WAL内容
  调试和学习WAL的好工具

pgbench/
  性能基准测试
```

---

## 4.5 src/interfaces - Client Libraries

```
src/interfaces/
|
+-- libpq/            # C client library
|   +-- fe-auth.c     #   Authentication
|   +-- fe-connect.c  #   Connection management
|   +-- fe-exec.c     #   Query execution
|   +-- fe-protocol3.c#   Protocol v3 implementation
|
+-- ecpg/             # Embedded SQL preprocessor

客户端库：

libpq/
  PostgreSQL的官方C客户端库
  所有其他语言的驱动通常封装libpq

  fe-connect.c: 连接建立和管理
  fe-exec.c: 查询执行
  fe-auth.c: 认证处理

ecpg/
  嵌入式SQL预处理器
  将SQL嵌入C代码
```

---

## 4.6 doc - Documentation

```
doc/
|
+-- src/              # Documentation source (SGML)
|   +-- sgml/         #   Main documentation
|       +-- ref/      #     Reference pages (man pages)
|
+-- FAQ               # Frequently asked questions
+-- TODO              # Development roadmap
+-- KNOWN_BUGS        # Known issues
+-- MISSING_FEATURES  # Features not yet implemented

文档目录：

src/sgml/
  PostgreSQL文档源文件（SGML格式）
  这是PostgreSQL文档质量高的原因之一：
  文档与代码同步维护

TODO
  开发路线图
  了解未来方向的好地方

KNOWN_BUGS & MISSING_FEATURES
  已知问题和缺失功能
  判断某功能是否存在的快速参考
```

### Why PostgreSQL Documentation is Excellent

```
+--------------------------------------------------------------+
|           PostgreSQL Documentation Philosophy                |
+--------------------------------------------------------------+
|                                                              |
| 1. Documentation is code                                     |
|    - Lives in same repo                                      |
|    - Reviewed with patches                                   |
|    - Versioned with releases                                 |
|                                                              |
| 2. Reference + Tutorial + Internals                          |
|    - Reference: Every SQL command, function, config          |
|    - Tutorial: Learning path for new users                   |
|    - Internals: Appendices on source code                    |
|                                                              |
| 3. Community maintained                                      |
|    - Updates encouraged from anyone                          |
|    - Errors quickly corrected                                |
|                                                              |
+--------------------------------------------------------------+

PostgreSQL文档为何优秀：

1. 文档即代码
   - 与源码在同一仓库
   - 补丁必须包含文档更新
   - 与版本同步发布

2. 三层结构
   - 参考手册：所有SQL命令、函数、配置
   - 教程：新用户学习路径
   - 内部文档：源代码说明

3. 社区维护
   - 鼓励任何人贡献
   - 错误快速修正

这与许多商业数据库形成对比，
后者的文档经常过时或不完整。
```

---

## 4.7 Code Navigation Tips

### Finding Where Something is Implemented

```bash
# Find where a function is defined
grep -rn "FunctionName" src/backend/

# Find where a data type is defined
grep -rn "typedef.*TypeName" src/include/

# Find SQL grammar for a command
grep -n "CREATE TABLE" src/backend/parser/gram.y

# Find executor node for SeqScan
ls src/backend/executor/nodeSeq*

# Find all B-tree related code
find src -name "*btree*" -o -name "*nbtree*"

代码导航技巧：

# 查找函数定义
grep -rn "FunctionName" src/backend/

# 查找类型定义
grep -rn "typedef.*TypeName" src/include/

# 查找SQL语法
grep -n "CREATE TABLE" src/backend/parser/gram.y

# 查找特定执行节点
ls src/backend/executor/node*

# 使用ctags/cscope（推荐）
ctags -R src/
cscope -Rb
```

### Reading Code - Entry Points

```
To understand query processing:
1. Start at src/backend/tcop/postgres.c:PostgresMain()
2. Follow exec_simple_query() for simple queries
3. Follow exec_execute_message() for prepared statements

To understand storage:
1. Start at src/backend/storage/buffer/bufmgr.c
2. Look at ReadBuffer() and ReleaseBuffer()
3. Trace down to src/backend/storage/smgr/md.c

To understand locking:
1. Start at src/backend/storage/lmgr/lock.c
2. Look at LockAcquire() and LockRelease()
3. See deadlock.c for deadlock detection

代码阅读入口点：

查询处理流程：
1. tcop/postgres.c:PostgresMain() - 后端主循环
2. exec_simple_query() - 简单查询处理
3. 跟踪到parser, optimizer, executor

存储系统：
1. storage/buffer/bufmgr.c - 缓冲管理
2. ReadBuffer() - 读取页面
3. 跟踪到smgr/md.c - 实际文件操作

锁机制：
1. storage/lmgr/lock.c - 锁管理
2. LockAcquire() - 获取锁
3. deadlock.c - 死锁检测

事务和WAL：
1. access/transam/xact.c - 事务管理
2. access/transam/xlog.c - WAL核心
```

---

## Summary

```
+--------------------------------------------------------------+
|              Source Tree Navigation Guide                     |
+--------------------------------------------------------------+
|                                                              |
| Where to find:                                               |
|                                                              |
| SQL parsing        -> src/backend/parser/                    |
| Query optimization -> src/backend/optimizer/                 |
| Query execution    -> src/backend/executor/                  |
| Table storage      -> src/backend/access/heap/               |
| Index code         -> src/backend/access/nbtree/ (etc.)      |
| Buffer management  -> src/backend/storage/buffer/            |
| Lock management    -> src/backend/storage/lmgr/              |
| WAL implementation -> src/backend/access/transam/            |
| Data structures    -> src/include/                           |
| Client tools       -> src/bin/                               |
| Client library     -> src/interfaces/libpq/                  |
|                                                              |
+--------------------------------------------------------------+

源代码导航速查：

SQL解析       -> src/backend/parser/
查询优化      -> src/backend/optimizer/
查询执行      -> src/backend/executor/
表存储        -> src/backend/access/heap/
索引代码      -> src/backend/access/nbtree/ (B-tree)
              -> src/backend/access/gin/ (倒排索引)
              -> src/backend/access/gist/ (通用搜索树)
缓冲管理      -> src/backend/storage/buffer/
锁管理        -> src/backend/storage/lmgr/
WAL实现       -> src/backend/access/transam/
数据结构定义  -> src/include/
客户端工具    -> src/bin/
客户端库      -> src/interfaces/libpq/

下一节我们将详细分析SQL处理流水线。
```
