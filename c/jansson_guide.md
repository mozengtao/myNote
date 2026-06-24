# Jansson JSON Library 完整指南

## 概述

Jansson 是一个强大的 C 语言 JSON 处理库，专门用于编码、解码和操作 JSON 数据。

### 官方定位
> Encode, decode and manipulate JSON data.

### 核心工作流程

```text
          JSON文本
              │
      json_loads()
              │
              ▼
      Jansson对象树
              │
    ┌─────────┼─────────┐
    │         │         │
    ▼         ▼         ▼
 object     array     value
    │         │         │
    └─────────┼─────────┘
              │
        业务处理
              │
       json_dumps()
              │
              ▼
          JSON文本
```

### 解决的问题

对于复杂的 JSON 数据：
```json
{
    "name": "Tom",
    "age": 20,
    "skills": ["C", "Go", "Rust"]
}
```

**不使用 Jansson 的传统方法：**
```c
char *buf;
strstr(...)     // 字符串查找
strtok(...)     // 字符串分割
sscanf(...)     // 格式化输入
```
这种方式代码脆弱且容易出错。

**使用 Jansson 的方法：**
```c
json_loads()           // 解析JSON文本
json_object_get()      // 获取对象成员
json_array_get()       // 获取数组元素
json_integer_value()   // 获取整数值
json_string_value()    // 获取字符串值
```

**工作流程：**
```
JSON文本 → 解析为对象树 → 按类型安全访问
```
## 核心概念

### 统一数据类型：json_t

Jansson 最重要的设计理念是**统一数据类型**：

```c
typedef struct json_t json_t;
```

**所有 JSON 数据类型都用同一个指针表示：**

```
JSON 数据类型层次结构：
JSON
│
├── object     (JSON 对象)
├── array      (JSON 数组)
├── string     (字符串)
├── integer    (整数)
├── real       (浮点数)
├── true       (布尔真)
├── false      (布尔假)
└── null       (空值)

统一表示为：json_t *
```

### 类比理解

这种设计类似于 Linux 文件系统：

```
Linux VFS 模型：           Jansson 模型：
    file *                   json_t *
        │                        │
        ├── regular file         ├── object
        ├── directory            ├── array
        ├── socket               ├── string
        └── device               └── integer
```

### 实际应用

```c
// 所有变量都是相同的类型
json_t *root;     // 根节点
json_t *obj;      // 对象
json_t *array;    // 数组
json_t *value;    // 值

// 本质上都是：json_t *
```
## JSON 对象树模型

### 示例 JSON 数据

```json
{
  "name": "Tom",
  "age": 20,
  "skills": [
    "C",
    "Go",
    "Rust"
  ]
}
```

### 逻辑树结构

```
root(object)
│
├── name ──────→ "Tom" (string)
│
├── age ───────→ 20 (integer) 
│
└── skills ────→ array
                  │
                  ├── [0] → "C" (string)
                  ├── [1] → "Go" (string)
                  └── [2] → "Rust" (string)
```

### 内存中的表示

```
json_t(root) ── type: JSON_OBJECT
    │
    ├── json_t(name) ── type: JSON_STRING, value: "Tom"
    ├── json_t(age) ─── type: JSON_INTEGER, value: 20
    └── json_t(skills) ─ type: JSON_ARRAY
            │
            ├── json_t ─ type: JSON_STRING, value: "C"
            ├── json_t ─ type: JSON_STRING, value: "Go"
            └── json_t ─ type: JSON_STRING, value: "Rust"
```

### 核心理念

**JSON = 树结构 = json_t 节点树**

每个节点都是 `json_t` 类型，通过类型字段区分具体的数据类型。
## 引用计数机制

### 核心概念

引用计数是 Jansson 最重要的内存管理机制。

### json_t 结构

```c
json_t 内部结构（概念性）：
json_t
 │
 ├── type      // 数据类型
 ├── value     // 实际数据
 └── refcount  // 引用计数
```

### 引用计数生命周期

#### 1. 对象创建
```c
json_t *obj = json_object();
```
```
obj ─────→ json_object
           refcount=1
```

#### 2. 增加引用
```c
json_incref(obj);
```
```
refcount: 1 → 2
```

#### 3. 减少引用
```c
json_decref(obj);
```
```
refcount: 2 → 1
```

#### 4. 自动释放
```c
json_decref(obj);  // 最后一次调用
```
```
refcount: 1 → 0
↓
自动调用 free()
```

### 类比理解

**类似 Linux inode 的链接计数：**

```
Linux inode:          Jansson json_t:
  link count            ref count

文件被引用：+1         对象被持有：+1
文件被删除：-1         对象被释放：-1

link count = 0        ref count = 0
      ↓                      ↓
  删除文件               释放内存
```

