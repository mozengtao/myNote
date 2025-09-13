[GNU Automake](https://www.gnu.org/software/automake/manual/html_node/index.html)  
[GNU Automake](https://www.gnu.org/software/automake/manual/automake.html)  
[GNU make](https://www.gnu.org/software/make/manual/make.html)  
[man 1 make](https://linux.die.net/man/1/make)  
[GNU Autoconf](https://www.gnu.org/savannah-checkouts/gnu/autoconf/manual/autoconf-2.72/index.html)  
[Using Autotools to Configure, Make, and Install a Program](https://earthly.dev/blog/autoconf/)  
[]()  
[]()  


```bash
# Using Autotools to Configure, Make, and Install a Program
hello$ ls
configure.ac  hello.c  Makefile.am

hello.c:
#include <stdio.h>
int main(void) {
    puts("Hello, world!");
    return 0;
}

configure.ac:	# describes what your project needs and how to generate configure
AC_INIT([hello], [0.1], [maintainer@example.com])
AM_INIT_AUTOMAKE
AC_PROG_CC
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

Makefile.am:	# says what to build
bin_PROGRAMS = hello
hello_SOURCES = hello.c

clean-local:
        @rm config.status configure config.log
        @rm Makefile
        @rm -r autom4te.cache/
        @rm aclocal.m4
        @rm  compile install-sh missing Makefile.in

hello$ aclocal		# gathers macro definitions
hello$ autoconf		# generates ./configure from configure.ac
hello$ automake		# generates Makefile.in from Makefile.am
hello$ automake --add-missing
hello$ ./configure	# 1.Checks your system (compiler, libraries, etc.) 2.Substitutes results into the Makefile.in templates 3.Produces real Makefile files that are tailored to your machine
hello$ make
hello$ ./hello
Hello, world!

hello$ make dist
hello$ ls hello-0.1.tar.gz
hello-0.1.tar.gz

hello$ mkdir test
hello$ mv hello-0.1.tar.gz test
hello$ cd test/
hello/test$ tar zxvf hello-0.1.tar.gz
hello/test$ cd hello-0.1/
hello/test/hello-0.1$ ./configure
hello/test/hello-0.1$ make
hello/test/hello-0.1$ ./hello
Hello, world!
```

## dry run
```bash
# make -n 主要应用场景
1. Debug Makefile: 验证 Makefile的执行流程，确保实际执行时符合预期（依赖正确，不会覆写文件等）
2. Understanding the Build Process：用来学习和理解新项目的编译过程
3. Testing Changes：在实际运行前，测试新的Makefile修改符合预期

make -nB
	-n, --just-print, --dry-run, --recon
		Print the commands that would be executed, but do not execute them (except in certain circumstances).

	-B, --always-make
		Unconditionally make all targets.
```

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
	- [Variables Used by Implicit Rules](https://www.gnu.org/software/make/manual/html_node/Implicit-Variables.html)
	```bash
		LDFLAGS
			Extra flags to give to compilers when they are supposed to invoke the linker, ‘ld’, such as -L. Libraries (-lfoo) should be added to the LDLIBS variable instead.

		LDLIBS
			Library flags or names given to compilers when they are supposed to invoke the linker, ‘ld’. LOADLIBES is a deprecated (but still supported) alternative to LDLIBS. Non-library linker flags, such as -L, should go in the LDFLAGS variable.
	```
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