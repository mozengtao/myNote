# Python `json` Module 完整指南 —— 工作原理、典型示例、最佳实践与心智模式

---

# 一、JSON 在 Python 中的定位

Python 官方提供 `json` 模块用于：

> 在 **Python Object** 与 **JSON Text** 之间进行双向转换。

它实际上就是两个世界之间的一座桥梁：

```
          Python World
────────────────────────────────────

dict
list
tuple
str
int
float
bool
None

          │
          │ json.dumps()
          │ json.dump()
          ▼

==========================
      JSON Text
==========================

{
    "name":"Alice",
    "age":18,
    "skills":["Python","Go"]
}

          ▲
          │ json.loads()
          │ json.load()
          │

────────────────────────────────────
```

整个 json 模块，其实只有两个方向：

> **Python Object ⇄ JSON Text**

所以只需要记住四个 API。

---

# 二、四个最重要 API

| API | 输入 | 输出 | 用途 |
|------|------|------|------|
| json.loads() | JSON字符串 | Python对象 | 字符串解析 |
| json.load() | 文件 | Python对象 | 文件解析 |
| json.dumps() | Python对象 | JSON字符串 | 序列化 |
| json.dump() | Python对象 | 文件 | 保存JSON |

记忆口诀：

```
s -> string

load
    文件 -> object

loads
    string -> object

dump
    object -> 文件

dumps
    object -> string
```

这是 json 模块最核心的心智模型。

---

# 三、Python Object 与 JSON 类型映射

| JSON | Python |
|-------|---------|
| object | dict |
| array | list |
| string | str |
| number | int / float |
| true | True |
| false | False |
| null | None |

例如：

JSON

```json
{
    "name":"Tom",
    "age":18,
    "vip":true,
    "score":null
}
```

解析以后：

```python
{
    "name":"Tom",
    "age":18,
    "vip":True,
    "score":None
}
```

并没有产生新的 JSON Object 类型。

JSON Object 在 Python 中就是：

```
dict
```

---

# 四、工作原理

```
JSON文件
        │
        │ open()
        ▼

文本流(Text)

        │
        │ json.load()
        ▼

Python Object(dict/list...)

        │
        │ Python代码修改
        ▼

Python Object

        │
        │ json.dump()
        ▼

JSON文件
```

整个 json 模块其实就是：

> Parser（解析器）+ Serializer（序列化器）

---

# 五、典型示例（至少10个）

---

## 示例1：解析JSON字符串（loads）

```python
import json

text = '{"name":"Tom","age":18}'

obj = json.loads(text)

print(obj)
print(type(obj))
```

输出

```python
{'name': 'Tom', 'age': 18}

<class 'dict'>
```

心智模型：

```
JSON String
      │
      ▼
dict
```

---

## 示例2：解析JSON数组

```python
import json

text = '[1,2,3,4]'

numbers = json.loads(text)

print(numbers)
```

输出

```python
[1,2,3,4]
```

类型：

```
list
```

---

## 示例3：解析嵌套JSON

```python
import json

text = '''
{
    "name":"Alice",
    "address":{
        "city":"Beijing",
        "zip":100000
    }
}
'''

obj = json.loads(text)

print(obj["address"]["city"])
```

输出

```
Beijing
```

说明：

JSON Object 会递归变成 dict。

---

## 示例4：读取JSON文件（load）

假设：

config.json

```json
{
    "host":"localhost",
    "port":8080
}
```

Python

```python
import json

with open("config.json") as f:
    config = json.load(f)

print(config)
```

得到

```python
{
    "host":"localhost",
    "port":8080
}
```

工作流程：

```
File

↓

json.load()

↓

dict
```

---

## 示例5：Python对象转JSON字符串（dumps）

```python
import json

user = {
    "name":"Tom",
    "age":18
}

text = json.dumps(user)

print(text)
```

输出

```
{"name": "Tom", "age": 18}
```

注意：

返回的是：

```
str
```

不是文件。

---

## 示例6：保存JSON文件（dump）

```python
import json

data = {
    "name":"Tom",
    "age":18
}

with open("user.json","w") as f:
    json.dump(data,f)
```

生成：

```json
{"name": "Tom", "age": 18}
```

流程：

```
dict

↓

dump()

↓

JSON File
```

---

## 示例7：漂亮输出（indent）

```python
import json

user = {
    "name":"Alice",
    "age":18,
    "skills":["Python","Go"]
}

print(json.dumps(user,indent=4))
```

输出：

```json
{
    "name": "Alice",
    "age": 18,
    "skills": [
        "Python",
        "Go"
    ]
}
```

这是开发中最常用参数。

---

## 示例8：中文输出（ensure_ascii）

默认：

```python
json.dumps({"name":"张三"})
```

输出：

```
{"name":"\u5f20\u4e09"}
```

正确方式：

```python
json.dumps(
    {"name":"张三"},
    ensure_ascii=False
)
```

