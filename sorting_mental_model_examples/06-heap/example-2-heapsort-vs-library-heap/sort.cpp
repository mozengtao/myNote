/*
 * 场景：堆 —— 手写堆排序 vs C++ 标准库堆算法（make_heap/sort_heap）
 * 心智模型：堆真正的价值在于"动态维护最值"，而不是一次性整体排序；
 *          std::sort 通常比堆排序更快，这里做正确性和实现方式的对照。
 */
#include <algorithm>
#include <iostream>
#include <vector>

static void sift_down(std::vector<int> &arr, int n, int i)
{
    while (true) {
        int left = 2 * i + 1, right = 2 * i + 2, largest = i;
        if (left < n && arr[left] > arr[largest]) largest = left;
        if (right < n && arr[right] > arr[largest]) largest = right;
        if (largest == i)
            break;
        std::swap(arr[i], arr[largest]);
        i = largest;
    }
}

static void manual_heap_sort(std::vector<int> &arr)
{
    int n = static_cast<int>(arr.size());
    for (int i = n / 2 - 1; i >= 0; i--)
        sift_down(arr, n, i);
    for (int end = n - 1; end > 0; end--) {
        std::swap(arr[0], arr[end]);
        sift_down(arr, end, 0);
    }
}

int main()
{
    std::vector<int> arr = {23, 5, 67, 12, 89, 3, 45, 78, 1, 34};
    std::vector<int> via_library = arr;

    std::cout << "=== 堆排序：手写 Heap Sort vs std::make_heap/sort_heap ===\n";
    std::cout << "排序前: ";
    for (int v : arr)
        std::cout << v << " ";
    std::cout << "\n";

    manual_heap_sort(arr);

    // 标准库提供的"堆排序"两步走：先建堆，再逐个弹出最大值
    std::make_heap(via_library.begin(), via_library.end());
    std::sort_heap(via_library.begin(), via_library.end());

    std::cout << "手写 Heap Sort 结果: ";
    for (int v : arr)
        std::cout << v << " ";
    std::cout << "\n";

    std::cout << "std::make_heap+sort_heap 结果: ";
    for (int v : via_library)
        std::cout << v << " ";
    std::cout << "\n";

    bool ok = (arr == via_library);
    std::cout << "两种结果对比: " << (ok ? "一致（正确）" : "不一致（错误）") << "\n";
    std::cout << "\n说明：std::make_heap + std::sort_heap 就是标准库里的堆排序，\n"
                 "但工程上如果只是要排序，直接用 std::sort（Introsort）通常更快；\n"
                 "堆真正的价值在于配合 std::priority_queue 动态维护最值。\n";
    return 0;
}
