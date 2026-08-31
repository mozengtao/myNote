/*
 * 场景：数据库记录 —— 模拟分页存储，排序 (PageID, Offset) 而不是整行数据
 * 心智模型：真实数据库把行存储在磁盘页(Page)中，排序时用 (PageID, Offset)
 *          这种"元组标识符"作为间接层，最后才按需读取真正的页面数据。
 */
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

constexpr int kPageSize = 4;
constexpr int kPageCount = 3;

struct Tuple {
    int id;
    std::string name;
    double salary;
};

struct TupleRef {
    int page_id;
    int offset;
};

int main()
{
    std::vector<std::vector<Tuple>> pages(kPageCount, std::vector<Tuple>(kPageSize));

    std::vector<std::string> names = {"Alice", "Bob", "Carol", "Dave", "Eve", "Frank",
                                       "Grace", "Heidi", "Ivan", "Judy", "Karl", "Liam"};
    std::vector<double> salaries = {8600, 5200, 12000, 7300, 9600, 6100,
                                     7000, 6800, 9100, 5600, 8900, 6400};

    int k = 0;
    for (int p = 0; p < kPageCount; p++) {
        for (int o = 0; o < kPageSize; o++) {
            pages[p][o] = {k + 1, names[k], salaries[k]};
            k++;
        }
    }

    auto resolve = [&](TupleRef ref) -> Tuple & { return pages[ref.page_id][ref.offset]; };

    std::vector<TupleRef> refs;
    for (int p = 0; p < kPageCount; p++)
        for (int o = 0; o < kPageSize; o++)
            refs.push_back({p, o});

    std::cout << "=== 数据库记录：模拟分页存储，排序 (PageID, Offset) ===\n";
    std::cout << "共有 " << kPageCount << " 页，每页 " << kPageSize << " 条记录，总计 "
              << (kPageCount * kPageSize) << " 条记录\n";

    std::sort(refs.begin(), refs.end(),
              [&](TupleRef a, TupleRef b) { return resolve(a).salary < resolve(b).salary; });

    std::cout << "ORDER BY salary 结果 (通过 (PageID,Offset) 间接访问):\n";
    for (const auto &ref : refs) {
        const Tuple &t = resolve(ref);
        std::cout << "  page=" << ref.page_id << " offset=" << ref.offset << " -> id=" << t.id
                  << " name=" << t.name << " salary=" << t.salary << "\n";
    }

    std::cout << "\n说明：排序过程中搬移的只是一个很小的 (PageID, Offset) 元组，\n"
                 "真正的行数据始终留在原来的 Page 里，这正是数据库存储引擎\n"
                 "内部排序/索引扫描的典型做法。\n";
    return 0;
}
