/*
 * 场景：堆 —— 定时器/事件队列，动态维护"下一个最早触发的事件"
 * 心智模型：堆不是为了得到完整有序序列，而是为了快速获取当前最值，
 *          并且可以在新事件加入后继续保持这个能力，而不需要重新排序全部事件。
 */
#include <stdio.h>
#include <string.h>

typedef struct {
    int fire_time;
    char name[16];
    int period; /* 0 表示一次性事件，非 0 表示周期性事件 */
} Event;

#define CAP 16
static Event heap[CAP];
static int heap_size = 0;

static void swap_event(Event *a, Event *b)
{
    Event t = *a;
    *a = *b;
    *b = t;
}

static void sift_up(int i)
{
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap[parent].fire_time <= heap[i].fire_time)
            break;
        swap_event(&heap[parent], &heap[i]);
        i = parent;
    }
}

static void sift_down(int i)
{
    while (1) {
        int left = 2 * i + 1, right = 2 * i + 2, smallest = i;
        if (left < heap_size && heap[left].fire_time < heap[smallest].fire_time) smallest = left;
        if (right < heap_size && heap[right].fire_time < heap[smallest].fire_time) smallest = right;
        if (smallest == i)
            break;
        swap_event(&heap[i], &heap[smallest]);
        i = smallest;
    }
}

static void push_event(Event e)
{
    heap[heap_size] = e;
    sift_up(heap_size);
    heap_size++;
}

static Event pop_event(void)
{
    Event top = heap[0];
    heap_size--;
    heap[0] = heap[heap_size];
    sift_down(0);
    return top;
}

int main(void)
{
    push_event((Event){100, "heartbeat", 50});
    push_event((Event){80, "log-flush", 40});
    push_event((Event){200, "report", 0});
    push_event((Event){30, "retry", 20});

    printf("=== 堆：定时器事件队列，动态维护最早触发的事件 ===\n");
    printf("初始事件数 = %d\n\n", heap_size);

    for (int round = 1; round <= 6 && heap_size > 0; round++) {
        Event e = pop_event();
        printf("第 %d 轮：触发事件 [%s] fire_time=%d\n", round, e.name, e.fire_time);
        if (e.period > 0) {
            Event next = e;
            next.fire_time += e.period;
            push_event(next);
            printf("         -> 周期事件重新调度到 fire_time=%d\n", next.fire_time);
        }
    }

    printf("\n说明：每次只花 O(log n) 就能取出当前最早的事件，新事件（包括\n"
           "周期性事件的下一次触发）随时可以加入堆中，完全不需要对整个\n"
           "事件列表重新排序。这正是定时器、任务调度器等场景使用堆的原因。\n");
    return 0;
}
