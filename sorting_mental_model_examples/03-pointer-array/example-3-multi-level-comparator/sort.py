"""
场景：指针数组 —— 多级比较器（先按 department，再按 salary 降序）
心智模型：Comparator（这里就是 key 函数）可以组合多个 Key，
         形成"复合排序规则"，Move Strategy（重排引用）完全不受影响。
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

    print("=== 指针数组：多级 Comparator（先 department，再 salary 降序） ===")

    # key 返回一个元组 (department, -salary)：department 升序，salary 降序
    result = sorted(pool, key=lambda e: (e.department, -e.salary))

    print_view("排序结果", result)
    print(
        "\n说明：key 函数内部先比较 department，相同部门再比较 salary，"
        "排序算法本身完全不需要修改，Move Strategy 依旧只是重排引用。"
    )


if __name__ == "__main__":
    main()
