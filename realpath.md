- 获取文件绝对路径
- > print the resolved path
- ```bash
  morrism@localhost /tmp/x $ realpath a.out
  /tmp/x/a.out
  
  morrism@localhost /tmp/x/test $ readlink -f t1.txt
  /tmp/x/test/t1.txt
  
  ```
- 参考文档
	- [man realpath](https://man7.org/linux/man-pages/man1/realpath.1.html)