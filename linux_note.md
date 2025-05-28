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