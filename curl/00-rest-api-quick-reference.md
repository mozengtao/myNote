# 🎯 REST API + curl 快速参考指南

> **系统化掌握 REST API 的心智模型与工程实践**
> 
> 四个核心文件构成完整学习体系：
> 1. [01-rest-api-mental-model.md](./01-rest-api-mental-model.md) - 核心概念与架构理解
> 2. [02-curl-rest-api-mastery.md](./02-curl-rest-api-mastery.md) - curl实战技巧与模板  
> 3. [03-rest-api-network-stack.md](./03-rest-api-network-stack.md) - 网络栈深度理解
> 4. [04-rest-api-design-and-exercises.md](./04-rest-api-design-and-exercises.md) - 设计方法与实战练习

---

## 🧠 核心心智模型

### REST API 本质

```text
REST API = Resource Modeling + HTTP Semantics + Stateless Communication

邮政系统类比：
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│    写信     │    │ HTTP Request │    │   应用层    │
│  (用户)     │ -> │  (协议)      │ -> │  (REST)     │
└─────────────┘    └─────────────┘    └─────────────┘
       ↓                   ↓                   ↓
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   邮政网络  │    │  TCP/IP     │    │   网络栈    │
│  (传输)     │ -> │  (传输)     │ -> │  (系统)     │
└─────────────┘    └─────────────┘    └─────────────┘
```

### 四大约束原则

| 约束 | 系统类比 | 技术要求 | 实践要点 |
|------|----------|----------|----------|
| **Resource** | 文件系统路径 | URI用名词 | `/users/123` |
| **Representation** | 文件编码格式 | 内容协商 | `Accept: application/json` |
| **Stateless** | Unix命令 | 完整请求 | 每次携带认证信息 |
| **Uniform Interface** | 标准工具集 | HTTP语义 | GET/POST/PUT/DELETE |

---

## 📡 网络栈映射

### 技术栈层级

```text
┌─────────────────────────────────┐
│  curl 命令                       │  ← 应用工具
├─────────────────────────────────┤
│  HTTP 协议                      │  ← 应用协议
├─────────────────────────────────┤
│  TCP 传输                       │  ← 可靠传输
├─────────────────────────────────┤
│  IP 路由                        │  ← 网络层
├─────────────────────────────────┤
│  Ethernet                       │  ← 数据链路
└─────────────────────────────────┘
```

### 系统调用路径

```text
curl → socket() → connect() → send() → recv() → close()
```

---

## 🔧 curl 实战模板

### 基础CRUD操作

```bash
# 环境变量设置
export API_BASE="https://api.example.com"
export TOKEN="your-api-token"

# GET - 查询
curl -H "Accept: application/json" \
     -H "Authorization: Bearer $TOKEN" \
     "$API_BASE/users?page=1&limit=10"

# POST - 创建
curl -X POST \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer $TOKEN" \
     -d '{"name":"John","email":"john@example.com"}' \
     "$API_BASE/users"

# PUT - 更新
curl -X PUT \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer $TOKEN" \
     -d '{"name":"John Updated"}' \
     "$API_BASE/users/123"

# DELETE - 删除
curl -X DELETE \
     -H "Authorization: Bearer $TOKEN" \
     "$API_BASE/users/123"
```

### 调试模板

```bash
# 详细调试
curl -v \
     -H "Authorization: Bearer $TOKEN" \
     "$API_BASE/users" 2>&1 | tee debug.log

# 性能测量
curl -w "总时间: %{time_total}s\n连接时间: %{time_connect}s\n" \
     -s -o /dev/null \
     "$API_BASE/users"

# 错误处理
response=$(curl -s -w "HTTPSTATUS:%{http_code}" "$API_BASE/users")
status=$(echo "$response" | sed -n 's/.*HTTPSTATUS:\([0-9]*\)/\1/p')
body=$(echo "$response" | sed 's/HTTPSTATUS:.*//g')
```

---

## 🎯 REST vs RPC 对比

| 维度 | RPC风格 | REST风格 | 系统类比 |
|------|---------|----------|----------|
| **URL设计** | `POST /create_user` | `POST /users` | `create_file` vs `touch file` |
| **操作语义** | 调用远程函数 | 操作资源状态 | 函数调用 vs 文件操作 |
| **扩展性** | 每功能一端点 | 统一资源操作 | 专用工具 vs 标准工具集 |
| **缓存** | 难以缓存 | 天然支持 | 函数调用 vs 文件读取 |

### 你的curl命令分析

