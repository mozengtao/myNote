"""
Client
======
对应架构图的两端：

    Client -> HTTP Request -> ... -> HTTP Response -> Client

这个脚本扮演"Client"角色：拼出 HTTP Request（Method + URI + Headers + Body），
发给服务器，再打印回来的 HTTP Response（Status + Headers + Body）。
按顺序跑一遍 Book 资源的完整生命周期：create -> list -> get -> borrow ->
borrow again(冲突) -> replace -> delete，覆盖文档第六节 HTTP Method 语义表格
里的 GET / POST / PUT / PATCH / DELETE。

运行方式（先在另一个终端启动 server: `python app.py`）：
    python client.py
"""

import requests

BASE_URL = "http://127.0.0.1:5000"


def call(method: str, path: str, json: dict | None = None):
    """统一发请求 + 打印 Request/Response，方便对照架构图观察每一步。"""
    url = f"{BASE_URL}{path}"
    print(f"\n--> {method} {path}" + (f"  body={json}" if json else ""))
    # requests 在这里构造 "HTTP Request: Method + URI + Headers + Body"
    resp = requests.request(method, url, json=json)
    print(f"<-- {resp.status_code} {resp.reason}")
    # resp.json()/.text 就是在读 "HTTP Response: Status + Headers + Body" 里的 Body
    try:
        print(f"    body={resp.json()}")
    except ValueError:
        print(f"    body=<empty>")
    return resp


def main():
    # 1) POST /books：创建资源，Body 就是 Representation。
    created = call("POST", "/books", {"title": "REST", "author": "Roy Fielding"}).json()
    book_id = created["id"]

    # 2) GET /books：查询资源集合。
    call("GET", "/books")

    # 3) GET /books/<id>：查询单个资源。
    call("GET", f"/books/{book_id}")

    # 4) PATCH /books/<id>：借书，状态从 available -> borrowed（State Transfer）。
    call("PATCH", f"/books/{book_id}", {"status": "borrowed"})

    # 5) 再借一次同一本书：违反状态机规则，服务器应返回 409 Conflict。
    call("PATCH", f"/books/{book_id}", {"status": "borrowed"})

    # 6) PUT /books/<id>：整体替换资源（例如还书 + 改标题）。
    call(
        "PUT",
        f"/books/{book_id}",
        {"title": "REST (2nd Edition)", "author": "Roy Fielding", "status": "available"},
    )

    # 7) DELETE /books/<id>：删除资源。
    call("DELETE", f"/books/{book_id}")

    # 8) 确认已经被删除：应返回 404。
    call("GET", f"/books/{book_id}")


if __name__ == "__main__":
    main()
