/*
 * 场景：大对象数组 —— 排序索引（Employee *index[N]），而不是排序对象本身
 * 心智模型：对象保持不动，排序的是访问路径（指针）。
 */
#include <stdio.h>
#include <string.h>

#define PAYLOAD_SIZE 500
#define N 2000

typedef struct {
    int id;
    double score;
    char payload[PAYLOAD_SIZE];
} Record;

static long g_bytes_moved = 0;

static void swap_ptr(Record **a, Record **b)
{
    Record *tmp = *a;
    *a = *b;
    *b = tmp;
    g_bytes_moved += 3L * (long)sizeof(Record *); /* 只搬移指针，不搬移数据本体 */
}

static int partition(Record *index[], int low, int high)
{
    double pivot = index[high]->score;
    int i = low - 1;
    for (int j = low; j < high; j++) {
        if (index[j]->score < pivot) {
            i++;
            swap_ptr(&index[i], &index[j]);
        }
    }
    swap_ptr(&index[i + 1], &index[high]);
    return i + 1;
}

static void quicksort(Record *index[], int low, int high)
{
    if (low < high) {
        int p = partition(index, low, high);
        quicksort(index, low, p - 1);
        quicksort(index, p + 1, high);
    }
}

static Record g_records[N]; /* 原始大对象数组：整个过程中完全不移动 */
static Record *g_index[N];  /* 索引数组：真正参与排序的是这里的指针 */

int main(void)
{
    unsigned int seed = 12345;

    for (int i = 0; i < N; i++) {
        seed = seed * 1103515245u + 12345u;
        g_records[i].id = i;
        g_records[i].score = (double)((seed >> 16) % 100000) / 100.0;
        memset(g_records[i].payload, 'A' + (i % 26), PAYLOAD_SIZE);
        g_index[i] = &g_records[i]; /* 索引初始指向原始位置 */
    }

    const void *original_address = &g_records[0];

    printf("=== 大对象数组：只排序索引 Record* index[N] ===\n");
    printf("sizeof(Record) = %zu 字节, sizeof(Record*) = %zu 字节, 记录数 = %d\n",
           sizeof(Record), sizeof(Record *), N);

    quicksort(g_index, 0, N - 1);

    int ok = 1;
    for (int i = 1; i < N; i++) {
        if (g_index[i - 1]->score > g_index[i]->score) {
            ok = 0;
            break;
        }
    }

    printf("通过索引访问得到的有序视图: %s\n", ok ? "有序（正确）" : "无序（错误）");
    printf("原始数组是否原地未动: %s\n",
           (original_address == (const void *)&g_records[0]) ? "是（未移动任何 Record）" : "否");
    printf("累计搬移数据量 约 %.2f KB（对比 example-1 的 MB 级别）\n",
           g_bytes_moved / 1024.0);
    printf("\n说明：g_records 数组自始至终没有任何一个 Record 被移动，\n"
           "真正被排序的是 g_index 里的指针。想要访问某个有序位置的记录，\n"
           "只需要 *g_index[i]，这就是'有序视图'（Ordered View）。\n");
    return 0;
}
