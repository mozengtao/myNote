# Unified Composite Skeleton

A generic C skeleton capturing the Composite/Hierarchy pattern.

---

## Complete Skeleton

```c
/*
 * Generic Composite/Hierarchy Pattern Skeleton
 * Based on Linux kernel kobject implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * PART 1: NODE STRUCTURE
 * ================================================================ */

struct node {
    char name[64];
    struct node *parent;         /* Parent in hierarchy */
    struct node *children;       /* First child */
    struct node *sibling;        /* Next sibling */
    int refcount;                /* Reference counting */
    void (*release)(struct node *);  /* Destructor */
};

/* ================================================================
 * PART 2: LIFECYCLE FUNCTIONS
 * ================================================================ */

/* Initialize node */
void node_init(struct node *n, const char *name,
               void (*release)(struct node *))
{
    memset(n, 0, sizeof(*n));
    strncpy(n->name, name, sizeof(n->name) - 1);
    n->refcount = 1;
    n->release = release;
}

/* Add node to parent */
int node_add(struct node *n, struct node *parent)
{
    if (!n)
        return -1;
    
    n->parent = parent;
    
    if (parent) {
        /* Add to parent's children list */
        n->sibling = parent->children;
        parent->children = n;
        parent->refcount++;  /* Parent holds ref to child */
    }
    
    return 0;
}

/* Remove node from hierarchy */
void node_del(struct node *n)
{
    struct node **pp;
    
    if (!n || !n->parent)
        return;
    
    /* Remove from parent's children list */
    for (pp = &n->parent->children; *pp; pp = &(*pp)->sibling) {
        if (*pp == n) {
            *pp = n->sibling;
            n->parent->refcount--;
            break;
        }
    }
    
    n->parent = NULL;
    n->sibling = NULL;
}

/* Get reference */
struct node *node_get(struct node *n)
{
    if (n)
        n->refcount++;
    return n;
}

/* Put reference */
void node_put(struct node *n)
{
    if (!n)
        return;
    
    n->refcount--;
    if (n->refcount == 0) {
        if (n->release)
            n->release(n);
    }
}

/* ================================================================
 * PART 3: TRAVERSAL
 * ================================================================ */

/* Iterate children */
void for_each_child(struct node *n, void (*callback)(struct node *))
{
    struct node *child;
    
    for (child = n->children; child; child = child->sibling) {
        callback(child);
    }
}

/* Get full path */
int node_get_path(struct node *n, char *buf, int size)
{
    if (!n)
        return 0;
    
    if (n->parent) {
        int len = node_get_path(n->parent, buf, size);
        if (len < size - 1) {
            buf[len] = '/';
            len++;
        }
        strncpy(buf + len, n->name, size - len - 1);
        return strlen(buf);
    } else {
        strncpy(buf, n->name, size - 1);
        return strlen(buf);
    }
}

/* Print hierarchy */
void node_print_tree(struct node *n, int depth)
{
    struct node *child;
    int i;
    
    for (i = 0; i < depth; i++)
        printf("    ");
    printf("+-- %s (ref=%d)\n", n->name, n->refcount);
    
    for (child = n->children; child; child = child->sibling) {
        node_print_tree(child, depth + 1);
    }
}

/* ================================================================
 * PART 4: USAGE EXAMPLE
 * ================================================================ */

void my_release(struct node *n)
{
    printf("[RELEASE] Node '%s' freed\n", n->name);
}

int main(void)
{
    struct node root, child1, child2, grandchild;
    char path[256];
    
    printf("=== COMPOSITE SKELETON ===\n\n");
    
    /* Build hierarchy */
    node_init(&root, "root", my_release);
    node_init(&child1, "child1", my_release);
    node_init(&child2, "child2", my_release);
    node_init(&grandchild, "grandchild", my_release);
    
    node_add(&root, NULL);
    node_add(&child1, &root);
    node_add(&child2, &root);
    node_add(&grandchild, &child1);
    
    /* Print hierarchy */
    printf("--- Hierarchy ---\n");
    node_print_tree(&root, 0);
    
    /* Get path */
    printf("\n--- Path ---\n");
    node_get_path(&grandchild, path, sizeof(path));
    printf("Path to grandchild: /%s\n", path);
    
    /* Reference counting */
    printf("\n--- Reference counting ---\n");
    node_get(&grandchild);
    printf("After get: ref=%d\n", grandchild.refcount);
    node_put(&grandchild);
    printf("After put: ref=%d\n", grandchild.refcount);
    
    return 0;
}
```

---

## Mapping to Kernel

```
    SKELETON            KERNEL
    ========            ======
    
    struct node         struct kobject
    node_init           kobject_init
    node_add            kobject_add
    node_del            kobject_del
    node_get            kobject_get
    node_put            kobject_put
    release()           ktype->release()
```

---

## Key Implementation Points

```
    1. PARENT POINTER
       - Points to parent in hierarchy
       - NULL for root
    
    2. CHILDREN LIST
       - Linked list of children
       - Enables tree traversal
    
    3. REFERENCE COUNTING
       - Prevents premature deletion
       - Parent holds ref to children
    
    4. RELEASE CALLBACK
       - Called when refcount reaches 0
       - Custom cleanup per object type
```

---

## Version

Based on **Linux kernel v3.2** kobject patterns.
