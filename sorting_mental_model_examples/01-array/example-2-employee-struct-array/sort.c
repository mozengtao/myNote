/*
 * 场景：小型结构体数组按薪资(salary)排序
 * 心智模型：排序算法真正比较的是 Key（salary），而不是整个 Record；
 *          但当 Record 足够小时，直接整体交换仍然是最简单的 Move Strategy。
 */
#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
    char name[16];
    double salary;
} Employee;

/* Comparator：只关心 Key（salary），不关心 Record 的其它字段 */
static int less_than(const Employee *a, const Employee *b)
{
    return a->salary < b->salary;
}

/* Move Strategy：结构体较小，直接整体交换 */
static void swap_employee(Employee *a, Employee *b)
{
    Employee tmp = *a;
    *a = *b;
    *b = tmp;
}

static int partition(Employee arr[], int low, int high)
{
    Employee pivot = arr[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (less_than(&arr[j], &pivot)) {
            i++;
            swap_employee(&arr[i], &arr[j]);
        }
    }
    swap_employee(&arr[i + 1], &arr[high]);
    return i + 1;
}

static void quicksort(Employee arr[], int low, int high)
{
    if (low < high) {
        int p = partition(arr, low, high);
        quicksort(arr, low, p - 1);
        quicksort(arr, p + 1, high);
    }
}

static void print_employees(const char *label, Employee arr[], int n)
{
    printf("%s:\n", label);
    for (int i = 0; i < n; i++)
        printf("  id=%d name=%-8s salary=%.2f\n", arr[i].id, arr[i].name, arr[i].salary);
}

int main(void)
{
    Employee employees[] = {
        {1, "Alice", 8600.0},
        {2, "Bob", 5200.0},
        {3, "Carol", 12000.0},
        {4, "Dave", 7300.0},
        {5, "Eve", 9600.0},
    };
    int n = sizeof(employees) / sizeof(employees[0]);

    printf("=== 小对象数组排序：按薪资(salary)排序 Employee ===\n");
    print_employees("排序前", employees, n);

    quicksort(employees, 0, n - 1);

    print_employees("排序后", employees, n);
    printf("\n说明：Employee 结构体不大（几十字节），排序算法只比较 salary 这个 Key，\n"
           "但 Move Strategy 仍然是直接交换整个 Record，因为整体搬移的代价可以接受。\n");
    return 0;
}
