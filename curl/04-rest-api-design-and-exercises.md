# 🎯 REST API 设计方法与实战练习

## 🧠 核心类比：REST API设计就像城市规划

```text
城市规划                          REST API设计
┌─────────────┐                  ┌─────────────┐
│  街道命名   │                  │  URI设计    │
│ (地址体系)  │                  │ (资源路径)  │
└─────────────┘                  └─────────────┘
      ↓                                ↓
┌─────────────┐                  ┌─────────────┐
│  交通规则   │                  │ HTTP方法    │
│ (行为规范)  │                  │ (操作语义)  │
└─────────────┘                  └─────────────┘
      ↓                                ↓
┌─────────────┐                  ┌─────────────┐
│  标识系统   │                  │  状态码     │
│ (信号灯)    │                  │ (响应代码)  │
└─────────────┘                  └─────────────┘
      ↓                                ↓
┌─────────────┐                  ┌─────────────┐
│  区域规划   │                  │  版本控制   │
│ (功能分区)  │                  │ (API演进)   │
└─────────────┘                  └─────────────┘
```

---

## 🏗️ 第一部分：REST API设计方法论

### 设计原则：从业务到技术的映射

```text
业务抽象                         技术实现
┌─────────────────────────────┐  ┌─────────────────────────────┐
│         业务实体             │  │          REST资源           │
│                             │  │                             │
│ - 用户 (User)               │  │ /users                      │
│ - 订单 (Order)              │->│ /orders                     │
│ - 商品 (Product)            │  │ /products                   │
│ - 购物车 (Cart)             │  │ /carts                      │
└─────────────────────────────┘  └─────────────────────────────┘
              ↓                                ↓
┌─────────────────────────────┐  ┌─────────────────────────────┐
│         业务操作             │  │         HTTP方法            │
│                             │  │                             │
│ - 查看用户信息               │  │ GET /users/{id}             │
│ - 创建新用户                 │->│ POST /users                 │
│ - 更新用户信息               │  │ PUT /users/{id}             │
│ - 删除用户                   │  │ DELETE /users/{id}          │
└─────────────────────────────┘  └─────────────────────────────┘
```

### URI设计模式：资源层级与关系

#### 模式1：独立资源
```text
基础模式：
GET    /users           # 获取用户列表
GET    /users/{id}      # 获取特定用户
POST   /users           # 创建新用户  
PUT    /users/{id}      # 更新用户
DELETE /users/{id}      # 删除用户

实际示例：
GET    /users?page=1&limit=10&role=admin
POST   /users
{
  "name": "John Doe",
  "email": "john@example.com",
  "role": "user"
}
```

#### 模式2：嵌套资源（父子关系）
```text
层级关系：
GET    /users/{id}/orders           # 获取用户的所有订单
GET    /users/{id}/orders/{order_id} # 获取用户的特定订单
POST   /users/{id}/orders           # 为用户创建新订单
PUT    /users/{id}/orders/{order_id} # 更新用户的特定订单
DELETE /users/{id}/orders/{order_id} # 删除用户的特定订单

实际应用：
GET    /users/123/orders?status=pending
POST   /users/123/orders
{
  "product_id": 456,
  "quantity": 2,
  "shipping_address": "..."
}
```

#### 模式3：关联资源（多对多关系）
```text
标签系统：
GET    /articles/{id}/tags          # 获取文章的标签
POST   /articles/{id}/tags          # 为文章添加标签
DELETE /articles/{id}/tags/{tag_id} # 移除文章的标签

权限系统：
GET    /users/{id}/roles            # 获取用户角色
POST   /users/{id}/roles            # 分配角色
DELETE /users/{id}/roles/{role_id}  # 移除角色
```

### HTTP状态码的系统化使用

```text
状态码分类                       使用场景                    示例
┌─────────────────┐           ┌─────────────────┐         ┌─────────────────┐
│   2xx 成功      │           │ 操作成功完成    │         │ 200, 201, 204   │
└─────────────────┘           └─────────────────┘         └─────────────────┘
                                      ↓
┌─────────────────┐           ┌─────────────────┐         ┌─────────────────┐
│   4xx 客户端错误 │           │ 请求有问题      │         │ 400, 401, 404   │
└─────────────────┘           └─────────────────┘         └─────────────────┘
                                      ↓
┌─────────────────┐           ┌─────────────────┐         ┌─────────────────┐
│   5xx 服务器错误 │           │ 服务端故障      │         │ 500, 502, 503   │
└─────────────────┘           └─────────────────┘         └─────────────────┘

详细状态码映射：
200 OK              - GET成功，数据返回
201 Created         - POST成功，资源已创建
204 No Content      - DELETE成功，无内容返回
400 Bad Request     - 请求参数错误
401 Unauthorized    - 认证失败
403 Forbidden       - 权限不足
404 Not Found       - 资源不存在
409 Conflict        - 资源冲突（如重复创建）
422 Unprocessable   - 数据格式错误但语法正确
500 Internal Error  - 服务器内部错误
502 Bad Gateway     - 上游服务错误
503 Service Unavailable - 服务暂不可用
```

