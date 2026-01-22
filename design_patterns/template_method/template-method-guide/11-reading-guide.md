# Learning Path and Source Reading Guide (v3.2)

## Overview

```
+=============================================================================+
|                    LEARNING PATH OVERVIEW                                    |
+=============================================================================+

    RECOMMENDED ORDER:
    
    1. VFS (Simplest to understand)
       |
       +---> fs/read_write.c
       +---> include/linux/fs.h
       
    2. Device Model (Clear lifecycle)
       |
       +---> drivers/base/core.c
       +---> drivers/base/dd.c
       +---> include/linux/device.h
       
    3. Network TX (State machine)
       |
       +---> net/core/dev.c
       +---> include/linux/netdevice.h
       
    4. NAPI (Budget management)
       |
       +---> net/core/dev.c (net_rx_action)
       +---> include/linux/netdevice.h
       
    5. Block Layer (Complex merging)
       |
       +---> block/blk-core.c
       +---> include/linux/blkdev.h
```

**中文说明：**

推荐学习顺序：(1) VFS——最容易理解，从`fs/read_write.c`和`include/linux/fs.h`开始；(2) 设备模型——清晰的生命周期，看`drivers/base/core.c`、`drivers/base/dd.c`；(3) 网络TX——状态机模式，看`net/core/dev.c`；(4) NAPI——预算管理，同在`net/core/dev.c`；(5) 块层——复杂的合并逻辑，看`block/blk-core.c`。

---

## Phase 1: VFS Template Methods

### Files to Read

| File | What to Look For |
|------|------------------|
| `fs/read_write.c` | `vfs_read()`, `vfs_write()`, `vfs_llseek()` |
| `fs/open.c` | `vfs_open()`, `do_sys_open()` |
| `fs/file_table.c` | `fget()`, `fput()` reference counting |
| `include/linux/fs.h` | `struct file_operations`, `struct inode_operations` |
| `fs/ext4/file.c` | Example filesystem: `ext4_file_operations` |

### Key Functions to Trace

```
    vfs_read() TRACE:
    =================
    
    fs/read_write.c:361
    +--------------------------------------------------+
    | ssize_t vfs_read(struct file *file,              |
    |                  char __user *buf,               |
    |                  size_t count,                   |
    |                  loff_t *pos)                    |
    +--------------------------------------------------+
    
    LOOK FOR:
    1. Line ~365: FMODE_READ check (validation)
    2. Line ~370: f_op check (ops table present?)
    3. Line ~375: rw_verify_area() (permission)
    4. Line ~380: f_op->read() or do_sync_read() (HOOK)
    5. Line ~385: fsnotify_access() (notification)
    6. Line ~386: inc_syscr() (statistics)
```

### Reading Exercise

```
    EXERCISE 1: Trace a read system call

    1. Open fs/read_write.c
    2. Find vfs_read() function
    3. List all checks that happen BEFORE f_op->read()
    4. List all actions that happen AFTER f_op->read()
    5. Find where the actual filesystem code is called

    QUESTIONS TO ANSWER:
    - What happens if f_op->read is NULL?
    - Can a filesystem skip fsnotify?
    - Where is the lock acquired?
```

---

## Phase 2: Device Model Template Methods

### Files to Read

| File | What to Look For |
|------|------------------|
| `drivers/base/core.c` | `device_add()`, `device_del()` |
| `drivers/base/dd.c` | `driver_probe_device()`, `device_bind_driver()` |
| `drivers/base/bus.c` | `bus_add_device()`, `bus_probe_device()` |
| `include/linux/device.h` | `struct device`, `struct device_driver` |
| `drivers/pci/pci-driver.c` | Example: `pci_device_probe()` |

### Key Functions to Trace

