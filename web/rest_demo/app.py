"""
Router 层 + 服务器入口
======================
对应架构图最顶端的两步：

    Client -> HTTP Request -> Router -> Controller -> ...

Router 的职责很单一：把 "URI + HTTP Method" 映射到某一个 Controller 函数
（文档第九节表格："Router：URI 路由"）。Router 自己不处理业务，
也不关心 Body 里是什么，它只做"分发"。

运行方式：
    pip install -r requirements.txt
    python app.py
"""

import sys

from flask import Flask, jsonify

import controller

# 当 stdout 没有连到终端（例如被重定向到文件/日志收集器）时，Python 默认会用
# 块缓冲，导致 [Router]/[Controller]/[Service]/[Database] 这些教学用的追踪日志
# 不能实时出现。这里强制按行刷新，保证请求处理过程中的日志立刻可见。
sys.stdout.reconfigure(line_buffering=True)

app = Flask(__name__)

# ---------------------------------------------------------------------------
# Router：URI + HTTP Method -> Controller 函数。
# 每一行都对应架构图里 "Method + URI + Headers + Body" 被 Router 分发到
# 某个 Controller 的过程。
# ---------------------------------------------------------------------------
app.add_url_rule("/books", view_func=controller.list_books, methods=["GET"])
app.add_url_rule("/books", view_func=controller.create_book, methods=["POST"])
app.add_url_rule("/books/<int:book_id>", view_func=controller.get_book, methods=["GET"])
app.add_url_rule("/books/<int:book_id>", view_func=controller.replace_book, methods=["PUT"])
app.add_url_rule("/books/<int:book_id>", view_func=controller.patch_status, methods=["PATCH"])
app.add_url_rule("/books/<int:book_id>", view_func=controller.delete_book, methods=["DELETE"])


@app.errorhandler(404)
def handle_not_found(_err):
    # URI 本身没有匹配到任何路由（区别于"资源不存在"的业务级 404）。
    return jsonify({"error": "no such route"}), 404


if __name__ == "__main__":
    print("REST demo server starting on http://127.0.0.1:5000")
    print("Layers wired as: Router(app.py) -> Controller -> Service -> Repository -> Database")
    # debug=True 方便看报错栈；这是一个教学 demo，不建议直接照搬到生产环境。
    app.run(host="127.0.0.1", port=5000, debug=True, use_reloader=False)
