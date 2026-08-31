"""
场景：数据库记录 —— 模拟分页存储，排序 (PageID, Offset) 而不是整行数据
心智模型：真实数据库把行存储在磁盘页(Page)中，排序时用 (PageID, Offset)
         这种"元组标识符"作为间接层，最后才按需读取真正的页面数据。
"""

PAGE_SIZE = 4
PAGE_COUNT = 3


def main():
    names = ["Alice", "Bob", "Carol", "Dave", "Eve", "Frank",
             "Grace", "Heidi", "Ivan", "Judy", "Karl", "Liam"]
    salaries = [8600, 5200, 12000, 7300, 9600, 6100, 7000, 6800, 9100, 5600, 8900, 6400]

    # pages[page_id][offset] = 一条记录（模拟磁盘页里的定长记录布局）
    pages = [[None] * PAGE_SIZE for _ in range(PAGE_COUNT)]
    k = 0
    for p in range(PAGE_COUNT):
        for o in range(PAGE_SIZE):
            pages[p][o] = {"id": k + 1, "name": names[k], "salary": salaries[k]}
            k += 1

    def resolve(ref):
        page_id, offset = ref
        return pages[page_id][offset]

    refs = [(p, o) for p in range(PAGE_COUNT) for o in range(PAGE_SIZE)]

    print("=== 数据库记录：模拟分页存储，排序 (PageID, Offset) ===")
    print(f"共有 {PAGE_COUNT} 页，每页 {PAGE_SIZE} 条记录，总计 {PAGE_COUNT * PAGE_SIZE} 条记录")

    refs.sort(key=lambda ref: resolve(ref)["salary"])

    print("ORDER BY salary 结果 (通过 (PageID,Offset) 间接访问):")
    for ref in refs:
        t = resolve(ref)
        print(f"  page={ref[0]} offset={ref[1]} -> id={t['id']} name={t['name']:<6} salary={t['salary']:.0f}")

    print(
        "\n说明：排序过程中搬移的只是一个很小的 (PageID, Offset) 元组，"
        "真正的行数据始终留在原来的 Page 里，这正是数据库存储引擎"
        "内部排序/索引扫描的典型做法。"
    )


if __name__ == "__main__":
    main()
