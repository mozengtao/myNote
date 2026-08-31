/*
 * 场景：二叉搜索树 —— 简化版 AVL 旋转，插入时维持平衡而不是重新排序
 * 心智模型：树的最佳实践是"维持平衡"，而不是退化后再重新整体排序。
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int val;
    int height;
    struct Node *left;
    struct Node *right;
} Node;

static int height_of(Node *n) { return n ? n->height : 0; }
static int max_int(int a, int b) { return a > b ? a : b; }

static Node *new_node(int val)
{
    Node *n = malloc(sizeof(Node));
    n->val = val;
    n->height = 1;
    n->left = n->right = NULL;
    return n;
}

static void update_height(Node *n)
{
    n->height = 1 + max_int(height_of(n->left), height_of(n->right));
}

static int balance_factor(Node *n)
{
    return n ? height_of(n->left) - height_of(n->right) : 0;
}

static Node *rotate_right(Node *y)
{
    Node *x = y->left;
    Node *t2 = x->right;
    x->right = y;
    y->left = t2;
    update_height(y);
    update_height(x);
    return x;
}

static Node *rotate_left(Node *x)
{
    Node *y = x->right;
    Node *t2 = y->left;
    y->left = x;
    x->right = t2;
    update_height(x);
    update_height(y);
    return y;
}

/* 简化版 AVL 插入：插入后自底向上检查平衡因子，超出 [-1,1] 就旋转纠正 */
static Node *avl_insert(Node *root, int val)
{
    if (!root)
        return new_node(val);

    if (val < root->val)
        root->left = avl_insert(root->left, val);
    else
        root->right = avl_insert(root->right, val);

    update_height(root);
    int balance = balance_factor(root);

    if (balance > 1 && val < root->left->val)
        return rotate_right(root);
    if (balance < -1 && val >= root->right->val)
        return rotate_left(root);
    if (balance > 1 && val >= root->left->val) {
        root->left = rotate_left(root->left);
        return rotate_right(root);
    }
    if (balance < -1 && val < root->right->val) {
        root->right = rotate_right(root->right);
        return rotate_left(root);
    }
    return root;
}

/* 普通 BST 插入：不做任何平衡处理，最坏情况会退化成链表 */
static Node *bst_insert(Node *root, int val)
{
    if (!root)
        return new_node(val);
    if (val < root->val)
        root->left = bst_insert(root->left, val);
    else
        root->right = bst_insert(root->right, val);
    root->height = 1 + max_int(height_of(root->left), height_of(root->right));
    return root;
}

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
    const int n = 15;
    int values[15];
    for (int i = 0; i < n; i++)
        values[i] = i + 1; /* 递增序列：普通 BST 的最坏情况 */

    Node *bst_root = NULL;
    Node *avl_root = NULL;
    for (int i = 0; i < n; i++) {
        bst_root = bst_insert(bst_root, values[i]);
        avl_root = avl_insert(avl_root, values[i]);
    }

    int result[15];
    int idx = 0;
    inorder(avl_root, result, &idx);

    printf("=== 二叉搜索树：简化版 AVL 旋转，维持平衡而不是重新排序 ===\n");
    printf("按递增顺序插入 1..%d（普通 BST 的最坏情况）\n", n);
    printf("普通 BST 高度 = %d（最坏情况下退化成链表，高度 = n）\n", height_of(bst_root));
    printf("AVL   树高度  = %d（始终维持在 O(log n) 量级）\n", height_of(avl_root));

    printf("AVL 中序遍历结果: ");
    for (int i = 0; i < n; i++)
        printf("%d ", result[i]);
    printf("\n\n说明：面对同样的递增插入序列，AVL 通过旋转始终把树高度控制在\n"
           "O(log n)，而普通 BST 会退化成一条链表（高度 = n）。这正是\n"
           "'维持平衡，而不是重新排序'这一最佳实践的量化体现。\n");

    free_tree(bst_root);
    free_tree(avl_root);
    return 0;
}
