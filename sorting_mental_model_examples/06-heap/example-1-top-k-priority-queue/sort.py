"""
场景：堆 —— 用小顶堆维护数据流中最大的 K 个元素（Top-K）
心智模型：堆不是为了得到完整有序序列，而是为了快速获取当前最值。
"""

import heapq

K = 4


def main():
    stream = [5, 81, 3, 42, 97, 12, 6, 63, 29, 74, 8, 55]

    print(f"=== 堆：只用小顶堆维护 Top-{K}，不排序整条数据流 ===")
    print(f"数据流(共 {len(stream)} 个): {stream}")

    min_heap = []  # heapq 默认就是小顶堆
    for v in stream:
        if len(min_heap) < K:
            heapq.heappush(min_heap, v)
        elif v > min_heap[0]:
            heapq.heapreplace(min_heap, v)
        # v <= min_heap[0] 时直接丢弃，不需要进入 Top-K

    result = sorted(min_heap)
    print(f"Top-{K} 最大值: {result}")
    print(
        f"\n说明：整条数据流只需要遍历一次，堆里始终只保留 {K} 个候选元素，"
        "既不需要把全部数据存下来，也不需要对全部数据排序。"
    )


if __name__ == "__main__":
    main()
