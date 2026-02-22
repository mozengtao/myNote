# Phase 5 — Guided Code Walkthrough: Mock Character Device with Inheritance

## Objective

Implement a minimal character device that demonstrates all four kernel
OOP patterns in a single, cohesive example:

1. **Encapsulation** — state in structs, behavior in function pointer tables
2. **Inheritance** — struct embedding with two levels of derivation
3. **Polymorphism** — VFS dispatch through `file_operations`
4. **Lifetime management** — `kref` reference counting with `container_of`
   in the destructor

This is not a toy — this is the exact pattern used by real kernel drivers.

---

## Architecture

```
  ┌─────────────────────────────────────────────┐
  │  struct mock_char_device                     │
  │  ┌─────────────────────────────────────────┐ │
  │  │  struct base_char_device                │ │
  │  │  ┌────────────────────────────────────┐ │ │
  │  │  │  struct cdev  cdev                 │ │ │   ← kobject inside
  │  │  │    └── struct kobject kobj         │ │ │
  │  │  │         └── struct kref kref       │ │ │
  │  │  ├────────────────────────────────────┤ │ │
  │  │  │  struct kref  refcount             │ │ │   ← our own refcount
  │  │  └────────────────────────────────────┘ │ │
  │  ├─────────────────────────────────────────┤ │
  │  │  char buffer[128]                       │ │   ← derived-class data
  │  │  size_t buf_len                         │ │
  │  │  struct mutex lock                      │ │
  │  └─────────────────────────────────────────┘ │
  └─────────────────────────────────────────────┘
```

---

## Step 1: Define the Base Type

```c
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

struct base_char_device {
    struct cdev cdev;
    struct kref refcount;
};
```

**Design rationale:**

- `cdev` is embedded (not pointed to). This means `base_char_device`
  participates in the kobject hierarchy through `cdev.kobj`.
- `refcount` is separate from `cdev.kobj.kref`. Why? Because the
  `cdev` kobject has its own lifecycle managed by `cdev_add`/`cdev_del`.
  Our refcount tracks how many file descriptors (open files) reference
  this device. The two lifetimes are independent.

---

## Step 2: Define the Derived Type

```c
struct mock_char_device {
    struct base_char_device base;
    char buffer[128];
    size_t buf_len;
    struct mutex lock;
};
```

**Inheritance chain:**

```
  mock_char_device
    └── base_char_device
          ├── cdev
          │     └── kobject
          │           └── kref
          └── kref (our refcount)
```

This is two levels of embedding. Given any pointer in the chain, we
can recover any other level via `container_of`.

---

## Step 3: Recovery Macros

```c
static inline struct base_char_device *
to_base_dev(struct cdev *cdev)
{
    return container_of(cdev, struct base_char_device, cdev);
}

static inline struct mock_char_device *
to_mock_dev(struct base_char_device *base)
{
    return container_of(base, struct mock_char_device, base);
}
```

These are the downcast operations. Given a `struct cdev *` from the
inode, recover the full derived type in two steps:

```c
struct cdev *c = inode->i_cdev;
struct base_char_device *bdev = to_base_dev(c);
struct mock_char_device *mdev = to_mock_dev(bdev);
```

Or collapsed:

```c
struct mock_char_device *mdev =
    to_mock_dev(to_base_dev(inode->i_cdev));
```

---

## Step 4: The Destructor

```c
static void
mock_char_device_release(struct kref *ref)
{
    struct base_char_device *bdev;
    struct mock_char_device *mdev;

    bdev = container_of(ref, struct base_char_device, refcount);
    mdev = to_mock_dev(bdev);

    mutex_destroy(&mdev->lock);
    kfree(mdev);
}
```

**Critical points:**

1. The `kref_put` call passes `mock_char_device_release` as the
   release function. `kref` itself doesn't know what it's embedded in.

2. The function receives a `struct kref *` and must recover the
   containing `mock_char_device *` through two `container_of` steps.

3. `kfree(mdev)` frees the entire `mock_char_device`, which includes
   `base_char_device`, which includes `cdev`. The slab allocator
   reclaims the entire allocation.

