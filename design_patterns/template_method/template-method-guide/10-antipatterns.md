# Common Kernel Anti-Patterns (What to Avoid)

## Overview of Anti-Patterns

```
+=============================================================================+
|                    TEMPLATE METHOD ANTI-PATTERNS                             |
+=============================================================================+

    CORRECT PATTERN                        ANTI-PATTERNS
    ==============                         ==============

    Framework                              1. CALLBACK INVERSION
        |                                     Impl controls flow
        v
    +-------------+                        2. DRIVER-MANAGED LIFECYCLE
    | pre_step()  |                           Driver decides order
    +-------------+
        |                                  3. BROKEN INVARIANTS
        v                                     Impl can bypass checks
    +-------------+
    | ops->hook() |----> Implementation
    +-------------+
        |
        v
    +-------------+
    | post_step() |
    +-------------+

    Framework always in control            Implementation gains control
```

**中文说明：**

上图对比了正确的模板方法模式与三种反模式。正确模式中，框架始终控制流程。反模式包括：(1) 回调反转——实现控制流程；(2) 驱动管理生命周期——驱动决定顺序；(3) 破坏不变量——实现可以绕过检查。

---

## Anti-Pattern 1: Callback Inversion

### The Problem

```
    CALLBACK INVERSION:
    Implementation controls when/how framework code runs

    +----------------------------------------------------------+
    |  WRONG: Driver as entry point                            |
    +----------------------------------------------------------+
    |                                                          |
    |  /* Driver exports its own function */                   |
    |  ssize_t my_driver_read(file, buf, count) {              |
    |      // Driver decides to call VFS helpers               |
    |      if (i_feel_like_it)                                 |
    |          vfs_helper_function();                          |
    |                                                          |
    |      // Driver does work                                 |
    |      do_read();                                          |
    |                                                          |
    |      // Maybe call another helper?                       |
    |      if (some_condition)                                 |
    |          another_helper();                               |
    |  }                                                       |
    |                                                          |
    |  PROBLEMS:                                               |
    |  - Driver controls execution order                       |
    |  - Framework functions become optional                   |
    |  - Invariants not guaranteed                             |
    +----------------------------------------------------------+

    CORRECT: Framework as entry point
    +----------------------------------------------------------+
    |                                                          |
    |  /* VFS is the entry point */                            |
    |  ssize_t vfs_read(file, buf, count) {                    |
    |      pre_checks();        // Always runs                 |
    |      f_op->read(...);     // Driver runs here            |
    |      post_processing();   // Always runs                 |
    |  }                                                       |
    |                                                          |
    +----------------------------------------------------------+
```

**中文说明：**

回调反转是指实现层控制框架代码何时/如何运行。错误做法：驱动作为入口点，决定是否调用VFS辅助函数。问题：驱动控制执行顺序、框架函数变成可选、不变量无法保证。正确做法：VFS作为入口点，前置检查总是运行，驱动在中间运行，后处理总是运行。

### Real Example of What NOT to Do

```c
/*
 * ANTI-PATTERN: Callback Inversion
 * 
 * This would NEVER be accepted in kernel because the driver
 * controls when framework code runs.
 */

/* BAD: Driver decides when to do security checks */
ssize_t bad_driver_read(struct file *file, char __user *buf, size_t count)
{
    /* Driver starts work without checks */
    prepare_hardware();
    
    /* Driver decides "maybe I should check permissions" */
    if (paranoid_mode)
        check_permissions(file);  /* Optional?! */
    
    /* Driver does read */
    do_hardware_read(buf, count);
    
    /* Driver decides "maybe notify" */
    if (want_notifications)
        notify_something();  /* Optional?! */
    
    return count;
}

/* 
 * This is callback inversion because:
 * 1. Driver is the entry point (users call bad_driver_read)
 * 2. Framework functions are optional
 * 3. Driver controls ordering
 * 4. Invariants are not enforced
 */
```

### Why Kernel Prevents This

