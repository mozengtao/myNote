/*
 * 场景：堆 —— 手写堆排序，并与标准库 qsort 的结果做正确性对照
 * 心智模型：堆排序本身也是一种"整体排序"的用法，
 *          但它的核心价值仍然是"每次都能 O(log n) 拿到当前最大值"。
 */
#include <stdio.h>
#include <stdlib.h>

static void swap_int(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

static void sift_down(int arr[], int n, int i)
{
    while (1) {
        int left = 2 * i + 1, right = 2 * i + 2, largest = i;
        if (left < n && arr[left] > arr[largest]) largest = left;
        if (right < n && arr[right] > arr[largest]) largest = right;
        if (largest == i)
            break;
        swap_int(&arr[i], &arr[largest]);
        i = largest;
    }
}

static void heap_sort(int arr[], int n)
{
    for (int i = n / 2 - 1; i >= 0; i--)
        sift_down(arr, n, i);

    for (int end = n - 1; end > 0; end--) {
        swap_int(&arr[0], &arr[end]);
        sift_down(arr, end, 0);
    }
}

static int cmp_int(const void *a, const void *b)
{
    return (*(const int *)a) - (*(const int *)b);
}

int main(void)
{
    int arr[] = {23, 5, 67, 12, 89, 3, 45, 78, 1, 34};
    int n = sizeof(arr) / sizeof(arr[0]);

    int reference[10];
    for (int i = 0; i < n; i++)
        reference[i] = arr[i];
    qsort(reference, n, sizeof(int), cmp_int);

    printf("=== 堆排序：手写 Heap Sort vs 标准库 qsort ===\n");
    printf("排序前: ");
    for (int i = 0; i < n; i++)
        printf("%d ", arr[i]);
    printf("\n");

    heap_sort(arr, n);

    printf("Heap Sort 结果: ");
    for (int i = 0; i < n; i++)
        printf("%d ", arr[i]);
    printf("\n");

    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (arr[i] != reference[i]) {
            ok = 0;
            break;
        }
    }

    printf("与 qsort 结果对比: %s\n", ok ? "一致（正确）" : "不一致（错误）");
    printf("\n说明：C 标准库没有内置堆结构，工程上如果只是要\"排序\"，\n"
           "直接用 qsort（Introsort/快排类）通常比手写堆排序更快；\n"
           "堆真正的价值在于\"动态维护最值\"，而不是一次性整体排序。\n");
    return 0;
}
