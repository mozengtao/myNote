# Case 3: struct file Reference Counting

## Subsystem Background

```
+=============================================================================+
|                    FILE REFERENCE COUNTING                                   |
+=============================================================================+

    THE PROBLEM:
    ============

    struct file represents an open file.
    Multiple processes can share same struct file:
    - After fork(): Parent and child share file descriptors
    - After dup(): Multiple fds point to same file
    - After sendfile(): File passed between processes

    When to free struct file?
    - When ALL file descriptors pointing to it are closed
    - Reference counting solves this


    STRUCT FILE:
    ============

    struct file {
        /* ... */
        struct path         f_path;
        const struct file_operations *f_op;
        atomic_long_t       f_count;    /* Reference counter */
        unsigned int        f_flags;
        fmode_t             f_mode;
        loff_t              f_pos;
        /* ... */
    };


    LIFECYCLE:
    ==========

    open() -> struct file allocated, f_count = 1
    fork() -> f_count++
    dup()  -> f_count++
    close() -> f_count--
    f_count == 0 -> file released
```

**中文说明：**

struct file表示打开的文件。多个进程可以共享同一个struct file：fork后父子进程共享文件描述符，dup后多个fd指向同一个file，sendfile可以在进程间传递文件。何时释放struct file？当所有指向它的文件描述符都关闭时。引用计数解决了这个问题。

---

## File Reference Counting API

```c
/* fs/file_table.c */

/* Get a reference to struct file */
struct file *get_file(struct file *f)
{
    atomic_long_inc(&f->f_count);
    return f;
}

/* Release a reference (fput) */
void fput(struct file *file)
{
    if (atomic_long_dec_and_test(&file->f_count)) {
        /* Schedule actual release */
        /* (Not immediate due to RCU and context issues) */
        schedule_delayed_fput(file);
    }
}

/* Actual release (called from work queue) */
static void __fput(struct file *file)
{
    /* Close the file */
    if (file->f_op && file->f_op->release)
        file->f_op->release(inode, file);
    
    /* Release path references */
    path_put(&file->f_path);
    
    /* Free the file structure */
    put_filp(file);
}
```

---

## Minimal C Code Simulation