### 管理原则

- **获得所有权**：`refcount + 1`
- **释放所有权**：`refcount - 1` 
- **自动清理**：`refcount = 0` 时自动释放内存
## JSON 解析 API

### 从字符串解析：json_loads()

#### 函数签名
```c
json_t *json_loads(
    const char *input,    // JSON字符串
    size_t flags,         // 解析标志
    json_error_t *error   // 错误信息
);
```

#### 使用示例
```c
json_error_t error;
const char *json_text = "{\"name\":\"Tom\",\"age\":20}";

json_t *root = json_loads(json_text, 0, &error);
if (!root) {
    fprintf(stderr, "JSON解析错误：%s\n", error.text);
    return -1;
}
```

#### 工作流程
```
JSON字符串 ──→ json_loads() ──→ json_t树
```

### 从文件解析：json_load_file()

#### 函数签名
```c
json_t *json_load_file(
    const char *path,     // 文件路径
    size_t flags,         // 解析标志 
    json_error_t *error   // 错误信息
);
```

#### 使用示例
```c
json_error_t error;
json_t *root = json_load_file("config.json", 0, &error);
if (!root) {
    fprintf(stderr, "文件解析错误：%s\n", error.text);
    return -1;
}
```

#### 工作流程
```
文件 ──→ fopen() ──→ read() ──→ json_loads() ──→ json_t树
```

### 错误处理

#### json_error_t 结构
```c
typedef struct {
    int line;           // 错误行号
    int column;         // 错误列号
    int position;       // 错误位置
    char text[JSON_ERROR_TEXT_LENGTH];  // 错误描述
    char source[JSON_ERROR_SOURCE_LENGTH];  // 错误来源
} json_error_t;
```
## 类型检查 API

### 类型判断函数

| 函数 | 用途 | 示例 |
|------|------|------|
| `json_is_object(json_t *)` | 检查是否为对象 | `if (json_is_object(root)) { ... }` |
| `json_is_array(json_t *)` | 检查是否为数组 | `if (json_is_array(value)) { ... }` |
| `json_is_string(json_t *)` | 检查是否为字符串 | `if (json_is_string(value)) { ... }` |
| `json_is_integer(json_t *)` | 检查是否为整数 | `if (json_is_integer(value)) { ... }` |
| `json_is_real(json_t *)` | 检查是否为浮点数 | `if (json_is_real(value)) { ... }` |
| `json_is_true(json_t *)` | 检查是否为 true | `if (json_is_true(value)) { ... }` |
| `json_is_false(json_t *)` | 检查是否为 false | `if (json_is_false(value)) { ... }` |
| `json_is_null(json_t *)` | 检查是否为 null | `if (json_is_null(value)) { ... }` |

### 使用模式

#### 正确的方式：先检查类型，再取值

```c
json_t *value = json_object_get(root, "age");

if (json_is_integer(value)) {
    json_int_t age = json_integer_value(value);
    printf("年龄：%lld\n", age);
} else {
    fprintf(stderr, "age 字段不是整数类型\n");
}
```

#### 错误的方式：直接取值（可能导致崩溃）

```c
// 危险！如果 value 不是整数类型会出错
json_int_t age = json_integer_value(value);
```

### 安全编程模式

```
json_t * ──→ 类型检查 ──→ 类型匹配？ ──→ 取值
    │             │            │
    └─────────────┘            └─→ 错误处理
```
## Object API

### 对应的 JSON 结构
```json
{
    "name": "Tom",
    "age": 20
}
```

### 核心函数

#### 创建对象：json_object()
```c
json_t *obj = json_object();
// 创建一个空的 JSON 对象，引用计数 = 1
```

#### 获取对象成员：json_object_get()
```c
json_t *json_object_get(json_t *object, const char *key);
```

**使用示例：**
```c
json_t *name = json_object_get(obj, "name");
if (json_is_string(name)) {
    const char *name_str = json_string_value(name);
    printf("姓名：%s\n", name_str);
}
```

**访问流程：**
```
object ──→ key="name" ──→ value
```
类似于：`map["name"]`

#### 设置对象成员：json_object_set_new()
```c
int json_object_set_new(json_t *object, const char *key, json_t *value);
```

**使用示例：**
```c
json_object_set_new(obj, "age", json_integer(20));
json_object_set_new(obj, "name", json_string("Tom"));
```

**结果：**
```json
{
    "age": 20,
    "name": "Tom"
}
```

#### 删除对象成员：json_object_del()
```c
int json_object_del(json_t *object, const char *key);
```

**使用示例：**
```c
json_object_del(obj, "age");  // 删除 "age" 字段
```

