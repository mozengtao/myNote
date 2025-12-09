# 组合模式 (Composite Pattern)

## 核心思想

```
+------------------------------------------------------------------+
|                      COMPOSITE PATTERN                            |
+------------------------------------------------------------------+

    TREE STRUCTURE:
    
                        +------------------+
                        |   Root (Menu)    |
                        |   "Main Menu"    |
                        +--------+---------+
                                 |
            +--------------------+--------------------+
            |                    |                    |
    +-------v------+     +-------v------+     +-------v------+
    |   Composite  |     |    Leaf      |     |   Composite  |
    |  "File Menu" |     |   "Help"     |     |  "Edit Menu" |
    +-------+------+     +--------------+     +-------+------+
            |                                         |
    +-------+-------+                         +-------+-------+
    |       |       |                         |       |       |
    v       v       v                         v       v       v
  +---+   +---+   +---+                     +---+   +---+   +---+
  |New|   |Opn|   |Sav|                     |Cut|   |Cpy|   |Pst|
  +---+   +---+   +---+                     +---+   +---+   +---+
  Leaf    Leaf    Leaf                      Leaf   Leaf    Leaf


    UNIFORM INTERFACE:
    
    +--------------------------------------------------+
    |                 component_ops_t                   |
    |  +--------------------------------------------+  |
    |  | execute()    - Do operation                |  |
    |  | add()        - Add child (composite only)  |  |
    |  | remove()     - Remove child                |  |
    |  | get_child()  - Get child by index          |  |
    |  +--------------------------------------------+  |
    +--------------------------------------------------+
             ^                          ^
             |                          |
    +--------+--------+        +--------+--------+
    |      Leaf       |        |    Composite    |
    | (No children)   |        | (Has children)  |
    |  execute() {    |        |  execute() {    |
    |    do_action(); |        |    for each     |
    |  }              |        |      child      |
    |  add() = NULL   |        |    child->exec()|
    +-----------------+        |  }              |
                               +-----------------+


    RECURSIVE EXECUTION:
    
    execute(Root)
        |
        +--> execute(File Menu)
        |        |
        |        +--> execute(New)   --> action
        |        +--> execute(Open)  --> action
        |        +--> execute(Save)  --> action
        |
        +--> execute(Help)           --> action
        |
        +--> execute(Edit Menu)
                 |
                 +--> execute(Cut)   --> action
                 +--> execute(Copy)  --> action
                 +--> execute(Paste) --> action
```

**核心思想说明：**
- 将对象组合成树形结构表示"部分-整体"的层次结构
- 组合（Composite）和叶子（Leaf）实现相同接口
- 客户端可以统一处理单个对象和组合对象
- 递归遍历整个树形结构

## 实现思路

1. **定义公共接口**：组合和叶子共用的操作接口
2. **叶子节点**：实现基本操作，不包含子节点
3. **组合节点**：包含子节点列表，递归调用子节点操作
4. **统一访问**：客户端不区分叶子和组合

## 典型应用场景

- 文件系统（文件和文件夹）
- GUI组件树
- 菜单系统
- 组织架构
- 表达式树

## 完整代码示例

