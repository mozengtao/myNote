/*
 * 场景：大对象数组直接交换（反面示例）
 * 心智模型：当 Record 很大时，直接交换整个对象会带来巨大的数据搬移开销。
 *          本示例统计交换次数 x sizeof(Record)，直观显示这个代价有多大。
 */
#include <stdio.h>
#include <string.h>

#define PAYLOAD_SIZE 500
#define N 2000

typedef struct {
    int id;
    double score;
    char payload[PAYLOAD_SIZE]; /* 模拟大对象：例如日志记录、图像帧等大块数据 */
} Record;

static long g_bytes_moved = 0;

static int less_than(const Record *a, const Record *b)
{
    return a->score < b->score;
}

/* Move Strategy：直接交换整个大结构体——这是需要避免的反面写法 */
static void swap_record(Record *a, Record *b)
{
    Record tmp = *a;
    *a = *b;
    *b = tmp;
    g_bytes_moved += 3L * (long)sizeof(Record); /* 一次交换相当于 3 次整体拷贝 */
}

static int partition(Record arr[], int low, int high)
{
    Record pivot = arr[high];
    int i = low - 1;
    for (int j = low; j < high; j++) {
        if (less_than(&arr[j], &pivot)) {
            i++;
            swap_record(&arr[i], &arr[j]);
        }
    }
    swap_record(&arr[i + 1], &arr[high]);
    return i + 1;
}

static void quicksort(Record arr[], int low, int high)
{
    if (low < high) {
        int p = partition(arr, low, high);
        quicksort(arr, low, p - 1);
        quicksort(arr, p + 1, high);
    }
}

static Record records[N];

int main(void)
{
    unsigned int seed = 12345;

    for (int i = 0; i < N; i++) {
        seed = seed * 1103515245u + 12345u;
        records[i].id = i;
        records[i].score = (double)((seed >> 16) % 100000) / 100.0;
        memset(records[i].payload, 'A' + (i % 26), PAYLOAD_SIZE);
    }

    printf("=== 大对象数组：直接交换整个 Record（反面示例） ===\n");
    printf("sizeof(Record) = %zu 字节, 记录数 = %d\n", sizeof(Record), N);

    quicksort(records, 0, N - 1);

    int ok = 1;
    for (int i = 1; i < N; i++) {
        if (records[i - 1].score > records[i].score) {
            ok = 0;
            break;
        }
    }

    printf("排序结果: %s\n", ok ? "有序（正确）" : "无序（错误）");
    printf("累计搬移数据量 约 %.2f MB\n", g_bytes_moved / (1024.0 * 1024.0));
    printf("\n说明：每次交换都要整体拷贝 sizeof(Record)=%zu 字节，\n"
           "记录数一旦上升到百万级，直接交换的搬移总量会膨胀到几百 MB 甚至更多。\n"
           "参见 example-2-pointer-index-sort，只排序指针可以把搬移量降低两个数量级。\n",
           sizeof(Record));
    return 0;
}