```
    device_add() TRACE:
    ===================
    
    drivers/base/core.c:917
    +--------------------------------------------------+
    | int device_add(struct device *dev)               |
    +--------------------------------------------------+
    
    LOOK FOR:
    1. Line ~935: device_private_init() (initialization)
    2. Line ~955: kobject_add() (sysfs registration)
    3. Line ~980: device_create_file() (attributes)
    4. Line ~1020: bus_add_device() (bus registration)
    5. Line ~1040: bus_probe_device() (driver matching)
    6. Line ~1050: kobject_uevent() (userspace notification)
    
    NOTE: drv->probe() is called from bus_probe_device()
          via driver_probe_device() in dd.c
```

### Reading Exercise

```
    EXERCISE 2: Trace device registration

    1. Open drivers/base/core.c
    2. Find device_add() function
    3. Open drivers/base/dd.c
    4. Find driver_probe_device() function
    5. Trace how drv->probe() gets called

    QUESTIONS TO ANSWER:
    - What must happen before probe() is called?
    - What happens if probe() returns an error?
    - When is the uevent sent?
```

---

## Phase 3: Network TX Template Methods

### Files to Read

| File | What to Look For |
|------|------------------|
| `net/core/dev.c` | `dev_queue_xmit()`, `dev_hard_start_xmit()` |
| `net/sched/sch_generic.c` | `qdisc_run()`, `sch_direct_xmit()` |
| `include/linux/netdevice.h` | `struct net_device`, `struct net_device_ops` |
| `drivers/net/e1000/e1000_main.c` | Example: `e1000_xmit_frame()` |

### Key Functions to Trace

```
    dev_queue_xmit() TRACE:
    =======================
    
    net/core/dev.c:2458
    +--------------------------------------------------+
    | int dev_queue_xmit(struct sk_buff *skb)          |
    +--------------------------------------------------+
    
    LOOK FOR:
    1. Line ~2470: skb->dev validation
    2. Line ~2480: netdev_pick_tx() (queue selection)
    3. Line ~2490: __dev_xmit_skb() (qdisc handling)
    4. Line ~2520: HARD_TX_LOCK() (queue locking)
    5. Line ~2530: netif_tx_queue_stopped() check
    6. Line ~2540: ops->ndo_start_xmit() (HOOK)
    7. Line ~2550: HARD_TX_UNLOCK() (unlock)
```

### Reading Exercise

```
    EXERCISE 3: Trace packet transmission

    1. Open net/core/dev.c
    2. Find dev_queue_xmit() function
    3. Trace to dev_hard_start_xmit()
    4. Find where ndo_start_xmit is called

    QUESTIONS TO ANSWER:
    - What lock is held during ndo_start_xmit?
    - What checks happen before the driver is called?
    - What happens if driver returns NETDEV_TX_BUSY?
```

---

## Phase 4: NAPI Template Methods

### Files to Read

| File | What to Look For |
|------|------------------|
| `net/core/dev.c` | `net_rx_action()`, `napi_poll()` |
| `include/linux/netdevice.h` | `struct napi_struct`, `napi_schedule()` |
| `drivers/net/e1000/e1000_main.c` | Example: `e1000_clean()` |

### Key Functions to Trace

```
    net_rx_action() TRACE:
    ======================
    
    net/core/dev.c:4070
    +--------------------------------------------------+
    | static void net_rx_action(struct softirq_action *h)|
    +--------------------------------------------------+
    
    LOOK FOR:
    1. Line ~4080: budget = netdev_budget (budget init)
    2. Line ~4085: time_limit = jiffies + 2 (time limit)
    3. Line ~4100: list_for_each_entry_safe (poll list loop)
    4. Line ~4120: n->poll(n, weight) (HOOK - driver poll)
    5. Line ~4130: budget -= work (budget tracking)
    6. Line ~4140: napi_complete check
```

### Reading Exercise

```
    EXERCISE 4: Trace NAPI polling

    1. Open net/core/dev.c
    2. Find net_rx_action() function
    3. Find where driver's poll() is called
    4. Understand budget accounting

    QUESTIONS TO ANSWER:
    - What limits how long NAPI can run?
    - How is fairness between NAPI instances achieved?
    - When does NAPI re-enable interrupts?
```

