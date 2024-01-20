- peripheral component interconnect express 是一个用来连接高速器件的接口标准。
- PCIe的基本概念
  collapsed:: true
	- ```
	  PCIe（Peripheral Component Interconnect Express）
	  Slot（插槽）：指的是PCIe主板上用于安装PCIe扩展卡的物理插槽。每个插槽都提供了电源和数据连接接口，
	  允许将PCIe设备插入到计算机系统中并与之通信
	  
	  Lane（通道）：是PCIe中数据传输的基本单位。一个PCIe通道由一对差分信号线组成，分别用于发送和接收
	  数据。每个通道可以独立地进行数据传输，实现高速且双向的通信。通常，一个PCIe插槽会提供多个通道，其
	  数量根据PCIe版本和插槽规格的不同而有所变化
	  
	  Link（链路）：是由一个或多个lanes组成的物理连接，用于在PCIe设备之间进行数据传输。链路由一个发送
	  方和一个接收方组成，它们通过共享时钟和控制信号来进行数据传输。链路的带宽取决于所使用的通道数目，每
	  个通道传输速率为2.5 Gbps（PCIe 1.0/1.1）或每个通道传输速率为8.0 Gbps（PCIe 3.0）或更高
	  ```
	- PCIe slots
		- ![Pasted image 20221028091505.png](./assets/Pasted_image_20221028091505_1679559896747_0.png){:height 126, :width 776}
		- x之后的数字代表PCIe slot有多少lane可以用来通信，它决定了数据的传输速率。
			- PCIex1 有一个lane，在一个时钟周期只能传输一个比特的数据
			- PCIex2 有两个lane，在一个时钟周期只能传输两个比特的数据
	- PCIe Links and Lanes
		- ![Pasted image 20221028091448.png](./assets/Pasted_image_20221028091448_1679560012633_0.png)
	- Lane
		- ![Image 2.png](./assets/Image_2_1679560079893_0.png)
	- Link
		- ![Image 3.png](./assets/Image_3_1679560124488_0.png)
- 在软件层面，PCI和PCIe是兼容的
- 传统的内容访问方式（RAM-like）
  collapsed:: true
	- `int a = 1`
	- ![image.png](./assets/image_1691138055505_0.png)
		- CPU在地址总线上发出访问地址，访问地址先到达内存控制器
		- 内存控制器判断地址属于哪个器件地址范围，例如：如果地址属于RAM的地址，则内存控制器发出片选信号选中RAM器件（同时只能有1个器件被选中）
		- 操作被选中的设备
		- ![image.png](./assets/image_1691138474455_0.png)
- PCIe设备有自己独立的地址空间，该空间不同于CPU发出的地址空间，因此访问PCIe设备需要对地址进行空间转换
- 类比如下
	- CPU要访问I2C设备，必须先通过CPU地址空间的地址访问I2C控制器的寄存器，I2C设备控制器会根据寄存器来操作I2C设备（如发出I2C Slave地址，发送读写命令等操作）
	- 在CPU访问eMMC设备时，CPU通过总线接口（如SD或SPI）与eMMC控制器建立连接。CPU通过向eMMC控制器发送命令来控制eMMC设备的工作，如读取或写入数据、擦除数据、查询设备状态等。eMMC控制器负责将这些命令转化为与eMMC设备的通信，执行相应的操作，并将结果返回给CPU
- Linux PCI drivers
	- [Linux PCI drivers](https://bootlin.com/doc/legacy/pci-drivers/pci-drivers.pdf)
- 参考文档
	- [How PCI express devices talk](http://xillybus.com/tutorials/pci-express-tlp-pcie-primer-tutorial-guide-1)
	- [PCIE扫盲博文](https://codeantenna.com/a/J3LTEaynzW)
	- [PCI Express Technology](https://www.mindshare.com/files/ebooks/PCI%20Express%20Technology%203.0.pdf)
	- [PCI Express体系结构导读](https://github.com/vvvlan/misc/raw/master/PCI%2BExpress%E4%BD%93%E7%B3%BB%E7%BB%93%E6%9E%84%E5%AF%BC%E8%AF%BB.pdf) #pdf
	- [PCIe基础知识](https://xilinx.eetrend.com/content/2019/100046071.html)
	- [PCI Express SYSTEM ARCHITECTURE](https://www.mindshare.com/files/ebooks/pci%20express%20system%20architecture.pdf)