"""
场景：大对象数组 —— Python 中天然的"指针排序"
心智模型：对象保持不动，排序的是访问路径（引用）。

在 C / C++ 里，需要显式建立 Record* index[N] 才能避免直接拷贝大对象；
而 Python 的 list 本身存的就是对象引用，sorted()/list.sort() 从来不会
拷贝对象本体——这一点可以通过 id() 不变来验证。
"""

import random


class Record:
    __slots__ = ("id", "score", "payload")

    def __init__(self, id_, score, payload):
        self.id = id_
        self.score = score
        self.payload = payload  # 模拟大对象负载（比如一段很长的字符串/字节串）


def main():
    random.seed(42)
    n = 2000
    payload_size = 500

    records = [
        Record(i, random.uniform(0, 1000), bytes([65 + i % 26]) * payload_size)
        for i in range(n)
    ]

    # 排序前记录每个对象的身份（引用标识）
    original_ids = {id(r) for r in records}

    print("=== 大对象数组：Python list 天然只排序引用 ===")
    print(f"记录数 = {n}, 每个 payload 大小 约 {payload_size} 字节")

    records_sorted = sorted(records, key=lambda r: r.score)

    ok = all(records_sorted[i - 1].score <= records_sorted[i].score for i in range(1, n))
    same_objects = {id(r) for r in records_sorted} == original_ids

    print(f"排序结果: {'有序（正确）' if ok else '无序（错误）'}")
    print(f"排序前后对象身份集合是否完全相同: {'是（没有拷贝任何 Record）' if same_objects else '否'}")
    print(
        "\n说明：sorted() 只是重新排列了引用的顺序，Record 对象本身"
        "（包括很大的 payload）从未被拷贝或移动，这与 C/C++ 里手动维护"
        "指针索引数组达到的效果完全一致。"
    )


if __name__ == "__main__":
    main()
