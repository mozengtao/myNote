# Case 3: Reboot Notifier Chain

The reboot notifier chain demonstrates Observer pattern for system lifecycle events where multiple subsystems must perform cleanup before shutdown.

---

## Subsystem Context

```
+=============================================================================+
|                    REBOOT NOTIFICATION                                       |
+=============================================================================+

    SYSTEM SHUTDOWN REQUIREMENTS:
    ============================

    When system reboots/halts, many subsystems need notification:

    Subsystem           Action Required
    ---------           ---------------
    Filesystems         Sync and unmount
    Block devices       Flush caches
    Network             Close connections
    USB                 Power down devices
    ACPI                Prepare for power off
    Watchdog            Disable before reboot
    LEDs/Display        Show shutdown status

    All must complete BEFORE power off!


    REBOOT NOTIFIER CHAIN:
    =====================

    sys_reboot()
         |
         | kernel_restart() / kernel_halt()
         v
    +----+----+
    | Notifier |
    | Chain    |
    +----+----+
         |
         | blocking_notifier_call_chain(&reboot_notifier_list, ...)
         v
    +----------+    +----------+    +----------+
    | USB      | -> | Watchdog | -> | Driver X |
    | shutdown |    | disable  |    | cleanup  |
    +----------+    +----------+    +----------+
         |              |               |
         v              v               v
    cleanup()      cleanup()       cleanup()
```

**中文说明：**

重启通知器链展示了观察者模式在系统生命周期事件中的应用。当系统重启或关机时，许多子系统需要通知：文件系统需要同步和卸载、块设备需要刷新缓存、网络需要关闭连接、USB需要断电、看门狗需要禁用。所有这些必须在断电前完成。

---

## The Reboot Notifier Chain

```
    CHAIN DECLARATION:
    ==================

    /* kernel/sys.c */
    BLOCKING_NOTIFIER_HEAD(reboot_notifier_list);

    /* Why BLOCKING_NOTIFIER_HEAD? */
    - Shutdown happens in process context
    - Cleanup operations may need to sleep
    - Must wait for all cleanups to complete
    - Cannot use atomic notifier (no sleeping)


    NOTIFICATION EVENTS:
    ====================

    Event               When Triggered
    -----               --------------
    SYS_DOWN            System going down for halt
    SYS_HALT            System halting
    SYS_POWER_OFF       System powering off
    SYS_RESTART         System restarting

    (Defined in include/linux/notifier.h)
```

---

## Key Functions

```c
/* Registration - kernel/sys.c */
int register_reboot_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_register(
        &reboot_notifier_list, nb);
}

int unregister_reboot_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_unregister(
        &reboot_notifier_list, nb);
}

/* Kernel restart path - kernel/sys.c */
void kernel_restart(char *cmd)
{
    kernel_restart_prepare(cmd);
    
    if (!cmd)
        printk(KERN_EMERG "Restarting system.\n");
    else
        printk(KERN_EMERG "Restarting system with cmd '%s'.\n", cmd);
    
    machine_restart(cmd);
}

void kernel_restart_prepare(char *cmd)
{
    /* Notify all registered handlers */
    blocking_notifier_call_chain(&reboot_notifier_list, 
                                 SYS_RESTART, cmd);
    system_state = SYSTEM_RESTART;
    device_shutdown();
}

/* Similar for halt/power_off */
void kernel_halt(void)
{
    kernel_shutdown_prepare(SYSTEM_HALT);
    printk(KERN_EMERG "System halted.\n");
    machine_halt();
}
```

**中文说明：**

重启通知器使用BLOCKING_NOTIFIER_HEAD，因为关机在进程上下文中发生，清理操作可能需要睡眠，必须等待所有清理完成。register_reboot_notifier注册订阅者，kernel_restart_prepare在实际重启前调用通知器链，确保所有订阅者完成清理。

---

## Minimal C Simulation

