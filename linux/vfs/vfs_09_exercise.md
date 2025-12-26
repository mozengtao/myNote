# VFS Architecture Study: User-Space Mini-VFS Exercise

## Design Goals

```
+------------------------------------------------------------------+
|  MINI-VFS DESIGN GOALS                                           |
+------------------------------------------------------------------+

    WHAT WE WILL BUILD:
    ┌─────────────────────────────────────────────────────────────┐
    │  • User-space VFS-like framework in pure C                 │
    │  • struct file with operations table                       │
    │  • At least two backends (memory, file)                    │
    │  • Clear ownership and lifetime rules                      │
    │  • Reference counting for shared access                    │
    └─────────────────────────────────────────────────────────────┘

    KEY PATTERNS TO DEMONSTRATE:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Ops-based polymorphism                                 │
    │  2. Opaque handles                                         │
    │  3. Reference counting                                     │
    │  4. Registration system                                    │
    │  5. Explicit ownership                                     │
    │  6. Error handling with goto cleanup                       │
    └─────────────────────────────────────────────────────────────┘

    ARCHITECTURE OVERVIEW:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │    ┌─────────────────────────────────────────────────────┐  │
    │    │              APPLICATION LAYER                      │  │
    │    │   vfs_open() / vfs_read() / vfs_write() / vfs_close()│  │
    │    └───────────────────────┬─────────────────────────────┘  │
    │                            │                                 │
    │    ┌───────────────────────▼─────────────────────────────┐  │
    │    │              VFS FRAMEWORK                          │  │
    │    │   struct vfs_file + struct vfs_file_ops             │  │
    │    │   Dispatches to backend via ops pointer             │  │
    │    └───────────────────────┬─────────────────────────────┘  │
    │                            │                                 │
    │    ┌────────────┬──────────┴───────────┬─────────────────┐  │
    │    │            │                      │                 │  │
    │    ▼            ▼                      ▼                 │  │
    │ ┌──────┐   ┌──────┐              ┌──────────┐           │  │
    │ │memory│   │ file │              │  future  │           │  │
    │ │backend   │backend│              │ backends │           │  │
    │ └──────┘   └──────┘              └──────────┘           │  │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## Complete Implementation

```c
/* mini_vfs.c - Complete user-space VFS framework */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <errno.h>

/*============================================================================
 * SECTION 1: CORE TYPES AND FORWARD DECLARATIONS
 *============================================================================*/

/* Forward declarations for opaque types */
struct vfs_file;
struct vfs_backend;

/* File operations table (polymorphism interface) */
struct vfs_file_ops {
    int (*open)(struct vfs_file *file, const char *path, int flags);
    ssize_t (*read)(struct vfs_file *file, void *buf, size_t count);
    ssize_t (*write)(struct vfs_file *file, const void *buf, size_t count);
    off_t (*seek)(struct vfs_file *file, off_t offset, int whence);
    int (*close)(struct vfs_file *file);
};

/* Backend descriptor (for registration) */
struct vfs_backend {
    const char *name;
    const struct vfs_file_ops *ops;
    size_t private_size;  /* Size of backend-specific data */
    struct vfs_backend *next;  /* Linked list of backends */
};

/* Open file flags */
#define VFS_O_RDONLY  0x01
#define VFS_O_WRONLY  0x02
#define VFS_O_RDWR    (VFS_O_RDONLY | VFS_O_WRONLY)
#define VFS_O_CREATE  0x10
#define VFS_O_TRUNC   0x20

/* Seek whence values */
#define VFS_SEEK_SET  0
#define VFS_SEEK_CUR  1
#define VFS_SEEK_END  2

/*============================================================================
 * SECTION 2: VFS FILE STRUCTURE
 *============================================================================*/

/*
 * struct vfs_file - represents an open file
 *
 * OWNERSHIP RULES:
 *   - Created by vfs_open(), initial refcount = 1
 *   - vfs_file_get() increments refcount (borrow reference)
 *   - vfs_file_put() decrements refcount (release reference)
 *   - When refcount reaches 0, file is closed and freed
 *   - Caller must hold reference while accessing file
 */
