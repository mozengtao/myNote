/*
 * 场景：堆 —— 用小顶堆维护数据流中最大的 K 个元素（Top-K）
 * 心智模型：堆不是为了得到完整有序序列，而是为了快速获取当前最值。
 */
#include <algorithm>
#include <iostream>
#include <queue>
#include <vector>

constexpr int kK = 4;

int main()
{
    std::vector<int> stream = {5, 81, 3, 42, 97, 12, 6, 63, 29, 74, 8, 55};

    std::cout << "=== 堆：只用小顶堆维护 Top-" << kK << "，不排序整条数据流 ===\n";
    std::cout << "数据流(共 " << stream.size() << " 个): ";
    for (int v : stream)
        std::cout << v << " ";
    std::cout << "\n";

    // std::priority_queue 默认是大顶堆，传入 greater<int> 变成小顶堆
    std::priority_queue<int, std::vector<int>, std::greater<int>> min_heap;
    for (int v : stream) {
        if (static_cast<int>(min_heap.size()) < kK) {
            min_heap.push(v);
        } else if (v > min_heap.top()) {
            min_heap.pop();
            min_heap.push(v);
        }
    }

    std::vector<int> result;
    while (!min_heap.empty()) {
        result.push_back(min_heap.top());
        min_heap.pop();
    }
    std::sort(result.begin(), result.end());

    std::cout << "Top-" << kK << " 最大值: ";
    for (int v : result)
        std::cout << v << " ";
    std::cout << "\n\n说明：整条数据流只需要遍历一次，堆里始终只保留 " << kK << " 个候选元素，\n"
                 "既不需要把全部数据存下来，也不需要对全部数据排序。\n";
    return 0;
}