#### 获取对象大小：json_object_size()
```c
size_t json_object_size(const json_t *object);
```

**使用示例：**
```c
size_t count = json_object_size(obj);
printf("对象包含 %zu 个字段\n", count);
```

### Object 遍历

```c
const char *key;
json_t *value;

json_object_foreach(root, key, value) {
    printf("键：%s\n", key);
    if (json_is_string(value)) {
        printf("值：%s\n", json_string_value(value));
    }
}
```
## Array API

### 对应的 JSON 结构
```json
[
    "C",
    "Go",
    "Rust"
]
```

### 核心函数

#### 创建数组：json_array()
```c
json_t *arr = json_array();
// 创建一个空的 JSON 数组，引用计数 = 1
```

#### 添加元素：json_array_append_new()
```c
int json_array_append_new(json_t *array, json_t *value);
```

**使用示例：**
```c
json_t *arr = json_array();
json_array_append_new(arr, json_string("C"));
json_array_append_new(arr, json_string("Go"));
json_array_append_new(arr, json_string("Rust"));
```

**结果：**
```json
["C", "Go", "Rust"]
```

#### 获取数组元素：json_array_get()
```c
json_t *json_array_get(const json_t *array, size_t index);
```

**使用示例：**
```c
json_t *item = json_array_get(arr, 0);  // 获取第一个元素
if (json_is_string(item)) {
    printf("第一个元素：%s\n", json_string_value(item));
}
```
类似于：`arr[0]`

#### 获取数组大小：json_array_size()
```c
size_t json_array_size(const json_t *array);
```

**使用示例：**
```c
size_t count = json_array_size(arr);
printf("数组包含 %zu 个元素\n", count);
```

#### 删除数组元素：json_array_remove()
```c
int json_array_remove(json_t *array, size_t index);
```

**使用示例：**
```c
json_array_remove(arr, 0);  // 删除第一个元素
```

#### 插入元素：json_array_insert_new()
```c
int json_array_insert_new(json_t *array, size_t index, json_t *value);
```

### Array 遍历

```c
size_t index;
json_t *value;

json_array_foreach(arr, index, value) {
    printf("索引 %zu: ", index);
    if (json_is_string(value)) {
        printf("%s\n", json_string_value(value));
    }
}
```

**等价于：**
```c
for (size_t i = 0; i < json_array_size(arr); i++) {
    json_t *item = json_array_get(arr, i);
    // 处理 item
}
```
## Value API

### String 类型

#### 创建字符串
```c
json_t *json_string(const char *value);
json_t *json_stringn(const char *value, size_t len);
```

**使用示例：**
```c
json_t *s = json_string("hello world");
```

#### 获取字符串值
```c
const char *json_string_value(const json_t *string);
size_t json_string_length(const json_t *string);
```

**使用示例：**
```c
if (json_is_string(s)) {
    const char *str = json_string_value(s);
    printf("字符串：%s (长度：%zu)\n", str, json_string_length(s));
}
```

### Integer 类型

#### 创建整数
```c
json_t *json_integer(json_int_t value);
```

**使用示例：**
```c
json_t *i = json_integer(42);
```

#### 获取整数值
```c
json_int_t json_integer_value(const json_t *integer);
```

**使用示例：**
```c
if (json_is_integer(i)) {
    json_int_t value = json_integer_value(i);
    printf("整数：%lld\n", value);
}
```

### Real 类型（浮点数）

#### 创建浮点数
```c
json_t *json_real(double value);
```

**使用示例：**
```c
json_t *r = json_real(3.14159);
```

#### 获取浮点数值
```c
double json_real_value(const json_t *real);
```

**使用示例：**
```c
if (json_is_real(r)) {
    double value = json_real_value(r);
    printf("浮点数：%f\n", value);
}
```

### Boolean 类型

#### 创建布尔值
```c
json_t *json_true(void);    // 创建 true
json_t *json_false(void);   // 创建 false
```

#### 检查布尔值
```c
int json_is_true(const json_t *json);
int json_is_false(const json_t *json);
int json_is_boolean(const json_t *json);
```

**使用示例：**
```c
json_t *boolean = json_true();
if (json_is_true(boolean)) {
    printf("这是 true 值\n");
}
```

### Null 类型

#### 创建 null 值
```c
json_t *json_null(void);
```

#### 检查 null 值
```c
int json_is_null(const json_t *json);
```

**使用示例：**
```c
json_t *null_value = json_null();
if (json_is_null(null_value)) {
    printf("这是 null 值\n");
}
```
## JSON 生成 API

### 生成到内存：json_dumps()

#### 函数签名
```c
char *json_dumps(const json_t *json, size_t flags);
```

