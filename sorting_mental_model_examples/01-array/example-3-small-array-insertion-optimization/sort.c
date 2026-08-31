/*
 * 场景：大数组 + 小子数组优化（Quick Sort 混合 Insertion Sort）
 * 心智模型：Quick Sort 递归到很小的子数组时，Insertion Sort 常数因子更低、
 *          Cache 局部性更好，因此工程级实现（如 Introsort）都会在子数组
 *          长度低于阈值时切换到 Insertion Sort。
 */
#include <stdio.h>
#include <stdlib.h>

#define INSERTION_THRESHOLD 16

static void swap_int(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

/* 小数组场景下，Insertion Sort 移动次数少、分支预测友好 */
static void insertion_sort(int arr[], int low, int high)
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

static int partition(int arr[], int low, int high)
{
    int pivot = arr[high];
    int i = low - 1;
    for (int j = low; j < high; j++) {
        if (arr[j] < pivot) {
            i++;
            swap_int(&arr[i], &arr[j]);
        }
    }
    swap_int(&arr[i + 1], &arr[high]);
    return i + 1;
}

/* 混合策略：子数组足够大时用 Quick Sort，足够小时切换 Insertion Sort */
static void hybrid_sort(int arr[], int low, int high)
{
    while (low < high) {
        if (high - low + 1 <= INSERTION_THRESHOLD) {
            insertion_sort(arr, low, high);
            return;
        }
        int p = partition(arr, low, high);
        hybrid_sort(arr, low, p - 1);
        low = p + 1; /* 尾递归改循环，减少栈深度 */
    }
}

static int cmp_int(const void *a, const void *b)
{
    return (*(const int *)a) - (*(const int *)b);
}

int main(void)
{
    const int n = 40;
    int arr[40];

    srand(42);
    for (int i = 0; i < n; i++)
        arr[i] = rand() % 1000;

    int reference[40];
    for (int i = 0; i < n; i++)
        reference[i] = arr[i];
    qsort(reference, n, sizeof(int), cmp_int); /* 用标准库结果做正确性对照 */

    printf("=== 小数组优化：Quick Sort + Insertion Sort 混合策略 ===\n");
    printf("数组长度=%d, Insertion 阈值=%d\n", n, INSERTION_THRESHOLD);

    hybrid_sort(arr, 0, n - 1);

    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (arr[i] != reference[i]) {
            ok = 0;
            break;
        }
    }

    printf("排序后前 10 个元素: ");
    for (int i = 0; i < 10; i++)
        printf("%d ", arr[i]);
    printf("\n结果与标准库 qsort 对比: %s\n", ok ? "一致（正确）" : "不一致（错误）");
    printf("\n说明：子数组长度 <= %d 时切换 Insertion Sort，\n"
           "这正是 libstdc++ / glibc Introsort 内部采用的优化手段。\n",
           INSERTION_THRESHOLD);
    return 0;
}
