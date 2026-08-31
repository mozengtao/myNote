/*
 * 场景：任务调度 —— 最早截止时间优先（EDF），用堆维护任务指针
 * 心智模型：调度器只关心"下一个该运行的是谁"，用堆可以在 O(log N)
 *          时间内完成插入新任务、取出截止时间最近的任务，
 *          完全不需要对整个任务集合做完整排序。
 */
#include <stdio.h>
#include <string.h>

typedef struct {
    char name[16];
    int deadline;   /* 截止时间：数值越小越紧急 */
    int duration;   /* 预计运行时长 */
} Task;

#define MAXN 16

/* 小顶堆，堆中存放的是指针，交换的成本只是一个地址 */
typedef struct {
    Task *heap[MAXN];
    int size;
} MinHeap;

static void heap_init(MinHeap *h) { h->size = 0; }

static void heap_swap(Task **a, Task **b)
{
    Task *tmp = *a;
    *a = *b;
    *b = tmp;
}

static void heap_push(MinHeap *h, Task *t)
{
    int i = h->size++;
    h->heap[i] = t;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->heap[parent]->deadline <= h->heap[i]->deadline)
            break;
        heap_swap(&h->heap[parent], &h->heap[i]);
        i = parent;
    }
}

static Task *heap_pop(MinHeap *h)
{
    Task *top = h->heap[0];
    h->heap[0] = h->heap[--h->size];
    int i = 0;
    for (;;) {
        int left = 2 * i + 1, right = 2 * i + 2, smallest = i;
        if (left < h->size && h->heap[left]->deadline < h->heap[smallest]->deadline)
            smallest = left;
        if (right < h->size && h->heap[right]->deadline < h->heap[smallest]->deadline)
            smallest = right;
        if (smallest == i)
            break;
        heap_swap(&h->heap[i], &h->heap[smallest]);
        i = smallest;
    }
    return top;
}

int main(void)
{
    Task pool[] = {
        {"render-frame", 8, 2},
        {"send-heartbeat", 3, 1},
        {"flush-log", 15, 4},
        {"process-input", 5, 1},
        {"sync-db", 20, 6},
    };
    int n = sizeof(pool) / sizeof(pool[0]);

    printf("=== 任务调度：最早截止时间优先（EDF） ===\n");
    printf("任务到达顺序（未排序）：");
    for (int i = 0; i < n; i++)
        printf("%s(ddl=%d) ", pool[i].name, pool[i].deadline);
    printf("\n\n");

    /* 只把指针压入堆，堆内部的比较/交换从不触碰 Task 本体 */
    MinHeap h;
    heap_init(&h);
    for (int i = 0; i < n; i++)
        heap_push(&h, &pool[i]);

    printf("按截止时间依次调度：\n");
    int clock = 0;
    while (h.size > 0) {
        Task *t = heap_pop(&h);
        printf("  时刻 %2d：运行 [%s]（截止时间=%d，时长=%d）%s\n",
               clock, t->name, t->deadline, t->duration,
               clock + t->duration > t->deadline ? "<- 已超过截止时间！" : "");
        clock += t->duration;
    }

    printf("\n说明：EDF 调度不需要提前把所有任务按截止时间整体排序一次；\n"
           "新任务随时可以 O(log N) 插入堆中，取出下一个任务也是 O(log N)，\n"
           "堆里搬运的始终只是任务指针，Task 本体从未被移动过。\n");
    return 0;
}
