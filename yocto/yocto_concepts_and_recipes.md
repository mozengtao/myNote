# Yocto Project 核心概念与 Recipe 详解

本文档全面介绍 Yocto Project 的核心概念，重点深入分析 Recipe 的结构、编写方法、变量解析机制等内容。

---

## 目录

1. [Yocto 核心概念](#1-yocto-核心概念)
2. [Recipe 基础](#2-recipe-基础)
3. [Recipe 结构详解](#3-recipe-结构详解)
4. [变量解析顺序与优先级](#4-变量解析顺序与优先级)
5. [添加和管理 Recipe](#5-添加和管理-recipe)
6. [Recipe 高级特性](#6-recipe-高级特性)
7. [常见问题与最佳实践](#7-常见问题与最佳实践)

---

## 1. Yocto 核心概念

### 1.1 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        Yocto Project                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    Poky (Reference Distribution)         │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐       │   │
│  │  │    meta     │  │  meta-poky  │  │meta-yocto-bsp│      │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘       │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                  BitBake (Build Engine)                  │   │
│  │  ┌─────────────────────────────────────────────────────┐ │   │
│  │  │  解析 Recipes (.bb) → 执行 Tasks → 生成 Packages     │ │   │
│  │  └─────────────────────────────────────────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    OpenEmbedded-Core                     │   │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐      │   │
│  │  │ Recipes │  │ Classes │  │ Conf    │  │ Include │      │   │
│  │  │  (.bb)  │  │ (.bbclass)│ │ (.conf) │  │ (.inc)  │     │   │
│  │  └─────────┘  └─────────┘  └─────────┘  └─────────┘      │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 核心组件说明

| 组件 | 说明 |
|------|------|
| **Poky** | Yocto 参考发行版，包含 BitBake、OE-Core 和默认配置 |
| **BitBake** | 构建引擎，解析 recipes 并执行构建任务 |
| **OpenEmbedded-Core (OE-Core)** | 核心 metadata，包含基础 recipes 和 classes |
| **Metadata** | 构建指令的集合（recipes、classes、配置文件） |
| **Layer** | 组织 metadata 的目录结构 |
| **Recipe** | 描述如何构建软件包的文件（.bb） |
| **Class** | 可复用的构建逻辑（.bbclass） |
| **Machine** | 目标硬件平台配置 |
| **Distro** | 发行版策略配置 |
| **Image** | 最终系统镜像的定义 |

### 1.3 Layer（层）概念

```
layer 目录结构:
meta-mylayer/
├── conf/
│   └── layer.conf              # 层配置文件（必须）
├── recipes-*/                  # recipe 分类目录
│   ├── myapp/
│   │   ├── myapp_1.0.bb       # recipe 文件
│   │   ├── myapp_1.1.bb       # 另一版本
│   │   └── files/             # 补丁和配置文件
│   │       ├── 0001-fix.patch
│   │       └── myapp.conf
│   └── another-app/
│       └── another-app_2.0.bb
├── recipes-core/
│   └── images/
│       └── my-image.bb        # 镜像 recipe
├── classes/                    # 自定义 classes
│   └── myclass.bbclass
├── COPYING.MIT                 # 许可证
└── README                      # 说明文档
```

**layer.conf 示例：**

```bash
# conf/layer.conf
BBPATH .= ":${LAYERDIR}"

BBFILES += "${LAYERDIR}/recipes-*/*/*.bb \
            ${LAYERDIR}/recipes-*/*/*.bbappend"

BBFILE_COLLECTIONS += "mylayer"
BBFILE_PATTERN_mylayer = "^${LAYERDIR}/"
BBFILE_PRIORITY_mylayer = "6"

LAYERDEPENDS_mylayer = "core"
LAYERSERIES_COMPAT_mylayer = "kirkstone langdale"
```

### 1.4 构建流程

```
                    ┌─────────────┐
                    │   Source    │
                    │   Code      │
                    └──────┬──────┘
                           │ fetch
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                      WORKDIR                                  │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐   │
│  │ unpack  │───▶│  patch  │───▶│configure│───▶│ compile │   │
│  └─────────┘    └─────────┘    └─────────┘    └─────────┘   │
│                                                      │       │
│       ┌─────────┐    ┌─────────┐    ┌─────────┐     │       │
│       │ rootfs  │◀───│ package │◀───│ install │◀────┘       │
│       └────┬────┘    └─────────┘    └─────────┘             │
└────────────┼────────────────────────────────────────────────┘
             │
             ▼
      ┌─────────────┐
      │   Image     │
      │ (.wic/.img) │
      └─────────────┘
```

---

## 2. Recipe 基础

### 2.1 什么是 Recipe

Recipe（配方）是 Yocto 中描述如何获取、配置、编译、安装软件包的文件。

**命名规范：**
```
<package-name>_<version>.bb

示例：
busybox_1.35.0.bb
linux-yocto_5.15.bb
my-application_git.bb    # Git 版本
my-app_1.0+git.bb        # 带 Git 后缀
```

### 2.2 Recipe 类型

| 类型 | 说明 | 示例 |
|------|------|------|
| **普通 Recipe** | 构建应用程序或库 | `busybox_1.35.0.bb` |
| **Image Recipe** | 定义系统镜像 | `core-image-minimal.bb` |
| **Package Group** | 定义包组 | `packagegroup-base.bb` |
| **Native Recipe** | 构建主机工具 | `quilt-native_0.66.bb` |
| **Kernel Recipe** | 构建 Linux 内核 | `linux-yocto_5.15.bb` |

### 2.3 Recipe vs bbappend

| 文件类型 | 用途 | 优先级 |
|----------|------|--------|
| `.bb` | 原始 recipe 定义 | 基础 |
| `.bbappend` | 扩展/修改已有 recipe | 追加到原 recipe |

**bbappend 示例：**
```bash
# busybox_%.bbappend  (% 匹配任意版本)
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://my-custom.cfg"

do_install:append() {
    # 追加安装步骤
    install -d ${D}/etc/myconfig
}
```

---

## 3. Recipe 结构详解

### 3.1 完整 Recipe 模板

```bash
# my-application_1.0.bb
# Recipe 完整结构示例

#==============================================================================
# 1. 基本信息（描述性元数据）
#==============================================================================
SUMMARY = "My Application - A brief description"
DESCRIPTION = "A longer description of what this application does, \
               its features and use cases."
HOMEPAGE = "https://example.com/my-application"
BUGTRACKER = "https://github.com/user/my-application/issues"

# 作者和维护者
AUTHOR = "John Doe <john@example.com>"
MAINTAINER = "Jane Smith <jane@example.com>"
SECTION = "base"

#==============================================================================
# 2. 许可证信息（必须）
#==============================================================================
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=abc123def456..."

# 多许可证示例
# LICENSE = "GPLv2 & MIT"
# LIC_FILES_CHKSUM = "file://COPYING;md5=xxx \
#                     file://LICENSE.MIT;md5=yyy"

#==============================================================================
# 3. 源码获取
#==============================================================================
# 版本变量
PV = "1.0"                          # Package Version
PR = "r0"                           # Package Revision

# 源码位置
SRC_URI = "https://example.com/downloads/${BPN}-${PV}.tar.gz"
SRC_URI[md5sum] = "abc123..."
SRC_URI[sha256sum] = "def456..."

# Git 源码示例
# SRC_URI = "git://github.com/user/repo.git;branch=main;protocol=https"
# SRCREV = "a1b2c3d4e5f6..."
# S = "${WORKDIR}/git"

# 添加补丁和配置文件
SRC_URI += "\
    file://0001-fix-compilation.patch \
    file://0002-add-feature.patch \
    file://my-config.cfg \
"

# 源码解压目录
S = "${WORKDIR}/${BPN}-${PV}"

#==============================================================================
# 4. 依赖关系
#==============================================================================
# 构建时依赖（其他 recipes）
DEPENDS = "zlib openssl"

# 运行时依赖
RDEPENDS:${PN} = "bash"

# 推荐安装（软依赖）
RRECOMMENDS:${PN} = "my-application-plugins"

# 可选安装
RSUGGESTS:${PN} = "my-application-docs"

# 冲突包
RCONFLICTS:${PN} = "old-application"

# 替换包
RREPLACES:${PN} = "old-application"

# 提供虚拟包
PROVIDES = "virtual/my-service"

#==============================================================================
# 5. 继承 Class（复用构建逻辑）
#==============================================================================
inherit autotools           # GNU Autotools (./configure && make)
# inherit cmake             # CMake 项目
# inherit meson             # Meson 项目
# inherit setuptools3       # Python 项目
# inherit pkgconfig         # 使用 pkg-config

#==============================================================================
# 6. 编译配置
#==============================================================================
# Autotools 配置选项
EXTRA_OECONF = "--enable-feature1 --disable-feature2"

# CMake 配置选项
# EXTRA_OECMAKE = "-DENABLE_FEATURE=ON"

# 编译参数
EXTRA_OEMAKE = "VERBOSE=1"

# 编译器标志
CFLAGS += "-DMY_DEFINE=1"
CXXFLAGS += "-std=c++17"
LDFLAGS += "-lm"

#==============================================================================
# 7. 安装配置
#==============================================================================
# 安装目录
FILES:${PN} = "${bindir}/* ${sysconfdir}/*"
FILES:${PN}-dev = "${includedir}/* ${libdir}/*.so"
FILES:${PN}-doc = "${docdir}/*"

# 配置文件（不被覆盖）
CONFFILES:${PN} = "${sysconfdir}/my-application.conf"

#==============================================================================
# 8. 任务定义（可选，覆盖默认行为）
#==============================================================================
do_configure:prepend() {
    # 在默认 configure 之前执行
    bbnote "Preparing for configuration..."
}

do_compile() {
    # 完全自定义编译
    oe_runmake all
}

do_install() {
    # 自定义安装
    install -d ${D}${bindir}
    install -m 0755 ${B}/myapp ${D}${bindir}/myapp
    
    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/my-config.cfg ${D}${sysconfdir}/myapp.conf
}

do_install:append() {
    # 追加安装步骤
    install -d ${D}${docdir}/${BPN}
    install -m 0644 ${S}/README.md ${D}${docdir}/${BPN}/
}

#==============================================================================
# 9. 包配置
#==============================================================================
# 自定义包拆分
PACKAGES = "${PN} ${PN}-extra ${PN}-dev ${PN}-doc ${PN}-dbg"
FILES:${PN}-extra = "${datadir}/extras/*"

# 包架构
PACKAGE_ARCH = "${MACHINE_ARCH}"
# 或 all（架构无关）
# PACKAGE_ARCH = "all"

#==============================================================================
# 10. 其他配置
#==============================================================================
# 允许的许可证标志
LICENSE_FLAGS = "commercial"

# 仅在特定机器上构建
COMPATIBLE_MACHINE = "(qemux86|qemux86-64|raspberrypi4)"

# 启用特定 DISTRO_FEATURES
REQUIRED_DISTRO_FEATURES = "systemd"

# 构建时使用的 PACKAGECONFIG
PACKAGECONFIG ??= "feature1 feature2"
PACKAGECONFIG[feature1] = "--enable-f1,--disable-f1,dep1"
PACKAGECONFIG[feature2] = "--enable-f2,--disable-f2,dep2,rdep2"
```

### 3.2 常用变量速查

| 变量 | 说明 | 示例值 |
|------|------|--------|
| `PN` | 包名 | `my-application` |
| `PV` | 版本 | `1.0` |
| `PR` | 修订版 | `r0` |
| `BPN` | 基础包名（去除前缀） | `my-application` |
| `BP` | `${BPN}-${PV}` | `my-application-1.0` |
| `WORKDIR` | 工作目录 | `tmp/work/.../my-application/1.0-r0` |
| `S` | 源码目录 | `${WORKDIR}/${BP}` |
| `B` | 构建目录 | 通常等于 `${S}` |
| `D` | 安装目标目录 | `${WORKDIR}/image` |
| `STAGING_DIR_HOST` | 目标 sysroot | `tmp/sysroots/<machine>` |

### 3.3 目录变量详解

```
WORKDIR = tmp/work/<arch>/<recipe>/<version>/
├── temp/                    # 日志和脚本
│   ├── log.do_compile
│   ├── log.do_install
│   └── run.do_compile
├── <source-dir>/            # S - 解压后的源码
│   └── (源码文件)
├── image/                   # D - 安装目标
│   ├── usr/
│   ├── etc/
│   └── ...
├── package/                 # 打包前目录
├── packages-split/          # 拆分后的包
│   ├── <pn>/
│   ├── <pn>-dev/
│   └── <pn>-dbg/
├── deploy-<pn>/             # 部署文件
└── sysroot-destdir/         # Sysroot 内容
```

---

## 4. 变量解析顺序与优先级

### 4.1 配置文件加载顺序

```
优先级从低到高：

1. meta/conf/bitbake.conf           # BitBake 默认配置
         │
         ▼
2. meta*/conf/layer.conf            # 各层配置
         │
         ▼
3. meta*/conf/machine/<machine>.conf # 机器配置
         │
         ▼
4. meta*/conf/distro/<distro>.conf  # 发行版配置
         │
         ▼
5. build/conf/local.conf            # 本地配置
         │
         ▼
6. build/conf/auto.conf             # 自动配置（如果存在）
         │
         ▼
7. Recipe files (.bb, .bbappend)    # Recipe 文件
         │
         ▼
8. Classes (.bbclass)               # 继承的 class
```

### 4.2 变量赋值操作符

| 操作符 | 说明 | 示例 |
|--------|------|------|
| `=` | 默认赋值（延迟展开） | `VAR = "${OTHER}"` |
| `:=` | 立即赋值 | `VAR := "${OTHER}"` |
| `?=` | 默认值（如未设置） | `VAR ?= "default"` |
| `??=` | 弱默认值（最低优先级） | `VAR ??= "weak-default"` |
| `+=` | 追加（带空格） | `VAR += "more"` |
| `=+` | 前置（带空格） | `VAR =+ "prefix"` |
| `.=` | 追加（无空格） | `VAR .= "suffix"` |
| `=.` | 前置（无空格） | `VAR =. "prefix"` |
| `:append` | 追加操作符（新语法） | `VAR:append = " more"` |
| `:prepend` | 前置操作符（新语法） | `VAR:prepend = "prefix "` |
| `:remove` | 删除操作符 | `VAR:remove = "item"` |

### 4.3 赋值优先级示例

```bash
# 文件1: recipe.bb
VAR = "original"

# 文件2: recipe.bbappend  
VAR = "overridden"      # 会覆盖 recipe.bb 中的值

# 文件3: local.conf
VAR = "from-local"      # 会覆盖前面所有

# 最终结果: VAR = "from-local"
```

```bash
# ?= 和 ??= 的区别

# recipe.bb
VAR1 ?= "default1"      # 如果未设置，则使用此值
VAR2 ??= "weak-default" # 弱默认值

# local.conf
VAR1 = "from-local"     # VAR1 = "from-local"
# VAR2 未设置           # VAR2 = "weak-default"

# 另一种情况：
# 如果 VAR2 ?= "another" 在其他地方设置
# 则 VAR2 = "another"（?= 优先于 ??=）
```

### 4.4 Override（覆盖）机制

```bash
# Override 语法（新版 Yocto Honister+）
VAR:override = "value"

# 常用 Override 类型：

# 1. 机器特定
SRC_URI:append:qemux86-64 = " file://qemu-specific.patch"
EXTRA_OEMAKE:raspberrypi4 = "ARCH=arm"

# 2. 发行版特定
DISTRO_FEATURES:append:poky = " systemd"

# 3. 架构特定
CFLAGS:append:arm = " -mfpu=neon"
CFLAGS:append:x86-64 = " -m64"

# 4. 包特定
FILES:${PN}-extra = "/opt/extra/*"
RDEPENDS:${PN}-utils = "bash"

# 5. 任务特定
do_install:append() {
    # 追加安装步骤
}

# 6. 多重 Override（从右到左匹配）
SRC_URI:append:qemux86:pn-myrecipe = " file://special.patch"
```

### 4.5 Override 优先级

```
优先级从低到高：

1. 基础变量        VAR = "base"
2. 架构 Override   VAR:arm = "arm-value"
3. 机器 Override   VAR:qemux86 = "qemu-value"
4. 发行版 Override VAR:poky = "poky-value"
5. 本地 Override   VAR:forcevariable = "forced"
6. local.conf      VAR = "local-value"

# 最具体的 Override 获胜
```

### 4.6 变量展开时机

```bash
# 延迟展开（=）vs 立即展开（:=）

A = "1"
B = "${A}"      # 延迟展开，引用 A
C := "${A}"     # 立即展开，现在就是 "1"

A = "2"

# 结果：
# B = "2"  （延迟展开，最终解析时 A 已是 "2"）
# C = "1"  （立即展开，赋值时 A 是 "1"）
```

---

## 5. 添加和管理 Recipe

### 5.1 添加新 Recipe

**方法1：手动创建**

```bash
# 1. 创建目录结构
mkdir -p meta-mylayer/recipes-apps/myapp/files

# 2. 创建 recipe 文件
cat > meta-mylayer/recipes-apps/myapp/myapp_1.0.bb << 'EOF'
SUMMARY = "My Application"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=xxx"

SRC_URI = "https://example.com/myapp-${PV}.tar.gz"
SRC_URI[sha256sum] = "xxx"

inherit autotools
EOF

# 3. 构建测试
bitbake myapp
```

**方法2：使用 devtool（推荐）**

```bash
# 从 URL 创建
devtool add myapp https://github.com/user/myapp.git

# 构建测试
devtool build myapp

# 完成后移动到目标层
devtool finish myapp ../meta-mylayer
```

**方法3：使用 recipetool**

```bash
# 自动生成 recipe
recipetool create -o meta-mylayer/recipes-apps/myapp/ \
    https://example.com/myapp-1.0.tar.gz
```

### 5.2 修改已有 Recipe

**方法1：创建 bbappend**

```bash
# 1. 找到原 recipe 路径
bitbake-layers show-recipes myapp

# 2. 创建 bbappend（版本通配符）
mkdir -p meta-mylayer/recipes-apps/myapp
cat > meta-mylayer/recipes-apps/myapp/myapp_%.bbappend << 'EOF'
# 添加额外文件搜索路径
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# 添加补丁
SRC_URI += "file://my-fix.patch"

# 修改配置
EXTRA_OECONF += "--enable-my-feature"

# 追加安装步骤
do_install:append() {
    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/myconfig ${D}${sysconfdir}/
}
EOF
```

**方法2：使用 devtool modify**

```bash
# 提取源码到 workspace
devtool modify myapp

# 修改源码
cd workspace/sources/myapp
# ... 编辑代码 ...

# 构建测试
devtool build myapp

# 生成补丁并更新 recipe
devtool update-recipe myapp

# 完成
devtool reset myapp
```

### 5.3 删除 Recipe

```bash
# 方法1：直接删除 recipe 文件
rm meta-mylayer/recipes-apps/myapp/myapp_1.0.bb

# 方法2：从镜像中排除（不删除 recipe）
# 在 local.conf 或镜像 recipe 中：
IMAGE_INSTALL:remove = "myapp"

# 方法3：黑名单（完全禁止构建）
# 在 local.conf 中：
PNBLACKLIST[myapp] = "Not needed for this build"
```

### 5.4 Recipe 版本管理

```bash
# 多版本共存
recipes-apps/myapp/
├── myapp_1.0.bb      # 版本 1.0
├── myapp_1.1.bb      # 版本 1.1
├── myapp_2.0.bb      # 版本 2.0
└── myapp_git.bb      # Git 最新版

# 选择默认版本（在 local.conf 或 layer.conf 中）
PREFERRED_VERSION_myapp = "1.1"

# 通配符版本
PREFERRED_VERSION_myapp = "1.%"  # 1.x 系列最新

# 选择提供者
PREFERRED_PROVIDER_virtual/kernel = "linux-yocto"
```

---

## 6. Recipe 高级特性

### 6.1 PACKAGECONFIG 机制

```bash
# 定义可配置的特性
PACKAGECONFIG ??= "ssl ipv6"

# 格式: PACKAGECONFIG[feature] = "enable,disable,deps,rdeps,conflicts"
PACKAGECONFIG[ssl]  = "--with-ssl,--without-ssl,openssl"
PACKAGECONFIG[ipv6] = "--enable-ipv6,--disable-ipv6"
PACKAGECONFIG[gui]  = "--with-gui,--without-gui,gtk+3,gtk+3"

# 在 bbappend 或 local.conf 中启用/禁用
PACKAGECONFIG:append:pn-myapp = " gui"
PACKAGECONFIG:remove:pn-myapp = "ssl"
```

### 6.2 条件依赖

```bash
# 基于 DISTRO_FEATURES 的条件依赖
DEPENDS += "${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'systemd', '', d)}"

# 基于 MACHINE_FEATURES 的条件
RDEPENDS:${PN} += "${@bb.utils.contains('MACHINE_FEATURES', 'wifi', 'wireless-tools', '', d)}"

# 多条件判断
EXTRA_OECONF += "${@bb.utils.contains_any('DISTRO_FEATURES', 'x11 wayland', '--with-display', '', d)}"
```

### 6.3 虚拟包（Provides）

```bash
# 声明提供虚拟包
PROVIDES = "virtual/kernel"

# 另一个实现
# linux-yocto_5.15.bb
PROVIDES = "virtual/kernel"

# linux-ti_5.10.bb
PROVIDES = "virtual/kernel"

# 选择具体实现
PREFERRED_PROVIDER_virtual/kernel = "linux-ti"

# 依赖虚拟包
DEPENDS = "virtual/kernel"
```

### 6.4 自定义任务

```bash
# 添加新任务
python do_mytask() {
    bb.note("Running my custom task")
    # Python 代码
}
addtask mytask after do_compile before do_install

# Shell 任务
do_prepare_configs() {
    cp ${WORKDIR}/myconfig ${S}/
}
addtask prepare_configs after do_patch before do_configure

# 任务依赖
do_mytask[depends] = "other-recipe:do_build"
do_mytask[rdepends] = "another-recipe:do_install"
```

### 6.5 SRC_URI 详解

```bash
# 本地文件
SRC_URI = "file://myfile.patch"

# HTTP/HTTPS 下载
SRC_URI = "https://example.com/app-${PV}.tar.gz"
SRC_URI[sha256sum] = "..."

# Git 仓库
SRC_URI = "git://github.com/user/repo.git;branch=main;protocol=https"
SRCREV = "abc123..."
# 或使用 tag
# SRCREV = "${AUTOREV}"  # 始终最新

# 多个源
SRC_URI = "\
    git://github.com/user/repo.git;branch=main;protocol=https;name=main \
    git://github.com/user/submodule.git;protocol=https;name=sub;destsuffix=git/subdir \
    file://0001-fix.patch \
    file://config.cfg \
"
SRCREV_main = "abc123"
SRCREV_sub = "def456"

# Subversion
SRC_URI = "svn://svn.example.com/repos/app;module=trunk;protocol=http"

# Mercurial
SRC_URI = "hg://hg.example.com/repo;rev=abc123"

# 带用户名密码
SRC_URI = "git://github.com/user/private.git;protocol=https;user=myuser:mytoken"
```

### 6.6 补丁管理

```bash
# 补丁顺序（按 SRC_URI 中的顺序应用）
SRC_URI += "\
    file://0001-first-fix.patch \
    file://0002-second-fix.patch \
    file://0003-feature.patch;patchdir=subdir \
"

# 条件补丁
SRC_URI:append:arm = " file://arm-specific.patch"
SRC_URI:append:qemux86 = " file://qemu-fix.patch"

# 控制补丁应用
SRC_URI += "file://optional.patch;apply=no"

# 剥离级别
SRC_URI += "file://deep.patch;striplevel=2"
```

---

## 7. 常见问题与最佳实践

### 7.1 Recipe 编写最佳实践

```bash
# ✅ 好的做法

# 1. 使用变量而不是硬编码
SRC_URI = "https://example.com/${BPN}/${BPN}-${PV}.tar.gz"
# 而不是
# SRC_URI = "https://example.com/myapp/myapp-1.0.tar.gz"

# 2. 使用标准安装目录变量
install -d ${D}${bindir}
install -d ${D}${sysconfdir}
# 而不是
# install -d ${D}/usr/bin

# 3. 显式声明所有依赖
DEPENDS = "zlib openssl"
RDEPENDS:${PN} = "bash"

# 4. 正确设置 LICENSE 和校验和
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=abc123"

# 5. 使用 PACKAGECONFIG 管理可选特性
PACKAGECONFIG[feature] = "--enable-feature,--disable-feature,dep"
```

### 7.2 常见错误及解决

| 错误 | 原因 | 解决方案 |
|------|------|----------|
| `Nothing PROVIDES 'xxx'` | 依赖的 recipe 不存在 | 检查拼写，添加缺失的层 |
| `QA Issue: -dev package contains non-symlink .so` | 库文件分类错误 | 调整 `FILES:${PN}` |
| `do_fetch failed` | 网络问题或 URL 错误 | 检查 SRC_URI，使用镜像 |
| `License checksum mismatch` | 许可证文件变更 | 更新 `LIC_FILES_CHKSUM` |
| `Multiple .bb files for xxx` | 同名 recipe 冲突 | 使用 `PREFERRED_VERSION` |

### 7.3 调试技巧

```bash
# 查看变量最终值
bitbake -e myapp | grep ^VARIABLE=

# 查看变量定义位置
bitbake -e myapp | grep -A 5 "^# \$VARIABLE"

# 进入开发 shell
bitbake myapp -c devshell

# 查看任务日志
cat tmp/work/<arch>/myapp/1.0-r0/temp/log.do_compile

# 查看运行脚本
cat tmp/work/<arch>/myapp/1.0-r0/temp/run.do_compile

# 只执行单个任务
bitbake myapp -c compile

# 强制重新执行
bitbake myapp -c compile -f
```

### 7.4 目录结构示例

```
project/
├── poky/                           # Yocto 参考发行版
├── meta-openembedded/              # 社区层
├── meta-mylayer/                   # 自定义层
│   ├── conf/
│   │   └── layer.conf
│   ├── recipes-core/
│   │   ├── images/
│   │   │   └── my-image.bb
│   │   └── packagegroups/
│   │       └── packagegroup-my.bb
│   ├── recipes-apps/
│   │   └── myapp/
│   │       ├── myapp_1.0.bb
│   │       └── files/
│   │           └── patches...
│   ├── recipes-bsp/
│   │   └── u-boot/
│   │       └── u-boot_%.bbappend
│   └── recipes-kernel/
│       └── linux/
│           └── linux-yocto_%.bbappend
└── build/                          # 构建目录
    ├── conf/
    │   ├── local.conf
    │   └── bblayers.conf
    └── tmp/
        ├── work/
        ├── deploy/
        └── sysroots/
```

---

## 附录：常用 Class 列表

| Class | 用途 |
|-------|------|
| `autotools` | GNU Autotools 项目 |
| `cmake` | CMake 项目 |
| `meson` | Meson 构建系统 |
| `setuptools3` | Python 3 setuptools |
| `python_flit_core` | Python flit 项目 |
| `go` | Go 语言项目 |
| `rust` | Rust 项目 |
| `npm` | Node.js npm 包 |
| `kernel` | Linux 内核 |
| `module` | 内核模块 |
| `image` | 系统镜像 |
| `packagegroup` | 包组 |
| `systemd` | systemd 服务 |
| `update-rc.d` | SysVinit 脚本 |
| `useradd` | 创建用户/组 |
| `bin_package` | 预编译二进制包 |
| `native` | 构建主机工具 |
| `nativesdk` | SDK 工具 |

