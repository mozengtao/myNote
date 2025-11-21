# 📚 Protocol Buffers (Proto) 语法完整指南

## 🎯 什么是 .proto 文件？

`.proto` 文件是用来定义数据结构和服务接口的配置文件，类似于：
- **C/C++** 的头文件 (.h)
- **Java** 的接口定义
- **TypeScript** 的类型定义

---

## 📝 基本结构

### 完整示例

```protobuf
// 1. 指定语法版本（必需）
syntax = "proto3";

// 2. 定义包名（可选，推荐）
package myapp;

// 3. 导入其他 proto 文件（可选）
import "google/protobuf/timestamp.proto";

// 4. 定义消息（数据结构）
message User {
  int32 id = 1;
  string name = 2;
  string email = 3;
}

// 5. 定义服务（RPC 接口）
service UserService {
  rpc GetUser (UserRequest) returns (User) {}
}

message UserRequest {
  int32 user_id = 1;
}
```

---

## 🔤 1. 语法版本声明

```protobuf
// Proto3 语法（推荐，最新版本）
syntax = "proto3";

// Proto2 语法（旧版本）
syntax = "proto2";
```

**区别：**
- **proto3** - 简化版本，删除了一些复杂特性，更易用
- **proto2** - 功能更多，但更复杂

**建议：新项目使用 proto3**

---

## 📦 2. 包名（Package）

```protobuf
// 定义包名，避免命名冲突
package mycompany.myapp;

message User {
  int32 id = 1;
}

// 在 Python 中使用时：
// mycompany.myapp.User
```

**作用：**
- 组织代码
- 避免不同项目间的命名冲突
- 类似于 Python 的模块名

---

## 🗂️ 3. 消息定义（Message）

### 基本消息

```protobuf
message Person {
  // 字段类型 字段名 = 字段编号;
  int32 id = 1;
  string name = 2;
  int32 age = 3;
  string email = 4;
}
```

### 字段编号规则

```protobuf
message Example {
  string field1 = 1;    // ✅ 编号从 1 开始
  int32 field2 = 2;     // ✅ 连续编号
  bool field3 = 15;     // ✅ 可以跳号
  
  // ❌ 不能使用的编号：
  // - 0（保留）
  // - 19000-19999（Protocol Buffers 内部保留）
}
```

**重要：**
- 字段编号一旦使用就**不能修改**
- 删除字段时应该**保留编号**（防止未来误用）
- 1-15 号编码效率最高（占用 1 字节）

### 字段修饰符

```protobuf
message Message {
  // Proto3 中所有字段默认都是可选的
  string normal_field = 1;
  
  // repeated = 数组/列表
  repeated string tags = 2;
  repeated int32 scores = 3;
  
  // optional（Proto3 需要特殊设置）
  optional string optional_field = 4;
}
```

---

## 📊 4. 数据类型

### 基本数据类型

| Proto 类型 | Python 类型 | 说明 | 示例 |
|-----------|------------|------|------|
| **double** | float | 双精度浮点数 | 3.14159 |
| **float** | float | 单精度浮点数 | 3.14 |
| **int32** | int | 32位整数 | -2147483648 到 2147483647 |
| **int64** | int | 64位整数 | 更大的整数 |
| **uint32** | int | 无符号32位整数 | 0 到 4294967295 |
| **uint64** | int | 无符号64位整数 | 更大的正整数 |
| **bool** | bool | 布尔值 | true / false |
| **string** | str | UTF-8 字符串 | "Hello" |
| **bytes** | bytes | 字节序列 | b"data" |

### 使用示例

```protobuf
message DataTypes {
  double price = 1;              // 19.99
  float temperature = 2;          // 36.5
  int32 age = 3;                 // 25
  int64 population = 4;           // 7000000000
  uint32 count = 5;              // 100
  bool is_active = 6;            // true
  string name = 7;               // "张三"
  bytes data = 8;                // 二进制数据
}
```

---

## 📋 5. 枚举（Enum）

### 基本枚举

```protobuf
enum Status {
  // 第一个值必须是 0
  STATUS_UNKNOWN = 0;
  STATUS_PENDING = 1;
  STATUS_ACTIVE = 2;
  STATUS_INACTIVE = 3;
}

message User {
  string name = 1;
  Status status = 2;  // 使用枚举
}
```

