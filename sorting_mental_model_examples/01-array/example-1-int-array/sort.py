"""
场景：连续整型数组（list）排序
心智模型：连续内存 + 小对象 -> 直接排序，Move Strategy 就是"交换值本身"

Python 的 list.sort()/sorted() 底层是 Timsort（归并 + 二分插入排序的混合算法），
工程实践中应直接使用内置排序，这里通过 key 展示 Comparator 与容器的解耦。
"""


def print_list(label, data):
    print(f"{label}: {data}")


def main():
    scores = [88, 45, 67, 12, 99, 34, 76, 5, 58, 23]

    print("=== 连续数组排序：内置 sorted() + Comparator(key) ===")
    print_list("排序前", scores)

    # Comparator 通过 key 提供，Move Strategy（如何移动元素）由 Timsort 内部决定
    sorted_scores = sorted(scores, key=lambda x: x)

    print_list("排序后", sorted_scores)
    print(
        "\n说明：list 底层是连续的对象引用数组，元素是不可变整数，"
        "直接调用内置 sorted() 即可，无需手写算法。"
    )


if __name__ == "__main__":
    main()
