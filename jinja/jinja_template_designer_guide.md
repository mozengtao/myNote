# Jinja Template Designer 20/80 实战速查与 Mental Model

> 运行环境（已验证）：Python 3.10.18 + `jinja2` 3.1.4
> 本文所有 **Output** 代码块都不是手写推测的，而是用下面的脚本实际跑出来的：
> [jinja/designer_guide_examples/](designer_guide_examples/)
> 每个小节标题后会注明对应的验证脚本路径，你可以直接打开运行、修改、对照。

本指南遵循 **20/80 原则**：不追求覆盖 Jinja 的每一个边角语法，而是把日常写自动化脚本、生成配置文件（YAML/XML/CLI/HTML）时真正会用到的 20% 语法讲透，并且**每一条语法都配齐 Template + Data + Output**，让你在几秒钟内看懂它的作用。

优先级标注说明：

- **P0：必须掌握** —— 高频基础语法，日常使用离不开。
- **P1：高频实用** —— 常见数据处理、控制逻辑及模板复用。
- **P2：按需了解** —— 特殊功能、扩展机制和较少使用的语法。

---

# 01. Jinja Mental Model

## 1.1 Jinja 是什么？为什么需要模板引擎？

Jinja 是一个 **文本模板引擎（Template Engine）**：它把「静态文本骨架」和「动态数据」分离开，渲染时把数据"填"进骨架里，生成最终文本。

如果不用模板引擎，你大概率会写出这样的代码：

```python
lines = []
lines.append(f"hostname {hostname}")
for intf in interfaces:
    if intf["enabled"]:
        lines.append(f"interface {intf['name']}")
        lines.append(f" mtu {intf.get('mtu', 1500)}")
output = "\n".join(lines)
```

问题：**业务逻辑（Python）和文本格式（输出长什么样）耦合在一起**，格式稍微一变就要改 Python 代码，而且很难让不懂 Python 的人（比如网络工程师）去维护"输出模板长什么样"。

Jinja 把"长什么样"交给模板文件，把"数据怎么来"交给 Python，两者通过一次 `render()` 调用组合，这就是模板引擎存在的意义。

## 1.2 Jinja 与 Python 的职责边界

| 职责 | 谁负责 |
|---|---|
| 从数据库/API/配置文件读取原始数据 | Python |
| 复杂业务计算（打分、聚合、排序、分类） | **优先 Python**（模板里也能做，但不推荐） |
| 决定最终文本长什么样（格式、缩进、字段顺序） | Jinja Template |
| 根据条件决定输出哪些字段 | Jinja Template（简单判断）/ Python（复杂判断） |
| 字符串格式化、默认值兜底、简单过滤 | Jinja Filters |
| 把渲染结果写入文件/发送网络请求 | Python |

一句话：**Python 负责"数据从哪来、算成什么样"，Jinja 负责"最终文本长什么样"。**

## 1.3 Template、Context、Environment、Render Output 分别是什么？

- **Template（模板）**：一段包含 `{{ }}` / `{% %}` / `{# #}` 语法的文本源（字符串或 `.j2` 文件）。它本身不是最终结果，只是"排版规则 + 占位符"。
- **Context（上下文/数据）**：传给 `render()` 的 Python 数据（dict、对象、列表等），模板里的变量名从这里查找。
- **Environment（环境）**：Jinja 的"配置中心"，决定了：去哪里加载模板（`Loader`）、空白怎么处理（`trim_blocks` 等）、缺失变量怎么处理（`Undefined`）、要不要自动转义（`autoescape`）、注册了哪些自定义 filter/test/全局函数。
- **Render Output（渲染结果）**：`template.render(**context)` 的返回值，是一个普通的 Python `str`，之后可以打印、写文件、通过网络发送。

## 1.4 整体架构图

```text
Python Application
       |
       | 1. Prepare Data
       v
   Context Data
       |
       +------------------+
       |                  |
       v                  v
 Template Source      Environment
       |                  |
       +--------+---------+
                |
                v
         Jinja Rendering
                |
                v
          Rendered Text
                |
                v
       File / CLI / Network
```

- `Template Source`：`.j2` 文件内容或 `from_string()` 传入的字符串。
- `Environment`：决定"怎么渲染"（空白处理、Undefined 策略、自动转义、可用的 filter/test）。
- `Context Data`：决定"渲染出什么值"。
- `Jinja Rendering`：Jinja 内部先把模板源编译成一棵语法树/字节码，再结合 Context 执行，得到字符串。
- `Rendered Text`：普通字符串，接下来做什么完全由 Python 决定（打印、写文件、发 HTTP 请求等）。

## 1.5 典型渲染生命周期

```text
Create Environment
       |
       v
Load / Create Template
       |
       v
Prepare Context Data
       |
       v
Render Template
       |
       v
Obtain Final String
       |
       v
Write File / Send / Print
```

对应最简代码（完整可运行，验证脚本：[designer_guide_examples/python_integration/from_string_demo.py](designer_guide_examples/python_integration/from_string_demo.py)）：

```python
from jinja2 import Environment, StrictUndefined

env = Environment(
    undefined=StrictUndefined,
    autoescape=False,
)

template = env.from_string(
    "device={{ name }}, port={{ port }}"
)

output = template.render(
    name="router-01",
    port=8080,
)

print(output)
```

**Output**

```text
device=router-01, port=8080
```

## 1.6 几组必须分清的边界

- **模板语法 vs Python 语法**：`{{ a if b else c }}` 看起来像 Python 三元表达式，但它是 Jinja 自己的表达式语法，不是在模板里执行 Python 代码；模板里不能写 `import`、不能定义 `class`、不能用 `print()`。
- **模板源文件 vs 渲染结果**：`.j2` 文件 / 字符串里的 `{{ name }}` 是"占位符"，渲染后才变成 `Morris` 这样的真实文本。
- **模板中的控制结构 vs 最终输出文本**：`{% if %}`、`{% for %}`、`{% set %}` 这些 `{% %}` 标签本身**不会出现在输出里**，它们只是控制"要不要输出、输出几次、临时变量是什么"；真正进入输出文本的只有 `{{ }}` 表达式的求值结果和模板里的普通字面文本。
- **Jinja 内置功能 vs 应用自定义**：`default`、`upper`、`length` 等是 Jinja **内置** filter；`is odd`、`is defined` 等是 Jinja **内置** test；而像 1.5 节代码里直接把 Python 函数塞进 `render(name=..., port=...)` 这种，或者通过 `env.filters['my_filter'] = ...` 注册的，属于**应用程序自定义**的 filter / test / 全局函数，官方文档之外的项目特定逻辑都应该放在这一层，而不是硬塞进模板语法里。

---

# 02. Core Syntax Cheat Sheet

下表是导航索引，后续每一行都会在对应章节给出完整的 Template + Data + Output。

| 分类 | 语法 / API | 主要作用 | 优先级 |
|---|---|---|---|
| 输出 | `{{ variable }}` | 输出变量 | P0 |
| 属性访问 | `{{ user.name }}` | 访问属性或字典键 | P0 |
| 索引访问 | `{{ items[0] }}` | 访问列表或字典 | P0 |
| 表达式 | `{{ a ~ b }}` | 字符串拼接 | P0 |
| 赋值 | `{% set %}` | 创建模板变量 | P0 |
| 判断 | `{% if %}` | 条件输出 | P0 |
| 循环 | `{% for %}` | 遍历序列 | P0 |
| 过滤器 | `{{ value \| default('N/A') }}` | 默认值处理 | P0 |
| 过滤器链 | `{{ value \| trim \| upper }}` | 连续转换 | P0 |
| 测试 | `{% if value is defined %}` | 检查变量状态 | P0 |
| 注释 | `{# ... #}` | 注释模板内容 | P0 |
| 循环元信息 | `loop.index` | 获取循环位置 | P1 |
| 循环过滤 | `{% for x in xs if x.enabled %}` | 过滤循环项 | P1 |
| 空集合处理 | `{% else %}`（for 的 else） | 处理没有迭代项的情况 | P1 |
| 空白控制 | `-{%`、`%}-` 等 | 控制多余空白 | P1 |
| 模板复用 | `{% include %}` | 引入其他模板 | P1 |
| 模板继承 | `{% extends %}` | 复用公共布局 | P1 |
| 块定义 | `{% block %}` | 定义可覆盖区域 | P1 |
| 宏 | `{% macro %}` | 创建可复用模板片段 | P1 |
| 导入 | `{% import %}`、`{% from %}` | 复用宏 | P1 |
| 调用函数 | `{{ func(arg) }}` | 调用可用函数 | P1 |
| 转义 | `escape` / `e` | 对输出内容进行转义 | P1 |
| 调试 | `StrictUndefined`、检查 Context | 定位渲染问题 | P1 |
| 空集合判断 | `{% if list %}` | 根据集合是否为空分支 | P1 |
| 跨循环状态 | `namespace()` | 需要在循环间保留可变状态时使用 | P2 |
| 循环交替 | `loop.cycle()` | 交替生成样式/标记 | P2 |
| 字典转键值对 | `dictsort` / `items` | 遍历字典（注意：Jinja **没有** `dict` 过滤器） | P2 |
| 复用带 context 的宏 | `{% import ... with context %}` | 让导入的宏能访问调用方变量 | P2 |

---

# 03. Variables and Expressions

验证脚本：[designer_guide_examples/variables_expressions_demo.py](designer_guide_examples/variables_expressions_demo.py)

## 3.1 变量与数据访问

#### `{{ name }}` —— 基本变量输出（P0）

**用途：** 把 Context 中的变量渲染到文本中，这是 Jinja 最基本的能力。

**Template**

```jinja2
Hello, {{ name }}!
```

**Data**

```python
{"name": "Morris"}
```

**Output**

```text
Hello, Morris!
```

**Mental Model**

```text
Template + Context Data
          |
          v
    Jinja Rendering
          |
          v
     Final Text
```

#### `{{ user.name }}` —— 属性/键访问（P0）

**用途：** 访问字典的键或对象的属性，语法统一，不用关心底层是 dict 还是对象。

**Template**

```jinja2
{{ user.name }}
```

**Data**

```python
{"user": {"name": "Alice", "age": 18}}
```

**Output**

```text
Alice
```

#### `{{ user["name"] }}` —— 索引写法访问字典键（P0）

**Template**

```jinja2
{{ user["name"] }}
```

**Data**

```python
{"user": {"name": "Alice", "age": 18}}
```

**Output**

```text
Alice
```

#### `{{ items[0] }}` —— 列表索引访问（P0）

**Template**

```jinja2
{{ items[0] }}
```

**Data**

```python
{"items": ["eth0", "eth1", "eth2"]}
```

**Output**

```text
eth0
```

#### 嵌套字典/列表访问（P0）

**Template**

```jinja2
{{ config.network.interface }}
```

**Data**

```python
{"config": {"network": {"interface": "eth0"}}}
```

**Output**

```text
eth0
```

嵌套"字典里套列表"同样适用：

**Template**

```jinja2
{{ config.network.interfaces[1] }}
```

**Data**

```python
{"config": {"network": {"interfaces": ["eth0", "eth1"]}}}
```

**Output**

```text
eth1
```

#### 访问不存在的变量 / 属性（P0）

默认 `Undefined` 策略下，访问不存在的变量或属性**不会报错**，而是渲染成空字符串：

**Template**

```jinja2
[{{ missing_var }}]
```

**Data**

```python
{}
```

**Output**

```text
[]
```

**Template**

```jinja2
[{{ user.nickname }}]
```

**Data**

```python
{"user": {"name": "Alice"}}
```

**Output**

```text
[]
```

> 这正是第 11 节"常见错误"里很多拼写错误长期不被发现的根本原因——默认行为是"静默吞掉"，不是报错。第 9 节会展示如何用 `StrictUndefined` 让这类问题在渲染阶段直接报错。

### `foo.bar` 与 `foo["bar"]` 的查找顺序差异

官方规则：

- `foo.bar` 先尝试 `getattr(foo, "bar")`（当成对象属性访问），失败后再退化为 `foo["bar"]`（当成字典键访问）。
- `foo["bar"]` 先尝试 `foo["bar"]`（当成字典/序列取值），失败后再退化为 `getattr(foo, "bar")`。

