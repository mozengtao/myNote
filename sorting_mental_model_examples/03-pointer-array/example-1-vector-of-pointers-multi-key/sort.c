/*
 * 场景：指针数组 —— 用不同 Comparator 反复对同一批指针重新排序
 * 心智模型：数据只有一份，指针数组可以按需切换排序规则，
 *          而不需要为每一种排序方式复制一份数据。
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

static int cmp_by_name(const void *pa, const void *pb)
{
    const Employee *a = *(Employee * const *)pa;
    const Employee *b = *(Employee * const *)pb;
    return strcmp(a->name, b->name);
}

static int cmp_by_salary_desc(const void *pa, const void *pb)
{
    const Employee *a = *(Employee * const *)pa;
    const Employee *b = *(Employee * const *)pb;
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

    printf("=== 指针数组：同一批指针，按不同 Comparator 反复排序 ===\n");

    qsort(ptrs, N, sizeof(Employee *), cmp_by_name);
    print_view("按 name 排序", ptrs, N);

    qsort(ptrs, N, sizeof(Employee *), cmp_by_salary_desc);
    print_view("按 salary 从高到低排序", ptrs, N);

    printf("\n说明：g_pool 里的 Employee 对象没有被复制或修改过，\n"
           "ptrs 数组每次只是重新排列 8 字节的指针，切换 Comparator 即可\n"
           "得到完全不同的排序视图，代价远低于交换整个 Employee。\n");
    return 0;
}