### 枚举规则

```protobuf
enum OrderStatus {
  // ✅ 必须从 0 开始
  ORDER_UNKNOWN = 0;
  ORDER_CREATED = 1;
  ORDER_PAID = 2;
  ORDER_SHIPPED = 3;
  ORDER_DELIVERED = 4;
}
```

**注意：**
- 第一个枚举值**必须是 0**
- 用作默认值
- 建议用 `XXX_UNKNOWN` 或 `XXX_UNSPECIFIED` 作为 0 值

---

## 🔗 6. 嵌套消息

### 嵌套定义

```protobuf
message Person {
  string name = 1;
  
  // 嵌套消息定义
  message Address {
    string street = 1;
    string city = 2;
    string country = 3;
  }
  
  Address address = 2;  // 使用嵌套消息
}
```

### 使用其他消息

```protobuf
message Address {
  string street = 1;
  string city = 2;
}

message Person {
  string name = 1;
  Address home_address = 2;      // 单个地址
  repeated Address addresses = 3; // 多个地址
}
```

---

## 🔄 7. 服务定义（Service）

### 基本服务

```protobuf
service UserService {
  // 一元 RPC：一个请求，一个响应
  rpc GetUser (GetUserRequest) returns (User) {}
  
  // 多个方法
  rpc CreateUser (CreateUserRequest) returns (User) {}
  rpc UpdateUser (UpdateUserRequest) returns (User) {}
  rpc DeleteUser (DeleteUserRequest) returns (Empty) {}
}

message GetUserRequest {
  int32 user_id = 1;
}

message CreateUserRequest {
  string name = 1;
  string email = 2;
}
```

### 四种 RPC 类型

```protobuf
service ChatService {
  // 1. 一元 RPC（最常见）
  rpc SendMessage (Message) returns (Response) {}
  
  // 2. 服务器流式 RPC
  rpc GetMessages (User) returns (stream Message) {}
  
  // 3. 客户端流式 RPC
  rpc UploadMessages (stream Message) returns (Response) {}
  
  // 4. 双向流式 RPC
  rpc Chat (stream Message) returns (stream Message) {}
}
```

---

## 🎨 8. 完整实例

### 用户管理系统

```protobuf
syntax = "proto3";

package user.v1;

// ==================== 枚举 ====================

enum UserRole {
  USER_ROLE_UNKNOWN = 0;
  USER_ROLE_ADMIN = 1;
  USER_ROLE_USER = 2;
  USER_ROLE_GUEST = 3;
}

enum UserStatus {
  USER_STATUS_UNKNOWN = 0;
  USER_STATUS_ACTIVE = 1;
  USER_STATUS_INACTIVE = 2;
  USER_STATUS_BANNED = 3;
}

// ==================== 消息 ====================

// 地址信息
message Address {
  string street = 1;
  string city = 2;
  string province = 3;
  string country = 4;
  string postal_code = 5;
}

// 用户信息
message User {
  int32 id = 1;
  string name = 2;
  string email = 3;
  int32 age = 4;
  UserRole role = 5;
  UserStatus status = 6;
  Address address = 7;
  repeated string tags = 8;           // 标签列表
  map<string, string> metadata = 9;   // 元数据键值对
}

// 请求消息
message GetUserRequest {
  int32 user_id = 1;
}

message CreateUserRequest {
  string name = 1;
  string email = 2;
  int32 age = 3;
  UserRole role = 4;
}

message UpdateUserRequest {
  int32 user_id = 1;
  string name = 2;
  string email = 3;
  UserStatus status = 4;
}

message DeleteUserRequest {
  int32 user_id = 1;
}

message ListUsersRequest {
  int32 page = 1;
  int32 page_size = 2;
  string filter = 3;  // 搜索过滤器
}

// 响应消息
message UserResponse {
  User user = 1;
  string message = 2;
}

message ListUsersResponse {
  repeated User users = 1;
  int32 total = 2;
  int32 page = 3;
}

message Empty {}

// ==================== 服务 ====================

service UserService {
  // 获取单个用户
  rpc GetUser (GetUserRequest) returns (UserResponse) {}
  
  // 创建用户
  rpc CreateUser (CreateUserRequest) returns (UserResponse) {}
  
  // 更新用户
  rpc UpdateUser (UpdateUserRequest) returns (UserResponse) {}
  
  // 删除用户
  rpc DeleteUser (DeleteUserRequest) returns (Empty) {}
  
  // 列出用户（分页）
  rpc ListUsers (ListUsersRequest) returns (ListUsersResponse) {}
  
  // 流式获取用户更新
  rpc WatchUsers (Empty) returns (stream User) {}
}
```