4. `mutex_destroy` is called before `kfree`. Destroying a mutex that
   is still held would be a bug — but we must still call it for
   lockdep cleanup in debug builds.

---

## Step 5: Implement `open()`

```c
static int
mock_open(struct inode *inode, struct file *filp)
{
    struct base_char_device *bdev;
    struct mock_char_device *mdev;

    bdev = to_base_dev(inode->i_cdev);
    mdev = to_mock_dev(bdev);

    kref_get(&bdev->refcount);
    filp->private_data = mdev;

    return 0;
}
```

**What happens here:**

1. `inode->i_cdev` points to the `struct cdev` that was registered with
   `cdev_add()`. The VFS set this up when the device node was opened.

2. `to_base_dev(inode->i_cdev)` — downcast from `cdev *` to
   `base_char_device *` via `container_of`.

3. `to_mock_dev(bdev)` — downcast from `base_char_device *` to
   `mock_char_device *` via `container_of`.

4. `kref_get(&bdev->refcount)` — increment our reference count. This
   ensures the device won't be freed while this file descriptor exists.

5. `filp->private_data = mdev` — stash the derived pointer for use in
   `read()`, `write()`, and `release()`. This avoids repeating the
   `container_of` chain on every operation.

---

## Step 6: Implement `release()`

```c
static int
mock_release(struct inode *inode, struct file *filp)
{
    struct mock_char_device *mdev = filp->private_data;

    kref_put(&mdev->base.refcount, mock_char_device_release);
    return 0;
}
```

When the last file descriptor is closed, `kref_put` decrements the
refcount. If it hits zero, `mock_char_device_release` is called,
which frees the object.

**Why `inode` is not used here:** The `release` callback only fires
when the last reference to the `struct file` is dropped. By that point,
we already have the device pointer in `filp->private_data`.

---

## Step 7: Implement `read()`

```c
static ssize_t
mock_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    struct mock_char_device *mdev = filp->private_data;
    ssize_t retval;

    if (mutex_lock_interruptible(&mdev->lock))
        return -ERESTARTSYS;

    if (*ppos >= mdev->buf_len) {
        retval = 0;
        goto out;
    }

    if (*ppos + count > mdev->buf_len)
        count = mdev->buf_len - *ppos;

    if (copy_to_user(buf, mdev->buffer + *ppos, count)) {
        retval = -EFAULT;
        goto out;
    }

    *ppos += count;
    retval = count;

out:
    mutex_unlock(&mdev->lock);
    return retval;
}
```

**Key patterns:**

- `filp->private_data` retrieves the derived type directly — no
  `container_of` needed because we stashed it in `open()`.
- `mutex_lock_interruptible` — the lock protects `buffer` and `buf_len`.
- `copy_to_user` — mandatory for transferring data to userspace.
  Direct pointer dereference of `buf` would be a security vulnerability.
- Position tracking via `*ppos` allows sequential reads.

---

## Step 8: Implement `write()`

```c
static ssize_t
mock_write(struct file *filp, const char __user *buf,
           size_t count, loff_t *ppos)
{
    struct mock_char_device *mdev = filp->private_data;
    ssize_t retval;

    if (mutex_lock_interruptible(&mdev->lock))
        return -ERESTARTSYS;

    if (count > sizeof(mdev->buffer) - 1)
        count = sizeof(mdev->buffer) - 1;

    if (copy_from_user(mdev->buffer, buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    mdev->buffer[count] = '\0';
    mdev->buf_len = count;
    *ppos = count;
    retval = count;

out:
    mutex_unlock(&mdev->lock);
    return retval;
}
```

---

## Step 9: The vtable

```c
static const struct file_operations mock_fops = {
    .owner   = THIS_MODULE,
    .open    = mock_open,
    .release = mock_release,
    .read    = mock_read,
    .write   = mock_write,
};
```

This is the vtable. It is `const` — immutable after compilation. The
VFS will dispatch through these pointers:

```
  vfs_read(file, ...)
    → file->f_op->read(file, ...)
      → mock_read(file, ...)
```

