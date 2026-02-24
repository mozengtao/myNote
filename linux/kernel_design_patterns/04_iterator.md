# Module 4: Iterator

> **Pattern**: Iterator (GoF)
> **Kernel idioms**: `seq_file` protocol, `list_for_each_entry` macros, cursor-based traversal

---

## Mental Model

Access elements of a collection in a defined order without exposing
internal structure. In C: a "cursor" or position state plus "next"
(and sometimes "start"/"stop") functions. The collection type is often
opaque to the consumer.

```
  COLLECTION (opaque internal structure)
  ┌───────────────────────────────────────┐
  │  [A] ──→ [B] ──→ [C] ──→ [D]          │  (could be list, tree, hash...)
  └───────────────────────────────────────┘
        ▲
        │
  ┌─────┴──────────┐
  │   ITERATOR     │
  │  .start()      │  → position at first element
  │  .next()       │  → advance to next element
  │  .stop()       │  → release resources
  │  .show()       │  → render current element
  └────────────────┘
```

**GoF mapping:**
- **Iterator interface** → `struct seq_operations` (or macro-based traversal)
- **ConcreteIterator** → each `seq_operations` instance per /proc file
- **Aggregate** → the collection being iterated (list, tree, table)
- **Client** → VFS read path / `seq_file` framework

---

## In the Kernel (v3.2)

### Pattern A: `list_for_each_entry` — Macro-Based Iteration

`include/linux/list.h`, lines 418–421:

```c
#define list_for_each_entry(pos, head, member)                  \
    for (pos = list_entry((head)->next, typeof(*pos), member);  \
         &pos->member != (head);                                \
         pos = list_entry(pos->member.next, typeof(*pos), member))
```

This macro encapsulates the traversal of an intrusive linked list.
The caller never touches `.next`/`.prev` directly:

```c
struct my_device {
    struct list_head list;
    int id;
    char name[32];
};

struct list_head device_list;

/* Iterate — the list layout is hidden behind the macro */
struct my_device *dev;
list_for_each_entry(dev, &device_list, list) {
    printk("device: %s (id=%d)\n", dev->name, dev->id);
}
```

**What the macro hides:**

- `list_entry` is `container_of` — it converts a `list_head *` back to
  the enclosing struct pointer.
- The loop termination check (`&pos->member != (head)`) detects the
  circular list sentinel without the caller knowing it's circular.
- Variants exist for safe removal during iteration (`list_for_each_entry_safe`),
  reverse iteration (`list_for_each_entry_reverse`), and continue-from-point
  (`list_for_each_entry_continue`).

```
  Kernel list structure (circular, intrusive):

       ┌──────────────────────────────────────────┐
       │                                          │
       ▼                                          │
  ┌──────────┐     ┌─────────┐     ┌─────────┐    │
  │  HEAD    │──→  │ dev A   │──→  │ dev B   │──→─┘
  │(sentinel)│◄──  │.list    │◄──  │.list    │◄──
  └──────────┘     └─────────┘     └─────────┘

  list_for_each_entry iterates: dev A → dev B → (back to HEAD = stop)
```

### The Iterator Family

| Macro | Purpose |
|-------|---------|
| `list_for_each_entry` | Forward traversal, typed |
| `list_for_each_entry_reverse` | Backward traversal |
| `list_for_each_entry_safe` | Safe to delete current element during traversal |
| `list_for_each_entry_continue` | Resume from a given position |
| `hlist_for_each_entry` | Same for hash list (singly-linked bucket chains) |

---

### Pattern B: `seq_file` — Formal Iterator Protocol for /proc

`include/linux/seq_file.h`, lines 30–35:

```c
struct seq_operations {
    void * (*start) (struct seq_file *m, loff_t *pos);
    void   (*stop)  (struct seq_file *m, void *v);
    void * (*next)  (struct seq_file *m, void *v, loff_t *pos);
    int    (*show)  (struct seq_file *m, void *v);
};
```

This is a four-method **iterator interface**:

```
  seq_file framework                    Your implementation
  ──────────────────                    ────────────────────
  1. calls start(m, &pos)   ────────→  lock collection; return first element
         │
         ▼
  2. calls show(m, element)  ────────→  format element into buffer
         │
         ▼
  3. calls next(m, element, &pos) ──→  advance to next; return it (or NULL)
         │
         ├── if non-NULL: go to step 2
         │
         ▼
  4. calls stop(m, element)  ────────→  unlock collection; release resources
```

