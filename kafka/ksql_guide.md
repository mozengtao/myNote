# ksqlDB 核心概念、工作原理、心智模型与 CLI 使用指南

> 目标：
>
> 学完本文后，你应该建立如下心智模型：
>
> > Kafka = 数据仓库（Data Lake）
> >
> > Producer = 写数据
> >
> > Consumer = 读数据
> >
> > Kafka Streams = Java API 做流处理
> >
> > **ksqlDB = 用 SQL 做 Kafka Streams**

---

# 一、一句话理解 ksqlDB

> **ksqlDB 就是 Kafka 的 Streaming SQL Engine。**

它允许使用 SQL 去完成：

- 数据过滤
- 字段提取
- Join
- 聚合
- Window
- ETL(Extract -> Transform -> Load)
- Streaming Pipeline

而无需写 Java Kafka Streams 程序。

所以：

```
Kafka Streams
        │
        ▼
Java API

=====================

ksqlDB
        │
        ▼
SQL
```

可以认为：

```
SQL
 ↓
ksqlDB
 ↓
Kafka Streams
 ↓
Kafka Consumer + Producer
 ↓
Kafka Cluster
```

所以：

> **ksqlDB 本质就是 Kafka Streams 的 SQL 封装。**

---

# 二、为什么需要 ksqlDB

没有 ksqlDB：

```
Producer
    │
    ▼
Kafka Topic
    │
    ▼
Java Consumer
    │
业务代码
    │
过滤
转换
聚合
Join
Window
    │
    ▼
Producer
    │
    ▼
New Topic
```

每一个 ETL：

需要：

- Java
- Kafka Streams
- 部署
- 打包

十分繁琐。

---

有了 ksqlDB：

```
Producer
     │
     ▼
orders
     │
     ▼
CREATE STREAM ...
SELECT ...
EMIT CHANGES;
     │
     ▼
new_orders
```

全部 SQL 完成。

---

# 三、整体架构

```
                    +----------------+
                    |   ksql CLI     |
                    +-------+--------+
                            |
                            |
                            v
                 +----------------------+
                 |    ksqlDB Server     |
                 |                      |
                 | SQL Parser           |
                 | Optimizer            |
                 | Query Engine         |
                 +----------+-----------+
                            |
                            |
                    Kafka Streams API
                            |
                            ▼
                  +-------------------+
                  | Kafka Cluster     |
                  +-------------------+
```

所以：

CLI 根本不处理数据。

真正处理数据的是：

```
ksqlDB Server
```

---

# 四、核心对象

整个 ksqlDB 只有几个重要对象。

```
Topic
 │
 ├───────────────┐
 │               │
 ▼               ▼
Stream         Table
```

---

## Topic

Kafka 永远只有 Topic。

例如：

```
orders
payments
users
```

Topic 不知道：

这是：

- stream
- table

这些都是：

ksqlDB 赋予 Topic 的逻辑含义。

---

# 五、Stream

最重要概念。

Stream 表示：

> **不断追加的新数据。**

例如：

```
订单

order1

order2

order3

order4
```

永远 append。

```
offset

0

1

2

3

4

5

...
```

可以理解：

```
append-only log
```

所以：

Stream == Log

---

例如：

```
CREATE STREAM orders (
    id INT,
    user STRING,
    amount DOUBLE
)
WITH (...);
```

对应：

```
Kafka Topic

orders
```

里面不断追加：

```
offset=1

{id:1}

offset=2

{id:2}

offset=3

{id:3}
```

---

心智模型：

```
Stream

≈

tail -f logfile
```

永远往后读。

---

# 六、Table

Table 表示：

> 最新状态。

例如：

```
User

ID    Name

1     Tom

2     Jack
```

后来：

```
ID=1

Tom

↓

Tommy
```

Table 保存：

```
1 -> Tommy

2 -> Jack
```

不是所有历史。

而是：

当前值。

---

所以：

```
Stream

保存：

所有事件

=================

Table

保存：

最终状态
```

---

心智模型：

```
Stream

Git Commit History

===================

Table

Git HEAD
```

---

# 七、Stream vs Table

```
                 Stream

Order Created

Order Paid

Order Shipped

Order Finished

========================

Table

OrderID

Current Status
```

所以：

```
事件流

↓

Table

状态
```

---

# 八、Push Query

