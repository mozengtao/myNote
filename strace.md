- strace 是 linux 下的一个诊断工具，可以用来捕获linux的系统调用命令以及信号，可以用来调试或定位问题。
- 常用选项
	- ```bash
	  # strace 版本
	  root@slot-120:~ [P2]# strace -V
	  strace -- version 5.5
	  
	  # 保存trace结果到文件
	  strace -o /tmp/strace.ls ls
	  
	  # verbose 模式
	  morrism@localhost /tmp/x $ strace -v ls
	  execve("/usr/bin/ls", ["ls"], ["LS_COLORS=rs=0:di=01;34:ln=01;36"..., "SSH_CONNECTION=10.254.2.112 6437"..., "MODULES_RUN_QUARANTINE=LD_LIBRAR"..., "LANG=en_U
	  S.UTF-8", "HISTCONTROL=ignoredups", "HOSTNAME=localhost.localdomain", "KDEDIRS=/usr", "JENKINS_FPGA_URL=http://jenkins."..., "S_COLORS=auto", "XDG_SESSION_ID=
	  2", "MODULES_CMD=/usr/share/Modules/l"..., "USER=morrism", "SELINUX_ROLE_REQUESTED=", "PWD=/tmp/x", "SSH_ASKPASS=/usr/libexec/openssh"..., "HOME=/home/morrism
	  ", "SSH_CLIENT=10.254.60.57 57496 22", "TMUX=/tmp/tmux-69157242/default,"..., "SELINUX_LEVEL_REQUESTED=", "https_proxy=http://10.254.3.3:31"..., "XDG_DATA_DIR
	  S=/home/morrism/.loc"..., "http_proxy=http://10.254.3.3:312"..., "ARTIFACTORY_EXT_URL=https://arti"..., "CSCOPE_EDITOR=vim", "BB_MIRROR_URL=ssh://git@bitbucke
	  "..., "LOADEDMODULES=", "no_proxy=.vecima.com,localhost,1"..., "SSH_TTY=/dev/pts/0", "MAIL=/var/spool/mail/morrism", "TERM=screen", "SHELL=/bin/bash", "all_pr
	  oxy=http://10.254.3.3:3128", "SELINUX_USE_CURRENT_RANGE=", "BITBUCKET_MIRROR=ssh://git@bitbu"..., "TMUX_PANE=%47", "SHLVL=2", "MANPATH=:", "ARTIFACTORY_URL=ht
	  tps://artifact"..., "MODULEPATH=/etc/scl/modulefiles:"..., "LOGNAME=morrism", "DBUS_SESSION_BUS_ADDRESS=unix:pa"..., "XDG_RUNTIME_DIR=/run/user/691572"..., "M
	  ODULEPATH_modshare=/usr/share/m"..., "PATH=/usr/share/Modules/bin:/usr"..., "PS1=\\[\\033[01;32m\\]\\u@\\h \\[\\033["..., "MODULESHOME=/usr/share/Modules", "H
	  ISTSIZE=1000", "ftp_proxy=http://10.254.3.3:3128", "LESSOPEN=||/usr/bin/lesspipe.sh "..., "BASH_FUNC_module%%=() {  _module"..., "BASH_FUNC__module_raw%%=() {
	    un"..., "BASH_FUNC_switchml%%=() {  types"..., "BASH_FUNC_scl%%=() {  if [ \"$1\" "..., "BASH_FUNC_ml%%=() {  module ml \""..., "_=/usr/bin/strace", "OLDPWD
	  =/home/morrism"]) = 0
	  ......
	  
	  # trace子进程
	  strace -f program
	  
	  # trace系统调用的统计信息
	  morrism@localhost /tmp/x $ strace -c ls
	  a.out  dad.c                      fridge  main.c  mom.c       result.txt  t1.sh  test1    test.c        test.o       trace1.txt
	  dad    embedded-linux-primer.pdf  main    mom     output.txt  softlink    test   test1.c  test_dynamic  test_static  trace2.txt
	  % time     seconds  usecs/call     calls    errors syscall
	  ------ ----------- ----------- --------- --------- ----------------
	   43.24    0.000374         374         1           execve
	   18.15    0.000157           7        20           mmap
	    8.44    0.000073           8         9           openat
	    8.21    0.000071           5        13           read
	    8.21    0.000071           5        14           mprotect
	    4.05    0.000035           3        11           close
	    3.93    0.000034           3        10           fstat
	    3.47    0.000030           4         7           lseek
	    1.16    0.000010           5         2         1 access
	    0.58    0.000005           1         3           brk
	    0.58    0.000005           2         2         1 arch_prctl
	  ......
	  ------ ----------- ----------- --------- --------- ----------------
	  100.00    0.000865           8       107         2 total
	  
	  # 指定需要统计的系统调用
	  morrism@localhost /tmp/x $ strace -e read,write ls
	  read(3, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\200z\0\0\0\0\0\0"..., 832) = 832
	  read(3, "\4\0\0\0 \0\0\0\5\0\0\0GNU\0\1\0\0\300\4\0\0\0\30\0\0\0\0\0\0\0"..., 48) = 48
	  read(3, "\4\0\0\0 \0\0\0\5\0\0\0GNU\0\1\0\0\300\4\0\0\0\30\0\0\0\0\0\0\0"..., 48) = 48
	  read(3, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\300\30\0\0\0\0\0\0"..., 832) = 832
	  read(3, "\4\0\0\0\20\0\0\0\5\0\0\0GNU\0\2\0\0\300\4\0\0\0\3\0\0\0\0\0\0\0", 32) = 32
	  read(3, "\4\0\0\0\20\0\0\0\5\0\0\0GNU\0\2\0\0\300\4\0\0\0\3\0\0\0\0\0\0\0", 32) = 32
	  read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\2405\2\0\0\0\0\0"..., 832) = 832
	  read(3, "\4\0\0\0\20\0\0\0\5\0\0\0GNU\0\2\0\0\300\4\0\0\0\3\0\0\0\0\0\0\0", 32) = 32
	  read(3, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\200#\0\0\0\0\0\0"..., 832) = 832
	  read(3, "\4\0\0\0\20\0\0\0\5\0\0\0GNU\0\2\0\0\300\4\0\0\0\3\0\0\0\0\0\0\0", 32) = 32
	  read(3, "\4\0\0\0\20\0\0\0\5\0\0\0GNU\0\2\0\0\300\4\0\0\0\3\0\0\0\0\0\0\0", 32) = 32
	  read(3, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\300\20\0\0\0\0\0\0"..., 832) = 832
	  read(3, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0pn\0\0\0\0\0\0"..., 832) = 832
	  write(1, "a.out  dad.c\t\t\t  fridge  main.c "..., 102a.out  dad.c                        fridge  main.c  mom.c       result.txt  t1.sh  test1    test.c     test.o        trace1.txt
	  ) = 102
	  write(1, "dad    embedded-linux-primer.pdf"..., 122dad    embedded-linux-primer.pdf  main         mom     output.txt  softlink    test   test1.c  test_dynamictest_static  trace2.txt
	  ) = 122
	  +++ exited with 0 +++
	  
	  # attach到指定的进程进行观测
	  strace -p 22443
	  
	  # 显示系统调用的时间戳
	  strace -t ls
	  
	  # 显示每个系统调用所花费的时间
	  strace -r ls
	  
	  
	  ```
	- strace pwd
	- strace -p pid
	- strace -c whoami
