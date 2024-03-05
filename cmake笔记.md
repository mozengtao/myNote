- 只有单一源文件的CMakeLists.txt的编写
    ```bash
    ## 目录结构
    tree .
    .
    ├── CMakeLists.txt
    └── main.c

    0 directories, 2 files

    ## main.c 内容

    #include <stdio.h>

    int main(void)
    {
            printf("hello world!\n");

            return 0;
    }

    ## CMakeLists.txt 内容

    # 指定项目所需的最低 CMake 版本
    cmake_minimum_required(VERSION 3.10)

    # 定义项目的名称、版本和其他属性
    project(mytest)

    # 创建一个可执行目标
    add_executable(mytest
            ${CMAKE_CURRENT_SOURCE_DIR}/main.c

    ## 执行cmake命令生成对应的Makefile及相关文件
    cmake -S . -B build

    ## 进入build目录执行make命令生成对应的可执行文件
    # cd build/
    tree . -L 1
    .
    ├── CMakeCache.txt
    ├── CMakeFiles
    ├── cmake_install.cmake
    └── Makefile

    1 directory, 3 files

    # make
    Scanning dependencies of target mytest
    make[2]: Warning: File 'CMakeFiles/mytest.dir/depend.make' has modification time 0.55 s in the future
    [ 50%] Building C object CMakeFiles/mytest.dir/main.c.o
    [100%] Linking C executable mytest
    make[2]: warning:  Clock skew detected.  Your build may be incomplete.
    [100%] Built target mytest

    # ./mytest
    hello world!
    ```
- 编译静态库
    ```bash
    ## tree .
    .
    ├── CMakeLists.txt
    ├── include
    │   └── myadd.h
    └── src
        ├── CMakeLists.txt
        └── myadd.c

    2 directories, 4 files

    # cat CMakeLists.txt
    add_subdirectory(src)

    # cat include/myadd.h
    #ifndef _MYADD_H_
    #define _MYADD_H_

    int maydd(int, int);

    #endif

    # cat src/CMakeLists.txt
    add_library(myadd STATIC
            ${CMAKE_CURRENT_SOURCE_DIR}/myadd.c
    )

    target_include_directories(myadd PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    # cat src/myadd.c
    int myadd(int i1, int i2)
    {
            return i1 + i2;
    }

    ## cmake -S . -B buid

    ## ls
    CMakeCache.txt  CMakeFiles  cmake_install.cmake  Makefile  src

    ## tree . -L 1
    .
    ├── CMakeCache.txt
    ├── CMakeFiles
    ├── cmake_install.cmake
    ├── Makefile
    └── src

    2 directories, 3 files

    ## make
    ## ls src/
    CMakeFiles  cmake_install.cmake  libmyadd.a  Makefile
    ```