默认：

```
SELECT *

FROM orders

EMIT CHANGES;
```

不会结束。

它一直输出。

```
client

|

receive

receive

receive

receive
```

就像：

```
tail -f
```

---

# 九、Pull Query

Table 可以：

```
SELECT *

FROM users

WHERE id=100;
```

立即返回。

```
100

Tom
```

所以：

```
Stream

Push

==================

Table

Pull
```

---

# 十、Persistent Query

例如：

```
CREATE STREAM vip_orders AS

SELECT *

FROM orders

WHERE amount>1000;
```

这个 Query：

不会退出。

一直运行。

```
orders

↓

filter

↓

vip_orders
```

像一个：

后台服务。

---

# 十一、Transient Query

例如：

```
SELECT *

FROM orders

EMIT CHANGES;
```

只是：

查看。

退出后：

结束。

不会创建 Topic。

---

# 十二、Window

流没有结束。

所以：

聚合必须：

Window。

例如：

```
5分钟统计订单数量
```

```
Window

10:00-10:05

↓

count=12

------------------

10:05-10:10

↓

count=20
```

否则：

永远统计不完。

---

# 十三、Join

支持：

```
Stream-Stream

Stream-Table

Table-Table
```

例如：

```
orders

JOIN

users
```

得到：

```
订单

+

用户名
```

---

# 十四、工作原理

例如：

```
SELECT *

FROM orders

WHERE amount>100;
```

整个流程：

```
CLI

↓

发送 SQL

↓

Server Parser

↓

生成 Logical Plan

↓

Kafka Streams Topology

↓

Consumer

↓

Filter

↓

Producer

↓

Result Topic
```

本质：

```
SQL

↓

Kafka Streams DSL

↓

Kafka Consumer

↓

Kafka Producer
```

---

# 十五、内部执行流程

```
             SQL

              │

              ▼

      SQL Parser

              │

              ▼

      Logical Plan

              │

              ▼

     Physical Plan

              │

              ▼

 Kafka Streams Topology

              │

              ▼

Consumer -> Processor -> Producer

              │

              ▼

Kafka Topic
```

---

# 十六、生命周期

例如：

```
CREATE STREAM ...

↓

Create Metadata

↓

Build Streams Topology

↓

Start Consumer

↓

Consume Topic

↓

Process Record

↓

Write Output Topic

↓

Checkpoint Offset
```

循环：

```
poll()

↓

process()

↓

produce()

↓

commit()
```

直到停止。

---

# 十七、CLI 常用命令

CLI 可以理解成：

```
SQL Shell
```

作用：

- 发送 SQL
- 查看 Metadata
- 查看 Query
- 管理 Stream/Table

并不负责：

数据处理。

---

## 1）连接 Server

```sql
ksql http://localhost:8088
```

连接：

```
CLI

↓

Server
```

---

## 2）SHOW TOPICS

```sql
SHOW TOPICS;
```

查看：

Kafka Topic。

输出：

```
orders

payments

users
```

---

## 3）SHOW STREAMS

```sql
SHOW STREAMS;
```

查看：

所有 Stream。

---

## 4）SHOW TABLES

```sql
SHOW TABLES;
```

查看：

Table。

---

## 5）SHOW QUERIES

```sql
SHOW QUERIES;
```

查看：

后台运行 Query。

例如：

```
CSAS

CTAS

INSERT INTO
```

---

## 6）DESCRIBE

```sql
DESCRIBE orders;
```

查看：

```
Schema

Topic

Serde

Partition

Key
```

---

## 7）DESCRIBE EXTENDED

```sql
DESCRIBE EXTENDED orders;
```

查看更多：

- Query
- Statistics
- Source Topic
- Kafka Topic
- Consumer Group

---

## 8）PRINT

```sql
PRINT 'orders';
```

直接查看：

Kafka Topic。

类似：

```
kafka-console-consumer
```

可以：

```
PRINT 'orders'
FROM BEGINNING;
```

查看历史。

---

## 9）LIST PROPERTIES

```sql
LIST PROPERTIES;
```

查看：

Server 配置。

---

## 10）SET

```sql
SET 'auto.offset.reset'='earliest';
```

修改 Session 配置。

---

## 11）UNSET

```sql
UNSET 'auto.offset.reset';
```

恢复默认。

---

## 12）EXPLAIN