对普通 `dict` 来说，两种写法结果通常一样（因为 dict 没有名为 `name` 之类的"属性"，只有键）。但当一个对象**同时**有同名属性和同名字典键，且值不同的时候，两种写法会给出不同结果——这正是歧义所在。

**Template**

```jinja2
dot:    {{ obj.name }}
bracket: {{ obj["name"] }}
```

**Data（Python，构造一个继承自 dict 又有同名属性的对象）**

```python
class ObjWithBothAttrAndItem(dict):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.name = "ATTRIBUTE-VALUE"

obj = ObjWithBothAttrAndItem(name="DICT-KEY-VALUE")
```

**Output**

```text
dot:    ATTRIBUTE-VALUE
bracket: DICT-KEY-VALUE
```

**工程场景：** 绝大多数情况下用 `.attr` 写法就够了，更简洁。但如果你的数据模型里出现了"自定义对象 + 同名属性/键"这种情况（比如某些 ORM 对象、Pydantic 模型包了一层 dict），遇到取值不符合预期时，第一反应应该是怀疑这里的歧义，用 `["key"]` 显式地按字典键取值来排除干扰。

## 3.2 表达式

#### 字符串拼接 `~`（P0）

`~` 会把两边的值都转成字符串再拼接，不要求两边都是字符串（这点比 Python 的 `+` 更宽松）。

**Template**

```jinja2
{{ 'vlan-' ~ vlan_id }}
```

**Data**

```python
{"vlan_id": 100}
```

**Output**

```text
vlan-100
```

#### 算术运算（P0）

**Template / Output 对照（Data 均为空，字面量运算）**

| Template | Output |
|---|---|
| `{{ 10 + 5 }}` | `15` |
| `{{ 10 - 5 }}` | `5` |
| `{{ 10 * 5 }}` | `50` |
| `{{ 10 / 4 }}` | `2.5` |
| `{{ 10 // 4 }}` | `2` |
| `{{ 10 % 4 }}` | `2` |

#### 比较运算（P0）

**Template**

```jinja2
{{ mtu == 1500 }}
```

**Data**

```python
{"mtu": 1500}
```

**Output**

```text
True
```

`!=`、`>`、`<`、`>=`、`<=` 用法完全相同，这里不再逐一展开。

#### 逻辑运算（P0）

**Template**

```jinja2
{{ enabled and mtu > 0 }}
```

**Data**

```python
{"enabled": True, "mtu": 1500}
```

**Output**

```text
True
```

`or`、`not` 用法同理。

#### 成员判断 `in`（P0）

**Template**

```jinja2
{{ 'eth0' in interfaces }}
```

**Data**

```python
{"interfaces": ["eth0", "eth1"]}
```

**Output**

```text
True
```

#### 三元表达式 `a if condition else b`（P0）

**Template**

```jinja2
{{ 'UP' if interface.enabled else 'DOWN' }}
```

**Data（enabled=True）**

```python
{"interface": {"enabled": True}}
```

**Output**

```text
UP
```

**Data（enabled=False）**

```python
{"interface": {"enabled": False}}
```

**Output**

```text
DOWN
```

#### 函数调用（P1）

模板里可以调用传入 Context 的函数，也可以调用 Jinja 内置的全局函数（如 `range`）：

**Template**

```jinja2
{{ range(3) | list }}
```

**Output**

```text
[0, 1, 2]
```

**Template（调用自定义函数）**

```jinja2
{{ format_mtu(1500) }}
```

**Data**

```python
def format_mtu(value):
    return f"MTU={value}"

{"format_mtu": format_mtu}
```

**Output**

```text
MTU=1500
```

#### 属性访问 + 索引访问组合（P0）

**Template**

```jinja2
{{ devices[0].interfaces[1].name }}
```

**Data**

```python
{
    "devices": [
        {"interfaces": [{"name": "eth0"}, {"name": "eth1"}]}
    ]
}
```

**Output**

```text
eth1
```

## 3.3 `{% set %}`

#### 普通变量赋值（P0）

**Template**

```jinja2
{% set greeting = 'Hello' %}{{ greeting }}, {{ name }}!
```

**Data**

```python
{"name": "Morris"}
```

**Output**

```text
Hello, Morris!
```

#### 表达式赋值（P0）

**Template**

```jinja2
{% set total_ports = len_a + len_b %}{{ total_ports }}
```

**Data**

```python
{"len_a": 3, "len_b": 5}
```

**Output**

```text
8
```

#### 多变量解包（P1）

**Template**

```jinja2
{% set a, b = (1, 2) %}a={{ a }}, b={{ b }}
```

**Output**

```text
a=1, b=2
```

#### 预先计算值以简化模板（P1）

**Template**

```jinja2
{% set enabled_count = interfaces | selectattr('enabled') | list | length %}Enabled interfaces: {{ enabled_count }}
```

**Data**

```python
{
    "interfaces": [
        {"name": "eth0", "enabled": True},
        {"name": "eth1", "enabled": False},
        {"name": "eth2", "enabled": True},
    ]
}
```

**Output**

```text
Enabled interfaces: 2
```

### 循环中 `{% set %}` 的作用域陷阱（P0，必须理解）

**错误用法：** 以为 `{% set total = total + item.value %}` 能像 Python 里的累加器一样在循环间持续累加。

**Template**

```jinja2
{% set total = 0 %}
{% for item in items %}
{% set total = total + item.value %}
{% endfor %}
total (WRONG, still 0) = {{ total }}
```

**Data**

```python
{"items": [{"value": 10}, {"value": 20}, {"value": 30}]}
```

**Output**

```text
total (WRONG, still 0) = 0
```

**Mental Model（为什么会这样）**

```text
for 循环体内的 {% set %}
        |
        v
  只在"当前这次循环迭代"的作用域里生效
        |
        v
  循环体结束 -> 这次迭代创建的 total 被丢弃
        |
        v
  外层的 total 永远是循环开始前的那个值（0）
```

Jinja 的 `for` 循环会为每次迭代创建一个新的局部作用域，循环体内 `{% set %}` 赋的新值**不会**传播到外层作用域，也不会在下一次迭代里被继承——这与 C/Go/Rust 里"循环变量在循环体外依然可见"的直觉不同，是 Jinja 初学者最容易踩的坑之一。

### 正确做法 1：`namespace()` 跨循环保存可变状态（P2）

**Template**

```jinja2
{% set ns = namespace(total=0) %}
{% for item in items %}
{% set ns.total = ns.total + item.value %}
{% endfor %}
total (namespace) = {{ ns.total }}
```

**Data（同上）**

**Output**

```text
total (namespace) = 60
```

`namespace()` 创建的是一个"可变容器对象"，`ns.total = ...` 修改的是这个对象的属性，而不是重新创建一个局部变量，所以能跨迭代保留状态。

### 正确做法 2（推荐）：优先在 Python 里把复杂计算做完

**Python**

```python
precomputed_total = sum(item["value"] for item in items)
template.render(total=precomputed_total)
```

**Template**

```jinja2
total (computed in Python) = {{ total }}
```

**Output**

```text
total (computed in Python) = 60
```

**工程建议：** 只要涉及"跨循环累加/聚合"，第一选择永远是在 Python 里用 `sum()`/`Counter`/列表推导算好，再把结果传给模板。`namespace()` 只在你确实必须在模板里做这件事（比如模板是业务方直接维护、Python 侧不方便改）时才用。

---

# 04. Conditions and Loops

验证脚本：[designer_guide_examples/conditions_loops_demo.py](designer_guide_examples/conditions_loops_demo.py)

## 4.1 条件判断

#### `{% if %}` / `{% elif %}` / `{% else %}`（P0）

**Template**

```jinja2
{% if level == 'error' %}
[ERROR] {{ message }}
{% elif level == 'warning' %}
[WARN] {{ message }}
{% else %}
[INFO] {{ message }}
{% endif %}
```

**Data 1**

```python
{"level": "error", "message": "disk full"}
```

**Output 1**

```text
[ERROR] disk full
```

**Data 2**

```python
{"level": "warning", "message": "disk 80% full"}
```

**Output 2**

```text
[WARN] disk 80% full
```

**Data 3**

```python
{"level": "info", "message": "backup done"}
```

**Output 3**

```text
[INFO] backup done
```

#### 多条件组合（P0）

**Template**

```jinja2
{% if enabled and mtu >= 1500 %}OK{% else %}SKIP{% endif %}
```

**Data（enabled=True, mtu=9000）→ Output：** `OK`

**Data（enabled=True, mtu=1000）→ Output：** `SKIP`

#### 判断变量是否定义：`is defined`（P0）

**Template**

```jinja2
{% if port is defined %}port={{ port }}{% else %}no port{% endif %}
```

**Data**

```python
{}
```

**Output**

```text
no port
```

#### 判断变量是否为 `none`：`is none`（P0）

**Template**

```jinja2
{% if port is none %}port is None{% else %}port={{ port }}{% endif %}
```

**Data**

```python
{"port": None}
```

**Output**

```text
port is None
```

> 注意：`port is defined` 和 `port is none` 回答的是两个不同的问题——"这个键存在吗"和"这个键的值是不是 Python 的 `None`"。一个字段在 Context 里被显式设置成 `None`，`is defined` 会是 `True`，但 `is none` 也会是 `True`；一个字段完全没传，`is defined` 是 `False`，而 `is none` 对一个 `Undefined` 对象求值也会是 `False`（因为 `Undefined is not None`）。两者必须结合使用才能覆盖"缺失"和"显式为空"两种情况，详见第 6 节。

#### 判断列表是否为空（P1）

**Template**

```jinja2
{% if interfaces %}has interfaces{% else %}no interfaces{% endif %}
```

**Data（空列表）→ Output：** `no interfaces`
**Data（非空列表）→ Output：** `has interfaces`

#### 根据配置生成不同的 YAML 字段（P0，工程高频）

**Template**

```jinja2
name: {{ device.name }}
{% if device.description is defined %}
description: {{ device.description }}
{% endif %}
{% if device.vlan is defined %}
vlan: {{ device.vlan }}
{% endif %}
```

**Data 1（字段齐全）**

```python
{"device": {"name": "eth0", "description": "uplink", "vlan": 10}}
```

**Output 1**

```text
name: eth0
description: uplink
vlan: 10
```

**Data 2（只有必填字段）**

```python
{"device": {"name": "eth1"}}
```

**Output 2**

```text
name: eth1
```

## 4.2 循环

#### `{% for item in items %}`（P0）

**Template**

```jinja2
{% for item in items %}{{ item }} {% endfor %}
```

**Data**

```python
{"items": ["eth0", "eth1", "eth2"]}
```

**Output**

```text
eth0 eth1 eth2 
```

#### 遍历字典（P0）

**Template**

```jinja2
{% for key, value in config.items() %}{{ key }}={{ value }} {% endfor %}
```

**Data**

```python
{"config": {"mtu": 1500, "enabled": True}}
```

**Output**

```text
mtu=1500 enabled=True 
```

> `config.items()` 是在调用 Python dict 的方法（Jinja 允许在表达式里调用对象方法）。如果想用 Jinja 风格的过滤器写法，也可以用 `{% for key, value in config | dictsort %}`（按 key 排序）或 `{% for key, value in config | items %}`（保持原始顺序），详见第 5 节。

#### 遍历对象（字典）列表（P0）

**Template**

```jinja2
{% for device in devices %}{{ device.name }}({{ device.status }}) {% endfor %}
```

**Data**

```python
{
    "devices": [
        {"name": "sw1", "status": "up"},
        {"name": "sw2", "status": "down"},
    ]
}
```

**Output**

```text
sw1(up) sw2(down) 
```

#### 嵌套循环（P1）

**Template**

```jinja2
{% for device in devices %}{% for interface in device.interfaces %}{{ device.name }}.{{ interface }} {% endfor %}{% endfor %}
```

**Data**

```python
{
    "devices": [
        {"name": "sw1", "interfaces": ["eth0", "eth1"]},
        {"name": "sw2", "interfaces": ["eth0"]},
    ]
}
```

**Output**

```text
sw1.eth0 sw1.eth1 sw2.eth0 
```

#### 循环内的 `{% if %}`（P0）

**Template**

```jinja2
{% for device in devices %}{% if device.enabled %}{{ device.name }} {% endif %}{% endfor %}
```

**Data**

