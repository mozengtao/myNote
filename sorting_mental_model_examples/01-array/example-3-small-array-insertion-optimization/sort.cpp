/*
 * 场景：大数组 + 小子数组优化（Quick Sort 混合 Insertion Sort，简化版 Introsort）
 * 心智模型：递归到小规模子数组时，Insertion Sort 常数因子更低，
 *          这是 std::sort 内部真实采用的优化策略之一。
 */
#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

constexpr int kInsertionThreshold = 16;

static void insertion_sort(std::vector<int> &arr, int low, int high)
{
    for (int i = low + 1; i <= high; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= low && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

static int partition(std::vector<int> &arr, int low, int high)
{
    int pivot = arr[high];
    int i = low - 1;
    for (int j = low; j < high; j++) {
        if (arr[j] < pivot) {
            i++;
            std::swap(arr[i], arr[j]);
        }
    }
    std::swap(arr[i + 1], arr[high]);
    return i + 1;
}

static void hybrid_sort(std::vector<int> &arr, int low, int high)
{
    while (low < high) {
        if (high - low + 1 <= kInsertionThreshold) {
            insertion_sort(arr, low, high);
            return;
        }
        int p = partition(arr, low, high);
        hybrid_sort(arr, low, p - 1);
        low = p + 1;
    }
}

int main()
{
    const int n = 40;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 999);

    std::vector<int> arr(n);
    for (int i = 0; i < n; i++)
        arr[i] = dist(rng);

    std::vector<int> reference = arr;
    std::sort(reference.begin(), reference.end());

    std::cout << "=== 小数组优化：Quick Sort + Insertion Sort 混合策略 ===\n";
    std::cout << "数组长度=" << n << ", Insertion 阈值=" << kInsertionThreshold << "\n";

    hybrid_sort(arr, 0, n - 1);

    bool ok = (arr == reference);

    std::cout << "排序后前 10 个元素: ";
    for (int i = 0; i < 10; i++)
        std::cout << arr[i] << " ";
    std::cout << "\n结果与 std::sort 对比: " << (ok ? "一致（正确）" : "不一致（错误）") << "\n";
    std::cout << "\n说明：子数组长度 <= " << kInsertionThreshold
              << " 时切换 Insertion Sort，这正是 std::sort/Introsort 内部的真实优化手段。\n";
    return 0;
}
