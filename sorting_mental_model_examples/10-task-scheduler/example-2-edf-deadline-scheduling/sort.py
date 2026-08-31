"""
场景：任务调度 —— 最早截止时间优先（EDF），用堆维护任务引用
心智模型：调度器只关心"下一个该运行的是谁"，用堆可以在 O(log N)
         时间内完成插入新任务、取出截止时间最近的任务，
         完全不需要对整个任务集合做完整排序。
"""

import heapq
from dataclasses import dataclass, field


@dataclass
class Task:
    name: str
    deadline: int  # 截止时间：数值越小越紧急
    duration: int  # 预计运行时长


@dataclass(order=True)
class HeapEntry:
    """堆里只存放排序键 + 任务引用，比较时不涉及 Task 本体的搬运。"""
    deadline: int
    task: Task = field(compare=False)


def main():
    pool = [
        Task("render-frame", 8, 2),
        Task("send-heartbeat", 3, 1),
        Task("flush-log", 15, 4),
        Task("process-input", 5, 1),
        Task("sync-db", 20, 6),
    ]

    print("=== 任务调度：最早截止时间优先（EDF） ===")
    print("任务到达顺序（未排序）：" + " ".join(f"{t.name}(ddl={t.deadline})" for t in pool))
    print()

    # 只把 (deadline, 引用) 压入堆，堆内部的比较/交换从不触碰 Task 本体
    heap = []
    for t in pool:
        heapq.heappush(heap, HeapEntry(t.deadline, t))

    print("按截止时间依次调度：")
    clock = 0
    while heap:
        entry = heapq.heappop(heap)
        t = entry.task
        overdue = "<- 已超过截止时间！" if clock + t.duration > t.deadline else ""
        print(f"  时刻 {clock:2d}：运行 [{t.name}]（截止时间={t.deadline}，时长={t.duration}）{overdue}")
        clock += t.duration

    print(
        "\n说明：EDF 调度不需要提前把所有任务按截止时间整体排序一次；"
        "新任务随时可以 O(log N) 插入堆中，取出下一个任务也是 O(log N)，"
        "堆里搬运的始终只是任务引用，Task 本体从未被移动过。"
    )


if __name__ == "__main__":
    main()
