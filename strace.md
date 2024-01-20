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
	- [man strace](https://man7.org/linux/man-pages/man1/strace.1.html)
	- [strace command](https://www.baeldung.com/linux/strace-command)
	- [The strace Command in Linux](https://www.baeldung.com/linux/strace-command)
	- [Linux How to use strace - debugging - troubleshooting](https://www.math-linux.com/linux/tutorials/article/linux-how-to-use-strace-debugging-troubleshooting)
	- [Exploring System Internals with lsof and strace](http://www.myhowto.org/solving-problems/7-exploring-system-internals-with-lsof-and-strace/)