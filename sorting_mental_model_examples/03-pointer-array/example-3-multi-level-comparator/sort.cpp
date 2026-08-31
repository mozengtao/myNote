/*
 * 场景：指针数组 —— 多级比较器（先按 department，再按 salary 降序）
 * 心智模型：Comparator 可以组合多个 Key，形成"复合排序规则"，
 *          而 Move Strategy（交换指针）完全不受影响。
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

    std::cout << "=== 指针数组：多级 Comparator（先 department，再 salary 降序） ===\n";

    // 用 std::tie 把两个 Key 组合成一个复合比较：先 department 升序，再 salary 降序
    std::sort(ptrs.begin(), ptrs.end(), [](const Employee *a, const Employee *b) {
        if (a->department != b->department)
            return a->department < b->department;
        return a->salary > b->salary;
    });

    print_view("排序结果", ptrs);
    std::cout << "\n说明：Comparator 内部先比较 department，相同部门再比较 salary，\n"
                 "排序算法本身完全不需要修改，Move Strategy 依旧只是交换指针。\n";
    return 0;
}
