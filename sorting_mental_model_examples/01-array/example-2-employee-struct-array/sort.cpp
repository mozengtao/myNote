/*
 * 场景：小型对象数组（std::vector<Employee>）按薪资排序
 * 心智模型：排序算法只关心 Key（salary），Comparator 与 Record 类型解耦
 */
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

struct Employee {
    int id;
    std::string name;
    double salary;
};

static void print_employees(const std::string &label, const std::vector<Employee> &v)
{
    std::cout << label << ":\n";
    for (const auto &e : v) {
        std::cout << "  id=" << e.id << " name=" << std::setw(8) << std::left << e.name
                  << " salary=" << e.salary << "\n";
    }
}

int main()
{
    std::vector<Employee> employees = {
        {1, "Alice", 8600.0},
        {2, "Bob", 5200.0},
        {3, "Carol", 12000.0},
        {4, "Dave", 7300.0},
        {5, "Eve", 9600.0},
    };

    std::cout << "=== 小对象数组排序：std::sort 按 salary 排序 Employee ===\n";
    print_employees("排序前", employees);

    // Comparator 只提取 salary 做比较，std::sort 负责决定如何移动 Employee 对象
    std::sort(employees.begin(), employees.end(),
              [](const Employee &a, const Employee &b) { return a.salary < b.salary; });

    print_employees("排序后", employees);
    std::cout << "\n说明：vector<Employee> 内存连续，对象不大，std::sort 直接搬移整个对象，\n"
                 "但比较逻辑（Comparator）只关心 salary 这个 Key。\n";
    return 0;
}
