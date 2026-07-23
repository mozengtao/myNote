# SQL —— 声明（Declaration）

> **核心驱动力：描述"要什么"，而不是"怎么做"。**
> SQL 程序员不问"该怎么一行行遍历数据来得到结果"，而问"最终这个结果集应该长什么样"。

---

## 心智模型图解

```
Table（原始数据集合）
     │
     ▼
Selection（筛选满足条件的行）
     │
     ▼
Projection（选择需要的列/聚合）
     │
     ▼
Result Set（目标结果集）
```

SQL 是声明式语言：你只描述"目标集合的样子"（从哪些表来、要哪些条件、要哪些列），
具体"怎么扫描、走不走索引、先过滤哪张表"这些执行细节，全部交给数据库的查询优化器决定。

---

## 核心驱动力详解

- **描述目标，而非过程**：`SELECT` 语句本身没有"循环"，你写的是"结果集的定义"，不是"取数据的步骤"。
- **一切都是集合运算**：`WHERE` 是筛选、`JOIN` 是集合的组合、`GROUP BY` 是分组归约、`UNION` 是集合并集。
- **顺序在逻辑上不代表执行顺序**：`SELECT ... FROM ... WHERE ... GROUP BY ... HAVING ...` 的书写顺序，
  和数据库实际执行的顺序（`FROM → WHERE → GROUP BY → HAVING → SELECT → ORDER BY`）并不一致。
- **约束和视图，也是"声明"的延伸**：你声明"这份数据必须满足什么规则"，而不是写代码去检查它。

---

## 典型代码片段

### 1. `SELECT ... WHERE` —— 声明式的筛选

```sql
SELECT name, price
FROM products
WHERE category = 'electronics' AND price > 100;
```

**心智模型解读**：这条语句没有告诉数据库"怎么去扫描表"，只是声明了
"我要的结果集是：products 表里，category 是 electronics 且 price 大于 100 的那些行的 name 和 price 两列"。

### 2. `JOIN` —— 声明式地组合两个集合

```sql
SELECT o.id AS order_id, c.name AS customer_name
FROM orders AS o
JOIN customers AS c ON o.customer_id = c.id
WHERE o.status = 'shipped';
```

**心智模型解读**：`JOIN ... ON` 声明的是"两个集合之间的对应关系"，
不是"先遍历 orders，再对每一行去 customers 里查找"这种过程式描述——具体怎么关联（哈希连接/嵌套循环）由优化器决定。

### 3. `GROUP BY` + 聚合函数 —— 声明"如何分组、如何归约"

```sql
SELECT customer_id, COUNT(*) AS order_count, SUM(amount) AS total_spent
FROM orders
GROUP BY customer_id
HAVING SUM(amount) > 1000;
```

**心智模型解读**：这条语句声明的是"目标结果集"的形状：按 `customer_id` 分组，
每组算出订单数和总金额，只保留总金额超过 1000 的分组——`HAVING` 是对"分组后的结果"再做一次筛选。

### 4. 子查询 —— 用嵌套集合定义"筛选条件本身"

```sql
SELECT name
FROM products
WHERE price > (SELECT AVG(price) FROM products);
```

**心智模型解读**：括号里的子查询本身也是一个"声明式的集合定义"（平均价格），
外层查询用这个值作为筛选条件——子查询让"用另一个集合的计算结果来定义当前集合"变得自然。

### 5. CTE（`WITH` 子句）—— 把复杂声明拆成可读的"命名步骤"

```sql
WITH monthly_totals AS (
    SELECT customer_id, DATE_TRUNC('month', order_date) AS month, SUM(amount) AS total
    FROM orders
    GROUP BY customer_id, DATE_TRUNC('month', order_date)
)
SELECT customer_id, month, total
FROM monthly_totals
WHERE total > 500;
```

**心智模型解读**：`WITH` 并不引入"过程步骤"，`monthly_totals` 仍然只是一个被命名的临时结果集声明，
它让复杂查询可以像"先定义一个中间集合，再基于它定义最终集合"这样分层书写，而不是嵌套一堆子查询。

### 6. 窗口函数 —— 在不折叠行的前提下声明"跨行的计算"

```sql
SELECT
    name,
    department,
    salary,
    RANK() OVER (PARTITION BY department ORDER BY salary DESC) AS dept_rank
FROM employees;
```

**心智模型解读**：`OVER (PARTITION BY ... ORDER BY ...)` 声明了"在每个部门内部，按薪水排名"，
但不像 `GROUP BY` 那样把多行折叠成一行——它是在保留原始行的前提下，声明每行相对于所在"窗口"的位置。

### 7. `UNION` / `INTERSECT` —— 直接用集合运算符声明结果

```sql
SELECT email FROM newsletter_subscribers
UNION
SELECT email FROM customers
WHERE last_order_date > '2026-01-01';
```

**心智模型解读**：`UNION` 直接对应数学上的"并集"概念，声明的是"两个结果集合并去重后的集合"，
完全不需要描述"怎么把两份数据拼起来、怎么去重"这些执行细节。

### 8. 声明式约束 —— `CHECK`、`FOREIGN KEY`，把规则交给数据库而不是应用代码

```sql
CREATE TABLE orders (
    id SERIAL PRIMARY KEY,
    customer_id INT NOT NULL REFERENCES customers(id),
    amount NUMERIC NOT NULL CHECK (amount > 0),
    status TEXT NOT NULL CHECK (status IN ('pending', 'shipped', 'cancelled'))
);
```

**心智模型解读**：`CHECK`/`REFERENCES` 声明的是"这份数据必须永远满足的规则"，
不管数据是通过哪个应用、哪条 SQL 插入的，数据库都会强制校验——这是"声明规则"而不是"写校验代码"的思维。

