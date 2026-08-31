/*
 * 场景：数据库记录 —— ORDER BY 排的是 RowID，而不是整行数据
 * 心智模型：一行记录可能有几 KB，直接搬移整行代价很高；
 *          数据库内部通常排序 RowID/指针，最后再按需读取真正的行数据。
 */
#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
    char name[16];
    double salary;
} Row;

#define N 8
static Row g_table[N] = {
    {1, "Alice", 8600}, {2, "Bob", 5200}, {3, "Carol", 12000}, {4, "Dave", 7300},
    {5, "Eve", 9600}, {6, "Frank", 6100}, {7, "Grace", 7000}, {8, "Heidi", 6800},
};

static void sort_rowids_by_salary(int rowids[], int n)
{
    /* 插入排序即可，重点不在算法本身，而在于排的是 RowID */
    for (int i = 1; i < n; i++) {
        int key = rowids[i];
        int j = i - 1;
        while (j >= 0 && g_table[rowids[j]].salary > g_table[key].salary) {
            rowids[j + 1] = rowids[j];
            j--;
        }
        rowids[j + 1] = key;
    }
}

int main(void)
{
    int rowids[N];
    for (int i = 0; i < N; i++)
        rowids[i] = i; /* RowID 就是数组下标，模拟数据库的行标识 */

    const void *original_first = &g_table[0];

    printf("=== 数据库记录：ORDER BY salary，只排序 RowID ===\n");

    sort_rowids_by_salary(rowids, N);

    printf("ORDER BY salary 结果 (通过 RowID 间接访问):\n");
    for (int i = 0; i < N; i++) {
        Row *r = &g_table[rowids[i]];
        printf("  RowID=%d id=%d name=%-6s salary=%.0f\n", rowids[i], r->id, r->name, r->salary);
    }

    printf("原始 g_table 是否原地未动: %s\n",
           (original_first == (const void *)&g_table[0]) ? "是" : "否");
    printf("\n说明：真正参与排序的是 rowids 数组里的整数（RowID），\n"
           "g_table 里的每一行都没有被移动过，这就是数据库常说的\n"
           "'排索引/排 RowID，而不是排整行数据'。\n");
    return 0;
}
