"""
场景：数据库记录 —— 复合索引 ORDER BY department, salary DESC
心智模型：复合索引本质上也是排序 RowID，只是 Comparator（key）里组合了多个列。
"""

from dataclasses import dataclass


@dataclass
class Row:
    id: int
    name: str
    department: str
    salary: float


def main():
    table = [
        Row(1, "Alice", "Eng", 8600), Row(2, "Bob", "Sales", 5200),
        Row(3, "Carol", "Eng", 12000), Row(4, "Dave", "Sales", 7300),
        Row(5, "Eve", "Eng", 9600), Row(6, "Frank", "Sales", 6100),
        Row(7, "Grace", "HR", 7000), Row(8, "Heidi", "HR", 6800),
    ]

    rowids = list(range(len(table)))

    print("=== 数据库记录：复合索引 ORDER BY department, salary DESC ===")

    # key 返回 (department, -salary)：department 升序，salary 降序
    rowids.sort(key=lambda rowid: (table[rowid].department, -table[rowid].salary))

    for rowid in rowids:
        r = table[rowid]
        print(f"  RowID={rowid} name={r.name:<6} dept={r.department:<6} salary={r.salary:.0f}")

    print(
        "\n说明：key 函数内部先比较 department，再比较 salary，"
        "真正被排序的仍然只是 rowids 里的整数，这就是数据库"
        "'复合索引'的核心思路：多列组合成一个排序规则，作用在行标识上。"
    )


if __name__ == "__main__":
    main()
