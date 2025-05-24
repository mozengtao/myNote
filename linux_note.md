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