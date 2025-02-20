- [[gcc]]
- ```bash
  # .h头文件定义接口，.c源文件实现头文件定义的接口
  # 头文件位于系统目录（工具链对应目录）或者用户指定目录（通过 -I 选项指定）
  # 库文件和头文件类似，也位于系统目录或者用户指定的目录
  #include <stdio.h>
  
  int main(int argc, char *argv[])
  {
  	if(argc >= 2)
      	printf("hello %s!\n", argv[1]);
      else
      	printf("hello world!\n");
      
      return 0;
  }
  
  # 查看详细编译过程
  gcc -o hello hello.c -v
  整个编译过程：
  	预处理		gcc -E -o hello.i hello.c
  	编译		 gcc -S -o hello.s hello.i
  	汇编		 gcc -c -o hello.o hello.s
  	链接		 gcc -o hello.o hello.c	//完整的过程
  通常所说的编译可能指的是预处理+编译+汇编，或者指的是预处理+编译+汇编+链接
  
  gcc -o main main.c myfunc.c:
  	1. 编译生成main.o(预处理+编译+汇编)
      2. 编译生成myfun.o(预处理+编译+汇编)
      3. 链接main.o，myfun.o以及其他需要的的库文件生成最终的可执行文件main
  
  
  gcc -o hello hello.c
  file hello
  交叉编译
  arm-xxx-gcc -o hello hello.c
  file hello
  
  
  gcc -o main f1.c f2.c
  当源文件个数较多时，通常使用如下命令进行编译
  gcc -c -o f1.o f1.c
  gcc -c -o f2.o f2.c
  gcc -o main f1.o f2.o
  这样的好处是，当其中一个或者若干个源文件发生变化时，只需要编译发生变化的源文件之后和其他的.o文件进行链接
  
  
  有用的选项
  gcc -E main.c	// 查看预处理结果
  gcc -E -dM main.c	// 把所有的宏展开
  gcc -Wp,-MD,abc.dep -c -o main.o main.c	// 生成依赖文件abc.dep，Makefile会用到
  echo 'main() {}' | gcc -E -v -	// 列出头文件目录，库文件目录
  
  gcc -M c.c	// 打印依赖
  gcc -M -MF c.d c.c // 保存依赖到文件c.d
  gcc -c -o c.o c.c -MD -MF c.d	// 编译c.o，同时保存依赖到c.d
  ```
- ```bash
  -c  Compile or assemble the source files, but do not link.
  -S  Stop after the stage of compilation proper; do not assemble.
  -E  Stop after the preprocessing stage; do not run the compiler proper.
  -v  Print (on standard error output) the commands executed to run the stages of compilation.
  -o file
  	Place output in file file.
  -###
      Like -v except the commands are not executed and arguments are quoted unless they contain 
      only alphanumeric characters or "./-_".  This is useful for shell scripts to capture
      the driver-generated command lines.
  -I dir
  	Add the directory dir to the list of directories to be searched for header files during preprocessing.
  -Ldir
  	Add directory dir to the list of directories to be searched for -l.	// 指定链接时而非运行时的库文件路径
  -l library
  	Search the library named library when linking.  // 指定链接时的库文件名称
      
  LIBRARY_PATH (affects gcc during linking)
  	In addition to the standard default directories like /usr/lib or /usr/local/lib, 
      the GNU linker, ld, also uses the directories in LIBRARY_PATH while searching for 
      libraries in the linking phase.
  
  LD_LIBRARY_PATH (used at runtime)
  	In addition to a set of standard directories, the dynamic linker, ld.so, also uses 
  	LD_LIBRARY_PATH for finding shared libraries before loading them to the address space 
  	of the process.
  ```
