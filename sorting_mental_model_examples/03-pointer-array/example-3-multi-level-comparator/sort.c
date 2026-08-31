/*
 * 场景：指针数组 —— 多级比较器（先按 department，再按 salary 降序）
 * 心智模型：Comparator 可以组合多个 Key，形成"复合排序规则"，
 *          而 Move Strategy（交换指针）完全不受影响。
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    int id;
    char name[16];
    char department[8];
    double salary;
} Employee;

static Employee g_pool[] = {
    {1, "Alice", "Eng", 8600},
    {2, "Bob", "Sales", 5200},
    {3, "Carol", "Eng", 12000},
    {4, "Dave", "Sales", 7300},
    {5, "Eve", "Eng", 9600},
    {6, "Frank", "Sales", 6100},
    {7, "Grace", "HR", 7000},
    {8, "Heidi", "HR", 6800},
};
#define N (int)(sizeof(g_pool) / sizeof(g_pool[0]))

/* 多级 Comparator：先比 department，相同再比 salary（降序） */
static int cmp_multi_level(const void *pa, const void *pb)
{
    const Employee *a = *(Employee * const *)pa;
    const Employee *b = *(Employee * const *)pb;

    int dept_cmp = strcmp(a->department, b->department);
    if (dept_cmp != 0)
        return dept_cmp;

    if (a->salary < b->salary) return 1;
    if (a->salary > b->salary) return -1;
    return 0;
}

static void print_view(const char *label, Employee *ptrs[], int n)
{
    printf("%s:\n", label);
    for (int i = 0; i < n; i++)
        printf("  id=%d name=%-6s dept=%-6s salary=%.0f\n",
               ptrs[i]->id, ptrs[i]->name, ptrs[i]->department, ptrs[i]->salary);
}

int main(void)
{
    Employee *ptrs[N];
    for (int i = 0; i < N; i++)
        ptrs[i] = &g_pool[i];

    printf("=== 指针数组：多级 Comparator（先 department，再 salary 降序） ===\n");

    qsort(ptrs, N, sizeof(Employee *), cmp_multi_level);
    print_view("排序结果", ptrs, N);

    printf("\n说明：Comparator 内部先比较 department，相同部门再比较 salary，\n"
           "排序算法本身完全不需要修改，Move Strategy 依旧只是交换指针。\n");
    return 0;
}
