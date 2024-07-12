- 驱动
	- 字面意思
		- 驱使运动
	- 物理上的驱动
		- 用脚踢足球，脚驱动足球
	- 硬件中的驱动
		- 电源适配器通过电压转换给设备供电，因此电源适配器称为设备的电源驱动
	- linux驱动
		- 广义驱动
			- 一段代码操作了硬件去“动”(工作)，即驱动程序提供了操作设备的逻辑方法，这段代码就称为硬件的驱动程序
		- 狭义驱动
			- 操作系统中用来操控硬件的逻辑方法部分代码
	- 参考文档
		- [从零学Linux驱动](https://api.eestar.com/article/share/2021/08/d66f320210811213750879.pdf)
-
- 设备
	- 字符设备
		- 字节设备，软件按照字节流对设备进行访问
			- lcd
			- uart
	- 块设备
		- 块设备是相对于字符设备而言，软件以"块"(多个字节构成的一个单位)为单位对设备进行操作，块的大小由具体的硬件设备决定
			- nand flash
			- 硬盘
			- sd卡
	- 网络设备
		- 专门为网卡设计的驱动模型
- 驱动注册过程
	- linux内核的总线上有2条链表
		- driver链表
			- 新driver注册时，向driver链表添加元素
			- 例如`amba_driver_register(struct amba_driver *drv)`
		- device链表
			- 新device注册时，向device链表添加元素
			- 例如`amba_device_register(struct amba_device *dev, struct resource *parent)`
	- 总线在linux内核里是一个虚拟的概念，用来管理设备和驱动
		- 当有新的driver注册到该总线时，会触发该总线的match操作，即遍历该总线下对应的device链表找到与该注册driver匹配的设备，如果匹配则driver与device匹配成功，即给device安装好了driver，之后可以通过该driver操作该设备
		- 当有新的device注册到该总线时，会触发会触发该总线的match操作，即遍历该总线下对应的driver链表找到与该注册device匹配的driver，如果匹配则driver与device匹配成功，即给device安装好了driver，之后可以通过该driver操作该设备
	- 注意
		- match是总线的行为
		- probe是match成功后具体执行的操作，由总线间接调用，当probe函数执行时候总线会传递device相关的硬件资源信息
	- 驱动代码中的device从哪里来
		- `drivers/of/platform.c`
			- ```c
			  of_platform_default_populate_init
			  	of_platform_default_populate
			      	of_platform_populate
			          	 of_platform_bus_create
			               	of_amba_device_create
			                  	amba_device_add
			  ```
- [[module_init]]
- 参考文档
	- [Linux设备驱动开发详解](https://github.com/kevinwangkk/LDD4.0_note/blob/master/Linux%E8%AE%BE%E5%A4%87%E9%A9%B1%E5%8A%A8%E5%BC%80%E5%8F%91%E8%AF%A6%E8%A7%A3%EF%BC%9A%E5%9F%BA%E4%BA%8E%E6%9C%80%E6%96%B0%E7%9A%84Linux4.0%E5%86%85%E6%A0%B8.pdf) #pdf
	- [LinuxDrivers](https://sysplay.github.io/books/LinuxDrivers/book/index.html) #driver
	- [Linux Device Drivers Series](https://www.opensourceforu.com/tag/linux-device-drivers-series/page/2/) #driver
	- [Driver Model](https://www.kernel.org/doc/html/latest/driver-api/driver-model/)
	- [LinuxDrivers](https://sysplay.github.io/books/LinuxDrivers/book/index.html) #driver
	- [Linux Device Driver Tutorials](https://embetronicx.com/linux-device-driver-tutorials/) #driver
	- [DriverPractice](https://github.com/starnight/DriverPractice) #driver #github
	- [lcd驱动应该怎么写](http://www.wujique.com/2021/05/16/lcd%e9%a9%b1%e5%8a%a8%e5%ba%94%e8%af%a5%e6%80%8e%e4%b9%88%e5%86%99%ef%bc%9f/)
	- [linux驱动子系统](https://www.cnblogs.com/lizhuming/tag/%2Flabel%2Flinux/)
	- [驱动程序分层分离概念](http://wiki.100ask.org/images/c/c1/%E9%B1%BC%E6%A0%91%E7%AC%94%E8%AE%B0%E4%B9%8B%E7%AC%AC14%E8%AF%BE%E9%A9%B1%E5%8A%A8%E7%A8%8B%E5%BA%8F%E5%88%86%E5%B1%82%E5%88%86%E7%A6%BB%E6%A6%82%E5%BF%B5.pdf)
	- [Linux-IIC驱动详解](https://cloud.tencent.com/developer/article/1015834?areaSource=104001.229&traceId=x6TfYAuCHtxo8-owGalg1)
	- [Linux-Nor Flash驱动详解](https://cloud.tencent.com/developer/article/1012379?areaSource=104001.239&traceId=x6TfYAuCHtxo8-owGalg1)
	- [Linux-块设备驱动(详解)](https://cloud.tencent.com/developer/article/1012375?areaSource=104001.240&traceId=x6TfYAuCHtxo8-owGalg1)
	- [Linux-块设备驱动之框架详细分析(详解)](https://cloud.tencent.com/developer/article/1012369?areaSource=104001.242&traceId=x6TfYAuCHtxo8-owGalg1)
	- [Linux-Nand Flash驱动(分析MTD层并制作NAND驱动)](https://cloud.tencent.com/developer/article/1012366?areaSource=104001.243&traceId=x6TfYAuCHtxo8-owGalg1)
	- [linux-LCD层次分析(详解)](https://cloud.tencent.com/developer/article/1012349?areaSource=104001.248&traceId=x6TfYAuCHtxo8-owGalg1)
	- [linux-platform机制实现驱动层分离(详解)](https://cloud.tencent.com/developer/article/1012345?areaSource=104001.251&traceId=x6TfYAuCHtxo8-owGalg1)
	- [Linux-LCD驱动(详解)](https://cloud.tencent.com/developer/article/1012342?areaSource=104001.252&traceId=x6TfYAuCHtxo8-owGalg1)
	- [Llinux-触摸屏驱动(详解)](https://cloud.tencent.com/developer/article/1012333?areaSource=104001.254&traceId=x6TfYAuCHtxo8-owGalg1)
	- [制作第一个驱动程序](https://cloud.tencent.com/developer/article/1012235?areaSource=104001.275&traceId=x6TfYAuCHtxo8-owGalg1)
	- [Linux设备驱动的软件架构思想](http://www.mrchen.love/Article/ID/28)
	- [Linux设备驱动中的软件架构思想](https://www.cnblogs.com/wanjianjun777/p/10951912.html)

	[Linux Device Drivers data structures](https://www.linuxtv.org/downloads/v4l-dvb-internals/device-drivers/index.html)
	[bootlin: Basics of I2C on linux](https://bootlin.com/pub/conferences/2022/elce/ceresoli-basics-of-i2c-on-linux/ceresoli-basics-of-i2c-on-linux.pdf)
	[Buildroot examples](https://github.com/gvvsnrnaveen/buildroot/tree/main)
	[Driver implementer’s API guide](https://www.kernel.org/doc/html/latest/driver-api/)
	[I2C Device Interface](https://www.infradead.org/~mchehab/rst_conversion/i2c/dev-interface.html)
	[Implementing I2C device drivers](https://docs.kernel.org/i2c/writing-clients.html)
	[I2C Bus Driver Dummy Linux Device Driver](https://embetronicx.com/tutorials/linux/device-drivers/i2c-bus-driver-dummy-linux-device-driver-using-raspberry-pi/)
	[Linux Kernel Programming by Examples](http://marcocorvi.altervista.org/games/lkpe/)
	[I2C overview](https://wiki.st.com/stm32mpu/wiki/I2C_overview)
	[Linux内核: I2C子系统分析](https://www.cnblogs.com/schips/p/linux-subsystem-i2c-0-about.html)