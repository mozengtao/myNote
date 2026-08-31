/*
 * 场景：堆 —— 用小顶堆维护数据流中最大的 K 个元素（Top-K）
 * 心智模型：堆不是为了得到完整有序序列，而是为了快速获取当前最值；
 *          这里进一步利用它做"只维护 Top-K，不必排序全部数据"。
 */
#include <stdio.h>

#define K 4

static int heap[K];
static int heap_size = 0;

static void swap_int(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

/* 小顶堆：堆顶始终是当前 Top-K 中最小的一个，方便判断新元素是否能进入 */
static void sift_up(int i)
{
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap[parent] <= heap[i])
            break;
        swap_int(&heap[parent], &heap[i]);
        i = parent;
    }
}

static void sift_down(int i)
{
    while (1) {
        int left = 2 * i + 1, right = 2 * i + 2, smallest = i;
        if (left < heap_size && heap[left] < heap[smallest]) smallest = left;
        if (right < heap_size && heap[right] < heap[smallest]) smallest = right;
        if (smallest == i)
            break;
        swap_int(&heap[i], &heap[smallest]);
        i = smallest;
    }
}

static void offer(int val)
{
    if (heap_size < K) {
        heap[heap_size] = val;
        sift_up(heap_size);
        heap_size++;
    } else if (val > heap[0]) {
        heap[0] = val;
        sift_down(0);
    }
    /* val <= heap[0] 时直接丢弃：不需要进入 Top-K，也不需要排序整条流 */
}

int main(void)
{
    int stream[] = {5, 81, 3, 42, 97, 12, 6, 63, 29, 74, 8, 55};
    int n = sizeof(stream) / sizeof(stream[0]);

    printf("=== 堆：只用小顶堆维护 Top-%d，不排序整条数据流 ===\n", K);
    printf("数据流(共 %d 个): ", n);
    for (int i = 0; i < n; i++)
        printf("%d ", stream[i]);
    printf("\n");

    for (int i = 0; i < n; i++)
        offer(stream[i]);

    /* 简单地对堆内 K 个元素排个序展示结果（K 很小，代价可忽略） */
    for (int i = 0; i < heap_size; i++)
        for (int j = i + 1; j < heap_size; j++)
            if (heap[i] > heap[j])
                swap_int(&heap[i], &heap[j]);

    printf("Top-%d 最大值: ", K);
    for (int i = 0; i < heap_size; i++)
        printf("%d ", heap[i]);
    printf("\n\n说明：整条数据流只需要遍历一次，堆里始终只保留 %d 个候选元素，\n"
           "既不需要把全部数据存下来，也不需要对全部数据排序。\n",
           K);
    return 0;
}
