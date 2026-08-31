/*
 * 场景：外部排序 —— 完整流程：生成大数据 -> 分块排序 -> 落盘 -> K 路归并 -> 校验
 * 心智模型：内存排序解决不了的问题，就把排序过程分阶段完成——
 *          这里把 example-1（分块落盘）和 example-2（K 路归并）串联成完整流程。
 */
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

constexpr int kTotal = 100;
constexpr int kMemLimit = 10; // 模拟内存一次只能容纳 10 个元素

struct HeapItem {
    int value;
    int chunk_index;
};

struct CompareByValue {
    bool operator()(const HeapItem &a, const HeapItem &b) const { return a.value > b.value; }
};

int main()
{
    std::vector<int> data(kTotal);
    unsigned int seed = 987654321u;
    for (int i = 0; i < kTotal; i++) {
        seed = seed * 1103515245u + 12345u;
        data[i] = static_cast<int>((seed >> 16) % 10000);
    }

    int chunk_count = (kTotal + kMemLimit - 1) / kMemLimit;

    std::cout << "=== 外部排序完整流程：生成 -> 分块排序 -> 落盘 -> K 路归并 -> 校验 ===\n";
    std::cout << "总数据量 = " << kTotal << "，模拟内存一次只能容纳 " << kMemLimit
              << " 个元素，共分 " << chunk_count << " 块\n\n";

    // 阶段一：分块排序 + 落盘
    std::vector<std::string> filenames(chunk_count);
    for (int c = 0; c < chunk_count; c++) {
        int start = c * kMemLimit;
        int len = std::min(kMemLimit, kTotal - start);

        std::vector<int> chunk(data.begin() + start, data.begin() + start + len);
        std::sort(chunk.begin(), chunk.end());

        filenames[c] = "es_chunk_cpp_" + std::to_string(c) + ".txt";
        std::ofstream out(filenames[c]);
        for (int v : chunk)
            out << v << "\n";
    }
    std::cout << "阶段一完成：" << chunk_count << " 个块均已在内存中排序并写入磁盘\n";

    // 阶段二：K 路归并
    std::vector<std::ifstream> files(chunk_count);
    std::priority_queue<HeapItem, std::vector<HeapItem>, CompareByValue> pq;
    for (int c = 0; c < chunk_count; c++) {
        files[c].open(filenames[c]);
        int v;
        if (files[c] >> v)
            pq.push({v, c});
    }

    std::vector<int> merged;
    merged.reserve(kTotal);
    while (!pq.empty()) {
        HeapItem top = pq.top();
        pq.pop();
        merged.push_back(top.value);
        int v;
        if (files[top.chunk_index] >> v)
            pq.push({v, top.chunk_index});
    }

    for (int c = 0; c < chunk_count; c++) {
        files[c].close();
        std::remove(filenames[c].c_str());
    }
    std::cout << "阶段二完成：" << chunk_count << " 路归并结束，输出 " << merged.size()
              << " 个元素，临时文件已清理\n";

    // 校验：与整体内存排序(std::sort)结果做对照
    std::vector<int> reference = data;
    std::sort(reference.begin(), reference.end());
    bool ok = (merged == reference);

    std::cout << "\n与整体内存排序(std::sort)结果对比: "
              << (ok ? "完全一致（正确）" : "不一致（错误）") << "\n";
    std::cout << "\n说明：全程没有一次性把 " << kTotal << " 个元素都放进内存排序，而是拆成 "
              << chunk_count
              << " 个可以放进内存的小块分别排序，再通过 K 路归并合并成\n"
                 "最终结果——这就是数据库、Hadoop、Spark 等系统处理超大数据排序的标准做法。\n";
    return 0;
}
