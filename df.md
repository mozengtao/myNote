- 示例
  collapsed:: true
	- 查看目录或文件所在的设备
		- ```bash
		  # 查看文件所在的设备或文件系统
		  root@slot-120:/opt/cal# df /etc/profile.d/
		  Filesystem     1K-blocks  Used Available Use% Mounted on
		  /dev/root         102400 91488     10912  90% /
		  
		  # 查看目录的挂载设备或文件系统
		  root@slot-120:/opt/cal# cat /proc/mounts
		  /dev/root / jffs2 rw,relatime 0 0
		  devtmpfs /dev devtmpfs rw,relatime,size=569528k,nr_inodes=142382,mode=755 0 0
		  proc /proc proc rw,relatime 0 0
		  sysfs /sys sysfs rw,relatime 0 0
		  ......
		  
		  # 通过proc下的命令行参数查看根目录挂载的真正设备
		  root@slot-120:/opt/cal# cat /proc/cmdline
		  root=/dev/mtdblock2 rootfstype=jffs2 rw ramdisk_size=20000000 console=ttyS0,115200 usdpaa_mem=256M bportals=s0 qportals=s0 firmware_class.path=/ default_hugepagesz=256m hugepagesz=256m hugepages=1 isolcpus=1-7
		  
		  # 通过stat命令查看根目录挂载的真正设备
		  root@slot-120:/opt/cal# stat /
		    File: /
		    Size: 0               Blocks: 0          IO Block: 1      directory
		  Device: 1f02h/7938d     Inode: 1           Links: 23
		  Access: (0755/drwxr-xr-x)  Uid: (    0/    root)   Gid: (    0/    root)
		  Access: 2018-03-09 20:34:56.000000000 +0800
		  Modify: 2023-01-28 08:58:49.000000000 +0800
		  Change: 2023-01-28 08:58:49.000000000 +0800
		   Birth: -
		  # Device: 1f02h/7938d 意味着主设备号 0x1f，次设备号0x02
		  
		  # 查找对应主设备号 0x1f，次设备号0x02的设备
		  root@slot-120:/opt/cal# ls -l /dev/ | grep "31, *2 "
		  brw-rw---- 1 root disk     31,   2 Jan  1  1970 mtdblock2
		  ```
		- ```bash
		  [morrism@centos-base freescale]$ df -hP /homes
		  Filesystem               Size  Used Avail Use% Mounted on
		  /dev/mapper/centos-root  197G  127G   71G  65% /
		  ```
- 参考文档
	- [man df](https://linuxcommand.org/lc3_man_pages/df1.html)
	- [General overview of the Linux file system](https://tldp.org/LDP/intro-linux/html/sect_03_01.html)
	- [How to find the root device](https://bootlin.com/blog/find-root-device/)
-