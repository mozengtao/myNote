[cmake-commands](https://cmake.org/cmake/help/latest/manual/cmake-commands.7.html)  
[**cmake-examples**](https://github.com/ttroy50/cmake-examples/tree/master)  
[CppUTest manual](https://cpputest.github.io/manual.html)  
[CMake configure_file: Embedding JSON in C++ file at build step](https://iamsorush.com/posts/cpp-cmake-configure-file/)  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  

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