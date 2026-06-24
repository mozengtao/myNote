# Linux 下通过源码编译安装软件的完整流程与心智模式

以编译安装 tig 为例：

```bash
sudo dnf install -y \
    ncurses-dev \
    libreadline-dev \
    libpcre-dev

cd /tmp
git clone https://github.com/jonas/tig.git
cd tig

make configure
./configure
make

sudo make install
```

很多人只是机械地记忆：

> configure → make → make install

但如果理解其背后的工程模型，就能够编译绝大多数开源软件。

---

# 一、整体心智模型

整个源码安装过程，本质上是：

```
┌────────────────────┐
│ 1. 获取源码         │
└─────────┬──────────┘
          │
          ▼
┌────────────────────┐
│ 2. 安装依赖         │
│    (编译环境)       │
└─────────┬──────────┘
          │
          ▼
┌────────────────────┐
│ 3. 生成构建系统     │
│ (configure/CMake)  │
└─────────┬──────────┘
          │
          ▼
┌────────────────────┐
│ 4. 编译源码         │
│      make          │
└─────────┬──────────┘
          │
          ▼
┌────────────────────┐
│ 5. 运行测试         │
│ make test/check    │
└─────────┬──────────┘
          │
          ▼
┌────────────────────┐
│ 6. 安装到系统       │
│   make install     │
└─────────┬──────────┘
          │
          ▼
┌────────────────────┐
│ 7. 验证安装结果     │
└────────────────────┘
```

实际上：

> 源码安装 = 将源代码转换成系统可执行文件的流水线。

---

# 二、从 CPU 视角理解源码安装

CPU 根本不认识：

```c
printf("hello");
```

CPU 只能执行：

```
机器码
↓
ELF 可执行文件
↓
二进制指令
```

源码安装就是完成这个转换：

```
源代码
  │
  ▼
构建系统生成 Makefile
  │
  ▼
编译器 gcc/clang
  │
  ▼
目标文件 (*.o)
  │
  ▼
链接器 ld
  │
  ▼
ELF 可执行文件
  │
  ▼
复制到系统目录
```

---

# 三、第一步：安装编译依赖

例如：

```bash
sudo dnf install -y \
    ncurses-dev \
    libreadline-dev \
    libpcre-dev
```

作用：

```
安装头文件 (*.h)
安装库文件 (*.so/*.a)
安装 pkg-config 文件
```

例如：

```
ncurses
├── libncurses.so
└── ncurses.h
```

---

## 为什么需要 -dev 包？

运行程序只需要：

```
libxxx.so
```

编译程序需要：

```
libxxx.so
xxx.h
pkgconfig/*.pc
```

因此 Linux 通常拆成：

```
运行时包

libreadline

开发包

libreadline-dev
```

---

# 四、获取源码

通常有四种方式：

---

## Git

```bash
git clone https://github.com/jonas/tig.git
```

适合：

```
开发版
最新代码
需要切换分支
```

---

## Release Tarball

```bash
wget xxx.tar.gz

tar xf xxx.tar.gz
```

适合：

```
稳定版本
生产环境
```

---

## 软件仓库 SRPM

例如：

```bash
dnf download --source tig
```

适合：

```
研究发行版打包方式
```

---

## Git Submodule

适合：

```
大型工程
依赖嵌套
```

---

# 五、生成 configure

这里容易困惑。

看到：

```bash
make configure
```

很多人会问：

> configure 不应该已经存在吗？

实际上：

```
Git源码
↓
configure.ac
Makefile.am
↓
autoconf/automake
↓
configure
Makefile.in
```

即：

```
开发者源码
      ↓
生成 configure
      ↓
用户运行 configure
```

---

心智模型：

```
开发者：

configure.ac
    ↓
autoconf
    ↓
configure

用户：

configure
    ↓
Makefile
```

---

# 六、configure 的作用

执行：

```bash
./configure
```

实际上是在做：

```
环境探测
```

类似：

```pseudo
有没有 gcc？

有没有 ncurses？

有没有 readline？

头文件在哪？

库文件在哪？

安装路径是什么？
```

最终生成：

```
config.h
Makefile
```

---

其工作流程：

```
configure
    │
    ├─ 检查编译器
    │
    ├─ 检查头文件
    │
    ├─ 检查库文件
    │
    ├─ 检查系统调用
    │
    └─ 生成 Makefile
```

---

常见参数：

```bash
./configure --prefix=/usr/local
```