struct vfs_file {
    atomic_int refcount;              /* Reference count */
    const struct vfs_file_ops *ops;   /* Operations table */
    const struct vfs_backend *backend;/* Backend descriptor */
    char path[256];                   /* File path */
    int flags;                        /* Open flags */
    off_t position;                   /* Current position */
    bool is_open;                     /* File state */
    char private_data[];              /* Backend-specific data (flexible array) */
};

/* Get reference (increment refcount) */
static inline struct vfs_file *vfs_file_get(struct vfs_file *file) {
    if (file)
        atomic_fetch_add(&file->refcount, 1);
    return file;
}

/* Release reference (decrement refcount, free if last) */
static void vfs_file_put(struct vfs_file *file);

/*============================================================================
 * SECTION 3: BACKEND REGISTRATION SYSTEM
 *============================================================================*/

/* Global backend registry */
static struct {
    struct vfs_backend *head;
    int count;
} vfs_registry = { .head = NULL, .count = 0 };

/*
 * Register a backend with the VFS framework
 * Returns 0 on success, -1 on error
 */
int vfs_register_backend(struct vfs_backend *backend) {
    if (!backend || !backend->name || !backend->ops)
        return -1;
    
    /* Check for duplicate */
    for (struct vfs_backend *b = vfs_registry.head; b; b = b->next) {
        if (strcmp(b->name, backend->name) == 0)
            return -1;  /* Already registered */
    }
    
    /* Add to front of list */
    backend->next = vfs_registry.head;
    vfs_registry.head = backend;
    vfs_registry.count++;
    
    printf("[VFS] Registered backend: %s\n", backend->name);
    return 0;
}

/*
 * Find backend by name
 * Returns backend or NULL if not found
 */
const struct vfs_backend *vfs_find_backend(const char *name) {
    for (struct vfs_backend *b = vfs_registry.head; b; b = b->next) {
        if (strcmp(b->name, name) == 0)
            return b;
    }
    return NULL;
}

/* List all registered backends */
void vfs_list_backends(void) {
    printf("[VFS] Registered backends (%d):\n", vfs_registry.count);
    for (struct vfs_backend *b = vfs_registry.head; b; b = b->next) {
        printf("  - %s (private_size=%zu)\n", b->name, b->private_size);
    }
}

/*============================================================================
 * SECTION 4: VFS PUBLIC API
 *============================================================================*/

/*
 * vfs_open - Open a file using specified backend
 *
 * OWNERSHIP: Returns owned reference. Caller must call vfs_close().
 *
 * @backend_name: Name of backend to use
 * @path: Path to open
 * @flags: Open flags (VFS_O_*)
 *
 * Returns: File handle on success, NULL on error
 */
struct vfs_file *vfs_open(const char *backend_name, const char *path, int flags) {
    struct vfs_file *file = NULL;
    const struct vfs_backend *backend;
    int ret;
    
    /* Find backend */
    backend = vfs_find_backend(backend_name);
    if (!backend) {
        fprintf(stderr, "[VFS] Unknown backend: %s\n", backend_name);
        goto err;
    }
    
    /* Allocate file structure + private data */
    file = malloc(sizeof(*file) + backend->private_size);
    if (!file) {
        fprintf(stderr, "[VFS] Out of memory\n");
        goto err;
    }
    
    /* Initialize file structure */
    atomic_init(&file->refcount, 1);
    file->ops = backend->ops;
    file->backend = backend;
    strncpy(file->path, path, sizeof(file->path) - 1);
    file->path[sizeof(file->path) - 1] = '\0';
    file->flags = flags;
    file->position = 0;
    file->is_open = false;
    memset(file->private_data, 0, backend->private_size);
    
    /* Call backend open */
    if (file->ops->open) {
        ret = file->ops->open(file, path, flags);
        if (ret < 0) {
            fprintf(stderr, "[VFS] Backend open failed: %d\n", ret);
            goto err_free;
        }
    }
    
    file->is_open = true;
    printf("[VFS] Opened %s via %s (refcount=%d)\n", 
           path, backend_name, atomic_load(&file->refcount));
    return file;
    
err_free:
    free(file);
err:
    return NULL;
}

/*
 * vfs_read - Read from file
 *
 * @file: File handle (caller must hold reference)
 * @buf: Buffer to read into
 * @count: Number of bytes to read
 *
 * Returns: Bytes read, or negative error code
 */
