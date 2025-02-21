- [Kernel Module](https://www.codeinsideout.com/blog/android/kernel-module/#loadable-kernel-module)
- [[Internals of insmod and rmmod]]
  id:: 64146e74-d28d-4f05-95f7-e876551a1263
- 相关命令
  collapsed:: true
	- ```bash
	  # 查看用户手动insmod插入的模块
	  root@slot-18:~ [P2]# lsmod
	  Module                  Size  Used by
	  shm                    11847  0
	  l1d                     6263  0
	  hetmgr                 10873  0
	  
	  # 查看模块的配置选项（y, ...）
	  root@slot-18:~ [P2]# zcat /proc/config.gz | grep DPAA
	  CONFIG_FSL_USDPAA=y
	  CONFIG_FSL_DPAA_ETH_MAX_BUF_COUNT=128
	  CONFIG_FSL_SDK_DPAA_ETH=y
	  
	  
	  # 查看编入linux kernel的模块
	  root@slot-18:~ [P2]# cat /lib/modules/$(uname -r)/modules.builtin
	  kernel/kernel/configs.ko
	  kernel/fs/binfmt_script.ko
	  kernel/fs/mbcache.ko
	  kernel/fs/exportfs/exportfs.ko
	  kernel/fs/ext2/ext2.ko
	  kernel/fs/fuse/fuse.ko
	  kernel/fs/jffs2/jffs2.ko
	  kernel/fs/lockd/lockd.ko
	  kernel/fs/nfs/nfs.ko
	  kernel/fs/nfs/nfsv2.ko
	  kernel/fs/nfs/nfsv3.ko
	  
	  ```
- 参考文档
	- [Linux Device Driver example for dump_stack() to print the stack trace of module loading](https://www.thegeekdiary.com/linux-device-driver-example-for-dump_stack-to-print-the-stack-trace-of-module-loading/)