- > Init is the parent of all processes. Its primary role is to create processes from a script stored in the file **/etc/inittab**. This file usually has entries which cause init to spawn gettys on each line that users can log in. It also controls autonomous processes required by any particular system.
- 如何确定本机的 init system 类型
	- ```sh
	  morrism@PC2886:~$ ps --pid 1
	    PID TTY          TIME CMD
	      1 ?        00:00:00 init
	  ```
- init启动过程
	- 对于bash脚本，可以通过[[bash]]内置的`set`命令的`-x`选项使能trace功能来打印命令的执行过程
	- ```bash
	  /sbin/init是所有用户进程的父进程，它的工作是根据/etc/inittab中的条目创建其他的用户进程，
	  /sbin/init是内核完成启动之后运行的第一个用户进程，尽管如此，init还是被认为是启动过程的一部分
	  ```
	- inittab示例
		- ```bash
		  root@slot-18:~ [P2]# cat /etc/inittab
		  # /etc/inittab: init(8) configuration.
		  ......
		  # The default runlevel.
		  id:5:initdefault:
		  
		  # Boot-time system configuration/initialization script.
		  # This is run first except when booting in emergency (-b) mode.
		  si::sysinit:/etc/init.d/rcS		>>>>>> 启动时第一个执行的脚本
		  
		  # What to do in single-user mode.
		  ~~:S:wait:/sbin/sulogin
		  
		  # /etc/init.d executes the S and K scripts upon change
		  # of runlevel.
		  #
		  # Runlevel 0 is halt.
		  # Runlevel 1 is single-user.
		  # Runlevels 2-5 are multi-user.
		  # Runlevel 6 is reboot.
		  
		  l0:0:wait:/etc/init.d/rc 0
		  l1:1:wait:/etc/init.d/rc 1
		  l2:2:wait:/etc/init.d/rc 2
		  l3:3:wait:/etc/init.d/rc 3
		  l4:4:wait:/etc/init.d/rc 4
		  l5:5:wait:/etc/init.d/rc 5		>>>>>> runlevel为5时执行命令/etc/init.d/rc 5
		  l6:6:wait:/etc/init.d/rc 6
		  # Normally not reached, but fallthrough in case of emergency.
		  z6:6:respawn:/sbin/sulogin
		  S0:12345:respawn:/bin/start_getty 115200 ttyS0 vt102
		  ```
- [[runlevel]]
	- 打印之前和当前的SysV运行级别
	- > When you request the **init** command to change run levels, all processes without an entry in the *runlevel* field for the target run level receive a warning signal (**SIGTERM**). There is a 20-second grace period before processes are forcibly terminated by the kill signal (**SIGKILL**).
- 参考文档
	- [/etc/inittab](https://sites.ualberta.ca/dept/chemeng/AIX-43/share/man/info/C/a_doc_lib/files/aixfiles/inittab.htm)
	- [man inittab](https://www.cyberciti.biz/howto/question/man/inittab-man-page.php)
		- ```bash
		  Identifier:RunLevel:Action:Command
		  or
		  id:runlevels:action:process
		  (If no run level is specified, the process is assumed to be valid at all run levels.)
		  
		  Action
		  Action字段指定了init命令如果处理Command字段指定的命令，常用的action如下
		  	initdefault		
		  		An initdefault entry specifies the runlevel which should be entered after system boot. 
		          If none exists, init will ask for a runlevel on the console. 
		          The process field is ignored.
		  	sysinit
		      	The process will be executed during system boot. 
		          It will be executed before any boot or bootwait entries. 
		          The runlevels field is ignored.
		  	wait
		      	The process will be started once when the specified runlevel is entered and init 
		          will wait for its termination.
		      respawn
		      	The process will be restarted whenever it terminates (e.g. getty).
		  ```
	- [[update-rc.d]]
	- [Run Control Scripts](https://docs.huihoo.com/opensolaris/system-administration-guide-basic-administration/html/ch14s10.html)
	- [The /sbin/init Program](http://coffeenix.net/doc/kernel/startup.html/x394.html)
	- [man init](https://linux.die.net/man/8/init)
	- [Special Files/Directories](https://www.sharetechnote.com/html/Linux_SpecialFiles.html)
	- [init and runlevels](http://linux-training.be/sysadmin/ch15.html)
		- ```bash
		  # in /etc/inittab
		  si::sysinit:/etc/init.d/rcS
		  
		  # in /etc/init.d/rcS
		  exec /etc/init.d/rc S
		  
		  即/etc/init.d/rcS  will always run（independent of the selected runlevel）,The script is 
		  actually running all scripts in the /etc/rcS.d/ directory in alphabetical order.
		  (即/etc/rcS.d/目录下的脚本会最先执行，按照字母顺序顺序执行)
		  
		  ```
	- [**init**](https://www.learnlinux.org.za/courses/build/internals/ch03s02)