/*
 * 场景：二叉搜索树（BST）—— 中序遍历天然有序
 * 心智模型：树本身已经有序，不需要"整体排序"，中序遍历即可得到有序序列。
 */
#include <iostream>
#include <vector>

struct Node {
    int val;
    Node *left;
    Node *right;
    explicit Node(int v) : val(v), left(nullptr), right(nullptr) {}
};

static Node *insert(Node *root, int val)
{
    if (!root)
        return new Node(val);
    if (val < root->val)
        root->left = insert(root->left, val);
    else
        root->right = insert(root->right, val);
    return root;
}

// 中序遍历：左 -> 根 -> 右，对 BST 而言天然按升序访问
static void inorder(Node *root, std::vector<int> &result)
{
    if (!root)
        return;
    inorder(root->left, result);
    result.push_back(root->val);
    inorder(root->right, result);
}

static void free_tree(Node *root)
{
    if (!root)
        return;
    free_tree(root->left);
    free_tree(root->right);
    delete root;
}

int main()
{
    std::vector<int> values = {50, 30, 70, 20, 40, 60, 80, 35, 65};

    Node *root = nullptr;
    for (int v : values)
        root = insert(root, v);

    std::cout << "=== BST：中序遍历天然有序 ===\n";
    std::cout << "插入顺序: ";
    for (int v : values)
        std::cout << v << " ";
    std::cout << "\n";

    std::vector<int> result;
    inorder(root, result);

    std::cout << "中序遍历结果: ";
    for (int v : result)
        std::cout << v << " ";
    std::cout << "\n\n说明：从未对数据做过'排序'这个动作，BST 的结构本身就保证了\n"
                 "中序遍历得到的序列一定是升序的，这就是'树天然有序'。\n";

    free_tree(root);
    return 0;
}