```python
{
    "devices": [
        {"name": "eth0", "enabled": True},
        {"name": "eth1", "enabled": False},
    ]
}
```

**Output**

```text
eth0 
```

#### 带过滤条件的循环：`{% for x in xs if x.enabled %}`（P1）

**Template**

```jinja2
{% for device in devices if device.enabled %}{{ device.name }} {% endfor %}
```

**Data**

```python
{
    "devices": [
        {"name": "eth0", "enabled": True},
        {"name": "eth1", "enabled": False},
        {"name": "eth2", "enabled": True},
    ]
}
```

**Output**

```text
eth0 eth2 
```

这和"循环内的 `{% if %}`"效果相似，但**语义更清晰**（一眼看出"这个循环只处理满足条件的项"），而且能和 `{% for ... %}{% else %}` 配合，判断"过滤后是不是一个都没有"。

#### `{% for %}` 的 `{% else %}`（P1）

`for ... else` 在"循环体一次都没执行"时触发，常被误以为和 Python 的 `for...else`（循环正常结束时触发）一样，其实不是——Jinja 的 `else` 只看"有没有迭代到元素"。

**Template**

```jinja2
{% for device in devices %}{{ device.name }} {% else %}no devices{% endfor %}
```

**Data（空列表）**

```python
{"devices": []}
```

**Output**

```text
no devices
```

**结合过滤条件的 for-else（过滤后为空）**

**Template**

```jinja2
{% for device in devices if device.enabled %}{{ device.name }} {% else %}no enabled devices{% endfor %}
```

**Data**

```python
{"devices": [{"name": "eth0", "enabled": False}]}
```

**Output**

```text
no enabled devices
```

**Data（有元素）→ Output：**

```text
eth0 
```

#### `loop.index` 与 `loop.index0`（P1，必须用同一份数据对比）

**Template**

```jinja2
{% for item in items %}{{ loop.index }}:{{ loop.index0 }}:{{ item }} {% endfor %}
```

**Data**

```python
{"items": ["eth0", "eth1", "eth2"]}
```

**Output**

```text
1:0:eth0 2:1:eth1 3:2:eth2 
```

`loop.index` 从 **1** 开始计数（适合展示给人看，比如"第几条配置"），`loop.index0` 从 **0** 开始计数（适合用作数组下标）。

#### `loop.first` / `loop.last`（P1）

**Template**

```jinja2
{% for item in items %}{% if loop.first %}[FIRST]{% endif %}{{ item }}{% if loop.last %}[LAST]{% endif %} {% endfor %}
```

**Data（同上）**

**Output**

```text
[FIRST]eth0 eth1 eth2[LAST] 
```

常见用途：第一项不加分隔符、最后一项不加逗号（比如拼 CLI 参数列表）。

#### `loop.length`（P1）

**Template**

```jinja2
{% for item in items %}{{ loop.index }}/{{ loop.length }} {% endfor %}
```

**Output**

```text
1/3 2/3 3/3 
```

#### `loop.cycle()`（P2，按需了解）

**Template**

```jinja2
{% for item in items %}{{ loop.cycle('odd', 'even') }}:{{ item }} {% endfor %}
```

**Data**

```python
{"items": ["eth0", "eth1", "eth2", "eth3"]}
```

**Output**

```text
odd:eth0 even:eth1 odd:eth2 even:eth3 
```

典型用途：隔行样式（表格斑马纹）、交替生成某种标记。日常配置生成里不算高频，知道存在即可。

**`loop` 对象使用频率小结：**

- 日常高频：`loop.index`、`loop.index0`、`loop.first`、`loop.last`
- 偶尔用到：`loop.length`
- 知道存在即可：`loop.cycle()`、`loop.previtem`/`loop.nextitem`、`loop.depth`（嵌套循环深度，Jinja 2.11+）

---

# 05. Filters

验证脚本：[designer_guide_examples/filters_demo.py](designer_guide_examples/filters_demo.py)

## 5.1 Filter 的 Mental Model

```text
Input Value
    |
    v
Filter 1
    |
    v
Filter 2
    |
    v
Rendered Output
```

**Template**

```jinja2
{{ name | trim | upper }}
```

过滤器用 `|` 连接，**从左到右依次执行**：`trim` 先对原始值处理，它的返回值再作为输入传给 `upper`。本质上等价于 Python 里的 `upper(trim(name))`，只是写成了"从左到右"的流水线语法，可读性更好。

## 5.2 必须详细演示的过滤器

### `default`（P0，两种形态必须对比）

`default(value, boolean=False)`：第一参数是兜底值；`boolean` 参数决定判断标准——

- `boolean=False`（默认）：只有当值**完全未定义**（Undefined）时才使用兜底值。
- `boolean=True`：只要值是 **Python 假值**（空字符串 `''`、`0`、`False`、`None`、空列表等）就使用兜底值。

**Template**

```jinja2
{{ value | default('N/A') }}
```

| Data | Output | 说明 |
|---|---|---|
| `{}`（未传 value） | `N/A` | 触发默认值 |
| `{"value": ""}` | （空字符串） | **不**触发默认值，空字符串已"定义" |
| `{"value": 0}` | `0` | **不**触发默认值 |
| `{"value": False}` | `False` | **不**触发默认值 |

**Template**

```jinja2
{{ value | default('N/A', true) }}
```

| Data | Output | 说明 |
|---|---|---|
| `{}`（未传 value） | `N/A` | 触发默认值 |
| `{"value": ""}` | `N/A` | 触发默认值（空字符串是假值） |
| `{"value": 0}` | `N/A` | 触发默认值（0 是假值） |
| `{"value": False}` | `N/A` | 触发默认值（False 本身就是假值） |

**工程场景：** 生成配置时，`retries: {{ retries | default(3) }}` 如果用户显式传了 `retries=0`（表示"不重试"），用不带 `true` 的默认写法能正确保留 `0`；但如果字段语义是"空字符串/0 都应该当成未配置"，就要显式加 `true`。**混用这两种语义是第 11 节里最常见的 bug 来源之一。**

### `trim` / `upper` / `lower` / `capitalize` / `title`（P0）

| Filter | Template | Data | Output |
|---|---|---|---|
| `trim` | `[{{ hostname \| trim }}]` | `{"hostname": "   router-01   "}` | `[router-01]` |
| `upper` | `{{ name \| upper }}` | `{"name": "router-01"}` | `ROUTER-01` |
| `lower` | `{{ name \| lower }}` | `{"name": "ROUTER-01"}` | `router-01` |
| `capitalize` | `{{ text \| capitalize }}` | `{"text": "hello world"}` | `Hello world` |
| `title` | `{{ text \| title }}` | `{"text": "hello world"}` | `Hello World` |

注意 `capitalize` 只把**第一个字符**大写、其余转小写；`title` 把**每个单词**首字母大写。

### `replace`（P0）

**Template**

```jinja2
{{ path | replace('/', '_') }}
```

**Data**

```python
{"path": "/etc/network/interfaces"}
```

**Output**

```text
_etc_network_interfaces
```

### `join`（P0）

**Template**

```jinja2
{{ interfaces | join(', ') }}
```

**Data**

```python
{"interfaces": ["eth0", "eth1", "eth2"]}
```

**Output**

```text
eth0, eth1, eth2
```

`join` 支持 `attribute=` 参数，直接从对象列表里取属性拼接，省掉一次 `map`：

**Template**

```jinja2
{{ devices | join(', ', attribute='name') }}
```

**Data**

```python
{"devices": [{"name": "sw1"}, {"name": "sw2"}, {"name": "sw3"}]}
```

**Output**

```text
sw1, sw2, sw3
```

### `length`（P0）

**Template**

```jinja2
{{ interfaces | length }}
```

**Data**

```python
{"interfaces": ["eth0", "eth1", "eth2"]}
```

**Output**

```text
3
```

对字符串同样适用：`{{ 'router-01' | length }}` → `9`。

### `first` / `last`（P0）

**Template**

```jinja2
{{ interfaces | first }}
```

**Output**

```text
eth0
```

**Template**

```jinja2
{{ interfaces | last }}
```

**Output**

```text
eth2
```

（Data 同上）

### `sort`（P1）

**Template**

```jinja2
{{ ports | sort | join(', ') }}
```

**Data**

```python
{"ports": [443, 22, 8080, 80]}
```

**Output**

```text
22, 80, 443, 8080
```

反向排序：`{{ ports | sort(reverse=true) | join(', ') }}` → `8080, 443, 80, 22`

按对象属性排序：`{{ devices | sort(attribute='name') | map(attribute='name') | join(', ') }}`，数据 `[{"name": "sw3"}, {"name": "sw1"}, {"name": "sw2"}]` → `sw1, sw2, sw3`

### `unique`（P1）

**Template**

```jinja2
{{ vlans | unique | list }}
```

**Data**

```python
{"vlans": [10, 20, 10, 30, 20]}
```

**Output**

```text
[10, 20, 30]
```

**注意事项：** `unique` 返回的是**惰性迭代器**（generator），在 `{{ }}` 里直接打印会显示类似 `<generator object ...>`，必须配合 `| list`（或放进 `{% for %}` 里消费）才能拿到实际内容。

### `reverse`（P1）

**Template**

```jinja2
{{ interfaces | reverse | list }}
```

**Data**

```python
{"interfaces": ["eth0", "eth1", "eth2"]}
```

**Output**

```text
['eth2', 'eth1', 'eth0']
```

对字符串：`{{ 'router' | reverse }}` → `retuor`（字符串场景不需要 `| list`，因为 `reverse` 对字符串直接返回字符串）。

### `int` / `float` / `string`（P0）

**Template**

```jinja2
{{ mtu | int }}
```

**Data**

```python
{"mtu": "1500"}
```

**Output**

```text
1500
```

转换失败时可以给默认值：`{{ mtu | int(0) }}`，`{"mtu": "not-a-number"}` → `0`（不会抛异常）。

`float`：`{{ cpu_usage | float }}`，`{"cpu_usage": "12.5"}` → `12.5`

`string`：把非字符串转成字符串，常用于和 `~` 配合：`{{ (vlan_id | string) ~ '-tag' }}`，`{"vlan_id": 100}` → `100-tag`

### `round` / `abs`（P1）

**Template**

```jinja2
{{ 12.3456 | round(2) }}
```

**Output**

```text
12.35
```

**Template**

```jinja2
{{ 12.1 | round(0, 'ceil') }}
```

**Output**

```text
13.0
```

**Template**

```jinja2
{{ delta | abs }}
```

**Data**

```python
{"delta": -8}
```

**Output**

```text
8
```

### `list`（P1，用于物化迭代器）

**Template**

```jinja2
{{ 'abc' | list }}
```

**Output**

```text
['a', 'b', 'c']
```

**典型用途：配合会返回迭代器的过滤器**（`map`、`select`、`unique` 等）：

**Template**

```jinja2
{{ (numbers | map('int') | list) }}
```

**Data**

```python
{"numbers": ["1", "2", "3"]}
```

**Output**

```text
[1, 2, 3]
```

### `dictsort` 与 `items`（P2，关于 `dict` 过滤器的重要澄清）

> **重要澄清：** Jinja 内置过滤器里**并没有名叫 `dict` 的过滤器**（截至 3.1.x）。如果你需要"遍历字典的键值对"，应该用：
> - `dictsort`：按 key 排序后返回 `(key, value)` 列表；
> - `items`（Jinja 3.1 新增）：不排序，按插入顺序返回 `(key, value)` 列表，相当于 Python 的 `dict.items()`。

**Template**

```jinja2
{% for key, value in config | dictsort %}{{ key }}={{ value }} {% endfor %}
```

**Data**

```python
{"config": {"mtu": 1500, "enabled": True, "name": "eth0"}}
```

**Output**

```text
enabled=True mtu=1500 name=eth0 
```

**Template**

```jinja2
{% for key, value in config | items %}{{ key }}={{ value }} {% endfor %}
```

**Data**

```python
{"config": {"name": "eth0", "mtu": 1500}}
```

**Output**

```text
name=eth0 mtu=1500 
```

可以看到 `dictsort` 按 key 字母序重排了（`enabled` < `mtu` < `name`），而 `items` 保留了 Python dict 的原始插入顺序。

### `map`（P0）

**Template**

```jinja2
{{ devices | map(attribute='name') | list }}
```

**Data**