```c
/* Simplified reboot notifier simulation */

#include <stdio.h>
#include <stdlib.h>

/* Reboot events */
#define SYS_RESTART     0x0001
#define SYS_HALT        0x0002
#define SYS_POWER_OFF   0x0003

/* Return values */
#define NOTIFY_OK       0x0001
#define NOTIFY_DONE     0x0000

/* Notifier block */
struct notifier_block {
    int (*notifier_call)(struct notifier_block *nb,
                         unsigned long event, void *data);
    struct notifier_block *next;
    int priority;
};

/* Blocking notifier head (simplified) */
struct blocking_notifier_head {
    struct notifier_block *head;
    /* In kernel: also has rwsem */
};

static struct blocking_notifier_head reboot_notifier_list = {
    .head = NULL
};

/* Register to reboot chain */
int register_reboot_notifier(struct notifier_block *nb)
{
    struct notifier_block **p;
    
    /* Insert by priority */
    for (p = &reboot_notifier_list.head; *p; p = &(*p)->next) {
        if (nb->priority > (*p)->priority)
            break;
    }
    nb->next = *p;
    *p = nb;
    
    printf("[KERNEL] Registered reboot notifier (priority %d)\n", 
           nb->priority);
    return 0;
}

/* Call all reboot notifiers */
int blocking_notifier_call_chain(struct blocking_notifier_head *nh,
                                 unsigned long event, void *data)
{
    struct notifier_block *nb;
    
    printf("[KERNEL] Calling reboot notifier chain (event=%lu)\n", event);
    
    for (nb = nh->head; nb; nb = nb->next) {
        nb->notifier_call(nb, event, data);
    }
    return NOTIFY_OK;
}

/* ====== USB SUBSYSTEM (SUBSCRIBER 1) ====== */

int usb_reboot_notify(struct notifier_block *nb,
                      unsigned long event, void *data)
{
    switch (event) {
    case SYS_RESTART:
    case SYS_HALT:
    case SYS_POWER_OFF:
        printf("  [USB] Powering down all USB devices...\n");
        printf("  [USB] USB cleanup complete\n");
        break;
    }
    return NOTIFY_OK;
}

static struct notifier_block usb_notifier = {
    .notifier_call = usb_reboot_notify,
    .priority = 100,  /* High priority - USB first */
};

/* ====== WATCHDOG SUBSYSTEM (SUBSCRIBER 2) ====== */

int watchdog_reboot_notify(struct notifier_block *nb,
                           unsigned long event, void *data)
{
    switch (event) {
    case SYS_RESTART:
    case SYS_HALT:
    case SYS_POWER_OFF:
        printf("  [WATCHDOG] Disabling hardware watchdog...\n");
        printf("  [WATCHDOG] Watchdog disabled\n");
        break;
    }
    return NOTIFY_OK;
}

static struct notifier_block watchdog_notifier = {
    .notifier_call = watchdog_reboot_notify,
    .priority = 50,
};

/* ====== FILESYSTEM SUBSYSTEM (SUBSCRIBER 3) ====== */

int fs_reboot_notify(struct notifier_block *nb,
                     unsigned long event, void *data)
{
    switch (event) {
    case SYS_RESTART:
    case SYS_HALT:
    case SYS_POWER_OFF:
        printf("  [FS] Syncing all filesystems...\n");
        printf("  [FS] Filesystems synced\n");
        break;
    }
    return NOTIFY_OK;
}

static struct notifier_block fs_notifier = {
    .notifier_call = fs_reboot_notify,
    .priority = 0,  /* Lower priority - fs last */
};

/* ====== KERNEL SHUTDOWN PATH ====== */

void kernel_restart_prepare(const char *cmd)
{
    printf("\n[KERNEL] Preparing for restart...\n");
    blocking_notifier_call_chain(&reboot_notifier_list, 
                                 SYS_RESTART, (void *)cmd);
    printf("[KERNEL] All notifiers called\n");
}

void kernel_halt_prepare(void)
{
    printf("\n[KERNEL] Preparing for halt...\n");
    blocking_notifier_call_chain(&reboot_notifier_list,
                                 SYS_HALT, NULL);
    printf("[KERNEL] All notifiers called\n");
}

void kernel_restart(const char *cmd)
{
    kernel_restart_prepare(cmd);
    printf("\n[KERNEL] System is restarting now...\n");
    /* machine_restart() would be called here */
}

void kernel_halt(void)
{
    kernel_halt_prepare();
    printf("\n[KERNEL] System halted.\n");
    /* machine_halt() would be called here */
}

int main(void)
{
    printf("=== REBOOT NOTIFIER DEMONSTRATION ===\n\n");

    /* Subsystems register during boot */
    printf("--- Subsystem Registration (during boot) ---\n");
    register_reboot_notifier(&usb_notifier);
    register_reboot_notifier(&watchdog_notifier);
    register_reboot_notifier(&fs_notifier);

    /* User initiates reboot */
    printf("\n--- User Initiates Reboot ---\n");
    kernel_restart(NULL);

    return 0;
}

/*
 * Output:
 *
 * === REBOOT NOTIFIER DEMONSTRATION ===
 *
 * --- Subsystem Registration (during boot) ---
 * [KERNEL] Registered reboot notifier (priority 100)
 * [KERNEL] Registered reboot notifier (priority 50)
 * [KERNEL] Registered reboot notifier (priority 0)
 *
 * --- User Initiates Reboot ---
 *
 * [KERNEL] Preparing for restart...
 * [KERNEL] Calling reboot notifier chain (event=1)
 *   [USB] Powering down all USB devices...
 *   [USB] USB cleanup complete
 *   [WATCHDOG] Disabling hardware watchdog...
 *   [WATCHDOG] Watchdog disabled
 *   [FS] Syncing all filesystems...
 *   [FS] Filesystems synced
 * [KERNEL] All notifiers called
 *
 * [KERNEL] System is restarting now...
 */
```

