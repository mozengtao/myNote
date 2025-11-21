# ⚡ Protocol Buffers 快速参考卡片

## 🎯 基本模板

```protobuf
syntax = "proto3";
package myapp;

message MyMessage {
  int32 field = 1;
}

service MyService {
  rpc MyMethod (Request) returns (Response) {}
}
```

---

## 📊 数据类型速查

| Proto   | Python | 示例 |
|---------|--------|------|
| `int32` | `int` | `123` |
| `int64` | `int` | `9999999999` |
| `uint32` | `int` | `100` (≥0) |
| `float` | `float` | `3.14` |
| `double` | `float` | `3.14159` |
| `bool` | `bool` | `true` |
| `string` | `str` | `"hello"` |
| `bytes` | `bytes` | `b"data"` |

---

## 🔤 字段修饰符

```protobuf
message Example {
  // 单个值（默认）
  string name = 1;
  
  // 列表/数组
  repeated string tags = 2;
  
  // 字典/映射
  map<string, int32> scores = 3;
}
```

---

## 🎨 枚举

```protobuf
enum Status {
  STATUS_UNKNOWN = 0;  // 必须从 0 开始
  STATUS_ACTIVE = 1;
  STATUS_INACTIVE = 2;
}

message User {
  Status status = 1;
}
```

---

## 🏗️ 服务类型

```protobuf
service MyService {
  // 一元：单请求 → 单响应
  rpc Get (Req) returns (Res) {}
  
  // 服务器流：单请求 → 多响应
  rpc List (Req) returns (stream Res) {}
  
  // 客户端流：多请求 → 单响应
  rpc Upload (stream Req) returns (Res) {}
  
  // 双向流：多请求 ↔ 多响应
  rpc Chat (stream Req) returns (stream Res) {}
}
```

---

## 💡 命名规范

```protobuf
// 消息：PascalCase
message UserProfile {}

// 字段：snake_case
message User {
  string first_name = 1;
  int32 user_id = 2;
}

// 枚举：UPPER_SNAKE_CASE
enum USER_STATUS_ACTIVE = 1;

// 服务：PascalCase
service UserService {}

// 方法：PascalCase
rpc GetUser () returns () {}
```

---

## 🔧 常用模式

### 分页请求
```protobuf
message PageRequest {
  int32 page = 1;
  int32 page_size = 2;
}

message PageResponse {
  repeated Item items = 1;
  int32 total = 2;
}
```

### 错误响应
```protobuf
message Response {
  bool success = 1;
  string error_message = 2;
  Data data = 3;
}
```

### 时间戳
```protobuf
import "google/protobuf/timestamp.proto";

message Post {
  google.protobuf.Timestamp created_at = 1;
}
```

---

## ⚠️ 注意事项

### ✅ 要做的
- 字段编号从 1 开始
- 1-15 最高效（1 字节）
- 枚举从 0 开始
- 使用 reserved 保留删除的字段

### ❌ 不要做的
- 不要修改已有字段编号
- 不要使用 19000-19999（保留）
- 不要删除字段后重用编号
- 枚举不要跳过 0

---

## 🚀 生成代码

```bash
# Python
python3 -m grpc_tools.protoc \
    -I. \
    --python_out=. \
    --grpc_python_out=. \
    your_file.proto
```

---

## 📝 完整示例

```protobuf
syntax = "proto3";
package user.v1;

enum UserRole {
  USER_ROLE_UNKNOWN = 0;
  USER_ROLE_USER = 1;
  USER_ROLE_ADMIN = 2;
}

message Address {
  string street = 1;
  string city = 2;
}

message User {
  int32 id = 1;
  string name = 2;
  string email = 3;
  UserRole role = 4;
  Address address = 5;
  repeated string tags = 6;
  map<string, string> metadata = 7;
}

message GetUserRequest {
  int32 user_id = 1;
}

message UserResponse {
  User user = 1;
}

service UserService {
  rpc GetUser (GetUserRequest) returns (UserResponse) {}
}
```

---

## 🔗 资源链接

- **详细指南**: `proto语法指南.md`
- **实际示例**: `calculator.proto`, `advanced_example.proto`
- **官方文档**: https://developers.google.com/protocol-buffers

**保存此卡片作为快速参考！** 📌