- ```bash
  morrism@localhost /tmp/x $ gcc -o t1 t1.c myadd.c -###
  Using built-in specs.
  COLLECT_GCC=/usr/bin/gcc
  COLLECT_LTO_WRAPPER=/usr/libexec/gcc/x86_64-redhat-linux/8/lto-wrapper
  OFFLOAD_TARGET_NAMES=nvptx-none
  OFFLOAD_TARGET_DEFAULT=1
  Target: x86_64-redhat-linux
  Configured with: ../configure --enable-bootstrap --enable-languages=c,c++,fortran,lto --prefix=/usr --mandir=/usr/share/man --infodir=/usr/share/info --with-bugurl=http://bugzilla.redhat.com/bugzilla --enable-shared --enable-threads=posix --enable-checking=release --enable-multilib --with-system-zlib --enable-__cxa_atexit --disable-libunwind-exceptions --enable-gnu-unique-object --enable-linker-build-id --with-gcc-major-version-only --with-linker-hash-style=gnu --enable-plugin --enable-initfini-array --with-isl --disable-libmpx --enable-offload-targets=nvptx-none
  --without-cuda-driver --enable-gnu-indirect-function --enable-cet --with-tune=generic --with-arch_32=x86-64 --build=x86_64-redhat-linux
  Thread model: posix
  gcc version 8.4.1 20200928 (Red Hat 8.4.1-1) (GCC)
  ### 1
  COLLECT_GCC_OPTIONS='-o' 't1' '-mtune=generic' '-march=x86-64'
   /usr/libexec/gcc/x86_64-redhat-linux/8/cc1 -quiet t1.c -quiet -dumpbase t1.c "-mtune=generic" "-march=x86-64" -auxbase t1 -o /tmp/cc4iLGe6.s
  COLLECT_GCC_OPTIONS='-o' 't1' '-mtune=generic' '-march=x86-64'
   as --64 -o /tmp/ccxoKYq3.o /tmp/cc4iLGe6.s
  
  ### 2
  COLLECT_GCC_OPTIONS='-o' 't1' '-mtune=generic' '-march=x86-64'
   /usr/libexec/gcc/x86_64-redhat-linux/8/cc1 -quiet myadd.c -quiet -dumpbase myadd.c "-mtune=generic" "-march=x86-64" -auxbase myadd -o /tmp/cc4iLGe6.s
  COLLECT_GCC_OPTIONS='-o' 't1' '-mtune=generic' '-march=x86-64'
   as --64 -o /tmp/cc6QFLR2.o /tmp/cc4iLGe6.s
  COMPILER_PATH=/usr/libexec/gcc/x86_64-redhat-linux/8/:/usr/libexec/gcc/x86_64-redhat-linux/8/:/usr/libexec/gcc/x86_64-redhat-linux/:/usr/lib/gcc/x86_64-redhat-linux/8/:/usr/lib/gcc/x86_64-redhat-linux/
  LIBRARY_PATH=/usr/lib/gcc/x86_64-redhat-linux/8/:/usr/lib/gcc/x86_64-redhat-linux/8/../../../../lib64/:/lib/../lib64/:/usr/lib/../lib64/:/usr/lib/gcc/x86_64-redhat-linux/8/../../../:/lib/:/usr/lib/
  COLLECT_GCC_OPTIONS='-o' 't1' '-mtune=generic' '-march=x86-64'
  
  ### 3
  /usr/libexec/gcc/x86_64-redhat-linux/8/collect2 -plugin /usr/libexec/gcc/x86_64-redhat-linux/8/liblto_plugin.so "-plugin-opt=/usr/libexec/gcc/x86_64-redhat-linux/8/lto-wrapper" "-plugin-opt=-fresolution=/tmp/cch5UBQ2.res" "-plugin-opt=-pass-through=-lgcc" "-plugin-opt=-pass-through=-lgcc_s" "-plugin-opt=-pass-through=-lc" "-plugin-opt=-pass-through=-lgcc" "-plugin-opt=-pass-through=-lgcc_s" --build-id --no-add-needed --eh-frame-hdr "--hash-style=gnu" -m elf_x86_64 -dynamic-linker /lib64/ld-linux-x86-64.so.2 -o t1 /usr/lib/gcc/x86_64-redhat-linux/8/../../../../lib64/crt1.o /usr/lib/gcc/x86_64-redhat-linux/8/../../../../lib64/crti.o /usr/lib/gcc/x86_64-redhat-linux/8/crtbegin.o -L/usr/lib/gcc/x86_64-redhat-linux/8 -L/usr/lib/gcc/x86_64-redhat-linux/8/../../../../lib64 -L/lib/../lib64 -L/usr/lib/../lib64 -L/usr/lib/gcc/x86_64-redhat-linux/8/../../.. /tmp/ccxoKYq3.o /tmp/cc6QFLR2.o -lgcc --as-needed -lgcc_s --no-as-needed -lc -lgcc --as-needed -lgcc_s --no-as-needed /usr/lib/gcc/x86_64-redhat-linux/8/crtend.o /usr/lib/gcc/x86_64-redhat-linux/8/../../../../lib64/crtn.o
  COLLECT_GCC_OPTIONS='-o' 't1' '-mtune=generic' '-march=x86-64'
  
  ```
- ```bash
  mount -t nfs -o nolock,vers=3 192.168.1.137:/home/root/ndf_rootfs /mnt
  
  cat /prc/mounts
  同一个设备可以挂载在多个目录下
  mount -t jffs2 /dev/mtdblock7 /mnt/
  
  mount -t sysfs none /mnt
  ```
- ```bash
  交叉编译
  ./configure --host=arm-buildroot-linux-gnueabihf --prefix=$PWD/tmp
  make
  make install
  
  --host=HOST      		cross-compile to build programs to run on HOST [BUILD]
  --prefix=PREFIX         install architecture-independent files in PREFIX
                          [/usr/local]
  ```
- 参考链接
	- [LIBRARY_PATH vs LD_LIBRARY_PATH](https://www.baeldung.com/linux/library_path-vs-ld_library_path)
	- [**libpng**](http://www.libpng.org/pub/png/libpng.html)
	- [FreeType](https://freetype.org/)