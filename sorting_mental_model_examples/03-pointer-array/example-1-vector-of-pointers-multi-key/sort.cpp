/*
 * 场景：指针数组 —— 用不同 Comparator 反复对同一批指针重新排序
 * 心智模型：数据只有一份，指针数组可以按需切换排序规则，
 *          而不需要为每一种排序方式复制一份数据。
 */
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

struct Employee {
    int id;
    std::string name;
    std::string department;
    double salary;
};

static void print_view(const std::string &label, const std::vector<Employee *> &ptrs)
{
    std::cout << label << ":\n";
    for (const auto *e : ptrs) {
        std::cout << "  id=" << e->id << " name=" << e->name << " dept=" << e->department
                   << " salary=" << e->salary << "\n";
    }
}

int main()
{
    std::vector<Employee> pool = {
        {1, "Alice", "Eng", 8600}, {2, "Bob", "Sales", 5200},
        {3, "Carol", "Eng", 12000}, {4, "Dave", "Sales", 7300},
        {5, "Eve", "Eng", 9600}, {6, "Frank", "Sales", 6100},
        {7, "Grace", "HR", 7000}, {8, "Heidi", "HR", 6800},
    };

    std::vector<Employee *> ptrs;
    for (auto &e : pool)
        ptrs.push_back(&e);

    std::cout << "=== 指针数组：同一批指针，按不同 Comparator 反复排序 ===\n";

    std::sort(ptrs.begin(), ptrs.end(),
              [](const Employee *a, const Employee *b) { return a->name < b->name; });
    print_view("按 name 排序", ptrs);

    std::sort(ptrs.begin(), ptrs.end(),
              [](const Employee *a, const Employee *b) { return a->salary > b->salary; });
    print_view("按 salary 从高到低排序", ptrs);

    std::cout << "\n说明：pool 里的 Employee 对象没有被复制或修改过，\n"
                 "ptrs 每次只是重新排列 8 字节的指针，切换 Comparator 即可\n"
                 "得到完全不同的排序视图，代价远低于交换整个 Employee。\n";
    return 0;
}
