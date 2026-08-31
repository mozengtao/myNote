"""
场景：外部排序 —— 数据量超过内存限制时，分块排序后落盘临时文件
心智模型：内存排序解决不了的问题，就把排序过程分阶段完成。
"""

import os
import random

TOTAL = 40
CHUNK_SIZE = 8  # 模拟"内存一次只能装下 8 个数"


def main():
    random.seed(20240101)
    data = [random.randint(0, 999) for _ in range(TOTAL)]

    print("=== 外部排序：分块排序 + 落盘临时文件 ===")
    print(f"总数据量 = {TOTAL}，模拟内存一次只能容纳 {CHUNK_SIZE} 个元素")

    chunk_count = (TOTAL + CHUNK_SIZE - 1) // CHUNK_SIZE
    filenames = []

    for c in range(chunk_count):
        start = c * CHUNK_SIZE
        chunk = sorted(data[start:start + CHUNK_SIZE])  # 块内排序：内存完全放得下

        filename = f"chunk_py_{c}.txt"
        with open(filename, "w") as f:
            f.writelines(f"{v}\n" for v in chunk)
        filenames.append(filename)

        print(f"第 {c} 块 ({len(chunk)} 个元素) 已排序并写入磁盘文件 {filename}")

    # 验证每个文件内部确实有序，然后清理临时文件
    all_sorted = True
    for filename in filenames:
        with open(filename) as f:
            values = [int(line) for line in f]
        if values != sorted(values):
            all_sorted = False
        os.remove(filename)

    print(f"\n所有分块文件内部有序: {'是' if all_sorted else '否'}（临时文件已清理）")
    print(
        "\n说明：每一块都在内存里完成排序，然后立刻写入磁盘释放内存。"
        "接下来用 K 路归并（参见 example-2）即可把这些有序小文件合并成"
        "一个整体有序结果，这就是 External Merge Sort 的第一阶段。"
    )


if __name__ == "__main__":
    main()
