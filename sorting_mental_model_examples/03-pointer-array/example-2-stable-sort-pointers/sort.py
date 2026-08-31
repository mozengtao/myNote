"""
场景：指针数组 —— 稳定排序（保持相同 Key 的相对顺序）
心智模型：Python 内置的 sorted()/list.sort()（Timsort）本身就是稳定的，
         排序的是引用，相同 Key 记录的原有相对顺序会被保留。
"""

from dataclasses import dataclass


@dataclass
class Employee:
    id: int
    name: str
    department: str


def print_view(label, employees):
    print(f"{label}:")
    for e in employees:
        print(f"  id={e.id} name={e.name:<6} dept={e.department:<6}")


def main():
    pool = [
        Employee(1, "Alice", "Eng"),
        Employee(2, "Bob", "Sales"),
        Employee(3, "Carol", "Eng"),
        Employee(4, "Dave", "Sales"),
        Employee(5, "Eve", "Eng"),
        Employee(6, "Frank", "Sales"),
        Employee(7, "Grace", "HR"),
        Employee(8, "Heidi", "HR"),
    ]

    print("=== 指针数组：sorted() 天然稳定，保持相同 department 的相对顺序 ===")
    print_view("排序前（按 id 插入顺序）", pool)

    stable_view = sorted(pool, key=lambda e: e.department)

    print_view("稳定排序后（按 department）", stable_view)
    print(
        "\n说明：Eng 组内部仍然是 Alice(1) -> Carol(3) -> Eve(5) 的原始相对顺序，"
        "Sales 组内部仍然是 Bob(2) -> Dave(4) -> Frank(6)。"
        "Python 的 sorted() 保证稳定，这是 Timsort 算法自带的性质。"
    )


if __name__ == "__main__":
    main()
