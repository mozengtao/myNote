/*
 * 场景：二叉搜索树 —— 借助中序遍历做 Top-K / 区间查询
 * 心智模型：树已经是有序的，查询"最小的 K 个"或"某个区间内的元素"
 *          不需要先导出全部数据再排序，只需要有选择地遍历/剪枝。
 */
#include <iostream>
#include <vector>

struct Node {
    int val;
    Node *left;
    Node *right;
    explicit Node(int v) : val(v), left(nullptr), right(nullptr) {}
};

static long g_visited_nodes = 0;

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

// Top-K：中序遍历本身就是升序，收集到 k 个就可以提前停止
static void k_smallest(Node *root, int k, std::vector<int> &result)
{
    if (!root || static_cast<int>(result.size()) >= k)
        return;
    g_visited_nodes++;
    k_smallest(root->left, k, result);
    if (static_cast<int>(result.size()) < k)
        result.push_back(root->val);
    k_smallest(root->right, k, result);
}

// 区间查询 [lo, hi]：利用 BST 的有序性剪枝，跳过不可能落在区间内的子树
static void range_query(Node *root, int lo, int hi, std::vector<int> &result)
{
    if (!root)
        return;
    g_visited_nodes++;
    if (root->val > lo)
        range_query(root->left, lo, hi, result);
    if (root->val >= lo && root->val <= hi)
        result.push_back(root->val);
    if (root->val < hi)
        range_query(root->right, lo, hi, result);
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
    std::vector<int> values = {50, 30, 70, 20, 40, 60, 80, 10, 25, 35, 45, 55, 65, 75, 90};

    Node *root = nullptr;
    for (int v : values)
        root = insert(root, v);

    std::cout << "=== BST：借助中序遍历做 Top-K / 区间查询 ===\n";
    std::cout << "树中共有 " << values.size() << " 个节点\n\n";

    int k = 4;
    std::vector<int> topk;
    g_visited_nodes = 0;
    k_smallest(root, k, topk);
    std::cout << "最小的 " << k << " 个值: ";
    for (int v : topk)
        std::cout << v << " ";
    std::cout << "(访问节点数 = " << g_visited_nodes << "，总节点数 = " << values.size() << ")\n";

    int lo = 30, hi = 60;
    std::vector<int> range_result;
    g_visited_nodes = 0;
    range_query(root, lo, hi, range_result);
    std::cout << "区间 [" << lo << ", " << hi << "] 内的值: ";
    for (int v : range_result)
        std::cout << v << " ";
    std::cout << "(访问节点数 = " << g_visited_nodes << "，总节点数 = " << values.size() << ")\n";

    std::cout << "\n说明：两个查询都没有把全部节点取出来再排序，\n"
                 "而是利用 BST 天然有序的特性做剪枝遍历，访问节点数明显少于总节点数。\n";

    free_tree(root);
    return 0;
}
