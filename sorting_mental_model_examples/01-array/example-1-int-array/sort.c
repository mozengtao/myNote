/*
 * 场景：连续整型数组排序
 * 心智模型：连续内存 + 小对象 -> 直接排序，Move Strategy 就是"交换值本身"
 *
 * 本示例手写一个快速排序，用来展示 Comparator（比较逻辑）
 * 与 Move Strategy（交换数组元素）是如何配合工作的。
 * 工程实践中直接使用标准库的 qsort() 即可，此处手写是为了教学目的。
 */
#include <stdio.h>

/* Comparator：只负责回答"谁大谁小"，不关心如何移动数据 */
static int less_than(int a, int b)
{
    return a < b;
}

/* Move Strategy：直接交换两个整数（连续数组场景下最简单的移动方式） */
static void swap_int(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

static int partition(int arr[], int low, int high)
{
    int pivot = arr[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (less_than(arr[j], pivot)) {
            i++;
            swap_int(&arr[i], &arr[j]);
        }
    }
    swap_int(&arr[i + 1], &arr[high]);
    return i + 1;
}

static void quicksort(int arr[], int low, int high)
{
    if (low < high) {
        int p = partition(arr, low, high);
        quicksort(arr, low, p - 1);
        quicksort(arr, p + 1, high);
    }
}

static void print_array(const char *label, int arr[], int n)
{
    printf("%s: ", label);
    for (int i = 0; i < n; i++)
        printf("%d ", arr[i]);
    printf("\n");
}

int main(void)
{
    int scores[] = {88, 45, 67, 12, 99, 34, 76, 5, 58, 23};
    int n = sizeof(scores) / sizeof(scores[0]);

    printf("=== 连续数组排序：直接交换整数 ===\n");
    print_array("排序前", scores, n);

    quicksort(scores, 0, n - 1);

    print_array("排序后", scores, n);
    printf("\n说明：数组连续存储，随机访问 O(1)，Cache 命中率高，\n"
           "因此直接对元素做原地交换是成本最低的移动策略。\n"
           "工程实践中应直接使用标准库 qsort()（内部通常是 Introsort）。\n");
    return 0;
}
