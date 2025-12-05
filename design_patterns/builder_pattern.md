# Builder Pattern (构建器模式)

## ASCII Diagram

```
+-------------------+          +-------------------+
|      Director     |          |      Builder      |<<interface>>
+-------------------+          +-------------------+
| - builder         |--------->| + buildPartA()    |
+-------------------+          | + buildPartB()    |
| + construct()     |          | + getResult()     |
+-------------------+          +-------------------+
                                        ^
                                        |
                     +------------------+------------------+
                     |                                     |
          +-------------------+                 +-------------------+
          | ConcreteBuilder1  |                 | ConcreteBuilder2  |
          +-------------------+                 +-------------------+
          | + buildPartA()    |                 | + buildPartA()    |
          | + buildPartB()    |                 | + buildPartB()    |
          | + getResult()     |                 | + getResult()     |
          +-------------------+                 +-------------------+
                   |                                     |
                   v                                     v
          +-------------------+                 +-------------------+
          |     Product1      |                 |     Product2      |
          +-------------------+                 +-------------------+
```

**中文说明：**
- **Director（指挥者）**：负责调用 Builder 的各个构建步骤，控制构建流程
- **Builder（抽象构建器）**：定义创建产品各部分的抽象接口
- **ConcreteBuilder（具体构建器）**：实现 Builder 接口，构建和装配产品各部分
- **Product（产品）**：最终被构建的复杂对象

---

## 核心思想

将一个复杂对象的**构建过程**与其**表示**分离，使得同样的构建过程可以创建不同的表示。通过一步一步地构建复杂对象，允许用户只通过指定复杂对象的类型和内容就能构建它们。

---

## 应用场景

1. **创建复杂对象**：对象由多个部分组成，构建步骤复杂
2. **构建过程独立于组成部分**：相同的构建过程需要创建不同的表示
3. **需要生成的对象有复杂的内部结构**：对象内部属性相互依赖
4. **实际应用**：
   - HTML/XML 文档生成器
   - SQL 查询构建器
   - 复杂配置对象的创建
   - 游戏角色创建器

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 封装性好 | 客户端不需要知道产品内部组成细节 |
| 易于扩展 | 新增具体构建器无需修改原有代码 |
| 精细控制 | 可以精细控制产品的创建过程 |
| 代码复用 | 相同的构建过程可以创建不同的产品 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 产品差异大时不适用 | 如果产品内部变化复杂，需要定义很多具体构建者类 |
| 增加代码量 | 需要额外创建多个类 |

---

## Python 代码示例

### 应用前：直接构建复杂对象

```python
# 问题：构建 SQL 查询语句，代码混乱且难以维护

def build_query(table, columns=None, where=None, order_by=None, limit=None):
    """手动拼接 SQL 查询"""
    sql = "SELECT "
    
    if columns:
        sql += ", ".join(columns)
    else:
        sql += "*"
    
    sql += f" FROM {table}"
    
    if where:
        conditions = []
        for key, value in where.items():
            if isinstance(value, str):
                conditions.append(f"{key} = '{value}'")
            else:
                conditions.append(f"{key} = {value}")
        sql += " WHERE " + " AND ".join(conditions)
    
    if order_by:
        sql += f" ORDER BY {order_by}"
    
    if limit:
        sql += f" LIMIT {limit}"
    
    return sql

# 使用：参数混乱，可读性差
query1 = build_query(
    "users",
    ["id", "name", "email"],
    {"status": "active", "age": 18},
    "created_at DESC",
    10
)
print(query1)
# SELECT id, name, email FROM users WHERE status = 'active' AND age = 18 ORDER BY created_at DESC LIMIT 10

# 问题：
# 1. 参数顺序难记
# 2. 无法灵活组合
# 3. 添加新功能需要修改函数签名
```

### 应用后：使用 Builder 模式

