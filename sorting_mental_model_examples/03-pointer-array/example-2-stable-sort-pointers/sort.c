/*
 * 场景：指针数组 —— 稳定排序（保持相同 Key 的相对顺序）
 * 心智模型：交换的是指针，但排序算法本身是否稳定，决定了
 *          相同 Key 的记录在排序前后相对顺序是否被打乱。
 *          C 标准库 qsort() 不保证稳定，这里手写一个稳定的
 *          归并排序作用于指针数组，来演示"稳定地排序指针"。
 */
#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
    char name[16];
    char department[8];
} Employee;

static Employee g_pool[] = {
    {1, "Alice", "Eng"},
    {2, "Bob", "Sales"},
    {3, "Carol", "Eng"},
    {4, "Dave", "Sales"},
    {5, "Eve", "Eng"},
    {6, "Frank", "Sales"},
    {7, "Grace", "HR"},
    {8, "Heidi", "HR"},
};
#define N (int)(sizeof(g_pool) / sizeof(g_pool[0]))

static int less_by_department(const Employee *a, const Employee *b)
{
    return strcmp(a->department, b->department) < 0;
}

/* 稳定归并：合并时优先取左区间元素，保持相同 Key 记录的原有相对顺序 */
static void merge(Employee *ptrs[], Employee *tmp[], int low, int mid, int high)
{
    int i = low, j = mid + 1, k = low;
    while (i <= mid && j <= high) {
        if (!less_by_department(ptrs[j], ptrs[i])) /* ptrs[i] <= ptrs[j] 时优先取左边 */
            tmp[k++] = ptrs[i++];
        else
            tmp[k++] = ptrs[j++];
    }
    while (i <= mid)
        tmp[k++] = ptrs[i++];
    while (j <= high)
        tmp[k++] = ptrs[j++];
    for (int x = low; x <= high; x++)
        ptrs[x] = tmp[x];
}

static void merge_sort(Employee *ptrs[], Employee *tmp[], int low, int high)
{
    if (low >= high)
        return;
    int mid = low + (high - low) / 2;
    merge_sort(ptrs, tmp, low, mid);
    merge_sort(ptrs, tmp, mid + 1, high);
    merge(ptrs, tmp, low, mid, high);
}

static void print_view(const char *label, Employee *ptrs[], int n)
{
    printf("%s:\n", label);
    for (int i = 0; i < n; i++)
        printf("  id=%d name=%-6s dept=%-6s\n", ptrs[i]->id, ptrs[i]->name, ptrs[i]->department);
}

int main(void)
{
    Employee *ptrs[N];
    Employee *tmp[N];
    for (int i = 0; i < N; i++)
        ptrs[i] = &g_pool[i];

    printf("=== 指针数组：稳定排序，保持相同 department 的相对顺序 ===\n");
    print_view("排序前（按 id 插入顺序）", ptrs, N);

    merge_sort(ptrs, tmp, 0, N - 1);

    print_view("稳定排序后（按 department）", ptrs, N);
    printf("\n说明：Eng 组内部仍然是 Alice(1) -> Carol(3) -> Eve(5) 的原始相对顺序，\n"
           "Sales 组内部仍然是 Bob(2) -> Dave(4) -> Frank(6)。\n"
           "这就是'稳定排序'：只重排指针，但不打乱相同 Key 记录的原有相对顺序。\n");
    return 0;
}