```bash
# 原始命令（RPC风格）
curl -X "POST" \
"http://10.254.25.207:4000/create_vrpds?quantity=1&starting_mac=3C:C4:4F:20:00:01&dhcp_option=dhcp"

# 问题：URI包含动词create，参数在query string

# REST改进版本
curl -X POST \
     -H "Content-Type: application/json" \
     -d '{
       "quantity": 1,
       "starting_mac": "3C:C4:4F:20:00:01", 
       "dhcp_option": "dhcp",
       "datastore": {"additionalProp1": {}}
     }' \
     "http://10.254.25.207:4000/api/v1/vrpds"
```

---

## 🚨 常见问题诊断

### 网络问题诊断层级

```text
应用层 → 传输层 → 网络层 → 链路层
curl -v   ss -tuln   ping      ethtool
```

### 错误码快速判断

| 状态码 | 含义 | 常见原因 | 诊断命令 |
|--------|------|----------|----------|
| **Connection refused** | 端口未开放 | 服务未启动 | `telnet host port` |
| **Connection timeout** | 网络不通 | 防火墙/路由 | `ping host` |
| **401** | 认证失败 | Token错误 | 检查Authorization头 |
| **404** | 资源不存在 | URL错误 | 检查endpoint |
| **500** | 服务器错误 | 后端故障 | 查看服务器日志 |

---

## 🏗️ API设计快速检查

### URI设计规范

```text
✅ 正确模式                      ❌ 错误模式
GET  /users                     GET  /getUsers
POST /users                     POST /createUser  
GET  /users/123                 GET  /getUserById/123
PUT  /users/123                 POST /updateUser/123
DELETE /users/123               POST /deleteUser/123

GET  /users/123/orders          GET  /getUserOrders/123
POST /users/123/orders          POST /createOrderForUser/123
```

### 状态码使用

```text
操作类型        成功状态码      错误状态码
GET            200 OK          404 Not Found
POST           201 Created     400 Bad Request, 409 Conflict
PUT            200 OK          400 Bad Request, 404 Not Found  
DELETE         204 No Content  404 Not Found
```

---

## 🛠️ 开发工具集成

### bash函数封装

```bash
# ~/.bashrc 添加
api_get() { curl -H "Authorization: Bearer $API_TOKEN" "$API_BASE/$1" | jq '.'; }
api_post() { curl -X POST -H "Content-Type: application/json" -H "Authorization: Bearer $API_TOKEN" -d "$2" "$API_BASE/$1" | jq '.'; }
api_put() { curl -X PUT -H "Content-Type: application/json" -H "Authorization: Bearer $API_TOKEN" -d "$2" "$API_BASE/$1" | jq '.'; }
api_delete() { curl -X DELETE -H "Authorization: Bearer $API_TOKEN" "$API_BASE/$1"; }

# 使用示例
api_get "users/123"
api_post "users" '{"name":"John","email":"john@example.com"}'
```

### 测试自动化

```bash
#!/bin/bash
# 健康检查脚本
check_api() {
    local endpoint=$1
    local expected=$2
    
    status=$(curl -s -w "%{http_code}" -o /dev/null "$API_BASE$endpoint")
    
    if [ "$status" = "$expected" ]; then
        echo "✓ $endpoint: OK ($status)"
    else
        echo "✗ $endpoint: FAIL ($status, expected $expected)"
    fi
}

check_api "/health" "200"
check_api "/api/users" "200"
```

---

## 📚 学习验证清单

### 基础水平（能调用API）
- [ ] 理解REST vs RPC区别
- [ ] 能发送基础CRUD请求  
- [ ] 会设置HTTP头和认证

### 中级水平（能调试API）  
- [ ] 熟练使用curl -v调试
- [ ] 能分析网络连接问题
- [ ] 会编写bash脚本自动化

### 高级水平（能设计API）
- [ ] 能设计RESTful URI结构
- [ ] 理解HTTP状态码语义
- [ ] 能优化API性能

### 专家水平（能架构系统）
- [ ] 深度理解网络栈原理
- [ ] 能设计分布式API架构
- [ ] 能解决复杂的网络问题

---

## 🎯 一句话精华

> **REST API就是用HTTP协议操作抽象资源的分布式通信方式，而curl是这个世界的瑞士军刀。**

### 记忆口诀

```text
资源用名词，操作用动词（HTTP Method）
状态无保存，接口要统一
curl加-v，调试无忧虑
网络分层次，问题逐级查
```

---

**🚀 现在你已经掌握了从底层原理到实际应用的完整REST API知识体系！**

去实践中应用这些知识吧，在真实项目中不断深化理解。记住：**最好的学习是在解决真实问题的过程中发生的。**