#### 使用示例
```c
json_t *root = json_object();
json_object_set_new(root, "name", json_string("Tom"));
json_object_set_new(root, "age", json_integer(20));

// 生成格式化的 JSON 字符串
char *json_str = json_dumps(root, JSON_INDENT(4));
printf("生成的 JSON:\n%s\n", json_str);

free(json_str);  // 记得释放内存
json_decref(root);
```

**输出：**
```json
{
    "name": "Tom",
    "age": 20
}
```

### 输出到文件：json_dump_file()

#### 函数签名
```c
int json_dump_file(const json_t *json, const char *path, size_t flags);
```

#### 使用示例
```c
int result = json_dump_file(root, "output.json", JSON_INDENT(4));
if (result != 0) {
    fprintf(stderr, "写入文件失败\n");
}
```

### 格式化选项

| 选项 | 描述 |
|------|------|
| `JSON_INDENT(n)` | 使用 n 个空格缩进 |
| `JSON_COMPACT` | 紧凑格式，不使用空格和换行 |
| `JSON_ENSURE_ASCII` | 确保输出为 ASCII |
| `JSON_SORT_KEYS` | 按键名排序 |

### 工作流程

```
json_t树 ──→ json_dumps()/json_dump_file() ──→ JSON 文本/文件
```
## Pack/Unpack 高级 API

### 概述

Pack/Unpack API 提供了类似 `printf`/`scanf` 的格式化方式来创建和解析 JSON。

```c
// 类比
printf("%d %s", num, str);      // 格式化输出
scanf("%d %s", &num, str);      // 格式化输入

json_pack("{s:s,s:i}", ...);    // 格式化创建 JSON
json_unpack(root, "{s:s,s:i}", ...);  // 格式化解析 JSON
```

### json_pack() - 创建 JSON

#### 函数签名
```c
json_t *json_pack(const char *fmt, ...);
```

#### 格式符号

| 符号 | 类型 | 示例 |
|------|------|------|
| `s` | 字符串 | `"s", "hello"` |
| `i` | 整数 | `"i", 42` |
| `f` | 浮点数 | `"f", 3.14` |
| `b` | 布尔值 | `"b", 1` |
| `n` | null | `"n"` |
| `o` | 对象（增加引用） | `"o", json_obj` |
| `O` | 对象（不增加引用） | `"O", json_obj` |

#### 使用示例
```c
json_t *person = json_pack(
    "{s:s, s:i, s:[s,s,s]}",
    "name", "Tom",
    "age", 20,
    "skills", "C", "Go", "Rust"
);
```

**生成的 JSON：**
```json
{
  "name": "Tom",
  "age": 20,
  "skills": ["C", "Go", "Rust"]
}
```

### json_unpack() - 解析 JSON

#### 函数签名
```c
int json_unpack(json_t *root, const char *fmt, ...);
```

#### 使用示例
```c
char *name;
int age;
json_t *skills;

int result = json_unpack(
    person,
    "{s:s, s:i, s:o}",
    "name", &name,
    "age", &age,
    "skills", &skills
);

if (result == 0) {
    printf("姓名：%s，年龄：%d\n", name, age);
} else {
    printf("解析失败\n");
}
```

### 复杂示例

```c
// 创建嵌套结构
json_t *data = json_pack(
    "{s:{s:s,s:i},s:[i,i,i]}",
    "person",
        "name", "Alice",
        "age", 25,
    "numbers", 1, 2, 3
);

// 解析嵌套结构
char *name;
int age;
json_t *numbers;

json_unpack(
    data,
    "{s:{s:s,s:i},s:o}",
    "person",
        "name", &name,
        "age", &age,
    "numbers", &numbers
);
```
## 工程实践最佳模式

### 模式 1：配置文件读取

**使用场景**：应用程序启动时读取配置

```c
typedef struct {
    char *server_host;
    int server_port;
    int max_connections;
} config_t;

int load_config(const char *filename, config_t *config) {
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);
    if (!root) {
        fprintf(stderr, "配置文件错误：%s\n", error.text);
        return -1;
    }

    const char *host;
    int port, max_conn;

    if (json_unpack(root, "{s:s,s:i,s:i}",
                   "host", &host,
                   "port", &port,
                   "max_connections", &max_conn) != 0) {
        json_decref(root);
        return -1;
    }

    config->server_host = strdup(host);
    config->server_port = port;
    config->max_connections = max_conn;

    json_decref(root);
    return 0;
}
```

### 模式 2：REST API 响应解析

**使用场景**：解析 HTTP API 返回的 JSON 数据