ssize_t vfs_read(struct vfs_file *file, void *buf, size_t count) {
    if (!file || !file->is_open)
        return -EBADF;
    if (!(file->flags & VFS_O_RDONLY))
        return -EACCES;
    if (!file->ops->read)
        return -ENOSYS;
    
    return file->ops->read(file, buf, count);
}

/*
 * vfs_write - Write to file
 *
 * @file: File handle (caller must hold reference)
 * @buf: Buffer to write from
 * @count: Number of bytes to write
 *
 * Returns: Bytes written, or negative error code
 */
ssize_t vfs_write(struct vfs_file *file, const void *buf, size_t count) {
    if (!file || !file->is_open)
        return -EBADF;
    if (!(file->flags & VFS_O_WRONLY))
        return -EACCES;
    if (!file->ops->write)
        return -ENOSYS;
    
    return file->ops->write(file, buf, count);
}

/*
 * vfs_seek - Seek to position
 *
 * @file: File handle
 * @offset: Offset to seek
 * @whence: VFS_SEEK_SET, VFS_SEEK_CUR, or VFS_SEEK_END
 *
 * Returns: New position, or negative error code
 */
off_t vfs_seek(struct vfs_file *file, off_t offset, int whence) {
    if (!file || !file->is_open)
        return -EBADF;
    if (!file->ops->seek)
        return -ENOSYS;
    
    return file->ops->seek(file, offset, whence);
}

/*
 * vfs_close - Close file and release reference
 *
 * OWNERSHIP: Releases caller's reference. File may remain open
 *            if other references exist.
 *
 * @file: File handle to close
 */
void vfs_close(struct vfs_file *file) {
    vfs_file_put(file);
}

/* Internal: release reference and free if last */
static void vfs_file_put(struct vfs_file *file) {
    if (!file)
        return;
    
    int old_ref = atomic_fetch_sub(&file->refcount, 1);
    printf("[VFS] vfs_file_put: refcount %d -> %d\n", old_ref, old_ref - 1);
    
    if (old_ref == 1) {
        /* We were the last reference */
        printf("[VFS] Closing and freeing %s\n", file->path);
        
        if (file->is_open && file->ops->close) {
            file->ops->close(file);
        }
        file->is_open = false;
        free(file);
    }
}

/*============================================================================
 * SECTION 5: MEMORY BACKEND IMPLEMENTATION
 *============================================================================*/

/* Private data for memory backend */
struct mem_private {
    char *buffer;
    size_t size;
    size_t capacity;
};

/* Access private data from file */
static inline struct mem_private *mem_priv(struct vfs_file *file) {
    return (struct mem_private *)file->private_data;
}

static int mem_open(struct vfs_file *file, const char *path, int flags) {
    struct mem_private *priv = mem_priv(file);
    
    priv->capacity = 4096;
    priv->buffer = malloc(priv->capacity);
    if (!priv->buffer)
        return -ENOMEM;
    
    priv->size = 0;
    memset(priv->buffer, 0, priv->capacity);
    
    printf("[MEM] Opened memory file: %s (capacity=%zu)\n", path, priv->capacity);
    return 0;
}

static ssize_t mem_read(struct vfs_file *file, void *buf, size_t count) {
    struct mem_private *priv = mem_priv(file);
    size_t available = priv->size - file->position;
    size_t to_read = count < available ? count : available;
    
    if (to_read == 0)
        return 0;  /* EOF */
    
    memcpy(buf, priv->buffer + file->position, to_read);
    file->position += to_read;
    
    printf("[MEM] Read %zu bytes at pos %ld\n", to_read, (long)(file->position - to_read));
    return to_read;
}

static ssize_t mem_write(struct vfs_file *file, const void *buf, size_t count) {
    struct mem_private *priv = mem_priv(file);
    size_t new_size = file->position + count;
    
    /* Grow buffer if needed */
    if (new_size > priv->capacity) {
        size_t new_cap = priv->capacity * 2;
        while (new_cap < new_size)
            new_cap *= 2;
        
        char *new_buf = realloc(priv->buffer, new_cap);
        if (!new_buf)
            return -ENOMEM;
        
        priv->buffer = new_buf;
        priv->capacity = new_cap;
        printf("[MEM] Grew buffer to %zu\n", new_cap);
    }
    
    memcpy(priv->buffer + file->position, buf, count);
    file->position += count;
    if (file->position > (off_t)priv->size)
        priv->size = file->position;
    
    printf("[MEM] Wrote %zu bytes at pos %ld (size=%zu)\n", 
           count, (long)(file->position - count), priv->size);
    return count;
}