```python
{"devices": [{"name": "sw1"}, {"name": "sw2"}]}
```

**Output**

```text
['sw1', 'sw2']
```

也可以传一个 filter 名字，对每个元素应用该 filter：

**Template**

```jinja2
{{ names | map('upper') | list }}
```

**Data**

```python
{"names": ["eth0", "eth1"]}
```

**Output**

```text
['ETH0', 'ETH1']
```

### `select` / `reject`（P1）

**Template**

```jinja2
{{ numbers | select('odd') | list }}
```

**Data**

```python
{"numbers": [1, 2, 3, 4, 5, 6]}
```

**Output**

```text
[1, 3, 5]
```

**Template**

```jinja2
{{ numbers | reject('odd') | list }}
```

**Output**

```text
[2, 4, 6]
```

`select` 保留满足 test 的元素，`reject` 保留**不**满足 test 的元素——两者互补。

### `selectattr` / `rejectattr`（P0，对象列表筛选的核心工具）

**Template**

```jinja2
{% for d in devices | selectattr('enabled') | list %}{{ d.name }} {% endfor %}
```

**Data**

```python
{
    "devices": [
        {"name": "eth0", "enabled": True},
        {"name": "eth1", "enabled": False},
        {"name": "eth2", "enabled": True},
    ]
}
```

**Output**

```text
eth0 eth2 
```

**Template**

```jinja2
{% for d in devices | rejectattr('enabled') | list %}{{ d.name }} {% endfor %}
```

**Output**

```text
eth1 
```

配合 test 做更精确的筛选（`equalto` 是最常用的组合）：

**Template**

```jinja2
{{ devices | selectattr('mtu', 'equalto', 1500) | map(attribute='name') | list }}
```

**Data**

```python
{"devices": [{"name": "eth0", "mtu": 1500}, {"name": "eth1", "mtu": 9000}]}
```

**Output**

```text
['eth0']
```

### `tojson`（P1，生成前端数据/调试用）

**Template**

```jinja2
{{ config | tojson }}
```

**Data**

```python
{"config": {"name": "eth0", "mtu": 1500, "enabled": True}}
```

**Output**

```text
{"enabled": true, "mtu": 1500, "name": "eth0"}
```

常用于在 HTML 模板里把 Python 数据安全地嵌入 `<script>` 标签供前端 JS 使用，或者调试时快速看清一个复杂对象的结构。

### `escape` / `e` 与 `safe`（P1，与 `autoescape` 配合理解）

**Template**

```jinja2
{{ raw | escape }}
```

**Data**

```python
{"raw": "<b>eth0</b>"}
```

**Output（`autoescape=False` 的 Environment 下）**

```text
&lt;b&gt;eth0&lt;/b&gt;
```

`e` 是 `escape` 的别名，效果完全一致。

**当 Environment 开启 `autoescape=True` 时**，`{{ }}` 默认就会自动转义，不需要手动加 `| escape`：

**Template**

```jinja2
{{ raw }}
```

**Output（`autoescape=True`）**

```text
&lt;b&gt;eth0&lt;/b&gt;
```

如果你确实需要在 `autoescape=True` 的环境里输出"不转义的原始 HTML"（比如这段内容是你自己生成、确认安全的），用 `safe` 显式放行：

**Template**

```jinja2
{{ raw | safe }}
```

**Output**

```text
<b>eth0</b>
```

**注意事项：** `safe` 会跳过转义，**绝不要**对用户输入直接用 `safe`，否则存在 XSS 风险（第 9、11 节会详细展开）。

## 5.3 过滤器组合案例

#### 案例 A：字符串清理

**Template**

```jinja2
{{ hostname | trim | lower }}
```

**Data**

```python
{"hostname": "  Router-01  "}
```

**Output**

```text
router-01
```

#### 案例 B：列表格式化

**Template**

```jinja2
{{ interfaces | join(', ') }}
```

**Data**

```python
{"interfaces": ["eth0", "eth1", "eth2"]}
```

**Output**

```text
eth0, eth1, eth2
```

#### 案例 C：对象列表筛选

**Template**

```jinja2
{% for device in devices | selectattr('enabled') | list %}
{{ device.name }}
{% endfor %}
```

**Data**

```python
{
    "devices": [
        {"name": "eth0", "enabled": True},
        {"name": "eth1", "enabled": False},
        {"name": "eth2", "enabled": True},
    ]
}
```

**Output**

```text
eth0
eth2
```

#### 案例 D：缺失字段默认值

**Template**

```jinja2
{{ device.description | default('No description') }}
```

**Data**

```python
{"device": {"name": "eth0"}}
```

**Output**

```text
No description
```

---

# 06. Tests

验证脚本：[designer_guide_examples/tests_demo.py](designer_guide_examples/tests_demo.py)

## 6.1 Filter 和 Test 的区别

```text
Filter:
Value -> Transform -> New Value

Test:
Value -> Check Condition -> Boolean
```

语法上也不同：Filter 用 `|` 连接（`value | upper`），Test 用 `is` 连接（`value is defined`），并且 Test 几乎总是出现在 `{% if %}` 里做分支判断。

## 6.2 用同一份「端口配置」数据集贯穿所有测试

**Template（核心案例，必须记住）**

```jinja2
{% if port is defined and port is not none %}
port: {{ port }}
{% else %}
# port is not configured
{% endif %}
```

**Data 1：字段完全缺失**

```python
{}
```

**Output 1**

```text
# port is not configured
```

**Data 2：字段存在但值是 `None`**

```python
{"port": None}
```

**Output 2**

```text
# port is not configured
```

**Data 3：字段有有效值**

```python
{"port": 8080}
```

**Output 3**

```text
port: 8080
```

**Mental Model**

```text
                 port 字段缺失 ──┐
                                 ├──> # port is not configured
          port = None ──────────┘

          port = 8080 ────────────> port: 8080
```

**关键区分：**

- `defined` 检查的是"这个变量/键在 Context 或父作用域里有没有被赋值"。
- `none` 检查的是"值是不是 Python 的 `None`"。
- 一个字段"缺失"（没传）和一个字段"显式为 `None`"是两种不同的状态，但很多时候业务上都应该当作"未配置"处理——所以上面的核心模式是 `is defined and is not none` **两个条件都要判断**，缺一不可。
- `default` 是**过滤器**（转换值），`defined` 是**测试**（判断状态），两者解决的问题不同，不能互相替代：`port | default(80)` 只在 `port` 未定义时生效，如果 `port` 被显式设置成 `None`，`default` 过滤器**不会**介入（因为 `None` 已经是"已定义"的值）。

## 6.3 其余常用 Test 一览

| Test | Template | Data | Output | 说明 |
|---|---|---|---|---|
| `is defined` | `{{ port is defined }}` | `{"port": 22}` | `True` | 字段已定义 |
| `is undefined` | `{{ port is undefined }}` | `{}` | `True` | 字段未定义 |
| `is string` | `{{ name is string }}` | `{"name": "eth0"}` | `True` | 类型是字符串 |
| `is number` | `{{ mtu is number }}` | `{"mtu": 1500}` | `True` | 是数字（int 或 float） |
| `is integer` | `{{ mtu is integer }}` | `{"mtu": 1500}` | `True` | 是整数 |
| `is float` | `{{ cpu is float }}` | `{"cpu": 12.5}` | `True` | 是浮点数 |
| `is boolean` | `{{ enabled is boolean }}` | `{"enabled": True}` | `True` | 是布尔值 |
| `is mapping` | `{{ config is mapping }}` | `{"config": {"mtu": 1500}}` | `True` | 是字典 |
| `is sequence` | `{{ interfaces is sequence }}` | `{"interfaces": ["eth0", "eth1"]}` | `True` | 是序列（list/tuple/str 等） |
| `is iterable` | `{{ interfaces is iterable }}` | 同上 | `True` | 可迭代 |
| `is odd` | `{{ 3 is odd }}` | — | `True` | 奇数 |
| `is even` | `{{ 4 is even }}` | — | `True` | 偶数 |
| `is divisibleby(n)` | `{{ 9 is divisibleby(3) }}` | — | `True` | 能被 n 整除 |
| `is divisibleby(n)` | `{{ 10 is divisibleby(3) }}` | — | `False` | 不能整除 |
| `is sameas(x)` | `{{ enabled is sameas true }}` | `{"enabled": True}` | `True` | 与 `True` 是同一对象 |
| `is sameas(x)` | `{{ enabled is sameas true }}` | `{"enabled": 1}` | `False` | `1` 和 `True` 不是同一对象，即使 `1 == True` |

**`is sameas` 的典型用途：** 区分"值相等"和"身份相同"。在 Python 里 `1 == True` 成立，但 `1 is True` 不成立；`is sameas` 对应的正是 Python 的 `is`，适合用来精确判断一个值"确实是布尔 `True`/`False` 本身"，而不是碰巧相等的 `0`/`1`。日常配置生成中不算高频，但在处理"字段到底是布尔类型还是数字类型"的歧义数据时很有用。

**不能仅依赖默认 Undefined 行为处理所有缺失字段：** 默认 `Undefined` 会让 `{{ missing }}` 静默渲染成空字符串，`missing | upper` 这类操作也往往不报错而是返回奇怪的空值，容易把"数据缺失"的问题一路带到最终产物里才被发现。只要是"字段缺了就必须明确处理"的场景（生成关键配置、CI 产物），都应该显式用 `is defined`/`is none` 判断，或者直接把 Environment 切到 `StrictUndefined`（见第 9 节）。

---

# 07. Whitespace Control

验证脚本：[designer_guide_examples/whitespace_demo.py](designer_guide_examples/whitespace_demo.py)

生成 YAML / XML / CLI 命令这类对格式敏感的文本时，空白控制不是"锦上添花"，而是**决定输出是否合法**的关键。

## 7.1 语法位置

| 写法 | 含义 | 合法位置 |
|---|---|---|
| `{%- ... %}` | 去掉该标签**前面**的空白（含换行） | 语句标签开头，如 `{%- if %}` |
| `{% ... -%}` | 去掉该标签**后面**的空白（含换行） | 语句标签结尾，如 `{% if ... -%}` |
| `{{- ... }}` | 去掉表达式标签**前面**的空白 | 表达式标签开头 |
| `{{ ... -}}` | 去掉表达式标签**后面**的空白 | 表达式标签结尾 |
| `{%- ... -%}` | 前后空白都去掉 | 两端同时需要清理时 |

这些符号只能写在标签的**最内侧**（紧贴 `{%`/`%}`/`{{`/`}}`），不能随意拼接到别的位置。

## 7.2 对照：普通循环 vs 带空白控制的循环

**Data（下面两个示例共用）**

```python
{
    "interfaces": [
        {"name": "eth0"},
        {"name": "eth1"},
        {"name": "eth2"},
    ]
}
```

**Template（普通写法，默认 Environment，不开 `trim_blocks`/`lstrip_blocks`）**

```jinja2
interfaces:
{% for interface in interfaces %}
  - name: {{ interface.name }}
{% endfor %}
```

**Output（用 `repr()` 精确展示）**

```text
'interfaces:\n\n  - name: eth0\n\n  - name: eth1\n\n  - name: eth2\n'
```

渲染成文本直接看是这样（注意每个 `- name` 前面多了一个空行）：

```text
interfaces:

  - name: eth0

  - name: eth1

  - name: eth2
```

**原因：** 每个 `{% for %}`/`{% endfor %}` 标签所在的那一整行里，标签本身不输出任何字符，但标签**后面的换行符**会被原样保留，这些多余的换行堆积起来就变成了空行。

**Template（加上 `{%- ... %}` 空白控制）**

```jinja2
interfaces:
{%- for interface in interfaces %}
  - name: {{ interface.name }}
{%- endfor %}
```

**Output**

```text
'interfaces:\n  - name: eth0\n  - name: eth1\n  - name: eth2'
```

渲染成文本：

```text
interfaces:
  - name: eth0
  - name: eth1
  - name: eth2
```

`{%- for %}` 吃掉了 `for` 标签前面的换行，`{%- endfor %}` 吃掉了每次循环体结尾、`endfor` 标签前面的换行，刚好消除了多余空行。

## 7.3 Environment 级配置 vs 手动空白控制符

逐个模板加 `-` 很繁琐，工程中更常见的做法是在 `Environment` 层面统一配置：

