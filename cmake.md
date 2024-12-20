[**cmake-examples**](https://github.com/ttroy50/cmake-examples/tree/master)
[CppUTest manual](https://cpputest.github.io/manual.html)  

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
	- [Cmake之深入理解find_package()的用法](https://zhuanlan.zhihu.com/p/97369704)
	- [Tutorial: Easily supporting CMake install and find_package()](https://www.foonathan.net/2016/03/cmake-install/#content)
	- [cmake 使用依赖项指南](https://runebook.dev/zh/docs/cmake/guide/using-dependencies/index#guide:Using%20Dependencies%20Guide)
	- [Modern CMake](https://xiazuomo.gitbook.io/modern-cmake-chinese/introduction)
	- [**CMake-Best-Practices**](https://github.com/PacktPublishing/CMake-Best-Practices)
	- [**CMake-Cookbook**](https://github.com/PacktPublishing/CMake-Cookbook/tree/master)
	- [An Introduction to Modern CMake](https://cliutils.gitlab.io/modern-cmake/)
	- [Effective Modern CMake](https://gist.github.com/mbinna/c61dbb39bca0e4fb7d1f73b0d66a4fd1)
	- [**More Modern CMake**](https://hsf-training.github.io/hsf-training-cmake-webpage/)
	- [**CMake Reference Documentation**](https://cmake.org/cmake/help/latest/index.html)
	- [**CMake 3.26 Document 简体中文**](https://runebook.dev/zh/docs/cmake/-index-)
	- [cmake-properties(7)](https://hsf-training.github.io/hsf-training-cmake-webpage/05-variables/index.html)
	- [CMake Fundamentals](https://jeremimucha.com/category/cmake/)
	- [cmake-examples](https://github.com/ttroy50/cmake-examples)
	- [通过例子学习CMake](https://sfumecjf.github.io/cmake-examples-Chinese/)
	- [cmake-commands(7)](https://cmake.org/cmake/help/latest/manual/cmake-commands.7.html#id2)
	- [CMake-Cookbook中文版](https://github.com/xiaoweiChen/CMake-Cookbook/tree/master/content)
	- [mastering-cmake](https://lrita.github.io/images/posts/cplusplus/mastering-cmake.pdf) #pdf
	- [cmake 实践](https://cmake.readthedocs.io/en/latest/index.html#)
	- [cmake](https://cmake.org/cmake/help/latest/manual/cmake.1.html)
	- [cmake-commands](https://cmake.org/cmake/help/latest/manual/cmake-commands.7.html#id2)
	- [cmake cheet sheet](https://cppcheatsheet.com/notes/cmake_basic.html)
	- [CMake debugging](https://cliutils.gitlab.io/modern-cmake/chapters/features/debug.html)
	- [Cmake常用指令](https://github.com/Liuyvjin/notebook/blob/master/Cmake/Cmake%E5%B8%B8%E7%94%A8%E5%91%BD%E4%BB%A4.md)
	- [A typical Linux project using CMake](https://www.kaizou.org/2014/11/typical-cmake-project.html)
	- [CMakeLists.txt 注释版](https://github.com/gongluck/CVIP/blob/master/code/cmake/CMakeLists.txt)
	```bash
	#1
	include(CMakePrintHelpers)
	cmake_print_variables(MY_VARIABLE)

	#2
	cmake -S . -B build --trace-source=CMakeLists.txt
	cmake -S . -B build --trace-source=CMakeLists.txt --trace-expand

	#3
	cmake . -DCMAKE_BUILD_TYPE=Debug

	Warning:
	Never do an “in-source” build - that is, run cmake . from the source directory. It will pollute your source directory with build outputs, CMake configuration files, and will disable out-of-source builds. 
	```
	- [CMake 是怎么工作的？](https://www.tisonkun.org/2022/04/15/how-cmake-works/)
	- `add_library OBJECT`
		[对象文件的使用方法](https://wangshuyi.cn/2024/03/30/%E5%AF%B9%E8%B1%A1%E6%96%87%E4%BB%B6%E7%9A%84%E4%BD%BF%E7%94%A8%E6%96%B9%E6%B3%95/)