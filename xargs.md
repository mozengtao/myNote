- xargs处理的优先级或顺序：**先分割，再分批，然后传递到参数位**
- ```bash
  # 直接将标准输入的内容传递给cat
  [root@xuexi tmp]# echo "/etc/inittab" | cat 
  
  # 将标准输入的内容经过xargs处理后传递给cat
  [root@xuexi tmp]# echo "/etc/inittab" | xargs cat   
  
  # 将搜索的文件传递给grep的参数位进行搜索，若不使用xargs，则grep将报错
  [root@xuexi tmp]# find /etc -maxdepth 1 -name "*.conf" -print0 | xargs -0 -i grep "hostname" -l {} 
  ```
- 背景
	- Linux 下^^有些命令不接受 ”标准输入“ 作为参数^^
		- 管道命令 `|` 会将管道左侧的标准输出转化为管道右侧的标准输入，例如 `cat /etc/passwd | grep root`，管理会将 `cat /etc/passwd` 的标准输出转换为 `grep root` 的标准输入进行处理
		- `grep` 命令接受 ”标准输入“ 作为参数，因此 `cat /etc/passwd | grep root` 等价于 `grep root /etc/passwd`
		- `echo` 命令不接受 “标准输入” 作为参数，`echo "hello world" | echo` 不会有内容输出
			- 解决办法：`echo "hello world" | xargs echo`
- xargs 命令的作用
	- ^^将标准输入转换为命令行参数^^
- xargs 常用参数
	- ```bash
	         -t, --verbose
	                Print the command line on the standard error output before executing it.
	         --delimiter=delim, -d delim
	                Input items are terminated by the specified character.  The specified delimiter may be a single character, a C-style character escape such as \n, or an octal or  hexadeci‐
	                mal escape code.  Octal and hexadecimal escape codes are understood as for the printf command.   Multibyte characters are not supported.  When processing the input, quotes
	                and backslash are not special; every character in the input is taken literally.  The -d option disables any end-of-file string, which is treated like any  other  argument.
	                You  can  use this option when the input consists of simply newline-separated items, although it is almost always better to design your program to use --null where this is
	                possible.
	         -p, --interactive
	                Prompt the user about whether to run each command line and read a line from the terminal.  Only run the command line if the response starts with `y' or `Y'.  Implies -t.
	  
	  		-I replace-str
	                Replace occurrences of replace-str in the initial-arguments with names read from standard input.  Also, unquoted blanks do not terminate input items; instead the separator
	                is the newline character.  Implies -x and -L 1.
	  
	  		-n max-args, --max-args=max-args
	                Use at most max-args arguments per command line.  Fewer than max-args arguments will be used if the size (see the -s option) is exceeded, unless the -x option is given, in
	                which case xargs will exit.
	  
	  
	         find /tmp -name core -type f -print0 | xargs -0 /bin/rm -f
	         Find files named core in or below the directory /tmp and delete them, processing filenames in such a way that file or directory names containing spaces or newlines are  correctly
	         handled.
	  
	  ```
	- `-0` 与 `find` 命令
		- `find` 命令有一个特殊的参数 `-print0` 指定输出的文件列表以 `null` 分割，`xargs` 命令的  `-0` 选项表示用 `null` 当做分隔符，如 `find /path -name "*.tmp" -print0 | xargs -0 rm`
	- `-L`
		- 如果标准输入包含多行，`-L` 指定多少行作为一个命令行参数，如 `echo -e "a\nb\c\nc" | xargs -L 1 `
	- `-n`
		- 如果同一行包含多项时，`-n` 用来指定执行一次命令的参数个数，如 `echo {0..9} | xargs -n 2 echo`
	- `-I`
		- 将一个命令行参数传递给多个命令时，`-I` 用来指定每一项命令行参数的替代字符串，如 `cat files.txt | xargs -I file sh -c 'echo file; touch file'`
	- `--max-procs`
		- `xargs` 默认只用一个进程执行命令。如果命令要执行多次，必须等上一次执行完，才能执行下一次。
		- `--max-procs` 参数指定同时用多少个进程并行执行命令。 `--max-procs 2` 表示同时最多使用两个进程， `--max-procs 0` 表示不限制进程数。如 `docker ps -q | xargs -n 1 --max-procs 0 docker kill` 表示关闭尽可能多的 `Docker` 容器，速度上会快一些
- 参考文档
	- [xargs原理剖析及用法详解](https://www.cnblogs.com/f-ck-need-u/p/5925923.html)
	- [man xargs](https://www.man7.org/linux/man-pages/man1/xargs.1.html)
	- [How to Use the xargs Command on Linux](https://www.howtogeek.com/435164/how-to-use-the-xargs-command-on-linux/)
	- [Using xargs in Combination With bash -c to Create Complex Commands](https://www.howtogeek.com/devops/using-xargs-in-combination-with-bash-c-to-create-complex-commands/)