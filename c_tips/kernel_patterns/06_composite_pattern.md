# Composite Pattern in Linux Kernel

## 1. Pattern Overview

```
+------------------------------------------------------------------+
|                     COMPOSITE PATTERN                             |
+------------------------------------------------------------------+
|                                                                   |
|    +------------------+                                           |
|    |    Component     |<----------------------------------+       |
|    +------------------+                                   |       |
|    | + operation()    |                                   |       |
|    +--------+---------+                                   |       |
|             ^                                             |       |
|             |                                             |       |
|    +--------+--------+                                    |       |
|    |                 |                                    |       |
|    v                 v                                    |       |
| +------+      +-------------+                             |       |
| | Leaf |      |  Composite  |-----------------------------+       |
| +------+      +-------------+        (contains children)          |
| |op()  |      | - children[]|                                     |
| +------+      | + add()     |                                     |
|               | + remove()  |                                     |
|               | + op() {    |                                     |
|               |   for child |                                     |
|               |     child   |                                     |
|               |     ->op(); |                                     |
|               | }           |                                     |
|               +-------------+                                     |
|                                                                   |
|    Tree structure: Composites contain Leaves or other Composites  |
|    Uniform interface for both individual and composite objects    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 组合模式将对象组合为树形结构，统一处理单个对象和组合对象。在Linux内核中，设备树(Device Tree)、文件系统目录结构、kobject层次结构都是组合模式的典型应用。组合对象包含子对象列表，递归调用子对象的操作。

---

## 2. Linux Kernel Implementation

### 2.1 Kernel Example: Device Tree Structure

```c
/* From: include/linux/of.h */

/**
 * struct device_node - Device tree node (Component)
 *
 * Represents nodes in the device tree, which forms a tree structure.
 * Each node can have children (composite) or be a leaf.
 */
struct device_node {
    const char *name;              /* Node name */
    const char *type;              /* Node type */
    phandle phandle;               /* Unique handle */
    char *full_name;               /* Full path name */

    struct property *properties;    /* Node properties */
    struct property *deadprops;     /* Removed properties */
    
    /* Tree structure - COMPOSITE PATTERN */
    struct device_node *parent;     /* Parent node */
    struct device_node *child;      /* First child (composite has children) */
    struct device_node *sibling;    /* Next sibling */
    struct device_node *next;       /* Next of same type */
    struct device_node *allnext;    /* Next in list of all nodes */
    
    struct kref kref;              /* Reference count */
    unsigned long _flags;
    void *data;
};

/* Traversing the tree - uniform operation on all nodes */
#define for_each_child_of_node(parent, child) \
    for (child = of_get_next_child(parent, NULL); child != NULL; \
         child = of_get_next_child(parent, child))
```

### 2.2 Kernel Example: kobject Hierarchy

```c
/* From: include/linux/kobject.h */

/**
 * struct kobject - Kernel object (Component in composite pattern)
 *
 * Forms the basis of the sysfs hierarchy.
 * kobjects can contain other kobjects (ksets).
 */
struct kobject {
    const char          *name;
    struct list_head    entry;        /* Sibling list */
    struct kobject      *parent;      /* Parent kobject */
    struct kset         *kset;        /* Containing kset */
    struct kobj_type    *ktype;       /* Type information */
    struct sysfs_dirent *sd;          /* Sysfs directory */
    struct kref         kref;         /* Reference count */
    /* ... */
};

/**
 * struct kset - Collection of kobjects (Composite)
 *
 * A kset is both a kobject and a container for other kobjects.
 */
struct kset {
    struct list_head list;            /* List of contained kobjects */
    spinlock_t list_lock;
    struct kobject kobj;              /* Embedded kobject */
    const struct kset_uevent_ops *uevent_ops;
};

/* Operations on the tree */
void kobject_get(struct kobject *kobj);
void kobject_put(struct kobject *kobj);