static off_t mem_seek(struct vfs_file *file, off_t offset, int whence) {
    struct mem_private *priv = mem_priv(file);
    off_t new_pos;
    
    switch (whence) {
    case VFS_SEEK_SET:
        new_pos = offset;
        break;
    case VFS_SEEK_CUR:
        new_pos = file->position + offset;
        break;
    case VFS_SEEK_END:
        new_pos = priv->size + offset;
        break;
    default:
        return -EINVAL;
    }
    
    if (new_pos < 0)
        return -EINVAL;
    
    file->position = new_pos;
    printf("[MEM] Seeked to %ld\n", (long)new_pos);
    return new_pos;
}

static int mem_close(struct vfs_file *file) {
    struct mem_private *priv = mem_priv(file);
    
    printf("[MEM] Closing memory file (size=%zu)\n", priv->size);
    free(priv->buffer);
    priv->buffer = NULL;
    priv->size = 0;
    priv->capacity = 0;
    
    return 0;
}

static const struct vfs_file_ops mem_ops = {
    .open  = mem_open,
    .read  = mem_read,
    .write = mem_write,
    .seek  = mem_seek,
    .close = mem_close,
};

static struct vfs_backend mem_backend = {
    .name = "memory",
    .ops = &mem_ops,
    .private_size = sizeof(struct mem_private),
};

/*============================================================================
 * SECTION 6: FILE BACKEND IMPLEMENTATION
 *============================================================================*/

/* Private data for file backend */
struct file_private {
    FILE *fp;
};

static inline struct file_private *file_priv(struct vfs_file *file) {
    return (struct file_private *)file->private_data;
}

static int file_open(struct vfs_file *file, const char *path, int flags) {
    struct file_private *priv = file_priv(file);
    const char *mode;
    
    if ((flags & VFS_O_RDWR) == VFS_O_RDWR) {
        mode = (flags & VFS_O_CREATE) ? "w+" : "r+";
    } else if (flags & VFS_O_WRONLY) {
        mode = (flags & VFS_O_CREATE) ? "w" : "r+";
    } else {
        mode = "r";
    }
    
    priv->fp = fopen(path, mode);
    if (!priv->fp) {
        printf("[FILE] Failed to open: %s\n", path);
        return -errno;
    }
    
    printf("[FILE] Opened: %s (mode=%s)\n", path, mode);
    return 0;
}

static ssize_t file_read(struct vfs_file *file, void *buf, size_t count) {
    struct file_private *priv = file_priv(file);
    
    size_t n = fread(buf, 1, count, priv->fp);
    if (n == 0 && ferror(priv->fp))
        return -EIO;
    
    file->position += n;
    printf("[FILE] Read %zu bytes\n", n);
    return n;
}

static ssize_t file_write(struct vfs_file *file, const void *buf, size_t count) {
    struct file_private *priv = file_priv(file);
    
    size_t n = fwrite(buf, 1, count, priv->fp);
    if (n == 0 && ferror(priv->fp))
        return -EIO;
    
    fflush(priv->fp);
    file->position += n;
    printf("[FILE] Wrote %zu bytes\n", n);
    return n;
}

static off_t file_seek(struct vfs_file *file, off_t offset, int whence) {
    struct file_private *priv = file_priv(file);
    int w;
    
    switch (whence) {
    case VFS_SEEK_SET: w = SEEK_SET; break;
    case VFS_SEEK_CUR: w = SEEK_CUR; break;
    case VFS_SEEK_END: w = SEEK_END; break;
    default: return -EINVAL;
    }
    
    if (fseek(priv->fp, offset, w) < 0)
        return -errno;
    
    file->position = ftell(priv->fp);
    printf("[FILE] Seeked to %ld\n", (long)file->position);
    return file->position;
}

static int file_close(struct vfs_file *file) {
    struct file_private *priv = file_priv(file);
    
    printf("[FILE] Closing file\n");
    if (priv->fp) {
        fclose(priv->fp);
        priv->fp = NULL;
    }
    return 0;
}

