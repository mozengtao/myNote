# 🚀 gRPC Python 完整示例

## 📖 什么是 gRPC？

**gRPC** 是 Google 开发的高性能、开源的 RPC 框架：
- 使用 **Protocol Buffers** 作为接口定义语言
- 支持多种编程语言
- 提供四种服务方法：一元、服务器流、客户端流、双向流
- HTTP/2 协议，性能优异

## 🗂️ 文件说明

```
grpc_example/
├── calculator.proto          # Protocol Buffer 服务定义
├── grpc_server.py           # gRPC 服务器
├── grpc_client.py           # gRPC 客户端
├── grpc_setup.sh            # 自动设置脚本
├── calculator_pb2.py        # 自动生成（消息类）
└── calculator_pb2_grpc.py   # 自动生成（服务代码）
```

## 🎯 快速开始

### 方法 1：使用自动化脚本（推荐）

```bash
# 一键设置环境并生成代码
bash grpc_setup.sh
```

### 方法 2：手动设置

**步骤 1: 安装依赖**
```bash
pip3 install grpcio grpcio-tools
```

**步骤 2: 生成 Python 代码**
```bash
python3 -m grpc_tools.protoc \
    -I. \
    --python_out=. \
    --grpc_python_out=. \
    calculator.proto
```

这会生成两个文件：
- `calculator_pb2.py` - Protocol Buffer 消息类
- `calculator_pb2_grpc.py` - gRPC 服务代码

**步骤 3: 运行服务器**
```bash
# 终端 1
python3 grpc_server.py
```

**步骤 4: 运行客户端**
```bash
# 终端 2
python3 grpc_client.py
```

## 📝 Protocol Buffer 定义

`calculator.proto` 定义服务接口：

```protobuf
syntax = "proto3";

service Calculator {
  rpc Add (BinaryOperation) returns (Result) {}
  rpc Subtract (BinaryOperation) returns (Result) {}
  rpc Multiply (BinaryOperation) returns (Result) {}
  rpc Divide (BinaryOperation) returns (Result) {}
  rpc GetSquares (Number) returns (stream Result) {}  // 流式
}

message BinaryOperation {
  double a = 1;
  double b = 2;
}

message Result {
  double value = 1;
  string message = 2;
}
```

### Protocol Buffer 的优势

1. **强类型** - 编译时类型检查
2. **高效** - 二进制序列化，比 JSON 小 3-10 倍
3. **跨语言** - 同一个 .proto 可生成多种语言代码
4. **版本兼容** - 向前/向后兼容

## 🔍 核心代码解析

### 服务器端 (grpc_server.py)

```python
# 1. 实现服务
class CalculatorServicer(calculator_pb2_grpc.CalculatorServicer):
    def Add(self, request, context):
        result = request.a + request.b
        return calculator_pb2.Result(value=result)

# 2. 创建并启动服务器
server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
calculator_pb2_grpc.add_CalculatorServicer_to_server(
    CalculatorServicer(), server
)
server.add_insecure_port('[::]:50051')
server.start()
```

### 客户端 (grpc_client.py)

```python
# 1. 创建连接
channel = grpc.insecure_channel('localhost:50051')

# 2. 创建存根（stub）
stub = calculator_pb2_grpc.CalculatorStub(channel)

# 3. 调用远程方法
response = stub.Add(calculator_pb2.BinaryOperation(a=5, b=10))
print(response.value)  # 15
```

## 🎨 功能特点

### 1. 一元 RPC（最简单）

**客户端发送一个请求，服务器返回一个响应**

```python
# 客户端
response = stub.Add(calculator_pb2.BinaryOperation(a=5, b=10))
print(response.value)  # 15
```

### 2. 服务器流式 RPC

**客户端发送一个请求，服务器返回多个响应**

```python
# 服务器端
def GetSquares(self, request, context):
    for i in range(1, int(request.value) + 1):
        yield calculator_pb2.Result(value=i * i)

# 客户端
responses = stub.GetSquares(calculator_pb2.Number(value=5))
for response in responses:
    print(response.value)  # 1, 4, 9, 16, 25
```

### 3. 错误处理

```python
# 服务器端
if request.b == 0:
    context.set_code(grpc.StatusCode.INVALID_ARGUMENT)
    context.set_details("除数不能为零！")
    return calculator_pb2.Result()

# 客户端
try:
    response = stub.Divide(calculator_pb2.BinaryOperation(a=10, b=0))
except grpc.RpcError as e:
    print(f"错误: {e.details()}")
    print(f"状态码: {e.code()}")
```

## 📊 gRPC vs 其他 RPC

### gRPC vs XML-RPC

| 特性 | gRPC | XML-RPC |
|------|------|---------|
| **协议** | HTTP/2 | HTTP/1.1 |
| **序列化** | Protocol Buffers | XML |
| **性能** | 非常快 | 较慢 |
| **类型安全** | 强类型 | 弱类型 |
| **流式** | 支持 | 不支持 |
| **学习曲线** | 中等 | 简单 |

### gRPC vs REST

