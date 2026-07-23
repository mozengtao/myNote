# Python YAML 解析完全指南 —— 工作原理、典型用法与心智模型

> **目标**
>
> 阅读完本文后，应能够回答下面几个问题：
>
> - YAML 与 JSON 有什么区别？
> - Python 如何解析 YAML？
> - Python 对象与 YAML 是如何互相转换的？
> - safe_load 和 load 有什么区别？
> - 多文档 YAML 如何处理？
> - 自定义 Python Object 如何保存为 YAML？
> - 工作中什么时候应该选择 YAML？

---

# 1. YAML 是什么？

YAML（YAML Ain't Markup Language）是一种**面向人类阅读**的数据序列化格式。

它最大的特点就是：

> **使用缩进表示层次，而不是括号。**

例如：

JSON

```json
{
  "name": "Tom",
  "age": 20,
  "skills": [
    "Python",
    "Go"
  ]
}
```

对应 YAML

```yaml
name: Tom
age: 20

skills:
  - Python
  - Go
```

可以看到：

没有

```
{}
[]
,
```

全部使用：

- 缩进
- :
- -

表示数据结构。

---

# 2. Python 使用哪个库？

最常见：

```python
pip install pyyaml
```

导入：

```python
import yaml
```

最重要的四个 API：

```
yaml.safe_load()

yaml.safe_dump()

yaml.safe_load_all()

yaml.safe_dump_all()
```

另外还有：

```
yaml.load()

yaml.dump()
```

但是一般不推荐直接使用。

---

# 3. YAML 与 Python 对象对应关系（最重要）

这是理解 YAML 的核心。

| YAML | Python |
|-------|---------|
| string | str |
| number | int / float |
| true | bool |
| false | bool |
| null | None |
| sequence (-) | list |
| mapping (:) | dict |

例如

YAML

```yaml
name: Alice
age: 18
student: true

skills:
  - Python
  - Rust
```

解析以后：

```python
{
    "name": "Alice",
    "age":18,
    "student":True,
    "skills":[
        "Python",
        "Rust"
    ]
}
```

所以：

> **YAML 本质上就是 Python 数据结构（dict/list）的另一种文本表示。**

---

# 4. 工作原理（内部流程）

整个解析过程：

```
          YAML File

               │

               ▼

        YAML Scanner
       （词法分析）

               │

               ▼

        YAML Parser
      （语法树 AST）

               │

               ▼

      Constructor

               │

               ▼

 Python dict/list/int/str...
```

反过来：

```
Python Object

      │

      ▼

Representer

      │

      ▼

Emitter

      │

      ▼

YAML Text
```

因此：

```
YAML
     ⇄
Python Object
```

实际上就是：

> 文本 ←→ Python Object

---

# 5. 示例1：解析 YAML 字符串（safe_load）

```python
import yaml

text = """
name: Alice
age: 20
"""

obj = yaml.safe_load(text)

print(obj)
```

输出

```python
{'name': 'Alice', 'age': 20}
```

类型：

```python
type(obj)
```

输出

```python
dict
```

---

## 心智模型

```
YAML String

      │

safe_load()

      │

      ▼

dict
```

---

# 6. 示例2：解析 YAML 文件

config.yaml

```yaml
host: localhost
port: 8080
debug: true
```

Python

```python
import yaml

with open("config.yaml") as f:
    config = yaml.safe_load(f)

print(config)
```

输出：

```python
{
    'host':'localhost',
    'port':8080,
    'debug':True
}
```

这是最常见的配置文件读取方式。

---

# 7. 示例3：保存 Python 对象到 YAML

Python

```python
import yaml

config = {
    "host":"localhost",
    "port":8080
}

text = yaml.safe_dump(config)

print(text)
```

输出：

```yaml
host: localhost
port: 8080
```

---

## 心智模型

```
dict

 │

safe_dump()

 │

 ▼

YAML Text
```

---

# 8. 示例4：保存到 YAML 文件

```python
import yaml

config = {
    "host":"localhost",
    "port":8080
}

with open("config.yaml","w") as f:
    yaml.safe_dump(config,f)
```

以后读取：

```python
yaml.safe_load(...)
```

即可恢复。

---

# 9. 示例5：复杂嵌套结构

Python

```python
config = {
    "server":{
        "host":"localhost",
        "port":8080
    },
    "users":[
        "Tom",
        "Alice"
    ]
}
```

保存以后：

```yaml
server:
  host: localhost
  port: 8080

users:
  - Tom
  - Alice
```

读取以后：

```python
dict

↓

server

↓

dict

↓

users

↓

list
```

完全恢复。

---

# 10. 示例6：读取列表

YAML

```yaml
- apple
- banana
- orange
```

Python

```python
items = yaml.safe_load(text)

print(items)
```

输出：

```python
['apple','banana','orange']
```

类型：

```
list
```

---

# 11. 示例7：多文档 YAML（safe_load_all）

很多 Kubernetes 文件：

```yaml
apiVersion: v1
kind: ConfigMap

---

apiVersion: apps/v1
kind: Deployment
```

Python

```python
docs = yaml.safe_load_all(text)

for doc in docs:
    print(doc)
```

输出：

```
dict

dict
```

---

## 心智模型

```
Document1

---

Document2

---

Document3

↓

Generator

↓

for doc in docs
```

---

# 12. 示例8：生成多个 YAML 文档

