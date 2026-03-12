# Appendix: Clean Architecture in Linux Kernel v3.2 -- Quick Reference

## A. Complete Interface-to-Implementation Mapping

This appendix provides a quick-reference for every abstraction struct analyzed,
its source location, and the concrete implementations that plug into it.

---

## A.1 Scheduler (`struct sched_class`)

**Interface Definition:** `include/linux/sched.h:1084-1126`

| Function Pointer | Purpose | CFS Implementation | RT Implementation |
|---|---|---|---|
| `enqueue_task` | Add task to runqueue | `enqueue_task_fair` | `enqueue_task_rt` |
| `dequeue_task` | Remove task from runqueue | `dequeue_task_fair` | `dequeue_task_rt` |
| `pick_next_task` | Select next task to run | `pick_next_task_fair` | `pick_next_task_rt` |
| `put_prev_task` | Relinquish current task | `put_prev_task_fair` | `put_prev_task_rt` |
| `check_preempt_curr` | Check if new task should preempt | `check_preempt_wakeup` | `check_preempt_curr_rt` |
| `task_tick` | Timer tick processing | `task_tick_fair` | `task_tick_rt` |
| `task_fork` | New task creation hook | `task_fork_fair` | _(not set)_ |
| `select_task_rq` | CPU selection for task | `select_task_rq_fair` | `select_task_rq_rt` |
| `set_curr_task` | Set current task on rq | `set_curr_task_fair` | `set_curr_task_rt` |
| `get_rr_interval` | Get timeslice length | `get_rr_interval_fair` | `get_rr_interval_rt` |

**Plugin Files:**
- `kernel/sched_fair.c:5044` -- `fair_sched_class`
- `kernel/sched_rt.c:1803` -- `rt_sched_class`
- `kernel/sched_stoptask.c:83` -- `stop_sched_class`
- `kernel/sched_idletask.c:74` -- `idle_sched_class`

---

## A.2 VFS (`struct file_operations`)

**Interface Definition:** `include/linux/fs.h:1220-1245`

| Function Pointer | Purpose | ext4 | NFS |
|---|---|---|---|
| `llseek` | File seek | `ext4_llseek` | `nfs_file_llseek` |
| `read` | Synchronous read | `do_sync_read` | `do_sync_read` |
| `write` | Synchronous write | `do_sync_write` | `do_sync_write` |
| `aio_read` | Async read | `generic_file_aio_read` | `nfs_file_read` |
| `aio_write` | Async write | `ext4_file_write` | `nfs_file_write` |
| `mmap` | Memory mapping | `ext4_file_mmap` | `nfs_file_mmap` |
| `open` | File open | `ext4_file_open` | `nfs_file_open` |
| `release` | File close | `ext4_release_file` | `nfs_file_release` |
| `fsync` | Sync to disk | `ext4_sync_file` | `nfs_file_fsync` |
| `lock` | File locking | _(not set)_ | `nfs_lock` |
| `splice_read` | Zero-copy read | `generic_file_splice_read` | `nfs_file_splice_read` |
| `splice_write` | Zero-copy write | `generic_file_splice_write` | `nfs_file_splice_write` |
| `fallocate` | Preallocate space | `ext4_fallocate` | _(not set)_ |

**Plugin Files:**
- `fs/ext4/file.c:230` -- `ext4_file_operations`
- `fs/nfs/file.c:610` -- `nfs_file_operations`

---

## A.3 VFS (`struct inode_operations`)

**Interface Definition:** `include/linux/fs.h:1247-1271`

| Function Pointer | Purpose |
|---|---|
| `lookup` | Directory entry lookup |
| `create` | Create new inode |
| `link` | Create hard link |
| `unlink` | Remove directory entry |
| `symlink` | Create symbolic link |
| `mkdir` | Create directory |
| `rmdir` | Remove directory |
| `mknod` | Create special file |
| `rename` | Rename entry |
| `permission` | Check access permissions |
| `setattr` | Set inode attributes |
| `getattr` | Get inode attributes |

---

## A.4 VFS (`struct super_operations`)

**Interface Definition:** `include/linux/fs.h` (near line 1150)

| Function Pointer | Purpose |
|---|---|
| `alloc_inode` | Allocate filesystem-specific inode |
| `destroy_inode` | Free filesystem-specific inode |
| `dirty_inode` | Mark inode as dirty |
| `write_inode` | Write inode to disk |
| `drop_inode` | Called when last ref dropped |
| `evict_inode` | Remove inode from memory |
| `put_super` | Release superblock |
| `sync_fs` | Sync filesystem |
| `statfs` | Get filesystem statistics |
| `remount_fs` | Remount with new options |
| `show_options` | Show mount options in /proc |