| Method | Role | C++ Equivalent |
|--------|------|---------------|
| `start` | Position at beginning; acquire lock | `begin()` + lock |
| `next` | Advance to next element | `++it` |
| `stop` | Release lock/resources (end of iteration) | destructor |
| `show` | Render current element to output | `operator<<` |

### seq_file Example: `/proc/modules`

`kernel/module.c` implements a seq_file iterator over the loaded modules list:

```c
static void *m_start(struct seq_file *m, loff_t *pos)
{
    mutex_lock(&module_mutex);
    return seq_list_start(&modules, *pos);
}

static void *m_next(struct seq_file *m, void *p, loff_t *pos)
{
    return seq_list_next(p, &modules, pos);
}

static void m_stop(struct seq_file *m, void *p)
{
    mutex_unlock(&module_mutex);
}

static int m_show(struct seq_file *m, void *p)
{
    struct module *mod = list_entry(p, struct module, list);
    /* format module info into seq_file buffer */
    seq_printf(m, "%s %u", mod->name, mod->init_size + mod->core_size);
    /* ... */
    return 0;
}

static const struct seq_operations modules_op = {
    .start  = m_start,
    .next   = m_next,
    .stop   = m_stop,
    .show   = m_show,
};
```

**What the framework hides:**

- Buffering: `seq_file` manages a kernel buffer, handles partial reads,
  and copies to userspace. The implementation just calls `seq_printf()`.
- Restart: if the buffer overflows mid-iteration, the framework calls
  `stop()` then `start()` again from the right position.
- Locking: by convention, `start()` acquires the lock and `stop()` releases
  it, ensuring the collection is stable during iteration.

### seq_file Architecture

```
  Userspace: read(fd, buf, 4096)
       │
       ▼
  VFS → seq_file framework
       │
       ├── calls .start(m, &pos)     → lock + position
       │
       ├── loop:
       │     ├── .show(m, element)   → render to kernel buffer
       │     ├── .next(m, elem, &pos)→ advance cursor
       │     └── (repeat until buffer full or NULL)
       │
       ├── calls .stop(m, last)      → unlock
       │
       └── copy_to_user(buf, kernel_buffer, count)
```

### Real Code Path Walkthrough: `cat /proc/modules` Through `seq_read`

Trace what happens when userspace runs `cat /proc/modules`. The seq_file
framework drives the iterator protocol; the module-specific `m_start`/
`m_next`/`m_show`/`m_stop` provide the collection-specific logic.

```
  USERSPACE
  ─────────
  fd = open("/proc/modules", O_RDONLY)
       │   → proc file_operations.open → seq_open(file, &modules_op)
       │     allocates struct seq_file, attaches it as file->private_data
       │     sets m->op = &modules_op
       │
  read(fd, buf, 4096)
       │
       ▼
  fs/seq_file.c:133 — seq_read(file, buf, 4096, &ppos)
  ┌──────────────────────────────────────────────────────────────────┐
  │  m = file->private_data;     /* our seq_file context */          │
  │  mutex_lock(&m->lock);                                           │
  │                                                                  │
  │  /* allocate kernel-side buffer if first read */                 │
  │  if (!m->buf)                                                    │
  │      m->buf = kmalloc(PAGE_SIZE, GFP_KERNEL);                    │
  │                                                                  │
  │  ──── ITERATOR START ────                                        │
  │  pos = m->index;                                                 │
  │  p = m->op->start(m, &pos);    ◄── calls m_start()               │
  │       │                                                          │
  │       │  kernel/module.c — m_start():                            │
  │       │    mutex_lock(&module_mutex);     /* lock the list */    │
  │       │    return seq_list_start(&modules, *pos);                │
  │       │      → walks the modules list to position *pos           │
  │       │      → returns list_head pointer to first module         │
  │       ▼                                                          │
  │  ──── ITERATE LOOP ────                                          │
  │  while (1) {                                                     │
  │      err = m->op->show(m, p);    ◄── calls m_show()              │
  │           │                                                      │
  │           │  kernel/module.c — m_show():                         │
  │           │    mod = list_entry(p, struct module, list);         │
  │           │    seq_printf(m, "%s %u ...", mod->name, ...);       │
  │           │      → writes "e1000e 245760 ..." into m->buf        │
  │           ▼                                                      │
  │      if (m->count < m->size)   /* buffer has room */             │
  │          goto Fill;             /* try to fit more records */    │
  │      /* buffer full — stop, grow buffer, restart */              │
  │  }                                                               │
  │                                                                  │
  │  Fill:                                                           │
  │  while (m->count < size) {                                       │
  │      p = m->op->next(m, p, &next);   ◄── calls m_next()          │
  │           │                                                      │
  │           │  kernel/module.c — m_next():                         │
  │           │    return seq_list_next(p, &modules, pos);           │
  │           │      → advance to next module in the list            │
  │           │      → returns next list_head or NULL                │
  │           ▼                                                      │
  │      if (!p) break;             /* end of module list */         │
  │      err = m->op->show(m, p);   /* render next module */         │
  │  }                                                               │
  │                                                                  │
  │  ──── ITERATOR STOP ────                                         │
  │  m->op->stop(m, p);    ◄── calls m_stop()                        │
  │       │                                                          │
  │       │  kernel/module.c — m_stop():                             │
  │       │    mutex_unlock(&module_mutex);   /* release lock */     │
  │       ▼                                                          │
  │  ──── COPY TO USERSPACE ────                                     │
  │  copy_to_user(buf, m->buf, n);                                   │
  │  *ppos += copied;                                                │
  │  mutex_unlock(&m->lock);                                         │
  │  return copied;                                                  │
  └──────────────────────────────────────────────────────────────────┘
```

