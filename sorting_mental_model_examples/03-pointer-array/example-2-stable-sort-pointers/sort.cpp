/*
 * 场景：指针数组 —— 稳定排序（保持相同 Key 的相对顺序）
 * 心智模型：C++ 标准库直接提供 std::stable_sort，
 *          排序的是指针，但相同 Key 记录的原有相对顺序会被保留。
 */
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

struct Employee {
    int id;
    std::string name;
    std::string department;
};

static void print_view(const std::string &label, const std::vector<Employee *> &ptrs)
{
    std::cout << label << ":\n";
    for (const auto *e : ptrs)
        std::cout << "  id=" << e->id << " name=" << e->name << " dept=" << e->department << "\n";
}

int main()
{
    std::vector<Employee> pool = {
        {1, "Alice", "Eng"}, {2, "Bob", "Sales"}, {3, "Carol", "Eng"},
        {4, "Dave", "Sales"}, {5, "Eve", "Eng"}, {6, "Frank", "Sales"},
        {7, "Grace", "HR"}, {8, "Heidi", "HR"},
    };

    std::vector<Employee *> ptrs;
    for (auto &e : pool)
        ptrs.push_back(&e);

    std::cout << "=== 指针数组：std::stable_sort 保持相同 department 的相对顺序 ===\n";
    print_view("排序前（按 id 插入顺序）", ptrs);

    std::stable_sort(ptrs.begin(), ptrs.end(), [](const Employee *a, const Employee *b) {
        return a->department < b->department;
    });

    print_view("稳定排序后（按 department）", ptrs);
    std::cout << "\n说明：Eng 组内部仍然是 Alice(1) -> Carol(3) -> Eve(5) 的原始相对顺序，\n"
                 "Sales 组内部仍然是 Bob(2) -> Dave(4) -> Frank(6)。\n"
                 "std::sort 不保证稳定，需要稳定性时应改用 std::stable_sort。\n";
    return 0;
}
