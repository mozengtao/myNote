"""
场景：外部排序 —— 用最小堆做 K 路归并
心智模型：多个已经有序的小文件，可以通过维护一个"当前最小候选"的堆，
         每次只需要 O(log k) 就能确定下一个应该输出的元素。

Python 标准库的 heapq.merge() 正是为这个场景设计的：
它接受多个"已经有序"的可迭代对象，内部维护一个小顶堆，
按需从各个来源惰性地取值，天然支持流式读取大文件。
"""

import heapq
import os

K = 4


def read_sorted_ints(filename):
    """逐行读取文件，模拟"数据太大不能一次性载入内存"的流式读取。"""
    with open(filename) as f:
        for line in f:
            yield int(line)


def main():
    chunks = [
        [3, 9, 15, 22, 40, 58],
        [1, 6, 18, 25, 33, 47],
        [2, 11, 12, 30, 44, 60],
        [5, 8, 20, 27, 35, 50],
    ]
    filenames = [f"sorted_py_{i}.txt" for i in range(K)]

    print("=== 外部排序：K 路归并（最小堆） ===")

    for filename, chunk in zip(filenames, chunks):
        with open(filename, "w") as f:
            f.writelines(f"{v}\n" for v in chunk)
        print(f"已生成有序小文件 {filename}")

    # heapq.merge 内部就是用堆来维护"K 个来源里当前最小的候选"
    merged = list(heapq.merge(*(read_sorted_ints(fn) for fn in filenames)))

    for filename in filenames:
        os.remove(filename)

    print(f"\n归并结果 ({len(merged)} 个元素): {merged}")

    ok = all(merged[i - 1] <= merged[i] for i in range(1, len(merged)))
    print(f"归并结果是否整体有序: {'是' if ok else '否'}")
    print(
        f"\n说明：整个过程堆里最多只保留 {K} 个候选值，每次弹出堆顶就能确定"
        f"全局下一个最小值，不需要把 {K} 个文件的全部内容都读入内存再排序。"
    )


if __name__ == "__main__":
    main()
