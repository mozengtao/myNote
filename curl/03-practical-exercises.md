# curl 实战练习：从基础到精通

---

## 🧪 练习设计理念

这些练习按照真实工程场景设计，每个练习都对应生产环境中的常见需求：
1. **练习1** - API 调试基础（GET 请求和参数处理）
2. **练习2** - 服务集成实战（POST 请求和认证）
3. **练习3** - 故障排查技能（网络调试和错误定位）

---

## 📋 练习 1：构造一个 GET 请求 - API 探索基础

### 🎯 目标
- 理解 query 参数的构造和编码
- 掌握 HTTP headers 的设置
- 学会分析 API 响应结构

### 📋 任务背景
你需要调用一个用户管理API，获取特定条件下的用户列表。API 文档如下：

```
GET /api/v1/users
Query Parameters:
- page: 页码 (默认1)  
- limit: 每页条数 (默认10)
- role: 用户角色过滤 (admin/user/guest)
- active: 是否激活 (true/false)
- created_after: 创建时间过滤 (ISO 8601格式)

Headers:
- Authorization: Bearer <token>
- Accept: application/json
- User-Agent: 自定义客户端标识
```

### 💡 练习步骤

#### 步骤 1：基础 GET 请求
```bash
# 目标：获取第1页的10个用户
curl "http://jsonplaceholder.typicode.com/users"

# 分析响应：
# - 观察返回的 JSON 结构
# - 注意 Content-Type header
# - 记录响应时间
```

#### 步骤 2：添加 Query 参数
```bash
# 手动构造 URL
curl "http://jsonplaceholder.typicode.com/users?_page=2&_limit=5"

# 使用 -G 和 -d 参数（推荐方式）
curl -G "http://jsonplaceholder.typicode.com/users" \
  -d "_page=2" \
  -d "_limit=5"

# 对比两种方式的差异：
# - 手动构造：直观但容易出错
# - -G -d：自动URL编码，更安全
```

#### 步骤 3：处理特殊字符和编码
```bash
# 如果参数包含特殊字符
curl -G "http://httpbin.org/get" \
  -d "name=张三" \
  -d "email=user@example.com" \
  -d "search=hello world & test"

# 观察 httpbin 返回的 args 字段，看URL编码效果
```

#### 步骤 4：设置 Headers
```bash
# 完整的 API 调用
curl -G "http://httpbin.org/get" \
  -H "Accept: application/json" \
  -H "User-Agent: MyApp/1.0.0 (Learning curl)" \
  -H "Authorization: Bearer fake-token-for-testing" \
  -d "page=1" \
  -d "limit=10" \
  -d "role=admin"

# 分析响应中的 headers 字段，确认服务器收到了我们的 headers
```

### 🔍 进阶挑战

#### 挑战 1：时间参数处理
```bash
# 获取2023年1月1日之后创建的用户
CREATED_AFTER=$(date -d "2023-01-01" --iso-8601)
curl -G "http://httpbin.org/get" \
  -d "created_after=$CREATED_AFTER" \
  -H "Accept: application/json"
```

#### 挑战 2：响应处理和错误检查
```bash
#!/bin/bash
# 创建一个健壮的 API 调用脚本

API_BASE="http://httpbin.org"
TOKEN="your-api-token"

# 函数：调用用户API
call_users_api() {
    local page=${1:-1}
    local limit=${2:-10}
    local role=${3:-""}
    
    echo "Fetching users: page=$page, limit=$limit, role=$role"
    
    # 构建参数数组
    local params=("-d" "page=$page" "-d" "limit=$limit")
    if [ -n "$role" ]; then
        params+=("-d" "role=$role")
    fi
    
    # 发起请求并检查状态
    local response=$(curl -s -w "\n%{http_code}" -G "$API_BASE/get" \
        -H "Accept: application/json" \
        -H "Authorization: Bearer $TOKEN" \
        "${params[@]}")
    
    # 分离响应体和状态码
    local response_body=$(echo "$response" | head -n -1)
    local http_code=$(echo "$response" | tail -n 1)
    
    # 检查HTTP状态码
    if [ "$http_code" -eq 200 ]; then
        echo "✓ API call successful"
        echo "$response_body" | jq '.'
    else
        echo "✗ API call failed (HTTP $http_code)"
        echo "$response_body"
        return 1
    fi
}

# 测试不同场景
call_users_api 1 5 "admin"
call_users_api 2 10
call_users_api 999 10  # 测试不存在的页面
```