---

## A.5 Network (`struct proto_ops`)

**Interface Definition:** `include/linux/net.h:164-207`

| Function Pointer | Purpose | TCP (inet_stream_ops) | UDP (inet_dgram_ops) |
|---|---|---|---|
| `release` | Close socket | `inet_release` | `inet_release` |
| `bind` | Bind to address | `inet_bind` | `inet_bind` |
| `connect` | Connect to peer | `inet_stream_connect` | `inet_dgram_connect` |
| `accept` | Accept connection | `inet_accept` | `sock_no_accept` |
| `getname` | Get socket name | `inet_getname` | `inet_getname` |
| `poll` | Poll for events | `tcp_poll` | `udp_poll` |
| `listen` | Listen for connections | `inet_listen` | `sock_no_listen` |
| `shutdown` | Shutdown socket | `inet_shutdown` | `inet_shutdown` |
| `sendmsg` | Send message | `inet_sendmsg` | `inet_sendmsg` |
| `recvmsg` | Receive message | `inet_recvmsg` | `inet_recvmsg` |
| `sendpage` | Send page | `inet_sendpage` | `inet_sendpage` |

**Plugin Files:**
- `net/ipv4/af_inet.c:908` -- `inet_stream_ops` (TCP)
- `net/ipv4/af_inet.c` -- `inet_dgram_ops` (UDP)

---

## A.6 Network (`struct net_protocol`)

**Interface Definition:** `include/net/protocol.h:36-47`

| Function Pointer | Purpose |
|---|---|
| `handler` | Process incoming packet for this protocol |
| `err_handler` | Handle ICMP errors for this protocol |
| `gso_send_check` | GSO validation |
| `gso_segment` | GSO segmentation |
| `gro_receive` | GRO receive processing |
| `gro_complete` | GRO completion |

---

## A.7 Security (`struct security_operations`)

**Interface Definition:** `include/linux/security.h:1380-1524+`

| Hook Category | Example Hooks | Count |
|---|---|---|
| **Process/Capability** | `ptrace_access_check`, `capable`, `capget` | ~6 |
| **Binary Execution** | `bprm_set_creds`, `bprm_check_security` | ~5 |
| **Superblock** | `sb_alloc_security`, `sb_mount`, `sb_umount` | ~12 |
| **Inode** | `inode_create`, `inode_permission`, `inode_setattr` | ~20 |
| **File** | `file_permission`, `file_mmap`, `file_ioctl` | ~10 |
| **Task** | `task_create`, `task_kill`, `task_setscheduler` | ~15 |
| **IPC** | `msg_queue_msgsnd`, `shm_shmat` | ~15 |
| **Network** | `socket_create`, `socket_bind`, `sk_alloc_security` | ~20 |
| **Key Management** | `key_alloc`, `key_permission` | ~3 |
| **Audit** | `audit_rule_init`, `audit_rule_match` | ~5 |
| **Total** | | **~150+** |

**Plugin Files:**
- `security/selinux/hooks.c:5452` -- `selinux_ops`
- `security/apparmor/lsm.c:624` -- `apparmor_ops`
- `security/smack/smack_lsm.c` -- `smack_ops`
- `security/tomoyo/tomoyo.c` -- `tomoyo_security_ops`

---

## A.8 Interrupts (`struct irq_chip`)

**Interface Definition:** `include/linux/irq.h:267-332`

| Function Pointer | Purpose |
|---|---|
| `irq_startup` | Start up the IRQ |
| `irq_shutdown` | Shut down the IRQ |
| `irq_enable` | Enable the IRQ |
| `irq_disable` | Disable the IRQ |
| `irq_ack` | Acknowledge the IRQ |
| `irq_mask` | Mask (disable) the IRQ |
| `irq_mask_ack` | Mask and acknowledge |
| `irq_unmask` | Unmask (enable) the IRQ |
| `irq_eoi` | End-of-interrupt signal |
| `irq_set_affinity` | Set CPU affinity |
| `irq_set_type` | Set trigger type (edge/level) |
| `irq_set_wake` | Set wakeup capability |

---

## A.9 Timekeeping (`struct clocksource`)

**Interface Definition:** `include/linux/clocksource.h:166-201`

| Function Pointer / Field | Purpose | TSC | HPET |
|---|---|---|---|
| `read` | Read current cycle count | `read_tsc` | `read_hpet` |
| `resume` | Resume after suspend | `resume_tsc` | `hpet_resume_counter` |
| `name` | Human-readable name | `"tsc"` | `"hpet"` |
| `rating` | Quality rating (higher = better) | `300` | `250` |
| `mask` | Bitmask for valid bits | `CLOCKSOURCE_MASK(64)` | `HPET_MASK` |
| `flags` | Capability flags | `IS_CONTINUOUS \| MUST_VERIFY` | `IS_CONTINUOUS` |