### 错误响应设计模式

```json
// 标准错误响应格式
{
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "请求参数验证失败", 
    "details": [
      {
        "field": "email",
        "message": "邮箱格式不正确"
      },
      {
        "field": "age", 
        "message": "年龄必须大于0"
      }
    ],
    "request_id": "req_123456789",
    "timestamp": "2026-05-23T14:30:00Z"
  }
}

// 不同错误类型的响应
// 400 Bad Request
{
  "error": {
    "code": "INVALID_REQUEST",
    "message": "请求格式错误"
  }
}

// 401 Unauthorized  
{
  "error": {
    "code": "AUTHENTICATION_REQUIRED",
    "message": "需要身份认证"
  }
}

// 404 Not Found
{
  "error": {
    "code": "RESOURCE_NOT_FOUND", 
    "message": "用户不存在",
    "resource": "User",
    "resource_id": "123"
  }
}

// 500 Internal Server Error
{
  "error": {
    "code": "INTERNAL_ERROR",
    "message": "服务器内部错误",
    "request_id": "req_123456789"
  }
}
```

---

## 🎪 第二部分：实战练习

### 练习1：调用公开REST API

#### 目标
- 理解HTTP方法的实际使用
- 掌握查询参数和认证方式
- 学会分析API响应

#### 练习任务：GitHub API

```bash
# 1. 获取GitHub用户信息（无认证）
curl -H "Accept: application/json" \
     https://api.github.com/users/octocat

# 预期响应：
{
  "login": "octocat",
  "id": 1,
  "name": "The Octocat",
  "company": "GitHub",
  "blog": "https://github.com/blog",
  ...
}

# 2. 获取用户的仓库列表
curl -H "Accept: application/json" \
     "https://api.github.com/users/octocat/repos?type=public&sort=updated"

# 3. 搜索仓库（带查询参数）
curl -H "Accept: application/json" \
     "https://api.github.com/search/repositories?q=language:python&sort=stars"

# 4. 使用认证获取私有信息（需要个人访问令牌）
curl -H "Accept: application/json" \
     -H "Authorization: token YOUR_GITHUB_TOKEN" \
     https://api.github.com/user

# 任务要求：
# - 分析每个响应的结构
# - 理解分页机制（Link header）
# - 观察错误响应（如无效认证）
```

### 练习2：设计并实现简单的REST API

#### 场景：个人博客系统

##### 需求分析
```text
业务实体：
- 文章 (Article)
- 评论 (Comment)  
- 标签 (Tag)
- 用户 (User)

业务关系：
- 用户可以写多篇文章（一对多）
- 文章可以有多条评论（一对多）
- 文章可以有多个标签（多对多）
```

##### API设计
```text
资源设计：
┌─────────────────────────────────┐
│            Articles             │
│  GET    /articles               │ # 文章列表
│  GET    /articles/{id}          │ # 特定文章
│  POST   /articles               │ # 创建文章
│  PUT    /articles/{id}          │ # 更新文章
│  DELETE /articles/{id}          │ # 删除文章
└─────────────────────────────────┘
              ↓
┌─────────────────────────────────┐
│        Article Comments         │
│  GET    /articles/{id}/comments │ # 文章评论
│  POST   /articles/{id}/comments │ # 添加评论
│  DELETE /comments/{comment_id}  │ # 删除评论
└─────────────────────────────────┘
              ↓
┌─────────────────────────────────┐
│         Article Tags            │
│  GET    /articles/{id}/tags     │ # 文章标签
│  POST   /articles/{id}/tags     │ # 添加标签
│  DELETE /articles/{id}/tags/{tag}│ # 移除标签
└─────────────────────────────────┘
```

