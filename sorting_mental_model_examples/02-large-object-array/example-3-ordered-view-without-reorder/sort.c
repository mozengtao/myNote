/*
 * 场景：大对象数组 —— 同一份数据的多个"有序视图"
 * 心智模型：数据本身只保存一份且从不移动，可以同时维护多个不同排序
 *          规则的索引数组，得到多个互不冲突的"有序视图"。
 */
#include <stdio.h>
#include <string.h>

#define PAYLOAD_SIZE 200
#define N 8

typedef struct {
    int id;
    char name[16];
    double score;
    char payload[PAYLOAD_SIZE];
} Record;

static void swap_ptr(Record **a, Record **b)
{
    Record *tmp = *a;
    *a = *b;
    *b = tmp;
}

static void sort_index_by_score(Record *index[], int n)
{
    /* 数据量很小，插入排序即可 */
    for (int i = 1; i < n; i++) {
        int j = i;
        while (j > 0 && index[j - 1]->score > index[j]->score) {
            swap_ptr(&index[j - 1], &index[j]);
            j--;
        }
    }
}

static void sort_index_by_name(Record *index[], int n)
{
    for (int i = 1; i < n; i++) {
        int j = i;
        while (j > 0 && strcmp(index[j - 1]->name, index[j]->name) > 0) {
            swap_ptr(&index[j - 1], &index[j]);
            j--;
        }
    }
}

static void print_view(const char *label, Record *index[], int n)
{
    printf("%s:\n", label);
    for (int i = 0; i < n; i++)
        printf("  id=%d name=%-8s score=%.1f\n", index[i]->id, index[i]->name, index[i]->score);
}

int main(void)
{
    Record records[N] = {
        {1, "Frank", 72.5, {0}}, {2, "Alice", 88.0, {0}}, {3, "Eve", 65.3, {0}},
        {4, "Carol", 91.2, {0}}, {5, "Bob", 77.8, {0}}, {6, "Grace", 83.4, {0}},
        {7, "Heidi", 59.9, {0}}, {8, "Dave", 95.0, {0}},
    };

    Record *index_by_score[N];
    Record *index_by_name[N];
    for (int i = 0; i < N; i++) {
        index_by_score[i] = &records[i];
        index_by_name[i] = &records[i];
    }

    const void *original_first = &records[0];

    printf("=== 大对象数组：同一份数据的两个有序视图 ===\n");

    sort_index_by_score(index_by_score, N);
    sort_index_by_name(index_by_name, N);

    print_view("视图一：按 score 排序", index_by_score, N);
    print_view("视图二：按 name 排序", index_by_name, N);

    printf("原始数组是否原地未动: %s\n",
           (original_first == (const void *)&records[0]) ? "是" : "否");
    printf("records[0] 仍然是: id=%d name=%s（未被排序改变）\n", records[0].id, records[0].name);
    printf("\n说明：records 数组只保存一份数据且自始至终没有被重新排列，\n"
           "index_by_score / index_by_name 是两个独立的'有序视图'，\n"
           "彼此互不干扰，也不需要为每一种排序需求复制一份数据。\n");
    return 0;
}