```c
/*
 * FILE REFERENCE COUNTING SIMULATION
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdatomic.h>

/* ==========================================================
 * SIMPLIFIED FILE STRUCTURE
 * ========================================================== */

struct file_operations;
struct file;

struct file_operations {
    int (*open)(struct file *);
    int (*release)(struct file *);
    ssize_t (*read)(struct file *, char *, size_t);
    ssize_t (*write)(struct file *, const char *, size_t);
};

struct file {
    char path[256];
    const struct file_operations *f_op;
    atomic_long f_count;        /* Reference counter */
    int f_flags;
    long f_pos;
    void *private_data;
};

/* ==========================================================
 * FILE OPERATIONS
 * ========================================================== */

/* Get a reference */
struct file *get_file(struct file *f)
{
    if (f) {
        long old = atomic_fetch_add(&f->f_count, 1);
        printf("[FILE] get_file('%s'): f_count %ld -> %ld\n", 
               f->path, old, old + 1);
    }
    return f;
}

/* Release a reference */
void fput(struct file *file)
{
    if (!file) return;
    
    long old = atomic_fetch_sub(&file->f_count, 1);
    printf("[FILE] fput('%s'): f_count %ld -> %ld\n", 
           file->path, old, old - 1);
    
    if (old == 1) {
        /* Was 1, now 0 - release the file */
        printf("[FILE] f_count hit 0, releasing '%s'\n", file->path);
        
        /* Call file's release operation */
        if (file->f_op && file->f_op->release) {
            file->f_op->release(file);
        }
        
        /* Free private data */
        if (file->private_data) {
            free(file->private_data);
        }
        
        /* Free file structure */
        free(file);
    }
}

/* ==========================================================
 * EXAMPLE FILE SYSTEM
 * ========================================================== */

int myfs_open(struct file *f)
{
    printf("[MYFS] Opening file '%s'\n", f->path);
    f->private_data = malloc(1024);  /* File buffer */
    return 0;
}

int myfs_release(struct file *f)
{
    printf("[MYFS] Releasing file '%s'\n", f->path);
    return 0;
}

static struct file_operations myfs_ops = {
    .open = myfs_open,
    .release = myfs_release,
};

/* Open a file */
struct file *do_open(const char *path)
{
    struct file *f = malloc(sizeof(*f));
    if (!f) return NULL;
    
    strncpy(f->path, path, sizeof(f->path) - 1);
    f->f_op = &myfs_ops;
    atomic_store(&f->f_count, 1);  /* Initial reference */
    f->f_flags = 0;
    f->f_pos = 0;
    f->private_data = NULL;
    
    if (f->f_op->open) {
        f->f_op->open(f);
    }
    
    printf("[OPEN] File '%s' opened, f_count = 1\n", path);
    return f;
}

/* ==========================================================
 * PROCESS SIMULATION
 * ========================================================== */

struct process {
    char name[32];
    struct file *files[16];  /* File descriptor table */
    int num_files;
};

struct process *create_process(const char *name)
{
    struct process *p = malloc(sizeof(*p));
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->num_files = 0;
    memset(p->files, 0, sizeof(p->files));
    return p;
}

int process_open(struct process *p, const char *path)
{
    struct file *f = do_open(path);
    if (!f) return -1;
    
    int fd = p->num_files++;
    p->files[fd] = f;
    
    printf("[%s] Opened '%s' as fd %d\n", p->name, path, fd);
    return fd;
}

void process_close(struct process *p, int fd)
{
    if (fd < 0 || fd >= p->num_files || !p->files[fd])
        return;
    
    struct file *f = p->files[fd];
    printf("[%s] Closing fd %d ('%s')\n", p->name, fd, f->path);
    
    p->files[fd] = NULL;
    fput(f);
}

/* Fork: Child inherits file descriptors */
struct process *process_fork(struct process *parent, const char *child_name)
{
    struct process *child = create_process(child_name);
    child->num_files = parent->num_files;
    
    printf("[FORK] '%s' forking to '%s'\n", parent->name, child_name);
    
    /* Inherit all open files */
    for (int i = 0; i < parent->num_files; i++) {
        if (parent->files[i]) {
            child->files[i] = get_file(parent->files[i]);
            printf("  fd %d: '%s' inherited\n", i, parent->files[i]->path);
        }
    }
    
    return child;
}

/* Dup: Create new fd pointing to same file */
int process_dup(struct process *p, int oldfd)
{
    if (oldfd < 0 || oldfd >= p->num_files || !p->files[oldfd])
        return -1;
    
    struct file *f = p->files[oldfd];
    int newfd = p->num_files++;
    
    p->files[newfd] = get_file(f);
    printf("[%s] Dup fd %d to %d ('%s')\n", p->name, oldfd, newfd, f->path);
    
    return newfd;
}

void destroy_process(struct process *p)
{
    printf("[%s] Exiting, closing all files\n", p->name);
    
    for (int i = 0; i < p->num_files; i++) {
        if (p->files[i]) {
            process_close(p, i);
        }
    }
    
    free(p);
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */

int main(void)
{
    printf("=================================================\n");
    printf("FILE REFERENCE COUNTING DEMONSTRATION\n");
    printf("=================================================\n\n");
    
    /* Create parent process */
    printf("--- Parent opens file ---\n");
    struct process *parent = create_process("parent");
    int fd = process_open(parent, "/etc/passwd");
    
    printf("\nf_count of '/etc/passwd': %ld\n", 
           atomic_load(&parent->files[fd]->f_count));
    
    /* Fork creates child with inherited fds */
    printf("\n--- Fork ---\n");
    struct process *child = process_fork(parent, "child");
    
    printf("\nf_count of '/etc/passwd': %ld (shared by parent and child)\n", 
           atomic_load(&parent->files[fd]->f_count));
    
    /* Dup in parent */
    printf("\n--- Dup in parent ---\n");
    int fd2 = process_dup(parent, fd);
    
    printf("\nf_count of '/etc/passwd': %ld\n", 
           atomic_load(&parent->files[fd]->f_count));
    
    /* Parent closes original fd */
    printf("\n--- Parent closes original fd ---\n");
    process_close(parent, fd);
    
    printf("\nf_count of '/etc/passwd': %ld (still alive!)\n", 
           atomic_load(&parent->files[fd2]->f_count));
    
    /* Child exits */
    printf("\n--- Child exits ---\n");
    destroy_process(child);
    
    printf("\nf_count of '/etc/passwd': %ld\n", 
           atomic_load(&parent->files[fd2]->f_count));
    
    /* Parent closes duped fd */
    printf("\n--- Parent closes duped fd (last reference) ---\n");
    process_close(parent, fd2);
    
    /* File is now freed */
    
    /* Parent exits */
    printf("\n--- Parent exits ---\n");
    destroy_process(parent);
    
    printf("\n=================================================\n");
    printf("KEY INSIGHTS:\n");
    printf("- struct file has f_count for reference counting\n");
    printf("- fork() increments f_count for inherited files\n");
    printf("- dup() increments f_count for new fd\n");
    printf("- close() decrements f_count\n");
    printf("- File freed when f_count reaches 0\n");
    printf("=================================================\n");
    
    return 0;
}
```

---

## Real Kernel Code

```c
/* fs/file_table.c (simplified) */

struct file *get_file(struct file *f)
{
    atomic_long_inc(&f->f_count);
    return f;
}
EXPORT_SYMBOL(get_file);

void fput(struct file *file)
{
    if (atomic_long_dec_and_test(&file->f_count)) {
        /* ... delayed release ... */
    }
}
EXPORT_SYMBOL(fput);


/* fs/open.c - sys_open creates file with f_count = 1 */

/* kernel/fork.c - fork increments f_count */
static int copy_files(...)
{
    /* ... */
    get_file(file);  /* Inherit parent's file */
    /* ... */
}

/* fs/fcntl.c - dup increments f_count */
static int do_dup(...)
{
    get_file(file);
    /* ... */
}
```

---

## Key Takeaways

1. **f_count tracks all references**: fds, inherited, duplicated
2. **fork increments count**: Child gets reference to shared files
3. **dup increments count**: New fd shares same struct file
4. **close decrements count**: File freed when last fd closed
5. **Deferred release**: fput may delay actual release for safety
