## I/O设备
> I/O设备 = 一个能与CPU交换数据的接口/控制器
> 就是 "几组约定好功能的线"（寄存器），通过握手信号从线上读/写数据
> 给寄存器"赋予"一个内存地址(Address Decoder)，CPU可以直接使用指令(in/out / MMIO)和设备交换数据
+---------------------------------------------------------------------+                                    
|                +-----------+    +---------------+     +-----------+ |                                    
|   Registers    |  Status   |    |    Command    |     |  Data     | |   Interface                        
|                +-----------+    +---------------+     +-----------+ |                                    
|                                                                     |                                    
| ------------------------------------------------------------------- |                                    
|                                                                     |                                    
|   Micro-controller(CPU)                                             |   Internals                        
|   Memory (DRAM or SRAM or Both)                                     |                                    
|   Other Hardware-specific Chips                                     |                                    
+---------------------------------------------------------------------+        

## Page Cache
> Page Cache（页面缓存）​​ 是Linux内核用于缓存磁盘文件数据的内存区域，旨在通过减少磁盘I/O次数来提升系统性能。当应用程序访问文件时，数据首先被加载到Page Cache中，后续的读写操作可直接在内存中完成，从而避免频繁访问低速磁盘
[The Page Cache and Page Writeback](https://github.com/firmianay/Life-long-Learner/blob/master/linux-kernel-development/chapter-16.md)  
[Essential Page Cache theory](https://biriukov.dev/docs/page-cache/2-essential-page-cache-theory/)  
[]()  
[]()  
[]()  
[]()  

## Udev
[Linux设备模型(3)_Uevent](http://www.wowotech.net/device_model/uevent.html/)  
[udev(7)](https://www.mankier.com/7/udev)  
[Udev: Introduction to Device Management In Modern Linux System](https://www.linux.com/news/udev-introduction-device-management-modern-linux-system/)  
[Writing udev rules](https://www.reactivated.net/writing_udev_rules.html)  
[Linux udev rules](https://www.downtowndougbrown.com/2014/03/linux-udev-rules/)  
[udev: Device Manager for the Linux Kernel in Userspace](https://insujang.github.io/2018-11-27/udev-device-manager-for-the-linux-kernel-in-userspace/)  
[What are Udev, HAL, Dbus and Netlink ?](https://vlinux-freak.blogspot.com/2019/02/what-are-udev-hal-dbus-and-netlink.html)  
[Getting started with Udev Rules](https://medium.com/@somesh557/getting-started-with-udev-rules-part-a-54a450eedefc)  
```
Hot-plugging
  Hot-plugging (which is the word used to describe the process of inserting devices into a running system) is achieved in a Linux distribution by a combination of three components: Udev, HAL, and Dbus.

Udev
  Udev is a userspace daemon, that supplies a dynamic device directory containing only the nodes for devices which are connected to the system. It creates or removes the device node files in the /dev directory as they are plugged in or taken out. Dbus is like a system bus which is used for inter-process communication. The HAL gets information from the Udev service, when a device is attached to the system and it creates an XML representation of that device. It then notifies the corresponding desktop application like Nautilus through the Dbus and Nautilus will open the mounted device files.

Dbus
  Dbus is an IPC mechanism, which allows applications to register for system device events.

how Udev mechanism works
  Udev depends on the sysfs file system which was introduced in the 2.5 kernel. It is sysfs which makes devices visible in user space. When a device is added or removed, kernel events are produced which will notify Udev in userspace. Udev directly listens to Netlink socket to know about device state change events (kernel uevents).

  +------------+                        +--------------+                        
  |            |   Kernel netlink msg   |              |                        
  |            |   indicates device has |              |                        
  |            |   been plugged or ...  |              |                        
  |            |<-----------------------|              |                        
  |            |          1             |              |                        
  |            |                        |              |                        
  |   Udev     |                        |    Kernel    |                        
  |            |                        |              |                        
  |            |                        |              |                        
  |            |  2   +----------+  3   |              |                        
  |            |----->| modprobe |----->|              |                        
  |            |      +----------+      |              |                        
  |            |                        |              |                        
  +------------+                        +--------------+                        
 Udev invokes modprobe                  Modprobe loads module                   
 with the module alias                  with alias mapping from                 
 containing vendor ID                   modules alias.                          
 and device ID.                                                                 
 
```
## `/dev/loop-control`
[Linux loop devices](https://blog.devops.dev/linux-loop-devices-451002bf69d9)  
[loop(4)](https://www.mankier.com/4/loop)  
[losetup(8)](https://www.mankier.com/8/losetup)  
[Access Control Lists](https://wiki.archlinux.org/title/Access_Control_Lists)  
[]()  
> ​Loop Device​​ 是一种虚拟块设备，允许将普通文件（如 ISO 镜像、磁盘镜像）作为块设备挂载使用, /dev/loop-control 是 Linux 内核提供的​​动态管理 Loop Device 的字符设备接口​​，主要用于按需分配和释放 Loop 设备号, 用户程序通过 ioctl 系统调用与其交互
```
```

## setfacl
[Mastering Linux: ‘setfacl’ Command Installation Methods](https://ioflood.com/blog/install-setfacl-command-linux/)  
[Using ‘setfacl’ | A Linux Command for File Access Control](https://ioflood.com/blog/setfacl-linux-command/)  
[setfacl(1)](https://www.mankier.com/1/setfacl)  
[getfacl(1)](https://www.mankier.com/1/getfacl)  
[]()  
> The setfacl command in Linux is used to set file access control lists, allowing you to manage permissions for different users and groups. A basic syntax template of the setfacl command might look like this: setfacl [arguments] [user_or_group_permissions] filename

## sysfs
> The sysfs filesystem is a pseudo-filesystem which provides an interface to kernel data structures. (More precisely, the files and directories in sysfs provide a view of the kobject structures defined internally within the kernel.) The files under sysfs provide information about devices, kernel modules, filesystems, and other kernel components.
[sysfs(5)](https://www.mankier.com/5/sysfs)  
[sysfs](https://www.kernel.org/doc/Documentation/filesystems/sysfs.txt)  
[sysfs - _The_ filesystem for exporting kernel objects](https://docs.kernel.org/filesystems/sysfs.html)  
[A complete guide to sysfs — Part 1: introduction to kobject](https://medium.com/@emanuele.santini.88/sysfs-in-linux-kernel-a-complete-guide-part-1-c3629470fc84)  
[A complete guide to sysfs — Part 2: improving the attributes](https://medium.com/@emanuele.santini.88/a-complete-guide-to-sysfs-part-2-improving-the-attributes-1dbc1fca9b75)  
[Rules on how to access information in sysfs](https://docs.kernel.org/admin-guide/sysfs-rules.html)  
[The sysfs Filesystem](https://www.kernel.org/doc/ols/2005/ols2005v1-pages-321-334.pdf)  
[How sysfs helps in communicating with userspace and kernel space ??](https://medium.com/@vikasv210/how-sysfs-helps-in-communicating-with-userspace-and-kernel-space-8329fab7c9ea)  
[]()  

## procfs
> The directory /proc contains (among other things) one subdirectory for each process running on the system, which is named after the process ID (PID).
[The /proc Filesystem](https://docs.kernel.org/filesystems/proc.html)  
[proc(5)](https://www.mankier.com/5/proc)  
[]()  
[]()  

## debugfs
[How To Use debugfs](https://linuxlink.timesys.com/docs/wiki/engineering/HOWTO_Use_debugfs)  
[DebugFS](https://docs.kernel.org/filesystems/debugfs.html)  
[Debugfs kernel debugging](https://developer.ridgerun.com/wiki/index.php/Debugfs_kernel_debugging)  
[]()  


## linux设备驱动模型
```c
// 核心概念
kobject
    kobject 是 Linux 内核用于管理和表示对象的通用机制，具有以下特征
        统一的生命周期管理（引用计数
        层级结构（可以有父子
        支持事件通知（用于 udev
        可自动映射到 /sys 文件系统
kset
    一组 kobject 的集合，表示子系统或设备类
    /**
    * struct kset - a set of kobjects of a specific type, belonging to a specific subsystem.
    *
    * A kset defines a group of kobjects.  They can be individually
    * different "types" but overall these kobjects all want to be grouped
    * together and operated on in the same manner.  ksets are used to
    * define the attribute callbacks and other common events that happen to
    * a kobject.
    */
device
    表示系统中的一个物理或虚拟设备，内部嵌套了 kobject
    /**
    * At the lowest level, every device in a Linux system is represented by an
    * instance of struct device. The device structure contains the information
    * that the device model core needs to model the system. Most subsystems,
    * however, track additional information about the devices they host. As a
    * result, it is rare for devices to be represented by bare device structures;
    * instead, that structure, like kobject structures, is usually embedded within
    * a higher-level representation of the device.
    */
device_driver
    表示驱动程序，也含有 kobject
    /**
    * The device driver-model tracks all of the drivers known to the system.
    * The main reason for this tracking is to enable the driver core to match
    * up drivers with new devices. Once drivers are known objects within the
    * system, however, a number of other things become possible. Device drivers
    * can export information and configuration variables that are independent
    * of any specific device.
    */
bus_type
    表示一种总线类型（如 PCI、USB）
    /**
    * A bus is a channel between the processor and one or more devices. For the
    * purposes of the device model, all devices are connected via a bus, even if
    * it is an internal, virtual, "platform" bus. Buses can plug into each other.
    * A USB controller is usually a PCI device, for example. The device model
    * represents the actual connections between buses and the devices they control.
    * A bus is represented by the bus_type structure. It contains the name, the
    * default attributes, the bus' methods, PM operations, and the driver core's
    * private data.
    */
class
    逻辑设备类别（如 block、net、input）
    /**
    * A class is a higher-level view of a device that abstracts out low-level
    * implementation details. Drivers may see a SCSI disk or an ATA disk, but,
    * at the class level, they are all simply disks. Classes allow user space
    * to work with devices based on what they do, rather than how they are
    * connected or how they work.
    */
sysfs
    /sys 中呈现设备模型的接口，由 kobject 驱动


struct kobject {
    const char      *name;
    struct list_head entry;
    struct kobject  *parent;
    struct kset     *kset;
    struct kobj_type *ktype;
    struct sysfs_dirent *sd;
    struct kref     kref;     // 引用计数
};

struct device {
    struct kobject kobj;
    struct device  *parent;
    struct device_driver *driver;
    struct bus_type *bus;
    void *platform_data;
    ...
};

struct device_driver {
    struct kobject kobj;
    struct bus_type *bus;
    const char *name;
    ...
};

struct bus_type {
    struct kobject kobj;
    struct kset subsys;
    struct kset drivers;
    struct kset devices;
    ...
};

struct kset {
	struct list_head list;
	spinlock_t list_lock;
	struct kobject kobj;
	const struct kset_uevent_ops *uevent_ops;
} __randomize_layout;

struct class {
	const char		*name;
	struct module		*owner;

	const struct attribute_group	**class_groups;
	const struct attribute_group	**dev_groups;
	struct kobject			*dev_kobj;

	int (*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);
	char *(*devnode)(struct device *dev, umode_t *mode);

	void (*class_release)(struct class *class);
	void (*dev_release)(struct device *dev);

	int (*shutdown_pre)(struct device *dev);

	const struct kobj_ns_type_operations *ns_type;
	const void *(*namespace)(struct device *dev);

	void (*get_ownership)(struct device *dev, kuid_t *uid, kgid_t *gid);

	const struct dev_pm_ops *pm;

	struct subsys_private *p;
};

// example 1
// my_kobject.c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Example: kobject and sysfs");
MODULE_VERSION("0.1");

static struct kobject *example_kobj;
static int myval = 0;

/* 显示属性的回调函数 */
static ssize_t myval_show(struct kobject *kobj,
                          struct kobj_attribute *attr,
                          char *buf)
{
    return sprintf(buf, "%d\n", myval);
}

/* 设置属性的回调函数 */
static ssize_t myval_store(struct kobject *kobj,
                           struct kobj_attribute *attr,
                           const char *buf,
                           size_t count)
{
    sscanf(buf, "%d", &myval);
    return count;
}

/* 创建属性结构体 */
static struct kobj_attribute myval_attribute =
    __ATTR(myval, 0664, myval_show, myval_store);

/* 模块加载时调用 */
static int __init mykobj_init(void)
{
    int retval;

    // 在 /sys/kernel/ 下创建 mykobj 目录
    example_kobj = kobject_create_and_add("mykobj", kernel_kobj);
    if (!example_kobj)
        return -ENOMEM;

    // 创建属性文件
    retval = sysfs_create_file(example_kobj, &myval_attribute.attr);
    if (retval)
        kobject_put(example_kobj);  // 失败则释放

    pr_info("my_kobject module loaded\n");
    return retval;
}

/* 模块卸载时调用 */
static void __exit mykobj_exit(void)
{
    sysfs_remove_file(example_kobj, &myval_attribute.attr);
    kobject_put(example_kobj);
    pr_info("my_kobject module unloaded\n");
}

module_init(mykobj_init);
module_exit(mykobj_exit);

// Makefile
obj-m += my_kobject.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

// test
make

sudo insmod my_kobject.ko

cat /sys/kernel/mykobj/myval                  // 0
echo 42 | sudo tee /sys/kernel/mykobj/myval
cat /sys/kernel/mykobj/myval                  // 42

sudo rmmod my_kobject

// example 2
// example_dev.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

// Module metadata
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grok");
MODULE_DESCRIPTION("Example character device driver with custom bus and kobject");
MODULE_VERSION("1.0");

// Device data structure
struct example_device {
    struct device dev;          // Device structure with embedded kobject
    struct cdev cdev;           // Character device structure
    struct class *class;        // Device class for /sys/class
    int value;                  // Stored integer value
    bool active;                // Device status (active/inactive)
    dev_t devno;                // Device number (major/minor)
};

// Bus data structure
static struct bus_type example_bus_type = {
    .name = "example_bus",
    .match = NULL,              // Match function (to be defined)
    .probe = NULL,              // Probe function (to be defined)
};

// Driver data structure
struct example_driver {
    struct device_driver driver; // Driver structure with embedded kobject
};

// Global device instance
static struct example_device *ex_dev;
static struct example_driver *ex_driver;

// Sysfs attributes
static ssize_t value_show(struct device *dev, struct device_attribute *attr, char *buf) {
    struct example_device *ex_dev = container_of(dev, struct example_device, dev);
    return sprintf(buf, "%d\n", ex_dev->value);
}

static ssize_t value_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count) {
    struct example_device *ex_dev = container_of(dev, struct example_device, dev);
    if (kstrtoint(buf, 10, &ex_dev->value))
        return -EINVAL;
    return count;
}
static DEVICE_ATTR(value, 0644, value_show, value_store);

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf) {
    struct example_device *ex_dev = container_of(dev, struct example_device, dev);
    return sprintf(buf, "%s\n", ex_dev->active ? "active" : "inactive");
}
static DEVICE_ATTR_RO(status);

static ssize_t version_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%s\n", MODULE_VERSION);
}
static DEVICE_ATTR_RO(version);

// Attribute group for organized sysfs entries
static struct attribute *example_attrs[] = {
    &dev_attr_value.attr,
    &dev_attr_status.attr,
    &dev_attr_version.attr,
    NULL,
};

static struct attribute_group example_attr_group = {
    .attrs = example_attrs,
};

// Bus match function
static int example_bus_match(struct device *dev, struct device_driver *drv) {
    // Simple match: check if device and driver belong to example_bus
    return dev->bus == &example_bus_type && drv == &ex_driver->driver;
}

// Bus probe function
static int example_bus_probe(struct device *dev) {
    pr_info("example_dev: Probing device %s\n", dev_name(dev));
    return 0;
}

// File operations for /dev/example_dev
static int example_dev_open(struct inode *inode, struct file *file) {
    struct example_device *ex_dev = container_of(inode->i_cdev, struct example_device, cdev);
    ex_dev->active = true;
    file->private_data = ex_dev;
    return 0;
}

static int example_dev_release(struct inode *inode, struct file *file) {
    struct example_device *ex_dev = file->private_data;
    ex_dev->active = false;
    return 0;
}

static ssize_t example_dev_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
    struct example_device *ex_dev = file->private_data;
    char tmp[32];
    int len = sprintf(tmp, "%d\n", ex_dev->value);
    if (*offset >= len)
        return 0;
    if (count > len - *offset)
        count = len - *offset;
    if (copy_to_user(buf, tmp + *offset, count))
        return -EFAULT;
    *offset += count;
    return count;
}

static ssize_t example_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset) {
    struct example_device *ex_dev = file->private_data;
    char tmp[32];
    if (count >= sizeof(tmp))
        return -EINVAL;
    if (copy_from_user(tmp, buf, count))
        return -EFAULT;
    tmp[count] = '\0';
    if (kstrtoint(tmp, 10, &ex_dev->value))
        return -EINVAL;
    return count;
}

static const struct file_operations example_dev_fops = {
    .owner = THIS_MODULE,
    .open = example_dev_open,
    .release = example_dev_release,
    .read = example_dev_read,
    .write = example_dev_write,
};

// Driver structure
static struct example_driver ex_driver_instance = {
    .driver = {
        .name = "example_driver",
        .bus = &example_bus_type,
    },
};

// Module initialization
static int __init example_dev_init(void) {
    int ret;

    // Register the bus
    ret = bus_register(&example_bus_type);
    if (ret) {
        pr_err("example_dev: Failed to register bus\n");
        return ret;
    }
    example_bus_type.match = example_bus_match;
    example_bus_type.probe = example_bus_probe;

    // Allocate device structure
    ex_dev = kzalloc(sizeof(struct example_device), GFP_KERNEL);
    if (!ex_dev) {
        pr_err("example_dev: Failed to allocate device structure\n");
        ret = -ENOMEM;
        goto unregister_bus;
    }

    // Initialize device data
    ex_dev->value = 0;
    ex_dev->active = false;

    // Allocate device number
    ret = alloc_chrdev_region(&ex_dev->devno, 0, 1, "example_dev");
    if (ret) {
        pr_err("example_dev: Failed to allocate device number\n");
        goto free_dev;
    }

    // Initialize and add character device
    cdev_init(&ex_dev->cdev, &example_dev_fops);
    ex_dev->cdev.owner = THIS_MODULE;
    ret = cdev_add(&ex_dev->cdev, ex_dev->devno, 1);
    if (ret) {
        pr_err("example_dev: Failed to add cdev\n");
        goto unregister_chrdev;
    }

    // Create device class
    ex_dev->class = class_create(THIS_MODULE, "example_class");
    if (IS_ERR(ex_dev->class)) {
        pr_err("example_dev: Failed to create class\n");
        ret = PTR_ERR(ex_dev->class);
        goto del_cdev;
    }

    // Initialize and register device
    device_initialize(&ex_dev->dev);
    ex_dev->dev.bus = &example_bus_type;
    ex_dev->dev.parent = NULL; // Virtual device, no parent
    dev_set_name(&ex_dev->dev, "example_dev");
    ret = device_add(&ex_dev->dev);
    if (ret) {
        pr_err("example_dev: Failed to add device\n");
        goto destroy_class;
    }

    // Create device node in /dev
    if (!device_create(ex_dev->class, NULL, ex_dev->devno, NULL, "example_dev")) {
        pr_err("example_dev: Failed to create device node\n");
        ret = -ENOMEM;
        goto del_device;
    }

    // Create sysfs attribute group
    ret = sysfs_create_group(&ex_dev->dev.kobj, &example_attr_group);
    if (ret) {
        pr_err("example_dev: Failed to create sysfs group\n");
        goto destroy_device_node;
    }

    // Register driver
    ex_driver = &ex_driver_instance;
    ret = driver_register(&ex_driver->driver);
    if (ret) {
        pr_err("example_dev: Failed to register driver\n");
        goto remove_sysfs;
    }

    pr_info("example_dev: Driver loaded successfully\n");
    return 0;

remove_sysfs:
    sysfs_remove_group(&ex_dev->dev.kobj, &example_attr_group);
destroy_device_node:
    device_destroy(ex_dev->class, ex_dev->devno);
del_device:
    device_del(&ex_dev->dev);
destroy_class:
    class_destroy(ex_dev->class);
del_cdev:
    cdev_del(&ex_dev->cdev);
unregister_chrdev:
    unregister_chrdev_region(ex_dev->devno, 1);
free_dev:
    kfree(ex_dev);
unregister_bus:
    bus_unregister(&example_bus_type);
    return ret;
}

// Module cleanup
static void __exit example_dev_exit(void) {
    driver_unregister(&ex_driver->driver);
    sysfs_remove_group(&ex_dev->dev.kobj, &example_attr_group);
    device_destroy(ex_dev->class, ex_dev->devno);
    device_del(&ex_dev->dev);
    class_destroy(ex_dev->class);
    cdev_del(&ex_dev->cdev);
    unregister_chrdev_region(ex_dev->devno, 1);
    bus_unregister(&example_bus_type);
    kfree(ex_dev);
    pr_info("example_dev: Driver unloaded\n");
}

module_init(example_dev_init);
module_exit(example_dev_exit);

// Makefile
obj-m += example_dev.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

// test
make 

sudo insmod example_dev.ko

ls /sys/devices/virtual/example_bus/example_dev/              // Outputs: power  status  subsystem  uevent  value  version
cat /sys/devices/virtual/example_bus/example_dev/value        // Outputs: 0
cat /sys/devices/virtual/example_bus/example_dev/status       // Outputs: inactive
cat /sys/devices/virtual/example_bus/example_dev/version      // Outputs: 1.0
ls /sys/class/example_class                                   // Outputs: example_dev
ls /sys/bus/example_bus                                       // Outputs: devices  drivers
ls /sys/bus/example_bus/drivers                               // Outputs: example_driver
ls /dev/example_dev                                           // Outputs: /dev/example_dev

echo 42 | sudo tee /dev/example_dev

cat /dev/example_dev
# Outputs: 42

cat /sys/devices/virtual/example_bus/example_dev/value
# Outputs: 42
cat /sys/devices/virtual/example_bus/example_dev/status
# Outputs: active (if device file is open)

echo example_dev | sudo tee /sys/bus/example_bus/drivers/example_driver/unbind

echo example_dev | sudo tee /sys/bus/example_bus/drivers/example_driver/bind

sudo rmmod example_dev

// sysfs structure
/sys/bus/example_bus:
    devices/example_dev: Link to /sys/devices/virtual/example_bus/example_dev.
    drivers/example_driver: Driver directory with bind, unbind, and device links.
/sys/devices/virtual/example_bus/example_dev:
    value: Read/write integer value.
    status: Read-only device state.
    version: Read-only driver version.
    power, subsystem, uevent: Standard device attributes.
/sys/class/example_class/example_dev: Link to the device.
/dev/example_dev: Character device file.

// user-space test_example_dev.c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main() {
    int fd = open("/dev/example_dev", O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/example_dev");
        return 1;
    }

    // Write value
    char buf[32] = "123";
    if (write(fd, buf, strlen(buf)) < 0) {
        perror("Write failed");
        close(fd);
        return 1;
    }

    // Read value
    char read_buf[32] = {0};
    if (read(fd, read_buf, sizeof(read_buf) - 1) < 0) {
        perror("Read failed");
        close(fd);
        return 1;
    }
    printf("Read value: %s\n", read_buf);

    close(fd);
    return 0;
}

// test
gcc -o test_example_dev test_example_dev.c
sudo ./test_example_dev
# Outputs: Read value: 123
```

## miscs
```bash
  morrism@localhost ~ $ ls /dev/input/
  by-id  by-path  event0  event1  event2  event3  event4  event5  event6  js0  mice  mouse0  mouse1  mouse2  mouse3
  
  morrism@localhost ~ $ cat /proc/bus/input/devices
  I: Bus=0019 Vendor=0000 Product=0001 Version=0000
  N: Name="Power Button"
  P: Phys=LNXPWRBN/button/input0
  S: Sysfs=/devices/LNXSYSTM:00/LNXPWRBN:00/input/input0
  U: Uniq=
  H: Handlers=kbd event0
  B: PROP=0
  B: EV=3
  B: KEY=10000000000000 0
  ......
  
  
  struct input_value {
      __u16 type;		// type of value (EV_KEY, EV_ABS, etc)
      __u16 code;		// the value code
      __s32 value;	// the value
  };
```

[聊聊 Linux IO](https://www.0xffffff.org/2017/05/01/41-linux-io/)  
[Input Documentation](https://www.kernel.org/doc/html/latest/input/index.html)  
[Input Subsystem](https://www.kernel.org/doc/html/latest/driver-api/input.html)  
[Creating an input device driver](https://docs.kernel.org/input/input-programming.html)  
[Input Drivers](http://embeddedlinux.org.cn/essentiallinuxdevicedrivers/final/ch07.html)  
[Linux Kernel Development Second Edition](https://litux.nl/mirror/kerneldevelopment/0672327201/toc.html)  