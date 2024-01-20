- > time a simple command or give resource usage
- 示例
	- ```bash
	  morrism@localhost ~ $ time ls
	   agnew    core.816636   ecm324   morris      repos      routing.txt   t1.py   tmp    'tr'$'\033'
	   backup   debug         lua      nohup.out   req-3957   sionqq        t1.sh   tools
	  
	  real    0m0.003s
	  user    0m0.002s
	  sys     0m0.001s
	  
	  real: clock time, 墙上时间，从程序开始执行到程序执行结束经过的时间
	  user: 程序在用户态花费的时间
	  sys:  程序在内核态花费的时间
	  
	  注意：在多核系统上，user或者sys时间可能大于real时间
	  ```
- 参考文档
	- [man time](https://man7.org/linux/man-pages/man1/time.1.html)