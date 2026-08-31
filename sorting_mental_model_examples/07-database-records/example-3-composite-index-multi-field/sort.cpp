/*
 * 场景：数据库记录 —— 复合索引 ORDER BY department, salary DESC
 * 心智模型：复合索引本质上也是排序 RowID，只是 Comparator 里组合了多个列。
 */
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

struct Row {
    int id;
    std::string name;
    std::string department;
    double salary;
};

int main()
{
    std::vector<Row> table = {
        {1, "Alice", "Eng", 8600}, {2, "Bob", "Sales", 5200}, {3, "Carol", "Eng", 12000},
        {4, "Dave", "Sales", 7300}, {5, "Eve", "Eng", 9600}, {6, "Frank", "Sales", 6100},
        {7, "Grace", "HR", 7000}, {8, "Heidi", "HR", 6800},
    };

    std::vector<int> rowids(table.size());
    for (size_t i = 0; i < table.size(); i++)
        rowids[i] = static_cast<int>(i);

    std::cout << "=== 数据库记录：复合索引 ORDER BY department, salary DESC ===\n";

    // 复合 Comparator：先按 department 升序，再按 salary 降序
    std::sort(rowids.begin(), rowids.end(), [&](int a, int b) {
        const Row &ra = table[a];
        const Row &rb = table[b];
        if (ra.department != rb.department)
            return ra.department < rb.department;
        return ra.salary > rb.salary;
    });

    for (int rowid : rowids) {
        const Row &r = table[rowid];
        std::cout << "  RowID=" << rowid << " name=" << r.name << " dept=" << r.department
                   << " salary=" << r.salary << "\n";
    }

    std::cout << "\n说明：Comparator 内部先比较 department，再比较 salary，\n"
                 "真正被排序的仍然只是 rowids 里的整数，这就是数据库\n"
                 "'复合索引'的核心思路：多列组合成一个排序规则，作用在行标识上。\n";
    return 0;
}
