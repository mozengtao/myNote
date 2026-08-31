"""
场景：数据库记录 —— ORDER BY 排的是 RowID，而不是整行数据
心智模型：一行记录可能有几 KB，直接搬移整行代价很高；
         数据库内部通常排序 RowID/指针，最后再按需读取真正的行数据。
"""

from dataclasses import dataclass


@dataclass
class Row:
    id: int
    name: str
    salary: float


def main():
    table = [
        Row(1, "Alice", 8600), Row(2, "Bob", 5200), Row(3, "Carol", 12000), Row(4, "Dave", 7300),
        Row(5, "Eve", 9600), Row(6, "Frank", 6100), Row(7, "Grace", 7000), Row(8, "Heidi", 6800),
    ]

    rowids = list(range(len(table)))  # RowID 就是下标，模拟数据库的行标识
    original_first_id = id(table[0])

    print("=== 数据库记录：ORDER BY salary，只排序 RowID ===")

    rowids.sort(key=lambda rowid: table[rowid].salary)

    print("ORDER BY salary 结果 (通过 RowID 间接访问):")
    for rowid in rowids:
        r = table[rowid]
        print(f"  RowID={rowid} id={r.id} name={r.name:<6} salary={r.salary:.0f}")

    print(f"原始 table 是否原地未动: {'是' if id(table[0]) == original_first_id else '否'}")
    print(
        "\n说明：真正参与排序的是 rowids 列表里的整数（RowID），"
        "table 里的每一行都没有被移动过，这就是数据库常说的"
        "'排索引/排 RowID，而不是排整行数据'。"
    )


if __name__ == "__main__":
    main()