```c
/*============================================================================
 * 组合模式示例 - 文件系统
 *============================================================================*/

/*---------------------------------------------------------------------------
 * filesystem.h - 公共接口定义
 *---------------------------------------------------------------------------*/
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* 节点类型 */
typedef enum {
    NODE_FILE,
    NODE_DIRECTORY
} node_type_t;

/* 关键点：公共接口 - 文件和目录都实现此接口 */
typedef struct fs_node fs_node_t;

typedef struct {
    void (*display)(fs_node_t *node, int indent);
    size_t (*get_size)(fs_node_t *node);
    int (*search)(fs_node_t *node, const char *name, fs_node_t **result);
    void (*destroy)(fs_node_t *node);
    
    /* 组合节点特有操作（叶子节点返回错误） */
    bool (*add)(fs_node_t *node, fs_node_t *child);
    bool (*remove)(fs_node_t *node, const char *name);
    fs_node_t* (*get_child)(fs_node_t *node, int index);
    int (*get_child_count)(fs_node_t *node);
} fs_node_ops_t;

struct fs_node {
    node_type_t type;
    char name[64];
    const fs_node_ops_t *ops;
    void *data;
};

/* 创建函数 */
fs_node_t* file_create(const char *name, size_t size);
fs_node_t* directory_create(const char *name);

/* 便捷函数 */
void fs_display(fs_node_t *node);
size_t fs_get_total_size(fs_node_t *node);
fs_node_t* fs_search(fs_node_t *root, const char *name);

#endif /* FILESYSTEM_H */


/*---------------------------------------------------------------------------
 * file.c - 叶子节点实现（文件）
 *---------------------------------------------------------------------------*/
#include "filesystem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    size_t size;
    char extension[16];
} file_data_t;

static void file_display(fs_node_t *node, int indent) {
    file_data_t *data = (file_data_t *)node->data;
    
    /* 打印缩进 */
    for (int i = 0; i < indent; i++) printf("  ");
    
    printf("📄 %s (%zu bytes)\n", node->name, data->size);
}

static size_t file_get_size(fs_node_t *node) {
    file_data_t *data = (file_data_t *)node->data;
    return data->size;
}

static int file_search(fs_node_t *node, const char *name, fs_node_t **result) {
    if (strcmp(node->name, name) == 0) {
        *result = node;
        return 1;
    }
    return 0;
}

static void file_destroy(fs_node_t *node) {
    if (node != NULL) {
        free(node->data);
        free(node);
    }
}

/* 关键点：叶子节点的组合操作返回失败 */
static bool file_add(fs_node_t *node, fs_node_t *child) {
    (void)node; (void)child;
    printf("Error: Cannot add child to a file!\n");
    return false;
}

static bool file_remove(fs_node_t *node, const char *name) {
    (void)node; (void)name;
    printf("Error: Cannot remove child from a file!\n");
    return false;
}

static fs_node_t* file_get_child(fs_node_t *node, int index) {
    (void)node; (void)index;
    return NULL;
}

static int file_get_child_count(fs_node_t *node) {
    (void)node;
    return 0;
}

static const fs_node_ops_t file_ops = {
    .display = file_display,
    .get_size = file_get_size,
    .search = file_search,
    .destroy = file_destroy,
    .add = file_add,
    .remove = file_remove,
    .get_child = file_get_child,
    .get_child_count = file_get_child_count
};

fs_node_t* file_create(const char *name, size_t size) {
    fs_node_t *node = malloc(sizeof(fs_node_t));
    file_data_t *data = malloc(sizeof(file_data_t));
    
    if (node == NULL || data == NULL) {
        free(node);
        free(data);
        return NULL;
    }
    
    node->type = NODE_FILE;
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->ops = &file_ops;
    
    data->size = size;
    node->data = data;
    
    return node;
}


/*---------------------------------------------------------------------------
 * directory.c - 组合节点实现（目录）
 *---------------------------------------------------------------------------*/
#define MAX_CHILDREN 32

typedef struct {
    fs_node_t *children[MAX_CHILDREN];
    int child_count;
} directory_data_t;

static void directory_display(fs_node_t *node, int indent) {
    directory_data_t *data = (directory_data_t *)node->data;
    
    /* 打印缩进 */
    for (int i = 0; i < indent; i++) printf("  ");
    
    printf("📁 %s/\n", node->name);
    
    /* 关键点：递归显示所有子节点 */
    for (int i = 0; i < data->child_count; i++) {
        data->children[i]->ops->display(data->children[i], indent + 1);
    }
}

/* 关键点：递归计算总大小 */
static size_t directory_get_size(fs_node_t *node) {
    directory_data_t *data = (directory_data_t *)node->data;
    size_t total = 0;
    
    for (int i = 0; i < data->child_count; i++) {
        total += data->children[i]->ops->get_size(data->children[i]);
    }
    
    return total;
}

/* 关键点：递归搜索 */
static int directory_search(fs_node_t *node, const char *name, fs_node_t **result) {
    directory_data_t *data = (directory_data_t *)node->data;
    
    /* 检查自己 */
    if (strcmp(node->name, name) == 0) {
        *result = node;
        return 1;
    }
    
    /* 递归搜索子节点 */
    for (int i = 0; i < data->child_count; i++) {
        if (data->children[i]->ops->search(data->children[i], name, result)) {
            return 1;
        }
    }
    
    return 0;
}

/* 关键点：递归销毁 */
static void directory_destroy(fs_node_t *node) {
    if (node == NULL) return;
    
    directory_data_t *data = (directory_data_t *)node->data;
    
    /* 先销毁所有子节点 */
    for (int i = 0; i < data->child_count; i++) {
        data->children[i]->ops->destroy(data->children[i]);
    }
    
    free(data);
    free(node);
}

/* 关键点：组合节点可以添加子节点 */
static bool directory_add(fs_node_t *node, fs_node_t *child) {
    directory_data_t *data = (directory_data_t *)node->data;
    
    if (data->child_count >= MAX_CHILDREN) {
        printf("Error: Directory is full!\n");
        return false;
    }
    
    data->children[data->child_count++] = child;
    return true;
}

static bool directory_remove(fs_node_t *node, const char *name) {
    directory_data_t *data = (directory_data_t *)node->data;
    
    for (int i = 0; i < data->child_count; i++) {
        if (strcmp(data->children[i]->name, name) == 0) {
            /* 销毁子节点 */
            data->children[i]->ops->destroy(data->children[i]);
            
            /* 移动后续元素 */
            for (int j = i; j < data->child_count - 1; j++) {
                data->children[j] = data->children[j + 1];
            }
            data->child_count--;
            return true;
        }
    }
    return false;
}

static fs_node_t* directory_get_child(fs_node_t *node, int index) {
    directory_data_t *data = (directory_data_t *)node->data;
    
    if (index >= 0 && index < data->child_count) {
        return data->children[index];
    }
    return NULL;
}

static int directory_get_child_count(fs_node_t *node) {
    directory_data_t *data = (directory_data_t *)node->data;
    return data->child_count;
}

static const fs_node_ops_t directory_ops = {
    .display = directory_display,
    .get_size = directory_get_size,
    .search = directory_search,
    .destroy = directory_destroy,
    .add = directory_add,
    .remove = directory_remove,
    .get_child = directory_get_child,
    .get_child_count = directory_get_child_count
};

fs_node_t* directory_create(const char *name) {
    fs_node_t *node = malloc(sizeof(fs_node_t));
    directory_data_t *data = malloc(sizeof(directory_data_t));
    
    if (node == NULL || data == NULL) {
        free(node);
        free(data);
        return NULL;
    }
    
    node->type = NODE_DIRECTORY;
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->ops = &directory_ops;
    
    memset(data, 0, sizeof(directory_data_t));
    node->data = data;
    
    return node;
}


/*---------------------------------------------------------------------------
 * 便捷函数
 *---------------------------------------------------------------------------*/
void fs_display(fs_node_t *node) {
    if (node != NULL) {
        node->ops->display(node, 0);
    }
}

size_t fs_get_total_size(fs_node_t *node) {
    if (node != NULL) {
        return node->ops->get_size(node);
    }
    return 0;
}

fs_node_t* fs_search(fs_node_t *root, const char *name) {
    fs_node_t *result = NULL;
    if (root != NULL) {
        root->ops->search(root, name, &result);
    }
    return result;
}


/*---------------------------------------------------------------------------
 * 使用示例 - main.c
 *---------------------------------------------------------------------------*/
#include "filesystem.h"
#include <stdio.h>

int main(void) {
    printf("=== Composite Pattern Demo ===\n\n");
    
    /* 关键点：构建树形结构 */
    printf("--- Building file system tree ---\n\n");
    
    /* 根目录 */
    fs_node_t *root = directory_create("root");
    
    /* 子目录 */
    fs_node_t *docs = directory_create("documents");
    fs_node_t *pics = directory_create("pictures");
    fs_node_t *src = directory_create("source");
    
    /* 文件 */
    fs_node_t *readme = file_create("readme.txt", 1024);
    fs_node_t *report = file_create("report.pdf", 2048576);
    fs_node_t *photo1 = file_create("vacation.jpg", 3145728);
    fs_node_t *photo2 = file_create("family.png", 2097152);
    fs_node_t *main_c = file_create("main.c", 4096);
    fs_node_t *utils_c = file_create("utils.c", 2048);
    fs_node_t *header = file_create("header.h", 512);
    
    /* 组装树结构 */
    root->ops->add(root, docs);
    root->ops->add(root, pics);
    root->ops->add(root, src);
    root->ops->add(root, readme);
    
    docs->ops->add(docs, report);
    
    pics->ops->add(pics, photo1);
    pics->ops->add(pics, photo2);
    
    src->ops->add(src, main_c);
    src->ops->add(src, utils_c);
    src->ops->add(src, header);
    
    /* 关键点：统一操作 - 无论是文件还是目录 */
    printf("--- File System Structure ---\n\n");
    fs_display(root);
    
    printf("\n--- Total Size Calculation ---\n");
    size_t total = fs_get_total_size(root);
    printf("Total size: %zu bytes (%.2f MB)\n", total, (float)total / 1048576);
    
    printf("\n--- Size of 'pictures' directory ---\n");
    size_t pics_size = fs_get_total_size(pics);
    printf("Pictures size: %zu bytes (%.2f MB)\n", pics_size, (float)pics_size / 1048576);
    
    printf("\n--- Search for 'main.c' ---\n");
    fs_node_t *found = fs_search(root, "main.c");
    if (found != NULL) {
        printf("Found: %s (%zu bytes)\n", found->name, found->ops->get_size(found));
    }
    
    printf("\n--- Remove 'vacation.jpg' ---\n");
    pics->ops->remove(pics, "vacation.jpg");
    printf("New pictures size: %zu bytes\n", fs_get_total_size(pics));
    
    printf("\n--- Updated Structure ---\n\n");
    fs_display(root);
    
    /* 清理 */
    printf("\n--- Cleanup ---\n");
    root->ops->destroy(root);
    printf("File system destroyed.\n");
    
    return 0;
}
```