### 📊 学习要点总结

1. **Query 参数构造**：
   - 手动拼接 vs `-G -d` 自动编码
   - 特殊字符的URL编码处理
   - 参数的条件性添加

2. **Headers 管理**：
   - Accept 头指定期望的响应格式
   - User-Agent 标识客户端
   - Authorization 处理认证

3. **响应处理**：
   - HTTP状态码检查
   - JSON响应的解析（配合jq）
   - 错误情况的处理

---

## 🔧 练习 2：调用一个 POST API - 服务集成实战

### 🎯 目标
- 掌握 JSON body 的构造和发送
- 理解不同认证方式的实现
- 学会处理复杂的请求/响应场景

### 📋 任务背景
你需要为一个用户管理系统实现注册和登录功能。API规格：

```
POST /api/v1/users (注册)
Content-Type: application/json
Body: {
  "username": "string",
  "email": "string", 
  "password": "string",
  "profile": {
    "firstName": "string",
    "lastName": "string",
    "department": "string"
  }
}

POST /api/v1/auth/login (登录)
Content-Type: application/json  
Body: {
  "username": "string",
  "password": "string"
}

Response: {
  "token": "jwt-token",
  "expiresIn": 3600,
  "user": { ... }
}
```

### 💡 练习步骤

#### 步骤 1：基础 POST 请求
```bash
# 简单的 JSON POST
curl -X POST "http://httpbin.org/post" \
  -H "Content-Type: application/json" \
  -d '{"message": "Hello World", "timestamp": "2024-01-01T00:00:00Z"}'

# 分析响应：
# - 观察 json 字段（服务器解析的结果）
# - 查看 data 字段（原始请求体）
# - 注意 Content-Length 的自动计算
```

#### 步骤 2：从文件读取 JSON
```bash
# 创建用户数据文件
cat > new_user.json << 'EOF'
{
  "username": "johndoe",
  "email": "john.doe@example.com",
  "password": "SecurePass123!",
  "profile": {
    "firstName": "John",
    "lastName": "Doe", 
    "department": "Engineering"
  }
}
EOF

# 从文件发送 JSON
curl -X POST "http://httpbin.org/post" \
  -H "Content-Type: application/json" \
  -d @new_user.json

# 从标准输入发送（管道）
cat new_user.json | curl -X POST "http://httpbin.org/post" \
  -H "Content-Type: application/json" \
  -d @-
```

#### 步骤 3：动态构造 JSON
```bash
#!/bin/bash
# 交互式用户注册脚本

read -p "Username: " username
read -p "Email: " email  
read -s -p "Password: " password
echo
read -p "First Name: " firstName
read -p "Last Name: " lastName
read -p "Department: " department

# 使用 jq 构造 JSON（推荐方式）
json_payload=$(jq -n \
  --arg username "$username" \
  --arg email "$email" \
  --arg password "$password" \
  --arg firstName "$firstName" \
  --arg lastName "$lastName" \
  --arg department "$department" \
  '{
    username: $username,
    email: $email,
    password: $password,
    profile: {
      firstName: $firstName,
      lastName: $lastName,
      department: $department
    }
  }')

echo "Registering user..."
curl -X POST "http://httpbin.org/post" \
  -H "Content-Type: application/json" \
  -d "$json_payload" \
  -w "Response time: %{time_total}s\n"
```

