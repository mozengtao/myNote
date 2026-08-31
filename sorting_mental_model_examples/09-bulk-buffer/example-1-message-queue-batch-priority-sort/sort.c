/*
 * 场景：批量缓冲区 —— 消息队列批量出队后按优先级排序指针
 * 心智模型：消息体可能较大（几百字节到几 KB），批处理时只排序消息指针，
 *          消息本身留在原来的缓冲区里不动。
 */
#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
    int priority; /* 数值越大优先级越高 */
    char payload[256];
} Message;

#define N 8
static Message g_inbox[N];

/* 稳定的插入排序：优先级相同时保持原始到达顺序（FIFO），这对消息处理很重要 */
static void sort_by_priority_desc(Message *ptrs[], int n)
{
    for (int i = 1; i < n; i++) {
        Message *key = ptrs[i];
        int j = i - 1;
        while (j >= 0 && ptrs[j]->priority < key->priority) {
            ptrs[j + 1] = ptrs[j];
            j--;
        }
        ptrs[j + 1] = key;
    }
}

int main(void)
{
    int priorities[N] = {2, 5, 2, 8, 5, 1, 8, 3};

    for (int i = 0; i < N; i++) {
        g_inbox[i].id = i + 1;
        g_inbox[i].priority = priorities[i];
        memset(g_inbox[i].payload, 'a' + i, sizeof(g_inbox[i].payload));
    }

    Message *ptrs[N];
    for (int i = 0; i < N; i++)
        ptrs[i] = &g_inbox[i];

    printf("=== 批量缓冲区：消息队列批量出队后按优先级排序指针 ===\n");
    printf("到达顺序 (id:priority): ");
    for (int i = 0; i < N; i++)
        printf("%d:%d ", g_inbox[i].id, g_inbox[i].priority);
    printf("\n");

    sort_by_priority_desc(ptrs, N);

    printf("按优先级处理顺序 (id:priority): ");
    for (int i = 0; i < N; i++)
        printf("%d:%d ", ptrs[i]->id, ptrs[i]->priority);
    printf("\n\n说明：id=2 和 id=5 优先级都是 5，排序后 id=2 仍然排在 id=5 前面，\n"
           "这是稳定排序保留的到达顺序（FIFO）。整个过程只移动 %zu 字节的\n"
           "指针，g_inbox 里每条消息的 payload 都没有被拷贝或搬移过。\n",
           sizeof(Message *));
    return 0;
}