```c
int parse_api_response(const char *json_response) {
    json_error_t error;
    json_t *root = json_loads(json_response, 0, &error);
    if (!root) {
        fprintf(stderr, "API 响应解析错误：%s\n", error.text);
        return -1;
    }

    // 检查状态
    json_t *status = json_object_get(root, "status");
    if (!json_is_string(status)) {
        json_decref(root);
        return -1;
    }

    const char *status_str = json_string_value(status);
    if (strcmp(status_str, "success") != 0) {
        json_decref(root);
        return -1;
    }

    // 提取数据
    json_t *data = json_object_get(root, "data");
    if (json_is_array(data)) {
        size_t index;
        json_t *item;

        json_array_foreach(data, index, item) {
            // 处理每个数据项
            if (json_is_object(item)) {
                process_data_item(item);
            }
        }
    }

    json_decref(root);
    return 0;
}
```

### 模式 3：生成 RPC 消息

**使用场景**：创建要发送给其他服务的 JSON 消息

```c
char *create_rpc_message(const char *method, json_t *params) {
    json_t *message = json_pack(
        "{s:s,s:i,s:s,s:o}",
        "jsonrpc", "2.0",
        "id", 1,
        "method", method,
        "params", params
    );

    char *json_str = json_dumps(message, JSON_COMPACT);
    json_decref(message);

    return json_str;  // 调用者负责 free()
}

// 使用示例
json_t *params = json_pack("{s:s,s:i}", "username", "alice", "age", 30);
char *rpc_msg = create_rpc_message("create_user", params);
// 发送 rpc_msg 到网络
free(rpc_msg);
```
## 完整心智模型总结

### 数据流模型

```
                    JSON文本
                        │
                        ▼
              json_loads/json_load_file
                        │
                        ▼
                  json_t 树 (root)
                        │
        ┌───────────────┼───────────────┐
        │               │               │
        ▼               ▼               ▼
    object           array           value
        │               │               │
        ▼               ▼               ▼
json_object_*     json_array_*    json_*_value
        │               │               │
        └───────────────┼───────────────┘
                        │
                        ▼
                    业务逻辑
                        │
                        ▼
              json_dumps/json_dump_file
                        │
                        ▼
                    JSON文本
```

### 内存管理模型

每个 json_t 节点包含：

```
json_t
 ├── type      (数据类型标识)
 ├── value     (实际数据)
 └── refcount  (引用计数)
```

**生命周期：**

1. **创建**：`refcount = 1`
2. **共享**：`json_incref()` → `refcount++`
3. **释放**：`json_decref()` → `refcount--`
4. **清理**：`refcount = 0` → 自动 `free()`

### 三个核心问题

学习和使用 Jansson 时，始终要思考这三个问题：

#### 1. 类型问题
**当前 json_t 的真实类型是什么？**
- `json_is_object()` / `json_is_array()` / `json_is_string()` 等
- 先检查类型，再进行相应操作

#### 2. 导航问题 
**我是在树上向下查找还是向上构建？**
- **向下查找**：`json_object_get()` / `json_array_get()`
- **向上构建**：`json_object_set_new()` / `json_array_append_new()`

#### 3. 所有权问题 ⚠️
**当前对象的引用计数归谁管理？**
- **New Reference**：调用者拥有，需要 `json_decref()`
- **Borrowed Reference**：被调用函数拥有，不需要手动释放
- **这是最容易出错的地方！**

### 最佳实践原则

1. **总是检查类型**：避免类型错误导致的崩溃
2. **匹配 incref/decref**：每个 `json_incref()` 都要有对应的 `json_decref()`
3. **使用 _new 变体**：优先使用 `json_object_set_new()` 等函数
4. **及时释放资源**：不再使用的 json_t 对象要及时 `json_decref()`

---

## 完整示例：Jansson + zlib 数据压缩传输

下面是一个综合示例，演示了如何使用 Jansson 创建复杂 JSON 数据，使用 zlib 进行压缩，模拟网络传输，然后解压缩和解析数据。

### 示例代码

