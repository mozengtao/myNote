# Jinja2 核心功能详解

Jinja2 是 Python 生态中最流行的模板引擎，其设计目标是**将数据(Data)与表现(View)分离**。它的核心能力可以归纳为以下几类：

| 功能 | 作用 | 解决的问题 |
|------|------|-----------|
| Variable（变量） | 输出变量内容 | 将程序数据动态渲染到模板 |
| Filter（过滤器） | 对变量进行转换 | 数据格式化、字符串处理、日期处理等 |
| Control Logic（控制逻辑） | 条件判断、循环 | 根据数据动态生成不同内容 |
| Test（测试器） | 判断变量类型或状态 | 提高条件判断可读性 |
| Macro（宏） | 封装可复用模板 | 消除重复代码 |
| Include | 引入其他模板 | 模板模块化 |
| Extend / Block | 模板继承 | 页面布局复用 |
| Whitespace Control | 控制空白字符 | 生成更干净的文本 |
| Custom Filter/Test | 自定义扩展 | 满足业务需求 |

---

# 1. Variable（变量）

## 作用

最基本的能力：把 Python 数据输出到模板。

Python：

```python
user = {
    "name": "Alice",
    "age": 18
}
```

模板：

```jinja2
Name: {{ user.name }}
Age: {{ user.age }}
```

结果：

```
Name: Alice
Age: 18
```

---

解决的问题：

如果没有模板，我们可能会这样写：

```python
print("Name:", user["name"])
print("Age:", user["age"])
```

大量字符串拼接既麻烦又难维护。

Jinja 可以把：

> 数据

和

> 页面布局

完全分离。

---

# 2. Filter（过滤器）

## 什么是 Filter？

Filter 就是：

> **对变量做转换。**

语法：

```jinja2
{{ value | filter }}
```

可以理解成 Linux Pipe：

```
value
   |
filter
   |
output
```

---

## 示例1：字符串转大写

模板：

```jinja2
{{ "hello" | upper }}
```

输出：

```
HELLO
```

---

## 示例2：首字母大写

```jinja2
{{ "hello world" | title }}
```

输出：

```
Hello World
```

---

## 示例3：默认值

Python：

```python
user = {}
```

模板：

```jinja2
{{ user.name | default("Unknown") }}
```

输出：

```
Unknown
```

---

解决的问题：

避免：

```python
if "name" in user:
    ...
else:
    ...
```

---

## 示例4：列表长度

```jinja2
{{ users | length }}
```

假设：

```python
users = ["Tom", "Jack", "Lucy"]
```

输出：

```
3
```

---

## 示例5：排序

```jinja2
{{ nums | sort }}
```

数据：

```python
nums = [5,1,8,3]
```

输出：

```
[1,3,5,8]
```

---

## 示例6：Join

```jinja2
{{ names | join(", ") }}
```

数据：

```python
["Alice","Bob","Tom"]
```

输出：

```
Alice, Bob, Tom
```

---

## 示例7：Replace

```jinja2
{{ filename | replace(".txt",".md") }}
```

输出：

```
README.md
```

---

## Filter 解决的问题

以前：

```python
",".join(names)
```

现在：

```jinja2
{{ names | join(",") }}
```

模板仍然保持"声明式"，无需写 Python 代码。

---

# 3. Control Logic（控制逻辑）

Control Logic 是 Jinja 最重要的能力之一。

主要包括：

- if
- for
- else
- elif

它解决的问题：

> **根据数据生成不同内容。**

---

# 3.1 if 判断

Python：

```python
user = {
    "vip": True
}
```

模板：

```jinja2
{% if user.vip %}
VIP User
{% else %}
Normal User
{% endif %}
```

输出：

```
VIP User
```

---

如果：

```python
user["vip"] = False
```

输出：

```
Normal User
```

---

解决的问题：

以前：

```python
if user["vip"]:
    print(...)
else:
    print(...)
```

现在：

逻辑直接写在模板里。

---

# 3.2 多条件判断

```jinja2
{% if score >= 90 %}
A
{% elif score >= 80 %}
B
{% else %}
C
{% endif %}
```

输入：

```
85
```

输出：

```
B
```

---

# 3.3 for 循环

Python：

```python
users = [
    "Alice",
    "Bob",
    "Tom"
]
```

模板：