#### 步骤 4：认证流程实现
```bash
#!/bin/bash
# 完整的登录+认证流程

API_BASE="http://httpbin.org"
USERNAME="testuser"
PASSWORD="testpass"

# 1. 登录获取token
echo "Step 1: Logging in..."
login_response=$(curl -s -X POST "$API_BASE/post" \
  -H "Content-Type: application/json" \
  -d "{\"username\": \"$USERNAME\", \"password\": \"$PASSWORD\"}")

# 模拟从响应中提取token（实际应用中会从真实API响应解析）
TOKEN="fake-jwt-token-for-demo"
echo "Login successful. Token: ${TOKEN:0:20}..."

# 2. 使用token调用受保护的API
echo "Step 2: Accessing protected resource..."
curl -X GET "$API_BASE/bearer" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Accept: application/json"

# 3. token过期处理示例
echo "Step 3: Handling token refresh..."
refresh_token_if_needed() {
    local response=$(curl -s -w "%{http_code}" -o /tmp/api_response \
      -H "Authorization: Bearer $TOKEN" \
      "$API_BASE/status/401")  # 模拟401错误
    
    local http_code="${response: -3}"
    
    if [ "$http_code" = "401" ]; then
        echo "Token expired, refreshing..."
        # 这里会调用refresh token API
        TOKEN="new-refreshed-token"
        echo "Token refreshed: ${TOKEN:0:20}..."
    fi
}

refresh_token_if_needed
```

### 🔍 进阶挑战

#### 挑战 1：文件上传（multipart/form-data）
```bash
# 创建测试文件
echo "User profile data" > profile.txt
echo '{"metadata": "test"}' > metadata.json

# 多部分表单上传
curl -X POST "http://httpbin.org/post" \
  -F "profile=@profile.txt" \
  -F "metadata=@metadata.json;type=application/json" \
  -F "username=johndoe" \
  -F "department=Engineering"

# 分析响应中的 files 和 form 字段
```

#### 挑战 2：批量操作
```bash
#!/bin/bash
# 批量创建用户

users=(
  "alice:alice@example.com:Engineering"
  "bob:bob@example.com:Marketing" 
  "charlie:charlie@example.com:Sales"
)

for user_data in "${users[@]}"; do
    IFS=':' read -r username email department <<< "$user_data"
    
    echo "Creating user: $username"
    
    json_payload=$(jq -n \
      --arg username "$username" \
      --arg email "$email" \
      --arg department "$department" \
      '{
        username: $username,
        email: $email,
        password: "DefaultPass123!",
        profile: {
          firstName: $username,
          lastName: "User",
          department: $department
        }
      }')
    
    response=$(curl -s -w "%{http_code}" -o /tmp/create_response \
      -X POST "http://httpbin.org/post" \
      -H "Content-Type: application/json" \
      -d "$json_payload")
    
    http_code="${response: -3}"
    
    if [ "$http_code" = "200" ]; then
        echo "✓ User $username created successfully"
    else
        echo "✗ Failed to create user $username (HTTP $http_code)"
        cat /tmp/create_response
    fi
    
    sleep 0.5  # 避免API限流
done
```

### 📊 学习要点总结

1. **JSON 数据处理**：
   - 内联JSON vs 文件JSON vs 动态构造
   - 使用 jq 安全构造JSON（避免注入）
   - Content-Type 的正确设置

2. **认证机制**：
   - Bearer Token 的使用
   - Token 过期和刷新处理
   - 错误状态码的响应

3. **高级特性**：
   - multipart/form-data 文件上传
   - 批量操作的错误处理
   - API限流的考虑

---

## 🚨 练习 3：调试失败请求 - 故障排查技能

### 🎯 目标  
- 掌握使用 curl + tcpdump + strace 进行网络调试
- 学会分层定位网络问题
- 建立系统化的故障排查思路

### 📋 任务背景
生产环境中，你的应用调用一个关键API时出现间歇性失败。错误现象：
- 有时请求成功，有时失败
- 失败时没有明确的错误信息
- 问题似乎与网络相关

你需要使用 curl 和相关工具定位问题根因。

### 💡 练习步骤

