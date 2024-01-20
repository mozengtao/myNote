- > Logical volume manager (LVM) introduces an extra layer between the physical disks and the file system allowing file systems to be :
  – resized and moved easily and online without requiring a system-wide outage.
  – Using discontinuous space on disk
  – meaningful names to volumes, rather than the usual cryptic device names.
  – span multiple physical disks
- lvm相关概念
	- ![image.png](../assets/image_1673592791917_0.png)
	- ![image.png](../assets/image_1673592803241_0.png)
	- pv
		- Physical volume (e.g. a partition, RAID array, etc)
	- vg
		- Volume Group - a collection of PV’s that we can use the space from
	- lv
		- Logical volume - a partition created from space in a VG
	- file system
- 扩展逻辑卷的大小
	- 创建新的硬盘分区
	- 添加创建的分区为物理卷
	- 添加物理卷到指定卷组
	- 从指定卷组分配空间给指定的逻辑卷
	- 调整文件系统大小
		- ```bash
		  # 创建新的分区/dev/sda3
		  fdisk /dev/sda
		  
		  # 创建物理卷
		  pvdisplay
		  pvcreate /dev/sda3
		  pvdisplay
		  
		  # 添加物理卷到指定卷组
		  vgdisplay | grep Name
		  vgextend vg_vmware /dev/sda3
		  vgdisplay
		  
		  # 从指定卷组分配空间给指定的逻辑卷
		  lvdisplay | grep Path
		  lvextend -l +100%FREE /dev/vg_vmware/lv_root
		  lvdisplay
		  
		  # 调整文件系统大小
		  resize2fs /dev/vg_vmware/lv_root
		  
		  # 查看调整后的文件系统大小
		  df -lh
		  ```
- 参考文档
	- [Device Mapper and Linux LVM](https://blog.codefarm.me/2021/11/29/device-mapper-and-linux-lvm/)
	- [Logical Volume Management on Linux](https://www.slashroot.in/advanced-guide-lvm-logical-volume-management-linux-part-1)
	- [lvm cheat sheet](http://www.datadisk.co.uk/html_docs/redhat/rh_lvm.htm)
	- [Beginner’s Guide to LVM](https://www.thegeekdiary.com/redhat-centos-a-beginners-guide-to-lvm-logical-volume-manager/)
	- [lvm howto](https://tldp.org/HOWTO/LVM-HOWTO/index.html)
	-
-