---

## 🗺️ 9. Map 类型

```protobuf
message User {
  string name = 1;
  
  // map<键类型, 值类型> 字段名 = 编号;
  map<string, string> attributes = 2;
  map<int32, string> id_to_name = 3;
  map<string, Address> addresses = 4;
}
```

**Python 使用：**
```python
user = User()
user.name = "张三"
user.attributes["phone"] = "123456789"
user.attributes["city"] = "北京"
```

---

## 📥 10. 导入（Import）

```protobuf
// 导入其他 proto 文件
import "google/protobuf/timestamp.proto";
import "common/address.proto";

message User {
  string name = 1;
  google.protobuf.Timestamp created_at = 2;
  common.Address address = 3;
}
```

**常用的 Google 类型：**
```protobuf
import "google/protobuf/timestamp.proto";  // 时间戳
import "google/protobuf/duration.proto";   // 时间段
import "google/protobuf/empty.proto";      // 空消息
import "google/protobuf/any.proto";        // 任意类型
```

---

## 🔧 11. 选项（Options）

```protobuf
syntax = "proto3";

// 包选项
option java_package = "com.example.user";
option java_outer_classname = "UserProtos";
option go_package = "example.com/user";

message User {
  string name = 1;
  
  // 字段选项
  string deprecated_field = 2 [deprecated = true];
}
```

---

## 💡 12. 最佳实践

### ✅ 命名规范

```protobuf
// 消息名：大驼峰（PascalCase）
message UserProfile {}
message OrderStatus {}

// 字段名：下划线分隔（snake_case）
message User {
  string first_name = 1;
  string last_name = 2;
  int32 user_id = 3;
}

// 枚举值：大写+下划线
enum Status {
  STATUS_UNKNOWN = 0;
  STATUS_ACTIVE = 1;
  STATUS_INACTIVE = 2;
}

// 服务名：大驼峰
service UserService {}
service OrderManagement {}

// RPC 方法名：大驼峰
rpc GetUser () returns () {}
rpc CreateOrder () returns () {}
```

### ✅ 版本管理

```protobuf
// 方式 1：包名中包含版本
package myapp.user.v1;

// 方式 2：使用注释
// Version: v1.2.0
// Last updated: 2025-11-20

message User {
  int32 id = 1;
  string name = 2;
  // 已废弃：改用 email_address
  string email = 3 [deprecated = true];
  string email_address = 4;
}
```

### ✅ 向后兼容

```protobuf
message User {
  int32 id = 1;
  string name = 2;
  
  // ❌ 删除字段时不要这样做：
  // （直接删除会导致编号被重用）
  
  // ✅ 正确做法：保留编号
  reserved 3, 4;  // 保留编号 3 和 4
  reserved "old_field", "deprecated_field";  // 保留字段名
  
  string email = 5;  // 新字段使用新编号
}
```

---

## 📝 13. 注释

```protobuf
// 单行注释

/*
 * 多行注释
 * 可以跨越多行
 */

// 推荐：为每个字段添加注释
message User {
  // 用户唯一标识符
  int32 id = 1;
  
  // 用户全名（必填）
  string name = 2;
  
  /* 
   * 用户邮箱地址
   * 格式：user@example.com
   */
  string email = 3;
}
```

---

## 🎯 14. 实用示例集

### 分页请求

```protobuf
message PageRequest {
  int32 page = 1;         // 页码（从 1 开始）
  int32 page_size = 2;    // 每页大小
  string sort_by = 3;     // 排序字段
  bool ascending = 4;     // 是否升序
}

message PageResponse {
  repeated User users = 1;  // 数据列表
  int32 total = 2;          // 总数
  int32 current_page = 3;   // 当前页
  int32 total_pages = 4;    // 总页数
}
```