---

## Real Kernel Subscribers (v3.2)

```
    REGISTERED REBOOT NOTIFIERS:
    ===========================

    File                              Function
    ----                              --------
    drivers/usb/core/hcd.c            usb_hcd_pci_shutdown
    drivers/watchdog/*.c              various watchdog_reboot
    drivers/acpi/sleep.c              acpi_reboot_notifier
    drivers/char/ipmi/ipmi_*.c        ipmi_reboot_notify
    drivers/md/md.c                   md_notify_reboot
    arch/x86/kernel/i8259.c           i8259A_shutdown
    drivers/cpufreq/cpufreq.c         cpufreq_reboot_notifier

    Each performs cleanup independently.
```

---

## What Kernel Core Does NOT Control

```
    Core Controls:
    --------------
    [X] When reboot is initiated
    [X] Calling the notifier chain
    [X] Waiting for all handlers to complete
    [X] Proceeding to machine_restart/halt

    Core Does NOT Control:
    ----------------------
    [ ] What each subsystem does for cleanup
    [ ] How long cleanup takes
    [ ] Which devices need special handling
    [ ] Subsystem-specific shutdown logic

    CRITICAL PROPERTY:
    ------------------
    All cleanup must complete BEFORE machine_restart().
    Blocking notifier ensures this - all callbacks run
    to completion before chain call returns.
```

**中文说明：**

内核核心控制：何时启动重启、调用通知器链、等待所有处理器完成、继续执行machine_restart/halt。内核核心不控制：每个子系统做什么清理、清理需要多长时间、哪些设备需要特殊处理、子系统特定的关机逻辑。关键属性：所有清理必须在machine_restart()之前完成，阻塞通知器确保这一点——所有回调运行完成后链调用才返回。

---

## Why Blocking Notifier Here

```
    ATOMIC vs BLOCKING:
    ===================

    Atomic Notifier:              Blocking Notifier:
    - Cannot sleep                - CAN sleep
    - RCU protected               - rwsem protected
    - For IRQ context             - For process context

    Shutdown MUST use Blocking because:
    -----------------------------------
    1. Filesystems need to sync (may sleep waiting for I/O)
    2. USB needs to communicate with devices (may sleep)
    3. Network may need to send FIN packets (may sleep)
    4. All cleanup should complete before power off

    Using atomic notifier would:
    - Prevent proper filesystem sync
    - Risk data corruption
    - Leave hardware in bad state
```

---

## Version

This case study is based on **Linux kernel v3.2**.

Key source files:
- `kernel/sys.c` - reboot_notifier_list and kernel_restart/halt
- `include/linux/notifier.h` - SYS_* event definitions
- `include/linux/reboot.h` - reboot API