```c
#define _POSIX_C_SOURCE 199309L  // For nanosleep
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include <jansson.h>

#define BUFFER_SIZE 8192

// 用户信息结构
typedef struct {
    int id;
    char *name;
    char *email;
    int age;
    char **skills;
    int skill_count;
} user_info_t;

// 压缩数据结构
typedef struct {
    unsigned char *data;
    size_t size;
    size_t capacity;
} compressed_data_t;

// 错误处理宏
#define CHECK_NULL_RETURN_INT(ptr, msg) \
    if (!(ptr)) { \
        fprintf(stderr, "错误: %s\n", msg); \
        return -1; \
    }

#define CHECK_NULL_RETURN_PTR(ptr, msg) \
    if (!(ptr)) { \
        fprintf(stderr, "错误: %s\n", msg); \
        return NULL; \
    }

/**
 * 使用 zlib 压缩数据
 */
int compress_data(const char *input, size_t input_len,
                  compressed_data_t *output) {
    // 分配输出缓冲区
    output->capacity = compressBound(input_len);
    output->data = malloc(output->capacity);
    CHECK_NULL_RETURN_INT(output->data, "分配压缩缓冲区失败");

    // 执行压缩
    int result = compress(output->data, &output->capacity,
                         (const Bytef*)input, input_len);

    if (result != Z_OK) {
        free(output->data);
        fprintf(stderr, "压缩失败: %d\n", result);
        return -1;
    }

    output->size = output->capacity;
    printf("压缩完成: %zu -> %zu 字节 (压缩率: %.1f%%)\n",
           input_len, output->size,
           (1.0 - (double)output->size / input_len) * 100);

    return 0;
}

/**
 * 使用 zlib 解压缩数据
 */
char *decompress_data(const compressed_data_t *input, size_t original_size) {
    char *output = malloc(original_size + 1);
    CHECK_NULL_RETURN_PTR(output, "分配解压缩缓冲区失败");

    uLongf dest_len = original_size;
    int result = uncompress((Bytef*)output, &dest_len,
                           input->data, input->size);

    if (result != Z_OK) {
        free(output);
        fprintf(stderr, "解压缩失败: %d\n", result);
        return NULL;
    }

    output[dest_len] = '\0';
    printf("解压缩完成: %zu -> %lu 字节\n", input->size, dest_len);

    return output;
}

/**
 * 创建用户信息的 JSON 对象
 */
json_t *create_user_json(const user_info_t *user) {
    json_t *root = json_object();
    CHECK_NULL_RETURN_PTR(root, "创建根对象失败");

    // 添加基本信息
    json_object_set_new(root, "id", json_integer(user->id));
    json_object_set_new(root, "name", json_string(user->name));
    json_object_set_new(root, "email", json_string(user->email));
    json_object_set_new(root, "age", json_integer(user->age));

    // 创建技能数组
    json_t *skills_array = json_array();
    for (int i = 0; i < user->skill_count; i++) {
        json_array_append_new(skills_array, json_string(user->skills[i]));
    }
    json_object_set_new(root, "skills", skills_array);

    // 添加时间戳和元数据
    json_object_set_new(root, "created_at", json_integer(time(NULL)));
    json_object_set_new(root, "version", json_string("1.2.0"));
    json_object_set_new(root, "active", json_true());

    // 添加嵌套的配置对象
    json_t *config = json_pack(
        "{s:{s:b,s:i},s:[s,s,s],s:f}",
        "preferences",
            "dark_mode", 1,
            "notification_level", 2,
        "languages", "zh-CN", "en-US", "ja-JP",
        "rating", 4.8
    );
    json_object_set_new(root, "config", config);

    return root;
}

/**
 * 从 JSON 对象解析用户信息
 */
int parse_user_json(json_t *root, user_info_t *user) {
    // 解析基本字段
    json_t *id_json = json_object_get(root, "id");
    json_t *name_json = json_object_get(root, "name");
    json_t *email_json = json_object_get(root, "email");
    json_t *age_json = json_object_get(root, "age");
    json_t *skills_json = json_object_get(root, "skills");

    // 类型检查
    if (!json_is_integer(id_json) || !json_is_string(name_json) ||
        !json_is_string(email_json) || !json_is_integer(age_json) ||
        !json_is_array(skills_json)) {
        fprintf(stderr, "JSON 格式错误\n");
        return -1;
    }

    // 提取基本信息
    user->id = json_integer_value(id_json);
    user->name = strdup(json_string_value(name_json));
    user->email = strdup(json_string_value(email_json));
    user->age = json_integer_value(age_json);

    // 提取技能数组
    user->skill_count = json_array_size(skills_json);
    user->skills = malloc(sizeof(char*) * user->skill_count);

    size_t index;
    json_t *skill_json;
    json_array_foreach(skills_json, index, skill_json) {
        if (json_is_string(skill_json)) {
            user->skills[index] = strdup(json_string_value(skill_json));
        }
    }

    return 0;
}

/**
 * 打印用户信息
 */
void print_user_info(const user_info_t *user, const char *title) {
    printf("\n=== %s ===\n", title);
    printf("ID: %d\n", user->id);
    printf("姓名: %s\n", user->name);
    printf("邮箱: %s\n", user->email);
    printf("年龄: %d\n", user->age);
    printf("技能: ");
    for (int i = 0; i < user->skill_count; i++) {
        printf("%s", user->skills[i]);
        if (i < user->skill_count - 1) printf(", ");
    }
    printf("\n");
}

/**
 * 释放用户信息内存
 */
void free_user_info(user_info_t *user) {
    free(user->name);
    free(user->email);
    for (int i = 0; i < user->skill_count; i++) {
        free(user->skills[i]);
    }
    free(user->skills);
}

/**
 * 演示复杂 JSON 操作
 */
void demonstrate_json_operations(json_t *root) {
    printf("\n=== JSON 操作演示 ===\n");

    // 遍历根对象
    printf("遍历根对象字段:\n");
    const char *key;
    json_t *value;
    json_object_foreach(root, key, value) {
        printf("  %s: ", key);
        if (json_is_string(value)) {
            printf("字符串 \"%s\"\n", json_string_value(value));
        } else if (json_is_integer(value)) {
            printf("整数 %lld\n", json_integer_value(value));
        } else if (json_is_real(value)) {
            printf("浮点数 %f\n", json_real_value(value));
        } else if (json_is_true(value)) {
            printf("布尔值 true\n");
        } else if (json_is_false(value)) {
            printf("布尔值 false\n");
        } else if (json_is_array(value)) {
            printf("数组 (大小: %zu)\n", json_array_size(value));
        } else if (json_is_object(value)) {
            printf("对象 (大小: %zu)\n", json_object_size(value));
        } else if (json_is_null(value)) {
            printf("null\n");
        }
    }

    // 深度访问嵌套数据
    printf("\n深度访问示例:\n");
    json_t *config = json_object_get(root, "config");
    if (json_is_object(config)) {
        json_t *preferences = json_object_get(config, "preferences");
        if (json_is_object(preferences)) {
            json_t *dark_mode = json_object_get(preferences, "dark_mode");
            if (json_is_boolean(dark_mode)) {
                printf("  暗色模式: %s\n",
                       json_is_true(dark_mode) ? "开启" : "关闭");
            }
        }

        json_t *languages = json_object_get(config, "languages");
        if (json_is_array(languages)) {
            printf("  支持的语言: ");
            size_t i;
            json_t *lang;
            json_array_foreach(languages, i, lang) {
                if (json_is_string(lang)) {
                    printf("%s ", json_string_value(lang));
                }
            }
            printf("\n");
        }
    }
}

/**
 * 模拟网络传输
 */
void simulate_network_transfer(const compressed_data_t *data) {
    printf("\n=== 模拟网络传输 ===\n");
    printf("传输数据包大小: %zu 字节\n", data->size);
    printf("模拟网络延迟...\n");

    // 模拟传输时间
    struct timespec req = {0, 100000000L}; // 100ms
    nanosleep(&req, NULL);

    printf("数据传输完成\n");
}

int main() {
    printf("Jansson + zlib 综合示例\n");
    printf("========================\n");

    // 1. 准备测试数据
    char *skills[] = {"C", "Python", "Go", "Rust", "JavaScript"};
    user_info_t original_user = {
        .id = 12345,
        .name = "张三",
        .email = "zhangsan@example.com",
        .age = 28,
        .skills = skills,
        .skill_count = 5
    };

    print_user_info(&original_user, "原始用户数据");

    // 2. 创建 JSON 对象
    json_t *user_json = create_user_json(&original_user);
    if (!user_json) {
        return 1;
    }

    // 3. 演示 JSON 操作
    demonstrate_json_operations(user_json);

    // 4. 序列化为 JSON 字符串
    char *json_string = json_dumps(user_json, JSON_INDENT(2));
    if (!json_string) {
        json_decref(user_json);
        return 1;
    }

    printf("\n=== JSON 序列化结果 ===\n");
    printf("JSON 大小: %zu 字节\n", strlen(json_string));
    printf("JSON 内容:\n%s\n", json_string);

    // 5. 压缩 JSON 数据
    printf("\n=== zlib 压缩 ===\n");
    compressed_data_t compressed;
    if (compress_data(json_string, strlen(json_string), &compressed) != 0) {
        free(json_string);
        json_decref(user_json);
        return 1;
    }

    // 6. 模拟网络传输
    simulate_network_transfer(&compressed);

    // 7. 解压缩数据
    printf("\n=== zlib 解压缩 ===\n");
    char *decompressed = decompress_data(&compressed, strlen(json_string));
    if (!decompressed) {
        free(compressed.data);
        free(json_string);
        json_decref(user_json);
        return 1;
    }

    // 8. 验证解压缩结果
    if (strcmp(json_string, decompressed) == 0) {
        printf("✓ 数据完整性验证通过\n");
    } else {
        printf("✗ 数据完整性验证失败\n");
    }

    // 9. 解析解压缩后的 JSON
    printf("\n=== JSON 解析 ===\n");
    json_error_t error;
    json_t *parsed_json = json_loads(decompressed, 0, &error);
    if (!parsed_json) {
        fprintf(stderr, "JSON 解析失败: %s\n", error.text);
        goto cleanup;
    }

    // 10. 提取用户信息
    user_info_t parsed_user = {0};
    if (parse_user_json(parsed_json, &parsed_user) == 0) {
        print_user_info(&parsed_user, "解析后的用户数据");
        free_user_info(&parsed_user);
    }

    // 11. 使用 json_unpack 的简化解析方式
    printf("\n=== 使用 json_unpack 简化解析 ===\n");
    const char *name, *email;
    int id, age;
    json_t *skills_array;

    if (json_unpack(parsed_json, "{s:i,s:s,s:s,s:i,s:o}",
                   "id", &id,
                   "name", &name,
                   "email", &email,
                   "age", &age,
                   "skills", &skills_array) == 0) {
        printf("使用 unpack 提取的数据:\n");
        printf("  ID: %d, 姓名: %s, 邮箱: %s, 年龄: %d\n",
               id, name, email, age);
        printf("  技能数量: %zu\n", json_array_size(skills_array));
    }

    printf("\n=== 示例完成 ===\n");
    printf("演示了以下 Jansson API:\n");
    printf("  ✓ json_object/json_array 创建\n");
    printf("  ✓ json_object_set_new/json_array_append_new\n");
    printf("  ✓ json_pack 快速创建\n");
    printf("  ✓ json_dumps 序列化\n");
    printf("  ✓ json_loads 反序列化\n");
    printf("  ✓ json_object_foreach/json_array_foreach 遍历\n");
    printf("  ✓ json_is_* 类型检查\n");
    printf("  ✓ json_*_value 值提取\n");
    printf("  ✓ json_unpack 格式化解析\n");
    printf("  ✓ 引用计数管理 (json_decref)\n");
    printf("  ✓ zlib 压缩/解压缩集成\n");

cleanup:
    // 清理资源
    json_decref(parsed_json);
    json_decref(user_json);
    free(compressed.data);
    free(json_string);
    free(decompressed);

    return 0;
}
```

