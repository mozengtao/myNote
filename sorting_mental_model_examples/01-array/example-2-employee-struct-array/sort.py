"""
场景：小型对象数组（列表 of dataclass）按薪资排序
心智模型：排序算法只关心 Key（salary），Comparator 与 Record 解耦
"""

from dataclasses import dataclass


@dataclass
class Employee:
    id: int
    name: str
    salary: float


def print_employees(label, employees):
    print(f"{label}:")
    for e in employees:
        print(f"  id={e.id} name={e.name:<8} salary={e.salary:.2f}")


def main():
    employees = [
        Employee(1, "Alice", 8600.0),
        Employee(2, "Bob", 5200.0),
        Employee(3, "Carol", 12000.0),
        Employee(4, "Dave", 7300.0),
        Employee(5, "Eve", 9600.0),
    ]

    print("=== 小对象数组排序：按 salary 排序 Employee ===")
    print_employees("排序前", employees)

    # key 就是 Comparator 的另一种表达形式：只告诉排序算法"按什么比较"
    employees_sorted = sorted(employees, key=lambda e: e.salary)

    print_employees("排序后", employees_sorted)
    print(
        "\n说明：Python list 中存的其实是对象引用，sorted() 内部搬移的是引用，"
        "并不会拷贝 Employee 对象本身——这天然就是'排指针'的思路（对照 03-pointer-array）。"
    )


if __name__ == "__main__":
    main()
