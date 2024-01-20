- [CMake tips](https://github.com/sailorhero/cmake_study/blob/master/README.md)
	- ```bash
	  cmake --help-commands-list
	  cmake --help-variable-list | grep CMAKE | grep HOST
	  cmake --help-property-list | grep NAME
	  cmake --help-property OUTPUT_NAME
	  cmake --help-module FindBoost | head -40
	  ```
- cmake的特点
	- 开放源代码，BSD许可
	- 跨平台，可生成native编译配置文件
	- 简化编译构建过程和编译过程，cmake的工具链：cmake + make
	- 可扩展，可以为cmake编写特定功能的模块
- 示例
	- 示例1
		- ```bash
		  morrism@PC2886:~/t1$ ls
		  CMakeLists.txt  main.c
		  
		  morrism@PC2886:~/t1$ cat main.c
		  #include <stdio.h>
		  
		  int main(void)
		  {
		          printf("hello world!\n");
		          return 0;
		  }
		  
		  morrism@PC2886:~/t1$ cat CMakeLists.txt
		  PROJECT(HELLO)
		  SET(SRC_LIST main.c)
		  MESSAGE(STATUS "This is BINARY dir" ${HELLO_BINARY_DIR})
		  MESSAGE(STATUS "This is SOURCE dir" ${HELLO_SOURCE_DIR})
		  ADD_EXECUTABLE(hello ${SRC_LIST})
		  
		  
		  in-source build(内部构建)：
		  morrism@PC2886:~/t1$ cmake .
		  morrism@PC2886:~/t1$ ls
		  CMakeCache.txt  CMakeFiles  cmake_install.cmake  CMakeLists.txt  hello  main.c  Makefile
		  
		  out-of-source build(外部构建)：
		  morrism@PC2886:~/t1$ ls
		  CMakeLists.txt  main.c
		  morrism@PC2886:~/t1$ mkdir build && cd build
		  morrism@PC2886:~/t1/build$ cmake ..
		  morrism@PC2886:~/t1/build$ ls
		  CMakeCache.txt  CMakeFiles  cmake_install.cmake  Makefile
		  morrism@PC2886:~/t1/build$ ls ../
		  build  CMakeLists.txt  main.c
		  ```
	- 改进的示例1
	  collapsed:: true
		-
- 参考文档
	- [CMake Reference Documentation](https://cmake.org/cmake/help/latest/index.html)
	- [cmake-examples](https://github.com/ttroy50/cmake-examples)
	- [通过例子学习CMake](https://sfumecjf.github.io/cmake-examples-Chinese/)
	- [cmake-commands(7)](https://cmake.org/cmake/help/latest/manual/cmake-commands.7.html#id2)
	- [CMake-Cookbook中文版](https://github.com/xiaoweiChen/CMake-Cookbook/tree/master/content)
	- [mastering-cmake](https://lrita.github.io/images/posts/cplusplus/mastering-cmake.pdf) #pdf
	- [cmake 实践](https://cmake.readthedocs.io/en/latest/index.html#)
	- [cmake](https://cmake.org/cmake/help/latest/manual/cmake.1.html)
	- [cmake-commands](https://cmake.org/cmake/help/latest/manual/cmake-commands.7.html#id2)
	- [cmake cheet sheet](https://cppcheatsheet.com/notes/cmake_basic.html)