/* Iterate over all kobjects in a kset */
#define list_for_each_entry(pos, head, member) \
    for (pos = list_first_entry(head, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_next_entry(pos, member))
```

### 2.3 Kernel Example: Device Hierarchy

```c
/* From: include/linux/device.h */

/**
 * struct device - The basic device structure
 *
 * Devices form a tree structure where buses contain devices,
 * and devices can have child devices.
 */
struct device {
    struct device       *parent;      /* Parent device (composite) */
    struct device_private *p;
    struct kobject kobj;              /* Embedded kobject */
    const char          *init_name;
    const struct device_type *type;
    
    struct bus_type     *bus;         /* Bus this device is on */
    struct device_driver *driver;
    
    /* Children list for composite behavior */
    struct klist_node   knode_class;
    struct class        *class;
    /* ... */
};

/**
 * device_for_each_child - Iterate over device's children
 * @parent: Parent device
 * @data: Data to pass to callback
 * @fn: Callback function
 *
 * Composite pattern: Apply operation to all children.
 */
int device_for_each_child(struct device *parent, void *data,
                          int (*fn)(struct device *dev, void *data))
{
    struct klist_iter i;
    struct device *child;
    int error = 0;

    klist_iter_init(&parent->p->klist_children, &i);
    while ((child = next_device(&i)) && !error)
        error = fn(child, data);
    klist_iter_exit(&i);
    return error;
}
```

### 2.4 Architecture Diagram

```
+------------------------------------------------------------------+
|               LINUX KERNEL COMPOSITE PATTERN                      |
|                    (Device Tree Example)                          |
+------------------------------------------------------------------+
|                                                                   |
|    Device Tree Structure                                          |
|                                                                   |
|                    +--------+                                     |
|                    |   /    |  Root Node (Composite)              |
|                    +---+----+                                     |
|                        |                                          |
|         +--------------+--------------+                           |
|         |              |              |                           |
|    +----v----+    +----v----+    +----v----+                      |
|    |  cpus   |    | memory  |    |   soc   |  (Composites)        |
|    +----+----+    +---------+    +----+----+                      |
|         |         (Leaf)              |                           |
|    +----+----+                   +----+----+----+                 |
|    |         |                   |    |    |    |                 |
|    v         v                   v    v    v    v                 |
| +-----+  +-----+              +----+ +--+ +---+ +----+            |
| |cpu0 |  |cpu1 |              |uart| |i2c| |spi| |gpio|           |
| +-----+  +-----+              +----+ +--+ +---+ +----+            |
| (Leaf)   (Leaf)               (Leaves - actual devices)          |
|                                                                   |
|    Uniform Interface:                                             |
|    - of_get_next_child(node, prev) - iterate children             |
|    - of_find_node_by_name(parent, name) - search tree             |
|    - of_property_read_u32(node, prop, val) - read property        |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Linux设备树是组合模式的典型应用。根节点包含多个子节点（如cpus、memory、soc），每个子节点可以是叶子节点（如cpu0、uart）或组合节点（如cpus包含多个cpu）。所有节点共享统一接口，可以递归遍历、搜索和读取属性，无论是单个设备还是设备组。

---

## 3. Advantages Analysis

| Advantage | Description |
|-----------|-------------|
| **Uniform Interface** | Same operations work on leaves and composites |
| **Recursive Structure** | Natural representation of hierarchies |
| **Simplified Client** | Client treats all objects uniformly |
| **Easy Traversal** | Standard iteration over tree structures |
| **Dynamic Structure** | Add/remove nodes at runtime |
| **Scalability** | Deep hierarchies handled naturally |

**中文说明：** 组合模式的优势包括：统一接口（相同操作适用于叶子和组合对象）、递归结构（自然表示层次关系）、简化客户端（客户端统一处理所有对象）、易于遍历（标准化的树遍历）、动态结构（运行时添加/删除节点）、可扩展性（自然处理深层次结构）。

---

## 4. User-Space Implementation Example

```c
/*
 * Composite Pattern - User Space Implementation
 * Mimics Linux Kernel's Device Tree / kobject hierarchy
 * 
 * Compile: gcc -o composite composite.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Component Interface - Uniform interface for all nodes
 * Similar to device_node in kernel
 * ============================================================ */

/* Forward declaration */
struct fs_node;

/* Node operations - same interface for files and directories */
struct fs_ops {
    void (*print)(struct fs_node *node, int depth);
    int (*get_size)(struct fs_node *node);
    void (*destroy)(struct fs_node *node);
};

/* Node types */
enum node_type {
    NODE_FILE,       /* Leaf */
    NODE_DIRECTORY   /* Composite */
};

/* Base node structure (Component) */
struct fs_node {
    char name[64];                  /* Node name */
    enum node_type type;            /* Leaf or Composite */
    const struct fs_ops *ops;       /* Operations */
    
    /* Tree structure */
    struct fs_node *parent;         /* Parent node */
    struct fs_node *first_child;    /* First child (for directories) */
    struct fs_node *next_sibling;   /* Next sibling */
    
    /* Node-specific data */
    void *private_data;
};

/* ============================================================
 * Leaf Node - File Implementation
 * ============================================================ */

struct file_data {
    int size;
    char content[256];
};

static void file_print(struct fs_node *node, int depth)
{
    struct file_data *data = node->private_data;
    
    /* Print with indentation */
    for (int i = 0; i < depth; i++) printf("  ");
    printf("|- %s (file, %d bytes)\n", node->name, data->size);
}

static int file_get_size(struct fs_node *node)
{
    struct file_data *data = node->private_data;
    return data->size;
}

static void file_destroy(struct fs_node *node)
{
    free(node->private_data);
    free(node);
}

static const struct fs_ops file_ops = {
    .print = file_print,
    .get_size = file_get_size,
    .destroy = file_destroy
};

/* Create a file node (Leaf) */
struct fs_node *create_file(const char *name, int size, const char *content)
{
    struct fs_node *node = malloc(sizeof(struct fs_node));
    struct file_data *data = malloc(sizeof(struct file_data));
    
    if (!node || !data) {
        free(node);
        free(data);
        return NULL;
    }
    
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->type = NODE_FILE;
    node->ops = &file_ops;
    node->parent = NULL;
    node->first_child = NULL;  /* Files have no children */
    node->next_sibling = NULL;
    
    data->size = size;
    strncpy(data->content, content ? content : "", sizeof(data->content) - 1);
    node->private_data = data;
    
    return node;
}

/* ============================================================
 * Composite Node - Directory Implementation
 * ============================================================ */

struct dir_data {
    int child_count;
};

static void dir_print(struct fs_node *node, int depth)
{
    struct fs_node *child;
    
    /* Print directory name */
    for (int i = 0; i < depth; i++) printf("  ");
    printf("+- %s/\n", node->name);
    
    /* COMPOSITE: Recursively print all children */
    for (child = node->first_child; child; child = child->next_sibling) {
        child->ops->print(child, depth + 1);
    }
}

static int dir_get_size(struct fs_node *node)
{
    struct fs_node *child;
    int total_size = 0;
    
    /* COMPOSITE: Sum sizes of all children recursively */
    for (child = node->first_child; child; child = child->next_sibling) {
        total_size += child->ops->get_size(child);
    }
    
    return total_size;
}

static void dir_destroy(struct fs_node *node)
{
    struct fs_node *child = node->first_child;
    struct fs_node *next;
    
    /* COMPOSITE: Destroy all children first */
    while (child) {
        next = child->next_sibling;
        child->ops->destroy(child);
        child = next;
    }
    
    free(node->private_data);
    free(node);
}

static const struct fs_ops dir_ops = {
    .print = dir_print,
    .get_size = dir_get_size,
    .destroy = dir_destroy
};

/* Create a directory node (Composite) */
struct fs_node *create_directory(const char *name)
{
    struct fs_node *node = malloc(sizeof(struct fs_node));
    struct dir_data *data = malloc(sizeof(struct dir_data));
    
    if (!node || !data) {
        free(node);
        free(data);
        return NULL;
    }
    
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->type = NODE_DIRECTORY;
    node->ops = &dir_ops;
    node->parent = NULL;
    node->first_child = NULL;
    node->next_sibling = NULL;
    
    data->child_count = 0;
    node->private_data = data;
    
    return node;
}

/* ============================================================
 * Tree Operations
 * ============================================================ */

/**
 * add_child - Add a child node to a directory
 * @parent: Parent directory (must be a directory)
 * @child: Child node to add
 *
 * Similar to device_add() in kernel.
 */
int add_child(struct fs_node *parent, struct fs_node *child)
{
    struct dir_data *data;
    
    if (!parent || !child || parent->type != NODE_DIRECTORY) {
        return -1;
    }
    
    /* Set parent pointer */
    child->parent = parent;
    
    /* Add to beginning of child list */
    child->next_sibling = parent->first_child;
    parent->first_child = child;
    
    /* Update child count */
    data = parent->private_data;
    data->child_count++;
    
    return 0;
}

/**
 * find_node - Find a node by name in subtree
 * @root: Root of subtree to search
 * @name: Name to find
 *
 * Recursive search - demonstrates composite traversal.
 */
struct fs_node *find_node(struct fs_node *root, const char *name)
{
    struct fs_node *child;
    struct fs_node *result;
    
    if (!root || !name) return NULL;
    
    /* Check current node */
    if (strcmp(root->name, name) == 0) {
        return root;
    }
    
    /* If directory, search children */
    if (root->type == NODE_DIRECTORY) {
        for (child = root->first_child; child; child = child->next_sibling) {
            result = find_node(child, name);
            if (result) return result;
        }
    }
    
    return NULL;
}

/**
 * count_nodes - Count all nodes in subtree
 * @root: Root of subtree
 *
 * Demonstrates recursive operation on composite structure.
 */
int count_nodes(struct fs_node *root)
{
    struct fs_node *child;
    int count = 1;  /* Count self */
    
    if (!root) return 0;
    
    /* If directory, count children */
    if (root->type == NODE_DIRECTORY) {
        for (child = root->first_child; child; child = child->next_sibling) {
            count += count_nodes(child);
        }
    }
    
    return count;
}

/**
 * get_path - Get full path of a node
 * @node: Node to get path for
 * @buf: Buffer to store path
 * @len: Buffer length
 */
void get_path(struct fs_node *node, char *buf, int len)
{
    char temp[256] = "";
    struct fs_node *current = node;
    
    /* Walk up to root, building path in reverse */
    while (current) {
        char segment[128];
        snprintf(segment, sizeof(segment), "/%s%s", current->name, temp);
        strncpy(temp, segment, sizeof(temp) - 1);
        current = current->parent;
    }
    
    strncpy(buf, temp, len - 1);
}

/* ============================================================
 * Main - Demonstrate Composite Pattern
 * ============================================================ */

int main(void)
{
    struct fs_node *root;
    struct fs_node *home;
    struct fs_node *etc;
    struct fs_node *user1;
    struct fs_node *found;
    char path[256];

    printf("=== Composite Pattern Demo (Filesystem Tree) ===\n\n");

    /* Create the tree structure */
    printf("--- Building filesystem tree ---\n\n");
    
    /* Root directory */
    root = create_directory("root");
    
    /* /home directory with users */
    home = create_directory("home");
    add_child(root, home);
    
    user1 = create_directory("alice");
    add_child(home, user1);
    add_child(user1, create_file("document.txt", 1024, "Hello"));
    add_child(user1, create_file("photo.jpg", 2048000, NULL));
    add_child(user1, create_directory("projects"));
    
    struct fs_node *projects = find_node(root, "projects");
    add_child(projects, create_file("main.c", 5120, "int main(){}"));
    add_child(projects, create_file("Makefile", 256, "all: main"));
    
    add_child(home, create_directory("bob"));
    struct fs_node *bob = find_node(root, "bob");
    add_child(bob, create_file("notes.txt", 512, "Notes"));
    
    /* /etc directory */
    etc = create_directory("etc");
    add_child(root, etc);
    add_child(etc, create_file("passwd", 1024, "root:x:0:0"));
    add_child(etc, create_file("hosts", 256, "127.0.0.1 localhost"));
    add_child(etc, create_directory("init.d"));
    
    /* Add some files to root */
    add_child(root, create_file("README.md", 4096, "System docs"));

    /* Display the tree */
    printf("--- Filesystem Structure ---\n\n");
    root->ops->print(root, 0);
    
    /* Calculate total size (composite operation) */
    printf("\n--- Size Calculations ---\n");
    printf("Total size of /root: %d bytes\n", root->ops->get_size(root));
    printf("Total size of /root/home: %d bytes\n", home->ops->get_size(home));
    printf("Total size of /root/home/alice: %d bytes\n", 
           find_node(root, "alice")->ops->get_size(find_node(root, "alice")));
    
    /* Count nodes */
    printf("\n--- Node Counts ---\n");
    printf("Total nodes in tree: %d\n", count_nodes(root));
    printf("Nodes under /home: %d\n", count_nodes(home));
    
    /* Find nodes */
    printf("\n--- Finding Nodes ---\n");
    found = find_node(root, "main.c");
    if (found) {
        get_path(found, path, sizeof(path));
        printf("Found 'main.c' at: %s\n", path);
        printf("Size: %d bytes\n", found->ops->get_size(found));
    }
    
    found = find_node(root, "bob");
    if (found) {
        get_path(found, path, sizeof(path));
        printf("Found 'bob' at: %s\n", path);
    }
    
    /* Cleanup - destroys entire tree */
    printf("\n--- Cleanup ---\n");
    root->ops->destroy(root);
    printf("Tree destroyed\n");

    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

---

## 5. Composite Tree Traversal

```
+------------------------------------------------------------------+
|                 COMPOSITE TREE TRAVERSAL                          |
+------------------------------------------------------------------+
|                                                                   |
|    Operation: get_size(root)                                      |
|                                                                   |
|                      +--------+                                   |
|                      |  root  | dir_get_size()                    |
|                      +---+----+ sum = 0                           |
|                          |                                        |
|         +----------------+----------------+                       |
|         |                |                |                       |
|    +----v----+      +----v----+      +----v----+                  |
|    |  home   |      |   etc   |      |README.md|                  |
|    |get_size |      |get_size |      |get_size |                  |
|    +----+----+      +----+----+      +---------+                  |
|         |                |           = 4096                       |
|    +----+----+      +----+----+                                   |
|    |         |      |         |                                   |
|    v         v      v         v                                   |
| +------+ +-----+ +------+ +-------+                               |
| |alice | | bob | |passwd| | hosts |                               |
| | size | |size | | 1024 | |  256  |                               |
| +--+---+ +--+--+ +------+ +-------+                               |
|    |        |                                                     |
|    v        v                                                     |
| +-----+  +------+                                                 |
| |files|  |notes |                                                 |
| |2049K|  | 512  |                                                 |
| +-----+  +------+                                                 |
|                                                                   |
|    Traversal Order (depth-first):                                 |
|    1. Visit root                                                  |
|    2. Visit home -> alice -> files, projects                      |
|    3. Visit home -> bob -> notes                                  |
|    4. Visit etc -> passwd, hosts, init.d                          |
|    5. Visit README.md                                             |
|    6. Sum all sizes and return                                    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 组合模式的树遍历过程：对根节点调用get_size()操作时，目录节点递归调用所有子节点的get_size()并累加结果，文件节点直接返回自身大小。这种深度优先遍历是组合模式的典型操作方式，对客户端来说，无论节点是文件还是目录，都使用相同的接口。

---

## 6. Key Implementation Points

1. **Unified Interface**: Both leaves and composites implement same operations
2. **Parent Pointer**: Enables tree traversal in both directions
3. **Child List**: Composite maintains list of children
4. **Recursive Operations**: Operations on composite call children recursively
5. **Type Identification**: Distinguish leaves from composites when needed
6. **Memory Management**: Destroy must clean up entire subtree

**中文说明：** 实现组合模式的关键点：叶子和组合对象实现相同接口、保存父节点指针支持双向遍历、组合节点维护子节点列表、组合节点的操作递归调用子节点、需要时可区分叶子和组合节点、销毁时必须清理整个子树。