**Plugin Files:**
- `arch/x86/kernel/tsc.c:757` -- `clocksource_tsc`
- `arch/x86/kernel/hpet.c:738` -- `clocksource_hpet`

---

## A.10 Timekeeping (`struct clock_event_device`)

**Interface Definition:** `include/linux/clockchips.h:82-108`

| Function Pointer | Purpose |
|---|---|
| `event_handler` | Called on clock event (tick, hrtimer) |
| `set_next_event` | Program next event (delta in ticks) |
| `set_next_ktime` | Program next event (absolute ktime) |
| `set_mode` | Switch device mode (periodic/oneshot/shutdown) |
| `broadcast` | Broadcast tick to other CPUs |

---

## A.11 Device Model (`struct bus_type`)

**Interface Definition:** `include/linux/device.h:86-106`

| Function Pointer | Purpose |
|---|---|
| `match` | Match device to driver |
| `uevent` | Generate uevent for userspace |
| `probe` | Probe device with driver |
| `remove` | Remove driver from device |
| `shutdown` | Shutdown device |
| `suspend` | Suspend device |
| `resume` | Resume device |

**Concrete Bus Types:**
- `platform_bus_type` -- `drivers/base/platform.c`
- `pci_bus_type` -- `drivers/pci/pci-driver.c`
- `usb_bus_type` -- `drivers/usb/core/driver.c`
- `i2c_bus_type` -- `drivers/i2c/i2c-core.c`
- `spi_bus_type` -- `drivers/spi/spi.c`

---

## B. Source File Index

### Core Logic (Use Case Layer)

| File | Subsystem | Role |
|---|---|---|
| `kernel/sched.c` | Process Mgmt | Core scheduler loop, `__schedule()`, `pick_next_task()` |
| `mm/memory.c` | Memory Mgmt | Page fault handler, `handle_mm_fault()` |
| `fs/read_write.c` | VFS | `vfs_read()`, `vfs_write()` |
| `fs/open.c` | VFS | `vfs_open()` |
| `fs/namei.c` | VFS | Path resolution |
| `net/socket.c` | Networking | Socket syscall handlers |
| `net/core/dev.c` | Networking | Network device core |
| `security/security.c` | Security | Security hook dispatch |
| `kernel/irq/handle.c` | IRQ | Generic IRQ event handling |
| `kernel/irq/chip.c` | IRQ | Flow handler implementations |
| `kernel/time/timekeeping.c` | Time | Generic timekeeping |
| `kernel/time/clockevents.c` | Time | Clock event management |

### Interface Definitions (Abstraction Layer)

| File | Key Structs |
|---|---|
| `include/linux/sched.h` | `sched_class`, `task_struct` |
| `include/linux/fs.h` | `file_operations`, `inode_operations`, `super_operations` |
| `include/linux/mm.h` | `vm_operations_struct` |
| `include/linux/mm_types.h` | `mm_struct`, `vm_area_struct` |
| `include/linux/net.h` | `proto_ops`, `net_proto_family`, `socket` |
| `include/net/protocol.h` | `net_protocol` |
| `include/net/sock.h` | `sock`, `proto` |
| `include/linux/security.h` | `security_operations` |
| `include/linux/irq.h` | `irq_chip` |
| `include/linux/irqdesc.h` | `irq_desc` |
| `include/linux/interrupt.h` | `irq_handler_t` |
| `include/linux/clocksource.h` | `clocksource` |
| `include/linux/clockchips.h` | `clock_event_device` |
| `include/linux/device.h` | `device_driver`, `bus_type` |
| `include/linux/platform_device.h` | `platform_driver` |
| `include/linux/pci.h` | `pci_driver` |

### Concrete Implementations (Plugin Layer)

| File | What it Implements |
|---|---|
| `kernel/sched_fair.c` | `fair_sched_class` (CFS) |
| `kernel/sched_rt.c` | `rt_sched_class` (Real-Time) |
| `kernel/sched_stoptask.c` | `stop_sched_class` |
| `kernel/sched_idletask.c` | `idle_sched_class` |
| `arch/x86/include/asm/pgtable.h` | x86 page table macros |
| `fs/ext4/file.c` | `ext4_file_operations` |
| `fs/ext4/namei.c` | `ext4_dir_inode_operations` |
| `fs/nfs/file.c` | `nfs_file_operations` |
| `net/ipv4/af_inet.c` | `inet_stream_ops`, `inet_dgram_ops` |
| `security/selinux/hooks.c` | `selinux_ops` |
| `security/apparmor/lsm.c` | `apparmor_ops` |
| `arch/x86/kernel/tsc.c` | `clocksource_tsc` |
| `arch/x86/kernel/hpet.c` | `clocksource_hpet` |
| `drivers/base/platform.c` | `platform_bus_type` |
| `drivers/pci/pci-driver.c` | `pci_bus_type` |

