/*
 * 场景：数据库记录 —— ORDER BY 排的是 RowID，而不是整行数据
 * 心智模型：一行记录可能有几 KB，直接搬移整行代价很高；
 *          数据库内部通常排序 RowID/指针，最后再按需读取真正的行数据。
 */
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

struct Row {
    int id;
    std::string name;
    double salary;
};

int main()
{
    std::vector<Row> table = {
        {1, "Alice", 8600}, {2, "Bob", 5200}, {3, "Carol", 12000}, {4, "Dave", 7300},
        {5, "Eve", 9600}, {6, "Frank", 6100}, {7, "Grace", 7000}, {8, "Heidi", 6800},
    };

    std::vector<int> rowids(table.size());
    for (size_t i = 0; i < table.size(); i++)
        rowids[i] = static_cast<int>(i); // RowID 就是下标，模拟数据库的行标识

    const Row *original_first = &table[0];

    std::cout << "=== 数据库记录：ORDER BY salary，只排序 RowID ===\n";

    std::sort(rowids.begin(), rowids.end(),
              [&](int a, int b) { return table[a].salary < table[b].salary; });

    std::cout << "ORDER BY salary 结果 (通过 RowID 间接访问):\n";
    for (int rowid : rowids) {
        const Row &r = table[rowid];
        std::cout << "  RowID=" << rowid << " id=" << r.id << " name=" << r.name
                  << " salary=" << r.salary << "\n";
    }

    std::cout << "原始 table 是否原地未动: " << (original_first == &table[0] ? "是" : "否") << "\n";
    std::cout << "\n说明：真正参与排序的是 rowids 数组里的整数（RowID），\n"
                 "table 里的每一行都没有被移动过，这就是数据库常说的\n"
                 "'排索引/排 RowID，而不是排整行数据'。\n";
    return 0;
}
