# Section 2: Why PostgreSQL Exists

PostgreSQL is not the first database. It was created because existing databases
made specific tradeoffs that the PostgreSQL designers disagreed with.
Understanding these historical decisions explains why PostgreSQL works the way
it does today.

---

## 2.1 Historical Context

### The Ingres Project (1970s)

```
Timeline of Database History:
+------------------------------------------------------------------+
|                                                                  |
| 1970    Codd's Relational Model (IBM Research)                   |
|   |                                                              |
| 1973    INGRES Project Begins (UC Berkeley)                      |
|   |     - Michael Stonebraker et al.                             |
|   |     - First practical relational database                    |
|   |                                                              |
| 1979    Oracle V2 Released (commercial)                          |
|   |                                                              |
| 1984    INGRES commercialized                                    |
|   |                                                              |
| 1986    POSTGRES Project Begins (UC Berkeley)                    |
|   |     - "Post-Ingres" = POSTGRES                               |
|   |     - Stonebraker's new research project                     |
|   |                                                              |
| 1989    First POSTGRES prototype                                 |
|   |                                                              |
| 1994    Postgres95 (SQL support added)                           |
|   |     - Andrew Yu and Jolly Chen                               |
|   |                                                              |
| 1996    PostgreSQL 6.0 (name changed, open source)               |
|   |                                                              |
| 2024    PostgreSQL 17.x (current)                                |
|   v                                                              |
+------------------------------------------------------------------+

数据库发展时间线：
- 1970年：Codd提出关系模型理论
- 1973年：UC Berkeley启动INGRES项目，首个实用关系数据库
- 1986年：POSTGRES项目启动，名字意为"Post-Ingres"（Ingres之后）
- 1994年：添加SQL支持，更名为Postgres95
- 1996年：更名为PostgreSQL，完全开源
这条时间线展示了PostgreSQL的学术起源和持续30年的发展历程。
```

Michael Stonebraker led the INGRES project at UC Berkeley. INGRES proved that
relational databases were practical, not just theoretical.

But by the mid-1980s, Stonebraker saw limitations in the relational model:

1. **No extensibility**: Users could not define new data types
2. **No complex objects**: Hard to store nested or structured data
3. **No rules**: Business logic had to live entirely in the application

### The POSTGRES Research Project (1986-1994)

POSTGRES was explicitly designed to solve these problems:

```
+-----------------------------+
|     POSTGRES Goals          |
+-----------------------------+
| 1. Extensibility            |
|    - Custom data types      |
|    - Custom operators       |
|    - Custom index methods   |
|                             |
| 2. Object-Relational        |
|    - Complex data types     |
|    - Inheritance            |
|    - Arrays, composites     |
|                             |
| 3. Active Database          |
|    - Rules system           |
|    - Triggers               |
|                             |
| 4. No Overwrite Storage     |
|    - Time-travel queries    |
|    - Historical data access |
+-----------------------------+

POSTGRES项目的设计目标：
1. 可扩展性 - 允许用户定义新的数据类型、运算符和索引方法
2. 对象关系模型 - 支持复杂数据类型、继承、数组和复合类型
3. 主动数据库 - 内置规则系统和触发器
4. 非覆盖存储 - 支持时间旅行查询，可访问历史数据

这些目标直接影响了PostgreSQL今天的架构设计。
```

The key insight: a database should be **extensible by design**, not as an
afterthought. Users should be able to:

- Define new data types (like `inet` for IP addresses, `geometry` for GIS)
- Define new operators (`&&` for array overlap)
- Define new index types (GiST, GIN for specialized workloads)
- Define new procedural languages (PL/Python, PL/Perl)

This extensibility is not bolted on. It is fundamental to PostgreSQL's
architecture.

### From Research to Production

```
POSTGRES (Research)                 PostgreSQL (Production)
+-------------------+               +-------------------+
| POSTQUEL language |    -->        | SQL language      |
| Academic codebase |    -->        | Production-ready  |
| Single developer  |    -->        | Open-source       |
| No optimization   |    -->        | Cost-based opt.   |
+-------------------+               +-------------------+
       1986-1993                         1996-present

从研究项目到生产系统的演变：
- POSTQUEL语言 -> 标准SQL语言
- 学术代码库 -> 生产级代码
- 单一开发者 -> 开源社区
- 无优化器 -> 基于代价的优化器

Postgres95/PostgreSQL的贡献者将学术原型转变为可靠的生产系统。
```

In 1994, Andrew Yu and Jolly Chen added SQL support (replacing the original
POSTQUEL language) and released Postgres95. In 1996, the project was renamed
PostgreSQL and became a community-driven open source project.