static const struct vfs_file_ops file_ops = {
    .open  = file_open,
    .read  = file_read,
    .write = file_write,
    .seek  = file_seek,
    .close = file_close,
};

static struct vfs_backend file_backend = {
    .name = "file",
    .ops = &file_ops,
    .private_size = sizeof(struct file_private),
};

/*============================================================================
 * SECTION 7: INITIALIZATION
 *============================================================================*/

void vfs_init(void) {
    printf("[VFS] Initializing mini-VFS framework\n");
    vfs_register_backend(&mem_backend);
    vfs_register_backend(&file_backend);
    vfs_list_backends();
}

/*============================================================================
 * SECTION 8: EXAMPLE USAGE
 *============================================================================*/

/* Copy data between two files (demonstrates backend independence) */
int vfs_copy(struct vfs_file *src, struct vfs_file *dst) {
    char buf[256];
    ssize_t n;
    size_t total = 0;
    
    while ((n = vfs_read(src, buf, sizeof(buf))) > 0) {
        ssize_t written = vfs_write(dst, buf, n);
        if (written != n)
            return -EIO;
        total += written;
    }
    
    printf("[VFS] Copied %zu bytes\n", total);
    return n < 0 ? n : 0;
}

/* Test shared access (reference counting) */
void test_shared_access(void) {
    printf("\n=== Test: Shared Access ===\n");
    
    /* Open file */
    struct vfs_file *f1 = vfs_open("memory", "/shared", VFS_O_RDWR | VFS_O_CREATE);
    if (!f1) return;
    
    /* Share reference */
    struct vfs_file *f2 = vfs_file_get(f1);
    printf("After sharing: refcount = %d\n", atomic_load(&f1->refcount));
    
    /* Write via f1 */
    vfs_write(f1, "hello", 5);
    
    /* Seek and read via f2 */
    vfs_seek(f2, 0, VFS_SEEK_SET);
    char buf[16] = {0};
    vfs_read(f2, buf, sizeof(buf));
    printf("Read via f2: '%s'\n", buf);
    
    /* Release f1's reference */
    vfs_close(f1);
    printf("After closing f1: refcount = %d\n", atomic_load(&f2->refcount));
    
    /* File still usable via f2 */
    vfs_seek(f2, 0, VFS_SEEK_SET);
    vfs_write(f2, "world", 5);
    
    /* Release f2's reference (file is freed here) */
    vfs_close(f2);
}

/* Test cross-backend copy */
void test_cross_backend(void) {
    printf("\n=== Test: Cross-Backend Copy ===\n");
    
    /* Create memory file with data */
    struct vfs_file *mem = vfs_open("memory", "/source", VFS_O_RDWR | VFS_O_CREATE);
    if (!mem) return;
    
    vfs_write(mem, "Data from memory backend!", 25);
    vfs_seek(mem, 0, VFS_SEEK_SET);
    
    /* Open real file */
    struct vfs_file *file = vfs_open("file", "/tmp/mini_vfs_test.txt", 
                                     VFS_O_RDWR | VFS_O_CREATE);
    if (!file) {
        vfs_close(mem);
        return;
    }
    
    /* Copy memory → file (same API, different backends) */
    vfs_copy(mem, file);
    
    /* Verify */
    vfs_seek(file, 0, VFS_SEEK_SET);
    char buf[64] = {0};
    vfs_read(file, buf, sizeof(buf));
    printf("Copied to file: '%s'\n", buf);
    
    vfs_close(mem);
    vfs_close(file);
}