**Key observation:** `seq_read` never knows it's iterating over modules. It
just calls `start`/`show`/`next`/`stop` through the `seq_operations` vtable.
The same `seq_read` function handles `/proc/meminfo`, `/proc/net/tcp`,
`/proc/interrupts` — hundreds of different collections, all using the same
iterator protocol. The collection (modules list), the locking strategy
(module_mutex), and the formatting (seq_printf) are all encapsulated inside
the four iterator methods.

---

## Why Iterator Here

### For `list_for_each_entry`:

Keeps the list structure private and stable. Callers iterate without
knowing whether the list is circular, singly-linked, or hashed. If the
kernel changes the list implementation, iterators still work.

### For `seq_file`:

Provides a safe, standard way to expose kernel data structures to
userspace through `/proc` and `/sys`. Without it, every `/proc` file
would need to implement its own buffering, partial-read handling,
and locking strategy.

**What would break without it:**

- Direct list walking by every consumer means every consumer must know
  the list layout, handle `container_of`, and manage locking.
- `/proc` files without `seq_file` would have ad-hoc buffer management,
  leading to truncation bugs, races, and inconsistent output.

---

## Where Else the Pattern Appears

| Iterator | Collection | Location |
|----------|-----------|----------|
| `list_for_each_entry` | Any `list_head`-based list | `include/linux/list.h` |
| `hlist_for_each_entry` | Hash table bucket chains | `include/linux/list.h` |
| `rbtree iteration` | Red-black trees | `include/linux/rbtree.h` |
| `idr_for_each` | IDR (integer-to-pointer map) | `include/linux/idr.h` |
| `radix_tree_for_each_*` | Radix tree entries | `include/linux/radix-tree.h` |
| `seq_file` | Any /proc or /sys file | `fs/seq_file.c` |
| `for_each_process` | All `task_struct` instances | `include/linux/sched.h` |
| `for_each_online_cpu` | Online CPU set | `include/linux/cpumask.h` |

---

## Pitfalls

1. **Modifying during iteration.** Removing an element from a `list_head`
   while iterating with `list_for_each_entry` corrupts the traversal.
   Use `list_for_each_entry_safe` instead.

2. **Locking.** `seq_file` start/stop must bracket the lock. If `show()`
   drops the lock, the collection may change between `show()` and `next()`.

3. **Position restart.** `seq_file` may call `start()` multiple times if
   the buffer fills up. The `loff_t *pos` must be a stable cursor that
   can resume iteration from the right point.

4. **Hash vs. list.** `hlist_for_each_entry` has a different signature
   (extra `node` parameter in v3.2). Don't mix list and hlist macros.

---

## Check Your Understanding

1. Implement a tiny `seq_file` "iterator" over a list you define.
   Identify the "cursor" and the "next" step in the API.

2. Why might the kernel prefer an explicit iterator API over giving
   every caller a pointer to the head and letting them walk the list?

3. What role does `start()`/`stop()` play in the seq_file protocol
   that `list_for_each_entry` doesn't provide?

4. Find one `seq_operations` in `fs/proc/`. What collection does it
   iterate? What lock does `start()` acquire?

---

Proceed to [Module 5: State](05_state.md).
