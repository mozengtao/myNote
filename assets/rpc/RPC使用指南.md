# 🎯 RPC 完整示例使用指南

## 📦 文件列表

### 1. **rpc_demo.py** ⭐️ 推荐先运行这个！
一体化演示，在单个脚本中同时运行服务器和客户端。

**运行方式：**
```bash
python3 rpc_demo.py
```

**特点：**
- ✅ 简单易用，一键运行
- ✅ 同时展示服务器和客户端
- ✅ 包含完整的测试用例
- ✅ 清晰展示 RPC 工作原理

---

### 2. **rpc_server.py** + **rpc_client.py**
分离式服务器和客户端，用于实际的分布式场景。

**运行方式：**

**步骤 1：** 打开第一个终端，启动服务器
```bash
python3 rpc_server.py
```

**步骤 2：** 打开第二个终端，运行客户端
```bash
python3 rpc_client.py
```

**特点：**
- ✅ 真实的客户端-服务器模式
- ✅ 可以在不同机器上运行
- ✅ 更完整的功能演示
- ✅ 包含更多服务和功能

---

### 3. **t1.py**
Python 函数调用追踪器（与 RPC 无关，用于追踪函数调用路径）

---

## 🎓 从演示中学到什么

### 1. RPC 的核心概念

**本地函数调用：**
```python
result = add(5, 10)  # 在本地执行
```

**远程函数调用（RPC）：**
```python
result = proxy.add(5, 10)  # 在远程服务器执行，但看起来一样！
```

### 2. 工作流程

```
客户端                           服务器
  |                               |
  | 1. proxy.add(5, 10)           |
  |------------------------------>|
  |                               | 2. 执行: return 5 + 10
  |                               |
  | 3. 返回结果: 15                |
  |<------------------------------|
  |                               |
```

### 3. 主要组件

#### 服务器端 (rpc_server.py)
```python
# 创建服务器
server = SimpleXMLRPCServer(("localhost", 8000))

# 注册函数
def add(x, y):
    return x + y

server.register_function(add, "add")

# 启动服务器
server.serve_forever()
```

#### 客户端 (rpc_client.py)
```python
# 连接服务器
proxy = xmlrpc.client.ServerProxy("http://localhost:8000")

# 调用远程函数
result = proxy.add(5, 10)  # 就像本地函数一样！
print(result)  # 15
```

---

## 🔥 实际运行示例

### 快速开始 (推荐)

```bash
# 运行一体化演示
python3 rpc_demo.py
```

**输出示例：**
```
🚀 [服务器] 启动在 localhost:9000
✅ [服务器] 准备就绪，等待请求...

📌 测试 1: 计算器服务
  [服务器] 执行加法: 15 + 27
[客户端] add(15, 27) = 42

  [服务器] 执行减法: 100 - 35
[客户端] subtract(100, 35) = 65
```

### 高级用法 (分离式)

**终端 1 - 服务器：**
```bash
$ python3 rpc_server.py
🚀 RPC 服务器启动中...
✅ 服务器运行在: http://localhost:8000
📡 等待客户端连接...
```

**终端 2 - 客户端：**
```bash
$ python3 rpc_client.py
🔌 正在连接到 RPC 服务器...
✅ 连接成功！

📌 1. 计算器服务测试
远程调用: add(15, 27) = 42
远程调用: subtract(100, 35) = 65
```

---

## 💡 核心功能演示

### 1. 基本计算服务
```python
proxy.add(15, 27)         # 42
proxy.subtract(100, 35)   # 65
proxy.multiply(8, 9)      # 72
proxy.divide(144, 12)     # 12.0
```

### 2. 用户数据服务
```python
# 获取用户
user = proxy.get_user(1)
# 返回: {'id': 1, 'name': '张三', 'age': 25}

# 获取所有用户
users = proxy.list_users()

# 添加新用户
proxy.add_user(4, "赵六", 32, "zhao@example.com")

# 搜索用户
results = proxy.search_users("张")
```

### 3. 数据处理
```python
stats = proxy.process_data([10, 20, 30, 40, 50])
# 返回: {
#   'count': 5,
#   'sum': 150,
#   'average': 30.0,
#   'min': 10,
#   'max': 50
# }
```

### 4. 错误处理
```python
try:
    proxy.divide(10, 0)  # 除以零
except xmlrpc.client.Fault as fault:
    print(f"远程错误: {fault.faultString}")
```

---

## 🚀 进阶使用

### 修改服务器地址

如果要在不同机器上运行：

**服务器端 (rpc_server.py):**
```python
host = "0.0.0.0"  # 监听所有网卡
port = 8000
```

**客户端 (rpc_client.py):**
```python
server_url = "http://192.168.1.100:8000"  # 改成服务器 IP
```

### 添加新功能

**在服务器端添加新函数：**
```python
def my_new_function(param):
    return f"处理: {param}"

server.register_function(my_new_function, "my_new_function")
```

**在客户端调用：**
```python
result = proxy.my_new_function("测试数据")
```

---

## 🎯 实际应用场景

1. **微服务架构** - 服务间通信
2. **分布式计算** - 多台机器协同工作
3. **远程控制** - 控制远程设备
4. **游戏服务器** - 客户端与服务器通信
5. **云服务 API** - 提供远程服务

---

## 📚 技术对比

### RPC vs REST API

| 特性         | RPC            | REST API |
|-------------|----- ----- -----|----------|
| **调用方式** | `getUser(123)`  | `GET /users/123` |
| **风格**     | 函数调用        | 资源操作 |
| **学习曲线** | 简单            | 中等 |
| **性能**     | 较快            | 稍慢 |
| **灵活性**   | 中等            | 高   |

### 其他 RPC 框架

- **gRPC** - Google，高性能，Protocol Buffers
- **Apache Thrift** - Facebook，跨语言
- **JSON-RPC** - 轻量级，基于 JSON
- **XML-RPC** - 本示例使用的，基于 XML

---

## ⚠️ 注意事项

1. **安全性**
   - 生产环境需要添加身份验证
   - 使用 HTTPS 加密通信
   - 验证输入参数

2. **网络问题**
   - 添加超时设置
   - 处理网络异常
   - 实现重试机制

3. **性能优化**
   - 减少远程调用次数
   - 批量处理数据
   - 使用连接池

4. **防火墙**
   - 确保端口开放
   - 配置正确的防火墙规则

---

## 🔧 故障排除

### 问题 1: 连接被拒绝
```
ConnectionRefusedError: [Errno 111] Connection refused
```
**解决：** 确保服务器正在运行

### 问题 2: 端口被占用
```
OSError: [Errno 98] Address already in use
```
**解决：** 更换端口或停止占用端口的程序
```bash
lsof -i :8000  # 查看占用端口的进程
```

### 问题 3: 方法不存在
```
<Fault 1: 'method "xxx" is not supported'>
```
**解决：** 检查服务器是否正确注册了该方法

---

## 📖 学习资源

- **Python 官方文档**: https://docs.python.org/3/library/xmlrpc.html
- **gRPC 官网**: https://grpc.io
- **RPC 原理**: https://en.wikipedia.org/wiki/Remote_procedure_call

---

## ✨ 总结

RPC 让分布式系统的开发变得简单：

1. ✅ **透明性** - 像本地调用一样简单
2. ✅ **灵活性** - 支持各种数据类型
3. ✅ **可扩展** - 轻松添加新功能
4. ✅ **跨平台** - 不同机器间通信

**开始体验：**
```bash
python3 rpc_demo.py
```

享受 RPC 的强大功能吧！🚀

