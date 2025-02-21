- 相关文件
	- `/etc/fstab`
		- ```bash
		  root@slot-50:/tmp []# cat /etc/fstab
		  # /etc/fstab: static file system information.
		  #
		  # <file system> <mount pt>     <type>   <options>         <dump> <pass>
		  /dev/root       /              ext2     rw,noauto         0      1
		  proc            /proc          proc     defaults          0      0
		  devpts          /dev/pts       devpts   defaults,gid=5,mode=620   0      0
		  tmpfs           /dev/shm       tmpfs    mode=0777         0      0
		  tmpfs           /run           tmpfs    mode=0755,nodev,nosuid 0  0
		  tmpfs           /tmp           tmpfs    defaults,size=500M          0      0
		  tmpfs           /var/volatile  tmpfs    defaults              0  0
		  sysfs           /sys           sysfs    defaults          0      0
		  none            /sys/kernel/debug       debugfs defaults        0       0
		  ```
	- `/proc/mounts`
	  collapsed:: true
		- ```bash
		  root@slot-52:/etc/init.d [P2]# cat /proc/mounts
		  rootfs / rootfs rw 0 0
		  ubi0_0 / ubifs ro,sync,relatime 0 0
		  devtmpfs /dev devtmpfs rw,relatime,size=381880k,nr_inodes=95470,mode=755 0 0
		  proc /proc proc rw,relatime 0 0
		  devpts /dev/pts devpts rw,relatime,gid=5,mode=620 0 0
		  tmpfs /dev/shm tmpfs rw,relatime,mode=777 0 0
		  tmpfs /run tmpfs rw,nosuid,nodev,relatime,mode=755 0 0
		  tmpfs /tmp tmpfs rw,relatime,size=512000k 0 0
		  tmpfs /var/volatile tmpfs rw,relatime 0 0
		  sysfs /sys sysfs rw,relatime 0 0
		  none /sys/kernel/debug debugfs rw,relatime 0 0
		  ubi14:app2 /opt ubifs rw,sync,relatime 0 0
		  ubi10:app /opt_oth ubifs rw,sync,relatime 0 0
		  
		  ```
	- mount选项
		- ```bash
		  -a, --all
		  	Mount all filesystems (of the given types) mentioned in fstab (except for those whose line contains the noauto keyword).  The filesystems are mounted following their order
		  	in  fstab.  The mount command compares filesystem source, target (and fs root for bind mount or btrfs) to detect already mounted filesystems. The kernel table with already
		  	mounted filesystems is cached during mount --all. It means that all duplicated fstab entries will be mounted.
		  
		  	Note that it is a bad practice to use mount -a for fstab checking. The recommended solution is findmnt --verify.
		  ```
- 参考文档
	- [man mount](https://man7.org/linux/man-pages/man8/mount.8.html)
	- [man fstab](https://man7.org/linux/man-pages/man5/fstab.5.html)
	-