/*
 * 场景：大对象数组 —— 排序指针数组，而不是排序对象本身
 * 心智模型：对象保持不动，排序的是访问路径（指针）。
 */
#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

constexpr int kPayloadSize = 500;

struct Record {
    int id;
    double score;
    char payload[kPayloadSize];
};

int main()
{
    const int n = 2000;
    std::vector<Record> records(n); // 原始大对象数组，整个过程中不会被移动
    unsigned int seed = 12345;

    for (int i = 0; i < n; i++) {
        seed = seed * 1103515245u + 12345u;
        records[i].id = i;
        records[i].score = static_cast<double>((seed >> 16) % 100000) / 100.0;
        std::memset(records[i].payload, 'A' + (i % 26), kPayloadSize);
    }

    std::vector<Record *> index(n);
    for (int i = 0; i < n; i++)
        index[i] = &records[i]; // 索引初始指向原始位置

    const Record *original_address = &records[0];

    std::cout << "=== 大对象数组：只排序 std::vector<Record*> index ===\n";
    std::cout << "sizeof(Record) = " << sizeof(Record)
              << " 字节, sizeof(Record*) = " << sizeof(Record *) << " 字节, 记录数 = " << n << "\n";

    // Comparator 解引用指针比较 score，std::sort 只交换指针本身
    std::sort(index.begin(), index.end(),
              [](const Record *a, const Record *b) { return a->score < b->score; });

    bool ok = true;
    for (int i = 1; i < n; i++) {
        if (index[i - 1]->score > index[i]->score) {
            ok = false;
            break;
        }
    }

    std::cout << "通过索引访问得到的有序视图: " << (ok ? "有序（正确）" : "无序（错误）") << "\n";
    std::cout << "原始数组是否原地未动: "
              << (original_address == &records[0] ? "是（未移动任何 Record）" : "否") << "\n";
    std::cout << "\n说明：records 数组自始至终没有任何一个 Record 被移动，\n"
                 "真正被排序的是 index 里的指针，std::sort 内部只交换 8 字节的指针值。\n";
    return 0;
}
