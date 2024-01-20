- [[ld]]
	- 编译时的静态链接器
- ld.so
	- 运行时动态链接器(加载器)，生命周期为运行时，当链接时指定库文件时会根据指定的路径加载指定的库
	- 运行时库文件搜索路径
		- 通过可执行文件中动态段DT_PATH指定，该动态段包含的路径，来自于在编译时，gcc加入连接参数`-Wl,-rpath`指定动态库搜索路径，多个路径之间用冒号分隔；例如`gcc -Wl,--rpath -Wl,${LIB_DIR1} ./app`
		- 通过环境变量LD_LIBRARY_PATH指定路径
		- `/etc/ld.so.cache`缓存的动态库路径，通过配置文件/etc/ld.so.conf增删路径（修改后需要运行ldconfig命令)
		- `/lib`
		- `/usr/lib`
- gcc库文件查找路径
	- `gcc -print-search-dirs`
- 动态链接库的使用
	- 编译生成动态链接库
		- `gcc -fPIC -shared myfunc.c -o libmyfunc.so`
			- 编译
				- `gcc -c -fPIC -o myfunc.o myfunc.c`
			- 生成动态库文件
				- `gcc -shared myfunc.o -o libmyfunc.so`
	- 使用动态链接库
		- 使用 运行时库文件搜索路径 指定搜索路径
		- `gcc -o test test.c -l myfunc -L . -Wl,-rpath=.`
			- `-Wl` 选项告诉编译器将后面的参数传递给链接器
				- > -Wl,option
				  Pass option as an option to the linker.  If option contains commas, it is split into multiple options at the commas.  You can use this syntax to pass an argument to the option.  For example, -Wl,-Map,output.map passes -Map output.map to the linker.  When using the GNU linker, you can also get the same effect with -Wl,-Map=output.map.
			- ```bash
			  morrism@localhost /tmp/x/x1 $ gcc -o test test.c -l myfunc -L .
			  morrism@localhost /tmp/x/x1 $ ldd test
			          linux-vdso.so.1 (0x00007ffddf30e000)
			          libmyfunc.so => not found
			          libc.so.6 => /lib64/libc.so.6 (0x00007fb183aa0000)
			          /lib64/ld-linux-x86-64.so.2 (0x00007fb183e65000)
			  morrism@localhost /tmp/x/x1 $ gcc -o test test.c -l myfunc -L . -Wl,-rpath=.
			  morrism@localhost /tmp/x/x1 $ ldd test
			          linux-vdso.so.1 (0x00007fff9bdcf000)
			          libmyfunc.so => ./libmyfunc.so (0x00007fa97b77a000)
			          libc.so.6 => /lib64/libc.so.6 (0x00007fa97b3b5000)
			          /lib64/ld-linux-x86-64.so.2 (0x00007fa97b97c000)
			  
			  ```
		- 示例
			- ```c
			  # myfunc.h文件
			  #ifndef _MYFUNC_H_
			  #define _MYFUNC_H_
			  
			  #include <stdio.h>
			  
			  void myfunc1(void);
			  void myfunc2(void);
			  
			  #endif
			  
			  # myfunc.c文件
			  #include "myfunc.h"
			  
			  void myfunc1(void)
			  {
			  	printf("this is func1\n");
			  }
			  
			  void myfunc2(void)
			  {
			  	printf("this is func2\n");
			  }
			  
			  # 生成动态库
			  gcc -fPIC -shared myfunc.c -o libmyfunc.so
			  
			  # test.c文件
			  #include "myfunc.h"
			  
			  int main(void)
			  {
			  	myfunc1();
			  	myfunc2();
			  
			  	return 0;
			  }
			  
			  # 使用动态链接库编译test.c
			  gcc -o test test.c -l myfunc -L .
			  ```
- 动态链接库命名规则
	- libname.so.x.y.z
		- lib
			- 统一前缀
		- name
			- 动态链接库名称
		- so
			- 统一后缀
		- x
			- 主版本号
		- y
			- 次版本号
		- z
			- 发型版本号
- 参考文档
	- [man ld.so](https://man7.org/linux/man-pages/man8/ld.so.8.html)