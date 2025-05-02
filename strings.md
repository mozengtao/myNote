strings
	print the sequences of printable characters in files

```bash
	# 直接显示进程的环境变量，不直观
	root@slot-50:/etc/init.d []# cat /proc/1621/environ
	SHELL=/bin/shPWD=/opt/berwick/build/armstrong/arm/binHOME=/CCAPARCH=armCCAPROOT_OTH=/opt_oth/berwickTERM=vt102CCAPROOT=/opt/berwickUSER=rootTAILF_LIBRARY_PATH=/lib-glibcSHLVL=1MNT=/opt/berwickLD_LIBRARY_PATH=/opt/berwick/tailf-confd-pkg/arm/lib/:/bcm/lib:/opt/berwick/build/librdkafka/arm/libPATH=/opt/berwick/build/turnbull-sw/arm/bin:/opt/berwick/tailf-confd-pkg/arm/bin:/sbin:/usr/sbin:/bin:/usr/binOLDPWD=/opt/berwick/build/armstrong/arm/bin_=/usr/bin/nohup
	
	# 通过strings命令显示进程的环境变量更直观
	root@slot-50$/etc/init.d []# strings /proc/1621/environ
	SHELL=/bin/sh
	PWD=/opt/berwick/build/armstrong/arm/bin
	HOME=/
	CCAPARCH=arm
	CCAPROOT_OTH=/opt_oth/berwick
	TERM=vt102
	CCAPROOT=/opt/berwick
	
	# 也可以利用tr命令进行展示
	root@slot-18:/var/log [P2]# cat /proc/3119/environ | tr '\0' '\n'
	SHELL=/bin/sh
	RUNLEVEL=5
	PWD=/opt/berwick/build/armstrong/powerpc/bin
	runlevel=5
	HOME=/
	CCAPARCH=powerpc
	CCAPROOT_OTH=/opt_oth/berwick
	TERM=linux
	CCAPROOT=/opt/berwick
```

[man strings](https://man7.org/linux/man-pages/man1/strings.1.html)  