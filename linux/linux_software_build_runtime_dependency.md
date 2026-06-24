# Linux 软件编译过程：运行时依赖（Runtime Dependency）与编译时依赖（Build Dependency）完整心智模型

---

# 一、先记住一句话

> **编译时依赖决定“程序如何被制造出来”，运行时依赖决定“程序如何被启动起来”。**

可以把软件开发理解成“造车”：

```text
编译阶段（Build Time）                    运行阶段（Run Time）

设计图纸、零件规格                        汽油、道路
工具、模具                               驾驶员
焊接设备                                 发动机

      ↓                                      ↓

  把汽车制造出来                      让汽车真正跑起来
```

---

# 二、软件从源码到运行的完整生命周期

```text
                ┌────────────────┐
                │  源代码 (*.c)   │
                └────────┬───────┘
                         │
                         │ 需要：
                         │ gcc
                         │ *.h
                         │ libxxx.so
                         │ pkg-config
                         │
                         ▼
                ┌────────────────┐
                │    编译阶段     │
                │     gcc        │
                └────────┬───────┘
                         │
                         │ 产生
                         ▼
                ┌────────────────┐
                │  可执行文件     │
                │     tig        │
                └────────┬───────┘
                         │
                         │ 需要：
                         │ ld-linux
                         │ libxxx.so
                         │
                         ▼
                ┌────────────────┐
                │    运行阶段     │
                │    ./tig       │
                └────────────────┘
```

---

# 三、编译时依赖（Build Dependency）

编译时依赖：

```text
用于“生产软件”
```

主要包括：

```text
gcc
g++
make
cmake
autoconf
automake
pkg-config

头文件 (*.h)

开发库：
libxxx-dev

静态库：
libxxx-staticdev
```

---

# 四、运行时依赖（Runtime Dependency）

运行时依赖：

```text
用于“运行软件”
```

主要包括：

```text
动态链接器

ld-linux

共享库

libxxx.so
```

---

# 五、一个完整示例：编译 tig

源码：

```text
tig
```

依赖：

```text
ncurses
readline
pcre
```

---

## 1）安装编译依赖

```bash
sudo dnf install -y \
    gcc \
    make \
    ncurses-dev \
    libreadline-dev \
    libpcre-dev
```

这些属于：

```text
Build Dependency
```

---

## 2）configure 检测环境

执行：

```bash
./configure
```

实际上在做：

```text
检查：

gcc 是否存在

ncurses.h 是否存在

readline.h 是否存在

pcre.h 是否存在

libncurses.so 是否存在

libreadline.so 是否存在

libpcre.so 是否存在
```

ASCII Diagram：

```text
configure

     │
     ├── gcc ?
     │
     ├── ncurses.h ?
     │
     ├── readline.h ?
     │
     ├── pcre.h ?
     │
     ├── -lncurses ?
     │
     ├── -lreadline ?
     │
     └── -lpcre ?

             ↓

         Makefile
```

---

# 六、编译过程到底发生了什么？

假设：

```c
#include <stdio.h>
#include <readline/readline.h>

int main() {
    char *s = readline("> ");
    printf("%s\n", s);
}
```

保存：

```text
main.c
```

编译：

```bash
gcc main.c -lreadline -o demo
```

内部过程：

---

## 阶段 1：预处理（Preprocessing）

执行：

```text
#include 展开
宏替换
条件编译
```

输入：

```c
#include <readline/readline.h>
```

输出：

```text
真正展开后的源码
```

需要：

```text
readline.h
```

来源：

```text
libreadline-dev
```

ASCII Diagram：

```text
main.c

    ↓

cpp

    ↓

main.i
```

---

## 阶段 2：编译（Compilation）

执行：

```text
C

↓

汇编
```

输出：

```text
main.s
```

ASCII Diagram：

```text
main.i

    ↓

gcc

    ↓

main.s
```

---

## 阶段 3：汇编（Assembly）

执行：

```text
汇编代码

↓

机器码
```

输出：

```text
main.o
```

ASCII Diagram：

```text
main.s

    ↓

as

    ↓

main.o
```

---

## 阶段 4：链接（Linking）

执行：

```text
main.o

+

libreadline.so
```

生成：

```text
demo
```

ASCII Diagram：

