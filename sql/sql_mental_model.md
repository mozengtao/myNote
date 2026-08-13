# SQLite、MySQL、PostgreSQL 全面对比：架构、适用场景与心智模式

---

# 一句话理解

| 数据库 | 一句话定位 |
|---------|-----------|
| SQLite | **一个文件就是一个数据库（Embedded Database）** |
| MySQL | **面向互联网业务的高性能关系数据库（Web Database）** |
| PostgreSQL | **功能最完整、最标准、最像数据库操作系统（Enterprise Database）** |

可以把三者理解成：

```
                Relational Database

                     │
     ┌───────────────┼────────────────┐
     │               │                │
 SQLite          MySQL              PostgreSQL
 单机文件         Web业务             企业平台
 Embedded        High Throughput    Feature Rich
```

---

# 三者最大的区别（心智模型）

其实只有一句：

> **SQLite 是一个库（Library）**
>
> **MySQL / PostgreSQL 是一个服务（Server）**

即：

```
SQLite

你的程序
     │
 sqlite3 library
     │
database.db（文件）


-----------------------------------

MySQL/PostgreSQL

你的程序
      │ TCP
      ▼
 Database Server
      │
 数据文件
```

因此：

SQLite 不需要启动服务。

MySQL/PostgreSQL 必须启动数据库进程。

这是最大的设计区别。

---

# 一、SQLite

## 它是什么？

SQLite 是一个：

> **嵌入式（Embedded）数据库。**

没有 Server。

整个数据库：

就是一个文件。

例如：

```
notes.db
```

就是完整数据库。

程序直接：

```
Application
      │
 sqlite3 API
      │
notes.db
```

没有网络。

没有 daemon。

没有 client/server。

---

## 工作方式

例如 Python：

```python
import sqlite3

conn = sqlite3.connect("test.db")
```

实际上：

```
connect()

↓

打开 test.db 文件

↓

解析 B-tree

↓

直接读写页面
```

没有网络通信。

---

## 心智模式

> **SQLite = 一个超级智能的文件格式**

类似：

```
Word

打开 document.docx

↓

修改

↓

保存
```

SQLite：

```
打开 test.db

↓

SQL 查询

↓

保存
```

区别只是：

```
SQLite
=
支持 SQL 的文件
```

---

## 优点

非常轻：

```
不到 1MB
```

零配置：

```
无需安装

无需启动

无需账号
```

事务支持：

```
ACID
```

支持：

- SQL
- Index
- Trigger
- View

几乎都有。

---

## 缺点

最大问题：

并发。

它采用：

```
文件锁
```

例如：

```
多个 Reader

OK

一个 Writer

锁整个数据库
```

因此：

大量写：

性能下降。

---

## 最适合

例如：

浏览器：

```
Chrome

History

Cookie

Bookmark
```

全部 SQLite。

手机：

```
Android

iOS
```

几乎全部 SQLite。

还有：

- 配置
- 日志
- 小工具
- 本地缓存
- CLI

---

## SQLite 心智模式

```
SQLite

不是数据库服务器

而是

一个支持 SQL 的数据文件。
```

---

# 二、MySQL

## 它是什么？

MySQL：

真正的

Client / Server 数据库。

```
Application

      │ TCP

      ▼

 MySQL Server

      │

 Data Files
```

多个客户端：

```
Python

Go

Java

PHP

Node

都连接

3306
```

共享一个数据库。

---

## 工作流程

例如：

```
SELECT *
FROM user;
```

流程：

```
Client

↓

TCP

↓

Parser

↓

Optimizer

↓

Executor

↓

Buffer Pool

↓

Disk
```

数据库：

真正是一台服务器。

---

## MySQL 的设计目标

互联网。

例如：

```
淘宝

京东

论坛

CMS

博客

ERP
```

大量：

```
CRUD
```

特点：

```
简单

快

稳定
```

---

## MySQL 最强优势

读写性能。

尤其：

```
InnoDB
```

Buffer Pool：

```
Memory

↓

热点数据

↓

减少 IO
```

对于：

```
Web

增删改查
```

性能极高。

---

## MySQL 特点

支持：

- ACID
- Transaction
- MVCC
- Replication
- Cluster

---

## MySQL 心智模式

> **MySQL = 一个高性能数据服务（Data Service）**

类似：

```
Nginx

负责 HTTP

MySQL

负责 SQL
```

