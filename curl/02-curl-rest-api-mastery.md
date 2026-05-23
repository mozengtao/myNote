# 🔧 curl + REST API 实战精通指南

## 🎯 核心类比：curl 就像瑞士军刀

```text
瑞士军刀                          curl
┌─────────────┐                  ┌─────────────┐
│    主刀     │                  │  基础请求    │
│   (基础)    │                  │ GET/POST     │
├─────────────┤                  ├─────────────┤
│   螺丝刀    │                  │   Headers    │
│  (精确)     │                  │   (精确)     │
├─────────────┤                  ├─────────────┤
│   开瓶器    │                  │   调试功能   │
│  (专用)     │                  │  -v, -i      │
├─────────────┤                  ├─────────────┤
│   钳子      │                  │   数据处理   │
│  (处理)     │                  │  -d, -F      │
└─────────────┘                  └─────────────┘
```

**核心理念**：curl不只是发请求的工具，更是REST API的**系统级调试器**。

---

## 🏗️ 第一部分：curl 基础架构理解

### curl的系统调用链路

```text
curl命令执行流程：

┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│  curl 解析参数   │ -> │  构建HTTP请求   │ -> │   系统调用      │
│  -X, -H, -d     │    │  Headers+Body   │    │ socket/connect  │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         ↓                        ↓                        ↓
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│  发送到服务器   │ <- │   TCP/IP传输    │ <- │  内核网络栈     │
│  等待响应       │    │   可靠传输      │    │  路由/传输      │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

### curl vs 其他HTTP工具对比

```text
工具特性对比表：
┌──────────┬────────────┬────────────┬────────────┐
│   工具   │    用途    │    优势    │    缺点    │
├──────────┼────────────┼────────────┼────────────┤
│   curl   │  命令行    │ 万能调试   │ 语法复杂   │
│  wget    │  下载文件  │ 简单下载   │ 功能有限   │
│ httpie   │  API测试   │ 语法友好   │ 功能不全   │
│ postman  │  GUI测试   │ 界面友好   │ 不利自动化 │
└──────────┴────────────┴────────────┴────────────┘

curl的独特价值：
- 系统原生，无需安装额外工具
- 支持所有HTTP特性
- 可与shell脚本完美集成
- 提供详细调试信息
```

---

## 🎪 第二部分：curl REST API 实战模式

### 模式1：GET请求 - 读取资源

#### 基础GET请求
```bash
# 最简单的GET请求
curl http://api.example.com/users

# 等价的完整写法
curl -X GET \
     -H "Accept: application/json" \
     http://api.example.com/users
```

#### 带查询参数的GET
```bash
# 查询参数 - 分页
curl "http://api.example.com/users?page=2&limit=10"

# 查询参数 - 过滤
curl "http://api.example.com/users?status=active&role=admin"

# URL编码处理
curl "http://api.example.com/search?q=hello%20world"
```

#### 带认证的GET
```bash
# Bearer Token认证
curl -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..." \
     http://api.example.com/users/profile

# Basic Auth认证
curl -u username:password http://api.example.com/users

# API Key认证
curl -H "X-API-Key: your-api-key-here" \
     http://api.example.com/users
```

### 模式2：POST请求 - 创建资源

#### JSON数据POST
```bash
# 创建用户
curl -X POST \
     -H "Content-Type: application/json" \
     -H "Accept: application/json" \
     -d '{
       "name": "John Doe",
       "email": "john@example.com",
       "role": "user"
     }' \
     http://api.example.com/users
```

#### 从文件读取数据
```bash
# 从文件读取JSON
curl -X POST \
     -H "Content-Type: application/json" \
     -d @user.json \
     http://api.example.com/users

# user.json内容：
{
  "name": "John Doe",
  "email": "john@example.com"
}
```

#### 表单数据POST
```bash
# application/x-www-form-urlencoded
curl -X POST \
     -H "Content-Type: application/x-www-form-urlencoded" \
     -d "name=John&email=john@example.com" \
     http://api.example.com/users

# 或者使用-F（自动设置multipart/form-data）
curl -X POST \
     -F "name=John" \
     -F "email=john@example.com" \
     http://api.example.com/users
```

### 模式3：PUT/PATCH请求 - 更新资源

#### PUT - 完整更新（幂等）
```bash
# 完整替换资源
curl -X PUT \
     -H "Content-Type: application/json" \
     -d '{
       "name": "John Updated",
       "email": "john.updated@example.com",
       "role": "admin"
     }' \
     http://api.example.com/users/123
```

#### PATCH - 部分更新
```bash
# 只更新特定字段
curl -X PATCH \
     -H "Content-Type: application/json" \
     -d '{
       "email": "new.email@example.com"
     }' \
     http://api.example.com/users/123
```

### 模式4：DELETE请求 - 删除资源

```bash
# 删除指定资源
curl -X DELETE \
     -H "Authorization: Bearer token..." \
     http://api.example.com/users/123

