# Source Reading Guide: Reference Counting in Linux v3.2

## Key Files

### kref Definition

```
include/linux/kref.h
~~~~~~~~~~~~~~~~~~~~
The main kref definition and API.

struct kref {
    atomic_t refcount;
};

void kref_init(struct kref *kref);
void kref_get(struct kref *kref);
int kref_put(struct kref *kref, void (*release)(struct kref *kref));

EXERCISE: Trace how kref_put uses atomic_dec_and_test.
```

### kref Implementation

```
lib/kref.c
~~~~~~~~~~
Implementation of kref functions.

Note: In modern kernels, most are inline in header.
In v3.2, check both header and .c file.

EXERCISE: Compare kref implementation across kernel versions.
```

---

## Subsystem Deep Dives

### 1. kobject Reference Counting

```
include/linux/kobject.h
~~~~~~~~~~~~~~~~~~~~~~~
struct kobject {
    const char      *name;
    struct list_head entry;
    struct kobject  *parent;
    struct kset     *kset;
    struct kobj_type *ktype;
    struct sysfs_dirent *sd;
    struct kref     kref;         /* <-- EMBEDDED KREF */
};

lib/kobject.c
~~~~~~~~~~~~~
Key functions:
- kobject_init(): Calls kref_init
- kobject_get(): Calls kref_get
- kobject_put(): Calls kref_put with kobject_release
- kobject_release(): Uses container_of, calls ktype->release

EXERCISE:
1. Trace kobject_put() to see the release chain
2. Find where ktype->release is called
3. Look for container_of usage in kobject_release
```

### 2. File Reference Counting

```
include/linux/fs.h
~~~~~~~~~~~~~~~~~~
struct file {
    /* ... */
    atomic_long_t   f_count;      /* <-- REFERENCE COUNTER */
    /* ... */
};

fs/file_table.c
~~~~~~~~~~~~~~~
Key functions:
- get_file(): Increments f_count
- fput(): Decrements f_count, schedules release
- __fput(): Actual file release

fs/open.c
~~~~~~~~~
- do_sys_open(): Creates file with initial reference

EXERCISE:
1. Trace fput() to see when file is actually released
2. Find where f_count is checked before operations
3. Look at how fork() handles file reference counting
```

### 3. Module Reference Counting

```
include/linux/module.h
~~~~~~~~~~~~~~~~~~~~~~
struct module {
    /* ... */
    struct module_ref ref;        /* <-- PER-CPU REFCOUNT */
    /* ... */
};

bool try_module_get(struct module *module);
void module_put(struct module *module);
void __module_get(struct module *module);

kernel/module.c
~~~~~~~~~~~~~~~
Key functions:
- try_module_get(): Increments if module not going
- module_put(): Decrements
- delete_module(): Checks refcount before unload

EXERCISE:
1. Trace try_module_get() to see how it checks module state
2. Find where module refcount prevents unload
3. Look at how file_operations->owner is used
```

### 4. Inode Reference Counting

```
include/linux/fs.h
~~~~~~~~~~~~~~~~~~
struct inode {
    /* ... */
    atomic_t        i_count;      /* <-- REFERENCE COUNTER */
    /* ... */
};

fs/inode.c
~~~~~~~~~~
Key functions:
- ihold(): Increments i_count (inode hold)
- iput(): Decrements i_count, may free inode
- iget_locked(): Gets inode with reference

EXERCISE:
1. Trace iput() to see inode destruction
2. Find where inodes are cached even with refcount 0
3. Look at how dentry interacts with inode refcount
```

---

## Reading Strategy

### Step 1: Find the Counter

```bash
# Find structures with kref:
grep -rn "struct kref" include/linux/*.h

# Find structures with atomic refcount:
grep -rn "atomic.*count\|refcount" include/linux/*.h | head -30

# Find structures with f_count pattern:
grep -rn "_count\>" include/linux/fs.h
```

### Step 2: Find Get/Put Functions

```bash
# Find get functions:
grep -rn "_get\|get_" fs/*.c | grep -v "\.o:" | head -30

# Find put functions:
grep -rn "_put\|put_" fs/*.c | grep -v "\.o:" | head -30
```

### Step 3: Trace a Complete Path

```
TRACE: File Reference Counting

1. fs/open.c: do_sys_open()
   - Creates struct file
   - f_count = 1

2. kernel/fork.c: copy_files()
   - On fork, for each inherited fd:
   - get_file(file)  /* f_count++ */

3. fs/open.c: sys_close()
   - fput(file)  /* f_count-- */

4. fs/file_table.c: fput()
   - if (atomic_long_dec_and_test(&file->f_count))
   -     schedule delayed_fput

5. fs/file_table.c: __fput()
   - file->f_op->release()
   - Free file structure
```

---

## Key Patterns to Look For

```
+=============================================================================+
|              REFERENCE COUNTING PATTERNS IN KERNEL                           |
+=============================================================================+

    PATTERN 1: KREF EMBEDDING
    ~~~~~~~~~~~~~~~~~~~~~~~~~
    struct xxx {
        struct kref ref;  /* or kref */
    };
    
    Find: kobject, usb_device, many drivers


    PATTERN 2: ATOMIC COUNTER
    ~~~~~~~~~~~~~~~~~~~~~~~~~
    struct xxx {
        atomic_t count;
        atomic_long_t f_count;
    };
    
    Find: file (f_count), inode (i_count), dentry (d_count)


    PATTERN 3: GET/PUT WRAPPER
    ~~~~~~~~~~~~~~~~~~~~~~~~~~
    struct xxx *xxx_get(struct xxx *);
    void xxx_put(struct xxx *);
    
    Find: kobject_get/put, ihold/iput, dget/dput


    PATTERN 4: CONTAINER_OF IN RELEASE
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void xxx_release(struct kref *ref)
    {
        struct xxx *obj = container_of(ref, struct xxx, ref);
        kfree(obj);
    }
    
    Find: kobject_release, driver-specific release functions
```

---

## Exercises

### Exercise 1: kobject Lifecycle

```
1. Find kobject_init() in lib/kobject.c
2. Trace to kref_init()
3. Find kobject_put()
4. Trace the release chain to ktype->release
5. Find a driver that defines kobj_type with release
```

### Exercise 2: File Descriptor Counting

```
1. Open fs/file_table.c
2. Find get_file() and fput()
3. Trace what happens in fork() to file refcounts
4. Find where fput() schedules delayed work
5. Find __fput() and the actual file release
```

### Exercise 3: Module Protection

```
1. Find try_module_get() in kernel/module.c
2. Trace what happens when module is GOING
3. Find where file_operations uses module owner
4. See how opening a file gets module reference
```

---

## Summary: Files to Read

| File | Content |
|------|---------|
| `include/linux/kref.h` | kref structure and API |
| `lib/kref.c` | kref implementation |
| `include/linux/kobject.h` | kobject with embedded kref |
| `lib/kobject.c` | kobject reference counting |
| `include/linux/fs.h` | file, inode with atomic counters |
| `fs/file_table.c` | File reference counting |
| `fs/inode.c` | Inode reference counting |
| `include/linux/module.h` | Module reference API |
| `kernel/module.c` | Module reference counting |
