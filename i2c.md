- i2c接口电路
	- ![I2C bus](https://www.abelectronics.co.uk/docs/kb/gettingstarted/i2c/1-i2cbus.svg)
- i2c地址
	- 7-bit 地址
		- ![I2C data packet](https://www.abelectronics.co.uk/docs/kb/gettingstarted/i2c/1-i2c-packet.svg)
	- 10-bit 地址
		- ![10 bit I2C address](https://www.abelectronics.co.uk/docs/kb/gettingstarted/i2c/1-i2c-10-bit-address.svg)
	- RW bit
		- 读
			- 1
		- 写
			- 0
- 时钟速率
	- Standard Mode
	  background-color:: yellow
		- 100 Kbit/s
		  background-color:: yellow
	- Full Speed
		- 400 Kbit/s
	- Fast Mode
		- 1 Mbit/s
	- High Speed
		- 3.2 Mbit/s
- Clock Stretching
	- 如果从设备在发送响应之前需要更多的时间来执行其他的操作，它可以将 SCL 线拉低，强制 master 等待直到从设备 ready，这个动作被称为 clock stretching。
- i2c-tools
	- i2cdetect
		- 检测 i2c 芯片
	- i2cdump
		- 检查 i2c 寄存器
	- i2cget
		- 从 i2c/SMBus 芯片寄存器读值
	- i2cset
		- 设置 i2c 寄存器
	- i2ctransfer
		- 以1次传输发送用户自定义的 I2c 消息
- 参考文档
	- [i2cdump](https://helpmanual.io/man8/i2cdump/)
	- [i2c deep dive](https://wiki.stmicroelectronics.cn/stm32mpu/index.php?title=I2C_overview)
	- [i2c dev interface](https://www.kernel.org/doc/Documentation/i2c/dev-interface)
	- [Implementing I2C device drivers in userspace](https://docs.kernel.org/i2c/dev-interface.html)
	- [I2C Communication from Linux Userspace](https://www.ctrlinux.com/blog/?p=38)
	- [logic analyzer](https://www.saleae.com/downloads/)
	- [I2C and SMBus Subsystem](https://www.infradead.org/~mchehab/kernel_docs/driver-api/i2c.html)
	- [Interfacing with I2C Devices](https://elinux.org/Interfacing_with_I2C_Devices)
	- [i2c dev interface](https://www.kernel.org/doc/Documentation/i2c/dev-interface)
	- [How to instantiate I2C devices](https://docs.kernel.org/i2c/instantiating-devices.html)
	- [I2C Guide](https://opencoursehub.cs.sfu.ca/bfraser/grav-cms/cmpt433/guides/files/I2CGuide.pdf)
	- ![AT24C256C-Datasheet.pdf](./assets/AT24C256C-Datasheet_1678341329919_0.pdf)
	- [linux 串行通信接口驱动框架](https://www.cnblogs.com/downey-blog/p/10491953.html)
	- [Linux-kernel中的i2c-dev驱动](https://mshrimp.github.io/2020/04/12/Linux-kernel%E4%B8%AD%E7%9A%84i2c-dev%E9%A9%B1%E5%8A%A8/)
	- [Introduction to I2C and SMBus](https://docs.kernel.org/i2c/summary.html#introduction-to-i2c-and-smbus)