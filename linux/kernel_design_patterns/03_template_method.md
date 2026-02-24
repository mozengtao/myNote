# Module 3: Template Method

> **Pattern**: Template Method (GoF)
> **Kernel idioms**: Fixed algorithm skeleton with pluggable hooks, callback flows, initcall levels

---

## Mental Model

A fixed algorithm skeleton runs in one place; specific steps are delegated
to pluggable hooks so different "subclasses" can customize only those steps.

In C: the skeleton is a function that calls function pointers (or registered
callbacks) at defined points. The core owns the control flow; concrete
implementations own only the steps that vary.

```
  SKELETON (framework function)
  ┌────────────────────────────────────────┐
  │ step 1: invariant work (framework)     │
  │ step 2: invariant work (framework)     │
  │ step 3: ──── CALL HOOK ────────────────┼──→ concrete implementation
  │ step 4: invariant work (framework)     │
  │ step 5: ──── CALL HOOK ────────────────┼──→ concrete implementation
  │ step 6: invariant work (framework)     │
  └────────────────────────────────────────┘
```

**GoF mapping:**
- **AbstractClass** → the framework function (e.g. `vfs_read`, `copy_process`)
- **templateMethod()** → the skeleton function itself
- **primitiveOperation()** → the hooks (ops struct methods, callbacks)
- **ConcreteClass** → each driver/filesystem/subsystem that fills in the hooks

### Template Method vs. Strategy

Both use function pointers, but their intent differs:

| Aspect | Template Method | Strategy |
|--------|----------------|----------|
| Who owns control flow? | The framework | The caller/context |
| What varies? | Individual steps within a fixed flow | The entire algorithm |
| Typical kernel form | `vfs_read()` calling `f_op->read` | `task->sched_class->pick_next_task()` |

---

## In the Kernel (v3.2)

### Example 1: `vfs_read()` — The Textbook Case

`fs/read_write.c`, lines 364–390:

```c
ssize_t
vfs_read(struct file *file, char __user *buf,
         size_t count, loff_t *pos)
{
    ssize_t ret;

    /* Step 1: Check permissions (INVARIANT) */
    if (!(file->f_mode & FMODE_READ))
        return -EBADF;

    /* Step 2: Validate operation exists (INVARIANT) */
    if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
        return -EINVAL;

    /* Step 3: Verify userspace buffer (INVARIANT) */
    if (unlikely(!access_ok(VERIFY_WRITE, buf, count)))
        return -EFAULT;

    /* Step 4: Security/LSM check (INVARIANT) */
    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        count = ret;

        /* Step 5: THE ACTUAL READ — delegated to the hook */
        if (file->f_op->read)
            ret = file->f_op->read(file, buf, count, pos);
        else
            ret = do_sync_read(file, buf, count, pos);

        /* Step 6: Post-read accounting (INVARIANT) */
        if (ret > 0) {
            fsnotify_access(file);
            add_rchar(current, ret);
        }
        inc_syscr(current);
    }

    return ret;
}
```

Annotated flow:

```
  vfs_read()                    (the "template method")
  ├── check FMODE_READ          (invariant: framework)
  ├── check f_op exists         (invariant: framework)
  ├── access_ok(buf)            (invariant: framework)
  ├── rw_verify_area()          (invariant: security)
  │
  ├── f_op->read()  ◄────────── THE HOOK (varies per filesystem/driver)
  │   │
  │   ├── ext4:   ext4_file_read()
  │   ├── pipe:   pipe_read()
  │   ├── socket: sock_aio_read()
  │   └── ramfs:  (uses generic_file_aio_read via do_sync_read)
  │
  ├── fsnotify_access()         (invariant: notification)
  └── add_rchar()               (invariant: accounting)
```

**Why this matters:** Every filesystem gets permissions checking, security
hooks, and accounting for free. No filesystem can "forget" to check
`FMODE_READ` or skip `fsnotify` — the framework handles it.

### Example 2: `copy_process()` — Process Creation

`kernel/fork.c`, lines 1042–1240+:

