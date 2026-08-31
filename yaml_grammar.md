# YAML 核心语法总结

YAML（YAML Ain't Markup Language）是一种**面向数据配置**的标记语言，以**缩进表示层次**，广泛用于 Kubernetes、Docker Compose、Ansible、GitHub Actions 等配置文件。

---

# 一、YAML 三大数据结构

YAML 只有三种基本数据结构：

| 数据结构 | YAML表示 | 对应语言 |
|----------|----------|----------|
| Scalar（标量） | 一个值 | string、int、bool、float、null |
| Sequence（序列） | `-` 开头 | List / Array |
| Mapping（映射） | `key: value` | Dictionary / Map |

---

## 1. Scalar（标量）

表示一个单独的值。

### 字符串

```yaml
name: Morris
city: Beijing
```

也可以加引号：

```yaml
name: "Morris"
path: '/usr/local/bin'
```

### 数字

```yaml
age: 18
price: 3.14
```

### 布尔值

```yaml
enable: true
debug: false
```

### Null

```yaml
value: null

# 或

value:
```

---

## 2. Sequence（序列）

使用 `-` 表示数组。

```yaml
languages:
  - C
  - C++
  - Rust
  - Go
```

等价于：

```text
[
  "C",
  "C++",
  "Rust",
  "Go"
]
```

数组元素可以是对象：

```yaml
users:
  - name: Tom
    age: 18

  - name: Jack
    age: 20
```

---

## 3. Mapping（映射）

键值对（Dictionary）。

```yaml
user:
  name: Morris
  age: 18
  city: Beijing
```

等价于

```json
{
  "user": {
    "name": "Morris",
    "age": 18,
    "city": "Beijing"
  }
}
```

---

# 二、三种数据结构可以任意组合

## Map → List

```yaml
servers:
  - nginx
  - mysql
  - redis
```

---

## List → Map

```yaml
users:
  - name: Tom
    age: 18

  - name: Jack
    age: 20
```

---

## Map → Map

```yaml
database:
  mysql:
    host: localhost
    port: 3306

  redis:
    host: localhost
    port: 6379
```

---

## List → List

```yaml
matrix:
  -
    - 1
    - 2

  -
    - 3
    - 4
```

---

## 多层嵌套

```yaml
company:
  name: OpenAI

  departments:
    - name: AI

      employees:
        - name: Alice
          age: 28

        - name: Bob
          age: 30
```

---

# 三、缩进规则（最重要）

YAML **没有 `{}` 和 `[]`**，完全依赖缩进表示层级。

正确：

```yaml
person:
  name: Morris
  age: 18
```

错误：

```yaml
person:
name: Morris
age: 18
```

建议统一使用 **2 个空格** 或 **4 个空格**，**不能使用 Tab**。

---

# 四、常用修饰符（特殊语法）

## 1. 注释 `#`

```yaml
# 数据库配置

host: localhost
port: 3306
```

---

## 2. 多行字符串

### `|` 保留换行（Literal Block）

```yaml
message: |
  Hello
  YAML
  World
```

结果：

```text
Hello
YAML
World
```

---

### `>` 折叠换行（Folded Block）

```yaml
message: >
  Hello
  YAML
  World
```

结果：

```text
Hello YAML World
```

---

## 3. 引号

### 双引号

支持转义字符：

```yaml
path: "C:\nProgram Files"
```

### 单引号

不会解析转义：

```yaml
path: 'C:\nProgram Files'
```

---

## 4. Anchor（锚点） `&`

定义一个对象。

```yaml
default: &server
  host: localhost
  port: 3306
```

---

## 5. Alias（引用） `*`

引用前面定义的锚点，可以引用任何类型。

```yaml
default: &server
  host: localhost
  port: 3306

mysql: *server
```

等价于：

```yaml
mysql:
  host: localhost
  port: 3306
```

---

## 6. Merge（合并） `<<`

继承并覆盖字段，只能合并 Mapping。

```yaml
default: &server
  host: localhost
  port: 3306

mysql:
  <<: *server
  database: test
```

结果：

```yaml
mysql:
  host: localhost
  port: 3306
  database: test
```

覆盖示例：

```yaml
default: &server
  host: localhost
  port: 3306

mysql:
  <<: *server
  port: 3307
```

最终：

```yaml
mysql:
  host: localhost
  port: 3307
```

Merge 可以合并多个对象:

```yaml
base: &base
  host: localhost

auth: &auth
  username: root
  password: 123

mysql:
  <<: [*base, *auth]
```

最终：

```yaml
mysql:
  host: localhost
  username: root
  password: 123
```

| 特性      | 引用 `*`     | 合并 `<<:`                     |
| ------- | ---------- | ---------------------------- |
| 作用      | 引用整个节点     | 将一个 Mapping 的键值复制到当前 Mapping |
| 是否可追加字段 | 否          | 可以                           |
| 是否可覆盖字段 | 否          | 可以                           |
| 适用于     | 任意 YAML 节点 | **只能用于 Mapping（对象）**         |


---

# 五、Flow Style（行内写法）

YAML 支持 JSON 风格的简写。

## 行内数组

```yaml
languages: [C, C++, Rust, Go]
```

---

## 行内对象

```yaml
user: {name: Morris, age: 18}
```

---

## 混合使用

```yaml
users:
  - {name: Tom, age: 18}
  - {name: Jack, age: 20}
```

---

# 六、类型自动推断

YAML 会自动识别数据类型。

```yaml
int: 100
float: 3.14
bool: true
null_value: null
string: hello
```

若希望按字符串处理，可加引号：

```yaml
version: "1.0"
zip: "00100"
```

---

# 七、核心语法速查表

| 语法 | 作用 | 示例 |
|------|------|------|
| `key: value` | 映射（Map） | `name: Morris` |
| `- item` | 序列（List） | `- Rust` |
| `#` | 注释 | `# comment` |
| `|` | 保留换行 | 多行文本 |
| `>` | 折叠换行 | 多行文本 |
| `"` | 双引号 | 支持转义 |
| `'` | 单引号 | 不解析转义 |
| `&name` | 定义锚点 | `&default` |
| `*name` | 引用锚点 | `*default` |
| `<<:` | 合并映射 | `<<: *default` |
| `[]` | 行内数组 | `[1, 2, 3]` |
| `{}` | 行内对象 | `{a: 1, b: 2}` |

---

# 八、学习口诀

> **YAML = 缩进 + 三种数据结构 + 少量修饰符**

- **Scalar（标量）**：一个值
- **Sequence（序列）**：`-`
- **Mapping（映射）**：`key: value`

核心规则：

1. **缩进决定层级**（不能使用 Tab）。
2. **Map 和 List 可以无限嵌套**。
3. **多行文本使用 `|` 或 `>`**。
4. **复用配置使用 `&`、`*`、`<<`**。
5. **支持 JSON 风格的行内写法 `{}`、`[]`**。