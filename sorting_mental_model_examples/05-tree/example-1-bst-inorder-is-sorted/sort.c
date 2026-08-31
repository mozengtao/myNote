/*
 * 场景：二叉搜索树（BST）—— 中序遍历天然有序
 * 心智模型：树本身已经有序，不需要"整体排序"，中序遍历即可得到有序序列。
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int val;
    struct Node *left;
    struct Node *right;
} Node;

static Node *new_node(int val)
{
    Node *node = malloc(sizeof(Node));
    node->val = val;
    node->left = node->right = NULL;
    return node;
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

/* 中序遍历：左 -> 根 -> 右，对 BST 而言天然按升序访问 */
static void inorder(Node *root, int result[], int *idx)
{
    if (!root)
        return;
    inorder(root->left, result, idx);
    result[(*idx)++] = root->val;
    inorder(root->right, result, idx);
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
    int values[] = {50, 30, 70, 20, 40, 60, 80, 35, 65};
    int n = sizeof(values) / sizeof(values[0]);

    Node *root = NULL;
    for (int i = 0; i < n; i++)
        root = insert(root, values[i]);

    printf("=== BST：中序遍历天然有序 ===\n");
    printf("插入顺序: ");
    for (int i = 0; i < n; i++)
        printf("%d ", values[i]);
    printf("\n");

    int result[9];
    int idx = 0;
    inorder(root, result, &idx);

    printf("中序遍历结果: ");
    for (int i = 0; i < n; i++)
        printf("%d ", result[i]);
    printf("\n\n说明：从未对数据做过'排序'这个动作，BST 的结构本身就保证了\n"
           "中序遍历得到的序列一定是升序的，这就是'树天然有序'。\n");

    free_tree(root);
    return 0;
}