int main(void) {
    printf("========================================\n");
    printf("    Mini-VFS Framework Demo\n");
    printf("========================================\n\n");
    
    /* Initialize framework */
    vfs_init();
    
    /* Test memory backend */
    printf("\n=== Test: Memory Backend ===\n");
    struct vfs_file *mf = vfs_open("memory", "/test", VFS_O_RDWR | VFS_O_CREATE);
    if (mf) {
        vfs_write(mf, "Hello, Mini-VFS!", 16);
        vfs_seek(mf, 0, VFS_SEEK_SET);
        
        char buf[32] = {0};
        vfs_read(mf, buf, sizeof(buf));
        printf("Read: '%s'\n", buf);
        
        vfs_close(mf);
    }
    
    /* Test shared access */
    test_shared_access();
    
    /* Test cross-backend copy */
    test_cross_backend();
    
    printf("\n========================================\n");
    printf("    Demo Complete\n");
    printf("========================================\n");
    
    return 0;
}
```

---

## Architecture Explanation

```
+------------------------------------------------------------------+
|  MINI-VFS ARCHITECTURE BREAKDOWN                                 |
+------------------------------------------------------------------+

    BOUNDARIES:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  APPLICATION                                                │
    │    ↓ vfs_open/read/write/close (public API)                │
    │  ──────────────────────────────────────────────────────────│
    │  VFS FRAMEWORK                                              │
    │    • Validates parameters                                   │
    │    • Manages refcounting                                    │
    │    • Dispatches to backend via ops                         │
    │    ↓ file->ops->method() (ops dispatch)                    │
    │  ──────────────────────────────────────────────────────────│
    │  BACKENDS (memory, file, ...)                               │
    │    • Implement actual I/O                                   │
    │    • Own private data                                       │
    │    • Know nothing about other backends                     │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    EXTENSIBILITY:
    ┌─────────────────────────────────────────────────────────────┐
    │  To add new backend:                                        │
    │    1. Define private data struct                           │
    │    2. Implement vfs_file_ops functions                     │
    │    3. Create vfs_backend descriptor                        │
    │    4. Call vfs_register_backend()                          │
    │                                                              │
    │  Framework code NEVER changes for new backends             │
    └─────────────────────────────────────────────────────────────┘

    OWNERSHIP:
    ┌─────────────────────────────────────────────────────────────┐
    │  vfs_file lifetime:                                         │
    │    • Created: vfs_open() → refcount = 1                    │
    │    • Shared: vfs_file_get() → refcount++                   │
    │    • Released: vfs_close() → refcount--                    │
    │    • Freed: when refcount reaches 0                        │
    │                                                              │
    │  Private data lifetime:                                     │
    │    • Allocated with vfs_file (flexible array)              │
    │    • Initialized by backend open()                         │
    │    • Cleaned up by backend close()                         │
    │    • Freed with vfs_file                                   │
    └─────────────────────────────────────────────────────────────┘

    SAFETY:
    ┌─────────────────────────────────────────────────────────────┐
    │  Guaranteed by design:                                      │
    │    • No use-after-free: refcount prevents premature free   │
    │    • No double-free: atomic refcount operations            │
    │    • No leaks: clear ownership (caller must close)         │
    │    • No type confusion: ops table matches backend          │
    │                                                              │
    │  NOT guaranteed (user responsibility):                     │
    │    • Correct flags (read-only file written to)             │
    │    • Valid paths (backend-specific)                        │
    │    • Thread safety (needs external synchronization)        │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  MINI-VFS PATTERNS DEMONSTRATED                                  |
+------------------------------------------------------------------+

    1. OPS-BASED POLYMORPHISM
       • struct vfs_file_ops with function pointers
       • Dispatch via file->ops->method()
       • Backends implement same interface

    2. REGISTRATION SYSTEM
       • struct vfs_backend descriptors
       • vfs_register_backend() adds to registry
       • vfs_find_backend() lookups by name

    3. REFERENCE COUNTING
       • atomic_int refcount in vfs_file
       • vfs_file_get() increments
       • vfs_file_put() decrements and frees

    4. FLEXIBLE ARRAY MEMBER
       • private_data[] at end of vfs_file
       • Size determined by backend->private_size
       • Single allocation for object + private data

    5. EXPLICIT OWNERSHIP
       • vfs_open() returns owned reference
       • vfs_close() releases reference
       • Shared via vfs_file_get()

    6. OPAQUE HANDLES
       • User sees struct vfs_file pointer
       • Cannot access private_data directly
       • Backend implementation hidden
```

**中文总结：**
- **实现目标**：用户空间VFS框架，包含内存和文件两个后端
- **核心模式**：ops多态、注册系统、引用计数、柔性数组、显式所有权
- **边界清晰**：应用层→VFS框架→后端，单向依赖
- **扩展性**：添加新后端只需实现ops并注册，无需修改框架
- **安全性**：引用计数防止释放后使用，所有权规则防止泄漏

