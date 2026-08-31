"""
场景：指针数组 —— 用不同 Comparator 反复对同一批"指针"重新排序
心智模型：数据只有一份，指针（在 Python 里就是对象引用）数组可以
         按需切换排序规则，而不需要为每一种排序方式复制一份数据。
"""

from dataclasses import dataclass


@dataclass
class Employee:
    id: int
    name: str
    department: str
    salary: float


def print_view(label, employees):
    print(f"{label}:")
    for e in employees:
        print(f"  id={e.id} name={e.name:<6} dept={e.department:<6} salary={e.salary:.0f}")


def main():
    pool = [
        Employee(1, "Alice", "Eng", 8600),
        Employee(2, "Bob", "Sales", 5200),
        Employee(3, "Carol", "Eng", 12000),
        Employee(4, "Dave", "Sales", 7300),
        Employee(5, "Eve", "Eng", 9600),
        Employee(6, "Frank", "Sales", 6100),
        Employee(7, "Grace", "HR", 7000),
        Employee(8, "Heidi", "HR", 6800),
    ]

    print("=== 指针数组：同一批引用，按不同 Comparator 反复排序 ===")

    view_by_name = sorted(pool, key=lambda e: e.name)
    print_view("按 name 排序", view_by_name)

    view_by_salary_desc = sorted(pool, key=lambda e: -e.salary)
    print_view("按 salary 从高到低排序", view_by_salary_desc)

    print(
        "\n说明：pool 里的 Employee 对象没有被复制或修改过，"
        "sorted() 每次只是生成一个新的引用列表，切换 key 即可"
        "得到完全不同的排序视图，代价远低于复制整个 Employee。"
    )


if __name__ == "__main__":
    main()
