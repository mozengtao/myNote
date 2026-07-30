# REST API 完整理解：工作原理、核心组件与心智模式

> **一句话理解：**
>
> REST（Representational State Transfer）是一种 **面向资源（Resource-Oriented）** 的 Web API 架构风格。
>
> 它将 **一切可管理的对象都视为 Resource（资源）**，通过 **HTTP Method** 对资源进行操作，通过 **JSON/XML 等 Representation（表示）** 在网络上传输资源。

---

# 一、REST 为什么会出现？

假设有一个图书管理系统。

传统 RPC 风格通常会设计成：

```text
getBook()
addBook()
deleteBook()
updateBook()
searchBook()
borrowBook()
...
```

随着业务增长：

```text
getBookByAuthor()
getBookByISBN()
getBookByCategory()
...
```

API 数量越来越多。

REST 提出了另一种思想：

> **不要围绕"动作（Action）"设计 API，而是围绕"资源（Resource）"设计 API。**

于是：

```text
/books
/users
/orders
/comments
```

所有资源保持不变。

真正发生变化的是：

```text
GET
POST
PUT
PATCH
DELETE
```

HTTP Method。

例如：

```http
GET    /books
POST   /books
PUT    /books/100
DELETE /books/100
```

整个 API 风格立即变得统一。

---

# 二、REST 名称到底是什么意思？

REST：

```
Representational State Transfer
```

通常翻译：

```
表述性状态转移
```

真正理解建议拆成四个词：

```
Resource
↓

Representation
↓

State
↓

Transfer
```

---

# 三、REST 的四个核心概念

---

## 1. Resource（资源）

REST 世界只有资源。

例如：

```
Book
User
Order
Container
VM
Network Interface
Route
```

都是资源。

例如：

```
Book #100
```

URI：

```
/books/100
```

REST 不关心对象是什么。

只关心：

> 是否可以唯一标识它。

因此：

```
URI = Resource Identifier
```

例如：

```
https://example.com/books/100
```

表示：

一本编号100的书。

注意：

它不是函数。

不是 RPC。

而是一个资源。

---

## 2. Representation（资源表示）

资源不能直接通过网络发送。

必须转换成一种可传输格式。

例如：

Book：

```
Book Object
```

可以表示为：

JSON：

```json
{
    "id":100,
    "title":"REST"
}
```

XML：

```xml
<Book>
...
</Book>
```

YAML：

```yaml
id:100
title:REST
```

因此：

```
Book
↓

JSON
```

JSON 就是：

```
Representation
```

REST 真正交换的是：

```
Representation

而不是

Resource Object
```

---

## 3. State（状态）

资源具有状态。

例如：

```
Book

Available

Borrowed

Reserved
```

Book 当前状态：

```json
{
    "status":"borrowed"
}
```

客户端：

```
PUT /books/100
```

发送：

```json
{
    "status":"available"
}
```

服务器：

修改资源状态。

因此：

```
Resource State Changed
```

---

## 4. Transfer（状态转移）

客户端：

```
PUT /books/100
```

发送：

```json
{
    "status":"borrowed"
}
```

服务器：

修改数据库。

返回：

```
200 OK
```

整个过程：

```
Representation

↓

Transfer

↓

State Changed
```

因此：

REST：

就是：

```
Representational State Transfer
```

---

# 四、REST API 的组成

一个 REST API 可以拆成下面几个组件：

```
Client

↓

HTTP Request

↓

URI

↓

HTTP Method

↓

REST Server

↓

Resource

↓

Representation(JSON)

↓

HTTP Response
```

例如：

```
Browser

↓

GET /books/100

↓

Book Resource

↓

JSON

↓

200 OK
```

---

# 五、REST 请求完整流程

例如：

```
GET /books/100
```

整个过程：

```
Client

↓

HTTP Request

↓

Router

↓

Controller

↓

Service

↓

Repository

↓

Database

↓

Book Object

↓

JSON Serialize

↓

HTTP Response

↓

Client
```

例如：

数据库得到：

```
Book
```

转换：

```json
{
    "id":100,
    "title":"REST"
}
```

返回：

```
HTTP/1.1 200 OK
```

---

# 六、HTTP 在 REST 中承担什么角色？

REST 并没有重新发明通信协议。

它直接利用 HTTP。

HTTP 已经提供：

```
GET

POST

PUT

PATCH

DELETE
```

REST 只是赋予它们统一语义。

| HTTP Method | REST 语义 |
|-------------|-----------|
| GET | 查询资源 |
| POST | 创建资源 |
| PUT | 整体替换资源 |
| PATCH | 局部更新资源 |
| DELETE | 删除资源 |

因此：

```
REST

=

HTTP

+

Resource
```

---

# 七、HTTP Request 的组成

例如：

```http
GET /books/100 HTTP/1.1

Host: example.com

Authorization: Bearer xxxxxx

Accept: application/json
```

组成：

```
Method

↓

URI

↓

Headers

↓

Body
```

POST 示例：

```http
POST /books

{
    "title":"REST"
}
```

Body：

就是：

```
Representation
```

---

# 八、HTTP Response 的组成

服务器返回：

```http
HTTP/1.1 200 OK

Content-Type: application/json

{
    "id":100
}
```

组成：

```
Status Code

↓

Headers

↓

Body
```

Body：

仍然是：

```
Representation
```

---

# 九、REST Server 内部组件

一个典型 REST 服务通常包含：

