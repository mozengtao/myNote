# Case 1: Directory Entry Cache (dcache)

The dcache demonstrates RCU for read-heavy path lookups.

---

## Subsystem Context

```
+=============================================================================+
|                    DCACHE RCU USAGE                                          |
+=============================================================================+

    PROBLEM:
    ========
    
    Path resolution (/home/user/file):
    - Happens on EVERY file operation
    - Must be FAST (millions of lookups/second)
    - File/directory changes are rare
    
    Traditional locking:
    - Lock on every dentry access
    - Severe contention on common paths (/, /home, etc.)
    - Unacceptable performance


    RCU SOLUTION:
    =============
    
    - Path walk uses RCU (no locks for reading)
    - Dentry lookup is lock-free
    - Only file create/delete takes locks
    
    
    PATH WALK:
    ==========
    
    lookup("/home/user/file")
        |
        v
    rcu_read_lock()
        |
        +-> lookup "home" in / (RCU-protected hash lookup)
        |
        +-> lookup "user" in /home
        |
        +-> lookup "file" in /home/user
        |
    rcu_read_unlock()
```

**中文说明：**

dcache问题：路径解析在每次文件操作时发生，必须快速。传统锁定在每次dentry访问时加锁，严重竞争。RCU解决方案：路径遍历使用RCU无锁读取，只有文件创建/删除才加锁。

---

## Key Structures

```c
/* fs/dcache.c */

struct dentry {
    unsigned int d_flags;
    struct dentry *d_parent;
    struct qstr d_name;
    struct inode *d_inode;
    struct hlist_bl_node d_hash;  /* RCU-protected hash */
    /* ... */
};

/* Dentry hash table - RCU protected */
static struct hlist_bl_head *dentry_hashtable;
```

---

## Key Functions

```c
/* RCU-mode path walk (simplified) */
struct dentry *__d_lookup_rcu(const struct dentry *parent,
                              const struct qstr *name)
{
    struct hlist_bl_head *b = d_hash(parent, name->hash);
    struct dentry *dentry;

    /* No locks - RCU protects the hash list */
    hlist_bl_for_each_entry_rcu(dentry, node, b, d_hash) {
        if (dentry->d_parent == parent &&
            d_same_name(dentry, name)) {
            return dentry;
        }
    }
    return NULL;
}

/* Path walk using RCU */
int path_walk_rcu(const char *name, struct nameidata *nd)
{
    rcu_read_lock();
    
    while (*name) {
        struct dentry *dentry;
        
        /* RCU-protected lookup */
        dentry = __d_lookup_rcu(nd->path.dentry, &this);
        if (!dentry) {
            rcu_read_unlock();
            return -ENOENT;
        }
        
        nd->path.dentry = dentry;
        name = next_component(name);
    }
    
    rcu_read_unlock();
    return 0;
}
```

---

## Minimal C Simulation

```c
/* Simplified dcache RCU simulation */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Simulated dentry */
struct dentry {
    char name[64];
    struct dentry *parent;
    struct dentry *next;  /* Hash chain */
};

/* Global hash table (simplified) */
#define HASH_SIZE 16
static struct dentry *hash_table[HASH_SIZE];

/* Simulated RCU */
static int rcu_read_count = 0;

void rcu_read_lock(void) {
    rcu_read_count++;
    printf("  [RCU] read_lock (count=%d)\n", rcu_read_count);
}

void rcu_read_unlock(void) {
    rcu_read_count--;
    printf("  [RCU] read_unlock (count=%d)\n", rcu_read_count);
}

/* Hash function */
unsigned int hash_name(const char *name) {
    unsigned int h = 0;
    while (*name)
        h = h * 31 + *name++;
    return h % HASH_SIZE;
}

/* RCU-protected lookup (no locks!) */
struct dentry *d_lookup_rcu(struct dentry *parent, const char *name)
{
    unsigned int h = hash_name(name);
    struct dentry *d;
    
    /* Walk hash chain - RCU protects */
    for (d = hash_table[h]; d; d = d->next) {
        if (d->parent == parent && strcmp(d->name, name) == 0)
            return d;
    }
    return NULL;
}

/* Path walk using RCU */
struct dentry *path_lookup_rcu(struct dentry *root, const char *path)
{
    struct dentry *dentry = root;
    char component[64];
    const char *p = path;
    
    printf("[LOOKUP] Path: %s\n", path);
    
    rcu_read_lock();
    
    while (*p) {
        /* Skip leading slashes */
        while (*p == '/') p++;
        if (!*p) break;
        
        /* Extract component */
        int i = 0;
        while (*p && *p != '/')
            component[i++] = *p++;
        component[i] = '\0';
        
        /* RCU-protected lookup */
        printf("  [LOOKUP] Component: %s\n", component);
        dentry = d_lookup_rcu(dentry, component);
        if (!dentry) {
            printf("  [LOOKUP] Not found!\n");
            rcu_read_unlock();
            return NULL;
        }
    }
    
    rcu_read_unlock();
    printf("  [LOOKUP] Found: %s\n", dentry->name);
    return dentry;
}

/* Add dentry to hash (with RCU publish) */
void d_add(struct dentry *dentry)
{
    unsigned int h = hash_name(dentry->name);
    
    /* rcu_assign_pointer equivalent */
    dentry->next = hash_table[h];
    hash_table[h] = dentry;
}

/* Create test dentries */
void setup_dcache(void)
{
    static struct dentry root = { .name = "/", .parent = NULL };
    static struct dentry home = { .name = "home", .parent = &root };
    static struct dentry user = { .name = "user", .parent = &home };
    static struct dentry file = { .name = "file.txt", .parent = &user };
    
    d_add(&root);
    d_add(&home);
    d_add(&user);
    d_add(&file);
}

int main(void)
{
    printf("=== DCACHE RCU SIMULATION ===\n\n");
    
    setup_dcache();
    
    /* Simulate path lookups */
    path_lookup_rcu(hash_table[hash_name("/")], "/home/user/file.txt");
    printf("\n");
    path_lookup_rcu(hash_table[hash_name("/")], "/home/nonexistent");
    
    return 0;
}
```

---

## What Core Does NOT Control

```
    RCU Core Controls:
    ------------------
    [X] Grace period tracking
    [X] Read-side critical section marking
    [X] Memory ordering guarantees

    Dcache Controls:
    ----------------
    [X] When to use RCU for lookups
    [X] Hash table structure
    [X] Dentry lifecycle

    Neither Controls:
    -----------------
    [ ] When files are accessed
    [ ] Filesystem-specific behavior
```

---

## Version

Based on **Linux kernel v3.2** fs/dcache.c.
