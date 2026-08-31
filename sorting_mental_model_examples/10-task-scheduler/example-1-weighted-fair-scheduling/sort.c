/*
 * 场景：任务调度 —— 类 vruntime 的加权公平调度，排序的是任务指针
 * 心智模型：调度器不需要每次都对全部任务重新排序，只需要把刚运行完的
 *          任务重新插入到正确的位置（按 vruntime 排序的运行队列）。
 */
#include <stdio.h>
#include <string.h>

typedef struct {
    char name[16];
    int weight;      /* 权重越大，占用 CPU 的比例应该越高 */
    double vruntime;  /* 虚拟运行时间：数值越小，越应该优先被调度 */
} Task;

#define N 4
static Task g_runqueue[N] = {
    {"nginx", 1, 0}, {"backup", 1, 0}, {"video-encode", 4, 0}, {"logger", 1, 0},
};

/* queue[0] 的 vruntime 刚变大，只需要把它移动到新的正确位置，
 * 不需要重新排序整个队列 —— 这就是"增量维护有序"而不是"整体重排"。 */
static void reinsert_head(Task *queue[], int n)
{
    Task *t = queue[0];
    int i = 0;
    while (i + 1 < n && queue[i + 1]->vruntime < t->vruntime) {
        queue[i] = queue[i + 1];
        i++;
    }
    queue[i] = t;
}

static void print_queue(const char *label, Task *queue[], int n)
{
    printf("%s: ", label);
    for (int i = 0; i < n; i++)
        printf("%s(vr=%.1f) ", queue[i]->name, queue[i]->vruntime);
    printf("\n");
}

int main(void)
{
    Task *queue[N];
    for (int i = 0; i < N; i++)
        queue[i] = &g_runqueue[i];

    printf("=== 任务调度：加权公平调度（类 vruntime） ===\n");
    print_queue("初始运行队列", queue, N);

    const double slice = 10.0; /* 每次调度分配的时间片 */
    for (int round = 1; round <= 6; round++) {
        Task *picked = queue[0]; /* 队首永远是 vruntime 最小的任务 */
        printf("第 %d 轮：调度 [%s]（当前 vruntime=%.1f，权重=%d）\n",
               round, picked->name, picked->vruntime, picked->weight);

        /* 权重越大，vruntime 增长越慢，从而获得更多实际运行时间 */
        picked->vruntime += slice / picked->weight;

        reinsert_head(queue, N);
        print_queue("         调度后运行队列", queue, N);
    }

    printf("\n说明：video-encode 权重是其它任务的 4 倍，vruntime 增长最慢，\n"
           "因此长期来看会被调度得更频繁，获得更多 CPU 时间；每一轮\n"
           "只需要把被调度过的任务重新插入队列，不需要整体重新排序。\n");
    return 0;
}
