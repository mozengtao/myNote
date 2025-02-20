- platform总线是虚拟总线
- platform总线将驱动分为两个部分
	- device
		- platform_device用来描述硬件资源信息
		- ```c
		  struct platform_device {
		    
		  }
		  
		  int platform_device_register(struct platform_device *device);
		  int platform_device_unregister(struct platform_device *device);
		  ```
	- driver
		- platform_driver用来实现操作硬件的方法
		- ```c
		  struct platform_driver {
		    
		  }
		  
		  int platform_driver_register(struct platform_driver *device);
		  int platform_driver_unregister(struct platform_driver *device);
		  
		  struct resource *platform_get_resource(struct platform_device *, unsigned int, unsigned int);
		  ```
	- device和driver依靠platform总线根据name属性进行匹配，匹配成功后驱动代码中的probe函数会被执行，probe函数会拿到device中的硬件资源并进行相应的设备初始化操作
-