```
    KERNEL ARCHITECTURE PREVENTS INVERSION:

    +----------------------------------------------------------+
    | 1. SYSTEM CALLS GO THROUGH FRAMEWORK                     |
    |                                                          |
    |    sys_read() --> vfs_read() --> f_op->read()            |
    |    ^               ^              ^                      |
    |    |               |              |                      |
    |    Entry       Framework      Implementation             |
    |    point       control                                   |
    |                                                          |
    |    Users CANNOT directly call f_op->read()               |
    +----------------------------------------------------------+
    
    +----------------------------------------------------------+
    | 2. OPS TABLES ARE NOT EXPORTED                           |
    |                                                          |
    |    struct file_operations ext4_file_ops;                 |
    |    /* Not exported to userspace */                       |
    |    /* Cannot be called directly */                       |
    +----------------------------------------------------------+
    
    +----------------------------------------------------------+
    | 3. REVIEW PROCESS CATCHES INVERSION                      |
    |                                                          |
    |    Any patch that exposes implementation as entry        |
    |    point would be rejected by maintainers                |
    +----------------------------------------------------------+
```

---

## Anti-Pattern 2: Driver-Managed Lifecycle

### The Problem

```
    DRIVER-MANAGED LIFECYCLE:
    Driver decides when to register, initialize, or cleanup

    +----------------------------------------------------------+
    |  WRONG: Driver controls binding lifecycle                |
    +----------------------------------------------------------+
    |                                                          |
    |  int bad_driver_init(void) {                             |
    |      /* Driver decides order */                          |
    |      create_device_node();    /* Before sysfs?! */       |
    |      init_hardware();         /* Before PM setup?! */    |
    |      register_with_bus();     /* Too late? */            |
    |      notify_userspace();      /* Before ready?! */       |
    |  }                                                       |
    |                                                          |
    |  PROBLEMS:                                               |
    |  - Race conditions (device visible before ready)         |
    |  - PM not initialized when hardware accessed             |
    |  - Bus binding inconsistent                              |
    |  - Userspace notified too early/late                     |
    +----------------------------------------------------------+

    CORRECT: Framework controls lifecycle
    +----------------------------------------------------------+
    |                                                          |
    |  /* Framework (device_add) controls order */             |
    |  device_add(dev) {                                       |
    |      kobject_add();          /* 1. sysfs first */        |
    |      bus_add_device();       /* 2. then bus */           |
    |      pm_runtime_enable();    /* 3. then PM */            |
    |      drv->probe(dev);        /* 4. then driver */        |
    |      kobject_uevent();       /* 5. then notify */        |
    |  }                                                       |
    |                                                          |
    +----------------------------------------------------------+
```

**中文说明：**

驱动管理生命周期是指驱动决定何时注册、初始化或清理。错误做法：驱动决定顺序，可能在sysfs之前创建设备节点、在PM设置前初始化硬件、太晚注册总线、在准备好之前通知用户空间。问题：竞态条件（设备在准备好前可见）、访问硬件时PM未初始化、总线绑定不一致、用户空间通知时机错误。正确做法：框架（device_add）控制顺序。

### Real Example of What NOT to Do

```c
/*
 * ANTI-PATTERN: Driver-Managed Lifecycle
 * 
 * This shows what happens when driver tries to manage
 * its own lifecycle instead of using device model.
 */

/* BAD: Driver manages own registration */
int bad_driver_probe(struct pci_dev *pdev)
{
    struct my_device *mydev;
    int ret;

    mydev = kzalloc(sizeof(*mydev), GFP_KERNEL);

    /* BAD: Create /dev node BEFORE device is ready */
    ret = register_chrdev(MAJOR, "mydev", &my_fops);
    /* User can open() now, but device not initialized! */

    /* BAD: Initialize hardware AFTER user can access */
    init_hardware(pdev);  /* Race condition! */

    /* BAD: Create sysfs AFTER /dev node */
    device_create(myclass, NULL, MKDEV(MAJOR, 0), NULL, "mydev0");
    /* Ordering inconsistent with other drivers */

    /* BAD: Notify udev manually (bypassing framework) */
    kobject_uevent(&mydev->kobj, KOBJ_ADD);
    
    return 0;
}

/*
 * CORRECT: Use device model
 */
int good_driver_probe(struct pci_dev *pdev)
{
    struct my_device *mydev;
    
    mydev = kzalloc(sizeof(*mydev), GFP_KERNEL);

    /* Just initialize hardware */
    init_hardware(pdev);

    /* Register with misc device subsystem */
    /* Framework handles /dev, sysfs, uevent ordering */
    ret = misc_register(&mydev->miscdev);

    return ret;
}
```

