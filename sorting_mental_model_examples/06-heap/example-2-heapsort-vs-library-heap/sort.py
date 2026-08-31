"""
场景：堆 —— 手写堆排序 vs Python 标准库 heapq
心智模型：堆真正的价值在于"动态维护最值"，而不是一次性整体排序；
         内置 sorted() 通常比堆排序更快，这里做正确性和实现方式的对照。
"""

import heapq


def sift_down(arr, n, i):
    while True:
        left, right, largest = 2 * i + 1, 2 * i + 2, i
        if left < n and arr[left] > arr[largest]:
            largest = left
        if right < n and arr[right] > arr[largest]:
            largest = right
        if largest == i:
            break
        arr[i], arr[largest] = arr[largest], arr[i]
        i = largest


def manual_heap_sort(arr):
    n = len(arr)
    for i in range(n // 2 - 1, -1, -1):
        sift_down(arr, n, i)
    for end in range(n - 1, 0, -1):
        arr[0], arr[end] = arr[end], arr[0]
        sift_down(arr, end, 0)


def heapq_sort(arr):
    """用标准库 heapq 实现"排序"：建堆后反复弹出最小值。"""
    heap = arr.copy()
    heapq.heapify(heap)
    return [heapq.heappop(heap) for _ in range(len(heap))]


def main():
    arr = [23, 5, 67, 12, 89, 3, 45, 78, 1, 34]
    via_heapq = arr.copy()

    print("=== 堆排序：手写 Heap Sort vs 标准库 heapq ===")
    print(f"排序前: {arr}")

    manual_heap_sort(arr)
    result_heapq = heapq_sort(via_heapq)

    print(f"手写 Heap Sort 结果: {arr}")
    print(f"heapq(建堆+反复弹出最小值) 结果: {result_heapq}")

    ok = arr == result_heapq
    print(f"两种结果对比: {'一致（正确）' if ok else '不一致（错误）'}")
    print(
        "\n说明：heapq 提供的是堆这种数据结构的基本操作，而不是直接的排序函数；"
        "工程上如果只是要排序，直接用内置 sorted()（Timsort）通常更快；"
        "堆真正的价值在于动态维护最值（参见 example-1、example-3）。"
    )


if __name__ == "__main__":
    main()