---

## Step 10: Module Init/Exit

```c
static struct mock_char_device *mock_dev;
static dev_t dev_num;

static int __init
mock_init(void)
{
    int ret;

    mock_dev = kzalloc(sizeof(*mock_dev), GFP_KERNEL);
    if (!mock_dev)
        return -ENOMEM;

    mutex_init(&mock_dev->lock);
    kref_init(&mock_dev->base.refcount);

    ret = alloc_chrdev_region(&dev_num, 0, 1, "mockdev");
    if (ret < 0)
        goto fail_alloc;

    cdev_init(&mock_dev->base.cdev, &mock_fops);
    mock_dev->base.cdev.owner = THIS_MODULE;

    ret = cdev_add(&mock_dev->base.cdev, dev_num, 1);
    if (ret < 0)
        goto fail_cdev;

    return 0;

fail_cdev:
    unregister_chrdev_region(dev_num, 1);
fail_alloc:
    kfree(mock_dev);
    return ret;
}

static void __exit
mock_exit(void)
{
    cdev_del(&mock_dev->base.cdev);
    unregister_chrdev_region(dev_num, 1);
    kref_put(&mock_dev->base.refcount, mock_char_device_release);
}

module_init(mock_init);
module_exit(mock_exit);
MODULE_LICENSE("GPL");
```

**Initialization sequence:**

1. `kzalloc` — allocate the derived type, zero-initialized.
2. `mutex_init` — initialize the lock.
3. `kref_init` — set refcount to 1 (the module holds the initial
   reference).
4. `alloc_chrdev_region` — get a device number from the kernel.
5. `cdev_init` — initialize the `cdev`, binding it to our `file_operations`.
6. `cdev_add` — make the device live (visible to userspace).

**Teardown sequence:**

1. `cdev_del` — remove the device from the VFS. No new opens will
   succeed after this.
2. `unregister_chrdev_region` — release the device number.
3. `kref_put` — drop the module's reference. If no file descriptors
   are open, the device is freed. If files are still open, the device
   lives until the last `release()` call drops the refcount to zero.

This last point is critical: **module exit must not `kfree` the device
directly.** If a userspace process still has the device open, freeing
it would cause a use-after-free when that process calls `read()` or
`close()`.

---

## Complete Object Lifecycle

```
  insmod mockdev.ko
       │
       ▼
  mock_init()
       │  kzalloc(mock_char_device)     refcount = 1
       │  cdev_add()                    device visible
       │
  ═══════════════════════════════════════════════
       │
  open("/dev/mockdev")
       │  mock_open()                   refcount = 2
       │
  read(fd, buf, 128)
       │  mock_read()                   (no refcount change)
       │
  write(fd, "hello", 5)
       │  mock_write()                  (no refcount change)
       │
  open("/dev/mockdev")                  (another fd)
       │  mock_open()                   refcount = 3
       │
  close(fd1)
       │  mock_release()                refcount = 2
       │
  rmmod mockdev
       │  mock_exit()
       │  cdev_del()                    no new opens
       │  kref_put()                    refcount = 1
       │                                (NOT freed — fd2 still open)
       │
  close(fd2)
       │  mock_release()                refcount = 0
       │  mock_char_device_release()    → kfree(mdev)
       │
  ═══════════════════════════════════════════════
       DEVICE FREED
```

---

## Exercise: Verify Your Understanding

Trace the following scenario and answer:

1. Two processes open the device simultaneously. Process A writes
   "hello". Process B reads. What serialization guarantees does the
   mutex provide?

2. If `mock_open` forgot the `kref_get`, and the module is unloaded
   while a process has the device open, what happens on the next
   `read()`? Be specific about which pointer dereference fails.

3. If `mock_char_device_release` called `kfree(&mdev->base)` instead
   of `kfree(mdev)`, what memory would be leaked? Draw the layout.

4. What happens if `cdev_del` is called after `kref_put` in
   `mock_exit`? Describe the race condition.

5. Why is `mock_fops` declared `const`? What attack becomes possible
   if it is not?