```sql
EXPLAIN
SELECT * FROM orders;
```

查看：

执行计划。

包括：

```
Logical Plan

Physical Plan

Topology
```

非常重要。

---

## 13）TERMINATE

```sql
TERMINATE <query_id>;
```

停止：

Persistent Query。

---

## 14）DROP STREAM

```sql
DROP STREAM orders;
```

删除：

Metadata。

默认：

Topic 不删除。

---

## 15）DROP TABLE

```sql
DROP TABLE users;
```

删除：

Table。

---

# 十八、CLI 使用流程

```
连接

↓

SHOW STREAMS

↓

DESCRIBE

↓

PRINT

↓

SELECT ...

↓

CREATE STREAM AS SELECT

↓

SHOW QUERIES

↓

TERMINATE

↓

DROP
```

---

# 十九、完整数据流示例

假设：

Producer 写：

```
orders
```

数据：

```
{id:1,amount:10}

{id:2,amount:300}

{id:3,amount:500}
```

SQL：

```sql
CREATE STREAM vip_orders AS

SELECT *

FROM orders

WHERE amount>100;
```

数据流：

```
Producer

        │

        ▼

Kafka Topic

orders

        │

        ▼

Consumer

        │

        ▼

Filter(amount>100)

        │

        ▼

Producer

        │

        ▼

vip_orders
```

对应：

Kafka Streams：

```
builder

↓

stream("orders")

↓

filter()

↓

to("vip_orders")
```

---

# 二十、整体心智模型（建议牢记）

```
                    Kafka

                     │

        Topic（数据存储）

                     │

        +------------+-------------+

        │                          │

     Stream                    Table

    （事件）                  （状态）

        │                          │

        └------------+-------------+

                     │

             SQL Query

                     │

      Parser → Logical Plan

                     │

             Kafka Streams

                     │

Consumer → Process → Producer

                     │

              New Topic
```

---

# 二十一、核心心智模型总结

## 1. 分层架构模型

```
SQL（声明"做什么"）
        │
        ▼
ksqlDB（生成流处理拓扑）
        │
        ▼
Kafka Streams（执行流处理）
        │
        ▼
Kafka Consumer / Producer
        │
        ▼
Kafka Topics（持久化日志）
```

**记忆口诀：**

> **SQL → Topology → Streams → Topics**

---

## 2. Stream 与 Table

```
Stream = Event Log = append only
Table  = Latest State = Key → Value
```

可以类比：

| 世界 | Stream | Table |
|------|--------|--------|
| Git | Commit History | HEAD |
| 数据库 | Binlog | 当前表 |
| Linux | `tail -f logfile` | `cat current_state` |
| Kafka | Topic 中所有事件 | Topic 经 Key 聚合后的最新状态 |

---

## 3. ksqlDB 查询类型

```
SELECT ... EMIT CHANGES
        │
        ├── Transient Query（查看结果，不持久化）
        └── Persistent Query（持续运行，构建新 Topic）
```

Persistent Query 更像一个**长期运行的数据管道（Pipeline）**。

---

## 4. 数据流模型

```
Topic
   │
Consumer
   │
Process（Filter / Map / Join / Aggregate）
   │
Producer
   │
Topic
```

可以不断串联形成：

```
Topic A
   │
   ▼
Filter
   │
Topic B
   │
Join
   │
Topic C
   │
Aggregate
   │
Topic D
```

整个 Kafka 集群因此演化为一个**由 Topic 和流处理节点组成的数据流图（Dataflow Graph）**。

---

## 5. CLI 心智模型

```
CLI ≈ SQL Shell ≈ mysql client ≈ psql
```

职责只有三类：

- **管理元数据**：`SHOW`、`DESCRIBE`
- **执行 SQL**：`SELECT`、`CREATE STREAM AS SELECT`
- **管理运行任务**：`SHOW QUERIES`、`TERMINATE`

**CLI 不参与数据处理，只负责与 ksqlDB Server 交互。**

---

## 6. 一句话总结

> **把 Kafka 看成持续追加的数据湖（Event Log），把 ksqlDB 看成运行在其上的实时 SQL 引擎。开发者只需声明"要什么数据"，ksqlDB 负责生成并持续运行对应的 Kafka Streams 拓扑，实现实时 ETL、聚合、Join 与分析。**