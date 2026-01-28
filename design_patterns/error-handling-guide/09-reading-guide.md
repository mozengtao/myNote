# Source Reading Guide: Error Handling in Linux v3.2

## Key Files

### ERR_PTR Macros

```
include/linux/err.h
~~~~~~~~~~~~~~~~~~~
Core ERR_PTR definitions:

#define MAX_ERRNO  4095
#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-MAX_ERRNO)

static inline void *ERR_PTR(long error);
static inline long PTR_ERR(const void *ptr);
static inline long IS_ERR(const void *ptr);
static inline long IS_ERR_OR_NULL(const void *ptr);
static inline void *ERR_CAST(const void *ptr);

EXERCISE: Trace why MAX_ERRNO is 4095 (relates to page size).
```

### Error Codes

```
include/linux/errno.h
include/asm-generic/errno.h
include/asm-generic/errno-base.h
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Standard error code definitions.

EXERCISE: Find all error codes and their meanings.
Common: ENOMEM, EINVAL, ENOENT, EIO, EPERM, EACCES
```

---

## Subsystem Deep Dives

### 1. VFS and File Operations

```
fs/namei.c
~~~~~~~~~~
Path lookup - heavy use of ERR_PTR:

struct dentry *lookup_one_len(...)
{
    if (!len)
        return ERR_PTR(-EACCES);
    ...
}

EXERCISE:
1. Find lookup_one_len and trace ERR_PTR usage
2. Find kern_path and see error propagation
3. Look at how errors bubble up through VFS layer
```

### 2. Driver Initialization

```
drivers/pci/pci-driver.c
drivers/usb/core/driver.c
drivers/platform/...
~~~~~~~~~~~~~~~~~~~~~~~~
Classic goto cleanup patterns:

static int xxx_probe(struct device *dev)
{
    ret = pci_enable_device(pdev);
    if (ret)
        goto err_enable;
    
    ret = pci_request_regions(pdev, ...);
    if (ret)
        goto err_regions;
    ...
    
err_regions:
    pci_disable_device(pdev);
err_enable:
    return ret;
}

EXERCISE:
1. Find a PCI driver probe function
2. Trace the goto cleanup pattern
3. Verify cleanup is in reverse order
```

### 3. Module Initialization

```
drivers/char/mem.c (simple)
drivers/block/loop.c (complex)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Module init with cleanup:

static int __init xxx_init(void)
{
    ret = register_chrdev(...);
    if (ret)
        goto err_chrdev;
    
    ret = class_register(...);
    if (ret)
        goto err_class;
    ...
    
err_class:
    unregister_chrdev(...);
err_chrdev:
    return ret;
}

EXERCISE:
1. Find a simple module init function
2. Look for __init and __exit
3. Compare init cleanup with exit cleanup
```

### 4. Memory Allocation

```
mm/slab.c
mm/slub.c
~~~~~~~~~
Memory allocation error handling:

void *kmalloc(size_t size, gfp_t flags)
{
    ...
    return NULL;  /* On failure - NOT ERR_PTR */
}

void *vmalloc(unsigned long size)
{
    ...
    return NULL;  /* On failure */
}

EXERCISE:
1. Verify kmalloc returns NULL not ERR_PTR
2. Find places that check kmalloc result
3. Look at __GFP_NOFAIL flag
```

---

## Reading Strategy

### Step 1: Find Error Handling

```bash
# Find goto cleanup patterns:
grep -rn "goto err_\|goto fail_\|goto out_" drivers/ | head -50

# Find ERR_PTR usage:
grep -rn "ERR_PTR\|IS_ERR\|PTR_ERR" fs/ | head -50

# Find error code returns:
grep -rn "return -E" kernel/ | head -50
```

### Step 2: Trace a Complete Error Path

```
TRACE: File open error handling

1. fs/open.c: do_sys_open()
   - Calls do_filp_open()
   - Checks IS_ERR(f)
   - Returns PTR_ERR(f) on error

2. fs/namei.c: do_filp_open()
   - Calls path_openat()
   - Returns ERR_PTR on failure

3. fs/namei.c: path_openat()
   - Multiple error points
   - Uses goto cleanup pattern
   - Returns ERR_PTR with specific code

4. Each layer:
   - Checks error from lower layer
   - Either handles or propagates
   - Error code preserved through layers
```

### Step 3: Compare init and exit

```c
/*
 * Good exercise: Compare a module's init and exit functions.
 * They should be mirror images.
 */

/* init: forward allocation */
static int __init my_init(void)
{
    alloc_a();  /* Step 1 */
    alloc_b();  /* Step 2 */
    alloc_c();  /* Step 3 */
    return 0;
}

/* exit: reverse deallocation */
static void __exit my_exit(void)
{
    free_c();   /* Undo step 3 */
    free_b();   /* Undo step 2 */
    free_a();   /* Undo step 1 */
}

/* Error cleanup in init should match exit order */
```

---

## Key Patterns to Look For

```
+=============================================================================+
|              ERROR HANDLING PATTERNS IN KERNEL                               |
+=============================================================================+

    PATTERN 1: GOTO CLEANUP
    ~~~~~~~~~~~~~~~~~~~~~~~
    if (error)
        goto err_xxx;
    ...
    err_xxx:
        cleanup();
        return -ERRNO;


    PATTERN 2: ERR_PTR RETURN
    ~~~~~~~~~~~~~~~~~~~~~~~~~
    if (bad)
        return ERR_PTR(-EINVAL);
    return valid_ptr;


    PATTERN 3: ERR_PTR CHECK
    ~~~~~~~~~~~~~~~~~~~~~~~~
    ptr = func();
    if (IS_ERR(ptr))
        return PTR_ERR(ptr);


    PATTERN 4: ERROR PROPAGATION
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    ret = lower_func();
    if (ret)
        return ret;


    PATTERN 5: SINGLE OUT LABEL
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~
    int ret = 0;
    ...
    if (error) {
        ret = -ENOMEM;
        goto out;
    }
    ...
    out:
        cleanup();
        return ret;
```

---

## Exercises

### Exercise 1: Driver Probe/Remove

```
1. Find drivers/net/e1000e/netdev.c (or similar)
2. Locate probe function
3. Draw the cleanup graph
4. Verify remove() is mirror of successful probe
```

### Exercise 2: Filesystem Mount

```
1. Find fs/ext4/super.c
2. Locate ext4_fill_super()
3. Count goto labels
4. Trace error propagation to mount failure
```

### Exercise 3: Memory Error Paths

```
1. Find mm/mmap.c
2. Look at do_mmap_pgoff()
3. Note mix of NULL checks and error codes
4. Trace what happens when allocation fails
```

---

## Summary: Files to Read

| File | Content |
|------|---------|
| `include/linux/err.h` | ERR_PTR macros |
| `include/linux/errno.h` | Error code definitions |
| `fs/namei.c` | Heavy ERR_PTR usage |
| `fs/open.c` | Error propagation in VFS |
| `drivers/pci/pci-driver.c` | goto cleanup in driver |
| `kernel/module.c` | Module init error handling |
| Any driver probe function | Real-world goto cleanup |
