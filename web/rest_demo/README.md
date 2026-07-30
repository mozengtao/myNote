# REST 完整工作流程 Demo（Python：Flask Server + requests Client）

这是一个可以直接运行的最小 Demo，对应 [`../how_rest_api_works.md`](../how_rest_api_works.md)
第十二节的架构图（第 791-827 行）：

```text
Client -> HTTP Request -> Router -> Controller -> Service -> Repository
   -> Database -> Resource Object -> Serialize(JSON) -> HTTP Response -> Client
```

每一个方块都拆成了一个单独的 Python 文件，运行时每一层都会在终端打印一行
`[层名] ...` 的日志，方便对照架构图，直观看到一次请求是怎么"自上而下、再自下而上"
穿过整个系统的。

## 目录结构与架构图的对应关系

| 文件 | 对应架构图组件 | 说明 |
|------|----------------|------|
| `model.py` | Resource Object / Serialize | `Book` 数据类 + `to_dict()` 序列化 |
| `database.py` | Database | 进程内内存字典，模拟持久化存储（重启即清空） |
| `repository.py` | Repository | 对 Database 的数据访问封装（CRUD） |
| `service.py` | Service | 业务逻辑：借书/还书状态机、非法状态跃迁校验 |
| `controller.py` | Controller | 解析 HTTP 请求、调用 Service、把结果/异常转成 HTTP 响应 |
| `app.py` | Router + 服务器入口 | Flask 路由表：URI + Method -> Controller 函数 |
| `client.py` | Client | 依次发起 POST/GET/PATCH/PUT/DELETE，打印完整请求/响应 |

## 运行方法

```bash
cd web/rest_demo
pip install -r requirements.txt

# 终端 A：启动服务器
python app.py

# 终端 B：运行客户端，跑一遍完整生命周期
python client.py
```

## `client.py` 会依次做什么

1. `POST /books` — 创建一本书（Body 就是 Representation）
2. `GET /books` — 查询资源列表
3. `GET /books/<id>` — 查询单个资源
4. `PATCH /books/<id>` `{"status": "borrowed"}` — 借书，状态从
   `available` 转移到 `borrowed`（对应文档 3.4 节 "Transfer（状态转移）"）
5. 再次 `PATCH .../<id>` 借同一本书 — 触发 Service 层状态机校验，
   服务器返回 `409 Conflict`
6. `PUT /books/<id>` — 整体替换资源（例如还书 + 改标题）
7. `DELETE /books/<id>` — 删除资源
8. 再次 `GET /books/<id>` — 确认已删除，返回 `404 Not Found`

运行时观察服务器终端（终端 A），每一次请求都会打印类似：

```text
[Controller] handle PATCH /books/1
[Service] update_status(id=1, new_status='borrowed')
[Database] SELECT books WHERE id=1 -> FOUND
[Database] UPDATE books SET ... WHERE id=1
```

这几行日志从上到下，正好就是架构图里 `Controller -> Service -> Repository -> Database`
这条链路的真实执行顺序。

## 范围说明

- 存储只用内存字典，不引入 SQLite/MySQL 等真实数据库，目的是让 Demo 聚焦在
  "分层结构"本身，而不是持久化细节。
- 依赖只有 `flask`（服务器）和 `requests`（客户端）。
