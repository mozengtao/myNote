/*
 * 场景：外部排序 —— 用最小堆做 K 路归并
 * 心智模型：多个已经有序的小文件，可以通过维护一个"当前最小候选"的堆，
 *          每次只需要 O(log k) 就能确定下一个应该输出的元素。
 */
#include <stdio.h>
#include <stdlib.h>

#define K 4
#define CHUNK_LEN 6

typedef struct {
    int value;
    int file_index;
} HeapItem;

static HeapItem heap[K];
static int heap_size = 0;
static FILE *files[K];

static void swap_item(HeapItem *a, HeapItem *b)
{
    HeapItem t = *a;
    *a = *b;
    *b = t;
}

static void sift_up(int i)
{
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap[parent].value <= heap[i].value)
            break;
        swap_item(&heap[parent], &heap[i]);
        i = parent;
    }
}

static void sift_down(int i)
{
    while (1) {
        int left = 2 * i + 1, right = 2 * i + 2, smallest = i;
        if (left < heap_size && heap[left].value < heap[smallest].value) smallest = left;
        if (right < heap_size && heap[right].value < heap[smallest].value) smallest = right;
        if (smallest == i)
            break;
        swap_item(&heap[i], &heap[smallest]);
        i = smallest;
    }
}

static void heap_push(HeapItem item)
{
    heap[heap_size] = item;
    sift_up(heap_size);
    heap_size++;
}

static HeapItem heap_pop(void)
{
    HeapItem top = heap[0];
    heap_size--;
    heap[0] = heap[heap_size];
    sift_down(0);
    return top;
}

int main(void)
{
    int chunks[K][CHUNK_LEN] = {
        {3, 9, 15, 22, 40, 58},
        {1, 6, 18, 25, 33, 47},
        {2, 11, 12, 30, 44, 60},
        {5, 8, 20, 27, 35, 50},
    };
    char filenames[K][32];

    printf("=== 外部排序：K 路归并（最小堆） ===\n");

    for (int i = 0; i < K; i++) {
        snprintf(filenames[i], sizeof(filenames[i]), "sorted_%d.txt", i);
        FILE *fp = fopen(filenames[i], "w");
        for (int j = 0; j < CHUNK_LEN; j++)
            fprintf(fp, "%d\n", chunks[i][j]);
        fclose(fp);
        printf("已生成有序小文件 %s\n", filenames[i]);
    }

    for (int i = 0; i < K; i++)
        files[i] = fopen(filenames[i], "r");

    /* 初始化：每个文件的第一个数字入堆 */
    for (int i = 0; i < K; i++) {
        int v;
        if (fscanf(files[i], "%d", &v) == 1)
            heap_push((HeapItem){v, i});
    }

    int merged[K * CHUNK_LEN];
    int merged_count = 0;
    while (heap_size > 0) {
        HeapItem top = heap_pop();
        merged[merged_count++] = top.value;

        int v;
        if (fscanf(files[top.file_index], "%d", &v) == 1)
            heap_push((HeapItem){v, top.file_index});
    }

    for (int i = 0; i < K; i++) {
        fclose(files[i]);
        remove(filenames[i]);
    }

    printf("\n归并结果 (%d 个元素): ", merged_count);
    for (int i = 0; i < merged_count; i++)
        printf("%d ", merged[i]);
    printf("\n");

    int ok = 1;
    for (int i = 1; i < merged_count; i++) {
        if (merged[i - 1] > merged[i]) {
            ok = 0;
            break;
        }
    }

    printf("归并结果是否整体有序: %s\n", ok ? "是" : "否");
    printf("\n说明：整个过程堆里最多只保留 %d 个候选值，每次弹出堆顶就能确定\n"
           "全局下一个最小值，不需要把 %d 个文件的全部内容都读入内存再排序。\n",
           K, K);
    return 0;
}
