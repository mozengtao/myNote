## 动态链接 和 静态链接
[**Program Library HOWTO**](https://tldp.org/HOWTO/Program-Library-HOWTO/index.html)  
[C++ dlopen mini HOWTO](https://tldp.org/HOWTO/C++-dlopen/)  
[Shared libraries with GCC on Linux](https://www.cprogramming.com/tutorial/shared-libraries-linux-gcc.html)  
[]()  
[]()  
[How dynamic linking for modular libraries works on Linux](https://opensource.com/article/22/5/dynamic-linking-modular-libraries-linux)  
[How to handle dynamic and static libraries in Linux](https://opensource.com/article/20/6/linux-libraries)  
```c
When we link an application against a shared library, the linker leaves some stubs (unresolved symbols) which need to be filled by dynamic linker at run time or at application loading time.

Loading of a shared library is of two types::
1. Dynamically linked libraries
2. Dynamically loaded libraries

// Dynamically linked libraries
gcc -c -Wall -Werror -fpic foo.c		// Compiling with Position Independent Code
gcc -shared -o libfoo.so foo.o			// Creating a shared library from an object file
gcc -L/home/username/foo -Wall -o test main.c -lfoo				// Linking with a shared library

// 1 use LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/home/username/foo:$LD_LIBRARY_PATH		// Making the library available at runtime
./test

// 2 use rpath
unset LD_LIBRARY_PATH
gcc -L/home/username/foo -Wl,-rpath=/home/username/foo -Wall -o test main.c -lfoo
./test

//  Dynamically loaded libraries
Useful for creating a "plug-in" architecture. 
The program takes full control by calling functions with the library. This is done using dlopen(), dlsym(), dlclose().


// 动态链接
// Locating a shared object during compilation
gcc -I ./include -c src/demo.c				// -I option: adds a directory to GCC's search path for header files

gcc -L`pwd`/lib -o myDemo demo.o -lexample	// -L option: adds an additional library path to GCC's search locations. 
											// -l: sets the name of the library you want to link against

ldd ./myDemo								// ldd: prints shared object dependencies
        linux-vdso.so.1 (0x00007ffe151df000)
        libexample.so => not found
        libc.so.6 => /lib64/libc.so.6 (0x00007f514b60a000)
        /lib64/ld-linux-x86-64.so.2 (0x00007f514b839000)

LD_LIBRARY_PATH=`pwd`/lib ldd ./			// LD_LIBRARY_PATH: the environment variable which defines the path to libraries
   linux-vdso.so.1 (0x00007ffe515bb000)
   libexample.so => /tmp/Demo/lib/libexample.so (0x0000...
   libc.so.6 => /lib64/libc.so.6 (0x00007eff037ee000)
   /lib64/ld-linux-x86-64.so.2 (0x00007eff03a22000)

LD_LIBRARY_PATH=`pwd`/lib ./myDemo			// 

// When to use LD_LIBRARY_PATH
/*
	1. compiling software that needs to link against a library that itself has just been compiled and has not yet been installed
	2. bundling software that's designed to run out of a single directory, with no install script or an install script that places libraries in non-standard directories
*/

// 静态链接
A static library is an archive (ar) of object files

file libmy_static.a		// libmy_static.a: current ar archive

ar -t libmy_static.a 	// look into the archive

ar -x libmy_static.a	// extract the archive's files
```