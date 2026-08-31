/*
 * 场景：数据库记录 —— 复合索引 ORDER BY department, salary DESC
 * 心智模型：复合索引本质上也是排序 RowID，只是 Comparator 里组合了多个列。
 */
#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
    char name[16];
    char department[8];
    double salary;
} Row;

#define N 8
static Row g_table[N] = {
    {1, "Alice", "Eng", 8600}, {2, "Bob", "Sales", 5200}, {3, "Carol", "Eng", 12000},
    {4, "Dave", "Sales", 7300}, {5, "Eve", "Eng", 9600}, {6, "Frank", "Sales", 6100},
    {7, "Grace", "HR", 7000}, {8, "Heidi", "HR", 6800},
};

/* 复合 Comparator：先按 department 升序，再按 salary 降序 —— 对应
 * SQL 里的 ORDER BY department, salary DESC */
static int less_than(int a_rowid, int b_rowid)
{
    Row *a = &g_table[a_rowid];
    Row *b = &g_table[b_rowid];
    int dept_cmp = strcmp(a->department, b->department);
    if (dept_cmp != 0)
        return dept_cmp < 0;
    return a->salary > b->salary;
}

static void sort_rowids(int rowids[], int n)
{
    for (int i = 1; i < n; i++) {
        int key = rowids[i];
        int j = i - 1;
        while (j >= 0 && less_than(key, rowids[j])) {
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
        rowids[i] = i;

    printf("=== 数据库记录：复合索引 ORDER BY department, salary DESC ===\n");

    sort_rowids(rowids, N);

    for (int i = 0; i < N; i++) {
        Row *r = &g_table[rowids[i]];
        printf("  RowID=%d name=%-6s dept=%-6s salary=%.0f\n",
               rowids[i], r->name, r->department, r->salary);
    }

    printf("\n说明：Comparator 内部先比较 department，再比较 salary，\n"
           "真正被排序的仍然只是 rowids 数组里的整数，这就是数据库\n"
           "'复合索引'的核心思路：多列组合成一个排序规则，作用在行标识上。\n");
    return 0;
}
