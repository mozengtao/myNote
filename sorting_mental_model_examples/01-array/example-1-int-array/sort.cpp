/*
 * 场景：连续整型数组排序（std::vector）
 * 心智模型：连续内存 + 小对象 -> 直接排序，Move Strategy 就是"交换值本身"
 *
 * C++ 工程实践中应直接使用 std::sort，其底层实现是 Introsort
 * （Quick Sort + Heap Sort 兜底 + 小数组切换 Insertion Sort）。
 * 这里演示如何把 Comparator 与容器解耦：排序算法不知道也不关心
 * vector 里存的是什么，只知道调用 comparator 得到大小关系。
 */
#include <algorithm>
#include <iostream>
#include <vector>

static void print_vector(const std::string &label, const std::vector<int> &v)
{
    std::cout << label << ": ";
    for (int x : v)
        std::cout << x << " ";
    std::cout << "\n";
}

int main()
{
    std::vector<int> scores = {88, 45, 67, 12, 99, 34, 76, 5, 58, 23};

    std::cout << "=== 连续数组排序：std::sort + Comparator ===\n";
    print_vector("排序前", scores);

    // Comparator 独立于 Move Strategy：std::sort 内部决定如何移动元素，
    // 这里只提供"谁排前面"的判断逻辑。
    std::sort(scores.begin(), scores.end(),
              [](int a, int b) { return a < b; });

    print_vector("排序后", scores);
    std::cout << "\n说明：std::sort 对连续内存的 vector<int> 默认使用 Introsort，\n"
                 "元素本身很小，直接交换值即可，无需引入指针间接层。\n";
    return 0;
}
