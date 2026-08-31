/*
 * 场景：外部排序 —— 数据量超过内存限制时，分块排序后落盘临时文件
 * 心智模型：内存排序解决不了的问题，就把排序过程分阶段完成。
 */
#include <stdio.h>
#include <stdlib.h>

#define TOTAL 40
#define CHUNK_SIZE 8 /* 模拟"内存一次只能装下 8 个数" */

static int cmp_int(const void *a, const void *b)
{
    return (*(const int *)a) - (*(const int *)b);
}

static void chunk_filename(char *buf, size_t size, int idx)
{
    snprintf(buf, size, "chunk_%d.txt", idx);
}

int main(void)
{
    int data[TOTAL];
    unsigned int seed = 20240101;
    for (int i = 0; i < TOTAL; i++) {
        seed = seed * 1103515245u + 12345u;
        data[i] = (int)((seed >> 16) % 1000);
    }

    printf("=== 外部排序：分块排序 + 落盘临时文件 ===\n");
    printf("总数据量 = %d，模拟内存一次只能容纳 %d 个元素\n", TOTAL, CHUNK_SIZE);

    int chunk_count = (TOTAL + CHUNK_SIZE - 1) / CHUNK_SIZE;
    for (int c = 0; c < chunk_count; c++) {
        int start = c * CHUNK_SIZE;
        int len = (start + CHUNK_SIZE <= TOTAL) ? CHUNK_SIZE : (TOTAL - start);

        int chunk[CHUNK_SIZE];
        for (int i = 0; i < len; i++)
            chunk[i] = data[start + i];

        qsort(chunk, len, sizeof(int), cmp_int); /* 块内排序：内存完全放得下 */

        char filename[64];
        chunk_filename(filename, sizeof(filename), c);
        FILE *fp = fopen(filename, "w");
        for (int i = 0; i < len; i++)
            fprintf(fp, "%d\n", chunk[i]);
        fclose(fp);

        printf("第 %d 块 (%d 个元素) 已排序并写入磁盘文件 %s\n", c, len, filename);
    }

    /* 验证每个文件内部确实有序，然后清理临时文件 */
    int all_sorted = 1;
    for (int c = 0; c < chunk_count; c++) {
        char filename[64];
        chunk_filename(filename, sizeof(filename), c);
        FILE *fp = fopen(filename, "r");
        int prev = 0, cur, has_prev = 0;
        while (fscanf(fp, "%d", &cur) == 1) {
            if (has_prev && prev > cur)
                all_sorted = 0;
            prev = cur;
            has_prev = 1;
        }
        fclose(fp);
        remove(filename);
    }

    printf("\n所有分块文件内部有序: %s（临时文件已清理）\n", all_sorted ? "是" : "否");
    printf("\n说明：每一块都在内存里完成排序，然后立刻写入磁盘释放内存。\n"
           "接下来用 K 路归并（参见 example-2）即可把这些有序小文件合并成\n"
           "一个整体有序结果，这就是 External Merge Sort 的第一阶段。\n");
    return 0;
}