#### 步骤 1：复现问题和基础诊断
```bash
# 创建一个可能失败的测试场景
# 我们使用 httpbin 的延迟接口模拟不稳定的API

API_URL="http://httpbin.org/delay/3"  # 延迟3秒响应

echo "=== 基础诊断 ==="

# 1. 基础请求测试
echo "Testing basic request..."
time curl -v "$API_URL"

# 2. 短超时测试（模拟超时失败）
echo -e "\n=== Testing with short timeout ==="
curl -v --connect-timeout 2 --max-time 2 "$API_URL"
echo "Exit code: $?"

# 3. 记录详细时序
echo -e "\n=== Detailed timing analysis ==="
curl -w "
DNS lookup:        %{time_namelookup}s
TCP connect:       %{time_connect}s
SSL handshake:     %{time_appconnect}s
Pre-transfer:      %{time_pretransfer}s
Start transfer:    %{time_starttransfer}s
Total time:        %{time_total}s
HTTP code:         %{http_code}
" -o /dev/null -s "$API_URL"
```

#### 步骤 2：网络层调试 (tcpdump)
```bash
#!/bin/bash
# 网络包分析脚本

TARGET_HOST="httpbin.org"
TARGET_PORT="80"

echo "=== 网络层调试 ==="

# 启动抓包（需要root权限）
start_packet_capture() {
    echo "Starting packet capture..."
    # 在后台启动tcpdump
    sudo tcpdump -i any -w /tmp/curl_debug.pcap \
      host "$TARGET_HOST" and port "$TARGET_PORT" &
    TCPDUMP_PID=$!
    echo "tcpdump started (PID: $TCPDUMP_PID)"
    sleep 1  # 等待tcpdump启动
}

# 停止抓包并分析
stop_and_analyze_capture() {
    echo "Stopping packet capture..."
    sudo kill $TCPDUMP_PID 2>/dev/null
    sleep 1
    
    echo "Analyzing captured packets..."
    # 分析TCP连接建立
    echo "=== TCP Connection Analysis ==="
    sudo tcpdump -r /tmp/curl_debug.pcap -nn | grep -E "(SYN|FIN|RST)"
    
    echo "=== HTTP Request/Response Analysis ==="
    sudo tcpdump -r /tmp/curl_debug.pcap -A | grep -E "(GET|POST|HTTP)"
}

# 如果有sudo权限，进行网络分析
if sudo -n true 2>/dev/null; then
    start_packet_capture
    
    # 发起请求
    echo "Making request while capturing..."
    curl -v "http://$TARGET_HOST/delay/1"
    
    stop_and_analyze_capture
else
    echo "No sudo access, skipping packet capture"
fi
```

#### 步骤 3：系统调用追踪 (strace)
```bash
#!/bin/bash
echo "=== 系统调用分析 ==="

# 追踪curl的系统调用
echo "Tracing system calls..."
strace -e trace=network,read,write -o /tmp/curl_strace.log \
  curl -s "http://httpbin.org/delay/1" > /dev/null

echo "=== Network System Calls ==="
grep -E "(socket|connect|send|recv)" /tmp/curl_strace.log

echo -e "\n=== Connection Timeline ==="
# 分析连接建立过程
echo "Socket creation:"
grep "socket(" /tmp/curl_strace.log

echo "Connection attempt:"
grep "connect(" /tmp/curl_strace.log

echo "Data transmission:"
grep -E "(write|send)" /tmp/curl_strace.log | head -3

echo "Data reception:"
grep -E "(read|recv)" /tmp/curl_strace.log | head -3
```

