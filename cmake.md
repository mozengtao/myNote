[Getting Started with CMake](https://cmake.org/getting-started/)  
[cmake-commands](https://cmake.org/cmake/help/latest/manual/cmake-commands.7.html)  
[**cmake-examples**](https://github.com/ttroy50/cmake-examples/tree/master)  
[CppUTest manual](https://cpputest.github.io/manual.html)  
[CMake configure_file: Embedding JSON in C++ file at build step](https://iamsorush.com/posts/cpp-cmake-configure-file/)  
[An Introduction to Modern CMake](https://cliutils.gitlab.io/modern-cmake/README.html)  
[Effective Modern CMake](https://gist.github.com/mbinna/c61dbb39bca0e4fb7d1f73b0d66a4fd1)  
[CMake Primer](https://llvm.org/docs/CMakePrimer.html)  
[CMake-Best-Practices](https://github.com/PacktPublishing/CMake-Best-Practices/tree/main)  
[More Modern CMake](https://hsf-training.github.io/hsf-training-cmake-webpage/aio/index.html)  
[More Modern CMake](https://hsf-training.github.io/hsf-training-cmake-webpage/)  
[It's Time To Do CMake Right](https://pabloariasal.github.io/2018/02/19/its-time-to-do-cmake-right/)  
[]()  
[]()  

## CMake 核心思想
1. 配置阶段 (Configure)
   1. 解析所有 CMakeLists.txt
   2. 根据项目定义、依赖、编译选项等，生成对应平台的构建描述文件 (如 Makefile)
2. 生成阶段 (Build/Generate)
   1. 使用生成的构建文件(Makefile/Ninja)去调用底层编译器(gcc, clang, msvc)
   2. 完成编译、链接、安装等动作

```
目标导向：
现代CMake把每个编译产物(可执行文件/库)看做一个 target , 每个 target 独立管理自己的属性，并让这些目标之间形成依赖关系图 (dependency graph)

target:
可执行程序：add_executable(my_app main.cpp)
静态库：add_library(my_lib STATIC lib.cpp)
动态库：add_library(my_lib SHARED lib.cpp)
接口目标（不产生文件，仅传播属性）：add_library(my_iface INTERFACE)

target property:
源文件
编译选项
头文件路径
链接库
宏定义
构建类型（Debug/Release）

依赖传播机制(PUBLIC/PRIVATE/INTERFACE)：
关键字 PUBLIC/PRIVATE/INTERFACE 决定了依赖属性是否传播

target_link_libraries(A PUBLIC B)

| 关键字          | 意义                                       | 谁能继承属性           |
| -----------    | ----------------------------------------   | --------------------- |
| `PRIVATE`      | 仅当前 target 使用依赖项的属性               | 只有 A                |
| `PUBLIC`       | 当前 target 和其依赖的下游 target 都继承属性 | A 和依赖 A 的 target   |
| `INTERFACE`    | 当前 target 不使用，只传播属性给下游         | 依赖 A 的 target       |

示例：
libA → libB → libC

# CMakeLists:
add_library(libC ...)
target_include_directories(libC PUBLIC include)

add_library(libB ...)
target_link_libraries(libB PUBLIC libC)

add_library(libA ...)
target_link_libraries(libA PRIVATE libB)

# 传播效果
| 目标   | 继承自谁                                        | 包含路径可见性        |
| ---- | -----------------------------------------        | --------------       |
| libC | -                                                | 自己有 include 路径   |
| libB | 链接 libC (PUBLIC) → 继承 include 路径            |                      |
| libA | 链接 libB (PRIVATE) → 不继承 libC 的 include 路径 |                      |

PRIVATE → 只自己能用
PUBLIC → 自己和下游能用
INTERFACE → 自己不用，但传播给别人

# 最佳实践小结
1. 永远优先使用 target 级命令（如 target_include_directories）
2. 清晰定义依赖方向（用 PUBLIC/PRIVATE/INTERFACE）
3. 模块化管理：每个子目录一个独立 CMakeLists.txt
4. 推荐使用 out-of-source 构建
5. 使用 find_package() + target_link_libraries() 管理外部库

# find_package()
作用：找到外部库或模块，并让你可以像使用普通 target 一样使用它

主要进行以下操作：
定位库文件 (.so, .a, .dll)
定位头文件目录 (include/)
设置变量（或导入 target）

# find_package() 两种模式
1. Module 模式 (老式)

find_package(OpenSSL REQUIRED)
CMake 会在系统路径（/usr/share/cmake*/Modules/）中搜索 FindOpenSSL.cmake
执行后会设置一堆变量，例如：
OPENSSL_FOUND
OPENSSL_INCLUDE_DIR
OPENSSL_LIBRARIES
然后可以这样用:
include_directories(${OPENSSL_INCLUDE_DIR})
target_link_libraries(myapp PRIVATE ${OPENSSL_LIBRARIES})

2. Config 模式 (现代)
第三方库自己安装时会生成 <package>Config.cmake
CMake 会直接加载这个文件，并自动提供 imported targets

find_package(OpenSSL CONFIG REQUIRED)
CMake 会在系统路径或你指定路径中查找 OpenSSLConfig.cmake
之后直接用：
target_link_libraries(myapp PRIVATE OpenSSL::SSL OpenSSL::Crypto)

这些 OpenSSL::SSL / OpenSSL::Crypto 是 已导入的目标（imported targets），包含了：
include 路径
编译选项
库文件路径
平台特定配置

# CMake 如何找到包（搜索路径）
CMake 搜索顺序大致如下：
1. 环境变量：CMAKE_PREFIX_PATH, CMAKE_MODULE_PATH
2. 系统安装目录：/usr/lib/cmake/<pkg>/, /usr/share/cmake/<pkg>/
3. 用户自定义路径：cmake -D<PKG>_DIR=/path/to/config 或者 find_package(MyLib REQUIRED PATHS /opt/mylib)

# target export 与 package config 的生成
如果你想让 自己的库 也能被 find_package() 找到，就需要：
1. 导出目标
add_library(mylib STATIC mylib.cpp)
install(TARGETS mylib EXPORT mylibTargets)
2. 安装头文件与配置文件
install(FILES mylib.h DESTINATION include)
3. 生成配置文件
install(
  EXPORT mylibTargets
  FILE mylibTargets.cmake
  NAMESPACE mylib::
  DESTINATION lib/cmake/mylib
)

然后提供一个 mylibConfig.cmake 文件（可手写或用 configure_package_config_file() 生成），内容通常是：
include("${CMAKE_CURRENT_LIST_DIR}/mylibTargets.cmake")

之后别人就能这样使用你的库了：
find_package(mylib REQUIRED)
target_link_libraries(their_app PRIVATE mylib::mylib)

# imported target 的工作方式
它不参与构建（不需要源码）
但携带完整的编译属性信息
可直接用于 target_link_libraries()

add_library(OpenSSL::SSL UNKNOWN IMPORTED)
set_target_properties(OpenSSL::SSL PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "/usr/include"
  IMPORTED_LOCATION "/usr/lib/libssl.so"
)

实际上就是 find_package(OpenSSL) 自动帮你完成的工作

# 综合例子：使用外部库 + 导出自己库
myapp/
├── CMakeLists.txt
├── lib/
│   ├── CMakeLists.txt
│   └── mylib.cpp
└── main.cpp

# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(myapp LANGUAGES CXX)

find_package(OpenSSL REQUIRED)

add_subdirectory(lib)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE mylib OpenSSL::SSL)

# lib/CMakeLists.txt
add_library(mylib STATIC mylib.cpp)
target_include_directories(mylib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS mylib EXPORT mylibTargets)
install(EXPORT mylibTargets
  FILE mylibTargets.cmake
  NAMESPACE mylib::
  DESTINATION lib/cmake/mylib)

安装后别人就能：
find_package(mylib REQUIRED)
target_link_libraries(their_app PRIVATE mylib::mylib)

# 总结
| 概念               | 作用                     | 示例                               |
| ----------------  | ----------------------   | -------------------------------- |
| `find_package()`  | 查找和加载外部库           | `find_package(OpenSSL REQUIRED)` |
| Module 模式       | 依赖 `FindXXX.cmake`      | `FindOpenSSL.cmake`              |
| Config 模式       | 依赖 `<pkg>Config.cmake`  | `OpenSSLConfig.cmake`            |
| `EXPORT`          | 导出目标供他人使用         | `install(EXPORT …)`              |
| Imported target   | 外部库的 target 映射      | `OpenSSL::SSL`                   |

```

- 基本目录结构
```
project/
├── CMakeLists.txt       # 根配置文件
├── src/
│   ├── main.cpp
│   └── CMakeLists.txt   # 子目录配置
└── include/
    └── ...
```
- 配置阶段
```
cmake -S . -B build

读取源目录 (-S .) 下所有的 CMakeLists.txt
检查编译器、系统信息、依赖包
生成构建系统描述（build/Makefile、build/*.ninja 等）
(可通过 CMakeCache.txt 缓存配置参数)
```
- 构建阶段
```
cmake --build build

使用生成的 Makefile 或 Ninja 构建规则
实际调用 g++、clang++ 等编译器进行编译与链接
生成最终二进制文件（如 app、.so、.a）
```

- [CMake tips](https://github.com/sailorhero/cmake_study/blob/master/README.md)
```bash
	cmake --help-commands-list
	cmake --help-variable-list | grep CMAKE | grep HOST
	cmake --help-property-list | grep NAME
	cmake --help-property OUTPUT_NAME
	cmake --help-module FindBoost | head -40
```

## 应用实例
1. 插件系统开发(动态加载模块)
2. 自动化代码生成
3. 避免硬编码插件列表

- 示例1
- 项目结构
```
.
├── CMakeLists.txt
├── include
│   ├── my_plugins.h
└── plugins
    └── CMakeLists.txt
```
- CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.12)
project(demo-configure-file C)

# 让 plugins/CMakeLists.txt 去累积变量并生成最终头文件
add_subdirectory(plugins)
```
- plugins/CMakeLists.txt
```cmake
# 定义一个宏：向两个变量累积“声明代码”和“注册代码”
macro(REGISTER_PLUGIN name)
  string(TOUPPER ${name} NAME)

  # 也可做条件开关：option(ENABLE_${NAME} "Enable ${name}" ON)
  # if(NOT ENABLE_${NAME})
  #   return()
  # endif()

  # 1) 累积 extern 声明到 MY_PLUGINS_DECL
  set(MY_PLUGINS_DECL
      "${MY_PLUGINS_DECL}extern struct my_plugin ${name}_plugin;\n")

  # 2) 累积注册代码片段到 MY_PLUGINS_ADD
  set(CODE "  /* register ${name} */\n")
  set(CODE "${CODE}  /* e.g., register_plugin(&${name}_plugin); */\n\n")
  set(MY_PLUGINS_ADD "${MY_PLUGINS_ADD}${CODE}")
endmacro()

# 实际注册两个插件，演示效果
REGISTER_PLUGIN(plugin_a)
REGISTER_PLUGIN(plugin_b)

# 把 include/my_plugins.h.in → include/my_plugins.h
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/../include/my_plugins.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/../include/my_plugins.h"
)
```
- include/my_plugins.h
```c
#ifndef MY_PLUGINS_H
#define MY_PLUGINS_H

/* Decls: 由 CMake 填充下面这块 */
@MY_PLUGINS_DECL@

/* 一个示例的注册函数：CMake 会把每个插件的注册代码片段拼到这里 */
static inline int my_plugins_register(void) {
@MY_PLUGINS_ADD@
  return 0;
}

#endif /* MY_PLUGINS_H */
```

- 构建生成 my_plugins.h
```bash
cmake -S . -B build
```
-  my_plugins.h
```c
#ifndef MY_PLUGINS_H
#define MY_PLUGINS_H

/* Decls: 由 CMake 填充下面这块 */
extern struct my_plugin plugin_a_plugin;
extern struct my_plugin plugin_b_plugin;


/* 一个示例的注册函数：CMake 会把每个插件的注册代码片段拼到这里 */
static inline int my_plugins_register(void) {
  /* register plugin_a */
  /* e.g., register_plugin(&plugin_a_plugin); */

  /* register plugin_b */
  /* e.g., register_plugin(&plugin_b_plugin); */


  return 0;
}

#endif /* MY_PLUGINS_H */

```

- 示例2
- 项目结构
```
project/
├── CMakeLists.txt        # CMake 主文件
├── plugins/             # 插件目录
│   ├── plugin1.c        # 插件1
│   ├── plugin2.c        # 插件2
├── templates/
│   ├── plugins.h.in     # 头文件模板
└── src/
    └── main.c          # 主程序
```
- CMakeLists.txt
```
cmake_minimum_required(VERSION 3.10)
project(PluginDemo)

# 定义插件列表
set(PLUGINS 
    plugin1
    plugin2
    # 可扩展更多插件...
)

# 初始化累积变量（用于存储声明和注册代码）
set(PLUGIN_DECLARATIONS "")
set(PLUGIN_REGISTRATIONS "")

# 遍历插件，生成声明和注册代码
foreach(PLUGIN ${PLUGINS})
    # 插件初始化函数名（如 plugin1_init, plugin2_init）
    set(INIT_FUNC "${PLUGIN}_init")

    # 累积声明代码（添加到 PLUGIN_DECLARATIONS）
    string(APPEND PLUGIN_DECLARATIONS 
        "extern void ${INIT_FUNC}(void);\n"
    )

    # 累积注册代码（添加到 PLUGIN_REGISTRATIONS）
    string(APPEND PLUGIN_REGISTRATIONS 
        "    ${INIT_FUNC}();\n"
    )
endforeach()

# 配置头文件（替换模板中的变量）
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/templates/plugins.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/plugins.h"
)

# 编译主程序
add_executable(app src/main.c plugins/plugin1.c plugins/plugin2.c)

# 让主程序能找到生成的头文件
target_include_directories(app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
```
- plugins/plugin1.c
```c
#include <stdio.h>

void plugin1_init() {
    printf("Plugin 1 initialized!\n");
}
```
- plugins/plugin2.c
```c
#include <stdio.h>

void plugin2_init() {
    printf("Plugin 2 initialized!\n");
}
```
- templates/plugins.h.in
```c
// 自动生成的插件头文件 - 请勿手动修改！
#pragma once

// 插件声明部分
@PLUGIN_DECLARATIONS@

// 插件注册函数
static void register_all_plugins() {
@PLUGIN_REGISTRATIONS@
}
```
- src/main.c
```c
#include "plugins.h"  // 引用生成的头文件

int main() {
    // 注册所有插件
    register_all_plugins();
    return 0;
}
```
- 构建生成 plugins.h
```bash
mkdir build && cd build
cmake .. && make
./app
```
- plugins.h
```c
// 自动生成的插件头文件 - 请勿手动修改！
#pragma once

// 插件声明部分
extern void plugin1_init(void);
extern void plugin2_init(void);

// 插件注册函数
static void register_all_plugins() {
    plugin1_init();
    plugin2_init();
}
```
- 示例2改进
- 项目结构
```
project/
├── CMakeLists.txt        # 主 CMake 文件
├── plugins/             # 插件目录（自动扫描）
│   ├── plugin1.c        # 插件1
│   ├── plugin2.c        # 插件2
│   └── plugin3_win.c    # Windows 专用插件
├── templates/
│   └── plugins.h.in     # 跨平台头文件模板
└── src/
    └── main.c          # 主程序
```
- CMakeLists.txt
```
cmake_minimum_required(VERSION 3.10)
project(PluginDemo)

# --- 动态插件发现 ---
# 扫描 plugins/ 目录下所有 .c 文件
file(GLOB PLUGIN_SOURCES "plugins/*.c")

# 提取插件名（去掉扩展名和路径）
foreach(PLUGIN_PATH ${PLUGIN_SOURCES})
    get_filename_component(PLUGIN ${PLUGIN_PATH} NAME_WE)
    list(APPEND PLUGINS ${PLUGIN})
endforeach()

# --- 条件编译选项 ---
# 定义选项：是否启用 Windows 专用插件
option(ENABLE_WIN_PLUGIN "Enable Windows-specific plugin" OFF)

# 过滤插件（根据条件编译选项）
if(NOT ENABLE_WIN_PLUGIN)
    list(REMOVE_ITEM PLUGINS "plugin3_win")  # 如果不启用，移除 Windows 插件
endif()

# --- 累积声明和注册代码 ---
set(PLUGIN_DECLARATIONS "")
set(PLUGIN_REGISTRATIONS "")

foreach(PLUGIN ${PLUGINS})
    set(INIT_FUNC "${PLUGIN}_init")

    # 累积声明代码
    string(APPEND PLUGIN_DECLARATIONS 
        "extern void ${INIT_FUNC}(void);\n"
    )

    # 累积注册代码
    string(APPEND PLUGIN_REGISTRATIONS 
        "    ${INIT_FUNC}();\n"
    )
endforeach()

# --- 生成跨平台头文件 ---
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/templates/plugins.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/plugins.h"
)

# --- 编译主程序 ---
add_executable(app src/main.c ${PLUGIN_SOURCES})

# 包含生成的头文件目录
target_include_directories(app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# 条件编译：Windows 插件需要额外链接库
if(ENABLE_WIN_PLUGIN AND WIN32)
    target_link_libraries(app PRIVATE some_windows_lib)
endif()
```
- plugins/plugin1.c
```c
```
- plugins/plugin2.c
```c
```
- plugins/plugin3_win.c
```c
#include <stdio.h>

void plugin3_win_init() {
    #ifdef _WIN32
        printf("Windows-specific plugin loaded!\n");
    #endif
}
```
- templates/plugins.h.in
```c
// 自动生成的插件头文件 - 请勿手动修改！
#pragma once

// --- 跨平台宏定义 ---
#ifdef _WIN32
    #define PLUGIN_API __declspec(dllexport)
#else
    #define PLUGIN_API __attribute__((visibility("default")))
#endif

// --- 插件声明 ---
@PLUGIN_DECLARATIONS@

// --- 插件注册函数 ---
static void register_all_plugins() {
    // 平台特定代码
    #ifdef _WIN32
        printf("Running on Windows\n");
    #else
        printf("Running on Linux/macOS\n");
    #endif

    // 注册所有插件
@PLUGIN_REGISTRATIONS@
}
```
- src/main.c
```c
```
- 启用 Windows 插件（需在 Windows 上运行）​
```bash
cmake .. -DENABLE_WIN_PLUGIN=ON && make
./app
```

- 示例
- 目录结构
```
demo_configure/
├── CMakeLists.txt
├── config.h.in
└── main.c
```
- CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.10)
project(demo_configure)

# Variables that will be substituted
set(PROJECT_NAME "DemoApp")
set(PROJECT_VERSION "1.0.0")

# Generate config.h from template
configure_file(
    ${CMAKE_SOURCE_DIR}/config.h.in
    ${CMAKE_BINARY_DIR}/config.h
)

# Add the executable
add_executable(demo main.c)

# Make sure the generated file is available before building main.c
target_include_directories(demo PRIVATE ${CMAKE_BINARY_DIR})

```
- config.h.in
```c
/* config.h.in — template file */
#define PROJECT_NAME "@PROJECT_NAME@"
#define PROJECT_VERSION "@PROJECT_VERSION@"

```
- main.c
```c
#include <stdio.h>
#include "config.h"

int main(void) {
    printf("Project: %s, Version: %s\n", PROJECT_NAME, PROJECT_VERSION);
    return 0;
}

```
- build and run
```bash
mkdir build && cd build
cmake ..
make
./demo
```
- Edit config.h.in
```c
/* config.h.in — template file */
#define PROJECT_NAME "@PROJECT_NAME@"
#define PROJECT_VERSION "@PROJECT_VERSION@"
#define BUILD_TIME "@CMAKE_BUILD_TYPE@"

```
- change a CMake variable
```cmake
/* config.h.in — template file */
#define PROJECT_NAME "@PROJECT_NAME@"
#define PROJECT_VERSION "@PROJECT_VERSION@"
#define BUILD_TIME "@CMAKE_BUILD_TYPE@"

```
```
stat config.h
touch ../CMakeLists.txt
make
stat config.h
```
- verbose CMake to confirm regeneration
```bash
make VERBOSE=1

or

cmake --trace-expand .
```

- 示例3(增强版)
- Directory layout
```
multi_configure/
├── CMakeLists.txt
├── version.h.in
├── config.json.in
└── main.c
```

- Step 1 – Template 1: version.h.in
```c
/* version.h.in — auto-generated at configure time */
#define PROJECT_NAME "@PROJECT_NAME@"
#define PROJECT_VERSION "@PROJECT_VERSION@"
#define GIT_COMMIT_HASH "@GIT_COMMIT_HASH@"
#define BUILD_TYPE "@CMAKE_BUILD_TYPE@"
```

- Step 2 – Template 2: config.json.in
```json
{
  "project": "@PROJECT_NAME@",
  "version": "@PROJECT_VERSION@",
  "commit": "@GIT_COMMIT_HASH@",
  "build_type": "@CMAKE_BUILD_TYPE@"
}
```

- Step 3 – CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.10)
project(multi_configure)

# ---- variables used in templates ----
set(PROJECT_NAME "DemoMulti")
set(PROJECT_VERSION "3.4.1")

# Get current git commit hash (optional)
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# ---- generate multiple files ----
configure_file(${CMAKE_SOURCE_DIR}/version.h.in ${CMAKE_BINARY_DIR}/version.h @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/config.json.in ${CMAKE_BINARY_DIR}/config.json @ONLY NEWLINE_STYLE UNIX)

# ---- build target ----
add_executable(demo main.c)
target_include_directories(demo PRIVATE ${CMAKE_BINARY_DIR})

# ---- install both generated files (optional) ----
install(FILES
    ${CMAKE_BINARY_DIR}/version.h
    ${CMAKE_BINARY_DIR}/config.json
    DESTINATION share/${PROJECT_NAME}
)
```

- Step 4 – main.c
```c
#include <stdio.h>
#include "version.h"

int main(void) {
    printf("=== Build Info ===\n");
    printf("Project: %s\n", PROJECT_NAME);
    printf("Version: %s\n", PROJECT_VERSION);
    printf("Commit: %s\n", GIT_COMMIT_HASH);
    printf("Build type: %s\n", BUILD_TYPE);
    return 0;
}
```

- Step 5 – Build and inspect
```bash
mkdir build && cd build
cmake ..
make
```



## 参考文档
[Cmake之深入理解find_package()的用法](https://zhuanlan.zhihu.com/p/97369704)
[Tutorial: Easily supporting CMake install and find_package()](https://www.foonathan.net/2016/03/cmake-install/#content)
[cmake 使用依赖项指南](https://runebook.dev/zh/docs/cmake/guide/using-dependencies/index#guide:Using%20Dependencies%20Guide)
[Modern CMake](https://xiazuomo.gitbook.io/modern-cmake-chinese/introduction)
[**CMake-Best-Practices**](https://github.com/PacktPublishing/CMake-Best-Practices)
[**CMake-Cookbook**](https://github.com/PacktPublishing/CMake-Cookbook/tree/master)
[An Introduction to Modern CMake](https://cliutils.gitlab.io/modern-cmake/)
[Effective Modern CMake](https://gist.github.com/mbinna/c61dbb39bca0e4fb7d1f73b0d66a4fd1)
[**More Modern CMake**](https://hsf-training.github.io/hsf-training-cmake-webpage/)
[**CMake Reference Documentation**](https://cmake.org/cmake/help/latest/index.html)
[**CMake 3.26 Document 简体中文**](https://runebook.dev/zh/docs/cmake/-index-)
[cmake-properties(7)](https://hsf-training.github.io/hsf-training-cmake-webpage/05-variables/index.html)
[CMake Fundamentals](https://jeremimucha.com/category/cmake/)
[cmake-examples](https://github.com/ttroy50/cmake-examples)
[通过例子学习CMake](https://sfumecjf.github.io/cmake-examples-Chinese/)
[cmake-commands(7)](https://cmake.org/cmake/help/latest/manual/cmake-commands.7.html#id2)
[CMake-Cookbook中文版](https://github.com/xiaoweiChen/CMake-Cookbook/tree/master/content)
[mastering-cmake](https://lrita.github.io/images/posts/cplusplus/mastering-cmake.pdf) #pdf
[cmake 实践](https://cmake.readthedocs.io/en/latest/index.html#)
[cmake](https://cmake.org/cmake/help/latest/manual/cmake.1.html)
[cmake-commands](https://cmake.org/cmake/help/latest/manual/cmake-commands.7.html#id2)
[cmake cheet sheet](https://cppcheatsheet.com/notes/cmake_basic.html)
[CMake debugging](https://cliutils.gitlab.io/modern-cmake/chapters/features/debug.html)
[Cmake常用指令](https://github.com/Liuyvjin/notebook/blob/master/Cmake/Cmake%E5%B8%B8%E7%94%A8%E5%91%BD%E4%BB%A4.md)
[A typical Linux project using CMake](https://www.kaizou.org/2014/11/typical-cmake-project.html)
[CMakeLists.txt 注释版](https://github.com/gongluck/CVIP/blob/master/code/cmake/CMakeLists.txt)
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
[CMake 是怎么工作的？](https://www.tisonkun.org/2022/04/15/how-cmake-works/)
`add_library OBJECT`
[对象文件的使用方法](https://wangshuyi.cn/2024/03/30/%E5%AF%B9%E8%B1%A1%E6%96%87%E4%BB%B6%E7%9A%84%E4%BD%BF%E7%94%A8%E6%96%B9%E6%B3%95/)