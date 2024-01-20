- ```bash
  qstrip=$(strip $(subst ",,$(1)))
  
  # make
         -B, --always-make
              Unconditionally make all targets.
         -d   Print  debugging information in addition to normal processing.
         -p, --print-data-base
              Print the data base (rules and variable values) that results from reading the makefiles; then execute as usual or as otherwise specified.  This also prints the version information given  by  the  -v  switch
              (see below).  To print the data base without trying to remake any files, use make -p -f/dev/null.
  
  ```
- makefile
	- 定义"自动化编译"的规则，即定义了如何去编译和链接程序的规则
- make
	- 执行makefile所定义的规则和命令
- 程序的编译和链接
	- 源文件编译生成中间目标文件，多个中间目标文件链接生成执行文件
- Makefile的规则
	- ```bash
	  target ... : prerequisites ...
	  	command
	      ...
	      ...
	  
	  target: 一个或者目标文件，可以是object file，可以是执行文件，还可以是一个标签（label）
	  prerequisites: 生成target所依赖的文件
	  command: 定义了生成target的规则
	  
	  target 这一个或多个的目标文件依赖于prerequisites 中的文件，其生成规则定义在 command 中，
	  prerequisites 中如果有一个以上的文件比 target 文件要新的话，command 所定义的命令就会被执行
	  ```
	- ```bash
	  ./configure --build=xxx  --prefix=${PWD}/tmp
	  make CC=xxx
	  make install
	  
	  # configure, make, make install
	  ./configure
	  	get ready to build the software on your specific system
	      convert Makefile.in template --> Makefile
	      configure and Makefile.in come from autotools, autotools is a suite of programs which 
	      includes autoconf, automake, and many other programs, all of which work together to 
	      make the life of a software maintainer significantly easier
	  make
	  	build the software
	  make install
	  	copy the built program, and its libraries and documentation, to the correct locations
	  
	  Example:
	  main.c:
	  #include <stdio.h>
	  
	  int
	  main(int argc, char* argv[])
	  {
	      printf("Hello world\n");
	      return 0;
	  }
	  
	  #1 Creating the configure script
	  configure.ac file written in m4sh—a combination of m4 macros and POSIX shell script—to 
	  describe what the configure script needs to do, to convert configure.ac --> configure
	  configure.ac:
	  AC_INIT([helloworld], [0.1], [george@thoughtbot.com])
	  AM_INIT_AUTOMAKE
	  AC_PROG_CC
	  AC_CONFIG_FILES([Makefile])
	  AC_OUTPUT
	  
	  #2 Creating the Makefile
	  automake use Makefile.am to generated the Makefile.in for us
	  Makefile.am:
	  AUTOMAKE_OPTIONS = foreign
	  bin_PROGRAMS = helloworld
	  helloworld_SOURCES = main.c
	  
	  #3 Putting it all together
	  On the maintainer’s system:
	  aclocal # Set up an m4 environment
	  autoconf # Generate configure from configure.ac
	  automake --add-missing # Generate Makefile.in from Makefile.am
	  ./configure # Generate Makefile from Makefile.in
	  make distcheck # Use Makefile to build and test a tarball to distribute
	  
	  On the end-user’s system:
	  ./configure # Generate Makefile from Makefile.in
	  make # Use Makefile to build the program
	  make install # Use Makefile to install the program
	  ```
- 参考文档
	- [The magic behind configure, make, make install - How it works in Unix](https://thoughtbot.com/blog/the-magic-behind-configure-make-make-install)
	- [Learn Makefiles With the tastiest examples](https://makefiletutorial.com/#top)
	- [跟我一起写makefile](https://awesome-programming-books.github.io/linux/%E8%B7%9F%E6%88%91%E4%B8%80%E8%B5%B7%E5%86%99makefile.pdf) #pdf
	- [GNU make](https://www.gnu.org/software/make/manual/make.html)
	- [Linux Kernel Makefiles](https://docs.kernel.org/kbuild/makefiles.html)
	- [C Makefile cheatsheet](https://cppcheatsheet.com/notes/c_make.html)
	- [Makefile-Templates](https://github.com/TheNetAdmin/Makefile-Templates)
	- [Makefile example(librdkafka)](https://github.com/confluentinc/librdkafka/blob/master/Makefile)
	- [Difference Between Makefile.am and Makefile.in](https://www.baeldung.com/linux/makefile-am-vs-in)
	- [Using Autotools to Configure, Make, and Install a Program](https://gist.github.com/pksunkara/988716)
	- [[Autoconf]]