---

## Phase 5: Block Layer Template Methods

### Files to Read

| File | What to Look For |
|------|------------------|
| `block/blk-core.c` | `submit_bio()`, `generic_make_request()` |
| `include/linux/blkdev.h` | `struct request_queue`, `struct bio` |
| `block/elevator.c` | I/O scheduler integration |
| `drivers/block/virtio_blk.c` | Example: `virtblk_make_request()` |

### Key Functions to Trace

```
    submit_bio() TRACE:
    ===================
    
    block/blk-core.c:1597
    +--------------------------------------------------+
    | void submit_bio(int rw, struct bio *bio)         |
    +--------------------------------------------------+
    
    LOOK FOR:
    1. Line ~1600: bio->bi_rw assignment
    2. Line ~1605: bio_has_data() check
    3. Line ~1610: task_io_account_*() (accounting)
    4. Line ~1615: generic_make_request() (continues to...)
    
    generic_make_request() (line ~1535):
    1. Line ~1550: q = bdev_get_queue() (get queue)
    2. Line ~1570: q->make_request_fn() (HOOK)
```

### Reading Exercise

```
    EXERCISE 5: Trace block I/O submission

    1. Open block/blk-core.c
    2. Find submit_bio() function
    3. Trace to generic_make_request()
    4. Find where make_request_fn is called

    QUESTIONS TO ANSWER:
    - What accounting happens before device sees bio?
    - How does plugging/merging relate to this path?
    - What happens on bio completion?
```

---

## Summary: What to Look For

```
+=============================================================================+
|                    READING CHECKLIST FOR EACH SUBSYSTEM                      |
+=============================================================================+

    For each template method function, identify:

    [ ] ENTRY POINT
        - What function do users/callers invoke?
        - What arguments does it take?

    [ ] PRE-HOOK STEPS
        - What validation happens?
        - What locks are acquired?
        - What security checks occur?
        - What state setup happens?

    [ ] HOOK POINT
        - Which ops table is used?
        - Which function pointer is called?
        - What arguments are passed to hook?

    [ ] POST-HOOK STEPS
        - What cleanup occurs?
        - What notifications are sent?
        - What statistics are updated?
        - What locks are released?

    [ ] ERROR HANDLING
        - What happens if pre-checks fail?
        - What happens if hook fails?
        - Is cleanup always performed?
```

**中文说明：**

每个子系统的阅读检查清单：(1) 入口点——用户调用什么函数、接受什么参数；(2) 钩子前步骤——什么验证发生、获取什么锁、进行什么安全检查、什么状态设置；(3) 钩子点——使用哪个ops表、调用哪个函数指针、传递什么参数；(4) 钩子后步骤——什么清理发生、发送什么通知、更新什么统计、释放什么锁；(5) 错误处理——前置检查失败会怎样、钩子失败会怎样、清理是否总是执行。

---

## Source File Quick Reference (v3.2)

| Subsystem | Template Method | File | Line (approx) |
|-----------|-----------------|------|---------------|
| VFS | `vfs_read()` | fs/read_write.c | 361 |
| VFS | `vfs_write()` | fs/read_write.c | 393 |
| VFS | `vfs_open()` | fs/open.c | 792 |
| Device | `device_add()` | drivers/base/core.c | 917 |
| Device | `driver_probe_device()` | drivers/base/dd.c | 204 |
| Network | `dev_queue_xmit()` | net/core/dev.c | 2458 |
| Network | `dev_hard_start_xmit()` | net/core/dev.c | 2295 |
| NAPI | `net_rx_action()` | net/core/dev.c | 4070 |
| Block | `submit_bio()` | block/blk-core.c | 1597 |
| Block | `generic_make_request()` | block/blk-core.c | 1535 |
