#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 10

// 1. 通用节点定义
typedef struct Node {
    char *key;
    void *value;        // 指向任意类型数据的指针
    struct Node *next;
} Node;

// 2. 哈希表结构
typedef struct {
    Node *buckets[TABLE_SIZE];
    void (*free_value)(void *); // 用户自定义的数据释放函数
} HashTable;

// 3. 初始化 (需要传入值释放回调)
void init_table(HashTable *ht, void (*free_func)(void *)) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        ht->buckets[i] = NULL;
    }
    ht->free_value = free_func;
}

// 4. 哈希函数
unsigned int hash(const char *key) {
    unsigned int hash_val = 0;
    while (*key) hash_val = (hash_val << 5) + *key++; 
    return hash_val % TABLE_SIZE;
}

// 5. 插入/更新 (支持 void *value)
void insert(HashTable *ht, char *key, void *value) {
    unsigned int index = hash(key);
    Node *curr = ht->buckets[index];

    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            // 如果存在旧值且有释放函数，先释放旧值
            if (ht->free_value) ht->free_value(curr->value);
            curr->value = value;
            return;
        }
        curr = curr->next;
    }

    Node *new_node = malloc(sizeof(Node));
    new_node->key = strdup(key);
    new_node->value = value; // 存储通用指针
    new_node->next = ht->buckets[index];
    ht->buckets[index] = new_node;
}

// 6. 查找 (返回 void *)
void *search(HashTable *ht, char *key) {
    unsigned int index = hash(key);
    Node *curr = ht->buckets[index];
    while (curr) {
        if (strcmp(curr->key, key) == 0) return curr->value;
        curr = curr->next;
    }
    return NULL;
}

// 7. 释放内存
void free_table(HashTable *ht) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        Node *curr = ht->buckets[i];
        while (curr) {
            Node *temp = curr;
            curr = curr->next;
            if (ht->free_value) ht->free_value(temp->value);
            free(temp->key);
            free(temp);
        }
        ht->buckets[i] = NULL;
    }
}

/*
Generic HashTable Layout
+---------------------+
| free_value pointer  | ----> [ User's Custom Free Function ]
+---------------------+
| buckets[TABLE_SIZE] |
+----------+----------+
           |
           v Index [k]
     +----------+         +----------+
     | key:str  |         | key:str  |
     | value:void*------> | [ User   | (e.g., struct Student,
     | next:node*---+     |  Data ]   |  double, array, etc.)
     +----------+   |     +----------+
                    v
              +----------+
              | key:str  |
              | value:void*------> [ Another Type of Data ]
              | next:nil |
              +----------+
*/

typedef struct {
    int id;
    char name[20];
} Student;

// 用户定义的释放逻辑（如果是静态分配或无需释放，可传 NULL）
void my_free_student(void *ptr) {
    free(ptr);
}

int main() {
    HashTable school_db;
    init_table(&school_db, my_free_student);

    // 存储 Student 结构体
    Student *s1 = malloc(sizeof(Student));
    s1->id = 1001; strcpy(s1->name, "Alice");
    insert(&school_db, "class_a", s1);

    // 查找并转换类型
    Student *res = (Student *)search(&school_db, "class_a");
    if (res) printf("Found: %s (ID: %d)\n", res->name, res->id);

    free_table(&school_db);
    return 0;
}