| 配置项 | 作用 |
|---|---|
| `trim_blocks=True` | 自动去掉 `{% %}` 标签**后面紧跟的第一个换行符**（相当于给每个 `%}` 自动加了隐式的 `-`） |
| `lstrip_blocks=True` | 自动去掉 `{% %}` 标签**所在行、标签之前的前导空白/缩进** |
| `keep_trailing_newline` | 控制是否保留模板源文件**末尾**最后一个换行符（默认 `False`，会丢弃一个） |

**同一份"普通写法"模板**，在 `Environment(trim_blocks=True, lstrip_blocks=True)` 下渲染：

**Output**

```text
'interfaces:\n  - name: eth0\n  - name: eth1\n  - name: eth2\n'
```

渲染文本与手动加 `-` 的版本几乎一样干净，**并且不需要在每个模板里手写 `-`**——这是工程中更推荐的做法：在 `Environment` 创建时统一打开 `trim_blocks` + `lstrip_blocks`，模板作者只在少数需要"精确控制到某个字符"的地方才手动加 `-`。

## 7.4 `keep_trailing_newline` 对比

**Template**

```jinja2
Hello, {{ name }}!

```

（模板源文件以一个换行符结尾）

**Environment 默认（`keep_trailing_newline=False`）**

**Output（repr）**

```text
'Hello, Morris!'
```

**Environment（`keep_trailing_newline=True`）**

**Output（repr）**

```text
'Hello, Morris!\n'
```

**工程含义：** 如果你生成的是要被其他工具（如 `diff`、Git）比较或要求"文件以换行符结尾"的文本文件（大多数 Linux 工具链的约定），应该开启 `keep_trailing_newline=True`，否则生成的文件可能缺最后一行的换行符，导致无意义的 diff 噪音或 lint 报警。

## 7.5 `{{- ... -}}` 控制表达式周围空白

**Template**

```jinja2
A
{{- ' B' -}}
C
```

**Output（repr）**

```text
'A BC'
```

`{{-` 吃掉了 "A" 后面的换行，`-}}` 吃掉了 "B" 后面的换行，所以 "A"、" B"、"C" 被拼接成了一行 `A BC`——这也说明空白控制符**一旦用多了容易把本该保留的分隔也吃掉**，使用时要盯着实际渲染结果，而不是凭直觉猜测（第 11 节会展示一个"控制过度"的真实翻车案例）。

---

# 08. Include / Inheritance / Macro

| 机制 | 核心目的 | 典型场景 |
|---|---|---|
| `include` | 引入模板并渲染，**自动共享**调用方 context | 公共片段、配置模块 |
| `extends` + `block` | 继承公共布局，子模板覆盖差异部分 | HTML 页面、统一配置框架 |
| `macro` + `import` | 复用**带参数**的模板片段，**默认不共享**调用方 context | 重复生成相同结构（类似函数） |

## 8.1 Include

验证脚本：[designer_guide_examples/include_demo/render.py](designer_guide_examples/include_demo/render.py)

**目录结构**

```text
include_demo/
├── render.py
└── templates/
    ├── main.j2      <- 主模板，内部 {% include "banner.j2" %}
    └── banner.j2    <- 被复用的公共片段
```

**templates/banner.j2**

```jinja2
# Managed by Jinja - do not edit manually
# hostname: {{ hostname }}
```

**templates/main.j2**

```jinja2
{% include "banner.j2" %}
interfaces:
{% for interface in interfaces %}
  - name: {{ interface.name }}
    enabled: {{ interface.enabled }}
{% endfor %}
```

**render.py（关键部分）**

```python
import os
from jinja2 import Environment, FileSystemLoader

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
env = Environment(
    loader=FileSystemLoader(os.path.join(BASE_DIR, "templates")),
    trim_blocks=True,
    lstrip_blocks=True,
    keep_trailing_newline=True,
)

data = {
    "hostname": "router-01",
    "interfaces": [
        {"name": "eth0", "enabled": True},
        {"name": "eth1", "enabled": False},
    ],
}

template = env.get_template("main.j2")
print(template.render(**data))
```

**Output**

```text
# Managed by Jinja - do not edit manually
# hostname: router-01
interfaces:
  - name: eth0
    enabled: True
  - name: eth1
    enabled: False
```

**关键点：** `banner.j2` 里的 `{{ hostname }}` 能正确渲染，是因为 `{% include %}` **默认会把调用方（`main.j2`）当前的整个 context 原样传给被包含的模板**——不需要显式传参，这是 `include` 和下面 `macro`/`import` 最大的行为差异。

## 8.2 Template Inheritance

验证脚本：[designer_guide_examples/inheritance_demo/render.py](designer_guide_examples/inheritance_demo/render.py)

**目录结构**

```text
inheritance_demo/
├── render.py
└── templates/
    ├── base.j2                  <- 基础模板，定义 interfaces/services 两个 block
    ├── child_access_switch.j2   <- 只覆盖 interfaces block
    └── child_core_router.j2     <- 覆盖 interfaces block，并用 super() 扩展 services block
```

**templates/base.j2**

```jinja2
device:
  hostname: {{ hostname }}
{% block interfaces %}
  interfaces: []
{% endblock %}
{% block services %}
  services:
    - ssh
{%- endblock %}
```

**templates/child_access_switch.j2**

```jinja2
{% extends "base.j2" %}
{% block interfaces %}
  interfaces:
{% for interface in interfaces %}
    - {{ interface }}
{% endfor %}
{% endblock %}
```

**templates/child_core_router.j2**

```jinja2
{% extends "base.j2" %}
{% block interfaces %}
  interfaces:
{% for interface in interfaces %}
    - {{ interface }}
{% endfor %}
{% endblock %}
{% block services %}
{{ super() }}
    - bgp
    - ospf
{% endblock %}
```

**Data（access switch）**

```python
{"hostname": "access-sw-01", "interfaces": ["eth0", "eth1"]}
```

**Output（渲染 `child_access_switch.j2`）**

```text
device:
  hostname: access-sw-01
  interfaces:
    - eth0
    - eth1
  services:
    - ssh
```

**Data（core router）**

```python
{"hostname": "core-rt-01", "interfaces": ["eth0", "eth1", "eth2"]}
```

**Output（渲染 `child_core_router.j2`）**

```text
device:
  hostname: core-rt-01
  interfaces:
    - eth0
    - eth1
    - eth2
  services:
    - ssh
    - bgp
    - ospf
```

**说明：**

- 两个子模板都**覆盖了 `interfaces` block**，所以各自的接口列表完全来自子模板。
- `child_access_switch.j2` **没有**定义 `services` block，所以它渲染出来的 `services` 部分**完全来自 `base.j2`**（只有 `- ssh`）。
- `child_core_router.j2` 定义了 `services` block，并在里面调用了 `{{ super() }}`——这会先渲染**父模板里这个 block 的原始内容**（也就是 `- ssh`），再拼接子模板自己追加的 `- bgp`、`- ospf`。如果不调用 `super()`，子模板的 `services` block 会**完全替换**父模板的内容，`- ssh` 就会丢失。

## 8.3 Macro

验证脚本：[designer_guide_examples/macro_demo/render.py](designer_guide_examples/macro_demo/render.py)

**目录结构**

```text
macro_demo/
├── render.py
└── templates/
    ├── macros.j2                   <- 定义若干可复用的宏
    ├── use_import.j2                <- {% import ... as m %}
    ├── use_from_import.j2           <- {% from ... import name1, name2 %}
    └── use_import_with_context.j2   <- {% import ... as m with context %}
```

**templates/macros.j2**

```jinja2
{% macro banner() %}
# Managed by Jinja - do not edit manually
{%- endmacro %}

{% macro interface_line(name, enabled) %}
  - name: {{ name }}
    enabled: {{ enabled }}
{%- endmacro %}

{% macro interface_line_with_default(name, enabled=True, mtu=1500) %}
  - name: {{ name }}
    enabled: {{ enabled }}
    mtu: {{ mtu }}
{%- endmacro %}

{% macro hostname_line() %}
hostname: {{ hostname | default('(no context)') }}
{%- endmacro %}
```

**templates/use_import.j2**

```jinja2
{% import "macros.j2" as m %}
{{ m.banner() }}
{{ m.interface_line('eth0', true) }}
{{ m.interface_line_with_default('eth1') }}
{{ m.interface_line_with_default('eth2', enabled=false, mtu=9000) }}
{{ m.hostname_line() }}
```

**Output（`data = {"hostname": "router-01"}`）**

```text
# Managed by Jinja - do not edit manually
  - name: eth0
    enabled: True
  - name: eth1
    enabled: True
    mtu: 1500
  - name: eth2
    enabled: False
    mtu: 9000
hostname: (no context)
```

**templates/use_from_import.j2**

```jinja2
{% from "macros.j2" import banner, interface_line %}
{{ banner() }}
{{ interface_line('eth0', true) }}
```

**Output**

```text
# Managed by Jinja - do not edit manually
  - name: eth0
    enabled: True
```

**关键对比：无参数宏 / 带参数宏 / 带默认参数的宏**

- `banner()`：无参数，直接返回固定文本片段。
- `interface_line(name, enabled)`：必填参数，调用时必须全部提供。
- `interface_line_with_default(name, enabled=True, mtu=1500)`：带默认参数，调用时可以只传 `name`（如 `m.interface_line_with_default('eth1')`），也可以用关键字参数覆盖默认值（如 `m.interface_line_with_default('eth2', enabled=false, mtu=9000)`）。

### `include` 与 `import` 的上下文行为差异（必须明确区分）

上面 `use_import.j2` 里调用 `m.hostname_line()` 输出的是 `hostname: (no context)`，**即使**渲染时传了 `hostname="router-01"`——因为 `{% import %}` **默认不会**把调用方的 context 传给被导入的宏。这与 `include` 的"自动共享 context"行为完全相反。

如果确实需要让导入的宏访问调用方的变量，要显式加 `with context`：

**templates/use_import_with_context.j2**

```jinja2
{% import "macros.j2" as m with context %}
{{ m.hostname_line() }}
```

**Output（同样 `data = {"hostname": "router-01"}`）**

```text
hostname: router-01
```

**工程建议：** 宏更像"函数"——所有它需要的数据应该**显式作为参数传入**（如 `interface_line(name, enabled)`），而不是依赖隐式 context。只有在极少数"宏需要访问大量全局配置"的场景才考虑 `with context`，否则会让宏的行为变得不透明、难以单独测试。`include` 则更像"文本拼接"，天然共享上下文更符合直觉。不要把两者的上下文行为混为一谈。

---

# 09. Python Integration

验证脚本目录：[designer_guide_examples/python_integration/](designer_guide_examples/python_integration/)

## 9.1 核心概念

- **`Environment`**：集中管理"怎么渲染"的所有配置——模板从哪加载（`loader`）、空白怎么处理、Undefined 策略、是否自动转义、自定义的 filter/test/global。一个应用通常只需要一个（或几个用途不同的）`Environment` 实例，不需要每次渲染都新建。
- **`from_string()` vs 从文件加载**：`env.from_string(text)` 适合模板很短、是程序内生成的字符串（比如一行 CLI 命令模板）；真实项目里模板通常更适合放在独立的 `.j2` 文件里，用 `FileSystemLoader` 按名字加载，这样模板可以被非 Python 开发者独立维护、版本控制里能看到清晰的 diff。
- **`render()` 如何接收 Context**：可以用关键字参数 `template.render(name="x", port=80)`，也可以传一个 dict 并展开 `template.render(**data)`，两者等价；也支持 `template.render(data)` 直接传一个 dict（不展开）作为 context 的补充来源。
- **`StrictUndefined`**：把"访问未定义变量/属性"从"静默返回空字符串"改成"立刻抛出 `UndefinedError`"，是在开发/CI 阶段揪出拼写错误、字段遗漏的核心工具。
- **`autoescape`**：决定 `{{ }}` 输出时要不要自动做 HTML 转义。生成 HTML/XML 片段时应该开启，生成纯文本/CLI/大多数 YAML 配置时通常不需要（也不应该，否则 `<`、`&` 等字符会被意外转义成 `&lt;`、`&amp;`）。

## 9.2 案例 1：`from_string()` 渲染字符串

**目录结构（Template 和 Data 都保存为独立文件）**