```
Client

↓

HTTP

↓

Router

↓

Controller

↓

Service

↓

Repository

↓

Database
```

例如：

```
GET /books/100

↓

Router

↓

BookController

↓

BookService

↓

BookRepository

↓

MySQL
```

职责：

| 组件 | 职责 |
|------|------|
| Router | URI 路由 |
| Controller | HTTP 请求处理 |
| Service | 业务逻辑 |
| Repository / DAO | 数据访问 |
| Database | 持久化存储 |

---

# 十、REST 六项架构约束

REST 架构定义了六项约束（前五项必须，第六项可选）。

| 约束 | 含义 |
|------|------|
| Client-Server | 客户端与服务器职责分离 |
| Stateless | 每个请求都是独立的，不依赖之前请求的上下文 |
| Cacheable | 响应可缓存，提高性能 |
| Uniform Interface | 统一接口（资源、URI、HTTP Method、表示形式一致） |
| Layered System | 支持代理、网关、负载均衡等多层架构 |
| Code on Demand（可选） | 服务器可下发可执行代码（例如 JavaScript） |

其中：

**Stateless（无状态）** 最容易误解。

它不是说服务器没有状态。

服务器当然保存：

```
Book
User
Order
Database
```

真正含义是：

> **服务器不保存客户端会话状态，每个请求都必须携带处理该请求所需的全部信息。**

例如：

```
GET /books/100
Authorization: Bearer xxxx
```

服务器无需依赖：

```
上一条请求是什么？
```

即可处理当前请求。

---

# 十一、REST 与 RPC 的区别

RPC：

```text
createUser()

deleteUser()

updateUser()

borrowBook()
```

REST：

```text
POST /users

DELETE /users/100

PUT /users/100

POST /books/100/borrow（某些业务也可能建模为借阅资源）
```

比较：

| REST | RPC |
|------|-----|
| 面向资源 | 面向操作 |
| URI 表示资源 | 方法名表示操作 |
| HTTP Method 表示动作 | 方法名本身表示动作 |
| 强调统一接口 | 强调远程函数调用 |

一句话：

```
REST

关注：

资源(Resource)

RPC

关注：

函数(Function)
```

---

# 十二、REST 完整工作流程（架构图）

```text
                     Client
                        │
                        │ HTTP Request
                        ▼
      +--------------------------------------+
      | Method + URI + Headers + Body        |
      +--------------------------------------+
                        │
                        ▼
                     Router
                        │
                        ▼
                  Controller
                        │
                        ▼
                     Service
                        │
                        ▼
                  Repository
                        │
                        ▼
                    Database
                        │
                Resource Object
                        │
             Serialize(JSON/XML)
                        │
                        ▼
                 HTTP Response
         Status + Headers + Body
                        │
                        ▼
                     Client
```

---

# 十三、REST 最重要的心智模式

REST 可以类比为：

## 图书馆模型

| 现实世界 | REST 世界 |
|----------|-----------|
| 一本书 | Resource |
| 图书编号 | URI |
| 借书、还书、查看 | HTTP Method |
| 图书信息卡 | Representation（JSON/XML） |
| 图书当前状态 | Resource State |
| 图书馆管理员 | REST Server |
| 借书的人 | Client |

---

## Everything is a Resource

这是 REST 最重要的一句话：

```
Everything is a Resource
```

资源拥有：

```
URI
```

资源通过：

```
Representation
```

进行网络传输。

资源通过：

```
HTTP Method
```

被操作。

服务器修改：

```
Resource State
```

最后：

返回新的：

```
Representation
```

整个流程：

```text
Everything is a Resource
           │
           ▼
Every Resource has a URI
           │
           ▼
HTTP Method describes the operation
           │
           ▼
Resource is transferred as a Representation
           │
           ▼
Server changes Resource State
           │
           ▼
Return a new Representation
```

---

# 十四、REST 的统一心智模型

可以将 REST 抽象成下面这一套完整模型：

```text
                    Resource
                        │
        ┌───────────────┼────────────────┐
        │               │                │
        ▼               ▼                ▼
       URI         Representation      State
                        │                │
                        └──────┬─────────┘
                               ▼
                         HTTP Request
                     (GET / POST / PUT / PATCH / DELETE)
                               │
                               ▼
                           REST Server
                               │
                               ▼
                      Business Logic
                               │
                               ▼
                           Database
                               │
                               ▼
                   Updated Representation
                               │
                               ▼
                        HTTP Response
```

---

# 十五、总结

REST 的设计思想可以浓缩为以下五条：

1. **Everything is a Resource（万物皆资源）**
   - 系统中的对象统一建模为资源。

2. **Every Resource has a URI（资源必须有唯一标识）**
   - URI 用于唯一定位资源。

3. **HTTP Method 表达动作**
   - GET 查询、POST 创建、PUT 替换、PATCH 更新、DELETE 删除。

4. **交换的是 Representation，而不是对象本身**
   - 客户端和服务器之间传输的是 JSON、XML、YAML 等资源表示。

5. **服务器根据请求修改 Resource State，并返回新的 Representation**
   - REST 本质上是资源状态在客户端和服务器之间的表示传递与状态变更。

---

# 一句话总结

> **REST = Resource + URI + HTTP Method + Representation + Stateless + Uniform Interface。**

它不是一种通信协议，而是一种**面向资源的 Web API 架构风格**，核心思想是：

> **Everything is a Resource，HTTP 负责操作资源，Representation 负责传输资源，服务器负责维护资源状态。**