##### 实现示例（Python Flask）
```python
# app.py
from flask import Flask, request, jsonify
import json
from datetime import datetime

app = Flask(__name__)

# 模拟数据库
articles = []
comments = []
next_id = 1

@app.route('/articles', methods=['GET'])
def get_articles():
    # 支持分页和过滤
    page = int(request.args.get('page', 1))
    limit = int(request.args.get('limit', 10))
    tag = request.args.get('tag')
    
    filtered_articles = articles
    if tag:
        filtered_articles = [a for a in articles if tag in a.get('tags', [])]
    
    start = (page - 1) * limit
    end = start + limit
    
    return jsonify({
        'data': filtered_articles[start:end],
        'pagination': {
            'page': page,
            'limit': limit,
            'total': len(filtered_articles)
        }
    })

@app.route('/articles', methods=['POST'])
def create_article():
    global next_id
    
    data = request.get_json()
    
    # 数据验证
    if not data or not data.get('title') or not data.get('content'):
        return jsonify({
            'error': {
                'code': 'VALIDATION_ERROR',
                'message': '标题和内容不能为空'
            }
        }), 400
    
    article = {
        'id': next_id,
        'title': data['title'],
        'content': data['content'],
        'tags': data.get('tags', []),
        'created_at': datetime.utcnow().isoformat(),
        'updated_at': datetime.utcnow().isoformat()
    }
    
    articles.append(article)
    next_id += 1
    
    return jsonify(article), 201

@app.route('/articles/<int:article_id>', methods=['GET'])
def get_article(article_id):
    article = next((a for a in articles if a['id'] == article_id), None)
    
    if not article:
        return jsonify({
            'error': {
                'code': 'RESOURCE_NOT_FOUND',
                'message': '文章不存在'
            }
        }), 404
    
    return jsonify(article)

@app.route('/articles/<int:article_id>', methods=['PUT'])
def update_article(article_id):
    article = next((a for a in articles if a['id'] == article_id), None)
    
    if not article:
        return jsonify({
            'error': {
                'code': 'RESOURCE_NOT_FOUND', 
                'message': '文章不存在'
            }
        }), 404
    
    data = request.get_json()
    
    # 更新字段
    if 'title' in data:
        article['title'] = data['title']
    if 'content' in data:
        article['content'] = data['content']  
    if 'tags' in data:
        article['tags'] = data['tags']
    
    article['updated_at'] = datetime.utcnow().isoformat()
    
    return jsonify(article)

@app.route('/articles/<int:article_id>', methods=['DELETE'])
def delete_article(article_id):
    global articles
    
    article = next((a for a in articles if a['id'] == article_id), None)
    
    if not article:
        return jsonify({
            'error': {
                'code': 'RESOURCE_NOT_FOUND',
                'message': '文章不存在'
            }
        }), 404
    
    articles = [a for a in articles if a['id'] != article_id]
    
    return '', 204

if __name__ == '__main__':
    app.run(debug=True, port=8080)
```

##### 测试脚本
```bash
#!/bin/bash
# test_api.sh

API_BASE="http://localhost:8080"

echo "=== 测试博客API ==="

echo "1. 创建文章"
curl -X POST \
     -H "Content-Type: application/json" \
     -d '{
       "title": "REST API最佳实践",
       "content": "本文介绍REST API的设计原则...",
       "tags": ["REST", "API", "设计"]
     }' \
     "$API_BASE/articles" | jq '.'

echo -e "\n2. 获取文章列表"
curl -s "$API_BASE/articles?page=1&limit=10" | jq '.'

echo -e "\n3. 获取特定文章"  
curl -s "$API_BASE/articles/1" | jq '.'

echo -e "\n4. 更新文章"
curl -X PUT \
     -H "Content-Type: application/json" \
     -d '{
       "title": "REST API最佳实践 (更新版)",
       "tags": ["REST", "API", "设计", "最佳实践"]
     }' \
     "$API_BASE/articles/1" | jq '.'

echo -e "\n5. 删除文章"
curl -X DELETE "$API_BASE/articles/1" -v

echo -e "\n6. 测试错误情况 - 获取不存在的文章"
curl -s "$API_BASE/articles/999" | jq '.'
```

### 练习3：curl高级调试技巧

#### 场景：API调用失败排查

##### 模拟问题环境
```bash
# 启动一个有问题的测试服务器（Python）
# problem_server.py
from flask import Flask, request
import time
import random

app = Flask(__name__)

@app.route('/slow')
def slow_endpoint():
    # 随机延迟，模拟性能问题
    time.sleep(random.uniform(2, 10))
    return {'message': 'slow response'}

@app.route('/error')  
def error_endpoint():
    # 随机返回错误
    if random.random() < 0.5:
        return {'error': 'random error'}, 500
    else:
        return {'message': 'success'}

@app.route('/auth')
def auth_endpoint():
    # 需要认证
    auth = request.headers.get('Authorization')
    if not auth or not auth.startswith('Bearer '):
        return {'error': 'authentication required'}, 401
    return {'message': 'authenticated'}

if __name__ == '__main__':
    app.run(port=8081)
```