## 运行输出示例

```
=== Composite Pattern Demo ===

--- Building file system tree ---

--- File System Structure ---

📁 root/
  📁 documents/
    📄 report.pdf (2048576 bytes)
  📁 pictures/
    📄 vacation.jpg (3145728 bytes)
    📄 family.png (2097152 bytes)
  📁 source/
    📄 main.c (4096 bytes)
    📄 utils.c (2048 bytes)
    📄 header.h (512 bytes)
  📄 readme.txt (1024 bytes)

--- Total Size Calculation ---
Total size: 7299136 bytes (6.96 MB)

--- Size of 'pictures' directory ---
Pictures size: 5242880 bytes (5.00 MB)

--- Search for 'main.c' ---
Found: main.c (4096 bytes)

--- Remove 'vacation.jpg' ---
New pictures size: 2097152 bytes

--- Updated Structure ---

📁 root/
  📁 documents/
    📄 report.pdf (2048576 bytes)
  📁 pictures/
    📄 family.png (2097152 bytes)
  📁 source/
    📄 main.c (4096 bytes)
    📄 utils.c (2048 bytes)
    📄 header.h (512 bytes)
  📄 readme.txt (1024 bytes)

--- Cleanup ---
File system destroyed.
```

## 优势分析

| 优势 | 说明 |
|------|------|
| **统一接口** | 文件和目录使用相同操作方式 |
| **递归处理** | 自动遍历整个树形结构 |
| **易于扩展** | 新增节点类型只需实现接口 |
| **层次清晰** | 自然表示部分-整体关系 |
| **简化客户端** | 不需要区分叶子和组合 |