```python
docs = [
    {"name":"Tom"},
    {"name":"Alice"}
]

print(yaml.safe_dump_all(docs))
```

输出：

```yaml
name: Tom
---
name: Alice
```

---

# 13. 示例9：保持中文输出

默认：

```python
yaml.safe_dump(data)
```

可能输出：

```yaml
"\u4F60\u597D"
```

正确方式：

```python
yaml.safe_dump(
    data,
    allow_unicode=True
)
```

输出：

```yaml
你好
```

---

# 14. 示例10：保持键顺序

Python 3.7+

```python
config = {
    "name":"Tom",
    "age":18,
    "city":"Beijing"
}
```

默认：

```python
yaml.safe_dump(config)
```

可能排序：

```yaml
age:
city:
name:
```

保持原顺序：

```python
yaml.safe_dump(
    config,
    sort_keys=False
)
```

输出：

```yaml
name: Tom
age: 18
city: Beijing
```

---

# 15. 示例11：Python 自定义对象

```python
class User:

    def __init__(self,name):
        self.name=name
```

不能直接：

```python
yaml.safe_dump(user)
```

通常做法：

```python
yaml.safe_dump(user.__dict__)
```

输出：

```yaml
name: Tom
```

或者定义自定义 Representer（高级用法）。

---

# 16. 示例12：配置文件最佳实践

config.yaml

```yaml
database:
  host: localhost
  port: 3306

redis:
  host: localhost

logging:
  level: INFO
```

Python

```python
with open("config.yaml") as f:
    cfg = yaml.safe_load(f)

db_host = cfg["database"]["host"]
```

大型 Python 项目：

```
config.yaml

↓

safe_load()

↓

dict

↓

Config Object

↓

Business Logic
```

这是 Django、Ansible、SaltStack、Kubeflow 等项目常见模式。

---

# 17. safe_load vs load

这是 PyYAML 最重要的问题。

## yaml.load()

可以构造任意 Python 对象。

例如：

```yaml
!!python/object/apply:os.system
- "rm -rf /"
```

如果直接：

```python
yaml.load(...)
```

可能执行危险代码（取决于 Loader 配置）。

因此：

**不要解析不可信来源的 YAML。**

---

## yaml.safe_load()

只能解析：

```
dict

list

str

int

float

bool

None
```

不会创建任意 Python 对象。

因此：

> **解析配置文件时，应优先使用 `yaml.safe_load()`。**

---

# 18. YAML 解析流程的心智模型

```
           YAML

      （文本格式）

           │

           ▼

      safe_load()

           │

           ▼

Python Object

(dict/list...)

           │

业务逻辑

           │

           ▼

safe_dump()

           │

           ▼

YAML
```

可以理解为：

> **YAML 是一种"数据交换语言"，Python 永远处理的是对象，YAML 只是对象的文本表示。**

---

# 19. YAML 与 JSON 对比

| 特性 | YAML | JSON |
|------|------|------|
| 可读性 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| 支持注释 | ✅ | ❌ |
| 使用缩进 | ✅ | ❌ |
| 配置文件 | ⭐⭐⭐⭐⭐ | ⭐⭐ |
| API 通信 | ⭐⭐ | ⭐⭐⭐⭐⭐ |
| Python 映射 | dict/list | dict/list |
| Kubernetes | ✅ | 很少 |
| Ansible | ✅ | 很少 |

经验法则：

- **配置文件**：优先 YAML。
- **网络接口（REST API）**：优先 JSON。
- **日志、消息队列、前后端交互**：JSON 更常见。
- **人类需要频繁编辑的静态配置**：YAML 更适合。

---

# 20. YAML 的黄金法则（Golden Rules）

## 法则一：YAML 本质是 Python 数据结构的文本表示

```
YAML ⇄ dict/list
```

不要把 YAML 当作一种编程语言，它只是数据描述格式。

---

## 法则二：Python 处理对象，不处理 YAML

应用程序内部应始终操作 `dict`、`list` 等对象，YAML 仅作为输入/输出媒介。

---

## 法则三：默认使用 `safe_load()` 和 `safe_dump()`

除非明确需要自定义对象序列化，否则避免使用 `load()`。

---

## 法则四：配置与代码分离

将环境相关参数（数据库、端口、日志级别等）放入 YAML，代码只负责读取和使用。

---

## 法则五：善用嵌套结构表达层次

用映射（`:`）表示对象，用序列（`-`）表示集合，保持缩进一致，避免过深嵌套。

---

# 21. YAML 核心心智模型（Architecture Diagram）

```text
                  ┌─────────────────────────┐
                  │      config.yaml        │
                  └────────────┬────────────┘
                               │
                               │ safe_load()
                               ▼
                    ┌──────────────────────┐
                    │   Python dict/list   │
                    └────────────┬─────────┘
                                 │
                Business Logic / Configuration Access
                                 │
                                 ▼
                    ┌──────────────────────┐
                    │ Updated Python Object│
                    └────────────┬─────────┘
                                 │
                                 │ safe_dump()
                                 ▼
                  ┌─────────────────────────┐
                  │      config.yaml        │
                  └─────────────────────────┘
```

> **一句话总结：**  
> **YAML = 面向人类的配置格式；Python = 面向对象的数据模型；`safe_load()` 与 `safe_dump()` 则是在文本世界与对象世界之间建立双向桥梁。**