```python
from abc import ABC, abstractmethod


class SQLBuilder(ABC):
    """抽象 SQL 构建器"""
    
    @abstractmethod
    def select(self, *columns):
        pass
    
    @abstractmethod
    def from_table(self, table):
        pass
    
    @abstractmethod
    def where(self, **conditions):
        pass
    
    @abstractmethod
    def order_by(self, column, desc=False):
        pass
    
    @abstractmethod
    def limit(self, count):
        pass
    
    @abstractmethod
    def build(self) -> str:
        pass


class MySQLQueryBuilder(SQLBuilder):
    """MySQL 查询构建器"""
    
    def __init__(self):
        self._reset()
    
    def _reset(self):
        self._columns = ["*"]
        self._table = ""
        self._conditions = []
        self._order = ""
        self._limit = ""
    
    def select(self, *columns):
        if columns:
            self._columns = list(columns)
        return self  # 返回 self 支持链式调用
    
    def from_table(self, table):
        self._table = table
        return self
    
    def where(self, **conditions):
        for key, value in conditions.items():
            if isinstance(value, str):
                self._conditions.append(f"{key} = '{value}'")
            else:
                self._conditions.append(f"{key} = {value}")
        return self
    
    def order_by(self, column, desc=False):
        direction = "DESC" if desc else "ASC"
        self._order = f" ORDER BY {column} {direction}"
        return self
    
    def limit(self, count):
        self._limit = f" LIMIT {count}"
        return self
    
    def build(self) -> str:
        sql = f"SELECT {', '.join(self._columns)} FROM {self._table}"
        
        if self._conditions:
            sql += " WHERE " + " AND ".join(self._conditions)
        
        sql += self._order + self._limit
        
        # 重置以便复用
        result = sql
        self._reset()
        return result


class PostgreSQLQueryBuilder(SQLBuilder):
    """PostgreSQL 查询构建器（支持 ILIKE）"""
    
    def __init__(self):
        self._reset()
    
    def _reset(self):
        self._columns = ["*"]
        self._table = ""
        self._conditions = []
        self._order = ""
        self._limit = ""
    
    def select(self, *columns):
        if columns:
            self._columns = list(columns)
        return self
    
    def from_table(self, table):
        self._table = table
        return self
    
    def where(self, **conditions):
        for key, value in conditions.items():
            if isinstance(value, str):
                # PostgreSQL 使用 ILIKE 进行不区分大小写匹配
                self._conditions.append(f"{key} ILIKE '{value}'")
            else:
                self._conditions.append(f"{key} = {value}")
        return self
    
    def order_by(self, column, desc=False):
        direction = "DESC" if desc else "ASC"
        self._order = f" ORDER BY {column} {direction}"
        return self
    
    def limit(self, count):
        self._limit = f" LIMIT {count}"
        return self
    
    def build(self) -> str:
        sql = f"SELECT {', '.join(self._columns)} FROM {self._table}"
        
        if self._conditions:
            sql += " WHERE " + " AND ".join(self._conditions)
        
        sql += self._order + self._limit
        
        result = sql
        self._reset()
        return result


class QueryDirector:
    """指挥者：封装常用查询构建流程"""
    
    def __init__(self, builder: SQLBuilder):
        self._builder = builder
    
    def build_active_users_query(self):
        """构建查询活跃用户的 SQL"""
        return (self._builder
                .select("id", "name", "email")
                .from_table("users")
                .where(status="active")
                .order_by("created_at", desc=True)
                .limit(10)
                .build())
    
    def build_product_query(self, category):
        """构建查询商品的 SQL"""
        return (self._builder
                .select("id", "name", "price")
                .from_table("products")
                .where(category=category, in_stock=1)
                .order_by("price")
                .build())


# ========== 使用示例 ==========

# 1. 直接使用构建器（链式调用）
mysql_builder = MySQLQueryBuilder()
query = (mysql_builder
         .select("id", "name", "email")
         .from_table("users")
         .where(status="active", age=18)
         .order_by("created_at", desc=True)
         .limit(10)
         .build())
print("MySQL Query:")
print(query)
# SELECT id, name, email FROM users WHERE status = 'active' AND age = 18 ORDER BY created_at DESC LIMIT 10

# 2. 使用 PostgreSQL 构建器
pg_builder = PostgreSQLQueryBuilder()
query = (pg_builder
         .select("id", "name")
         .from_table("users")
         .where(name="John")
         .build())
print("\nPostgreSQL Query:")
print(query)
# SELECT id, name FROM users WHERE name ILIKE 'John'

# 3. 使用指挥者封装常用查询
director = QueryDirector(MySQLQueryBuilder())
print("\nActive Users Query:")
print(director.build_active_users_query())

print("\nProduct Query:")
print(director.build_product_query("Electronics"))
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **可读性** | 参数顺序难记，调用时需要查看函数签名 | 链式调用，语义清晰，自文档化 |
| **灵活性** | 添加新功能需修改函数签名 | 新增方法即可，不影响现有代码 |
| **复用性** | 难以复用构建逻辑 | Director 可封装常用构建流程 |
| **扩展性** | 支持新数据库需修改原函数 | 新增 Builder 实现类即可 |
| **维护性** | 复杂逻辑堆积在一个函数中 | 职责分离，易于维护 |

---

## 变体：简化版 Builder（无 Director）

```python
class Pizza:
    """产品类"""
    def __init__(self):
        self.size = ""
        self.cheese = False
        self.pepperoni = False
        self.mushrooms = False
    
    def __str__(self):
        toppings = []
        if self.cheese:
            toppings.append("cheese")
        if self.pepperoni:
            toppings.append("pepperoni")
        if self.mushrooms:
            toppings.append("mushrooms")
        return f"{self.size} pizza with {', '.join(toppings) or 'no toppings'}"


class PizzaBuilder:
    """流式构建器"""
    
    def __init__(self):
        self._pizza = Pizza()
    
    def size(self, size):
        self._pizza.size = size
        return self
    
    def add_cheese(self):
        self._pizza.cheese = True
        return self
    
    def add_pepperoni(self):
        self._pizza.pepperoni = True
        return self
    
    def add_mushrooms(self):
        self._pizza.mushrooms = True
        return self
    
    def build(self):
        result = self._pizza
        self._pizza = Pizza()  # 重置
        return result


# 使用
pizza = (PizzaBuilder()
         .size("large")
         .add_cheese()
         .add_pepperoni()
         .build())
print(pizza)  # large pizza with cheese, pepperoni
```