# 软删除（通过PATCH标记）
curl -X PATCH \
     -H "Content-Type: application/json" \
     -d '{"deleted": true}' \
     http://api.example.com/users/123
```

---

## 🔍 第三部分：curl高级调试技巧

### 调试模式对比

```text
调试级别                         使用场景
┌─────────────────┐             ┌─────────────────┐
│   -v (verbose)  │             │   全面调试      │
│   显示完整交互  │      ->     │   网络问题排查   │
└─────────────────┘             └─────────────────┘

┌─────────────────┐             ┌─────────────────┐
│ -i (include)    │             │   查看响应头    │
│ 只显示响应头    │      ->     │   状态码调试    │
└─────────────────┘             └─────────────────┘

┌─────────────────┐             ┌─────────────────┐
│ -s (silent)     │             │   脚本自动化    │
│ 静默模式       │      ->     │   只要响应体    │
└─────────────────┘             └─────────────────┘

┌─────────────────┐             ┌─────────────────┐
│ --trace-ascii   │             │   深度调试      │
│ ASCII跟踪      │      ->     │   协议级问题    │
└─────────────────┘             └─────────────────┘
```

### 实战调试案例

#### 案例1：API返回404问题排查

```bash
# 步骤1：使用-v查看完整请求
curl -v http://api.example.com/users/999

# 输出分析：
# > GET /users/999 HTTP/1.1        <- 请求行
# > Host: api.example.com          <- 请求头  
# > User-Agent: curl/7.68.0        <- User-Agent
# > Accept: */*                    <- Accept头
# > 
# < HTTP/1.1 404 Not Found         <- 响应状态
# < Content-Type: application/json <- 响应头
# < Content-Length: 45             <- 响应长度
# < 
# {"error": "User not found"}      <- 响应体

# 步骤2：检查URL是否正确
curl -v http://api.example.com/users/123  # 存在的用户ID

# 步骤3：检查API文档，确认endpoint格式
curl -v http://api.example.com/api/v1/users/999  # 可能需要版本前缀
```

#### 案例2：POST请求400错误排查

```bash
# 问题请求
curl -X POST \
     -H "Content-Type: application/json" \
     -d '{"name":"John"}' \
     -v \
     http://api.example.com/users

# 典型400响应分析：
# < HTTP/1.1 400 Bad Request
# < Content-Type: application/json
# < 
# {"error": "email is required"}

# 修正请求
curl -X POST \
     -H "Content-Type: application/json" \
     -d '{
       "name": "John",
       "email": "john@example.com"
     }' \
     -v \
     http://api.example.com/users
```

#### 案例3：认证失败排查

```bash
# 401问题
curl -H "Authorization: Bearer invalid-token" \
     -v \
     http://api.example.com/users

# 输出分析：
# < HTTP/1.1 401 Unauthorized
# < WWW-Authenticate: Bearer realm="api"
# < 
# {"error": "Invalid token"}

# 检查token格式
echo "Bearer token" | base64 -d  # 如果是JWT，可以解码查看

# 使用正确token
curl -H "Authorization: Bearer valid-token" \
     -v \
     http://api.example.com/users
```

### 高级调试技巧

#### 保存请求和响应
```bash
# 保存响应体到文件
curl http://api.example.com/users > users.json

# 保存响应头到文件  
curl -D headers.txt http://api.example.com/users

# 同时保存头和体
curl -D headers.txt -o body.json http://api.example.com/users

# 保存完整交互过程
curl --trace-ascii trace.log http://api.example.com/users
```

#### 测量性能
```bash
# 查看详细时间信息
curl -w "Total time: %{time_total}s\nDNS lookup: %{time_namelookup}s\nConnect: %{time_connect}s\nSSL: %{time_appconnect}s\nTransfer: %{time_starttransfer}s\n" \
     http://api.example.com/users

# 输出示例：
# Total time: 0.156s
# DNS lookup: 0.003s  
# Connect: 0.028s
# SSL: 0.089s
# Transfer: 0.156s
```

---

## 🔗 第四部分：curl + 系统工具集成

### curl + jq：JSON处理神器

```bash
# 从API获取数据并美化输出
curl -s http://api.example.com/users | jq '.'

# 提取特定字段
curl -s http://api.example.com/users | jq '.data[].name'

# 过滤和统计
curl -s http://api.example.com/users | jq '.data | length'
curl -s http://api.example.com/users | jq '.data[] | select(.role == "admin")'

# 实战：批量操作
curl -s http://api.example.com/users | jq -r '.data[].id' | while read user_id; do
  curl -X DELETE "http://api.example.com/users/$user_id"
done
```

### curl + bash：自动化脚本

```bash
#!/bin/bash
# API健康检查脚本

API_BASE="http://api.example.com"
TOKEN="your-api-token"

# 检查API状态
check_endpoint() {
    local endpoint=$1
    local expected_status=$2
    
    status=$(curl -s -w "%{http_code}" -o /dev/null "$API_BASE$endpoint")
    
    if [ "$status" = "$expected_status" ]; then
        echo "✓ $endpoint: OK ($status)"
    else
        echo "✗ $endpoint: FAIL ($status, expected $expected_status)"
    fi
}