```jinja2
{% for user in users %}
- {{ user }}
{% endfor %}
```

输出：

```
- Alice
- Bob
- Tom
```

---

解决的问题：

避免 Python 中：

```python
for u in users:
    print(...)
```

模板自己完成循环。

---

# 3.4 loop 对象

Jinja 提供很多循环变量。

模板：

```jinja2
{% for item in items %}
{{ loop.index }} {{ item }}
{% endfor %}
```

数据：

```python
items = ["A","B","C"]
```

输出：

```
1 A
2 B
3 C
```

常见成员：

| 属性 | 含义 |
|-------|------|
| loop.index | 从1开始 |
| loop.index0 | 从0开始 |
| loop.first | 是否第一个 |
| loop.last | 是否最后一个 |
| loop.length | 总长度 |

例如：

```jinja2
{% for item in items %}
{% if loop.first %}
FIRST
{% endif %}
{{ item }}
{% endfor %}
```

---

# 3.5 for + if

模板：

```jinja2
{% for user in users %}
{% if user.age >= 18 %}
{{ user.name }}
{% endif %}
{% endfor %}
```

数据：

```python
[
    {"name":"Tom","age":16},
    {"name":"Alice","age":22},
    {"name":"Bob","age":30}
]
```

输出：

```
Alice
Bob
```

---

# 4. Test（测试器）

Test 用于：

> 判断变量是否满足某种条件。

语法：

```jinja2
value is test
```

例如：

```jinja2
{% if user.name is defined %}
```

解决的问题：

比：

```jinja2
if user.name != None
```

更加语义化。

---

示例：

```jinja2
{% if value is odd %}
Odd
{% endif %}
```

输入：

```
5
```

输出：

```
Odd
```

---

# 5. Macro（宏）

Macro 相当于：

> 模板中的函数。

例如：

```jinja2
{% macro button(text,color) %}
<button class="{{ color }}">
{{ text }}
</button>
{% endmacro %}
```

调用：

```jinja2
{{ button("Save","green") }}
{{ button("Delete","red") }}
```

输出：

```html
<button class="green">
Save
</button>

<button class="red">
Delete
</button>
```

---

解决的问题：

避免重复复制 HTML。

---

# 6. Include

例如：

```
header.html
footer.html
```

主模板：

```jinja2
{% include "header.html" %}

Main Page

{% include "footer.html" %}
```

解决的问题：

公共页面统一维护。

---

# 7. Template Inheritance（模板继承）

Base：

```jinja2
<html>

{% block content %}

{% endblock %}

</html>
```

Child：

```jinja2
{% extends "base.html" %}

{% block content %}

Hello World

{% endblock %}
```

输出：

```html
<html>

Hello World

</html>
```

---

解决的问题：

整个网站共享同一个布局。

---

# 8. Whitespace Control（空白控制）

普通：

```jinja2
{% for i in [1,2] %}
{{ i }}
{% endfor %}
```

可能产生很多空行。

使用：

```jinja2
{%- for i in [1,2] -%}
{{ i }}
{%- endfor -%}
```

输出更紧凑：

```
12
```

适合生成：

- 配置文件
- YAML
- JSON
- Shell
- C/C++ 代码

---

# 9. Custom Filter（自定义过滤器）

Python：

```python
def hex_filter(x):
    return hex(x)

env.filters["hex"] = hex_filter
```

模板：

```jinja2
{{ 255 | hex }}
```

输出：

```
0xff
```

---

解决的问题：

将业务逻辑封装成可复用的模板能力，而不是在模板中编写复杂表达式。

---

# 一个综合示例

Python 数据：

```python
users = [
    {"name":"Alice","age":18},
    {"name":"Tom","age":15},
    {"name":"Bob","age":22}
]
```

模板：

```jinja2
Users:

{% for user in users %}
{% if user.age >= 18 %}
- {{ user.name | upper }}
{% endif %}
{% endfor %}
```

输出：

```
Users:

- ALICE
- BOB
```

这个例子同时使用了：

- Variable（`user.name`）
- Filter（`upper`）
- Control Logic（`for`）
- Control Logic（`if`）

体现了 Jinja2 的核心思想：**使用模板负责"展示"，使用 Python 提供"数据"，通过过滤器处理数据格式，通过控制逻辑根据数据动态生成最终内容，从而实现数据与表现层的彻底解耦。**