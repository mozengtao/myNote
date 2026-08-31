/*
 * 场景：大对象数组直接交换（反面示例）
 * 心智模型：Record 很大时，直接交换整个对象搬移开销巨大。
 *          本示例统计交换次数 x sizeof(Record)，量化这个代价。
 */
#include <cstring>
#include <iostream>
#include <vector>

constexpr int kPayloadSize = 500;

struct Record {
    int id;
    double score;
    char payload[kPayloadSize]; // 模拟大对象：例如日志记录、图像帧等大块数据
};

static long g_bytes_moved = 0;

static void swap_record(Record &a, Record &b)
{
    Record tmp = a;
    a = b;
    b = tmp;
    g_bytes_moved += 3L * static_cast<long>(sizeof(Record)); // 一次交换相当于 3 次整体拷贝
}

static int partition(std::vector<Record> &arr, int low, int high)
{
    Record pivot = arr[high];
    int i = low - 1;
    for (int j = low; j < high; j++) {
        if (arr[j].score < pivot.score) {
            i++;
            swap_record(arr[i], arr[j]);
        }
    }
    swap_record(arr[i + 1], arr[high]);
    return i + 1;
}

static void quicksort(std::vector<Record> &arr, int low, int high)
{
    if (low < high) {
        int p = partition(arr, low, high);
        quicksort(arr, low, p - 1);
        quicksort(arr, p + 1, high);
    }
}

int main()
{
    const int n = 2000;
    std::vector<Record> records(n);
    unsigned int seed = 12345;

    for (int i = 0; i < n; i++) {
        seed = seed * 1103515245u + 12345u;
        records[i].id = i;
        records[i].score = static_cast<double>((seed >> 16) % 100000) / 100.0;
        std::memset(records[i].payload, 'A' + (i % 26), kPayloadSize);
    }

    std::cout << "=== 大对象数组：直接交换整个 Record（反面示例） ===\n";
    std::cout << "sizeof(Record) = " << sizeof(Record) << " 字节, 记录数 = " << n << "\n";

    quicksort(records, 0, n - 1);

    bool ok = true;
    for (int i = 1; i < n; i++) {
        if (records[i - 1].score > records[i].score) {
            ok = false;
            break;
        }
    }

    std::cout << "排序结果: " << (ok ? "有序（正确）" : "无序（错误）") << "\n";
    std::cout << "累计搬移数据量 约 " << (g_bytes_moved / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "\n说明：每次交换都要整体拷贝 sizeof(Record)=" << sizeof(Record) << " 字节，\n"
                 "记录数一旦上升到百万级，直接交换的搬移总量会膨胀到几百 MB 甚至更多。\n"
                 "参见 example-2-pointer-index-sort，只排序指针可以把搬移量降低两个数量级。\n";
    return 0;
}
