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