/*
 * 场景：外部排序 —— 用最小堆做 K 路归并
 * 心智模型：多个已经有序的小文件，可以通过维护一个"当前最小候选"的堆，
 *          每次只需要 O(log k) 就能确定下一个应该输出的元素。
 */
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

constexpr int kK = 4;
constexpr int kChunkLen = 6;

struct HeapItem {
    int value;
    int file_index;
};

struct CompareByValue {
    bool operator()(const HeapItem &a, const HeapItem &b) const { return a.value > b.value; }
};

int main()
{
    std::vector<std::vector<int>> chunks = {
        {3, 9, 15, 22, 40, 58},
        {1, 6, 18, 25, 33, 47},
        {2, 11, 12, 30, 44, 60},
        {5, 8, 20, 27, 35, 50},
    };
    std::vector<std::string> filenames(kK);

    std::cout << "=== 外部排序：K 路归并（最小堆） ===\n";

    for (int i = 0; i < kK; i++) {
        filenames[i] = "sorted_cpp_" + std::to_string(i) + ".txt";
        std::ofstream out(filenames[i]);
        for (int v : chunks[i])
            out << v << "\n";
        std::cout << "已生成有序小文件 " << filenames[i] << "\n";
    }

    std::vector<std::ifstream> files(kK);
    std::priority_queue<HeapItem, std::vector<HeapItem>, CompareByValue> pq;
    for (int i = 0; i < kK; i++) {
        files[i].open(filenames[i]);
        int v;
        if (files[i] >> v)
            pq.push({v, i});
    }

    std::vector<int> merged;
    merged.reserve(kK * kChunkLen);
    while (!pq.empty()) {
        HeapItem top = pq.top();
        pq.pop();
        merged.push_back(top.value);

        int v;
        if (files[top.file_index] >> v)
            pq.push({v, top.file_index});
    }

    for (int i = 0; i < kK; i++) {
        files[i].close();
        std::remove(filenames[i].c_str());
    }

    std::cout << "\n归并结果 (" << merged.size() << " 个元素): ";
    for (int v : merged)
        std::cout << v << " ";
    std::cout << "\n";

    bool ok = std::is_sorted(merged.begin(), merged.end());
    std::cout << "归并结果是否整体有序: " << (ok ? "是" : "否") << "\n";
    std::cout << "\n说明：整个过程堆里最多只保留 " << kK << " 个候选值，每次弹出堆顶就能确定\n"
                 "全局下一个最小值，不需要把 "
              << kK << " 个文件的全部内容都读入内存再排序。\n";
    return 0;
}
