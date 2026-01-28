# Identification Rules: Registration Pattern

Five concrete rules to identify Registration pattern in Linux kernel source code.

---

## Rule 1: Look for register/unregister Function Pairs

```c
/* Registration functions always come in pairs */
pci_register_driver(&my_driver);
pci_unregister_driver(&my_driver);

usb_register(&my_driver);
usb_deregister(&my_driver);

register_filesystem(&my_fs_type);
unregister_filesystem(&my_fs_type);

/* SIGNAL: register_*/unregister_* or *_register/*_unregister */
```

**中文说明：**

规则1：查找register/unregister函数对——注册函数总是成对出现。

---

## Rule 2: Look for Driver/Module Structures

```c
/* Driver structure with callbacks */
static struct pci_driver my_driver = {
    .name = "my_driver",
    .id_table = my_ids,
    .probe = my_probe,
    .remove = my_remove,
};

static struct file_system_type my_fs = {
    .name = "myfs",
    .mount = my_mount,
    .kill_sb = my_kill_sb,
    .owner = THIS_MODULE,
};

/* SIGNAL: static struct with .name and callbacks */
```

---

## Rule 3: Look for ID Tables

```c
/* Device ID matching tables */
static const struct pci_device_id my_ids[] = {
    { PCI_DEVICE(0x8086, 0x1234) },
    { PCI_DEVICE(0x8086, 0x5678) },
    { 0, }  /* Terminator */
};
MODULE_DEVICE_TABLE(pci, my_ids);

static const struct usb_device_id my_usb_ids[] = {
    { USB_DEVICE(0x1234, 0x5678) },
    { }
};

/* SIGNAL: xxx_device_id arrays with MODULE_DEVICE_TABLE */
```

**中文说明：**

规则3：查找ID表——用于设备匹配的数组，以空元素结尾，通常有MODULE_DEVICE_TABLE宏。

---

## Rule 4: Look for module_init/module_exit

```c
/* Module initialization */
static int __init my_init(void)
{
    return pci_register_driver(&my_driver);
}

static void __exit my_exit(void)
{
    pci_unregister_driver(&my_driver);
}

module_init(my_init);
module_exit(my_exit);

/* Or shorthand: */
module_pci_driver(my_driver);
module_usb_driver(my_driver);

/* SIGNAL: module_init registers, module_exit unregisters */
```

---

## Rule 5: Look for probe/remove Callbacks

```c
/* Device lifecycle callbacks */
static int my_probe(struct pci_dev *pdev,
                    const struct pci_device_id *id)
{
    /* Device found - initialize it */
    return 0;
}

static void my_remove(struct pci_dev *pdev)
{
    /* Device removed - cleanup */
}

/* SIGNAL: probe() called on device match, remove() on unbind */
```

**中文说明：**

规则5：查找probe/remove回调——probe在设备匹配时调用，remove在解绑时调用。

---

## Summary Checklist

```
+=============================================================================+
|                    REGISTRATION IDENTIFICATION CHECKLIST                     |
+=============================================================================+

    [ ] 1. REGISTER/UNREGISTER PAIRS
        pci_register_driver / pci_unregister_driver
        register_filesystem / unregister_filesystem
    
    [ ] 2. DRIVER STRUCTURE
        static struct xxx_driver with callbacks
    
    [ ] 3. ID TABLES
        xxx_device_id arrays with MODULE_DEVICE_TABLE
    
    [ ] 4. MODULE_INIT/EXIT
        Register in init, unregister in exit
    
    [ ] 5. PROBE/REMOVE CALLBACKS
        Lifecycle hooks for device binding

    SCORING:
    3+ indicators = Registration pattern
```

---

## Red Flags: NOT Registration

```
    THESE ARE NOT REGISTRATION:
    ===========================

    1. Simple list addition
       list_add() without subsystem callbacks
    
    2. Direct function calls
       No module_init structure
    
    3. No matching table
       Just a struct without device matching
```

---

## Version

Based on **Linux kernel v3.2**.
