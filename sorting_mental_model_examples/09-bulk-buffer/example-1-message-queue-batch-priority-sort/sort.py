"""
场景：批量缓冲区 —— 消息队列批量出队后按优先级排序指针
心智模型：消息体可能较大（几百字节到几 KB），批处理时只排序消息引用，
         消息本身留在原来的位置不动。
"""

from dataclasses import dataclass


@dataclass
class Message:
    id: int
    priority: int  # 数值越大优先级越高
    payload: bytes


def main():
    priorities = [2, 5, 2, 8, 5, 1, 8, 3]
    inbox = [
        Message(i + 1, p, bytes([97 + i]) * 256) for i, p in enumerate(priorities)
    ]

    print("=== 批量缓冲区：消息队列批量出队后按优先级排序指针 ===")
    print("到达顺序 (id:priority): " + " ".join(f"{m.id}:{m.priority}" for m in inbox))

    # sorted() 天然稳定：优先级相同时保留原始到达顺序（FIFO）
    processed_order = sorted(inbox, key=lambda m: -m.priority)

    print(
        "按优先级处理顺序 (id:priority): "
        + " ".join(f"{m.id}:{m.priority}" for m in processed_order)
    )
    print(
        "\n说明：id=2 和 id=5 优先级都是 5，排序后 id=2 仍然排在 id=5 前面，"
        "这是 sorted() 天然的稳定性保留的到达顺序（FIFO）。整个过程只重排了引用，"
        "每条消息的 payload 都没有被拷贝或搬移过。"
    )


if __name__ == "__main__":
    main()
