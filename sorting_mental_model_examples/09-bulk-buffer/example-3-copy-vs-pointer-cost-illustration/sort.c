/*
 * 场景：批量缓冲区 —— 直接搬移大缓冲区 vs 只排指针的成本对比
 * 心智模型：批量数据重排时，直接交换整个大缓冲区代价 = 交换次数 x sizeof(Buffer)；
 *          只交换指针代价 = 交换次数 x sizeof(指针)，两者可能相差几个数量级。
 */
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 2048
#define N 200

typedef struct {
    int seq_no;
    char data[BUF_SIZE];
} Buffer;

static Buffer g_buffers_a[N]; /* 用于"直接交换"方案 */
static Buffer g_buffers_b[N]; /* 用于"只排指针"方案（数据内容与 a 相同） */

static long g_bytes_direct = 0;
static long g_bytes_pointer = 0;

static void swap_buffer_direct(Buffer *a, Buffer *b)
{
    Buffer tmp = *a;
    *a = *b;
    *b = tmp;
    g_bytes_direct += 3L * (long)sizeof(Buffer);
}

static void swap_ptr(Buffer **a, Buffer **b)
{
    Buffer *tmp = *a;
    *a = *b;
    *b = tmp;
    g_bytes_pointer += 3L * (long)sizeof(Buffer *);
}

/* 用相同的乱序 key 数组分别驱动两种方案的冒泡排序，交换次数完全一致 */
static void bubble_sort_direct(Buffer arr[], int n)
{
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - 1 - i; j++)
            if (arr[j].seq_no > arr[j + 1].seq_no)
                swap_buffer_direct(&arr[j], &arr[j + 1]);
}

static void bubble_sort_pointer(Buffer *ptrs[], int n)
{
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - 1 - i; j++)
            if (ptrs[j]->seq_no > ptrs[j + 1]->seq_no)
                swap_ptr(&ptrs[j], &ptrs[j + 1]);
}

int main(void)
{
    unsigned int seed = 999;
    int seq_numbers[N];
    for (int i = 0; i < N; i++) {
        seed = seed * 1103515245u + 12345u;
        seq_numbers[i] = (int)((seed >> 16) % 10000);
    }

    for (int i = 0; i < N; i++) {
        g_buffers_a[i].seq_no = seq_numbers[i];
        g_buffers_b[i].seq_no = seq_numbers[i];
        memset(g_buffers_a[i].data, 'X', BUF_SIZE);
        memset(g_buffers_b[i].data, 'X', BUF_SIZE);
    }

    Buffer *ptrs[N];
    for (int i = 0; i < N; i++)
        ptrs[i] = &g_buffers_b[i];

    printf("=== 批量缓冲区：直接搬移 vs 只排指针的成本对比 ===\n");
    printf("缓冲区数量 = %d，sizeof(Buffer) = %zu 字节，sizeof(Buffer*) = %zu 字节\n",
           N, sizeof(Buffer), sizeof(Buffer *));

    bubble_sort_direct(g_buffers_a, N);
    bubble_sort_pointer(ptrs, N);

    int ok = 1;
    for (int i = 0; i < N; i++) {
        if (g_buffers_a[i].seq_no != ptrs[i]->seq_no) {
            ok = 0;
            break;
        }
    }

    printf("两种方案排序结果是否一致: %s\n", ok ? "是" : "否");
    printf("方案一 (直接交换 Buffer)   累计搬移数据量 约 %.2f MB\n",
           g_bytes_direct / (1024.0 * 1024.0));
    printf("方案二 (只交换 Buffer*)    累计搬移数据量 约 %.2f KB\n",
           g_bytes_pointer / 1024.0);
    printf("\n说明：两种方案交换次数完全相同，唯一的区别是每次交换搬移的\n"
           "数据量——直接交换整块缓冲区 vs 只交换一个指针，实际搬移量\n"
           "相差了几个数量级，这就是批量缓冲区场景下优先选择指针排序的原因。\n");
    return 0;
}