输出：

```json
{"name":"张三"}
```

开发中几乎都会加：

```python
ensure_ascii=False
```

---

## 示例9：排序Key（sort_keys）

```python
import json

data = {
    "c":3,
    "a":1,
    "b":2
}

print(json.dumps(data,sort_keys=True))
```

输出

```json
{"a":1,"b":2,"c":3}
```

适合：

- Git Diff
- 配置文件
- 测试

---

## 示例10：读取后修改再写回

```python
import json

with open("config.json") as f:
    config = json.load(f)

config["port"] = 9000

with open("config.json","w") as f:
    json.dump(config,f,indent=4)
```

工作流程：

```
config.json

↓

load()

↓

dict

↓

修改dict

↓

dump()

↓

config.json
```

这是开发中最常见的模式。

---

## 示例11：解析 API 返回的 JSON

```python
import requests

response = requests.get("https://example.com/api")

data = response.json()

print(data["users"])
```

等价于：

```python
json.loads(response.text)
```

心智模型：

```
HTTP

↓

JSON Text

↓

dict
```

---

## 示例12：保存复杂配置

```python
config = {
    "database":{
        "host":"localhost",
        "port":3306
    },
    "logging":{
        "level":"INFO"
    }
}

with open("config.json","w") as f:
    json.dump(
        config,
        f,
        indent=4,
        ensure_ascii=False
    )
```

适合：

- 配置文件
- 微服务
- 自动化脚本

---

# 六、序列化与反序列化

两个非常重要的概念：

```
Python Object

↓

Serialize

↓

JSON Text
```

叫做：

> Serialization（序列化）

反方向：

```
JSON Text

↓

Deserialize

↓

Python Object
```

叫做：

> Deserialization（反序列化）

所以：

```
dump(s)

就是 Serialize

load(s)

就是 Deserialize
```

---

# 七、常见参数

## indent

漂亮打印

```python
indent=4
```

---

## ensure_ascii

中文

```python
ensure_ascii=False
```

---

## sort_keys

排序

```python
sort_keys=True
```

---

## separators

压缩JSON

```python
json.dumps(
    obj,
    separators=(",",":")
)
```

输出：

```json
{"a":1,"b":2}
```

没有任何空格。

---

## default

自定义对象转换。

例如：

```python
from datetime import datetime
import json

data = {
    "time": datetime.now()
}

print(json.dumps(data, default=str))
```

否则：

```
TypeError

Object of type datetime is not JSON serializable
```

---

# 八、最佳实践

## ✅ 配置文件

始终：

```python
with open(path) as f:
    config = json.load(f)
```

不要：

```python
text = open(path).read()
json.loads(text)
```

因为：

> load() 已经帮你完成读取。

---

## ✅ 写JSON

推荐：

```python
json.dump(
    obj,
    f,
    indent=4,
    ensure_ascii=False
)
```

这是几乎所有 Python 项目的默认写法。

---

## ✅ 网络请求

```
response.text

↓

json.loads()

↓

dict
```

或直接：

```python
response.json()
```

---

## ✅ 修改配置

永远遵循：

```
Read

↓

Modify

↓

Write Back
```

不要字符串替换 JSON。

---

## ✅ 数据处理

JSON 一旦解析完成：

> **立即忘掉 JSON，把它当成普通 Python 对象（dict/list）来操作。**

例如：

不要一直想着：

```
JSON Object
```

而应该想着：

```
dict

↓

dict["users"]

↓

list

↓

for user in users
```

这是 Python 开发者最自然的思维方式。

---

# 九、JSON 模块黄金法则（心智模式）

## 心智模型一：JSON 只是文本

```
JSON ≠ Object

JSON

就是

String
```

只有经过：

```
json.loads()
```

才会变成真正可操作的 Python 对象。

---

## 心智模型二：四个 API 覆盖全部需求

```
字符串

↓

loads()

↓

对象

对象

↓

dumps()

↓

字符串


文件

↓

load()

↓

对象

对象

↓

dump()

↓

文件
```

记住：

> **s = string；没有 s = file。**

---

## 心智模型三：Python 中没有 JSON Object

解析后的 JSON：

```
Object

↓

dict

Array

↓

list
```

始终操作的是 Python 数据结构，而不是某种特殊的 JSON 类型。

---

## 心智模型四：Parser + Serializer

把 `json` 模块看成两个组件：

```
               json
                 │
     ┌───────────┴───────────┐
     │                       │
 Parser                  Serializer
(load/loads)            (dump/dumps)
     │                       │
JSON Text             Python Object
     │                       │
     ▼                       ▼
Python Object          JSON Text
```

遇到任何 JSON 相关问题时，先判断自己当前所处的世界（**JSON 文本**还是 **Python 对象**），再选择对应的 API。这种“**两种表示、四个入口**”的思维方式，比死记函数名更容易形成长期稳定的使用习惯。