# 执行检查
check_endpoint "/health" "200"
check_endpoint "/api/users" "200" 
check_endpoint "/api/orders" "200"
```

### curl + 系统监控

```bash
# 持续监控API响应时间
while true; do
    response_time=$(curl -w "%{time_total}" -s -o /dev/null http://api.example.com/health)
    echo "$(date): Response time: ${response_time}s"
    sleep 5
done

# 结合系统工具监控
curl -w "@curl-format.txt" -s http://api.example.com/users

# curl-format.txt内容:
#     time_namelookup:  %{time_namelookup}s\n
#        time_connect:  %{time_connect}s\n
#     time_appconnect:  %{time_appconnect}s\n
#    time_pretransfer:  %{time_pretransfer}s\n
#       time_redirect:  %{time_redirect}s\n
#  time_starttransfer:  %{time_starttransfer}s\n
#                     ----------\n
#          time_total:  %{time_total}s\n
```

---

## 📋 第五部分：curl REST API 标准模板

### 通用模板集合

```bash
# 模板1：GET请求（读取）
curl_get() {
    curl -s \
         -H "Accept: application/json" \
         -H "Authorization: Bearer ${API_TOKEN}" \
         "${API_BASE}/$1"
}

# 使用：curl_get "users/123"

# 模板2：POST请求（创建）
curl_post() {
    curl -s \
         -X POST \
         -H "Content-Type: application/json" \
         -H "Authorization: Bearer ${API_TOKEN}" \
         -d "$2" \
         "${API_BASE}/$1"
}

# 使用：curl_post "users" '{"name":"John","email":"john@example.com"}'

# 模板3：PUT请求（更新）
curl_put() {
    curl -s \
         -X PUT \
         -H "Content-Type: application/json" \
         -H "Authorization: Bearer ${API_TOKEN}" \
         -d "$2" \
         "${API_BASE}/$1"
}

# 模板4：DELETE请求（删除）
curl_delete() {
    curl -s \
         -X DELETE \
         -H "Authorization: Bearer ${API_TOKEN}" \
         "${API_BASE}/$1"
}

# 模板5：调试模式请求
curl_debug() {
    curl -v \
         -H "Accept: application/json" \
         -H "Authorization: Bearer ${API_TOKEN}" \
         "${API_BASE}/$1" 2>&1 | tee debug.log
}
```

### 错误处理模板

```bash
# 带错误处理的请求函数
api_request() {
    local method=$1
    local endpoint=$2
    local data=$3
    
    response=$(curl -s -w "HTTPSTATUS:%{http_code}" \
                    -X "$method" \
                    -H "Content-Type: application/json" \
                    -H "Authorization: Bearer ${API_TOKEN}" \
                    ${data:+-d "$data"} \
                    "${API_BASE}/$endpoint")
    
    # 分离响应体和状态码
    body=$(echo "$response" | sed 's/HTTPSTATUS:.*//g')
    status=$(echo "$response" | tr -d '\n' | sed -e 's/.*HTTPSTATUS://')
    
    # 错误处理
    case "$status" in
        200|201|204) 
            echo "$body" | jq '.'
            ;;
        400)
            echo "错误: 请求参数有误" >&2
            echo "$body" | jq '.error' >&2
            return 1
            ;;
        401)
            echo "错误: 认证失败，请检查token" >&2
            return 1
            ;;
        404)
            echo "错误: 资源不存在" >&2
            return 1
            ;;
        500)
            echo "错误: 服务器内部错误" >&2
            echo "$body" | jq '.error' >&2
            return 1
            ;;
        *)
            echo "未知错误: HTTP $status" >&2
            echo "$body" >&2
            return 1
            ;;
    esac
}

# 使用示例
api_request GET "users/123"
api_request POST "users" '{"name":"John","email":"john@example.com"}'
```

---

## 🏆 curl掌握程度自测

### 初级水平（能调用API）
- [ ] 能发送基本GET/POST请求
- [ ] 会设置Content-Type和Authorization头
- [ ] 能处理JSON数据

### 中级水平（能调试API）
- [ ] 熟练使用-v参数调试
- [ ] 能分析HTTP状态码和错误信息
- [ ] 会保存和分析请求响应

### 高级水平（能自动化API）
- [ ] 编写bash脚本自动化API调用
- [ ] 结合jq处理复杂JSON数据
- [ ] 实现错误处理和重试机制
- [ ] 能监控和测试API性能

### 专家水平（能系统集成）
- [ ] 深度理解HTTP协议细节
- [ ] 能从网络层面分析API问题
- [ ] 设计复杂的API测试框架
- [ ] 能优化API调用性能

---

**下一步预告**：第三部分将深入网络栈，从系统调用角度理解REST API的底层机制，以及实际的API设计方法和最佳实践。