---

## 2.2 PostgreSQL Design Philosophy

PostgreSQL makes specific tradeoffs that differentiate it from other databases.

### Correctness Over Speed

```
        Oracle/MySQL Approach         PostgreSQL Approach
        ----------------------         --------------------
        Fast by default               Correct by default
        "Trust the DBA"               "Prevent mistakes"
        Relax constraints for perf    Enforce constraints always

             Performance                    Correctness
                 ^                              ^
                 |    Oracle/MySQL              |
                 |    *                         |
                 |                              |     PostgreSQL
                 |                              |     *
                 +------------------------->   +------------------------->
                      Flexibility                   Strictness

设计理念对比：
- Oracle/MySQL方式：默认追求速度，相信DBA，为性能放松约束
- PostgreSQL方式：默认追求正确性，防止错误，始终强制约束

PostgreSQL宁可稍慢，也不允许静默的数据损坏或不一致。
这是一个有意的设计选择，而非能力不足。
```

**Example: Data type coercion**

MySQL:
```sql
INSERT INTO users (age) VALUES ('not a number');  -- Silently becomes 0
```

PostgreSQL:
```sql
INSERT INTO users (age) VALUES ('not a number');
-- ERROR: invalid input syntax for type integer
```

PostgreSQL refuses to silently corrupt your data. This is a deliberate choice.

### MVCC Instead of Locking

```
Traditional Locking:
+----------+     +----------+
| Writer   |     | Reader   |
+----+-----+     +----+-----+
     |                |
     v                v
+----+----------------+-----+
|    BLOCKED until          |
|    writer commits         |
+---------------------------+

传统锁定方式：写操作会阻塞读操作，直到写事务提交。

MVCC (Multi-Version Concurrency Control):
+----------+     +----------+
| Writer   |     | Reader   |
+----+-----+     +----+-----+
     |                |
     v                v
+----------+     +----------+
| New      |     | Old      |
| Version  |     | Version  |
+----------+     +----------+
     |                |
     v                v
   Both proceed without blocking

MVCC方式：写操作创建新版本，读操作看到旧版本，两者互不阻塞。

PostgreSQL使用MVCC，实现"读不阻塞写，写不阻塞读"。
这是PostgreSQL高并发性能的关键。
```

Most databases originally used locking: readers block writers, writers block
readers. PostgreSQL uses MVCC (Multi-Version Concurrency Control):

- Writers create new versions of rows, they do not modify in place
- Readers see a consistent snapshot, they are never blocked by writers
- "Readers don't block writers, writers don't block readers"

MVCC has costs (more storage for old versions, vacuum overhead) but the
concurrency benefits are substantial for most workloads.

### Extensibility as a First-Class Feature

```
PostgreSQL Extension Architecture:
+----------------------------------------------------------+
|                      User Space                          |
|  +--------------------+  +--------------------+          |
|  | PostGIS            |  | pg_trgm            |          |
|  | (Geometry types)   |  | (Text search)      |          |
|  +--------------------+  +--------------------+          |
|  +--------------------+  +--------------------+          |
|  | hstore             |  | Custom Extension   |          |
|  | (Key-value)        |  | (Your code here)   |          |
|  +--------------------+  +--------------------+          |
+----------------------------------------------------------+
|                   Extension API                          |
|  +----------+ +----------+ +----------+ +----------+     |
|  | Types    | | Operators| | Functions| | Indexes  |     |
|  +----------+ +----------+ +----------+ +----------+     |
+----------------------------------------------------------+
|                   PostgreSQL Core                        |
+----------------------------------------------------------+

PostgreSQL扩展架构：
- 用户空间：PostGIS（地理信息）、pg_trgm（文本搜索）、hstore（键值存储）等扩展
- 扩展API：提供类型、运算符、函数、索引等扩展接口
- PostgreSQL核心：底层数据库引擎

这种设计使PostgreSQL成为一个"数据库平台"而非仅仅是一个数据库。
```

PostgreSQL provides stable APIs for extending:

- **Data types**: `CREATE TYPE` + C functions
- **Operators**: `CREATE OPERATOR`
- **Index types**: `CREATE ACCESS METHOD`
- **Functions**: `CREATE FUNCTION` in SQL, C, Python, Perl, etc.
- **Foreign data wrappers**: Query external data sources as tables

This is why PostgreSQL has world-class support for:
- GIS (PostGIS)
- Full-text search (built-in + extensions)
- JSON (built-in)
- Time-series (TimescaleDB)
- Graph queries (Apache AGE)

### SQL Standards Compliance