- 参考文档
	- [Strace](https://eklitzke.org/strace)
	- [**man strace**](https://man7.org/linux/man-pages/man1/strace.1.html)
	- [strace command](https://www.baeldung.com/linux/strace-command)
	- [**The strace Command in Linux**](https://www.baeldung.com/linux/strace-command)
		```bash
		strace pwd

		sh -c 'echo $$; exec sleep 60'
		strace -p xxx

		改变环境变量的值
		strace -E var1=val1 pwd

		以另外一个用户运行和跟踪程序
		strace -u xxx whoami

		获取时间信息
		strace -t whoami

		获取统计信息
		strace -c whoami

		获取并排序统计信息
		strace -c -S errors whoami

		strace 表达式
		strace -e qualifier=[!]value[,value] command
		The qualifier must be from the list of trace, status, signal, quiet, abbrev, verbose, raw, read, write, fault, and inject.
			filtering (trace, status, signal, quiet)
			output formatting (abbrev, verbose, raw)
			syscalls tampering (fault, inject)
			file descriptor data dumping (read, write)
		Filtering Output by Syscall Name:
		strace -e trace=fstat whoami
		strace -e trace=!fstat whoami

		Filtering Output by Return Status:
		strace -e status=!successful whoami
		strace -e status=unfinished,unavailable whoami

		Filtering Output by Signal:
		strace -e signal=SIGBUS whoami

		Suppressing Additional Informational Message:
		strace -e quiet=exit whoami

		Formatting the Output
		Dereferencing Syscall Arguments:
		strace -e verbose=none whoami

		Abbreviating Syscall:
		strace -e whoami
		strace -e abbrev=none whoami

		Displaying Undecoded Arguments:
		strace whoami
		strace -e raw=execve whoami

		Syscall Tampering
		One of the most powerful features of strace expression is its ability to alter the syscall behavior using inject and fault qualifiers
		strace -e --inject=syscall_set[:error=errno|:retval=value][:signal=sig][:syscall=syscall][:delay_enter=delay][:delay_exit=delay][:when=expr] command

		Injecting Fault Into Syscalls
		strace -e inject=fstat:error=EPERM whoami

		Controlling When Does the Faults Get Injected
		strace -e inject=fstat:error=EPERM:when=2 whoami
		strace -e inject=fstat:error=EPERM:when=2+ whoami
		strace -e inject=fstat:error=EPERM:when=2+2 whoami

		Introducing Delays in Syscalls
		strace -e inject=fstat:delay_enter=2000000 whoami

		Inject the delay after the syscall:
		strace -e inject=fstat:delay_exit=2000000 whoami

		File Descriptor Data Dumping
		Dumping File Descriptors’ Data on Every Input Activity
		strace -e read=3 whoami (dump the data whenever there’s input activity on file descriptor 3)

		Dumping File Descriptors’ Data on Every Output Activity
		strace -e write=5 whoami (dump the data of file descriptor 5 on every output activity)

		print instruction pointer at the time of system call:
		strace -i ls

		print timestamp for each system call:
		strace -r ls

		prefix each output line with clock time:
		strace -t ls

		show time spent in system calls:
		strace -T ls

		跟踪程序并输出到文件
		strace -o trace.txt ls

		跟踪特定系统调用
		strace -e trace=open,close ls

		跟踪子进程的系统调用
		strace -f ls

		限制输出的字符串长度
		strace -s 100 ls

		跟踪网络相关的系统调用
		strace -e trace=network wget http://example.com

		跟踪文件I/O相关的系统调用
		strace -e trace=file ls /tmp

		跟踪内存相关的系统调用
		strace -e trace=memory ls

		跟踪信号交互
		strace -e trace=signal kill -USR1 `pidof myprocess`

		实时显示时间戳
		strace -tt ls

		以图形化方式显示跟踪信息
		strace -o output.txt -ttT -ff myprogram
		strace-graph < output.txt > graph.txt
		```
	- [Linux How to use strace - debugging - troubleshooting](https://www.math-linux.com/linux/tutorials/article/linux-how-to-use-strace-debugging-troubleshooting)
	- [Exploring System Internals with lsof and strace](http://www.myhowto.org/solving-problems/7-exploring-system-internals-with-lsof-and-strace/)
	- [强大的strace命令用法详解](https://blog.csdn.net/cs729298/article/details/81906375)