"""
Resource Object + Representation
=================================
对应架构图中的两个环节：

    Database -> Resource Object -> Serialize(JSON/XML)

`Book` 就是从数据库取出来之后、在内存里表示的那个"资源对象"（Resource Object）。
`to_dict()` 就是"Serialize"这一步：把 Python 对象转换成可以在网络上传输的
Representation（这里选择 JSON 作为表示格式）。
"""

from dataclasses import dataclass, asdict


@dataclass
class Book:
    """Resource Object：内存中的资源，对应 URI `/books/<id>`。"""

    id: int
    title: str
    author: str
    # status 是资源的 State（可用状态机：available <-> borrowed），
    # 对应文档第三节 "State（状态）" 的例子。
    status: str = "available"

    def to_dict(self) -> dict:
        """Serialize：Resource Object -> Representation(dict/JSON)。

        Flask 的 jsonify() 会把这里返回的 dict 序列化成 JSON 字符串，
        并写入 HTTP Response 的 Body，这正是架构图里
        "Serialize(JSON/XML)" -> "HTTP Response" 这一步。
        """
        return asdict(self)
