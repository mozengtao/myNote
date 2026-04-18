
- []()
- []()
- [SQL Basics Cheat Sheet](https://learnsql.com/blog/sql-basics-cheat-sheet)
- [SQLZoo](https://sqlzoo.net/wiki/SQL_Tutorial)
- [SQLBolt](https://sqlbolt.com/)
- [mysql必知必会](https://github.com/oxidation99/MyBooks-1/tree/master)
- [MySQL 8.4 Reference Manual](https://dev.mysql.com/doc/refman/8.4/en/functions.html)

- [SELECT语句](./sql/select.md)

- [sql_mastery_index](./sql/sql_mastery_index.md)
- [sql_mastery_fundamentals](./sql/sql_mastery_fundamentals.md)
- [sql_mastery_joins](./sql/sql_mastery_joins.md)
- [sql_mastery_groupby](./sql/sql_mastery_groupby.md)
- [sql_mastery_debug_performance](./sql/sql_mastery_debug_performance.md)

- [PostgreSQL Documentation](https://www.postgresql.org/docs/current/)
- [PostgreSQL 内部原理图解](tps://www.interdb.jp/pg/)
- [Crunchy Data Postgres Playground](https://www.crunchydata.com/developers/playgrounds)
- [PostgreSQL Tutorial](https://neon.com/postgresql/tutorial)
- [PostgreSQL 教程](https://postgresql.mosong.cc/guide/)
- []()


A DATABASE is a collection of data stored in a format that can easily be accessed

DBMS: Database Management System

2 categoris of DBMS:

- Relational Databases(RDBMS)
    - MySQL
    - SQL Server
    - Oracle

    - SQL(Structured Query Language) used to query or modify data in RDBMS
- NoSQL


## WSL 下 Setup MySQL 环境
```bash
# 安装
sudo apt install mysql-server -y

# 启动服务
sudo service mysql start

# 设置 MySQL 登录用户 root 密码
sudo mysql

ALTER USER 'root'@'localhost' IDENTIFIED WITH mysql_native_password BY '你的密码，例如mysql';
FLUSH PRIVILEGES;
EXIT;

# 登录 MySQL
mysql -u root -p
(mysql)

# 简单验证
-- 1. 创建数据库
CREATE DATABASE test_db;

-- 2. 切换并创建表
USE test_db;
CREATE TABLE heartbeat (id INT AUTO_INCREMENT PRIMARY KEY, ts TIMESTAMP);

-- 3. 插入数据
INSERT INTO heartbeat (ts) VALUES (CURRENT_TIMESTAMP);

-- 4. 查询验证
SELECT * FROM heartbeat;

-- 5. 清理环境
DROP DATABASE test_db;
```

### 编写并运行 SQL 脚本
```bash
# 创建 SQL 脚本文件
-- setup.sql 内容
CREATE DATABASE IF NOT EXISTS my_project;
USE my_project;

CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO users (username) VALUES ('admin'), ('developer');
SELECT * FROM users;

# 运行 SQL 脚本文件
mysql -u root -p < setup.sql
```

