"""
场景：外部排序 —— 完整流程：生成大数据 -> 分块排序 -> 落盘 -> K 路归并 -> 校验
心智模型：内存排序解决不了的问题，就把排序过程分阶段完成——
         这里把 example-1（分块落盘）和 example-2（K 路归并）串联成完整流程。
"""

import heapq
import os
import random

TOTAL = 100
MEM_LIMIT = 10  # 模拟内存一次只能容纳 10 个元素


def read_sorted_ints(filename):
    with open(filename) as f:
        for line in f:
            yield int(line)


def main():
    random.seed(2024)
    data = [random.randint(0, 9999) for _ in range(TOTAL)]

    chunk_count = (TOTAL + MEM_LIMIT - 1) // MEM_LIMIT

    print("=== 外部排序完整流程：生成 -> 分块排序 -> 落盘 -> K 路归并 -> 校验 ===")
    print(f"总数据量 = {TOTAL}，模拟内存一次只能容纳 {MEM_LIMIT} 个元素，共分 {chunk_count} 块\n")

    # 阶段一：分块排序 + 落盘
    filenames = []
    for c in range(chunk_count):
        start = c * MEM_LIMIT
        chunk = sorted(data[start:start + MEM_LIMIT])
        filename = f"es_chunk_py_{c}.txt"
        with open(filename, "w") as f:
            f.writelines(f"{v}\n" for v in chunk)
        filenames.append(filename)

    print(f"阶段一完成：{chunk_count} 个块均已在内存中排序并写入磁盘")

    # 阶段二：K 路归并（heapq.merge 内部就是一个小顶堆）
    merged = list(heapq.merge(*(read_sorted_ints(fn) for fn in filenames)))

    for filename in filenames:
        os.remove(filename)

    print(f"阶段二完成：{chunk_count} 路归并结束，输出 {len(merged)} 个元素，临时文件已清理")

    # 校验：与整体内存排序(sorted())结果做对照
    reference = sorted(data)
    ok = merged == reference

    print(f"\n与整体内存排序(sorted())结果对比: {'完全一致（正确）' if ok else '不一致（错误）'}")
    print(
        f"\n说明：全程没有一次性把 {TOTAL} 个元素都放进内存排序，而是拆成 {chunk_count} 个"
        "可以放进内存的小块分别排序，再通过 K 路归并合并成最终结果——"
        "这就是数据库、Hadoop、Spark 等系统处理超大数据排序的标准做法。"
    )


if __name__ == "__main__":
    main()