### Why Kernel Enforces Lifecycle

```
    LIFECYCLE ENFORCEMENT REASONS:

    +----------------------------------------------------------+
    | 1. HOTPLUG REQUIRES CONSISTENT ORDERING                  |
    |                                                          |
    |    USB device plugged in:                                |
    |    - Hub driver detects                                  |
    |    - Device model creates struct device                  |
    |    - sysfs populated                                     |
    |    - Driver probed                                       |
    |    - udev creates /dev node                              |
    |                                                          |
    |    If driver controlled this, each USB driver            |
    |    would have different ordering!                        |
    +----------------------------------------------------------+
    
    +----------------------------------------------------------+
    | 2. POWER MANAGEMENT REQUIRES KNOWN STATE                 |
    |                                                          |
    |    PM core needs to know:                                |
    |    - Is device initialized?                              |
    |    - Is driver bound?                                    |
    |    - What's the power state?                             |
    |                                                          |
    |    If driver managed lifecycle, PM would be              |
    |    inconsistent across drivers                           |
    +----------------------------------------------------------+
    
    +----------------------------------------------------------+
    | 3. SYSFS REQUIRES HIERARCHY                              |
    |                                                          |
    |    /sys/devices/pci0000:00/0000:00:1f.0/...              |
    |                                                          |
    |    Parent must exist before child                        |
    |    Framework guarantees this ordering                    |
    +----------------------------------------------------------+
```

---

## Anti-Pattern 3: Broken Invariants

### The Problem

```
    BROKEN INVARIANTS:
    Implementation can bypass framework checks or constraints

    +----------------------------------------------------------+
    |  WRONG: Implementation bypasses checks                   |
    +----------------------------------------------------------+
    |                                                          |
    |  /* Framework has a check */                             |
    |  ssize_t framework_write(file, buf, count) {             |
    |      if (!can_write(file))                               |
    |          return -EPERM;                                  |
    |                                                          |
    |      return impl->write(file, buf, count);               |
    |  }                                                       |
    |                                                          |
    |  /* BAD: Implementation has backdoor */                  |
    |  ssize_t bad_impl_write(file, buf, count) {              |
    |      if (special_flag)                                   |
    |          return direct_write_bypassing_check();          |
    |      return normal_write();                              |
    |  }                                                       |
    |                                                          |
    +----------------------------------------------------------+
```

**中文说明：**

破坏不变量是指实现能够绕过框架检查或约束。错误做法：框架有检查，但实现有后门可以绕过。这破坏了框架的安全保证，使检查变得无用。

### Examples of Broken Invariants

```c
/*
 * ANTI-PATTERN: Broken Invariants
 * Various ways implementations can break framework guarantees
 */

/* BAD 1: Returning wrong value to skip post-processing */
int bad_hook_return(struct context *ctx)
{
    do_work(ctx);
    
    /* Return special value hoping framework skips cleanup */
    return MAGIC_SKIP_CLEANUP;  /* DON'T DO THIS */
}

/* BAD 2: Releasing lock inside hook */
int bad_hook_unlock(struct context *ctx)
{
    /* Framework holds ctx->lock during hook */
    mutex_unlock(&ctx->lock);  /* BAD: breaks invariant! */
    
    do_work_without_lock();    /* Race condition! */
    
    mutex_lock(&ctx->lock);    /* Try to restore, but damage done */
    return 0;
}

/* BAD 3: Calling back into framework */
int bad_hook_reenter(struct context *ctx)
{
    /* Framework calls this hook */
    
    /* BAD: Call back into framework */
    framework_function(ctx);  /* Recursion! Deadlock! */
    
    return 0;
}

/* BAD 4: Modifying framework state */
int bad_hook_modify_state(struct context *ctx)
{
    /* Modify state that framework expects unchanged */
    ctx->framework_state = INVALID;
    
    return 0;  /* Framework will malfunction */
}

/* BAD 5: Spawning thread to continue after return */
int bad_hook_escape(struct context *ctx)
{
    /* Return quickly, but start background work */
    schedule_work(&continue_later_work);
    
    return 0;
    /* Framework releases lock, but work continues using ctx! */
}
```