##### 调试练习任务
```bash
# 任务1：性能问题调试
echo "=== 性能调试 ==="

# 测量响应时间
curl -w "DNS解析: %{time_namelookup}s\n连接建立: %{time_connect}s\n传输开始: %{time_starttransfer}s\n总时间: %{time_total}s\n" \
     -s -o /dev/null \
     http://localhost:8081/slow

# 并发测试
for i in {1..5}; do
    curl -w "请求$i: %{time_total}s\n" \
         -s -o /dev/null \
         http://localhost:8081/slow &
done
wait

# 任务2：错误处理调试
echo -e "\n=== 错误调试 ==="

# 捕获错误响应
for i in {1..10}; do
    response=$(curl -s -w "HTTPSTATUS:%{http_code}" http://localhost:8081/error)
    status=$(echo "$response" | sed -n 's/.*HTTPSTATUS:\([0-9]*\)/\1/p')
    body=$(echo "$response" | sed 's/HTTPSTATUS:.*//g')
    
    echo "请求$i: 状态码=$status, 响应=$body"
done

# 任务3：认证问题调试  
echo -e "\n=== 认证调试 ==="

# 无认证请求
echo "无认证:"
curl -v http://localhost:8081/auth 2>&1 | grep -E "(< HTTP|< \w+:|^\{)"

# 错误认证格式
echo -e "\n错误认证格式:"
curl -H "Authorization: Basic invalid" \
     -v http://localhost:8081/auth 2>&1 | grep -E "(< HTTP|< \w+:|^\{)"

# 正确认证
echo -e "\n正确认证:"
curl -H "Authorization: Bearer valid-token" \
     -v http://localhost:8081/auth 2>&1 | grep -E "(< HTTP|< \w+:|^\{)"
```

---

## 🛠️ 第三部分：REST API设计模板

### 模板1：标准CRUD API

```yaml
# api_template.yaml
openapi: 3.0.0
info:
  title: 标准REST API模板
  version: 1.0.0
  description: 基于最佳实践的REST API设计模板

paths:
  /resources:
    get:
      summary: 获取资源列表
      parameters:
        - name: page
          in: query
          schema:
            type: integer
            default: 1
        - name: limit
          in: query  
          schema:
            type: integer
            default: 20
        - name: sort
          in: query
          schema:
            type: string
            default: "created_at"
      responses:
        '200':
          description: 成功
          content:
            application/json:
              schema:
                type: object
                properties:
                  data:
                    type: array
                    items:
                      $ref: '#/components/schemas/Resource'
                  pagination:
                    $ref: '#/components/schemas/Pagination'
    
    post:
      summary: 创建资源
      requestBody:
        required: true
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/CreateResource'
      responses:
        '201':
          description: 创建成功
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/Resource'
        '400':
          description: 请求错误
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/Error'

  /resources/{id}:
    get:
      summary: 获取特定资源
      parameters:
        - name: id
          in: path
          required: true
          schema:
            type: integer
      responses:
        '200':
          description: 成功
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/Resource'
        '404':
          description: 资源不存在
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/Error'
    
    put:
      summary: 更新资源
      parameters:
        - name: id
          in: path
          required: true
          schema:
            type: integer
      requestBody:
        required: true
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/UpdateResource'
      responses:
        '200':
          description: 更新成功
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/Resource'
    
    delete:
      summary: 删除资源
      parameters:
        - name: id
          in: path
          required: true
          schema:
            type: integer
      responses:
        '204':
          description: 删除成功
        '404':
          description: 资源不存在

components:
  schemas:
    Resource:
      type: object
      properties:
        id:
          type: integer
        name:
          type: string
        created_at:
          type: string
          format: date-time
        updated_at:
          type: string
          format: date-time
    
    CreateResource:
      type: object
      required:
        - name
      properties:
        name:
          type: string
    
    UpdateResource:
      type: object
      properties:
        name:
          type: string
    
    Pagination:
      type: object
      properties:
        page:
          type: integer
        limit:
          type: integer
        total:
          type: integer
        pages:
          type: integer
    
    Error:
      type: object
      properties:
        error:
          type: object
          properties:
            code:
              type: string
            message:
              type: string
            details:
              type: array
              items:
                type: object
```

