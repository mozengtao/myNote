"""
场景：任务调度 —— "只取下一个"而非整体重排的增量选择模式
心智模型：很多调度器根本不需要一个完整有序的序列，只需要每次能
         快速回答"接下来该运行谁"。用一次线性扫描找最优任务、
         原地标记完成，比每次都对剩余任务重新排序要简单直接得多，
         尤其是当任务会不断有新到达、旧完成、优先级会变化时。
"""

from dataclasses import dataclass


@dataclass
class Task:
    name: str
    priority: int  # 数值越小优先级越高
    done: bool = False


def pick_next(tasks):
    """只扫描一次，在"未完成"的任务里找优先级最高的那个，
    不对任务列表做任何排序或搬运 —— 这是最朴素的"选择模式"。"""
    best = None
    for t in tasks:
        if t.done:
            continue
        if best is None or t.priority < best.priority:
            best = t
    return best


def print_pending(label, tasks):
    parts = " ".join(f"{t.name}(pri={t.priority})" for t in tasks if not t.done)
    print(f"{label}: {parts}")


def main():
    tasks = [
        Task("cleanup", 5),
        Task("alert", 1),
        Task("report", 3),
        Task("backup", 4),
        Task("healthcheck", 2),
    ]

    print("=== 任务调度：增量选择（不整体排序） ===")
    print_pending("初始待处理任务", tasks)

    round_no = 1
    while True:
        next_task = pick_next(tasks)
        if next_task is None:
            break
        print(f"第 {round_no} 轮：选中 [{next_task.name}]（优先级={next_task.priority}）")
        next_task.done = True  # 原地标记完成，不移动任何元素

        # 模拟运行期间有新任务到达：直接追加到末尾，不需要
        # 重新排序已有任务，也不影响后续 pick_next() 的正确性。
        if round_no == 2:
            tasks.append(Task("urgent-fix", 0))
            print("        （运行期间新任务 [urgent-fix] 到达，优先级=0）")

        print_pending("        剩余待处理任务", tasks)
        round_no += 1

    print(
        "\n说明：pick_next() 每次只做一次 O(n) 扫描来回答\"接下来该运行谁\"，"
        "任务列表自始至终没有被排序或搬运过；新任务可以随时追加，"
        "优先级也可以随时变化，都不影响下一次选择的正确性。"
        "这与前两个示例（增量插入、堆）是同一心智模式的另一种朴素实现："
        "调度器要的从来不是\"整体有序\"，而是\"每次都能选对下一个\"。"
    )


if __name__ == "__main__":
    main()