```text
python_integration/
├── from_string_demo.py
├── from_string_demo.j2     <- 模板文本
└── from_string_demo.json   <- 渲染用的数据
```

**from_string_demo.j2**

```jinja2
device={{ name }}, port={{ port }}
```

**from_string_demo.json**

```json
{
    "name": "router-01",
    "port": 8080
}
```

**Python**

```python
import json
import os
from jinja2 import Environment, StrictUndefined

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

env = Environment(
    undefined=StrictUndefined,
    autoescape=False,
)

with open(os.path.join(BASE_DIR, "from_string_demo.j2"), encoding="utf-8") as f:
    template_source = f.read()

with open(os.path.join(BASE_DIR, "from_string_demo.json"), encoding="utf-8") as f:
    data = json.load(f)

template = env.from_string(template_source)
output = template.render(**data)

print(output)
```

**Output**

```text
device=router-01, port=8080
```

**说明：** `from_string()` 接受的是**任意来源的字符串**（文件、数据库、网络都可以），这里把模板文本放进 `.j2` 文件只是为了方便单独维护和复用，并不等同于下面 9.3 节用 `FileSystemLoader` 按名字加载模板的机制——两者的区别在于"模板从哪来"，而不是"能不能用文件"。

## 9.3 案例 2：`FileSystemLoader` 从目录加载模板

**目录结构（Template 和 Data 都保存为独立文件）**

```text
python_integration/
├── filesystemloader_demo.py
├── filesystemloader_demo.json   <- 渲染用的数据
└── fsloader_templates/
    └── device.j2                <- Jinja 模板
```

**fsloader_templates/device.j2**

```jinja2
hostname: {{ hostname }}
model: {{ model | default('unknown') }}
```

**filesystemloader_demo.json**

```json
{
    "hostname": "router-01",
    "model": "ISR4451"
}
```

**Python**

```python
import json
import os
from jinja2 import Environment, FileSystemLoader

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
env = Environment(
    loader=FileSystemLoader(os.path.join(BASE_DIR, "fsloader_templates")),
    trim_blocks=True,
    lstrip_blocks=True,
)

with open(os.path.join(BASE_DIR, "filesystemloader_demo.json"), encoding="utf-8") as f:
    data = json.load(f)

template = env.get_template("device.j2")
output = template.render(**data)
print(output)
```

**Output**

```text
hostname: router-01
model: ISR4451
```

## 9.4 案例 3：将渲染结果写入 YAML 文件

**目录结构（Template 和 Data 都保存为独立文件）**

```text
python_integration/
├── render_to_yaml_demo.py
├── render_to_yaml_demo.j2     <- Jinja 模板
└── render_to_yaml_demo.json   <- 渲染用的数据
```

**render_to_yaml_demo.j2**

```jinja2
hostname: {{ hostname }}
interfaces:
{% for interface in interfaces %}
  - name: {{ interface.name }}
    enabled: {{ interface.enabled }}
{% endfor %}
```

**render_to_yaml_demo.json**

```json
{
    "hostname": "router-01",
    "interfaces": [
        {"name": "eth0", "enabled": true},
        {"name": "eth1", "enabled": false}
    ]
}
```

**Python**

```python
import json
import os
from jinja2 import Environment, FileSystemLoader

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
env = Environment(
    loader=FileSystemLoader(BASE_DIR),
    trim_blocks=True,
    lstrip_blocks=True,
    keep_trailing_newline=True,
)

with open(os.path.join(BASE_DIR, "render_to_yaml_demo.json"), encoding="utf-8") as f:
    data = json.load(f)

template = env.get_template("render_to_yaml_demo.j2")
output = template.render(**data)

with open(os.path.join(BASE_DIR, "generated_device.yaml"), "w", encoding="utf-8") as f:
    f.write(output)
```

**生成的 `generated_device.yaml` 内容（Output）**

```yaml
hostname: router-01
interfaces:
  - name: eth0
    enabled: True
  - name: eth1
    enabled: False
```

## 9.5 案例 4：用 `StrictUndefined` 捕获拼写错误

**目录结构（Template 和 Data 都保存为独立文件）**

```text
python_integration/
├── strict_undefined_demo.py
├── strict_undefined_demo.json                      <- 渲染用的数据（只提供了正确的 hostname）
└── strict_undefined_demo_templates/
    ├── default_typo.j2                             <- hostname=[{{ hostnmae }}]（故意拼错）
    └── strict_typo.j2                               <- hostname={{ hostnmae }}
```

**strict_undefined_demo_templates/default_typo.j2**

```jinja2
hostname=[{{ hostnmae }}]
```

**strict_undefined_demo_templates/strict_typo.j2**

```jinja2
hostname={{ hostnmae }}
```

**strict_undefined_demo.json**

```json
{
    "hostname": "router-01"
}
```

**Python**

```python
import json
import os
from jinja2 import Environment, FileSystemLoader, StrictUndefined, UndefinedError

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
TEMPLATES_DIR = os.path.join(BASE_DIR, "strict_undefined_demo_templates")

with open(os.path.join(BASE_DIR, "strict_undefined_demo.json"), encoding="utf-8") as f:
    data = json.load(f)

# 默认 Undefined：模板里把 hostname 拼成了 hostnmae，
# Python 端只传了正确的 hostname，但默认 Undefined 会静默把 {{ hostnmae }} 渲染成空字符串
env_default = Environment(loader=FileSystemLoader(TEMPLATES_DIR))
tmpl_default = env_default.get_template("default_typo.j2")
print(tmpl_default.render(**data))

# StrictUndefined：同样的拼写错误会在渲染时直接抛出 UndefinedError
env_strict = Environment(loader=FileSystemLoader(TEMPLATES_DIR), undefined=StrictUndefined)
tmpl_strict = env_strict.get_template("strict_typo.j2")
try:
    tmpl_strict.render(**data)
except UndefinedError as exc:
    print(f"UndefinedError: {exc}")
```

**Output**

```text
hostname=[]
UndefinedError: 'hostnmae' is undefined
```

**工程建议：** 开发环境和 CI 流水线里生成配置时，优先用 `StrictUndefined`；只有在"确实希望缺失字段优雅降级"的场景（比如面向最终用户的可选参数模板）才用默认 `Undefined` 并配合显式的 `default`/`is defined` 处理。

## 9.6 案例 5：对 HTML 输出正确配置自动转义

**目录结构（Template 和 Data 都保存为独立文件）**

```text
python_integration/
├── autoescape_html_demo.py
├── autoescape_html_demo.json                       <- 渲染用的数据（comment 模拟用户输入）
└── autoescape_html_demo_templates/
    ├── comment.j2                                   <- <p>Comment: {{ comment }}</p>
    └── hostname_text.j2                              <- hostname={{ hostname }}
```

**autoescape_html_demo_templates/comment.j2**

```jinja2
<p>Comment: {{ comment }}</p>
```

**autoescape_html_demo.json**

```json
{
    "comment": "<script>alert('xss')</script>",
    "hostname": "router-01"
}
```

**Python**

```python
import json
import os
from jinja2 import Environment, FileSystemLoader

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
TEMPLATES_DIR = os.path.join(BASE_DIR, "autoescape_html_demo_templates")

with open(os.path.join(BASE_DIR, "autoescape_html_demo.json"), encoding="utf-8") as f:
    data = json.load(f)

# 正确：autoescape=True，用户输入会被自动转义，避免 XSS
env_html = Environment(loader=FileSystemLoader(TEMPLATES_DIR), autoescape=True)
html_template = env_html.get_template("comment.j2")
print(html_template.render(comment=data["comment"]))

# 危险：autoescape=False 时，用户输入被原样注入
env_html_unsafe = Environment(loader=FileSystemLoader(TEMPLATES_DIR), autoescape=False)
html_template_unsafe = env_html_unsafe.get_template("comment.j2")
print(html_template_unsafe.render(comment=data["comment"]))
```

**Output**

```text
<p>Comment: &lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;</p>
<p>Comment: <script>alert('xss')</script></p>
```

**重要提醒：** `autoescape=False` 适合纯文本/配置文件类模板（这类场景里根本不存在"HTML 注入"问题），**但它绝不应该被当作 HTML 模板的默认安全配置**。只要模板最终会被浏览器当 HTML 解析，并且里面嵌入了任何"外部可控"的数据（用户输入、第三方 API 返回值等），就必须开启 `autoescape=True`（或用 `jinja2.select_autoescape()` 按文件扩展名自动判断），否则就是一个可被利用的 XSS 漏洞。

---

# 10. Six Complete Engineering Examples

验证脚本：[designer_guide_examples/case1_network_config.py](designer_guide_examples/case1_network_config.py) ~ [case6_undefined_debug.py](designer_guide_examples/case6_undefined_debug.py)

## Case 1：生成网络设备配置

```text
Problem
   |
   v
需要根据设备接口列表生成 CLI 风格配置，
只输出 enabled=True 的接口，mtu 缺失时用默认值 1500
```

Template 和 Data 都保存为独立文件：`case1_network_config.j2` + `case1_network_config.json`。

**Input Data（case1_network_config.json）**

```json
{
    "hostname": "router-01",
    "interfaces": [
        {"name": "eth0", "enabled": true, "mtu": 1500},
        {"name": "eth1", "enabled": false, "mtu": 9000}
    ]
}
```

**Template（case1_network_config.j2）**

```jinja2
hostname {{ hostname }}
{% for interface in interfaces %}
{% if interface.enabled %}
interface {{ interface.name }}
 mtu {{ interface.mtu | default(1500) }}
 no shutdown
{% endif %}
{% endfor %}
```

**Rendering（Python，`Environment(trim_blocks=True, lstrip_blocks=True, keep_trailing_newline=True)`）**

```python
with open("case1_network_config.json", encoding="utf-8") as f:
    config = json.load(f)

template = env.get_template("case1_network_config.j2")
output = template.render(**config)
print(output)
```

**Exact Output**

```text
hostname router-01
interface eth0
 mtu 1500
 no shutdown
```

**Engineering Explanation：** `eth1` 因为 `enabled=False` 被整体跳过，不会在配置里留下任何痕迹（不是注释掉，而是彻底不输出）；`eth0` 的 `mtu` 字段即使缺失也会用 `default(1500)` 兜底。`trim_blocks + lstrip_blocks` 保证了 `{% if %}`/`{% for %}` 那些控制行不会在输出里留下空行。

## Case 2：生成 YAML 配置

```text
Problem
   |
   v
根据服务列表生成合法 YAML，需要处理可选字段（port）
和可选子列表（tags），空列表要显式输出 []
```

Template 和 Data 都保存为独立文件：`case2_yaml_config.j2` + `case2_yaml_config.json`。

**Input Data（case2_yaml_config.json，一个文件里放了 `with_services`/`no_services` 两组数据）**

```json
{
    "with_services": {
        "hostname": "app-server-01",
        "services": [
            {"name": "web", "port": 443, "tags": ["frontend", "public"]},
            {"name": "worker", "tags": []}
        ]
    },
    "no_services": {
        "hostname": "app-server-02",
        "services": []
    }
}
```

**Template（case2_yaml_config.j2）**

```jinja2
hostname: {{ hostname }}
services:
{% for service in services %}
  - name: {{ service.name }}
    port: {{ service.port | default('N/A') }}
{% if service.tags %}
    tags:
{% for tag in service.tags %}
      - {{ tag }}
{% endfor %}
{% else %}
    tags: []
{% endif %}
{% endfor %}
```

**Exact Output**

```yaml
hostname: app-server-01
services:
  - name: web
    port: 443
    tags:
      - frontend
      - public
  - name: worker
    port: N/A
    tags: []
```

**空列表场景（`services: []`）的 Output：**

```yaml
hostname: app-server-02
services:
```

**用 `yaml.safe_load()` 验证解析结果：**

```python
{'hostname': 'app-server-01', 'services': [{'name': 'web', 'port': 443, 'tags': ['frontend', 'public']}, {'name': 'worker', 'port': 'N/A', 'tags': []}]}
```

**Engineering Explanation：** `{% if service.tags %}` 利用了"空列表是假值"这一点，用同一个条件同时覆盖"字段缺失"和"字段是空列表"两种情况，避免输出 `tags: [None]` 之类的脏数据。**YAML 缩进为什么关键：** `- name:` 和 `port:`/`tags:` 必须对齐在同一缩进层级，`tags:` 下面的 `- frontend` 要比 `tags:` 多缩进 2 格才会被解析成它的子列表，缩进差 1 个空格就会导致 YAML 解析出完全不同的结构甚至直接报错（第 11 节有具体翻车案例）。

