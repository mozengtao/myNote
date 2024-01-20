- [[uboot完全分析与移植]]
- [[uboot 配置及编译过程]]
- uboot命令
	- `/etc/fw_env.config`
		- `fw_env.config`是 uboot-envtools 所用到的配置文件，即执行命令`fw_(printenv/saveenv)`时候会读取该文件的内容，并根据其中的mtd设备信息进行uboot env的操作
		- ```bash
		  # Configuration file for fw_(printenv/saveenv) utility
		  
		  root@slot-120:~ [P2]# cat /etc/fw_env.config
		  # Configuration file for fw_(printenv/saveenv) utility.
		  # Up to two entries are valid, in this case the redundant
		  # environment sector is assumed present.
		  # Notice, that the "Number of sectors" is ignored on NOR and SPI-dataflash.
		  # Futhermore, if the Flash sector size is ommitted, this value is assumed to
		  # be the same as the Environment size, which is valid for NOR and SPI-dataflash
		  
		  # NOR example
		  # MTD device name       Device offset   Env. size       Flash sector size       Number of sectors
		  /dev/mtd12              0x0000          0x2000
		  /dev/mtd15              0x0000          0x2000
		  
		  # MTD SPI-dataflash example
		  # MTD device name       Device offset   Env. size       Flash sector size       Number of sectors
		  #/dev/mtd5              0x4200          0x4200
		  #/dev/mtd6              0x4200          0x4200
		  
		  # NAND example
		  #/dev/mtd0              0x4000          0x4000          0x20000                 2
		  
		  morrism@localhost ~/repos/fsl-u-boot (develop) $ ls tools/env/
		  aes.c  crc32.c  ctype.c  env_attr.c  env_flags.c  fw_env.c  fw_env.config  fw_env.h  fw_env_main.c  linux_string.c  Makefile  README
		  morrism@localhost ~/repos/fsl-u-boot (develop) $ realpath tools/env/
		  /home/morrism/repos/fsl-u-boot/tools/env
		  ```
		- ```bash
		  未注释 '/dev/mtd15              0x0000          0x2000' 前
		  root@slot-120:~ [P2]# strace -e openat fw_printenv nandboot
		  openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_LARGEFILE|O_CLOEXEC) = 3
		  openat(AT_FDCWD, "/usr/lib/libubootenv.so.0.2", O_RDONLY|O_LARGEFILE|O_CLOEXEC) = 3
		  openat(AT_FDCWD, "/lib/libz.so.1", O_RDONLY|O_LARGEFILE|O_CLOEXEC) = 3
		  openat(AT_FDCWD, "/lib/libc.so.6", O_RDONLY|O_LARGEFILE|O_CLOEXEC) = 3
		  openat(AT_FDCWD, "/etc/fw_env.config", O_RDONLY) = 3
		  openat(AT_FDCWD, "/dev/mtd12", O_RDONLY) = 4
		  openat(AT_FDCWD, "/dev/mtd15", O_RDONLY) = 4
		  openat(AT_FDCWD, "/var/lock/fw_printenv.lock", O_WRONLY|O_CREAT|O_TRUNC, 0666) = 3
		  openat(AT_FDCWD, "/dev/mtd12", O_RDONLY) = 4
		  openat(AT_FDCWD, "/dev/mtd15", O_RDONLY) = 4
		  
		  注释后
		  root@slot-120:~ [P2]# strace -e openat fw_printenv nandboot
		  openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_LARGEFILE|O_CLOEXEC) = 3
		  openat(AT_FDCWD, "/usr/lib/libubootenv.so.0.2", O_RDONLY|O_LARGEFILE|O_CLOEXEC) = 3
		  openat(AT_FDCWD, "/lib/libz.so.1", O_RDONLY|O_LARGEFILE|O_CLOEXEC) = 3
		  openat(AT_FDCWD, "/lib/libc.so.6", O_RDONLY|O_LARGEFILE|O_CLOEXEC) = 3
		  openat(AT_FDCWD, "/etc/fw_env.config", O_RDONLY) = 3
		  openat(AT_FDCWD, "/dev/mtd12", O_RDONLY) = 4
		  openat(AT_FDCWD, "/var/lock/fw_printenv.lock", O_WRONLY|O_CREAT|O_TRUNC, 0666) = 3
		  openat(AT_FDCWD, "/dev/mtd12", O_RDONLY) = 4
		  Cannot read environment, using default
		  openat(AT_FDCWD, "/etc/u-boot-initial-env", O_RDONLY) = -1 ENOENT (No such file or directory)
		  Cannot read default environment from file
		  ```
		- [libubootenv](https://github.com/sbabic/libubootenv) #github
	- 常用命令
	  collapsed:: true
		- ```bash
		  Monitor Commands - Overview:
		  ============================
		  
		  go      - start application at address 'addr'
		  run     - run commands in an environment variable
		  bootm   - boot application image from memory
		  bootp   - boot image via network using BootP/TFTP protocol
		  bootz   - boot zImage from memory
		  tftpboot- boot image via network using TFTP protocol
		                 and env variables "ipaddr" and "serverip"
		                 (and eventually "gatewayip")
		  tftpput - upload a file via network using TFTP protocol
		  rarpboot- boot image via network using RARP/TFTP protocol
		  diskboot- boot from IDE devicebootd   - boot default, i.e., run 'bootcmd'
		  loads   - load S-Record file over serial line
		  loadb   - load binary file over serial line (kermit mode)
		  md      - memory display
		  mm      - memory modify (auto-incrementing)
		  nm      - memory modify (constant address)
		  mw      - memory write (fill)
		  cp      - memory copy
		  cmp     - memory compare
		  crc32   - checksum calculation
		  i2c     - I2C sub-system
		  sspi    - SPI utility commands
		  base    - print or set address offset
		  printenv- print environment variables
		  setenv  - set environment variables
		  saveenv - save environment variables to persistent storage
		  protect - enable or disable FLASH write protection
		  erase   - erase FLASH memory
		  flinfo  - print FLASH memory information
		  nand    - NAND memory operations (see doc/README.nand)
		  bdinfo  - print Board Info structure
		  iminfo  - print header information for application image
		  coninfo - print console devices and informations
		  ide     - IDE sub-system
		  loop    - infinite loop on address range
		  loopw   - infinite write loop on address range
		  mtest   - simple RAM test
		  icache  - enable or disable instruction cache
		  dcache  - enable or disable data cache
		  reset   - Perform RESET of the CPU
		  echo    - echo args to console
		  version - print monitor version
		  help    - print online help
		  ?       - alias for 'help'
		  
		  
		  => ? (help)
		  ?       - alias for 'help'
		  base    - print or set address offset
		  bcm24   - bcm phy 1 byte access
		  bcm48   - bcm phy 4 byte access
		  bdinfo  - print Board Info structure
		  boot    - boot default, i.e., run 'bootcmd'
		  bootd   - boot default, i.e., run 'bootcmd'
		  ......
		  
		  => ? nand
		  nand - NAND sub-system
		  
		  Usage:
		  nand info - show available NAND devices
		  nand device [dev] - show or set current device
		  nand read - addr off|partition size
		  nand write - addr off|partition size
		      read/write 'size' bytes starting at offset 'off'
		      to/from memory address 'addr', skipping bad blocks.
		  
		  => ? md
		  md - memory display
		  
		  Usage:
		  md [.b, .w, .l] address [# of objects]
		  
		  
		  => ? mw
		  mw - memory write (fill)
		  
		  Usage:
		  mw [.b, .w, .l] address value [count]
		  
		  ```
	- [[nand]]
	- tftp
	  collapsed:: true
		- ```bash
		  tftp 0x87800000 zImage # 下载 zImage 到 DRAM 中
		  nand erase 0x40000000 0xA0000000 # 从地址 0x40000000 开始擦除 10MB 空间
		  nand write 0x87800000 0x40000000 0xA0000000 # 将接收到的zImage写到 NAND 中
		  
		  tftp 0x87800000 imux6.dtb # 下载设备树文件到 DRAM 中
		  nand erase 0x60000000 0x10000000 # 从地址 0x60000000 开始擦除 1MB 空间
		  nand write 0x87800000 0x60000000 0x10000000 # 将接收到的设备树文件写到 NAND 中
		  ```
	- fdt
	  collapsed:: true
		- ```bash
		  fdt addr 83000000 # 设置 fdt 的 DRAM 地址为 0x83000000
		  fdt header # 查看设备树头信息
		  fdt print # 查看设备树信息
		  ```
	- uboot常用命令
	  collapsed:: true
		- ```bash
		  => ? run
		  run - run commands in an environment variable
		  
		  Usage:
		  run var [...]
		      - run the commands in the environment variable(s) 'var'
		  => help run
		  run - run commands in an environment variable
		  
		  Usage:
		  run var [...]
		      - run the commands in the environment variable(s) 'var'
		  
		  => boot --help
		  boot - boot default, i.e., run 'bootcmd'
		  
		  => print bootcmd
		  bootcmd=if run loadbootscript; then; run bootscript; elif run loadbootscript2; then; run bootscript; else; echo Fail running script, run nandboot3; fi
		  => print loadbootscript
		  loadbootscript=sf probe; sf read $loadaddr $n_scr $s_scr; imi
		  => print loadaddr
		  loadaddr=1000000
		  => print n_scr
		  n_scr=0x160000
		  => print s_scr
		  s_scr=0x8000
		  => print loadbootscript2
		  loadbootscript2=sf probe; sf read $loadaddr $n_scr2 $s_scr; imi
		  => print bootscript
		  bootscript=echo Running bootscript ...; source ${loadaddr}
		  => print nandboot3
		  nandboot3=setenv bootargs root=$mtd_rfs3 rootfstype=jffs2 rw ramdisk_size=$rdsz console=$consoledev,$baudrate $othbootargs;nand read $loadaddr $n_k3 $ksz;nand read $fdtaddr $nfdt3 $fdtsz;bootm $loadaddr - $fdtaddr
		  
		  
		  => ? nand
		  nand - NAND sub-system
		  ......
		  
		  => print nandboot
		  nandboot=nandboot1
		  => printenv nandboot
		  nandboot=nandboot1
		  
		  # tftp
		  setenv ipaddr 10.10.40.15
		  setenv serverip 10.10.40.16
		  setenv gatewayip 10.10.40.1
		  setenv netmask 255.255.255.0
		  
		  setenv forbes4bootp "fspi 0 0x10400f0;fspi 1 0;fspi 2 0x10"
		  saveenv
		  run forbes4boot
		  ```
	- uboot环境变量
		- [U-Boot Environment Variables](https://dl.acm.org/doi/fullHtml/10.5555/2685503.2685505)
		- [Environment Variables](https://u-boot.readthedocs.io/en/latest/usage/environment.html)
		- [mkenvimage: a tool to generate a U-Boot environment binary image](https://bootlin.com/blog/mkenvimage-uboot-binary-env-generator/)
- bootscript
- U_BOOT_CMD
- 参考文档
	- [Makefiles](https://u-boot.readthedocs.io/en/latest/develop/makefiles.html#makefiles)
	- [uboot的配置流程分析](http://www.embeddedlinux.org.cn/emb-linux/system-development/201702/18-6190.html#)
	- [Porting U-Boot and Linux on new ARM boards: a step-by-step guide](https://elinux.org/images/2/2a/Schulz-how-to-support-new-board-u-boot-linux.pdf)
	- [U-Boot User’s Guide](https://software-dl.ti.com/processor-sdk-linux/esd/docs/06_03_00_106/AM335X/linux/Foundational_Components_U-Boot.html)
	- [U-Boot Boot-up Process](https://doc-en.rvspace.org/VisionFive2/PG_Ethernet/JH7110_SDK/u_boot_bootup_process.html)
	- [Directory hierarchy](https://u-boot.readthedocs.io/en/latest/develop/directories.html)
	- [U-Boot programming howto](http://xillybus.com/tutorials/uboot-hacking-howto-1)
	- [Board Initialisation Flow](https://elixir.bootlin.com/u-boot/v2023.07.02/source/README)
	- [U-Boot Reference Manual](https://hub.digi.com/dp/path=/support/asset/u-boot-reference-manual/) **ref**
	- [uboot source code](https://elixir.bootlin.com/u-boot/latest/source) #sourcecode
	- [The U-Boot Documentation](https://u-boot.readthedocs.io/en/latest/index.html)
	- [Porting U-Boot and Linux on new ARM boards](https://elinux.org/images/2/2a/Schulz-how-to-support-new-board-u-boot-linux.pdf)
	- [U-Boot Quick Reference](https://www.nxp.com/docs/en/user-guide/MEDIA5200UBPG.pdf)
	- [U-Boot programming](http://xillybus.com/tutorials/uboot-hacking-howto-1)
	- [How to setup a bootloader for an embedded linux machine](https://www.msystechnologies.com/blog/how-to-setup-a-bootloader-for-an-embedded-linux-machine/)
	- [Preloader and U-Boot Customization](https://www.rocketboards.org/foswiki/Documentation/PreloaderUbootCustomization131)
	- [uboot-tools](https://www.mankier.com/package/uboot-tools)
		- [mkimage](https://www.mankier.com/1/mkimage)
		- [dumpimage(1)](https://manpages.debian.org/experimental/u-boot-tools/dumpimage.1.en.html)
		- [fw_printenv(8)](https://manpages.debian.org/experimental/u-boot-tools/fw_printenv.8.en.html)
		- [fw_setenv(8)](https://manpages.debian.org/experimental/u-boot-tools/fw_setenv.8.en.html)
		- [kwboot(1)](https://manpages.debian.org/experimental/u-boot-tools/kwboot.1.en.html)
		- [mkimage(1)](https://manpages.debian.org/experimental/u-boot-tools/mkimage.1.en.html)
		- [mkenvimage](https://bootlin.com/blog/mkenvimage-uboot-binary-env-generator/)
		- [U-Boot/Tools](https://linux-sunxi.org/U-Boot/Tools)
	- [U-Boot Script](https://ece453.engr.wisc.edu/u-boot-script/)
		- [u-boot autoscript source code](https://elixir.bootlin.com/u-boot/U-Boot-0_4_1/source/common/cmd_autoscript.c#L55)
	- [x86 Assembly/Bootloaders](https://en.wikibooks.org/wiki/X86_Assembly/Bootloaders)
	- [uboot-startup-sequence](https://www.slideshare.net/HouchengLin/uboot-startup-sequence)
	- [Flameman/uboot](https://elinux.org/Flameman/uboot)
	- uboot启动流程
		- [u-boot启动流程分析(1)_平台相关部分](http://www.wowotech.net/u-boot/boot_flow_1.html/comment-page-2)
		- [uboot编译流程分析](http://www.pedestrian.com.cn/u-boot/compile.html)
		- [uboot-编译和启动流程](https://www.kancloud.cn/bjm123456/embedded/2576138)
		- [Tiny4412 u-boot分析（2）u-boot启动流程](https://www.cnblogs.com/CoderTian/p/5995409.html)
		- [Uboot启动流程分析 野火](https://doc.embedfire.com/lubancat/build_and_deploy/zh/latest/building_image/boot_image_analyse/boot_image_analyse.html)
		- [ARM Linux系统移植 U-Boot 启动流程](https://mrchen.love/Article/ID/57)
		- [uboot启动流程（1）](https://adtxl.com/index.php/archives/755.html)
	- [U-boot学习笔记 嵌入式项目，基于imx6q的编译执行](https://github.com/zhaojh329/U-boot-1/tree/master)
-