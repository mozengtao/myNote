- 命令示例
	- ```bash
	  root@slot-18:~ [P2]# pmap 3120
	  3120:   /opt/berwick/build/armstrong/powerpc/bin/cms cpu=0 ndsnpu=4
	  00001000      4K rw-s- fsl-usdpaa
	  00002000      4K rw-s- fsl-usdpaa
	  00004000     16K rw-s- fsl-usdpaa
	  00008000     16K rw-s- fsl-usdpaa
	  00100000     16K r-x--   [ anon ]
	  0f3c8000     28K r-x-- libatomic.so.1.2.0
	  0f3cf000     96K ----- libatomic.so.1.2.0
	  0f3e7000      4K r---- libatomic.so.1.2.0
	  0f3e8000      4K rw--- libatomic.so.1.2.0
	  ```
- 参考文档
	- [内存泄漏的在线排查](https://panzhongxian.cn/cn/2020/12/memory-leak-problem-1/)
	- [man 5 proc](https://man7.org/linux/man-pages/man5/proc.5.html)
	- [How to Check Memory Usage Per Process on Linux](https://linuxhint.com/check_memory_usage_process_linux/)
	- [man 2 mmap](https://man7.org/linux/man-pages/man2/mmap.2.html)
	- [浅谈内存泄漏](http://blog.wuzhenyu.com.cn/2021/04/27/talk-about-memory-leak.html)
	- [man 1 pmap](https://docs.oracle.com/cd/E19683-01/816-0210/6m6nb7mhj/index.html)