/*
 * 场景：外部排序 —— 数据量超过内存限制时，分块排序后落盘临时文件
 * 心智模型：内存排序解决不了的问题，就把排序过程分阶段完成。
 */
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

constexpr int kTotal = 40;
constexpr int kChunkSize = 8; // 模拟"内存一次只能装下 8 个数"

int main()
{
    std::vector<int> data(kTotal);
    unsigned int seed = 20240101;
    for (int i = 0; i < kTotal; i++) {
        seed = seed * 1103515245u + 12345u;
        data[i] = static_cast<int>((seed >> 16) % 1000);
    }

    std::cout << "=== 外部排序：分块排序 + 落盘临时文件 ===\n";
    std::cout << "总数据量 = " << kTotal << "，模拟内存一次只能容纳 " << kChunkSize << " 个元素\n";

    int chunk_count = (kTotal + kChunkSize - 1) / kChunkSize;
    std::vector<std::string> filenames(chunk_count);

    for (int c = 0; c < chunk_count; c++) {
        int start = c * kChunkSize;
        int len = std::min(kChunkSize, kTotal - start);

        std::vector<int> chunk(data.begin() + start, data.begin() + start + len);
        std::sort(chunk.begin(), chunk.end()); // 块内排序：内存完全放得下

        filenames[c] = "chunk_cpp_" + std::to_string(c) + ".txt";
        std::ofstream out(filenames[c]);
        for (int v : chunk)
            out << v << "\n";

        std::cout << "第 " << c << " 块 (" << len << " 个元素) 已排序并写入磁盘文件 "
                  << filenames[c] << "\n";
    }

    // 验证每个文件内部确实有序，然后清理临时文件
    bool all_sorted = true;
    for (const auto &filename : filenames) {
        std::ifstream in(filename);
        int prev = 0, cur;
        bool has_prev = false;
        while (in >> cur) {
            if (has_prev && prev > cur)
                all_sorted = false;
            prev = cur;
            has_prev = true;
        }
        in.close();
        std::remove(filename.c_str());
    }

    std::cout << "\n所有分块文件内部有序: " << (all_sorted ? "是" : "否") << "（临时文件已清理）\n";
    std::cout << "\n说明：每一块都在内存里完成排序，然后立刻写入磁盘释放内存。\n"
                 "接下来用 K 路归并（参见 example-2）即可把这些有序小文件合并成\n"
                 "一个整体有序结果，这就是 External Merge Sort 的第一阶段。\n";
    return 0;
}