指定安装位置：

```
默认：

/usr/local/bin

改成：

/opt/tig/bin
```

例如：

```bash
./configure --prefix=/opt/tig
```

---

# 七、make 的本质

执行：

```bash
make
```

实际上：

```
读取 Makefile
执行依赖图
```

---

心智模型：

```
        tig
         │
 ┌───────┴────────┐
 │                │
main.o        refs.o
 │               │
main.c        refs.c
```

---

Make 判断：

```
main.c 修改了？

是 → 重新编译

否 → 跳过
```

---

因此 make 本质是：

```
增量构建系统
```

而不是：

```
编译器
```

真正编译的是：

```
gcc
clang
```

---

查看实际命令：

```bash
make V=1
```

或者：

```bash
make VERBOSE=1
```

---

# 八、并行编译

推荐：

```bash
make -j$(nproc)
```

含义：

```
使用所有 CPU 核心
```

例如：

```
8 核 CPU：

make -j8
```

---

心智模型：

```
main.c
refs.c
view.c
io.c

可同时编译
```

因此：

```
单线程：

40分钟

多线程：

5分钟
```

---

# 九、测试阶段

很多人忽略：

```bash
make test

或者

make check
```

---

作用：

```
验证编译结果正确
```

流程：

```
源码
↓
编译
↓
运行测试
↓
确认功能正常
↓
安装
```

生产环境推荐：

```
一定执行
```

---

# 十、安装阶段

执行：

```bash
sudo make install
```

作用：

```
复制文件
```

很多人误以为：

> make install 负责编译。

实际上：

```
make
负责生成

make install
负责复制
```

---

典型安装目录：

```
/usr/local/bin/

/usr/local/lib/

/usr/local/share/

/usr/local/include/
```

例如：

```
tig
↓

/usr/local/bin/tig
```

---

安装过程：

```
cp tig /usr/local/bin/

cp man/*.1 /usr/local/share/man/

cp docs/* ...
```

---

# 十一、卸载怎么办？

有些项目支持：

```bash
sudo make uninstall
```

本质：

```
执行 Makefile 中 uninstall 规则
```

---

如果不支持：

安装前建议：

```bash
./configure --prefix=/opt/tig
```

卸载：

```bash
sudo rm -rf /opt/tig
```

---

# 十二、查看安装了什么

安装前：

```bash
make -n install
```

作用：

```
只打印，不执行
```

例如：

```bash
install -m755 tig /usr/local/bin
install -m644 tig.1 ...
```

---

查看实际安装位置：

```bash
which tig

type -a tig
```

例如：

```bash
/usr/local/bin/tig
```

---

# 十三、常见构建系统

## 1. Autotools

```
configure
make
make install
```

代表：

```
tig
vim
git
```

---

## 2. CMake

```
cmake -B build
cmake --build build
sudo cmake --install build
```

代表：

```
LLVM
OpenCV
```

---

## 3. Meson

```
meson setup build

meson compile -C build

sudo meson install -C build
```

代表：

```
systemd
GNOME
```

---

## 4. Cargo

```
cargo build

cargo install
```

代表：

```
Rust 项目
```

---

## 5. Go

```
go build

go install
```

代表：

```
Go 项目
```

---

# 十四、完整心智模式总结

```
                Linux 源码安装流水线

获取源码
    │
    ▼
安装开发依赖
    │
    ▼
生成构建系统
(configure/CMake/Meson)
    │
    ▼
探测环境
(编译器/头文件/库)
    │
    ▼
生成 Makefile
    │
    ▼
make
    │
    ▼
gcc/clang 编译
    │
    ▼
ld 链接
    │
    ▼
ELF 可执行文件
    │
    ▼
make test/check
    │
    ▼
make install
    │
    ▼
复制到系统目录
    │
    ▼
PATH 可发现
    │
    ▼
用户运行程序
```

---

# 十五、一句话记忆

```
源码安装不是“安装软件”，

而是：

把开发者的源代码，
通过构建系统转换成适合当前机器运行的二进制，
再部署到操作系统标准目录中的过程。
```

或者更工程化地理解：

```
Source Code
    ↓
Build System
    ↓
Compiler Toolchain
    ↓
Binary Artifacts
    ↓
System Deployment
```

记住这个模型后，无论遇到：

```
Autotools
CMake
Meson
Cargo
Go
Bazel
```

都只是：

> “生成构建图 → 执行构建 → 部署产物” 的不同实现。