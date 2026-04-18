# 整体数据模型（先建立脑图）
```
customers ──< orders ──< order_items >── products
     │            │
     │            └── shippers
     │
     └── employees（自连接：manager）
     
users（JSON）
products（ARRAY tags）
```

# 建表 SQL（可直接执行）
```sql
-- ================================
-- 1. customers（客户表）
-- ================================
CREATE TABLE customers (
    customer_id SERIAL PRIMARY KEY,
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    email VARCHAR(100) UNIQUE,
    phone VARCHAR(20),
    state VARCHAR(50),
    city VARCHAR(50),
    points INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- ================================
-- 2. orders（订单表）
-- ================================
CREATE TABLE orders (
    order_id SERIAL PRIMARY KEY,
    customer_id INT REFERENCES customers(customer_id),
    order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    status VARCHAR(20), -- pending / paid / shipped
    total NUMERIC(10,2),
    shipper_id INT
);

-- ================================
-- 3. order_items（订单明细）
-- ================================
CREATE TABLE order_items (
    item_id SERIAL PRIMARY KEY,
    order_id INT REFERENCES orders(order_id),
    product_id INT,
    quantity INT,
    price NUMERIC(10,2)
);

-- ================================
-- 4. products（产品表，含 ARRAY）
-- ================================
CREATE TABLE products (
    product_id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    price NUMERIC(10,2),
    tags TEXT[] -- PostgreSQL 数组
);

-- ================================
-- 5. shippers（物流表）
-- ================================
CREATE TABLE shippers (
    shipper_id SERIAL PRIMARY KEY,
    name VARCHAR(100)
);

-- ================================
-- 6. employees（自连接：管理关系）
-- ================================
CREATE TABLE employees (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    manager_id INT REFERENCES employees(id)
);

-- ================================
-- 7. users（JSON 字段）
-- ================================
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    data JSONB
);
```

# 初始化测试数据（强烈建议执行）
```sql
-- customers
INSERT INTO customers (first_name, last_name, email, state, city, points)
VALUES
('John', 'Doe', 'john@example.com', 'CA', 'LA', 1200),
('Jane', 'Smith', 'jane@example.com', 'NY', 'NYC', 800),
('Alice', 'Brown', 'alice@example.com', 'CA', 'SF', 2000);

-- shippers
INSERT INTO shippers (name)
VALUES ('UPS'), ('FedEx');

-- products
INSERT INTO products (name, price, tags)
VALUES
('Laptop', 1200, ARRAY['tech', 'computer']),
('Phone', 800, ARRAY['tech', 'mobile']),
('Desk', 300, ARRAY['furniture']);

-- orders
INSERT INTO orders (customer_id, status, total, shipper_id)
VALUES
(1, 'paid', 1500, 1),
(1, 'pending', 500, 2),
(2, 'paid', 800, 1);

-- order_items
INSERT INTO order_items (order_id, product_id, quantity, price)
VALUES
(1, 1, 1, 1200),
(1, 2, 1, 300),
(2, 3, 2, 250),
(3, 2, 1, 800);

-- employees（自连接）
INSERT INTO employees (name, manager_id)
VALUES
('CEO', NULL),
('Manager1', 1),
('Employee1', 2);

-- users（JSON）
INSERT INTO users (data)
VALUES
('{"name": "Tom", "age": 30}'),
('{"name": "Jerry", "age": 25}');
```

