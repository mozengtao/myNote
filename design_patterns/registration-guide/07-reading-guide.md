# Source Reading Guide: Registration Pattern

A guided path through Linux kernel v3.2 source code.

---

## Reading Path Overview

```
    PHASE 1: PCI Driver Registration
    ================================
    include/linux/pci.h           <- struct pci_driver
    drivers/pci/pci-driver.c      <- pci_register_driver()
    
    PHASE 2: Filesystem Registration
    ================================
    include/linux/fs.h            <- struct file_system_type
    fs/filesystems.c              <- register_filesystem()
    
    PHASE 3: USB Driver Registration
    ================================
    include/linux/usb.h           <- struct usb_driver
    drivers/usb/core/driver.c     <- usb_register()
```

---

## Phase 1: PCI Driver Registration

### File: include/linux/pci.h

```
    WHAT TO LOOK FOR:
    =================
    
    struct pci_driver definition:
    - name, id_table
    - probe, remove
    - suspend, resume
    
    struct pci_device_id:
    - vendor, device
    - subvendor, subdevice
    - class, class_mask
```

### File: drivers/pci/pci-driver.c

```
    WHAT TO LOOK FOR:
    =================
    
    pci_register_driver():
    - How driver is added to list
    - How existing devices are matched
    
    pci_match_device():
    - How ID matching works
```

**中文说明：**

阶段1：PCI驱动注册。在pci.h中查找pci_driver结构定义，在pci-driver.c中学习pci_register_driver如何将驱动添加到列表并匹配设备。

---

## Phase 2: Filesystem Registration

### File: include/linux/fs.h

```
    WHAT TO LOOK FOR:
    =================
    
    struct file_system_type:
    - name
    - mount callback
    - kill_sb callback
```

### File: fs/filesystems.c

```
    WHAT TO LOOK FOR:
    =================
    
    register_filesystem():
    - Global file_systems list
    - How filesystem is added
    
    get_fs_type():
    - How mount finds filesystem by name
```

---

## Key Functions to Trace

| Function | File | Purpose |
|----------|------|---------|
| `pci_register_driver()` | drivers/pci/pci-driver.c | Register PCI driver |
| `pci_match_device()` | drivers/pci/pci-driver.c | Match driver to device |
| `register_filesystem()` | fs/filesystems.c | Register filesystem |
| `get_fs_type()` | fs/filesystems.c | Find filesystem by name |

---

## Tracing Exercise

```
    TRACE: Module Load to Device Probe
    ===================================
    
    1. Start at module_init in a simple driver
       (e.g., drivers/net/e1000/e1000_main.c)
    
    2. Trace pci_register_driver()
    
    3. See how driver is added to list
    
    4. Find where probe() is called
    
    5. Understand the complete registration flow
```

---

## Reading Checklist

```
    [ ] Read struct pci_driver definition
    [ ] Read pci_register_driver implementation
    [ ] Read struct file_system_type definition
    [ ] Read register_filesystem implementation
    [ ] Trace a real driver's module_init
```

---

## Version

This reading guide is for **Linux kernel v3.2**.