```c
static struct task_struct *
copy_process(unsigned long clone_flags,
             unsigned long stack_start,
             struct pt_regs *regs,
             unsigned long stack_size,
             int __user *child_tidptr,
             struct pid *pid,
             int trace)
{
    /* INVARIANT: validate clone flags */
    if ((clone_flags & (CLONE_NEWNS|CLONE_FS)) == ...)
        return ERR_PTR(-EINVAL);

    /* INVARIANT: security check */
    retval = security_task_create(clone_flags);

    /* INVARIANT: duplicate task struct */
    p = dup_task_struct(current);

    /* INVARIANT: credentials */
    retval = copy_creds(p, clone_flags);

    /* INVARIANT: scheduler setup */
    sched_fork(p);

    /* HOOKS — each "copies" a subsystem, behavior depends on flags: */
    retval = copy_semundo(clone_flags, p);
    retval = copy_files(clone_flags, p);
    retval = copy_fs(clone_flags, p);
    retval = copy_sighand(clone_flags, p);
    retval = copy_signal(clone_flags, p);
    retval = copy_mm(clone_flags, p);
    retval = copy_namespaces(clone_flags, p);
    retval = copy_io(clone_flags, p);
    retval = copy_thread(clone_flags, stack_start, stack_size, p, regs);

    /* INVARIANT: assign PID, set up parent/child links */
    /* INVARIANT: wake up new task */
}
```

The skeleton is the overall "create a process" algorithm. The hooks are
the `copy_*` functions — each subsystem decides whether to share or
duplicate its state based on `clone_flags`. `copy_thread()` is
architecture-specific.

```
  copy_process()                 (the "template method")
  ├── validate flags             (invariant)
  ├── security check             (invariant)
  ├── dup_task_struct()          (invariant)
  ├── copy_creds()               (invariant)
  ├── sched_fork()               (invariant)
  │
  ├── copy_files()    ◄───────── HOOK: share or dup file table?
  ├── copy_fs()       ◄───────── HOOK: share or dup fs context?
  ├── copy_mm()       ◄───────── HOOK: share or dup address space?
  ├── copy_thread()   ◄───────── HOOK: arch-specific register setup
  │
  ├── assign PID                 (invariant)
  └── wake up                    (invariant)
```

**Why this matters:** The overall process creation flow is correct by
construction. Each `copy_*` function only handles its own subsystem.
Adding a new namespace type means adding a new `copy_*` call — not
rewriting the fork path.

### Example 3: Initcalls — Boot as a Template Method

`include/linux/init.h`, lines 196–210:

```c
#define pure_initcall(fn)        __define_initcall("0",fn,0)
#define core_initcall(fn)        __define_initcall("1",fn,1)
#define postcore_initcall(fn)    __define_initcall("2",fn,2)
#define arch_initcall(fn)        __define_initcall("3",fn,3)
#define subsys_initcall(fn)      __define_initcall("4",fn,4)
#define fs_initcall(fn)          __define_initcall("5",fn,5)
#define device_initcall(fn)      __define_initcall("6",fn,6)
#define late_initcall(fn)        __define_initcall("7",fn,7)
```

The boot sequence is the template:

```
  Boot sequence (template method)
  ├── Level 0: pure_initcall     ← memory, fundamentals
  ├── Level 1: core_initcall     ← core subsystems
  ├── Level 2: postcore_initcall ← post-core setup
  ├── Level 3: arch_initcall     ← architecture-specific
  ├── Level 4: subsys_initcall   ← subsystem registration
  ├── Level 5: fs_initcall       ← filesystem init
  ├── Level 6: device_initcall   ← device drivers (module_init)
  └── Level 7: late_initcall     ← late/optional init
```

The "template" is the boot order enforced by linker sections.
The "hooks" are the individual init functions placed at each level by
the `*_initcall` macros. Each subsystem declares when it needs to run
without knowing what runs before or after it.

**The mechanism:**

`__define_initcall` places a function pointer into a linker section
(`.initcall0.init`, `.initcall1.init`, etc.). At boot, `do_initcalls()`
iterates these sections in order, calling each function pointer.

Adding a new subsystem init = writing one function + one macro. No
editing of the boot sequence code.

### Real Code Path Walkthrough: `write()` to a Pipe — Template Method in Action

Trace `write(fd, "hello", 5)` where `fd` refers to the write end of a pipe.
The template method (`vfs_write`) provides the invariant skeleton; the hook
(`pipe_write`) does the pipe-specific work.

