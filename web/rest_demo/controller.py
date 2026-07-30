"""
Controller 层
=============
对应架构图中：

    Router -> Controller -> Service

Controller 负责"HTTP 请求处理"（文档第九节表格）：
    1. 从 Flask 的 `request` 里拆出 Method + URI + Headers + Body；
    2. 调用 Service 执行业务逻辑；
    3. 把 Service 抛出的领域异常翻译成 HTTP 状态码；
    4. 用 jsonify() 把 Resource Object 序列化成 Representation，
       拼成 (Body, Status) 交还给 Router/Flask 去生成 HTTP Response。

Controller 本身不应该包含业务规则（那是 Service 的职责），
它只是"HTTP 世界"和"业务世界"之间的翻译层。
"""

from flask import jsonify, request

from database import Database
from repository import BookRepository
from service import BookService, BookNotFoundError, InvalidStatusTransitionError

# ---------------------------------------------------------------------------
# 组装（Composition Root）：把 Database -> Repository -> Service 串起来。
# 这几行本身不是某一个"框"，而是把架构图上下几层粘合在一起的地方。
# ---------------------------------------------------------------------------
_database = Database()
_repository = BookRepository(_database)
_service = BookService(_repository)


def list_books():
    """GET /books -> Router 分发过来的请求，在这里转交给 Service。"""
    print("[Controller] handle GET /books")
    books = _service.list_books()
    # Resource Object(list) -> Serialize(JSON) -> HTTP Response Body
    return jsonify([b.to_dict() for b in books]), 200


def get_book(book_id: int):
    """GET /books/<id>"""
    print(f"[Controller] handle GET /books/{book_id}")
    try:
        book = _service.get_book(book_id)
    except BookNotFoundError as exc:
        return jsonify({"error": str(exc)}), 404
    return jsonify(book.to_dict()), 200


def create_book():
    """POST /books -> 从 HTTP Body 里取出 Representation，创建资源。"""
    print("[Controller] handle POST /books")
    # request.get_json() 就是在解析 "HTTP Request: Method + URI + Headers + Body" 里的 Body
    payload = request.get_json(force=True) or {}
    title = payload.get("title")
    author = payload.get("author")
    if not title or not author:
        return jsonify({"error": "title and author are required"}), 400

    book = _service.create_book(title=title, author=author)
    # 201 Created，Header 里通常还会带 Location，这里为了简化只返回 Body。
    return jsonify(book.to_dict()), 201


def replace_book(book_id: int):
    """PUT /books/<id> -> 整体替换资源。"""
    print(f"[Controller] handle PUT /books/{book_id}")
    payload = request.get_json(force=True) or {}
    try:
        book = _service.replace_book(
            book_id,
            title=payload.get("title"),
            author=payload.get("author"),
            status=payload.get("status", "available"),
        )
    except BookNotFoundError as exc:
        return jsonify({"error": str(exc)}), 404
    except ValueError as exc:
        return jsonify({"error": str(exc)}), 400
    return jsonify(book.to_dict()), 200


def patch_status(book_id: int):
    """PATCH /books/<id> -> 局部更新，这里专门用于借书/还书（状态转移）。"""
    print(f"[Controller] handle PATCH /books/{book_id}")
    payload = request.get_json(force=True) or {}
    new_status = payload.get("status")
    if not new_status:
        return jsonify({"error": "status is required"}), 400

    try:
        book = _service.update_status(book_id, new_status)
    except BookNotFoundError as exc:
        return jsonify({"error": str(exc)}), 404
    except InvalidStatusTransitionError as exc:
        # 409 Conflict：请求本身合法，但和资源当前状态冲突（例如借一本已被借走的书）。
        return jsonify({"error": str(exc)}), 409
    except ValueError as exc:
        return jsonify({"error": str(exc)}), 400
    return jsonify(book.to_dict()), 200


def delete_book(book_id: int):
    """DELETE /books/<id>"""
    print(f"[Controller] handle DELETE /books/{book_id}")
    try:
        _service.delete_book(book_id)
    except BookNotFoundError as exc:
        return jsonify({"error": str(exc)}), 404
    # 204 No Content：删除成功，Body 为空。
    return "", 204