### 模板2：curl命令生成器

```bash
#!/bin/bash
# curl_generator.sh - REST API curl命令生成器

generate_curl() {
    local method=$1
    local endpoint=$2
    local data=$3
    local auth_type=$4
    local auth_value=$5
    
    # 基础命令
    cmd="curl"
    
    # HTTP方法
    if [ "$method" != "GET" ]; then
        cmd="$cmd -X $method"
    fi
    
    # 认证
    case "$auth_type" in
        "bearer")
            cmd="$cmd -H \"Authorization: Bearer $auth_value\""
            ;;
        "basic")
            cmd="$cmd -u $auth_value"
            ;;
        "apikey")
            cmd="$cmd -H \"X-API-Key: $auth_value\""
            ;;
    esac
    
    # 内容类型
    if [ -n "$data" ]; then
        cmd="$cmd -H \"Content-Type: application/json\""
        cmd="$cmd -d '$data'"
    fi
    
    # 通用头
    cmd="$cmd -H \"Accept: application/json\""
    
    # 端点
    cmd="$cmd \"$endpoint\""
    
    echo "$cmd"
}

# 使用示例
echo "=== CRUD操作curl命令 ==="

echo "1. GET请求："
generate_curl "GET" "https://api.example.com/users?page=1&limit=10" "" "bearer" "your-token"

echo -e "\n2. POST请求："
generate_curl "POST" "https://api.example.com/users" '{"name":"John","email":"john@example.com"}' "bearer" "your-token"

echo -e "\n3. PUT请求："
generate_curl "PUT" "https://api.example.com/users/123" '{"name":"John Updated"}' "bearer" "your-token"

echo -e "\n4. DELETE请求："
generate_curl "DELETE" "https://api.example.com/users/123" "" "bearer" "your-token"
```

### 模板3：API测试套件

```bash
#!/bin/bash
# api_test_suite.sh - REST API自动化测试套件

API_BASE="${1:-http://localhost:8080}"
AUTH_TOKEN="${2:-}"

# 测试结果统计
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# 测试辅助函数
run_test() {
    local test_name="$1"
    local expected_status="$2"
    local curl_command="$3"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo "运行测试: $test_name"
    
    # 执行请求并获取状态码
    response=$(eval "$curl_command -s -w 'HTTPSTATUS:%{http_code}'")
    status=$(echo "$response" | sed -n 's/.*HTTPSTATUS:\([0-9]*\)/\1/p')
    body=$(echo "$response" | sed 's/HTTPSTATUS:.*//g')
    
    # 验证结果
    if [ "$status" = "$expected_status" ]; then
        echo "✓ 测试通过 (状态码: $status)"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo "✗ 测试失败 (期望: $expected_status, 实际: $status)"
        echo "响应内容: $body"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    
    echo "---"
}

# 构建认证头
auth_header=""
if [ -n "$AUTH_TOKEN" ]; then
    auth_header="-H \"Authorization: Bearer $AUTH_TOKEN\""
fi

echo "=== REST API测试套件 ==="
echo "测试目标: $API_BASE"
echo "认证令牌: ${AUTH_TOKEN:0:20}${AUTH_TOKEN:20:+...}"
echo

# 测试用例
echo "=== CRUD操作测试 ==="

# 创建资源
run_test "创建用户" "201" \
    "curl -X POST $auth_header -H \"Content-Type: application/json\" \
     -d '{\"name\":\"Test User\",\"email\":\"test@example.com\"}' \
     $API_BASE/users"

# 获取资源列表
run_test "获取用户列表" "200" \
    "curl $auth_header $API_BASE/users"

# 获取特定资源
run_test "获取特定用户" "200" \
    "curl $auth_header $API_BASE/users/1"

# 更新资源
run_test "更新用户" "200" \
    "curl -X PUT $auth_header -H \"Content-Type: application/json\" \
     -d '{\"name\":\"Updated User\"}' \
     $API_BASE/users/1"

# 删除资源
run_test "删除用户" "204" \
    "curl -X DELETE $auth_header $API_BASE/users/1"

echo "=== 错误情况测试 ==="

# 404错误
run_test "获取不存在的用户" "404" \
    "curl $auth_header $API_BASE/users/999"

# 400错误
run_test "创建无效用户" "400" \
    "curl -X POST $auth_header -H \"Content-Type: application/json\" \
     -d '{\"invalid\":\"data\"}' \
     $API_BASE/users"

# 测试报告
echo
echo "=== 测试报告 ==="
echo "总测试数: $TOTAL_TESTS"
echo "通过: $PASSED_TESTS"
echo "失败: $FAILED_TESTS"
echo "成功率: $(( PASSED_TESTS * 100 / TOTAL_TESTS ))%"

if [ $FAILED_TESTS -eq 0 ]; then
    echo "🎉 所有测试通过！"
    exit 0
else
    echo "❌ 存在测试失败"
    exit 1
fi
```