### How Kernel Prevents Broken Invariants

```
    INVARIANT PROTECTION MECHANISMS:

    +----------------------------------------------------------+
    | 1. API DESIGN PREVENTS BYPASS                            |
    |                                                          |
    |    - No "skip post-processing" return values             |
    |    - Lock assertion macros (lockdep)                     |
    |    - RCU read-side critical section checking             |
    +----------------------------------------------------------+
    
    +----------------------------------------------------------+
    | 2. LOCKDEP CATCHES LOCK MISUSE                           |
    |                                                          |
    |    If implementation releases lock:                      |
    |    -> lockdep warns: "lock released with wrong owner"    |
    |    -> Test infrastructure catches this                   |
    +----------------------------------------------------------+
    
    +----------------------------------------------------------+
    | 3. REVIEW PROCESS                                        |
    |                                                          |
    |    Kernel maintainers know the invariants                |
    |    Any code that could break them is rejected            |
    +----------------------------------------------------------+
    
    +----------------------------------------------------------+
    | 4. DOCUMENTATION                                         |
    |                                                          |
    |    /* This function is called with lock held */          |
    |    /* Do NOT release the lock */                         |
    |    int (*hook)(struct context *ctx);                     |
    +----------------------------------------------------------+
```

---

## Summary: Anti-Pattern Detection

```
+=============================================================================+
|                    ANTI-PATTERN DETECTION CHECKLIST                          |
+=============================================================================+

    [ ] CALLBACK INVERSION SIGNALS:
        - Driver function is called directly by userspace
        - Framework functions called optionally from driver
        - No clear entry point in framework code
        - Driver decides execution order

    [ ] DRIVER-MANAGED LIFECYCLE SIGNALS:
        - Driver creates device nodes manually
        - Driver sends uevents directly
        - Driver manages sysfs without device model
        - Registration order varies between drivers

    [ ] BROKEN INVARIANT SIGNALS:
        - Special return values to "escape" framework
        - Lock released inside hook
        - Callback into framework from hook
        - Background work accessing context after return
        - Framework state modified by hook

    IF ANY OF THESE ARE PRESENT:
    +----------------------------------------------------------+
    |  REFACTOR TO USE TEMPLATE METHOD CORRECTLY:              |
    |  1. Framework function as sole entry point               |
    |  2. Device model for lifecycle management                |
    |  3. Implementation respects framework invariants         |
    +----------------------------------------------------------+
```

**中文说明：**

反模式检测清单：(1) 回调反转信号——驱动函数被用户空间直接调用、框架函数从驱动可选调用、没有明确的框架入口点、驱动决定执行顺序；(2) 驱动管理生命周期信号——驱动手动创建设备节点、直接发送uevent、不通过设备模型管理sysfs、驱动间注册顺序不同；(3) 破坏不变量信号——特殊返回值"逃离"框架、在钩子中释放锁、从钩子回调框架、返回后后台工作访问上下文、钩子修改框架状态。如果存在任何这些信号，需要重构为正确使用模板方法。

---

## Why These Anti-Patterns Are Actively Prevented

| Anti-Pattern | Consequence | Prevention |
|--------------|-------------|------------|
| Callback Inversion | Security bypass, inconsistent behavior | System call design, unexported symbols |
| Driver-Managed Lifecycle | Race conditions, PM failures | Device model APIs, review process |
| Broken Invariants | Crashes, data corruption, deadlocks | lockdep, API design, code review |

The Linux kernel's architecture is designed to make these anti-patterns difficult to implement and easy to detect during review.
