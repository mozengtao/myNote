"""
场景：任务调度 —— 类 vruntime 的加权公平调度，排序的是任务引用
心智模型：调度器不需要每次都对全部任务重新排序，只需要把刚运行完的
         任务重新插入到正确的位置（按 vruntime 排序的运行队列）。
"""

from dataclasses import dataclass


@dataclass
class Task:
    name: str
    weight: int  # 权重越大，占用 CPU 的比例应该越高
    vruntime: float = 0.0  # 虚拟运行时间：数值越小，越应该优先被调度


def reinsert_head(queue):
    """queue[0] 的 vruntime 刚变大，只需要把它移动到新的正确位置，
    不需要重新排序整个队列 —— 这就是"增量维护有序"而不是"整体重排"。"""
    t = queue[0]
    i = 0
    while i + 1 < len(queue) and queue[i + 1].vruntime < t.vruntime:
        queue[i] = queue[i + 1]
        i += 1
    queue[i] = t


def print_queue(label, queue):
    parts = " ".join(f"{t.name}(vr={t.vruntime:.1f})" for t in queue)
    print(f"{label}: {parts}")


def main():
    queue = [
        Task("nginx", 1),
        Task("backup", 1),
        Task("video-encode", 4),
        Task("logger", 1),
    ]

    print("=== 任务调度：加权公平调度（类 vruntime） ===")
    print_queue("初始运行队列", queue)

    slice_ns = 10.0  # 每次调度分配的时间片
    for round_no in range(1, 7):
        picked = queue[0]  # 队首永远是 vruntime 最小的任务
        print(f"第 {round_no} 轮：调度 [{picked.name}]（当前 vruntime={picked.vruntime:.1f}，权重={picked.weight}）")

        # 权重越大，vruntime 增长越慢，从而获得更多实际运行时间
        picked.vruntime += slice_ns / picked.weight

        reinsert_head(queue)
        print_queue("         调度后运行队列", queue)

    print(
        "\n说明：video-encode 权重是其它任务的 4 倍，vruntime 增长最慢，"
        "因此长期来看会被调度得更频繁，获得更多 CPU 时间；每一轮"
        "只需要把被调度过的任务重新插入队列，不需要整体重新排序。"
    )


if __name__ == "__main__":
    main()
