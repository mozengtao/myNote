- > look up or signal processes based on name and other
- ```bash
  root@slot-120:/tmp [P2]# pgrep cms
  3116
  root@slot-120:/tmp [P2]# ps -ef | grep cms
  root      3116  2938  0 15:19 ?        00:00:18 /opt/berwick/build/armstrong/powerpc/bin/cms cpu=0 ndsnpu=4
  
  ```
- 参考文档
	- [man pgrep](https://linux.die.net/man/1/pgrep)