### 编译和运行

```bash
# 安装依赖 (Ubuntu/Debian)
sudo apt-get install libjansson-dev zlib1g-dev

# 编译
gcc -o jansson_example jansson_example.c -ljansson -lz

# 如果遇到警告，可以使用更严格的编译选项
gcc -Wall -Wextra -std=c99 -o jansson_example jansson_example.c -ljansson -lz

# 运行
./jansson_example
```

### 编译注意事项

1. **错误处理宏**：代码中使用了两个不同的错误处理宏：
   - `CHECK_NULL_RETURN_INT`：用于返回整数的函数
   - `CHECK_NULL_RETURN_PTR`：用于返回指针的函数

2. **POSIX 兼容性**：代码使用了 `nanosleep` 函数，需要 `_POSIX_C_SOURCE` 宏定义

3. **依赖库**：确保系统已安装 Jansson 和 zlib 开发包

### 示例输出

```
Jansson + zlib 综合示例
========================

=== 原始用户数据 ===
ID: 12345
姓名: 张三
邮箱: zhangsan@example.com
年龄: 28
技能: C, Python, Go, Rust, JavaScript

=== JSON 操作演示 ===
遍历根对象字段:
  id: 整数 12345
  name: 字符串 "张三"
  email: 字符串 "zhangsan@example.com"
  age: 整数 28
  skills: 数组 (大小: 5)
  created_at: 整数 1719134640
  version: 字符串 "1.2.0"
  active: 布尔值 true
  config: 对象 (大小: 3)

深度访问示例:
  暗色模式: 开启
  支持的语言: zh-CN en-US ja-JP

=== JSON 序列化结果 ===
JSON 大小: 486 字节

=== zlib 压缩 ===
压缩完成: 486 -> 312 字节 (压缩率: 35.8%)

=== 模拟网络传输 ===
传输数据包大小: 312 字节
模拟网络延迟...
数据传输完成

=== zlib 解压缩 ===
解压缩完成: 312 -> 486 字节
✓ 数据完整性验证通过

=== JSON 解析 ===

=== 解析后的用户数据 ===
ID: 12345
姓名: 张三
邮箱: zhangsan@example.com
年龄: 28
技能: C, Python, Go, Rust, JavaScript

=== 示例完成 ===
```

这个综合示例展示了 Jansson 的所有主要 API 功能，结合 zlib 压缩，演示了一个完整的数据处理流水线：创建 → 序列化 → 压缩 → 传输 → 解压缩 → 解析 → 提取。