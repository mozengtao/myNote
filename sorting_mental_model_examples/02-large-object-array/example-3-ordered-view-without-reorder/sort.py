"""
场景：大对象数组 —— 同一份数据的多个"有序视图"
心智模型：数据本身只保存一份且从不移动，可以同时维护多个不同排序
         规则的引用列表，得到多个互不冲突的"有序视图"。
"""


class Record:
    def __init__(self, id_, name, score):
        self.id = id_
        self.name = name
        self.score = score
        self.payload = "x" * 200  # 模拟大对象负载


def print_view(label, view):
    print(f"{label}:")
    for r in view:
        print(f"  id={r.id} name={r.name} score={r.score}")


def main():
    records = [
        Record(1, "Frank", 72.5), Record(2, "Alice", 88.0), Record(3, "Eve", 65.3),
        Record(4, "Carol", 91.2), Record(5, "Bob", 77.8), Record(6, "Grace", 83.4),
        Record(7, "Heidi", 59.9), Record(8, "Dave", 95.0),
    ]

    original_first_id = id(records[0])

    print("=== 大对象数组：同一份数据的两个有序视图 ===")

    # sorted() 不修改原 list，也不拷贝 Record 对象，只是生成一个新的引用列表
    view_by_score = sorted(records, key=lambda r: r.score)
    view_by_name = sorted(records, key=lambda r: r.name)

    print_view("视图一：按 score 排序", view_by_score)
    print_view("视图二：按 name 排序", view_by_name)

    print(f"原始 list 是否原地未动: {'是' if id(records[0]) == original_first_id else '否'}")
    print(f"records[0] 仍然是: id={records[0].id} name={records[0].name}（未被排序改变）")
    print(
        "\n说明：records 只保存一份数据，view_by_score / view_by_name 是两个"
        "独立的'有序视图'（新的引用列表），彼此互不干扰，也没有拷贝任何 Record。"
    )


if __name__ == "__main__":
    main()
