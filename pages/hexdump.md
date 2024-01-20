- ```bash
  对于 hexdump -C FILE 命令，结果中可能会出现'*'所表示的行
  000107a0  62 75 67 6c 69 6e 6b 00  00 00 00 00 00 00 00 00  |buglink.........|
  000107b0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
  *
  000107d0  00 00 00 0b 00 00 00 01  00 00 00 02 10 00 01 54  |...............T|
  000107e0  00 00 01 54 00 00 00 0d  00 00 00 00 00 00 00 00  |...T............|
  它的含义是"all the elided lines are the same as the preceding line"，即所有省略的行都和之前的行的
  内容一致，如果需要显示所有的结果可以使用-v选项
  
  -v
  	The -v option causes hexdump to display all input data. Without the -v option, any number of 
      groups of output lines, which would be identical to the immediately preceding group of output
      lines (except for the input offsets), are replaced with a line comprised of a single asterisk(*).
  ```
- 参考
	- [man hexdump](https://linux.die.net/man/1/hexdump)
	- [How Hexdump works](https://opensource.com/article/19/8/dig-binary-files-hexdump)