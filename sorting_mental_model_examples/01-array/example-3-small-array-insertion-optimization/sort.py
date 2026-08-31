"""
场景：大数组 + 小子数组优化（Quick Sort 混合 Insertion Sort）
心智模型：子数组足够小时，Insertion Sort 常数因子更低，这是很多工程级
         排序实现（包括 Python 自己的 Timsort）内部采用的优化思路。

注意：Python 内置的 sorted()/list.sort() 使用 Timsort，本身就是一种
"小规模用二分插入排序，大规模用归并"的混合算法。这里手写一个
Quick Sort + Insertion Sort 的混合版本，用来对照理解同样的心智模式。
"""

import random

INSERTION_THRESHOLD = 16


def insertion_sort(arr, low, high):
    for i in range(low + 1, high + 1):
        key = arr[i]
        j = i - 1
        while j >= low and arr[j] > key:
            arr[j + 1] = arr[j]
            j -= 1
        arr[j + 1] = key


def partition(arr, low, high):
    pivot = arr[high]
    i = low - 1
    for j in range(low, high):
        if arr[j] < pivot:
            i += 1
            arr[i], arr[j] = arr[j], arr[i]
    arr[i + 1], arr[high] = arr[high], arr[i + 1]
    return i + 1


def hybrid_sort(arr, low, high):
    while low < high:
        if high - low + 1 <= INSERTION_THRESHOLD:
            insertion_sort(arr, low, high)
            return
        p = partition(arr, low, high)
        hybrid_sort(arr, low, p - 1)
        low = p + 1


def main():
    random.seed(42)
    n = 40
    arr = [random.randint(0, 999) for _ in range(n)]
    reference = sorted(arr)

    print("=== 小数组优化：Quick Sort + Insertion Sort 混合策略 ===")
    print(f"数组长度={n}, Insertion 阈值={INSERTION_THRESHOLD}")

    hybrid_sort(arr, 0, n - 1)

    ok = arr == reference
    print(f"排序后前 10 个元素: {arr[:10]}")
    print(f"结果与内置 sorted() 对比: {'一致（正确）' if ok else '不一致（错误）'}")
    print(
        f"\n说明：子数组长度 <= {INSERTION_THRESHOLD} 时切换 Insertion Sort，"
        "Python 内置的 Timsort 也采用了类似的'小规模插入排序'优化。"
    )


if __name__ == "__main__":
    main()
