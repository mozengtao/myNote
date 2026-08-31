/*
 * 场景：数据库记录 —— 模拟分页存储，排序 (PageID, Offset) 而不是整行数据
 * 心智模型：真实数据库把行存储在磁盘页(Page)中，排序时用 (PageID, Offset)
 *          这种"元组标识符"作为间接层，最后才按需读取真正的页面数据。
 */
#include <stdio.h>
#include <string.h>

#define PAGE_SIZE 4
#define PAGE_COUNT 3
#define N (PAGE_SIZE * PAGE_COUNT)

typedef struct {
    int id;
    char name[16];
    double salary;
} Tuple;

typedef struct {
    Tuple tuples[PAGE_SIZE];
} Page;

typedef struct {
    int page_id;
    int offset;
} TupleRef;

static Page g_pages[PAGE_COUNT];

static Tuple *resolve(TupleRef ref)
{
    return &g_pages[ref.page_id].tuples[ref.offset];
}

static void sort_refs_by_salary(TupleRef refs[], int n)
{
    for (int i = 1; i < n; i++) {
        TupleRef key = refs[i];
        double key_salary = resolve(key)->salary;
        int j = i - 1;
        while (j >= 0 && resolve(refs[j])->salary > key_salary) {
            refs[j + 1] = refs[j];
            j--;
        }
        refs[j + 1] = key;
    }
}

int main(void)
{
    const char *names[N] = {"Alice", "Bob", "Carol", "Dave", "Eve", "Frank",
                             "Grace", "Heidi", "Ivan", "Judy", "Karl", "Liam"};
    double salaries[N] = {8600, 5200, 12000, 7300, 9600, 6100, 7000, 6800, 9100, 5600, 8900, 6400};

    int k = 0;
    for (int p = 0; p < PAGE_COUNT; p++) {
        for (int o = 0; o < PAGE_SIZE; o++) {
            g_pages[p].tuples[o].id = k + 1;
            strncpy(g_pages[p].tuples[o].name, names[k], sizeof(g_pages[p].tuples[o].name) - 1);
            g_pages[p].tuples[o].salary = salaries[k];
            k++;
        }
    }

    TupleRef refs[N];
    k = 0;
    for (int p = 0; p < PAGE_COUNT; p++)
        for (int o = 0; o < PAGE_SIZE; o++)
            refs[k++] = (TupleRef){p, o};

    printf("=== 数据库记录：模拟分页存储，排序 (PageID, Offset) ===\n");
    printf("共有 %d 页，每页 %d 条记录，总计 %d 条记录\n", PAGE_COUNT, PAGE_SIZE, N);

    sort_refs_by_salary(refs, N);

    printf("ORDER BY salary 结果 (通过 (PageID,Offset) 间接访问):\n");
    for (int i = 0; i < N; i++) {
        Tuple *t = resolve(refs[i]);
        printf("  page=%d offset=%d -> id=%d name=%-6s salary=%.0f\n",
               refs[i].page_id, refs[i].offset, t->id, t->name, t->salary);
    }

    printf("\n说明：排序过程中搬移的只是一个 8 字节大小的 (PageID, Offset) 元组，\n"
           "真正的行数据始终留在原来的 Page 里，这正是数据库存储引擎\n"
           "内部排序/索引扫描的典型做法。\n");
    return 0;
}
