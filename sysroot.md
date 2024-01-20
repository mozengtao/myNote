- sysroot的作用
	- 为了编译产生最终的输出结果(可执行程序，对象文件等输出类型)，C和C++编译器还需要查询并用到相关的头文件和库文件。默认情况下，编译器会在本机的标准位置查找这些文件，当进行交叉编译时，我们希望指定的特定的目录来查找头文件和库文件，sysroot 目录就是用来^^指定编译器查找头文件和库文件的根目录^^。
- 指定方法
	- ```bash
	  gcc hello.c --sysroot=/home/morris
	  
	  # -isysroot 只影响头文件查找的根目录
	  gcc hello.c -isysroot /home/morris
	  ```