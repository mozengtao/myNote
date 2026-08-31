/*
 * 场景：批量缓冲区 —— 音视频帧缓冲区按时间戳重排，不拷贝大帧数据
 * 心智模型：网络抖动会导致帧乱序到达，重建播放顺序时只需要排序
 *          "帧引用"（指针/索引），庞大的帧数据本体不需要移动。
 */
#include <stdio.h>
#include <string.h>

#define FRAME_SIZE 4096 /* 模拟一帧压缩后的数据大小 */
#define N 6

typedef struct {
    int seq_no;
    long timestamp_ms;
    char data[FRAME_SIZE];
} Frame;

static Frame g_buffer[N];

static void sort_by_timestamp(Frame *ptrs[], int n)
{
    for (int i = 1; i < n; i++) {
        Frame *key = ptrs[i];
        int j = i - 1;
        while (j >= 0 && ptrs[j]->timestamp_ms > key->timestamp_ms) {
            ptrs[j + 1] = ptrs[j];
            j--;
        }
        ptrs[j + 1] = key;
    }
}

int main(void)
{
    /* 模拟网络抖动导致的乱序到达 */
    long arrival_timestamps[N] = {320, 80, 240, 0, 160, 400};

    for (int i = 0; i < N; i++) {
        g_buffer[i].seq_no = i;
        g_buffer[i].timestamp_ms = arrival_timestamps[i];
        memset(g_buffer[i].data, 'A' + i, FRAME_SIZE);
    }

    Frame *ptrs[N];
    for (int i = 0; i < N; i++)
        ptrs[i] = &g_buffer[i];

    printf("=== 批量缓冲区：音视频帧按时间戳重排（不拷贝帧数据） ===\n");
    printf("到达顺序 (timestamp_ms): ");
    for (int i = 0; i < N; i++)
        printf("%ld ", g_buffer[i].timestamp_ms);
    printf("\n每帧大小 = %d 字节\n", FRAME_SIZE);

    sort_by_timestamp(ptrs, N);

    printf("重建播放顺序 (timestamp_ms): ");
    for (int i = 0; i < N; i++)
        printf("%ld ", ptrs[i]->timestamp_ms);
    printf("\n\n说明：整个重排过程只交换了 %zu 字节大小的帧指针，\n"
           "每帧 %d 字节的实际数据始终留在 g_buffer 原来的位置上。\n",
           sizeof(Frame *), FRAME_SIZE);
    return 0;
}