| 特性 | gRPC | REST |
|------|------|------|
| **风格** | RPC | 资源导向 |
| **格式** | Protocol Buffers | JSON |
| **性能** | 更快 | 较慢 |
| **浏览器支持** | 需要代理 | 原生支持 |
| **流式** | 原生支持 | 需要特殊处理 |

## 🔄 四种服务方法类型

### 1. 一元 RPC (Unary)
```protobuf
rpc Add (BinaryOperation) returns (Result) {}
```
客户端 → 服务器 → 客户端

### 2. 服务器流式 (Server Streaming)
```protobuf
rpc GetSquares (Number) returns (stream Result) {}
```
客户端 → 服务器 → → → → 客户端

### 3. 客户端流式 (Client Streaming)
```protobuf
rpc SumNumbers (stream Number) returns (Result) {}
```
客户端 → → → → 服务器 → 客户端

### 4. 双向流式 (Bidirectional Streaming)
```protobuf
rpc Chat (stream Message) returns (stream Message) {}
```
客户端 ← → ← → ← → 服务器

## 🎯 实际应用场景

1. **微服务架构** - 服务间高效通信
2. **移动应用** - 节省流量，提高速度
3. **实时通信** - WebSocket 替代方案
4. **物联网** - 低延迟、高效率
5. **游戏服务器** - 实时数据同步

## 💡 最佳实践

### 1. 定义清晰的接口

```protobuf
// ✅ 好的设计
message UserRequest {
  int32 user_id = 1;
}

message UserResponse {
  int32 user_id = 1;
  string name = 2;
  string email = 3;
}

// ❌ 不好的设计
message Request {
  string data = 1;  // 太模糊
}
```

### 2. 错误处理

```python
# 使用合适的状态码
if not user_exists:
    context.set_code(grpc.StatusCode.NOT_FOUND)
    context.set_details(f"用户 {user_id} 不存在")
```

### 3. 超时设置

```python
# 设置超时
stub.Add(
    calculator_pb2.BinaryOperation(a=5, b=10),
    timeout=5  # 5 秒超时
)
```

### 4. 元数据传递

```python
# 传递认证 token
metadata = [('authorization', 'Bearer token123')]
response = stub.Add(request, metadata=metadata)
```

## 🔧 高级功能

### 拦截器（Interceptor）

```python
class AuthInterceptor(grpc.ServerInterceptor):
    def intercept_service(self, continuation, handler_call_details):
        # 在这里验证认证
        return continuation(handler_call_details)
```

### 压缩

```python
# 启用压缩
channel = grpc.insecure_channel(
    'localhost:50051',
    options=[('grpc.default_compression_algorithm', 1)]
)
```

### SSL/TLS 加密

```python
# 服务器端
credentials = grpc.ssl_server_credentials(...)
server.add_secure_port('[::]:50051', credentials)

# 客户端
credentials = grpc.ssl_channel_credentials(...)
channel = grpc.secure_channel('localhost:50051', credentials)
```

## 🐛 故障排除

### 问题 1: 模块导入错误
```
ModuleNotFoundError: No module named 'calculator_pb2'
```
**解决：** 运行 `bash grpc_setup.sh` 生成代码

### 问题 2: 连接被拒绝
```
grpc._channel._InactiveRpcError: Connection refused
```
**解决：** 确保服务器正在运行

### 问题 3: .proto 语法错误
**解决：** 检查 Protocol Buffer 语法，确保使用 `proto3`

### 问题 4: 端口被占用
```
OSError: [Errno 98] Address already in use
```
**解决：** 更换端口或停止占用端口的进程

## 📚 学习资源

- **gRPC 官网**: https://grpc.io
- **Protocol Buffers**: https://developers.google.com/protocol-buffers
- **gRPC Python**: https://grpc.io/docs/languages/python/
- **示例代码**: https://github.com/grpc/grpc/tree/master/examples/python

## 🎓 扩展练习

1. **添加新方法**
   - 在 .proto 中定义新方法
   - 实现服务器端逻辑
   - 在客户端调用

2. **实现客户端流式**
   - 客户端发送多个请求
   - 服务器返回一个汇总结果

3. **实现双向流式**
   - 实现一个聊天功能
   - 客户端和服务器相互发送消息

4. **添加认证**
   - 使用元数据传递 token
   - 在服务器端验证

5. **性能测试**
   - 比较 gRPC 和 REST 的性能
   - 测试不同数据量下的表现

## ✨ 总结

### gRPC 的优势

1. ✅ **高性能** - Protocol Buffers + HTTP/2
2. ✅ **强类型** - 编译时类型检查
3. ✅ **跨语言** - 支持 10+ 种语言
4. ✅ **流式支持** - 原生支持各种流式模式
5. ✅ **工具链完善** - 代码生成、调试工具

### 何时使用 gRPC？

**适合：**
- 微服务内部通信
- 需要高性能的场景
- 实时数据传输
- 多语言环境

**不适合：**
- 需要浏览器直接访问
- 简单的公共 API
- 需要人类可读的协议

## 🚀 开始使用

```bash
# 1. 设置环境
bash grpc_setup.sh

# 2. 启动服务器
python3 grpc_server.py

# 3. 运行客户端
python3 grpc_client.py
```

享受 gRPC 带来的高性能体验！🎉

