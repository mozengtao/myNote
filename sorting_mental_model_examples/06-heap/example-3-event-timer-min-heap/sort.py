"""
场景：堆 —— 定时器/事件队列，动态维护"下一个最早触发的事件"
心智模型：堆不是为了得到完整有序序列，而是为了快速获取当前最值，
         并且可以在新事件加入后继续保持这个能力，而不需要重新排序全部事件。
"""

import heapq
import itertools


def main():
    heap = []
    counter = itertools.count()  # 用递增序号打破 fire_time 相同时的比较歧义

    def push_event(fire_time, name, period):
        heapq.heappush(heap, (fire_time, next(counter), name, period))

    push_event(100, "heartbeat", 50)
    push_event(80, "log-flush", 40)
    push_event(200, "report", 0)
    push_event(30, "retry", 20)

    print("=== 堆：定时器事件队列，动态维护最早触发的事件 ===")
    print(f"初始事件数 = {len(heap)}\n")

    round_no = 1
    while heap and round_no <= 6:
        fire_time, _, name, period = heapq.heappop(heap)
        print(f"第 {round_no} 轮：触发事件 [{name}] fire_time={fire_time}")
        if period > 0:
            next_fire_time = fire_time + period
            push_event(next_fire_time, name, period)
            print(f"         -> 周期事件重新调度到 fire_time={next_fire_time}")
        round_no += 1

    print(
        "\n说明：每次只花 O(log n) 就能取出当前最早的事件，新事件（包括"
        "周期性事件的下一次触发）随时可以加入堆中，完全不需要对整个"
        "事件列表重新排序。heapq 就是 Python 里最常用的实现方式。"
    )


if __name__ == "__main__":
    main()