# SELECT 测试语句
```sql
-- ================================
-- 1. 基础查询（Projection）
-- ================================

-- 1. 查询所有列
SELECT * FROM customers;

-- 2. 查询指定列
SELECT first_name, last_name FROM customers;

-- 3. 列重命名
SELECT first_name AS fname FROM customers;

-- 4. 表别名
SELECT c.first_name FROM customers c;

-- 5. 常量列
SELECT first_name, 1 AS flag FROM customers;

-- 6. 表达式计算
SELECT price * quantity AS total FROM order_items;

-- 7. 字符串拼接
SELECT first_name || ' ' || last_name AS full_name FROM customers;

-- 8. 去重
SELECT DISTINCT state FROM customers;

-- 9. 多列去重
SELECT DISTINCT city, state FROM customers;

-- 10. LIMIT
SELECT * FROM customers LIMIT 10;

-- 11. OFFSET
SELECT * FROM customers LIMIT 10 OFFSET 20;

-- 12. 排序 ASC
SELECT * FROM customers ORDER BY first_name ASC;

-- 13. 排序 DESC
SELECT * FROM customers ORDER BY points DESC;

-- 14. 多列排序
SELECT * FROM customers ORDER BY state, city;

-- 15. 按表达式排序
SELECT first_name, points FROM customers ORDER BY points * 2 DESC;

-- 16. NULL 排序
SELECT * FROM customers ORDER BY points NULLS LAST;

-- 17. CASE 表达式
SELECT first_name,
       CASE WHEN points > 1000 THEN 'VIP'
            ELSE 'NORMAL'
       END AS level
FROM customers;

-- ================================
-- 2. 过滤（WHERE）
-- ================================

-- 18. 基本过滤
SELECT * FROM customers WHERE points > 1000;

-- 19. AND
SELECT * FROM customers WHERE state = 'CA' AND points > 1000;

-- 20. OR
SELECT * FROM customers WHERE state = 'CA' OR state = 'NY';

-- 21. NOT
SELECT * FROM customers WHERE NOT state = 'CA';

-- 22. BETWEEN
SELECT * FROM customers WHERE points BETWEEN 1000 AND 3000;

-- 23. IN
SELECT * FROM customers WHERE state IN ('CA', 'NY');

-- 24. NOT IN
SELECT * FROM customers WHERE state NOT IN ('CA', 'NY');

-- 25. LIKE
SELECT * FROM customers WHERE first_name LIKE 'J%';

-- 26. 模糊匹配 _
SELECT * FROM customers WHERE first_name LIKE '_a%';

-- 27. ILIKE（大小写不敏感）
SELECT * FROM customers WHERE first_name ILIKE 'j%';

-- 28. IS NULL
SELECT * FROM customers WHERE phone IS NULL;

-- 29. IS NOT NULL
SELECT * FROM customers WHERE phone IS NOT NULL;

-- 30. 正则匹配（PostgreSQL）
SELECT * FROM customers WHERE first_name ~ '^J';

-- ================================
-- 3. JOIN（核心）
-- ================================

-- 31. INNER JOIN
SELECT o.order_id, c.first_name
FROM orders o
JOIN customers c ON o.customer_id = c.customer_id;

-- 32. LEFT JOIN
SELECT c.first_name, o.order_id
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id;

-- 33. RIGHT JOIN
SELECT c.first_name, o.order_id
FROM customers c
RIGHT JOIN orders o ON c.customer_id = o.customer_id;

-- 34. FULL JOIN
SELECT c.first_name, o.order_id
FROM customers c
FULL JOIN orders o ON c.customer_id = o.customer_id;

-- 35. 多表 JOIN
SELECT o.order_id, c.first_name, s.name
FROM orders o
JOIN customers c ON o.customer_id = c.customer_id
JOIN shippers s ON o.shipper_id = s.shipper_id;

-- 36. 自连接
SELECT e1.name, e2.name AS manager
FROM employees e1
JOIN employees e2 ON e1.manager_id = e2.id;

-- 37. CROSS JOIN
SELECT * FROM customers CROSS JOIN products;

-- ================================
-- 4. 聚合（GROUP BY）
-- ================================

-- 38. COUNT
SELECT COUNT(*) FROM orders;

-- 39. SUM
SELECT SUM(total) FROM orders;

-- 40. AVG
SELECT AVG(total) FROM orders;

-- 41. MAX
SELECT MAX(total) FROM orders;

-- 42. MIN
SELECT MIN(total) FROM orders;

-- 43. GROUP BY
SELECT customer_id, COUNT(*)
FROM orders
GROUP BY customer_id;

-- 44. 多列 GROUP BY
SELECT customer_id, status, COUNT(*)
FROM orders
GROUP BY customer_id, status;

-- 45. HAVING
SELECT customer_id, COUNT(*)
FROM orders
GROUP BY customer_id
HAVING COUNT(*) > 5;

-- 46. HAVING + SUM
SELECT customer_id, SUM(total)
FROM orders
GROUP BY customer_id
HAVING SUM(total) > 1000;

-- ================================
-- 5. 子查询（Subquery）
-- ================================

-- 47. 标量子查询
SELECT first_name,
       (SELECT AVG(points) FROM customers) AS avg_points
FROM customers;

-- 48. IN 子查询
SELECT * FROM orders
WHERE customer_id IN (SELECT customer_id FROM customers WHERE state = 'CA');

-- 49. EXISTS
SELECT * FROM customers c
WHERE EXISTS (SELECT 1 FROM orders o WHERE o.customer_id = c.customer_id);

-- 50. NOT EXISTS
SELECT * FROM customers c
WHERE NOT EXISTS (SELECT 1 FROM orders o WHERE o.customer_id = c.customer_id);

-- ================================
-- 6. 窗口函数（高级核心）
-- ================================

-- 51. ROW_NUMBER
SELECT *,
       ROW_NUMBER() OVER (ORDER BY total DESC) AS rn
FROM orders;

-- 52. RANK
SELECT *,
       RANK() OVER (ORDER BY total DESC) AS rnk
FROM orders;

-- 53. DENSE_RANK
SELECT *,
       DENSE_RANK() OVER (ORDER BY total DESC) AS dr
FROM orders;

-- 54. PARTITION BY
SELECT *,
       ROW_NUMBER() OVER (PARTITION BY customer_id ORDER BY total DESC) AS rn
FROM orders;

-- 55. 累计和
SELECT *,
       SUM(total) OVER (ORDER BY order_date) AS running_total
FROM orders;

-- 56. 滑动窗口
SELECT *,
       AVG(total) OVER (ORDER BY order_date ROWS BETWEEN 2 PRECEDING AND CURRENT ROW)
FROM orders;

-- ================================
-- 7. CTE（WITH）
-- ================================

-- 57. 基本 CTE
WITH high_value AS (
    SELECT * FROM orders WHERE total > 1000
)
SELECT * FROM high_value;

-- 58. 多个 CTE
WITH a AS (SELECT * FROM customers),
     b AS (SELECT * FROM orders)
SELECT * FROM a JOIN b ON a.customer_id = b.customer_id;

-- ================================
-- 8. 集合操作
-- ================================

-- 59. UNION
SELECT name FROM customers
UNION
SELECT name FROM suppliers;

-- 60. UNION ALL
SELECT name FROM customers
UNION ALL
SELECT name FROM suppliers;

-- 61. INTERSECT
SELECT name FROM customers
INTERSECT
SELECT name FROM suppliers;

-- 62. EXCEPT
SELECT name FROM customers
EXCEPT
SELECT name FROM suppliers;

-- ================================
-- 9. 高级技巧（工程常用）
-- ================================

-- 63. Top-N 每组
SELECT *
FROM (
    SELECT *,
           ROW_NUMBER() OVER (PARTITION BY customer_id ORDER BY total DESC) AS rn
    FROM orders
) t
WHERE rn = 1;

-- 64. 去重保留最新
SELECT *
FROM (
    SELECT *,
           ROW_NUMBER() OVER (PARTITION BY email ORDER BY created_at DESC) AS rn
    FROM users
) t
WHERE rn = 1;

-- 65. 条件聚合
SELECT
    COUNT(*) AS total,
    SUM(CASE WHEN status = 'paid' THEN 1 ELSE 0 END) AS paid_cnt
FROM orders;

-- 66. Pivot（手写）
SELECT
    customer_id,
    SUM(CASE WHEN status = 'paid' THEN total ELSE 0 END) AS paid_total,
    SUM(CASE WHEN status = 'pending' THEN total ELSE 0 END) AS pending_total
FROM orders
GROUP BY customer_id;

-- 67. Anti Join
SELECT *
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
WHERE o.customer_id IS NULL;

-- 68. Semi Join
SELECT *
FROM customers c
WHERE EXISTS (
    SELECT 1 FROM orders o WHERE o.customer_id = c.customer_id
);

-- 69. 分页（通用）
SELECT *
FROM orders
ORDER BY id
LIMIT 10 OFFSET 20;

-- 70. Keyset Pagination（高性能）
SELECT *
FROM orders
WHERE id > 1000
ORDER BY id
LIMIT 10;

-- 71. JSON 查询（PostgreSQL）
SELECT data->>'name' FROM users;

-- 72. 数组查询
SELECT * FROM products WHERE tags @> ARRAY['tech'];

-- 73. 生成序列
SELECT generate_series(1, 10);

-- 74. 时间截断
SELECT DATE_TRUNC('month', created_at) FROM orders;

-- 75. 时间差
SELECT NOW() - created_at FROM orders;

-- ================================
-- （已达 75+，核心 80/20 已覆盖）
-- ================================
```