#### 步骤 4：DNS 问题诊断
```bash
#!/bin/bash
echo "=== DNS 诊断 ==="

TARGET_HOST="httpbin.org"

# 1. DNS解析测试
echo "DNS Resolution Test:"
dig "$TARGET_HOST" A +short
dig "$TARGET_HOST" AAAA +short

# 2. DNS解析时间测试
echo -e "\nDNS Resolution Timing:"
time nslookup "$TARGET_HOST" > /dev/null

# 3. curl 的DNS解析行为
echo -e "\ncurl DNS behavior:"
curl -w "DNS lookup time: %{time_namelookup}s\n" \
  -o /dev/null -s "http://$TARGET_HOST/get"

# 4. 使用IP绕过DNS
echo -e "\nBypassing DNS (using IP):"
IP=$(dig +short "$TARGET_HOST" | head -1)
if [ -n "$IP" ]; then
    echo "Resolved IP: $IP"
    curl -w "Connect time (IP): %{time_connect}s\n" \
      -H "Host: $TARGET_HOST" \
      -o /dev/null -s "http://$IP/get"
else
    echo "Failed to resolve IP"
fi
```

#### 步骤 5：SSL/TLS 问题诊断
```bash
#!/bin/bash
echo "=== SSL/TLS 诊断 ==="

HTTPS_TARGET="https://httpbin.org/get"

# 1. SSL连接时间分析
echo "SSL Connection Timing:"
curl -w "
DNS lookup:     %{time_namelookup}s
TCP connect:    %{time_connect}s  
SSL handshake:  %{time_appconnect}s
Total:          %{time_total}s
" -o /dev/null -s "$HTTPS_TARGET"

# 2. SSL证书信息
echo -e "\nSSL Certificate Info:"
curl -vvv "$HTTPS_TARGET" 2>&1 | grep -E "(certificate|SSL|TLS)"

# 3. 支持的SSL版本测试
echo -e "\nSSL Version Tests:"
for version in 1.0 1.1 1.2 1.3; do
    echo -n "TLS $version: "
    if curl -s --tlsv$version --tls-max $version "$HTTPS_TARGET" >/dev/null 2>&1; then
        echo "✓ Supported"
    else
        echo "✗ Not supported"
    fi
done
```

### 🔍 进阶挑战

#### 挑战 1：间歇性故障调试
```bash
#!/bin/bash
# 模拟间歇性故障的调试

echo "=== 间歇性故障调试 ==="

TARGET_URL="http://httpbin.org/status/200,200,200,500"  # 25%概率返回500

# 统计测试
declare -A results
total_requests=20

for i in $(seq 1 $total_requests); do
    response=$(curl -s -w "%{http_code}" -o /dev/null "$TARGET_URL")
    results[$response]=$((${results[$response]} + 1))
    echo -n "."
done

echo -e "\n\n=== 结果统计 ==="
for code in "${!results[@]}"; do
    percentage=$((${results[$code]} * 100 / total_requests))
    echo "HTTP $code: ${results[$code]} 次 ($percentage%)"
done

# 失败案例详细分析
echo -e "\n=== 失败案例分析 ==="
echo "Testing a known failure case..."
curl -v "http://httpbin.org/status/500" 2>&1 | \
    grep -E "(HTTP|Server|Date)" || echo "No server info available"
```

#### 挑战 2：网络路径诊断
```bash
#!/bin/bash
# 网络路径和延迟分析

echo "=== 网络路径诊断 ==="

TARGET_HOST="httpbin.org"

# 1. 路由跟踪
echo "Traceroute to $TARGET_HOST:"
traceroute "$TARGET_HOST" 2>/dev/null | head -10

# 2. 连接延迟测试
echo -e "\n=== 连接延迟测试 ==="
for i in {1..5}; do
    time_connect=$(curl -w "%{time_connect}" -o /dev/null -s "http://$TARGET_HOST/get")
    echo "Attempt $i: ${time_connect}s"
    sleep 1
done

# 3. 并发连接测试
echo -e "\n=== 并发连接测试 ==="
for concurrent in 1 5 10; do
    echo "Testing $concurrent concurrent connections..."
    start_time=$(date +%s.%N)
    
    for i in $(seq 1 $concurrent); do
        curl -s "http://$TARGET_HOST/get" > /dev/null &
    done
    wait  # 等待所有后台任务完成
    
    end_time=$(date +%s.%N)
    duration=$(echo "$end_time - $start_time" | bc)
    echo "  $concurrent connections completed in ${duration}s"
done
```

