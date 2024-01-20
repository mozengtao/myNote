- 示例
	- ```bash
	  root@slot-120:~ [P2]# file -s /dev/mtd0
	  /dev/mtd0: u-boot legacy uImage, Linux-4.1.8-V3.1-dev-1819, Linux/PowerPC, OS Kernel Image (gzip), 5852574 bytes, Fri Jun  4 13:57:11 2021, 
	  Load Address: 0x00000000, Entry Point: 0x00000000, Header CRC: 0x0C3150B7, Data CRC: 0xFCEC6B8B
	  
	  root@slot-120:~ [P2]# cat /proc/mtd
	  dev:    size   erasesize  name
	  mtd0: 00600000 00080000 "kernel1"
	  mtd1: 00080000 00080000 "DTB1"
	  mtd2: 06400000 00080000 "RFS1"
	  ......
	  ```
- 参考文档
	- [Linux File Command: How to Determine File Type in Linux](https://phoenixnap.com/kb/linux-file-command)
	- [man file](https://man7.org/linux/man-pages/man1/file.1.html)