---

## C. Architectural Diagram: Full System View

```
+============================================================================+
|                          USER SPACE                                        |
|  read()  write()  recv()  send()  ioctl()  mmap()  fork()  exec()         |
+====+=========+=========+=========+=========+=========+=========+==========+
     |         |         |         |         |         |         |
     | SYSCALL BOUNDARY (Stable ABI)
     v         v         v         v         v         v         v
+============================================================================+
|                        CORE KERNEL (Inward)                                |
|                                                                            |
|  +-----------+  +-----------+  +-----------+  +-----------+  +-----------+ |
|  | VFS       |  | Scheduler |  | Memory    |  | Network   |  | Security  | |
|  | vfs_read  |  | schedule  |  | handle_   |  | sock_     |  | security_ | |
|  | vfs_write |  | pick_next |  | mm_fault  |  | sendmsg   |  | file_perm | |
|  +-----+-----+  +-----+-----+  +-----+-----+  +-----+-----+  +-----+-----+ |
|        |              |              |              |              |       |
|        v              v              v              v              v       |
|  +=====================================================================+   |
|  |              ABSTRACTION LAYER (Interface Structs)                  |   |
|  |                                                                     |   |
|  |  file_operations  sched_class  vm_ops    proto_ops  security_ops    |   |
|  |  inode_operations              pgd/pud/  net_proto  irq_chip        |   |
|  |  super_operations              pmd/pte   proto      clocksource     |   |
|  +=====================================================================+   |
|        |              |              |              |              |       |
+========|==============|==============|==============|==============|=======+
         |              |              |              |              |
         v              v              v              v              v
+============================================================================+
|                     PLUGIN LAYER (Outward)                                 |
|                                                                            |
|  +-------+  +-------+  +--------+  +-------+  +---------+  +----------+    |
|  | ext4  |  | CFS   |  | x86    |  | TCP   |  | SELinux |  | TSC      |    |
|  | NFS   |  | RT    |  | ARM    |  | UDP   |  | AppArmor|  | HPET     |    |
|  | procfs|  | Stop  |  | MIPS   |  | IPv6  |  | SMACK   |  | ACPI PM  |    |
|  | tmpfs |  | Idle  |  | PPC    |  | SCTP  |  | TOMOYO  |  | PIT      |    |
|  +-------+  +-------+  +--------+  +-------+  +---------+  +----------+    |
|                                                                            |
|  +---------------------------------------------------------------------+   |
|  |                    DEVICE DRIVERS                                   |   |
|  |  e1000  rtl8139  ata_piix  i915  snd-hda  usb-storage  ...          |   |
|  +---------------------------------------------------------------------+   |
|                                                                            |
+============================================================================+
|                         HARDWARE                                           |
|  CPU   RAM   NIC   Disk   GPU   Sound   USB   Timers   Interrupt Ctrl      |
+============================================================================+
```

**Dependencies flow INWARD (upward in this diagram).**
**Execution flows OUTWARD (downward through the layers).**

---

## D. Registration API Summary

| Subsystem | Registration Function | What Gets Registered |
|---|---|---|
| Scheduler | _(compile-time `.next` chain)_ | `struct sched_class` instances |
| VFS | `inode->i_fop = &ops` (at mount/inode creation) | `struct file_operations` |
| VFS | `sb->s_op = &ops` (at mount) | `struct super_operations` |
| Network | `sock_register(&family_ops)` | `struct net_proto_family` |
| Network | `inet_add_protocol(&proto, IPPROTO_*)` | `struct net_protocol` |
| Security | `register_security(&ops)` | `struct security_operations` |
| IRQ | `request_irq(irq, handler, flags, name, dev)` | `irq_handler_t` callback |
| IRQ | `irq_set_chip(irq, &chip)` | `struct irq_chip` |
| Clock | `clocksource_register_khz(&cs, khz)` | `struct clocksource` |
| Clock | `clockevents_register_device(&dev)` | `struct clock_event_device` |
| Platform | `platform_driver_register(&drv)` | `struct platform_driver` |
| PCI | `pci_register_driver(&drv)` | `struct pci_driver` |
| USB | `usb_register(&drv)` | `struct usb_driver` |