---

# 三、PostgreSQL

## 它是什么？

PostgreSQL：

也是

Client / Server。

但是：

它的目标：

不是最快。

而是：

**最完整。**

---

## PostgreSQL 的设计理念

如果 MySQL：

追求：

```
快
```

PostgreSQL：

追求：

```
Everything
```

支持：

几乎所有 SQL 标准。

例如：

```
Window Function

CTE

Recursive Query

JSON

Array

GIS

Full Text

Custom Type

Stored Procedure

Extension
```

几乎都有。

---

## PostgreSQL 最大特点

不仅存：

```
Table
```

还能存：

```
JSON

XML

Array

Range

UUID

Network

Geometry
```

例如：

```sql
CREATE TABLE user (

    phones text[],

    location point,

    profile jsonb

);
```

一个字段：

直接：

JSON。

---

## PostgreSQL 更像什么？

很多人说：

> PostgreSQL 更像数据库操作系统。

因为：

它支持：

```
Extension

Plugin

Function

Language

Type
```

例如：

安装：

```
PostGIS
```

立刻支持：

地图。

安装：

```
TimescaleDB
```

立刻支持：

时序数据库。

---

## PostgreSQL 的能力

例如：

一个字段：

```
JSONB
```

可以：

```sql
SELECT *

FROM user

WHERE profile->>'city'='Toronto';
```

直接查询 JSON。

MySQL 后来才逐渐补充类似能力。

---

## PostgreSQL 心智模式

> **PostgreSQL = 数据平台（Data Platform）**

不是：

简单数据库。

而是：

可以扩展的数据处理平台。

---

# 三者能力比较

| 能力 | SQLite | MySQL | PostgreSQL |
|-------|---------|--------|------------|
| Server | ❌ | ✅ | ✅ |
| 文件数据库 | ✅ | ❌ | ❌ |
| 安装简单 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐ |
| 高并发 | ⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| SQL 标准 | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| JSON | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| GIS | 很弱 | 一般 | ⭐⭐⭐⭐⭐ |
| 扩展能力 | 很弱 | 一般 | ⭐⭐⭐⭐⭐ |
| Web | ⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| 企业 | ⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |

---

# 如何选择

## ① 单机应用

例如：

```
桌面软件

配置管理

浏览器

手机 App
```

选：

```
SQLite
```

原因：

```
无需安装

一个文件
```

---

## ② 网站

例如：

```
论坛

博客

ERP

CRM

商城
```

选：

```
MySQL
```

原因：

```
性能高

生态成熟
```

---

## ③ 企业系统

例如：

```
金融

GIS

数据分析

复杂业务

BI
```

选：

```
PostgreSQL
```

原因：

```
SQL 强

扩展丰富

标准完善
```

---

# 三者的架构心智模式

```
                 数据库

                    │
     ┌──────────────┼──────────────┐
     │              │              │
 SQLite         MySQL         PostgreSQL

 文件           服务            平台

 Embedded     Data Server    Data Platform
```

---

# 更高层的统一心智模式（80/20）

## SQLite —— "数据库就是文件"

```
Database

↓

File

↓

Library

↓

Direct Access
```

关键词：

> **数据库 ≈ 文件**

适用于：

> 单机、本地存储、嵌入式。

---

## MySQL —— "数据库就是服务"

```
Database

↓

Server

↓

Many Clients

↓

CRUD

↓

High Throughput
```

关键词：

> **数据库 ≈ 网络服务**

适用于：

> Web 应用、互联网业务、高并发读写。

---

## PostgreSQL —— "数据库就是数据平台"

```
Database

↓

Platform

↓

Extension

↓

Complex Query

↓

Rich Data Types

↓

Analytics
```

关键词：

> **数据库 ≈ 可扩展的数据处理平台**

适用于：

> 企业系统、复杂业务、分析型应用、需要丰富数据类型和高级 SQL 能力的场景。

---

# 最终总结（牢记三句话）

> **SQLite：数据库就是一个文件。**

> **MySQL：数据库就是一个高性能的数据服务。**

> **PostgreSQL：数据库就是一个可扩展的数据平台。**

理解这三种心智模式后，再面对不同项目时，可以快速判断：

- **只需要本地存储？** → SQLite
- **典型 Web/互联网 CRUD？** → MySQL
- **复杂业务、丰富 SQL、分析和扩展？** → PostgreSQL