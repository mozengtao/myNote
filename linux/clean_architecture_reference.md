# Linux Kernel v3.2 — Clean Architecture Reference Tables

> Companion document to `clean_architecture_analysis.md`.
> Contains detailed interface definitions, function pointer inventories, source code
> locations, and dependency chain evidence for each subsystem.

---

## Table of Contents

1. [VFS Operations Reference](#1-vfs-operations-reference)
2. [Scheduler Interface Reference](#2-scheduler-interface-reference)
3. [Memory Management Interface Reference](#3-memory-management-interface-reference)
4. [Network Stack Interface Reference](#4-network-stack-interface-reference)
5. [Device Driver Model Reference](#5-device-driver-model-reference)
6. [Security Operations Reference](#6-security-operations-reference)
7. [Interrupt Handling Reference](#7-interrupt-handling-reference)
8. [Time Management Reference](#8-time-management-reference)
9. [Registration Mechanism Catalog](#9-registration-mechanism-catalog)
10. [Dependency Evidence: Include Chain Analysis](#10-dependency-evidence-include-chain-analysis)

---

## 1. VFS Operations Reference

### 1.1 `struct file_operations` — `include/linux/fs.h:1223-1247`

| # | Function Pointer | Signature | Purpose |
|---|-----------------|-----------|---------|
| 1 | `llseek` | `loff_t (*)(struct file *, loff_t, int)` | Seek |
| 2 | `read` | `ssize_t (*)(struct file *, char __user *, size_t, loff_t *)` | Synchronous read |
| 3 | `write` | `ssize_t (*)(struct file *, const char __user *, size_t, loff_t *)` | Synchronous write |
| 4 | `aio_read` | `ssize_t (*)(struct kiocb *, const struct iovec *, unsigned long, loff_t)` | Async read |
| 5 | `aio_write` | `ssize_t (*)(struct kiocb *, const struct iovec *, unsigned long, loff_t)` | Async write |
| 6 | `readdir` | `int (*)(struct file *, void *, filldir_t)` | Read directory |
| 7 | `poll` | `unsigned int (*)(struct file *, struct poll_table_struct *)` | Poll/select |
| 8 | `unlocked_ioctl` | `long (*)(struct file *, unsigned int, unsigned long)` | Device control |
| 9 | `compat_ioctl` | `long (*)(struct file *, unsigned int, unsigned long)` | 32-bit compat |
| 10 | `mmap` | `int (*)(struct file *, struct vm_area_struct *)` | Memory map |
| 11 | `open` | `int (*)(struct inode *, struct file *)` | Open file |
| 12 | `flush` | `int (*)(struct file *, fl_owner_t)` | Flush |
| 13 | `release` | `int (*)(struct inode *, struct file *)` | Close/release |
| 14 | `fsync` | `int (*)(struct file *, loff_t, loff_t, int)` | Sync to disk |
| 15 | `aio_fsync` | `int (*)(struct kiocb *, int)` | Async sync |
| 16 | `fasync` | `int (*)(int, struct file *, int)` | Async notification |
| 17 | `lock` | `int (*)(struct file *, int, struct file_lock *)` | File locking |
| 18 | `sendpage` | `ssize_t (*)(struct file *, struct page *, int, size_t, loff_t *, int)` | Send page |
| 19 | `get_unmapped_area` | `unsigned long (*)(struct file *, unsigned long, ...)` | mmap address |
| 20 | `check_flags` | `int (*)(int)` | Validate flags |
| 21 | `flock` | `int (*)(struct file *, int, struct file_lock *)` | Advisory lock |
| 22 | `splice_write` | `ssize_t (*)(struct pipe_inode_info *, struct file *, ...)` | Splice write |
| 23 | `splice_read` | `ssize_t (*)(struct file *, loff_t *, struct pipe_inode_info *, ...)` | Splice read |
| 24 | `setlease` | `int (*)(struct file *, long, struct file_lock **)` | Set lease |
| 25 | `fallocate` | `long (*)(struct file *, int, loff_t, loff_t)` | Preallocate |

### 1.2 `struct inode_operations` — `include/linux/fs.h:1249-1275`

| # | Function Pointer | Purpose |
|---|-----------------|---------|
| 1 | `lookup` | Directory entry lookup |
| 2 | `follow_link` | Follow symlink |
| 3 | `permission` | Permission check |
| 4 | `get_acl` | Get POSIX ACL |
| 5 | `readlink` | Read symlink target |
| 6 | `put_link` | Release symlink data |
| 7 | `create` | Create file |
| 8 | `link` | Create hard link |
| 9 | `unlink` | Remove file |
| 10 | `symlink` | Create symlink |
| 11 | `mkdir` | Create directory |
| 12 | `rmdir` | Remove directory |
| 13 | `mknod` | Create special file |
| 14 | `rename` | Rename entry |
| 15 | `truncate` | Truncate file |
| 16 | `setattr` | Set attributes |
| 17 | `getattr` | Get attributes |
| 18 | `setxattr` | Set extended attribute |
| 19 | `getxattr` | Get extended attribute |
| 20 | `listxattr` | List extended attributes |
| 21 | `removexattr` | Remove extended attribute |
| 22 | `truncate_range` | Truncate range |
| 23 | `fiemap` | File extent map |

### 1.3 `struct super_operations` — `include/linux/fs.h:1291-1319`

| # | Function Pointer | Purpose |
|---|-----------------|---------|
| 1 | `alloc_inode` | Allocate inode |
| 2 | `destroy_inode` | Destroy inode |
| 3 | `dirty_inode` | Mark inode dirty |
| 4 | `write_inode` | Write inode to disk |
| 5 | `drop_inode` | Drop inode |
| 6 | `evict_inode` | Evict inode |
| 7 | `put_super` | Release superblock |
| 8 | `write_super` | Write superblock |
| 9 | `sync_fs` | Sync filesystem |
| 10 | `freeze_fs` | Freeze filesystem |
| 11 | `unfreeze_fs` | Unfreeze filesystem |
| 12 | `statfs` | Get filesystem stats |
| 13 | `remount_fs` | Remount filesystem |
| 14 | `umount_begin` | Start unmount |
| 15 | `show_options` | Show mount options |
| 16 | `show_devname` | Show device name |
| 17 | `show_path` | Show path |
| 18 | `show_stats` | Show stats |
| 19 | `quota_read` | Read quota |
| 20 | `quota_write` | Write quota |
| 21 | `bdev_try_to_free_page` | Free block device page |
| 22 | `nr_cached_objects` / `free_cached_objects` | Cache management |

### 1.4 `struct dentry_operations` — `include/linux/dcache.h:158-172`

| # | Function Pointer | Purpose |
|---|-----------------|---------|
| 1 | `d_revalidate` | Revalidate dentry |
| 2 | `d_hash` | Hash name |
| 3 | `d_compare` | Compare names |
| 4 | `d_delete` | Decide cache retention |
| 5 | `d_release` | Release dentry |
| 6 | `d_prune` | Prune dentry |
| 7 | `d_iput` | Release inode |
| 8 | `d_dname` | Dynamic name |
| 9 | `d_automount` | Automount trigger |
| 10 | `d_manage` | Manage transit |

### 1.5 `struct address_space_operations` — `include/linux/fs.h:419-449`

| # | Function Pointer | Purpose |
|---|-----------------|---------|
| 1 | `writepage` | Write page to backing store |
| 2 | `readpage` | Read page from backing store |
| 3 | `writepages` | Write multiple dirty pages |
| 4 | `set_page_dirty` | Mark page dirty |
| 5 | `readpages` | Read-ahead multiple pages |
| 6 | `write_begin` | Start buffered write |
| 7 | `write_end` | Complete buffered write |
| 8 | `bmap` | Logical to physical block |
| 9 | `invalidatepage` | Invalidate page |
| 10 | `releasepage` | Release private page data |
| 11 | `freepage` | Free page |
| 12 | `direct_IO` | Direct I/O bypass |
| 13 | `get_xip_mem` | Execute in place |
| 14 | `migratepage` | Page migration |
| 15 | `launder_page` | Clean page before freeing |
| 16 | `is_partially_uptodate` | Partial read optimization |
| 17 | `error_remove_page` | Remove page on error |

### 1.6 ext4 Implementation Cross-Reference

| VFS Interface | ext4 Symbol | File:Line |
|--------------|-------------|-----------|
| `file_operations` (regular) | `ext4_file_operations` | `fs/ext4/file.c:231-248` |
| `file_operations` (dir) | `ext4_dir_operations` | `fs/ext4/dir.c:41-51` |
| `inode_operations` (file) | `ext4_file_inode_operations` | `fs/ext4/file.c:250-262` |
| `inode_operations` (dir) | `ext4_dir_inode_operations` | `fs/ext4/namei.c:2571-2590` |
| `inode_operations` (special) | `ext4_special_inode_operations` | `fs/ext4/namei.c:2592-2601` |
| `super_operations` | `ext4_sops` | `fs/ext4/super.c:1271-1290` |
| `super_operations` (no journal) | `ext4_nojournal_sops` | `fs/ext4/super.c:1292-1309` |
| `address_space_operations` | `ext4_da_aops` (+3 variants) | `fs/ext4/inode.c:3024-3082` |
| `file_system_type` | `ext4_fs_type` | `fs/ext4/super.c:5012-5018` |

---

## 2. Scheduler Interface Reference

### 2.1 `struct sched_class` — `include/linux/sched.h:834-877`

| # | Function Pointer | Purpose |
|---|-----------------|---------|
| 1 | `next` | Chain to next class (not a callback) |
| 2 | `enqueue_task` | Add task to runqueue |
| 3 | `dequeue_task` | Remove task from runqueue |
| 4 | `yield_task` | Voluntary yield |
| 5 | `yield_to_task` | Yield to specific task |
| 6 | `check_preempt_curr` | Check if new task preempts current |
| 7 | `pick_next_task` | Choose next task |
| 8 | `put_prev_task` | Put previous task back |
| 9 | `select_task_rq` | Select CPU (SMP) |
| 10 | `pre_schedule` | Before schedule (SMP) |
| 11 | `post_schedule` | After schedule (SMP) |
| 12 | `task_waking` | Task about to wake (SMP) |
| 13 | `task_woken` | Task just woken (SMP) |
| 14 | `set_cpus_allowed` | Change allowed CPUs (SMP) |
| 15 | `rq_online` | Runqueue online (SMP) |
| 16 | `rq_offline` | Runqueue offline (SMP) |
| 17 | `set_curr_task` | Mark as current task |
| 18 | `task_tick` | Timer tick |
| 19 | `task_fork` | Fork setup |
| 20 | `switched_from` | Left this class |
| 21 | `switched_to` | Entered this class |
| 22 | `prio_changed` | Priority changed |
| 23 | `get_rr_interval` | Round-robin timeslice |
| 24 | `task_move_group` | Group migration (fair groups) |

### 2.2 Implementation Locations

| Class | Symbol | File:Line |
|-------|--------|-----------|
| Stop | `stop_sched_class` | `kernel/sched_stoptask.c:83-106` |
| Real-Time | `rt_sched_class` | `kernel/sched_rt.c:1803-1832` |
| CFS (Fair) | `fair_sched_class` | `kernel/sched_fair.c:5044-5079` |
| Idle | `idle_sched_class` | `kernel/sched_idletask.c:74-97` |

### 2.3 Core Entity: `task_struct`

**Location:** `include/linux/sched.h:924-1210` (partial — scheduling fields only)

| Field | Type | Purpose |
|-------|------|---------|
| `state` | `volatile long` | Run state |
| `on_rq` | `int` | On runqueue flag |
| `prio` | `int` | Dynamic priority |
| `static_prio` | `int` | Static priority |
| `normal_prio` | `int` | Normal priority |
| `rt_priority` | `unsigned int` | RT priority (0-99) |
| `sched_class` | `const struct sched_class *` | **Ops pointer** |
| `se` | `struct sched_entity` | CFS scheduling entity |
| `rt` | `struct sched_rt_entity` | RT scheduling entity |
| `policy` | `unsigned int` | Scheduling policy |
| `cpus_allowed` | `cpumask_t` | CPU affinity mask |

---

## 3. Memory Management Interface Reference

### 3.1 `struct vm_operations_struct` — `include/linux/mm.h:204-242`

| # | Function Pointer | Signature | Purpose |
|---|-----------------|-----------|---------|
| 1 | `open` | `void (*)(struct vm_area_struct *)` | VMA opened |
| 2 | `close` | `void (*)(struct vm_area_struct *)` | VMA closed |
| 3 | `fault` | `int (*)(struct vm_area_struct *, struct vm_fault *)` | Page fault |
| 4 | `page_mkwrite` | `int (*)(struct vm_area_struct *, struct vm_fault *)` | Write fault |
| 5 | `access` | `int (*)(struct vm_area_struct *, unsigned long, void *, int, int)` | Remote access |
| 6 | `set_policy` | `int (*)(struct vm_area_struct *, struct mempolicy *)` | NUMA set |
| 7 | `get_policy` | `struct mempolicy *(*)(struct vm_area_struct *, unsigned long)` | NUMA get |
| 8 | `migrate` | `int (*)(struct vm_area_struct *, ...)` | NUMA migrate |

### 3.2 Core Entities

| Entity | File:Line | Key Fields |
|--------|-----------|------------|
| `struct page` | `include/linux/mm_types.h:39-160` | `flags`, `mapping`, `_mapcount`, `_count`, `lru` |
| `struct vm_area_struct` | `include/linux/mm_types.h:200-255` | `vm_start`, `vm_end`, `vm_ops`, `vm_file`, `vm_flags` |
| `struct mm_struct` | `include/linux/mm_types.h:287-386` | `mmap`, `mm_rb`, `pgd`, `mmap_sem` |
| `struct zone` | `include/linux/mmzone.h:297-436` | `watermark[]`, `free_area[]`, `lru[]` |
| `struct pglist_data` | `include/linux/mmzone.h:626-659` | `node_zones[]`, `node_zonelists[]` |

---

## 4. Network Stack Interface Reference

### 4.1 `struct proto_ops` — `include/linux/net.h:161-208`

| # | Function Pointer | Purpose |
|---|-----------------|---------|
| 1 | `release` | Close socket |
| 2 | `bind` | Bind address |
| 3 | `connect` | Connect socket |
| 4 | `socketpair` | Create pair |
| 5 | `accept` | Accept connection |
| 6 | `getname` | Get socket name |
| 7 | `poll` | Poll for events |
| 8 | `ioctl` | Device control |
| 9 | `compat_ioctl` | 32-bit compat |
| 10 | `listen` | Listen for connections |
| 11 | `shutdown` | Shutdown socket |
| 12 | `setsockopt` | Set option |
| 13 | `getsockopt` | Get option |
| 14 | `compat_setsockopt` / `compat_getsockopt` | Compat options |
| 15 | `sendmsg` | Send message |
| 16 | `recvmsg` | Receive message |
| 17 | `mmap` | Memory map socket |
| 18 | `sendpage` | Sendfile support |
| 19 | `splice_read` | Splice support |

### 4.2 `struct net_device_ops` — `include/linux/netdevice.h:612-678`

Selected key hooks (40+ total):

| # | Function Pointer | Purpose |
|---|-----------------|---------|
| 1 | `ndo_init` | Device init |
| 2 | `ndo_open` | Device up |
| 3 | `ndo_stop` | Device down |
| 4 | `ndo_start_xmit` | **Transmit packet** (required) |
| 5 | `ndo_select_queue` | TX queue selection |
| 6 | `ndo_set_rx_mode` | Address filtering |
| 7 | `ndo_set_mac_address` | MAC change |
| 8 | `ndo_validate_addr` | Validate address |
| 9 | `ndo_do_ioctl` | Device ioctl |
| 10 | `ndo_change_mtu` | MTU change |
| 11 | `ndo_tx_timeout` | TX watchdog |
| 12 | `ndo_get_stats64` | Statistics |

### 4.3 Core Entities

| Entity | File:Line |
|--------|-----------|
| `struct socket` | `include/linux/net.h:127-152` |
| `struct sock` | `include/net/sock.h:236-341` |
| `struct sk_buff` | `include/linux/skbuff.h:364` |
| `struct net_device` | `include/linux/netdevice.h:682-928` |

---

## 5. Device Driver Model Reference

### 5.1 `struct bus_type` — `include/linux/device.h:86-106`

| # | Function Pointer | Purpose |
|---|-----------------|---------|
| 1 | `match` | Match device to driver |
| 2 | `uevent` | Generate uevent |
| 3 | `probe` | Probe device |
| 4 | `remove` | Remove device |
| 5 | `shutdown` | Shutdown |
| 6 | `suspend` | Suspend |
| 7 | `resume` | Resume |

### 5.2 `struct block_device_operations` — `include/linux/blkdev.h:951-966`

| # | Function Pointer | Purpose |
|---|-----------------|---------|
| 1 | `open` | Open block device |
| 2 | `release` | Release block device |
| 3 | `ioctl` | Block ioctl |
| 4 | `compat_ioctl` | 32-bit compat |
| 5 | `direct_access` | DAX-style access |
| 6 | `check_events` | Media change |
| 7 | `media_changed` | Legacy media change |
| 8 | `unlock_native_capacity` | Unlock capacity |
| 9 | `revalidate_disk` | Revalidate |
| 10 | `getgeo` | Get geometry |
| 11 | `swap_slot_free_notify` | Swap notification |

### 5.3 Embedding Pattern

Bus-specific driver structs embed `struct device_driver`:

| Wrapper | Embedded Field | File |
|---------|---------------|------|
| `struct pci_driver` | `.driver` | `include/linux/pci.h:551-565` |
| `struct platform_driver` | `.driver` | `include/linux/platform_device.h:164-171` |
| `struct usb_driver` | `.drvwrap.driver` | `include/linux/usb.h` |

---

## 6. Security Operations Reference

### 6.1 `struct security_operations` — `include/linux/security.h:1380-1655`

**~185 function pointers.** Grouped summary:

| Category | Count | Key Hooks |
|----------|-------|-----------|
| Task/Trace | 2 | `ptrace_access_check`, `ptrace_traceme` |
| Capabilities | 3 | `capget`, `capset`, `capable` |
| System | 5 | `quotactl`, `syslog`, `settime`, `vm_enough_memory` |
| Exec (BPRM) | 5 | `bprm_set_creds`, `bprm_check_security`, `bprm_secureexec` |
| Superblock | 13 | `sb_mount`, `sb_umount`, `sb_statfs`, `sb_kern_mount` |
| Path | 11 | `path_mknod`, `path_mkdir`, `path_chmod`, `path_chroot` |
| Inode | 28 | `inode_permission`, `inode_create`, `inode_setxattr` |
| File | 12 | `file_permission`, `file_mmap`, `file_ioctl`, `dentry_open` |
| Task/Cred | 25 | `task_create`, `task_kill`, `cred_prepare`, `task_setnice` |
| IPC | 15 | `ipc_permission`, `msg_queue_*`, `shm_*`, `sem_*` |
| Network | 30 | `socket_create`, `socket_bind`, `socket_sendmsg`, `sk_alloc_security` |
| XFRM | ~10 | `xfrm_policy_alloc_security`, `xfrm_state_alloc_security` |
| Keys | 4 | `key_alloc`, `key_free`, `key_permission` |
| Audit | 4 | `audit_rule_init`, `audit_rule_match` |
| Other | ~18 | `d_instantiate`, `getprocattr`, `setprocattr`, `secctx_*` |

### 6.2 Implementation Locations

| LSM | Ops Symbol | File:Line |
|-----|-----------|-----------|
| SELinux | `selinux_ops` | `security/selinux/hooks.c:5453` |
| AppArmor | `apparmor_ops` | `security/apparmor/lsm.c:624` |
| SMACK | `smack_ops` | `security/smack/smack_lsm.c:3484` |
| TOMOYO | `tomoyo_security_ops` | `security/tomoyo/tomoyo.c` |
| Capabilities | via `security_fixup_ops()` | `security/capability.c:875` |

---

## 7. Interrupt Handling Reference

### 7.1 `struct irq_chip` — `include/linux/irq.h:295-330`

| # | Function Pointer | Purpose |
|---|-----------------|---------|
| 1 | `irq_startup` | Start interrupt |
| 2 | `irq_shutdown` | Shut down interrupt |
| 3 | `irq_enable` | Enable interrupt |
| 4 | `irq_disable` | Disable interrupt |
| 5 | `irq_ack` | Acknowledge interrupt |
| 6 | `irq_mask` | Mask interrupt |
| 7 | `irq_mask_ack` | Acknowledge and mask |
| 8 | `irq_unmask` | Unmask interrupt |
| 9 | `irq_eoi` | End of interrupt |
| 10 | `irq_set_affinity` | Set CPU affinity |
| 11 | `irq_retrigger` | Retrigger IRQ |
| 12 | `irq_set_type` | Set trigger type |
| 13 | `irq_set_wake` | Configure wake-on-IRQ |
| 14 | `irq_bus_lock` | Lock slow bus |
| 15 | `irq_bus_sync_unlock` | Sync unlock bus |
| 16 | `irq_cpu_online` | CPU online |
| 17 | `irq_cpu_offline` | CPU offline |
| 18 | `irq_suspend` | Suspend |
| 19 | `irq_resume` | Resume |
| 20 | `irq_pm_shutdown` | PM shutdown |
| 21 | `irq_print_chip` | Debug output |

### 7.2 Core Entities

| Entity | File:Line |
|--------|-----------|
| `struct irq_desc` | `include/linux/irqdesc.h:40-73` |
| `struct irqaction` | `include/linux/interrupt.h:110-122` |
| `irq_flow_handler_t` | `include/linux/irq.h:35-36` |

### 7.3 Flow Handler Implementations

| Handler | File | Purpose |
|---------|------|---------|
| `handle_level_irq` | `kernel/irq/chip.c:344` | Level-triggered IRQs |
| `handle_edge_irq` | `kernel/irq/chip.c:477` | Edge-triggered IRQs |
| `handle_fasteoi_irq` | `kernel/irq/chip.c:403` | Fast EOI IRQs |
| `handle_simple_irq` | `kernel/irq/chip.c:310` | Simple IRQs |
| `handle_percpu_irq` | `kernel/irq/chip.c:525` | Per-CPU IRQs |

---

## 8. Time Management Reference

### 8.1 `struct clocksource` — `include/linux/clocksource.h:166-198`

| # | Function Pointer | Purpose |
|---|-----------------|---------|
| 1 | `read` | Read cycle count |
| 2 | `enable` | Enable clocksource |
| 3 | `disable` | Disable clocksource |
| 4 | `suspend` | Suspend callback |
| 5 | `resume` | Resume callback |

Additional fields: `name`, `rating`, `mult`, `shift`, `mask`, `max_idle_ns`, `flags`.

### 8.2 `struct clock_event_device` — `include/linux/clockchips.h:82-107`

| # | Function Pointer | Purpose |
|---|-----------------|---------|
| 1 | `event_handler` | Timer event callback |
| 2 | `set_next_event` | Program next event (cycles) |
| 3 | `set_next_ktime` | Program next event (ktime) |
| 4 | `set_mode` | Set operating mode |
| 5 | `broadcast` | Broadcast to CPUs |

Additional fields: `name`, `features`, `rating`, `mult`, `shift`, `min_delta_ns`,
`max_delta_ns`, `irq`, `cpumask`.

### 8.3 Timer Entity Types

| Entity | File:Line | Resolution | Use Case |
|--------|-----------|-----------|----------|
| `struct timer_list` | `include/linux/timer.h:12-34` | Jiffies (~1-10ms) | Kernel timeouts |
| `struct hrtimer` | `include/linux/hrtimer.h:107-118` | Nanoseconds | High-precision timers |

---

## 9. Registration Mechanism Catalog

### 9.1 Complete Registration Function Inventory

| Subsystem | Registration Function | File | Purpose |
|-----------|----------------------|------|---------|
| **VFS** | `register_filesystem()` | `fs/filesystems.c:69` | Register filesystem type |
| **VFS** | `cdev_add()` | `fs/char_dev.c:472` | Register character device |
| **Block** | `register_blkdev()` | `block/genhd.c:282` | Register block major |
| **Block** | `add_disk()` | `block/genhd.c:499` | Add disk device |
| **Network** | `register_netdev()` | `net/core/dev.c:5759` | Register network device |
| **Network** | `sock_register()` | `net/socket.c:2460` | Register protocol family |
| **Network** | `dev_add_pack()` | `net/core/dev.c:413` | Register packet handler |
| **Network** | `proto_register()` | `net/core/sock.c:2514` | Register protocol |
| **Driver** | `driver_register()` | `drivers/base/driver.c:222` | Register device driver |
| **Driver** | `pci_register_driver()` | `include/linux/pci.h:940` | Register PCI driver |
| **Driver** | `platform_driver_register()` | `drivers/base/platform.c:470` | Register platform driver |
| **Security** | `register_security()` | `security/security.c:111` | Register LSM |
| **IRQ** | `request_irq()` | `include/linux/interrupt.h:133` | Register IRQ handler |
| **IRQ** | `request_threaded_irq()` | `kernel/irq/manage.c:1317` | Register threaded IRQ |
| **IRQ** | `irq_set_chip()` | `kernel/irq/chip.c:26` | Set interrupt chip |
| **Time** | `clocksource_register()` | `kernel/time/clocksource.c:734` | Register clocksource |
| **Time** | `clockevents_register_device()` | `kernel/time/clockevents.c:281` | Register clock event |
| **Scheduler** | `sched_fork()` | `kernel/sched.c:3012` | Assign sched_class at fork |

### 9.2 Registration Pattern Categories

**Pattern A: Global Function Pointer Replacement**
- LSM: `security_ops = ops`
- Only one implementation active at a time

**Pattern B: Linked List Registration**
- Filesystems: `file_systems` linked list
- Network protocol families: `net_families[]` array
- Clock sources: ordered list by rating
- IRQ handlers: `irqaction` chain on `irq_desc`

**Pattern C: Object Field Assignment**
- Scheduler: `task->sched_class = &fair_sched_class`
- VFS: `inode->i_fop = &ext4_file_operations`
- Network: `sock->ops = &inet_stream_ops`
- VMA: `vma->vm_ops = &generic_file_vm_ops`

**Pattern D: Map/Table Registration**
- Character devices: `cdev_map` (kobj_map)
- Network devices: `dev_base_head` + hash tables
- Block devices: `major_names[]` array

---

## 10. Dependency Evidence: Include Chain Analysis

### 10.1 Inward Dependencies (Correct Direction)

| Implementation File | Includes Core Header |
|--------------------|---------------------|
| `fs/ext4/file.c` | `#include <linux/fs.h>` |
| `fs/ext4/super.c` | `#include <linux/fs.h>`, `<linux/vfs.h>` |
| `kernel/sched_fair.c` | `#include <linux/sched.h>` |
| `kernel/sched_rt.c` | `#include <linux/sched.h>` |
| `security/selinux/hooks.c` | `#include <linux/security.h>` |
| `security/apparmor/lsm.c` | `#include <linux/security.h>` |
| `drivers/net/.../e1000_main.c` | `#include <linux/netdevice.h>`, `<linux/pci.h>` |
| `drivers/char/mem.c` | `#include <linux/device.h>` |
| `arch/x86/kernel/apic/io_apic.c` | `#include <linux/irq.h>` |
| `arch/x86/kernel/tsc.c` | `#include <linux/clocksource.h>` |

### 10.2 Core Headers — Verified Clean

| Core Header | Includes FS-specific? | Includes Driver-specific? | Includes Arch-specific? |
|------------|----------------------|--------------------------|------------------------|
| `include/linux/fs.h` | No (except `nfs_fs_i.h` for lock union) | No | No (uses `asm/` generics) |
| `include/linux/sched.h` | No | No | No |
| `include/linux/mm.h` | No | No | No |
| `include/linux/net.h` | No | No | No |
| `include/linux/netdevice.h` | No | No | No |
| `include/linux/device.h` | No | No | No (uses `asm/device.h`) |
| `include/linux/security.h` | No | No | No |
| `include/linux/irq.h` | No | No | No (uses `asm/irq.h`) |
| `include/linux/clocksource.h` | No | No | No |
| `include/linux/clockchips.h` | No | No | No |

### 10.3 Dependency Violations

| Core File | Violating Include | Severity |
|-----------|------------------|----------|
| `kernel/sched.c` | `#include "sched_fair.c"`, `"sched_rt.c"`, etc. | **High** — core includes implementations |
| `include/linux/fs.h` | `#include <linux/nfs_fs_i.h>` | **Minimal** — small lock struct only |
| `include/linux/netdevice.h` | Contains `ip_ptr`, `ip6_ptr` fields | **Low** — protocol pointers in device struct |

---

*Generated from Linux Kernel v3.2.0 source analysis.*
