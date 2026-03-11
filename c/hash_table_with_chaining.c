#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 10

// 1. 定义链表节点
typedef struct Node {
    char *key;
    int value;
    struct Node *next;
} Node;

// 2. 定义哈希表结构
typedef struct {
    Node *buckets[TABLE_SIZE];
} HashTable;

// 3. 初始化哈希表
void init_table(HashTable *ht) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        ht->buckets[i] = NULL;
    }
}

// 4. 哈希函数 (简单的求余法)
unsigned int hash(char *key) {
    unsigned int hash_val = 0;
    while (*key) {
        hash_val += *key++;
    }
    return hash_val % TABLE_SIZE;
}

// 5. 插入或更新操作 (核心逻辑)
void insert(HashTable *ht, char *key, int value) {
    unsigned int index = hash(key);
    Node *curr = ht->buckets[index];

    // --- 关键逻辑：检查 Key 是否已存在 (处理重复键) ---
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            curr->value = value; // 找到重复键，执行更新
            return;
        }
        curr = curr->next;
    }

    // --- 关键逻辑：处理哈希冲突 (头插法插入新节点) ---
    Node *new_node = malloc(sizeof(Node));
    if (!new_node) return; // 内存申请失败处理

    new_node->key = strdup(key); // 拷贝键字符串
    new_node->value = value;
    new_node->next = ht->buckets[index]; // 新节点指向当前链表头
    ht->buckets[index] = new_node;       // 更新槽位指向新节点
}

// 6. 查找操作
int search(HashTable *ht, char *key) {
    unsigned int index = hash(key);
    Node *curr = ht->buckets[index];

    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            return curr->value;
        }
        curr = curr->next;
    }
    return -1; // 表示未找到
}

// 7. 释放内存
void free_table(HashTable *ht) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        Node *curr = ht->buckets[i];
        while (curr) {
            Node *temp = curr;
            curr = curr->next;
            free(temp->key);
            free(temp);
        }
        ht->buckets[i] = NULL;
    }
}

// 8. 打印哈希表结构 (用于观察冲突和链表状态)
void print_table(HashTable *ht) {
    printf("\n--- Hash Table Structure ---\n");
    for (int i = 0; i < TABLE_SIZE; i++) {
        printf("Bucket[%d]: ", i);
        Node *curr = ht->buckets[i];
        while (curr) {
            printf("[%s: %d] -> ", curr->key, curr->value);
            curr = curr->next;
        }
        printf("NULL\n");
    }
    printf("---------------------------\n");
}

int main() {
    HashTable my_table;
    init_table(&my_table);

    // 插入数据
    insert(&my_table, "apple", 100);
    insert(&my_table, "banana", 200);  // 假设与某个键冲突
    insert(&my_table, "orange", 300);
    
    // 演示更新操作 (触发 while 循环逻辑)
    insert(&my_table, "apple", 999); 

    // 打印当前结构
    print_table(&my_table);

    // 查询演示
    printf("Search 'apple': %d\n", search(&my_table, "apple"));
    printf("Search 'grape': %d\n", search(&my_table, "grape"));

    // 释放资源
    free_table(&my_table);

    return 0;
}

/*
Keys (Input Strings)
+----------+    +----------+    +----------+    +----------+
| "apple"  |    | "banana" |    | "orange" |    | "apple"  | (Update)
+----------+    +----------+    +----------+    +----------+
      |               |               |               |
      v               v               v               v
   hash() % TABLE_SIZE (e.g., 10)     |           Finds existing
      |               |               |           "apple" node,
      v               v               v           updates value.
   
HashTable->buckets[10] (Array of pointers)
+------+------+------+------+------+------+------+------+
| [0]  | [1]  | [2]  | [3]  | [4]  | [5]  | ...  | [9]  |
+------+------+------+------+------+------+------+------+
  |      |      |      |      |      |               |
 nil    nil    nil    nil    nil     |              nil
                                     |
                                     v
                               +-----------+
                               | key: "app"|
                               | val: 999  |
                               | next -----+---> +-----------+
                               +-----------+     | key: "ban"|
                                                 | val: 200  |
                                                 | next -----+---> nil
                                                 +-----------+

             (Chained collision list via "next" pointer)
             (Note: "apple" and "banana" both hashed to index 5)

关键点解释：
1. Buckets 数组：这是一个固定大小（TABLE_SIZE）的指针数组。每个元素 buckets[i] 都是一个链表的头指针。
2. 哈希冲突 (Collision)：图中假设 "apple" 和 "banana" 经过哈希计算后都指向了 index 5。
3. 链表 (Chaining)：通过 Node 结构体中的 next 指针，我们将冲突的节点连接起来。
4. 去重更新 (Update)：第二次插入 "apple" 时，while 循环会遍历 index 5 的链表。发现 "apple" 已存在后，直接修改其 value 为 999，而不会创建一个新的 "apple" 节点挂在链表上。
*/