```
  USERSPACE
  ─────────
  write(pipe_fd, "hello", 5)
       │
       ▼
  fs/read_write.c:476 — SYSCALL_DEFINE3(write, ...)
  ┌───────────────────────────────────────────────────────────────┐
  │  file = fget_light(fd, &fput_needed);                         │
  │  pos = file_pos_read(file);                                   │
  │  ret = vfs_write(file, buf, count, &pos);   ◄── ENTER TEMPLATE│
  │  file_pos_write(file, pos);                                   │
  │  fput_light(file, fput_needed);                               │
  └──────────────────────────────────────────────┬────────────────┘
                                                 │
                                                 ▼
  fs/read_write.c:393 — vfs_write(file, "hello", 5, &pos)
  ┌──────────────────────────────────────────────────────────────┐
  │                                                              │
  │  STEP 1 (invariant): permission check                        │
  │  if (!(file->f_mode & FMODE_WRITE))                          │
  │      return -EBADF;                                          │
  │                                                              │
  │  STEP 2 (invariant): validate ops exist                      │
  │  if (!file->f_op || (!file->f_op->write && !f_op->aio_write))│
  │      return -EINVAL;                                         │
  │                                                              │
  │  STEP 3 (invariant): verify userspace buffer is readable     │
  │  if (unlikely(!access_ok(VERIFY_READ, buf, count)))          │
  │      return -EFAULT;                                         │
  │                                                              │
  │  STEP 4 (invariant): security / LSM check                    │
  │  ret = rw_verify_area(WRITE, file, &pos, count);             │
  │                                                              │
  │  STEP 5 (HOOK — varies per file type):                       │
  │  ret = file->f_op->write(file, "hello", 5, &pos);            │
  │       │                                                      │
  │       │  file->f_op == &write_pipefifo_fops                  │
  │       │  file->f_op->write == pipe_write                     │
  │       ▼                                                      │
  │  ┌────────────────────────────────────────────────────┐      │
  │  │  fs/pipe.c — pipe_write(file, "hello", 5, &ppos)   │      │
  │  │    acquire pipe->mutex                             │      │
  │  │    find/allocate pipe_buffer for this data         │      │
  │  │    copy_from_user(buf_page, "hello", 5)            │      │
  │  │    wake_up readers waiting on this pipe            │      │
  │  │    release pipe->mutex                             │      │
  │  │    return 5 (bytes written)                        │      │
  │  └────────────────────────────────────────────────────┘      │
  │                                                              │
  │  STEP 6 (invariant): post-write accounting                   │
  │  if (ret > 0) {                                              │
  │      fsnotify_modify(file);          /* inotify watchers */  │
  │      add_wchar(current, ret);        /* process accounting */│
  │  }                                                           │
  │  inc_syscw(current);                 /* syscall counter */   │
  │                                                              │
  └──────────────────────────────────────────────────────────────┘
```

**What the template guarantees:** Even a buggy pipe implementation cannot
skip the permission check (step 1), the LSM security hook (step 4), or the
accounting update (step 6). The framework owns those invariants. The pipe
only implements the "how do I actually move bytes?" part.

**What changes if this were ext4 instead of a pipe?** Only step 5 — the hook.
Instead of `pipe_write`, the VFS would call `do_sync_write` → `ext4_file_write`
→ page cache → block I/O. Steps 1–4 and 6 stay identical.

---

## The General Pattern in C

```c
/* The template method — owns the skeleton */
int
framework_operation(struct object *obj, ...)
{
    /* invariant step 1 */
    ret = validate(obj);
    if (ret)
        return ret;

    /* invariant step 2: locking */
    lock(obj);

    /* HOOK: delegated to the concrete type */
    ret = obj->ops->do_work(obj, ...);

    /* invariant step 3: accounting, notification */
    update_stats(obj, ret);

    /* invariant step 4: unlock */
    unlock(obj);

    return ret;
}
```

Callers always go through `framework_operation()`, never directly to
`obj->ops->do_work()`. The framework guarantees that locking, validation,
and accounting always happen.

---

## Why Template Method Here

Ensures a single, correct control flow (process creation, VFS read, boot)
while allowing many modules/drivers/subsystems to participate. The core
code encodes policy (ordering, security, accounting) that must not be
bypassed.

**What would break without it:**
- Each filesystem would need to remember to check `FMODE_READ`, call
  `rw_verify_area()`, and do `fsnotify_access()`. Some would forget.
- Process creation could skip security checks if a subsystem wired its
  own fork path.
- Boot ordering would be manual and fragile.

---

## Check Your Understanding

1. Trace one initcall from its macro to execution. Where is the "template"
   and where are the "steps" that vary?

2. Name one place where adding a new "step" does not require modifying
   the core algorithm, only registering a hook.

3. In `vfs_read()`, what invariant steps protect user space from a
   buggy filesystem driver?

4. Why does the kernel call `f_op->read()` from within `vfs_read()`
   instead of letting user space call the filesystem's read function
   directly?

---

Proceed to [Module 4: Iterator](04_iterator.md).