PostgreSQL aims for SQL standard compliance where practical:

```
SQL Feature Support Comparison:
+------------------------+-------+-------+------------+
| Feature                | MySQL | PG    | SQL Standard|
+------------------------+-------+-------+------------+
| Window Functions       | 8.0+  | 8.4+  | SQL:2003   |
| CTEs (WITH clause)     | 8.0+  | 8.4+  | SQL:1999   |
| LATERAL joins          | 8.0+  | 9.3+  | SQL:1999   |
| FULL OUTER JOIN        | Yes   | Yes   | SQL-92     |
| CHECK constraints      | 8.0+* | Yes   | SQL-92     |
| Deferrable constraints | No    | Yes   | SQL-92     |
| EXCEPT/INTERSECT       | Yes   | Yes   | SQL-92     |
| Schemas                | No**  | Yes   | SQL-92     |
+------------------------+-------+-------+------------+
* MySQL CHECK constraints were ignored until 8.0.16
** MySQL uses "databases" differently

SQL功能支持对比：
- 窗口函数、CTE、LATERAL连接等现代SQL特性
- CHECK约束、可延迟约束等数据完整性特性
- EXCEPT/INTERSECT、Schema等标准特性

PostgreSQL在SQL标准兼容性方面通常领先于其他开源数据库。
这意味着你学习的SQL技能更具可移植性。
```

### What PostgreSQL Does NOT Optimize For

PostgreSQL makes deliberate tradeoffs. It is NOT optimized for:

1. **Extreme write throughput**: MVCC and WAL add overhead per write
2. **Embedded use**: PostgreSQL is a full server, not an embedded library
3. **Schemaless data**: While it has JSONB, relational is its strength
4. **Simple key-value lookups**: Redis will beat PostgreSQL for pure KV
5. **Analytical scans**: Column stores (ClickHouse) are faster for OLAP

```
                    PostgreSQL Sweet Spot

                         Write-Heavy
                              ^
                              |
              Key-Value   *---|----*   Time-Series
              Stores          |        Databases
                              |
    <-------------------------+------------------------->
         Simple Queries       |         Complex Queries
                              |
              Document    *---|----*   Analytical
              Stores          |        Databases
                              |
                              v
                         Read-Heavy

PostgreSQL最佳使用场景：
- 位于图中心偏右上方
- 适合：复杂查询、混合读写、事务处理、关系数据
- 不适合：纯键值存储、海量分析扫描、嵌入式场景

选择数据库时，理解这些边界很重要。PostgreSQL是通用型数据库中最强大的选择之一，
但不是所有场景的最优解。
```

PostgreSQL excels at:
- Complex queries with joins and aggregations
- Mixed read-write OLTP workloads
- Data integrity requirements
- Semi-structured data (JSONB) with relational
- Geospatial (PostGIS)
- Full-text search

---

## Summary

```
+----------------------------------------------------------+
|              Why PostgreSQL Exists                        |
+----------------------------------------------------------+
|                                                          |
|  Historical:                                             |
|  - Born from INGRES research (UC Berkeley)               |
|  - Designed to address relational model limitations      |
|  - 30+ years of production hardening                     |
|                                                          |
|  Design Philosophy:                                      |
|  - Correctness over raw speed                            |
|  - MVCC for concurrency (readers never blocked)          |
|  - Extensibility built into core architecture            |
|  - SQL standards compliance                              |
|                                                          |
|  Trade-offs:                                             |
|  - Not the fastest for simple operations                 |
|  - Not embedded (full server model)                      |
|  - MVCC has vacuum overhead                              |
|                                                          |
+----------------------------------------------------------+

PostgreSQL存在的原因总结：

历史背景：
- 源于UC Berkeley的INGRES研究项目
- 设计初衷是解决关系模型的局限性
- 经过30多年的生产环境打磨

设计理念：
- 正确性优先于原始速度
- 使用MVCC实现高并发（读操作永不阻塞）
- 可扩展性是核心架构的一部分
- 遵循SQL标准

设计权衡：
- 简单操作不是最快的
- 不是嵌入式数据库（完整的服务器模型）
- MVCC带来vacuum开销

理解这些设计决策有助于正确使用PostgreSQL，并知道何时应该选择其他工具。
```

PostgreSQL exists because its designers believed:
1. Extensibility should be fundamental, not an afterthought
2. Correctness matters more than benchmark wins
3. SQL standards compliance reduces vendor lock-in
4. MVCC provides better real-world concurrency than locking

In the next section, we examine PostgreSQL's high-level architecture and how
these design decisions manifest in its structure.