```text
main.o

    │
    │
    ├──── libreadline.so
    │
    └──── libc.so

            ↓

          ld

            ↓

          demo
```

---

# 七、为什么编译时需要 .so？

很多人误以为：

```text
只有运行才需要 .so
```

实际上：

```text
链接阶段也需要 .so
```

例如：

```bash
gcc main.c -lreadline
```

需要：

```text
libreadline.so
```

用于：

```text
确定 readline() 在哪里
```

---

# 八、运行时发生什么？

执行：

```bash
./demo
```

并不是：

```text
demo 直接运行
```

实际上：

```text
内核
 ↓
ld-linux
 ↓
libreadline.so
 ↓
libc.so
 ↓
demo
```

ASCII Diagram：

```text
./demo

    ↓

Kernel

    ↓

ELF Loader

    ↓

ld-linux

    ↓

加载：

    libc.so
    libreadline.so

    ↓

main()
```

---

# 九、运行时依赖从哪里来？

查看：

```bash
ldd demo
```

结果：

```text
libreadline.so.8
libc.so.6
libtinfo.so.6
```

这些就是：

```text
Runtime Dependencies
```

---

# 十、编译时依赖从哪里来？

查看：

```bash
rpm -ql libreadline-dev
```

可能看到：

```text
/usr/include/readline/readline.h

/usr/lib64/libreadline.so

/usr/lib64/pkgconfig/readline.pc
```

这些就是：

```text
Build Dependencies
```

---

# 十一、pkg-config 的作用

很多软件：

```bash
pkg-config --cflags readline
```

输出：

```text
-I/usr/include
```

获取：

```text
头文件位置
```

---

执行：

```bash
pkg-config --libs readline
```

输出：

```text
-lreadline
```

获取：

```text
链接参数
```

---

# 十二、动态链接与静态链接

动态链接：

```text
编译时：

需要：
    libreadline.so

运行时：

仍然需要：
    libreadline.so
```

ASCII Diagram：

```text
demo

    ↓

libreadline.so

    ↓

系统提供
```

---

静态链接：

```text
编译时：

需要：
    libreadline.a
```

运行时：

```text
不需要 readline
```

ASCII Diagram：

```text
demo

    ↓

已经包含 readline 代码
```

---

# 十三、依赖关系总结

动态链接：

```text
Build Time

gcc
make
libreadline-dev
    ↓
demo
    ↓
Runtime

libreadline.so
libc.so
```

---

静态链接：

```text
Build Time

gcc
make
libreadline-staticdev
    ↓
demo
    ↓
Runtime

几乎无额外依赖
```

---

# 十四、如何判断缺少哪类依赖？

缺少编译时依赖：

报错：

```text
fatal error:
xxx.h:
No such file or directory
```

说明：

```text
缺少 xxx-dev
```

---

报错：

```text
cannot find -lxxx
```

说明：

```text
缺少开发库或静态库
```

---

缺少运行时依赖：

报错：

```text
error while loading shared libraries:

libxxx.so:
cannot open shared object file
```

说明：

```text
运行环境没有对应共享库
```

---

# 十五、完整心智模型

```text
                    编译阶段
────────────────────────────────────

源码 (*.c)

    │

    ├── gcc
    │
    ├── make
    │
    ├── *.h
    │
    ├── pkg-config
    │
    └── libxxx-dev

            ↓

        可执行文件


────────────────────────────────────
                    运行阶段
────────────────────────────────────

可执行文件

    │

    ├── ld-linux
    │
    ├── libc.so
    │
    └── libxxx.so

            ↓

          main()
```

---

# 十六、最终总结

```text
编译时依赖（Build Dependency）

作用：
    把源码变成程序

典型内容：
    gcc
    make
    cmake
    *.h
    libxxx-dev
    pkg-config


运行时依赖（Runtime Dependency）

作用：
    把程序真正运行起来

典型内容：
    ld-linux
    libc.so
    libxxx.so
```

一句话记忆：

> **Build Dependency = “如何制造软件”；Runtime Dependency = “如何启动软件”。**

对于 Linux 下的大多数 C/C++ 项目：

```text
源码
 ↓
安装 xxx-dev
 ↓
configure/cmake
 ↓
make
 ↓
生成 ELF
 ↓
ldd 查看运行依赖
 ↓
程序运行
```

这就是 Linux 软件从源码到运行的完整生命周期。