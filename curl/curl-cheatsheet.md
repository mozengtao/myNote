# curl 快速参考卡片

---

## 🚀 最常用命令

### 基础请求
```bash
# GET 请求
curl http://api.example.com/users

# POST JSON 数据
curl -X POST http://api.example.com/users \
  -H "Content-Type: application/json" \
  -d '{"name": "John", "email": "john@example.com"}'

# PUT 更新
curl -X PUT http://api.example.com/users/123 \
  -H "Content-Type: application/json" \
  -d '{"name": "John Updated"}'

# DELETE 删除
curl -X DELETE http://api.example.com/users/123
```

### 认证方式
```bash
# Bearer Token
curl -H "Authorization: Bearer $TOKEN" http://api.example.com/protected

# Basic Auth
curl -u username:password http://api.example.com/protected

# API Key
curl -H "X-API-Key: $API_KEY" http://api.example.com/data
```

### 调试选项
```bash
# 详细输出
curl -v http://api.example.com/endpoint

# 只显示响应头
curl -I http://api.example.com/endpoint

# 包含响应头
curl -i http://api.example.com/endpoint

# 静默模式
curl -s http://api.example.com/endpoint
```

---

## ⚙️ 常用参数速查

| 参数 | 作用 | 示例 |
|------|------|------|
| `-X` | HTTP 方法 | `-X POST` |
| `-H` | 添加 Header | `-H "Content-Type: json"` |
| `-d` | 请求体数据 | `-d '{"key":"value"}'` |
| `-u` | 认证信息 | `-u user:pass` |
| `-v` | 详细模式 | `-v` |
| `-i` | 包含响应头 | `-i` |
| `-s` | 静默模式 | `-s` |
| `-o` | 输出到文件 | `-o result.json` |
| `-w` | 自定义输出格式 | `-w "%{http_code}"` |
| `-f` | 失败时返回错误码 | `-f` |

---

## 🔧 高级用法

### 文件操作
```bash
# 从文件读取数据
curl -X POST -d @data.json -H "Content-Type: application/json" URL

# 文件上传
curl -F "file=@document.pdf" -F "description=Test" URL

# 下载文件
curl -o filename.zip http://example.com/file.zip

# 断点续传
curl -C - -o large-file.zip http://example.com/large-file.zip
```

### 性能分析
```bash
# 时间统计
curl -w "
DNS lookup:     %{time_namelookup}s
Connect:        %{time_connect}s
Transfer start: %{time_starttransfer}s
Total:          %{time_total}s
" -o /dev/null -s URL

# 状态码检查
curl -w "%{http_code}" -o /dev/null -s URL
```

### 重试和超时
```bash
# 设置超时和重试
curl --connect-timeout 10 --max-time 30 --retry 3 --retry-delay 5 URL
```

---

## 🚨 故障排查

### 常见问题快速诊断

```bash
# DNS 问题
curl -v URL 2>&1 | grep "getaddrinfo"

# 连接问题  
curl -v --connect-timeout 5 URL

# SSL 问题
curl -v --insecure URL

# 响应慢
curl -w "%{time_starttransfer}" URL

# 查看完整请求
curl --trace-ascii trace.log URL
```

### 错误码含义

| Exit Code | 含义 | 解决方案 |
|-----------|------|----------|
| 0 | 成功 | - |
| 6 | DNS 解析失败 | 检查域名/DNS |
| 7 | 连接失败 | 检查网络/防火墙 |
| 28 | 操作超时 | 增加 `--max-time` |
| 35 | SSL 握手失败 | 检查证书/TLS版本 |
| 52 | 服务器无响应 | 检查服务状态 |

---

## 📝 实用脚本模板

### API 健康检查
```bash
#!/bin/bash
check_api() {
    local url=$1
    local http_code=$(curl -s -w "%{http_code}" -o /dev/null "$url")
    if [ "$http_code" = "200" ]; then
        echo "✓ $url is healthy"
        return 0
    else
        echo "✗ $url failed (HTTP $http_code)"
        return 1
    fi
}

check_api "http://api.example.com/health"
```

### 批量请求
```bash
#!/bin/bash
urls=(
    "http://api1.example.com/status"
    "http://api2.example.com/health"
    "http://api3.example.com/ping"
)

for url in "${urls[@]}"; do
    echo "Testing $url..."
    curl -f -s "$url" > /dev/null && echo "✓ OK" || echo "✗ Failed"
done
```

### 性能监控
```bash
#!/bin/bash
monitor_performance() {
    local url=$1
    while true; do
        local time_total=$(curl -w "%{time_total}" -o /dev/null -s "$url")
        local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
        echo "[$timestamp] Response time: ${time_total}s"
        sleep 60
    done
}

monitor_performance "http://api.example.com/endpoint"
```

---

## 💡 最佳实践

### DO ✅
- 总是设置 `Content-Type` 当发送 JSON
- 使用 `-f` 让 HTTP 错误返回非零退出码
- 在脚本中使用 `--retry` 提高可靠性
- 用 `-s` 在脚本中避免进度输出
- 设置合理的超时时间

### DON'T ❌
- 不要在 URL 中硬编码敏感信息
- 不要忽略 HTTP 状态码检查
- 不要在生产环境使用 `--insecure`
- 不要忘记处理网络错误情况
- 不要使用过短的超时时间

---

## 🔗 配合其他工具

```bash
# 与 jq 处理 JSON
curl -s http://api.example.com/users | jq '.[] | .name'

# 与 grep 过滤内容
curl -s http://api.example.com/status | grep -o "status.*"

# 管道链式处理
curl -s http://api.example.com/data | jq '.results[]' | head -5

# 保存 cookies
curl -c cookies.txt -b cookies.txt http://example.com/login

# 跟随重定向
curl -L http://example.com/redirect-url
```

---

打印此页作为桌面参考，让 curl 成为您的网络调试利器！