/*
 * 场景：大对象数组 —— 同一份数据的多个"有序视图"
 * 心智模型：数据本身只保存一份且从不移动，可以同时维护多个不同排序
 *          规则的索引数组，得到多个互不冲突的"有序视图"。
 */
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

constexpr int kPayloadSize = 200;

struct Record {
    int id;
    std::string name;
    double score;
    char payload[kPayloadSize];
};

static void print_view(const std::string &label, const std::vector<Record *> &index)
{
    std::cout << label << ":\n";
    for (const auto *r : index)
        std::cout << "  id=" << r->id << " name=" << r->name << " score=" << r->score << "\n";
}

int main()
{
    std::vector<Record> records = {
        {1, "Frank", 72.5, {}}, {2, "Alice", 88.0, {}}, {3, "Eve", 65.3, {}},
        {4, "Carol", 91.2, {}}, {5, "Bob", 77.8, {}}, {6, "Grace", 83.4, {}},
        {7, "Heidi", 59.9, {}}, {8, "Dave", 95.0, {}},
    };

    std::vector<Record *> index_by_score(records.size());
    std::vector<Record *> index_by_name(records.size());
    for (size_t i = 0; i < records.size(); i++) {
        index_by_score[i] = &records[i];
        index_by_name[i] = &records[i];
    }

    const Record *original_first = &records[0];

    std::cout << "=== 大对象数组：同一份数据的两个有序视图 ===\n";

    std::sort(index_by_score.begin(), index_by_score.end(),
              [](const Record *a, const Record *b) { return a->score < b->score; });
    std::sort(index_by_name.begin(), index_by_name.end(),
              [](const Record *a, const Record *b) { return a->name < b->name; });

    print_view("视图一：按 score 排序", index_by_score);
    print_view("视图二：按 name 排序", index_by_name);

    std::cout << "原始数组是否原地未动: " << (original_first == &records[0] ? "是" : "否") << "\n";
    std::cout << "records[0] 仍然是: id=" << records[0].id << " name=" << records[0].name
              << "（未被排序改变）\n";
    std::cout << "\n说明：records 只保存一份数据且从未被重新排列，\n"
                 "index_by_score / index_by_name 是两个独立的'有序视图'，彼此互不干扰。\n";
    return 0;
}
