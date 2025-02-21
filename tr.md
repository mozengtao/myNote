- > tr - translate or delete characters
- 以换行方式显示进程运行的环境变量
	- ```bash
	  root@slot-120:~ [P1]# tr '\0' '\n' < /proc/32214/environ
	  SHELL=/bin/sh
	  RUNLEVEL=5
	  PWD=/opt/berwick/build/armstrong/powerpc/bin
	  runlevel=5
	  HOME=/
	  CCAPARCH=powerpc
	  ...
	  
	  
	  root@slot-120:~ [P1]# od -c /proc/32214/cmdline
	  0000000   /   o   p   t   /   b   e   r   w   i   c   k   /   b   u   i
	  0000020   l   d   /   a   r   m   s   t   r   o   n   g   /   p   o   w
	  0000040   e   r   p   c   /   b   i   n   /   c   m   s  \0   c   p   u
	  0000060   =   0  \0   n   d   s   n   p   u   =   4  \0   r   s   t   c
	  0000100   n   t   =   2  \0
	  0000105
	  ```
- 参考文档
	- [man tr](https://helpmanual.io/man1/tr/)