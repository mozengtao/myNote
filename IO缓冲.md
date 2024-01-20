- 缓冲
	- 为了减少使用read和write调用的次数，标准IO库提供了缓冲
		- 块缓冲
			- 填满标准I/O缓冲区后，才进行实际的I/O操作，写磁盘文件通常是块缓冲
			- ```c
			  #include<stdio.h>
			  #include<unistd.h>
			  int main(void)
			  {
			      /*以可读可写的方式打开*/
			      FILE *fp = fopen("./test.txt","w+");
			      if(NULL == fp)
			      {
			          perror("open file failed");
			          return -1;
			      }
			      /*写入内容*/
			      char buf[] = "wechat:shouwangxiansheng\n";
			      fwrite(buf,sizeof(char),sizeof(buf),fp);
			      //fflush(fp);
			      /*sleep一段时间，以便观察*/
			      sleep(20);
			      fclose(fp);
			      return 0;
			  }
			  ```
		- 行缓冲
			- 遇到换行符或缓冲区已满(一般1024字节)，才执行实际的I/O操作
			- ```c
			  #include<stdio.h>
			  #include<unistd.h>
			  int main(void)
			  {
			      printf("wechat:shouwangxiansheng");
			      sleep(10);
			      return 0;
			  }
			  ```
		- 无缓冲
			- ```c
			  #include<stdio.h>
			  #include<unistd.h>
			  int main(void)
			  {
			      fprintf(stderr,"wechat:shouwangxiansheng");
			      sleep(10);
			      return 0;
			  }
			  ```
	- 常用输入输出的缓冲类型
		- 通常磁盘上的文件是块缓冲
		- 标准输入和标准输入通常是行缓冲的
		- 指向终端设备的流通常是行缓冲，而指向文件时，则是块缓冲
		- 为了尽可能显示错误信息，标准错误是不带缓冲的
	- setbuf
		- 用来设置流的缓冲buffer
		- [man 3 setbuffer](https://man7.org/linux/man-pages/man3/setbuf.3.html)
- [Ensuring data reaches disk](https://lwn.net/Articles/457667/)