### 错误处理

```protobuf
enum ErrorCode {
  ERROR_CODE_UNKNOWN = 0;
  ERROR_CODE_NOT_FOUND = 1;
  ERROR_CODE_PERMISSION_DENIED = 2;
  ERROR_CODE_INVALID_ARGUMENT = 3;
}

message Error {
  ErrorCode code = 1;
  string message = 2;
  map<string, string> details = 3;
}

message Response {
  bool success = 1;
  Error error = 2;
  User user = 3;
}
```

### 时间戳

```protobuf
import "google/protobuf/timestamp.proto";

message Post {
  string title = 1;
  string content = 2;
  google.protobuf.Timestamp created_at = 3;
  google.protobuf.Timestamp updated_at = 4;
}
```

---

## 🚀 15. 生成代码

### 编译 proto 文件

```bash
# Python
python3 -m grpc_tools.protoc \
    -I. \
    --python_out=. \
    --grpc_python_out=. \
    user.proto

# 生成文件：
# - user_pb2.py         (消息类)
# - user_pb2_grpc.py    (服务代码)
```

### 使用生成的代码

```python
import user_pb2

# 创建消息
user = user_pb2.User()
user.id = 1
user.name = "张三"
user.email = "zhangsan@example.com"
user.role = user_pb2.USER_ROLE_ADMIN

# 添加地址
user.address.street = "中关村大街1号"
user.address.city = "北京"

# 添加标签
user.tags.append("VIP")
user.tags.append("新用户")

# 添加元数据
user.metadata["phone"] = "123456789"
user.metadata["company"] = "科技公司"

print(user)
```

---

## 🔍 16. Proto3 vs Proto2

| 特性 | Proto3 | Proto2 |
|------|--------|--------|
| **默认值** | 所有字段都有默认值 | 需要显式设置 |
| **required** | 不支持 | 支持 |
| **optional** | 默认行为 | 需要显式声明 |
| **默认值** | 不能自定义 | 可以自定义 |
| **扩展** | 不支持 | 支持 |
| **语法** | 更简洁 | 更复杂 |

**建议：新项目使用 Proto3**

---

## 📚 17. 学习资源

### 官方文档
- **Language Guide**: https://developers.google.com/protocol-buffers/docs/proto3
- **Style Guide**: https://developers.google.com/protocol-buffers/docs/style
- **Python Tutorial**: https://developers.google.com/protocol-buffers/docs/pythontutorial

### 在线工具
- **Protobuf 在线编辑器**: https://protobuf-compiler.com
- **Protobuf 可视化**: https://protobuf.studio

---

## ✨ 总结

### 核心要点

1. ✅ **语法版本** - 使用 `syntax = "proto3";`
2. ✅ **消息定义** - 定义数据结构
3. ✅ **字段编号** - 从 1 开始，不可修改
4. ✅ **服务定义** - 定义 RPC 接口
5. ✅ **命名规范** - PascalCase 消息，snake_case 字段
6. ✅ **向后兼容** - 使用 reserved 保留删除的字段

### 快速参考

```protobuf
syntax = "proto3";                    // 1. 声明语法
package myapp;                        // 2. 定义包名

message User {                        // 3. 定义消息
  int32 id = 1;                      // 基本类型
  string name = 2;                   // 字符串
  repeated string tags = 3;          // 列表
  map<string, string> meta = 4;     // 字典
  Address address = 5;               // 嵌套消息
  UserRole role = 6;                 // 枚举
}

enum UserRole {                       // 4. 定义枚举
  USER_ROLE_UNKNOWN = 0;             // 必须从 0 开始
  USER_ROLE_ADMIN = 1;
}

service UserService {                 // 5. 定义服务
  rpc GetUser (Request) returns (User) {}
  rpc ListUsers (Request) returns (stream User) {}
}
```

### 下一步

1. 查看 `calculator.proto` - 实际示例
2. 运行 `bash grpc_setup.sh` - 生成代码
3. 修改 proto 文件并重新生成 - 实践学习

**开始编写你的 proto 文件吧！** 🚀