## Case 3：生成 CLI 命令

```text
Problem
   |
   v
为多个用户生成 useradd 命令，uid/shell 是可选参数，
要求每条命令独占一行，不能有多余空格或空行
```

Template 和 Data 都保存为独立文件：`case3_cli_commands.j2` + `case3_cli_commands.json`。

**Input Data（case3_cli_commands.json）**

```json
{
    "users": [
        {"name": "alice", "uid": 1001, "shell": "/bin/bash"},
        {"name": "bob", "shell": "/bin/zsh"},
        {"name": "carol"}
    ]
}
```

**Template（case3_cli_commands.j2，正确写法：用 `{% set %}` 拼好整条命令，最后统一 `{{ cmd }}` 输出一次）**

```jinja2
{% for user in users %}
{% set cmd = 'useradd ' ~ user.name %}
{% if user.uid is defined %}
{% set cmd = cmd ~ ' -u ' ~ user.uid %}
{% endif %}
{% if user.shell is defined %}
{% set cmd = cmd ~ ' -s ' ~ user.shell %}
{% endif %}
{{ cmd }}
{% endfor %}
```

**Exact Output**

```text
useradd alice -u 1001 -s /bin/bash
useradd bob -s /bin/zsh
useradd carol
```

**可读性验证（逐行拆解）**

```text
total_lines=3
1: 'useradd alice -u 1001 -s /bin/bash'
2: 'useradd bob -s /bin/zsh'
3: 'useradd carol'
```

**Engineering Explanation（重要的工程教训）：** 最初一版写法是把 `{% if %}...{% endif %}` 直接内联在同一行拼接（`useradd {{ user.name }}{% if ... %} -u {{ user.uid }}{% endif %}`），结果因为 `trim_blocks` 会吃掉**每一个** `{% %}` 标签后面的换行符——包括这一行末尾的 `{% endif %}`，导致三条命令被粘连成一整行，完全不可用。**修复方式：** 把"组装逻辑"（`{% set %}`/`{% if %}`）和"最终输出"（`{{ cmd }}`）分开，只在真正要输出内容的 `{{ cmd }}` 这一行末尾留换行——因为 `trim_blocks` 只影响 `{% %}` 语句标签，不影响 `{{ }}` 表达式标签后面的换行。这是空白控制里最容易踩的坑之一，也是为什么第 13 节建议"组装用 `{% set %}`，输出用一次 `{{ }}`"。

## Case 4：从数据列表生成摘要报告

```text
Problem
   |
   v
给定设备列表（含在线/离线状态），需要统计数量、
分别列出在线/离线设备名
```

Template 和 Data 都保存为独立文件：`case4_summary_report.j2` + `case4_summary_report.json`。

**Input Data（case4_summary_report.json）**

```json
{
    "devices": [
        {"name": "router-01", "status": "online"},
        {"name": "switch-02", "status": "offline"},
        {"name": "router-03", "status": "online"},
        {"name": "ap-04", "status": "offline"}
    ]
}
```

**Template（case4_summary_report.j2）**

```jinja2
Device Summary Report
======================
Total devices: {{ devices | length }}
Online devices: {{ devices | selectattr('status', 'equalto', 'online') | list | length }}
Offline devices: {{ devices | rejectattr('status', 'equalto', 'online') | list | length }}

Online device names: {{ devices | selectattr('status', 'equalto', 'online') | map(attribute='name') | join(', ') }}
Offline device names: {{ devices | rejectattr('status', 'equalto', 'online') | map(attribute='name') | join(', ') }}
```

**Exact Output**

```text
Device Summary Report
======================
Total devices: 4
Online devices: 2
Offline devices: 2

Online device names: router-01, router-03
Offline device names: switch-02, ap-04
```

**Engineering Explanation：** `selectattr('status', 'equalto', 'online')` 比 `selectattr('status') ` 更精确——后者只能判断字段是否为真值，前者能判断字段是否等于某个具体值。`length` 对"筛选后的列表"求个数，`map(attribute='name') | join(', ')` 是"对象列表 -> 逗号分隔的名字字符串"的标准组合，记住这一条流水线能解决大部分摘要类需求。

## Case 5：使用模板继承生成多种配置

```text
Problem
   |
   v
接入层交换机和核心路由器共享同样的配置骨架（hostname + 分隔符 + end），
但 interfaces/routing 的具体内容完全不同
```

Template 保存在 `case5_templates/` 目录下（已经是独立文件），Data 保存为独立文件：`case5_data.json`。

**Input Data（case5_data.json，一个文件里放了 `switch`/`router` 两组数据）**

```json
{
    "switch": {
        "hostname": "access-sw-01",
        "interfaces": ["GigabitEthernet0/1", "GigabitEthernet0/2"],
        "vlan": 10
    },
    "router": {
        "hostname": "core-rt-01",
        "interfaces": ["GigabitEthernet0/0", "GigabitEthernet0/1"],
        "ip_addresses": {
            "GigabitEthernet0/0": "10.0.0.1",
            "GigabitEthernet0/1": "10.0.1.1"
        }
    }
}
```

**base_device.j2**

```jinja2
hostname {{ hostname }}
!
{% block interfaces %}
! no interfaces defined
{% endblock %}
!
{% block routing %}
! no routing protocol configured
{% endblock %}
!
end
```

**access_switch.j2**

```jinja2
{% extends "base_device.j2" %}
{% block interfaces %}
{% for interface in interfaces %}
interface {{ interface }}
 switchport mode access
 switchport access vlan {{ vlan }}
{% endfor %}
{%- endblock %}
```

**core_router.j2**

```jinja2
{% extends "base_device.j2" %}
{% block interfaces %}
{% for interface in interfaces %}
interface {{ interface }}
 no switchport
 ip address {{ ip_addresses[interface] }} 255.255.255.0
{% endfor %}
{%- endblock %}
{% block routing %}
router ospf 1
{% for interface in interfaces %}
 network {{ ip_addresses[interface] }} 0.0.0.255 area 0
{% endfor %}
{%- endblock %}
```

**Exact Output（access_switch.j2 + switch_data）**

```text
hostname access-sw-01
!
interface GigabitEthernet0/1
 switchport mode access
 switchport access vlan 10
interface GigabitEthernet0/2
 switchport mode access
 switchport access vlan 10
!
! no routing protocol configured
!
end
```

**Exact Output（core_router.j2 + router_data）**

```text
hostname core-rt-01
!
interface GigabitEthernet0/0
 no switchport
 ip address 10.0.0.1 255.255.255.0
interface GigabitEthernet0/1
 no switchport
 ip address 10.0.1.1 255.255.255.0
!
router ospf 1
 network 10.0.0.1 0.0.0.255 area 0
 network 10.0.1.1 0.0.0.255 area 0
!
end
```

**Engineering Explanation：** `access_switch.j2` 没有覆盖 `routing` block，所以它的输出里这部分自动落回 `base_device.j2` 的默认文案（`! no routing protocol configured`）；`core_router.j2` 两个 block 都覆盖了，完全替换成自己的内容。公共的 `hostname`、`!` 分隔符、`end` 全部只在 `base_device.j2` 里写一次，两个子模板都自动继承，这正是"继承"相对"每个设备类型都复制一份完整模板"的核心价值——**公共结构改一处，所有子模板同步生效**。

## Case 6：调试缺失字段和 Undefined

```text
Problem
   |
   v
device 数据里没有 description 字段，也把 hostname 拼成了 hostnmae，
需要演示不同 Undefined 策略下这些问题会不会被发现
```

Template 和 Data 都保存为独立文件：`case6_templates/*.j2` + `case6_data.json`。

**Input Data（case6_data.json，注意：没有 `"description"` 字段）**

```json
{
    "hostname": "router-01",
    "mtu": 1500
}
```

**默认 Undefined：静默，不报错**

**Template（case6_templates/description_field.j2）**

```jinja2
description: {{ device.description }}
```

**Python（用方括号包裹渲染结果，便于在终端里看清空白边界）**

```python
env_default = Environment(loader=FileSystemLoader("case6_templates"))
template = env_default.get_template("description_field.j2")
print(f"[{template.render(device=DEVICE)}]")
```

**Output**

```text
[description: ]
```

可以看到 `description` 字段缺失时，没有任何报错，`{{ device.description }}` 直接被渲染成空字符串（方括号之间什么都没有）。

**同样的问题也会掩盖拼写错误：把 `hostname` 误拼成 `hostnmae` 一样会被默认 Undefined 悄悄吞掉**

**Template（case6_templates/hostname_typo.j2）**

```jinja2
hostname: {{ device.hostnmae }}
```

**Output**

```text
[hostname: ]
```

**StrictUndefined：立刻报错，报错信息直接指出缺失的属性名**

**Template（复用 case6_templates/description_field.j2，换一个 `StrictUndefined` 的 Environment）**

```jinja2
description: {{ device.description }}
```

**Output**

```text
UndefinedError: 'dict object' has no attribute 'description'
```

**`is defined` 做显式判断（不依赖 Undefined 默认行为）**

**Template（case6_templates/is_defined_guard.j2）**

```jinja2
{% if device.description is defined %}description: {{ device.description }}{% else %}# description not provided{% endif %}
```

**Output**

```text
# description not provided
```

**`default` 过滤器提供兜底值**

**Template（case6_templates/default_filter_fallback.j2）**

```jinja2
description: {{ device.description | default('No description provided') }}
```

**Output**

```text
description: No description provided
```

**适用于 CI / 自动化配置生成的建议模式：**

```python
def render_strict_or_report(template_name, context):
    env = Environment(loader=FileSystemLoader("case6_templates"), undefined=StrictUndefined)
    tmpl = env.get_template(template_name)
    try:
        return True, tmpl.render(**context)
    except UndefinedError as exc:
        return False, str(exc)

ok, result = render_strict_or_report("hostname_typo.j2", {"device": DEVICE})
```

**Output**

```text
ok=False, result='dict object' has no attribute 'hostnmae'
```

**Engineering Explanation：** 在 CI 流水线或自动化配置生成脚本中，建议用一个**专门的 `StrictUndefined` Environment 做"预检渲染"**——只要渲染不抛异常，就说明所有用到的字段名都拼写正确、数据齐全；拿到明确的 `UndefinedError` 之后，报错信息里会直接带上出错的属性名（如 `'hostnmae'`），比排查"为什么生成的配置里这一行是空的"要快得多。生产环境最终渲染时，可以换回默认 `Undefined` 配合显式的 `default`/`is defined` 做优雅降级——两种策略不冲突，分别用在"开发/校验阶段"和"最终生成阶段"。

---

# 11. Common Errors and Debugging

验证脚本：[designer_guide_examples/common_errors_demo.py](designer_guide_examples/common_errors_demo.py)

| 现象 | 可能原因 | 观察位置 | 修复方式 |
|---|---|---|---|
| 输出为空 | 变量未定义或值为空 | Context、Undefined 配置 | 使用 `StrictUndefined` 或显式默认值 |
| 渲染异常 | 缺少字段、属性访问失败 | 报错位置与输入结构 | 检查字段名和数据类型 |
| YAML 无法解析 | 缩进或空白错误 | 最终输出文本 | 调整模板并运行 YAML Parser |
| 输出出现空行 | Block 换行处理不当 | 渲染后的字符串 | 检查空白控制与 Environment 配置 |

## 11.1 变量名拼写错误

**Template**

```jinja2
hostname=[{{ hostnmae }}]
```

**Data**

```python
{"hostname": "router-01"}
```

**Output（默认 Undefined，不报错）**

```text
hostname=[]
```

**修复：** 用 `StrictUndefined` 跑一次预检（见 9.5/Case 6），渲染时会直接抛 `UndefinedError: 'hostnmae' is undefined`。

## 11.2 缺失字段和 Undefined

**Template**

```jinja2
mtu=[{{ device.mtu }}]
```

**Data**

```python
{"device": {"name": "eth0"}}
```

**Output**

```text
mtu=[]
```

**观察位置：** 最终文本里的空白往往不是"正常留空"，而是字段名打错或数据没传全——拿不准时直接切到 `StrictUndefined` 复现一次。