### 9. 视图（`VIEW`）—— 把一段声明本身，封装成一个"虚拟表"

```sql
CREATE VIEW high_value_customers AS
SELECT customer_id, SUM(amount) AS total_spent
FROM orders
GROUP BY customer_id
HAVING SUM(amount) > 10000;

SELECT * FROM high_value_customers WHERE total_spent > 50000;
```

**心智模型解读**：视图本身不存储数据，它只是给一段"结果集声明"起了个名字，
之后可以像查询普通表一样直接引用它——这是"声明可以被复用、被组合"的体现。

### 10. `CASE WHEN` —— 声明式的分支表达式

```sql
SELECT
    name,
    price,
    CASE
        WHEN price < 50 THEN 'cheap'
        WHEN price < 200 THEN 'mid-range'
        ELSE 'premium'
    END AS price_tier
FROM products;
```

**心智模型解读**：`CASE WHEN` 不是"if/else 语句"，而是一个表达式，
它声明的是"这一列的值该如何根据条件映射成另一个值"——本质上和 `SELECT` 整体的思维完全一致：描述值，而非描述步骤。

### 11. 逻辑执行顺序 vs 书写顺序 —— 理解声明式语言的"非过程性"

```sql
SELECT department, AVG(salary) AS avg_salary
FROM employees
WHERE hire_date > '2020-01-01'
GROUP BY department
HAVING AVG(salary) > 8000
ORDER BY avg_salary DESC;

-- 书写顺序：SELECT → FROM → WHERE → GROUP BY → HAVING → ORDER BY
-- 逻辑执行顺序：FROM → WHERE → GROUP BY → HAVING → SELECT → ORDER BY
```

**心智模型解读**：`WHERE` 里不能直接用 `SELECT` 里定义的聚合别名（比如 `avg_salary`），
正是因为逻辑上 `WHERE` 在 `SELECT` 之前生效——理解这个"逻辑执行顺序"，才能解释很多"为什么这样写不行"的报错。

### 12. `EXISTS` —— 声明"是否存在满足条件的关联行"，而非取出具体数据

```sql
SELECT c.name
FROM customers AS c
WHERE EXISTS (
    SELECT 1 FROM orders AS o
    WHERE o.customer_id = c.id AND o.amount > 1000
);
```

**心智模型解读**：`EXISTS` 声明的是一个布尔条件——"是否存在这样的行"，
它不关心子查询具体返回了什么值（哪怕写 `SELECT 1`），这进一步说明 SQL 里"声明意图"比"具体取值步骤"更重要。

---

## 黄金法则

> **不要想着遍历数据，而要想着描述目标集合。**

写一条 SQL 前，先问自己："我最终想要的结果集，长什么样？由哪些表、
经过哪些筛选/分组/聚合规则组成？"而不是想"该怎么一行一行处理数据"。

---

## 常见误区对比

### 误区一：用应用代码里的"循环思维"硬套 SQL，写出低效的逐行查询

```sql
-- 错误心智模型：把 SQL 当"取数据的入口"，剩下的逻辑放到应用层里用循环处理
-- （伪代码，展示误区）
-- for each customer in SELECT * FROM customers:
--     SELECT SUM(amount) FROM orders WHERE customer_id = customer.id
--     -- 对每个客户单独发一次查询（N+1 查询问题）
```

```sql
-- SQL 习惯写法：一次声明就拿到完整结果集
SELECT c.id, c.name, COALESCE(SUM(o.amount), 0) AS total_spent
FROM customers AS c
LEFT JOIN orders AS o ON o.customer_id = c.id
GROUP BY c.id, c.name;
```

**为什么后者更好**：一条声明式查询让数据库一次性完成"关联+分组+聚合"，
避免了应用层用循环反复发起查询（N+1 问题），执行效率和优化空间都远优于逐行处理。

### 误区二：在 `WHERE` 里试图直接引用聚合结果的别名

```sql
-- 错误心智模型：以为 WHERE 里能直接用 SELECT 里定义的聚合别名
SELECT department, AVG(salary) AS avg_salary
FROM employees
WHERE avg_salary > 8000  -- 报错：WHERE 阶段还没有 avg_salary 这一列
GROUP BY department;
```

```sql
-- SQL 习惯写法：对"分组后"的聚合结果做筛选，要用 HAVING
SELECT department, AVG(salary) AS avg_salary
FROM employees
GROUP BY department
HAVING AVG(salary) > 8000;
```

**为什么后者更好**：`WHERE` 在逻辑上发生于 `GROUP BY`/聚合计算之前，此时聚合列还不存在；
`HAVING` 专门用于"对分组后的聚合结果再筛选"，这正是理解 SQL 逻辑执行顺序后自然得出的正确写法。

---

## 快速上手 Checklist

- [ ] 写一条查询前，能先用一句话描述"最终结果集应该长什么样"吗？
- [ ] 能说出 SQL 语句的逻辑执行顺序（`FROM → WHERE → GROUP BY → HAVING → SELECT → ORDER BY`）吗？
- [ ] 遇到"要不要在应用层写循环查询"的场景，能判断出这是否是一个可以用 `JOIN`/子查询一次性声明的问题吗？
- [ ] 能分清 `WHERE`（筛选原始行）和 `HAVING`（筛选分组后的聚合结果）的适用场景吗？
- [ ] 知道视图、CTE 的作用是"给一段声明命名以便复用"，而不是"缓存了计算结果"（除非是物化视图）吗？

---

上一篇：[Haskell —— 变换](haskell.md) ・ 返回：[README（索引）](README.md)