### 📊 故障排查决策树

```text
网络请求失败
       │
       ▼
┌─ DNS解析问题? ────→ 使用IP直接访问测试
│  (time_namelookup > 2s)   │
│                          └─ 如果IP访问成功 → DNS配置问题
│
├─ TCP连接问题? ────→ 检查防火墙/端口
│  (connect失败)           │
│                          └─ ping + telnet 测试网络连通性
│
├─ SSL/TLS问题? ────→ 检查证书和协议版本
│  (SSL handshake失败)     │
│                          └─ 使用--insecure测试
│
├─ HTTP协议问题? ───→ 检查请求格式
│  (4xx状态码)             │
│                          └─ 对比工作的请求
│
└─ 服务器问题? ─────→ 检查服务器日志
   (5xx状态码或超时)        │
                           └─ 联系服务提供商
```

### 📊 学习要点总结

1. **分层调试思路**：
   - 网络层：tcpdump 分析包传输
   - 系统调用层：strace 追踪内核交互
   - 应用层：curl -v 查看HTTP协议

2. **常用诊断命令**：
   - `curl -v` - HTTP层调试
   - `curl -w` - 性能分析
   - `tcpdump` - 网络包分析
   - `strace` - 系统调用追踪

3. **故障定位策略**：
   - 从底层到高层逐层排查
   - 使用统计方法分析间歇性问题
   - 保存调试日志供后续分析

---

## 🎯 综合实战项目

创建一个完整的监控脚本，集成所有学到的技能：

```bash
#!/bin/bash
# 生产级API监控脚本

CONFIG_FILE="api_monitor.conf"
LOG_FILE="api_monitor.log"

# 加载配置
source "$CONFIG_FILE" 2>/dev/null || {
    echo "Creating default config file..."
    cat > "$CONFIG_FILE" << 'EOF'
# API监控配置
MONITOR_URLS=(
    "https://api.github.com/zen"
    "https://httpbin.org/get"
    "https://jsonplaceholder.typicode.com/users/1"
)
CHECK_INTERVAL=60
TIMEOUT=30
RETRY_COUNT=3
ALERT_THRESHOLD=2
EOF
    echo "Please edit $CONFIG_FILE and run again"
    exit 1
}

# 监控函数
monitor_api() {
    local url=$1
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    # 执行请求并收集指标
    local response=$(curl -s -w "
%{http_code}|%{time_total}|%{time_connect}|%{time_starttransfer}
" --max-time "$TIMEOUT" --retry "$RETRY_COUNT" "$url")
    
    local body=$(echo "$response" | head -n -1)
    local metrics=$(echo "$response" | tail -n 1)
    
    IFS='|' read -r http_code time_total time_connect time_starttransfer <<< "$metrics"
    
    # 判断健康状态
    local status="OK"
    if [[ ! "$http_code" =~ ^2[0-9][0-9]$ ]]; then
        status="ERROR"
    elif (( $(echo "$time_total > $ALERT_THRESHOLD" | bc -l) )); then
        status="SLOW"
    fi
    
    # 记录日志
    echo "[$timestamp] $url | $status | HTTP:$http_code | Total:${time_total}s | Connect:${time_connect}s" >> "$LOG_FILE"
    
    # 输出状态
    if [ "$status" = "OK" ]; then
        echo "✓ $url ($time_total s)"
    else
        echo "✗ $url - $status (HTTP:$http_code, ${time_total}s)"
    fi
}

# 主监控循环
echo "Starting API monitor..."
while true; do
    echo "=== $(date) ==="
    
    for url in "${MONITOR_URLS[@]}"; do
        monitor_api "$url"
    done
    
    echo "Sleeping for $CHECK_INTERVAL seconds..."
    sleep "$CHECK_INTERVAL"
done
```

这个综合项目展示了：
- 配置文件管理
- 错误处理和重试
- 性能指标收集
- 日志记录
- 健康状态判断

通过完成这三个练习，您将具备使用 curl 进行API调试、服务集成和故障排查的完整能力。