## 11.3 将 Python 表达式和 Jinja 语法混淆

**错误示范：在模板里直接用 Python 内置函数 `len()`**

**Template**

```jinja2
{{ len(items) }}
```

**Output（报错）**

```text
UndefinedError: 'len' is undefined
```

**正确写法：用 Jinja 的 `length` 过滤器**

**Template**

```jinja2
{{ items | length }}
```

**Output**

```text
3
```

**说明：** 模板的全局命名空间里**不会自动包含 Python 内置函数**（`len`、`str`、`print` 等都不可用，除非你显式通过 `render(len=len)` 或 `env.globals['len'] = len` 注入）。Jinja 提供了自己的一套等价能力（过滤器、测试、少量全局函数如 `range`），应该优先使用这些，而不是想当然地套用 Python 语法。

## 11.4 Filter 与 Test 混淆

**错误示范：把 `length` 当成 test 使用（`is length(3)` 根本不存在）**

**Template**

```jinja2
{{ items is length(3) }}
```

**Output（报错）**

```text
TemplateAssertionError: No test named 'length'.
```

**正确写法：`length` 是过滤器，要配合比较运算符**

**Template**

```jinja2
{{ (items | length) == 3 }}
```

**Output**

```text
True
```

## 11.5 `default` 对空字符串和 `0` 的处理不符合预期

**Template**

```jinja2
retries=[{{ retries | default(3) }}]
```

**Data**

```python
{"retries": 0}
```

**Output（很多人以为会是 3，实际是 0）**

```text
retries=[0]
```

**如果语义上 `0` 也应该被当成"未配置"：**

**Template**

```jinja2
retries=[{{ retries | default(3, true) }}]
```

**Output**

```text
retries=[3]
```

详见第 5 节 `default` 过滤器的完整真值表。

## 11.6 循环中的变量作用域问题

**Template**

```jinja2
{% set found = false %}
{% for x in items %}{% if x == 2 %}{% set found = true %}{% endif %}{% endfor %}
found=[{{ found }}] (WRONG: still false)
```

**Data**

```python
{"items": [1, 2, 3]}
```

**Output**

```text
found=[False] (WRONG: still false)
```

**修复：** 用 `namespace()`（`{% set ns = namespace(found=false) %}` ... `{% set ns.found = true %}`），或者更好的做法——在 Python 里用 `found = 2 in items` 算好再传给模板。详见第 3 节。

## 11.7 `include` 与 `import` 的使用场景混淆

**Template（宏文件 `err7_macro.j2`）**

```jinja2
{% macro show_hostname() %}hostname={{ hostname | default('(not visible without context)') }}{% endmacro %}
```

**Template（主模板）**

```jinja2
{% set hostname = 'router-01' %}
{% import "err7_macro.j2" as m %}
{{ m.show_hostname() }}
```

**Output**

```text
hostname=(not visible without context)
```

**原因：** `{% import %}` 默认**不共享**调用方 context，即使主模板里 `{% set hostname = 'router-01' %}` 在前，宏内部也看不到它。需要 `{% import ... with context %}` 才能让宏访问调用方变量（详见第 8.3 节），或者干脆把 `hostname` 作为参数显式传给宏。

## 11.8 输出中的多余换行或空格

**Template（默认 Environment，没开 `trim_blocks`/`lstrip_blocks`）**

```jinja2
line1
{% if true %}
line2
{% endif %}
line3
```

**Output（repr，可见问题）**

```text
'line1\n\nline2\n\nline3'
```

**修复：开启 `trim_blocks=True, lstrip_blocks=True`**

**Output（repr）**

```text
'line1\nline2\nline3'
```

## 11.9 YAML 缩进和列表格式错误

**错误示范：循环体里忘记写列表项前缀 `- `**

**Template**

```jinja2
hostname: {{ hostname }}
interfaces:
{% for interface in interfaces %}
name: {{ interface }}
{% endfor %}
```

**Data**

```python
{"hostname": "router-01", "interfaces": ["eth0", "eth1"]}
```

**Output（渲染结果，看起来"差不多"）**

```yaml
hostname: router-01
interfaces:
name: eth0
name: eth1
```

**用 `yaml.safe_load()` 解析后的真实结果（数据已经丢失！）**

```python
{'hostname': 'router-01', 'interfaces': None, 'name': 'eth1'}
```

**这是比"直接报错"更危险的情况：** YAML 不会因为出现重复的顶层 key `name` 而报错，而是**静默**用后一个值覆盖前一个值，`interfaces` 也因为下面没有合法的子节点而被解析成 `None`，`eth0` 这条数据彻底丢失，且过程中没有任何异常或警告。

**正确写法（补上 `- ` 前缀和两个空格缩进）**

```jinja2
hostname: {{ hostname }}
interfaces:
{% for interface in interfaces %}
  - name: {{ interface }}
{% endfor %}
```

**解析结果**

```python
{'hostname': 'router-01', 'interfaces': [{'name': 'eth0'}, {'name': 'eth1'}]}
```

**工程建议：** 任何生成 YAML 的流水线，**渲染完成后一定要再跑一次 `yaml.safe_load()` 做往返校验**，不要只用肉眼看"格式差不多就行"——上面的例子证明"看起来正常"和"解析结果正确"完全是两回事。

## 11.10 HTML 转义配置不正确

**Template**

```jinja2
<p>{{ comment }}</p>
```

**Data**

```python
{"comment": "<script>alert(1)</script>"}
```

**Output（`autoescape=False`，存在 XSS 风险）**

```html
<p><script>alert(1)</script></p>
```

**Output（`autoescape=True`，安全）**

```html
<p>&lt;script&gt;alert(1)&lt;/script&gt;</p>
```

## 11.11 将复杂业务逻辑放入模板

**反例：在模板里用 `{% set %}` 做加权评分和分档逻辑**

**Template**

```jinja2
{% set score = (a * 0.4 + b * 0.3 + c * 0.3) %}{% if score >= 90 %}A{% elif score >= 80 %}B{% elif score >= 60 %}C{% else %}D{% endif %}
```

**Data**

```python
{"a": 95, "b": 88, "c": 92}
```

**Output**

```text
A
```

**正例：在 Python 里算好，模板只做展示**

```python
def compute_grade(a, b, c):
    score = a * 0.4 + b * 0.3 + c * 0.3
    if score >= 90:
        return "A"
    if score >= 80:
        return "B"
    if score >= 60:
        return "C"
    return "D"
```

**Template**

```jinja2
{{ grade }}
```

**Data**

```python
{"grade": compute_grade(95, 88, 92)}
```

**Output**

```text
A
```

两者渲染结果完全一样，但正例的业务逻辑可以单独用 `pytest` 测试、可以被其他代码复用、修改时不需要碰模板文件——这才是"职责边界"应该划在的地方（呼应第 1 节）。

## 11.12 在模板中访问不存在的属性

**Template**

```jinja2
{{ device.ip_address }}
```

**Data**

```python
{"device": {"name": "eth0"}}
```

**Output（`StrictUndefined`，明确报错而不是静默留空）**

```text
UndefinedError: 'dict object' has no attribute 'ip_address'
```

在默认 Undefined 策略下这行只会渲染成空字符串，不会有任何提示——这也是为什么第 9、第 6 节反复强调：**不能仅依赖默认 Undefined 行为处理所有缺失字段**，关键字段必须用 `StrictUndefined` 或显式的 `is defined`/`default` 兜底。

---

# 12. Final Mental Model

```text
                  Python Application
                          |
                          v
                    Context Data
                          |
                          v
                    Jinja Template
                          |
           +--------------+--------------+
           |              |              |
           v              v              v
       Expressions    Statements      Comments
         {{ }}          {% %}          {# #}
           |              |
           v              v
       Variables       if / for / set
           |
           v
        Filters
           |
           v
       Rendered Text
           |
           v
    File / CLI / HTML / YAML / XML
```

一句话总结每个符号/关键字：

- `{{ ... }}`：**输出**表达式的结果，是唯一真正往最终文本里"写字"的语法。
- `{% ... %}`：**控制**模板渲染过程（分支、循环、赋值、复用），标签本身不输出任何字符。
- `{# ... #}`：模板注释，渲染时**完全被丢弃**，不出现在输出里，也不受 `autoescape` 影响。
- `|`：对值做**转换**（Filter），可以像管道一样链式调用。
- `is`：对值做**布尔判断**（Test），通常配合 `{% if %}` 使用。
- `for`：对一个序列**重复生成**内容，每次迭代有独立的局部作用域。
- `if`：根据条件**决定要不要生成**某段内容。
- `set`：在模板作用域里**创建/覆盖**一个临时变量，循环体内的 `set` 默认不会"泄漏"到循环外。
- `include`：**复用**一段模板片段，自动共享调用方的 context。
- `extends` / `block`：建立**模板继承**结构，子模板只需要写"和父模板不一样的部分"。
- `macro`：定义一段**带参数、可复用**的模板片段，行为更像函数，默认不共享调用方 context。
- **Python**：负责数据准备、复杂业务逻辑、与数据库/网络/文件系统的集成，以及最终决定渲染结果的去向。
- **Jinja**：负责把 Python 准备好的数据，按模板定义的格式，转换成最终文本。

---

# 13. Daily Reference Cheat Sheet

按日常使用频率从高到低排列，遇到问题先在这张表里定位，再去对应章节看完整的 Template+Data+Output。

| 优先级 | 语法 | 一句话记忆 |
|---|---|---|
| P0 | `{{ var }}` | 输出变量 |
| P0 | `{{ obj.attr }}` / `{{ obj["key"] }}` | 访问属性/键，同名歧义时用 `["key"]` 显式取键 |
| P0 | `{{ a ~ b }}` | 字符串拼接，不要求两边都是字符串 |
| P0 | `{{ value | default('N/A') }}` | 只处理"未定义"；要连假值一起处理加 `, true` |
| P0 | `{{ value | trim | upper }}` | 过滤器从左到右链式执行 |
| P0 | `{% if x is defined and x is not none %}` | 判断"真的有值"的标准写法 |
| P0 | `{% for item in items %}...{% endfor %}` | 遍历序列 |
| P0 | `{% set x = expr %}` | 创建模板局部变量，循环内赋值不会泄漏到外层 |
| P0 | `{# comment #}` | 注释，不出现在输出里 |
| P1 | `{% for x in xs if cond %}` | 带条件的循环，比循环内 `{% if %}` 更清晰 |
| P1 | `loop.index` / `loop.index0` | 1 开始 / 0 开始，前者给人看，后者当下标 |
| P1 | `loop.first` / `loop.last` | 处理首/尾项的特殊格式（分隔符、逗号） |
| P1 | `{% for %}...{% else %}` | 一个元素都没迭代到时的兜底分支 |
| P1 | `{%- ... -%}` / `trim_blocks` + `lstrip_blocks` | 消灭多余空行，优先在 Environment 层统一配置 |
| P1 | `{{ list | selectattr('field') | map(attribute='x') | join(', ') }}` | 对象列表筛选+取字段+拼接的黄金组合 |
| P1 | `{% include "x.j2" %}` | 复用片段，自动共享 context |
| P1 | `{% extends %}` + `{% block %}` + `{{ super() }}` | 继承公共骨架，只写差异部分 |
| P1 | `{% macro m(a, b=1) %}...{% endmacro %}` + `{% import ... as m %}` | 带参数的可复用片段，参数显式传入，不依赖 context |
| P1 | `StrictUndefined` | 开发/CI 阶段把"拼写错误"从静默变成立刻报错 |
| P2 | `namespace()` | 需要跨循环累加时用；优先考虑直接在 Python 里算好 |
| P2 | `dictsort` / `items` 过滤器 | 遍历字典用这两个，**没有 `dict` 过滤器** |
| P2 | `{% import ... with context %}` | 让导入的宏能访问调用方变量（少用） |
| P2 | `tojson` / `safe` | 嵌入 JSON 给前端用 / 显式放行不转义内容（慎用） |

**最后一条工程习惯：** 生成 YAML/JSON 这类结构化文本后，永远用对应的 Parser（`yaml.safe_load()`/`json.loads()`）回读校验一遍，而不是只看渲染结果"长得像不像"——第 11.9 节已经证明，格式错误往往不会报错，而是悄悄丢数据。
