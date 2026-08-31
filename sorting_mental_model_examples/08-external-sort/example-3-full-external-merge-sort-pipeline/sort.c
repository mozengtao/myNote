/*
 * 场景：外部排序 —— 完整流程：生成大数据 -> 分块排序 -> 落盘 -> K 路归并 -> 校验
 * 心智模型：内存排序解决不了的问题，就把排序过程分阶段完成——
 *          这里把 example-1（分块落盘）和 example-2（K 路归并）串联成完整流程。
 */
#include <stdio.h>
#include <stdlib.h>

#define TOTAL 100
#define MEM_LIMIT 10 /* 模拟内存一次只能容纳 10 个元素 */
#define MAX_CHUNKS ((TOTAL + MEM_LIMIT - 1) / MEM_LIMIT)

typedef struct {
    int value;
    int chunk_index;
} HeapItem;

static HeapItem heap[MAX_CHUNKS];
static int heap_size = 0;

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

static int cmp_int(const void *a, const void *b)
{
    return (*(const int *)a) - (*(const int *)b);
}

static void chunk_filename(char *buf, size_t size, int idx)
{
    snprintf(buf, size, "es_chunk_%d.txt", idx);
}

int main(void)
{
    int data[TOTAL];
    unsigned int seed = 987654321u;
    for (int i = 0; i < TOTAL; i++) {
        seed = seed * 1103515245u + 12345u;
        data[i] = (int)((seed >> 16) % 10000);
    }

    int chunk_count = (TOTAL + MEM_LIMIT - 1) / MEM_LIMIT;

    printf("=== 外部排序完整流程：生成 -> 分块排序 -> 落盘 -> K 路归并 -> 校验 ===\n");
    printf("总数据量 = %d，模拟内存一次只能容纳 %d 个元素，共分 %d 块\n\n",
           TOTAL, MEM_LIMIT, chunk_count);

    /* 阶段一：分块排序 + 落盘 */
    char filenames[MAX_CHUNKS][64];
    for (int c = 0; c < chunk_count; c++) {
        int start = c * MEM_LIMIT;
        int len = (start + MEM_LIMIT <= TOTAL) ? MEM_LIMIT : (TOTAL - start);

        int chunk[MEM_LIMIT];
        for (int i = 0; i < len; i++)
            chunk[i] = data[start + i];
        qsort(chunk, len, sizeof(int), cmp_int);

        chunk_filename(filenames[c], sizeof(filenames[c]), c);
        FILE *fp = fopen(filenames[c], "w");
        for (int i = 0; i < len; i++)
            fprintf(fp, "%d\n", chunk[i]);
        fclose(fp);
    }
    printf("阶段一完成：%d 个块均已在内存中排序并写入磁盘\n", chunk_count);

    /* 阶段二：K 路归并 */
    FILE *files[MAX_CHUNKS];
    for (int c = 0; c < chunk_count; c++) {
        files[c] = fopen(filenames[c], "r");
        int v;
        if (fscanf(files[c], "%d", &v) == 1)
            heap_push((HeapItem){v, c});
    }

    int *merged = malloc(sizeof(int) * TOTAL);
    int merged_count = 0;
    while (heap_size > 0) {
        HeapItem top = heap_pop();
        merged[merged_count++] = top.value;
        int v;
        if (fscanf(files[top.chunk_index], "%d", &v) == 1)
            heap_push((HeapItem){v, top.chunk_index});
    }

    for (int c = 0; c < chunk_count; c++) {
        fclose(files[c]);
        remove(filenames[c]);
    }
    printf("阶段二完成：%d 路归并结束，输出 %d 个元素，临时文件已清理\n", chunk_count, merged_count);

    /* 校验：与整体内存排序(qsort)结果做对照 */
    int reference[TOTAL];
    for (int i = 0; i < TOTAL; i++)
        reference[i] = data[i];
    qsort(reference, TOTAL, sizeof(int), cmp_int);

    int ok = (merged_count == TOTAL);
    if (ok) {
        for (int i = 0; i < TOTAL; i++) {
            if (merged[i] != reference[i]) {
                ok = 0;
                break;
            }
        }
    }

    printf("\n与整体内存排序(qsort)结果对比: %s\n", ok ? "完全一致（正确）" : "不一致（错误）");
    printf("\n说明：全程没有一次性把 %d 个元素都放进内存排序，而是拆成 %d 个\n"
           "可以放进内存的小块分别排序，再通过 K 路归并合并成最终结果——\n"
           "这就是数据库、Hadoop、Spark 等系统处理超大数据排序的标准做法。\n",
           TOTAL, chunk_count);

    free(merged);
    return 0;
}