---

## 🎯 第四部分：统一心智模型总结

### REST API的本质理解

```text
REST API = Resource Modeling + HTTP Semantics + Stateless Communication

┌─────────────────────────────────────────────────────────────┐
│                    REST API 心智模型                        │
│                                                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │  Resource Model │  │  HTTP Protocol  │  │ Stateless Comm │  │
│  │                 │  │                 │  │                 │  │
│  │ - URI Design    │  │ - Methods       │  │ - Self-contained│  │
│  │ - Relationships │  │ - Status Codes  │  │ - Scalability   │  │
│  │ - Hierarchies   │  │ - Headers       │  │ - Reliability   │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
│           ↓                     ↓                     ↓        │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │ Business Domain │  │ Technical Stack │  │ System Design  │  │
│  │                 │  │                 │  │                 │  │
│  │ - Entity Model  │  │ - TCP/IP        │  │ - Load Balancer │  │
│  │ - Operations    │  │ - DNS/Routing   │  │ - Caching       │  │
│  │ - Constraints   │  │ - TLS/Security  │  │ - Monitoring    │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 三个核心原则

1. **名词化原则**（Resource-Based）
   - URI使用名词，不使用动词
   - 通过HTTP方法表达操作意图
   - 资源层级体现业务关系

2. **无状态原则**（Stateless）
   - 每个请求包含完整信息
   - 服务端不保存客户端状态
   - 支持水平扩展和容错

3. **统一接口原则**（Uniform Interface）
   - 标准HTTP方法语义
   - 一致的状态码使用
   - 可预测的URL结构

### 五个常用curl模板

```bash
# 模板1：GET - 查询资源
curl -H "Accept: application/json" \
     -H "Authorization: Bearer $TOKEN" \
     "$API_BASE/resources?page=1&limit=10"

# 模板2：POST - 创建资源
curl -X POST \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer $TOKEN" \
     -d '{"key": "value"}' \
     "$API_BASE/resources"

# 模板3：PUT - 更新资源
curl -X PUT \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer $TOKEN" \
     -d '{"key": "updated_value"}' \
     "$API_BASE/resources/123"

# 模板4：DELETE - 删除资源
curl -X DELETE \
     -H "Authorization: Bearer $TOKEN" \
     "$API_BASE/resources/123"

# 模板5：调试模式
curl -v \
     -H "Authorization: Bearer $TOKEN" \
     "$API_BASE/resources" 2>&1 | tee debug.log
```

---

## 🏆 最终能力验证清单

### ✔ 基础理解能力
- [ ] 能区分REST vs RPC的本质差异
- [ ] 理解HTTP协议与网络栈的关系
- [ ] 掌握资源建模的基本方法

### ✔ 设计能力 
- [ ] 能设计符合REST原则的URI结构
- [ ] 能合理使用HTTP状态码
- [ ] 能设计一致的错误响应格式

### ✔ 调试能力
- [ ] 能用curl精准发送各类HTTP请求
- [ ] 能分析网络层面的API问题
- [ ] 能编写自动化测试脚本

### ✔ 系统能力
- [ ] 理解REST API在分布式系统中的作用
- [ ] 能优化API性能和可扩展性
- [ ] 能设计API监控和故障排查方案

### ✔ 工程实践
- [ ] 能实现完整的REST API服务
- [ ] 能编写API文档和测试用例
- [ ] 能集成认证、缓存、限流等机制

---

## 🚀 进阶学习路径

```text
当前水平评估 → 下一步学习方向

初级 (能调用API)
  ↓
中级 (能设计API)
  ↓  
高级 (能优化API) → GraphQL, gRPC对比学习
  ↓              → API网关和微服务架构  
专家 (能架构API系统) → 分布式API治理
  ↓              → API安全和合规
大师 (能定义标准) → 行业最佳实践贡献
```

**恭喜！** 🎉 你现在已经具备了从底层网络到架构设计的完整REST API知识体系。可以开始在真实项目中应用这些知识，并在实践中不断深化理解。