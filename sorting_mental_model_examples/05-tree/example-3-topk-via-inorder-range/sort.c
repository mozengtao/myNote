/*
 * 场景：二叉搜索树 —— 借助中序遍历做 Top-K / 区间查询
 * 心智模型：树已经是有序的，查询"最小的 K 个"或"某个区间内的元素"
 *          不需要先导出全部数据再排序，只需要有选择地遍历/剪枝。
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int val;
    struct Node *left;
    struct Node *right;
} Node;

static long g_visited_nodes = 0;

static Node *new_node(int val)
{
    Node *n = malloc(sizeof(Node));
    n->val = val;
    n->left = n->right = NULL;
    return n;
}

static Node *insert(Node *root, int val)
{
    if (!root)
        return new_node(val);
    if (val < root->val)
        root->left = insert(root->left, val);
    else
        root->right = insert(root->right, val);
    return root;
}

/* Top-K：中序遍历本身就是升序，收集到 k 个就可以提前停止 */
static void k_smallest(Node *root, int k, int result[], int *count)
{
    if (!root || *count >= k)
        return;
    g_visited_nodes++;
    k_smallest(root->left, k, result, count);
    if (*count < k)
        result[(*count)++] = root->val;
    k_smallest(root->right, k, result, count);
}

/* 区间查询 [lo, hi]：利用 BST 的有序性剪枝，跳过不可能落在区间内的子树 */
static void range_query(Node *root, int lo, int hi, int result[], int *count)
{
    if (!root)
        return;
    g_visited_nodes++;
    if (root->val > lo)
        range_query(root->left, lo, hi, result, count);
    if (root->val >= lo && root->val <= hi)
        result[(*count)++] = root->val;
    if (root->val < hi)
        range_query(root->right, lo, hi, result, count);
}

static void free_tree(Node *root)
{
    if (!root)
        return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

int main(void)
{
    int values[] = {50, 30, 70, 20, 40, 60, 80, 10, 25, 35, 45, 55, 65, 75, 90};
    int n = sizeof(values) / sizeof(values[0]);

    Node *root = NULL;
    for (int i = 0; i < n; i++)
        root = insert(root, values[i]);

    printf("=== BST：借助中序遍历做 Top-K / 区间查询 ===\n");
    printf("树中共有 %d 个节点\n\n", n);

    /* Top-K 最小值 */
    int k = 4;
    int topk[4];
    int topk_count = 0;
    g_visited_nodes = 0;
    k_smallest(root, k, topk, &topk_count);
    printf("最小的 %d 个值: ", k);
    for (int i = 0; i < topk_count; i++)
        printf("%d ", topk[i]);
    printf("(访问节点数 = %ld，总节点数 = %d)\n", g_visited_nodes, n);

    /* 区间查询 */
    int lo = 30, hi = 60;
    int range_result[15];
    int range_count = 0;
    g_visited_nodes = 0;
    range_query(root, lo, hi, range_result, &range_count);
    printf("区间 [%d, %d] 内的值: ", lo, hi);
    for (int i = 0; i < range_count; i++)
        printf("%d ", range_result[i]);
    printf("(访问节点数 = %ld，总节点数 = %d)\n", g_visited_nodes, n);

    printf("\n说明：两个查询都没有把全部 %d 个节点取出来再排序，\n"
           "而是利用 BST 天然有序的特性做剪枝遍历，访问节点数明显少于总节点数。\n",
           n);

    free_tree(root);
    return 0;
}
