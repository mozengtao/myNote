# BitBake 常用命令及技巧大全

BitBake 是 Yocto Project / OpenEmbedded 构建系统的核心构建工具。本文档整理了日常开发中常用的命令、参数和技巧。

---

## 目录

1. [bitbake 核心命令](#1-bitbake-核心命令)
2. [bitbake-layers 层管理](#2-bitbake-layers-层管理)
3. [bitbake-getvar 变量查询](#3-bitbake-getvar-变量查询)
4. [devtool 开发工具](#4-devtool-开发工具)
5. [recipetool 配方工具](#5-recipetool-配方工具)
6. [oe-pkgdata-util 包查询](#6-oe-pkgdata-util-包查询)
7. [调试与分析技巧](#7-调试与分析技巧)
8. [常用组合命令](#8-常用组合命令)
9. [快速参考表](#9-快速参考表)

---

## 1. bitbake 核心命令

### 1.1 基本构建

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 构建镜像 | `bitbake <image-name>` | 构建完整的 Linux 镜像 |
| 构建单个包 | `bitbake <recipe-name>` | 构建指定的 recipe |
| 构建多个包 | `bitbake <recipe1> <recipe2>` | 同时构建多个 recipe |

**示例：**

```bash
# 构建最小镜像
bitbake core-image-minimal

# 构建带命令行工具的镜像
bitbake core-image-full-cmdline

# 构建带 Sato GUI 的镜像
bitbake core-image-sato

# 构建内核
bitbake linux-yocto

# 构建 bootloader
bitbake u-boot

# 构建 busybox
bitbake busybox

# 同时构建多个包
bitbake busybox dropbear openssh
```

### 1.2 任务控制 (-c)

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 执行指定任务 | `bitbake <recipe> -c <task>` | 只执行特定任务 |
| 强制执行任务 | `bitbake <recipe> -c <task> -f` | 强制重新执行任务 |

**常用任务列表：**

| 任务名 | 说明 |
|--------|------|
| `fetch` | 下载源代码 |
| `unpack` | 解压源代码 |
| `patch` | 应用补丁 |
| `configure` | 配置（如 ./configure） |
| `compile` | 编译 |
| `install` | 安装到临时目录 |
| `package` | 打包 |
| `clean` | 清理构建目录 |
| `cleanall` | 清理所有（含下载缓存） |
| `cleansstate` | 清理 sstate 缓存 |
| `devshell` | 进入开发 shell |
| `listtasks` | 列出所有任务 |

**示例：**

```bash
# 只下载源码
bitbake busybox -c fetch

# 只配置
bitbake busybox -c configure

# 只编译
bitbake busybox -c compile

# 只安装
bitbake busybox -c install

# 只打包
bitbake busybox -c package

# 清理构建目录（保留下载）
bitbake busybox -c clean

# 清理所有（包括下载的源码）
bitbake busybox -c cleanall

# 清理 sstate 缓存（推荐用于完整重建）
bitbake busybox -c cleansstate

# 强制重新编译
bitbake busybox -c compile -f

# 列出所有可用任务
bitbake busybox -c listtasks

# 进入开发 shell（可手动编译调试）
bitbake busybox -c devshell

# 进入 Python 调试 shell
bitbake busybox -c devpyshell
```

### 1.3 信息查询

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 显示环境变量 | `bitbake <recipe> -e` | 显示所有变量展开后的值 |
| 生成依赖图 | `bitbake <recipe> -g` | 生成 .dot 格式依赖图 |
| 显示调试信息 | `bitbake <recipe> -D[DD]` | 增加调试输出级别 |
| 只解析不构建 | `bitbake <recipe> -p` | 解析 recipe 但不执行 |
| 显示版本 | `bitbake --version` | 显示 BitBake 版本 |

**示例：**

```bash
# 显示 busybox 所有变量
bitbake busybox -e

# 查看特定变量（配合 grep）
bitbake busybox -e | grep ^WORKDIR=
bitbake busybox -e | grep ^SRC_URI=
bitbake busybox -e | grep ^S=
bitbake busybox -e | grep ^D=
bitbake busybox -e | grep ^DEPENDS=
bitbake busybox -e | grep ^FILE=

# 生成依赖图
bitbake core-image-minimal -g
# 生成的文件：
#   task-depends.dot  - 任务级依赖
#   pn-buildlist      - 构建列表
#   recipe-depends.dot - recipe 级依赖

# 显示详细调试信息
bitbake busybox -D      # 1级调试
bitbake busybox -DD     # 2级调试
bitbake busybox -DDD    # 3级调试（最详细）

# 只解析不构建
bitbake busybox -p

# 解析并显示
bitbake busybox -p -e
```

### 1.4 构建控制选项

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 继续构建（忽略错误） | `bitbake -k <recipe>` | 遇错继续构建其他 |
| 设置并行任务数 | `bitbake -j <N> <recipe>` | BB_NUMBER_THREADS |
| 试运行 | `bitbake -n <recipe>` | 显示将执行的任务 |
| 指定配置文件 | `bitbake -R <file> <recipe>` | 读取额外配置 |

**示例：**

```bash
# 遇错继续构建
bitbake -k core-image-minimal

# 设置并行任务数
bitbake -j 8 core-image-minimal

# 试运行（不实际执行）
bitbake -n busybox

# 使用额外配置文件
bitbake -R myconfig.conf core-image-minimal

# 组合使用
bitbake -k -j 4 core-image-minimal
```

---

## 2. bitbake-layers 层管理

### 2.1 查询层信息

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 显示所有层 | `bitbake-layers show-layers` | 列出已添加的层及优先级 |
| 显示所有 recipes | `bitbake-layers show-recipes` | 列出所有可用 recipe |
| 搜索 recipes | `bitbake-layers show-recipes "<pattern>"` | 按名称搜索 |
| 显示被覆盖的 recipes | `bitbake-layers show-overlayed` | 显示被上层覆盖的 recipe |
| 显示 bbappend 文件 | `bitbake-layers show-appends` | 显示所有 .bbappend |
| 显示层间依赖 | `bitbake-layers show-cross-depends` | 层之间的依赖关系 |

**示例：**

```bash
# 显示所有已添加的层
bitbake-layers show-layers
# 输出示例：
# layer                 path                                      priority
# =============================================================================
# meta                  /home/user/poky/meta                      5
# meta-poky             /home/user/poky/meta-poky                 5
# meta-yocto-bsp        /home/user/poky/meta-yocto-bsp            5
# meta-mylayer          /home/user/meta-mylayer                   6

# 显示所有 recipes
bitbake-layers show-recipes

# 搜索包含 "glib" 的 recipes
bitbake-layers show-recipes "*glib*"

# 搜索所有内核 recipes
bitbake-layers show-recipes "linux-*"

# 显示被覆盖的 recipes
bitbake-layers show-overlayed

# 显示所有 .bbappend 文件
bitbake-layers show-appends

# 显示层间依赖
bitbake-layers show-cross-depends
```

### 2.2 管理层

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 添加层 | `bitbake-layers add-layer <path>` | 添加层到 bblayers.conf |
| 移除层 | `bitbake-layers remove-layer <name>` | 从配置中移除层 |
| 创建新层 | `bitbake-layers create-layer <path>` | 创建新的层结构 |

**示例：**

```bash
# 添加层（相对路径）
bitbake-layers add-layer ../meta-mylayer

# 添加层（绝对路径）
bitbake-layers add-layer /home/user/layers/meta-custom

# 移除层
bitbake-layers remove-layer meta-mylayer

# 创建新层
bitbake-layers create-layer ../meta-newlayer
# 创建的目录结构：
# meta-newlayer/
# ├── conf/
# │   └── layer.conf
# ├── COPYING.MIT
# ├── README
# └── recipes-example/
#     └── example/
#         └── example_0.1.bb

# 带选项创建层
bitbake-layers create-layer --priority 7 ../meta-newlayer
```

### 2.3 诊断命令

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 展平层 | `bitbake-layers flatten <dest>` | 将所有层合并到单一目录 |
| 查询 recipe 来源 | `bitbake-layers show-recipes -f <recipe>` | 显示 recipe 文件路径 |

**示例：**

```bash
# 展平所有层到临时目录（调试用）
bitbake-layers flatten /tmp/flattened-layers

# 查看 busybox recipe 的完整路径
bitbake-layers show-recipes -f busybox
```

---

## 3. bitbake-getvar 变量查询

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 查看全局变量 | `bitbake-getvar <VAR>` | 显示变量值 |
| 查看 recipe 变量 | `bitbake-getvar -r <recipe> <VAR>` | 显示特定 recipe 的变量 |
| 显示变量来源 | `bitbake-getvar --value <VAR>` | 只显示值，不显示来源 |

**示例：**

```bash
# 查看全局变量
bitbake-getvar DISTRO
bitbake-getvar MACHINE
bitbake-getvar TMPDIR
bitbake-getvar DEPLOY_DIR
bitbake-getvar IMAGE_INSTALL

# 查看特定 recipe 的变量
bitbake-getvar -r busybox SRC_URI
bitbake-getvar -r busybox WORKDIR
bitbake-getvar -r busybox S
bitbake-getvar -r busybox D
bitbake-getvar -r busybox DEPENDS
bitbake-getvar -r busybox RDEPENDS
bitbake-getvar -r busybox PV
bitbake-getvar -r busybox PN

# 查看内核相关变量
bitbake-getvar -r linux-yocto KERNEL_VERSION
bitbake-getvar -r linux-yocto KBUILD_DEFCONFIG

# 只显示值
bitbake-getvar --value MACHINE
```

---

## 4. devtool 开发工具

devtool 是 Yocto 提供的高级开发工具，简化了修改和创建 recipe 的流程。

### 4.1 修改现有 Recipe

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 提取源码到 workspace | `devtool modify <recipe>` | 自动提取并配置 |
| 提取到指定目录 | `devtool modify -x <recipe> <dir>` | 指定源码目录 |
| 构建修改后的代码 | `devtool build <recipe>` | 在 workspace 中构建 |
| 重置 workspace | `devtool reset <recipe>` | 清理 workspace |
| 更新 recipe | `devtool update-recipe <recipe>` | 将修改写回 recipe |

**示例：**

```bash
# 修改 busybox（提取到默认 workspace）
devtool modify busybox
# 源码位于: workspace/sources/busybox/

# 修改 busybox（提取到指定目录）
devtool modify -x busybox ~/my-busybox

# 修改源码后构建
cd workspace/sources/busybox
# ... 修改代码 ...
devtool build busybox

# 将修改写回原 recipe（生成 .bbappend）
devtool update-recipe busybox

# 将修改写回指定层
devtool update-recipe -a ../meta-mylayer busybox

# 重置 workspace（完成后清理）
devtool reset busybox
```

### 4.2 创建新 Recipe

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 从 URL 创建 | `devtool add <name> <url>` | 下载并创建 recipe |
| 从本地目录创建 | `devtool add <name> <path>` | 从本地源码创建 |
| 完成后提交到层 | `devtool finish <name> <layer>` | 移动 recipe 到目标层 |

**示例：**

```bash
# 从 GitHub 创建新 recipe
devtool add myapp https://github.com/user/myapp.git

# 从 tarball 创建
devtool add myapp https://example.com/myapp-1.0.tar.gz

# 从本地目录创建
devtool add myapp /path/to/local/source

# 指定版本
devtool add myapp https://github.com/user/myapp.git --version 1.0

# 构建新 recipe
devtool build myapp

# 完成开发，提交到目标层
devtool finish myapp ../meta-mylayer
```

### 4.3 升级 Recipe

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 升级到新版本 | `devtool upgrade <recipe>` | 自动升级 |
| 指定版本升级 | `devtool upgrade <recipe> --version <ver>` | 升级到指定版本 |
| 指定源码 URL | `devtool upgrade <recipe> --srcrev <rev>` | 指定 Git commit |

**示例：**

```bash
# 自动升级到最新版本
devtool upgrade busybox

# 升级到指定版本
devtool upgrade busybox --version 1.35.0

# 从指定 Git commit 升级
devtool upgrade busybox --srcrev abc123def456

# 升级后将变更写回
devtool update-recipe busybox
devtool reset busybox
```

### 4.4 部署测试

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 部署到目标 | `devtool deploy-target <recipe> <target>` | SSH 部署 |
| 撤销部署 | `devtool undeploy-target <recipe> <target>` | 撤销 |

**示例：**

```bash
# 部署到目标设备（通过 SSH）
devtool deploy-target busybox root@192.168.1.100

# 带端口号
devtool deploy-target busybox root@192.168.1.100:2222

# 撤销部署
devtool undeploy-target busybox root@192.168.1.100
```

### 4.5 状态查询

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 查看 workspace 状态 | `devtool status` | 列出所有正在开发的 recipe |
| 搜索 recipe | `devtool search <keyword>` | 搜索可用 recipe |

**示例：**

```bash
# 查看当前 workspace 状态
devtool status
# 输出示例：
# busybox: /home/user/workspace/sources/busybox
# myapp: /home/user/workspace/sources/myapp

# 搜索 recipe
devtool search gstreamer
```

---

## 5. recipetool 配方工具

### 5.1 创建 Recipe

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 从 URL 创建 | `recipetool create <url>` | 自动生成 recipe |
| 从本地创建 | `recipetool create <path>` | 从本地源码生成 |
| 指定输出目录 | `recipetool create -o <dir> <url>` | 指定 recipe 输出位置 |

**示例：**

```bash
# 从 tarball URL 创建 recipe
recipetool create https://example.com/myapp-1.0.tar.gz

# 从 Git 仓库创建
recipetool create https://github.com/user/myapp.git

# 指定输出到特定层
recipetool create -o ../meta-mylayer/recipes-apps/myapp \
    https://example.com/myapp-1.0.tar.gz

# 从本地源码创建
recipetool create /path/to/local/source
```

### 5.2 追加文件和设置变量

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 追加文件到包 | `recipetool appendfile <layer> <target> <file>` | 添加文件到 recipe |
| 设置变量 | `recipetool setvar <recipe> <VAR> <value>` | 设置 recipe 变量 |

**示例：**

```bash
# 将本地文件添加到 recipe（安装到 /usr/bin）
recipetool appendfile ../meta-mylayer /usr/bin/myscript ./myscript.sh

# 将配置文件添加到 recipe
recipetool appendfile ../meta-mylayer /etc/myapp.conf ./myapp.conf

# 设置 recipe 变量
recipetool setvar meta-mylayer/recipes-apps/myapp/myapp_1.0.bb \
    HOMEPAGE "https://example.com"

recipetool setvar meta-mylayer/recipes-apps/myapp/myapp_1.0.bb \
    LICENSE "MIT"
```

---

## 6. oe-pkgdata-util 包查询

| 目的 | 命令格式 | 说明 |
|------|----------|------|
| 列出所有包 | `oe-pkgdata-util list-pkgs` | 显示所有已构建的包 |
| 搜索包 | `oe-pkgdata-util list-pkgs "*pattern*"` | 按名称搜索包 |
| 列出包中文件 | `oe-pkgdata-util list-pkg-files <pkg>` | 显示包内容 |
| 查找文件所属包 | `oe-pkgdata-util find-path <path>` | 反向查询 |
| 查找包对应 recipe | `oe-pkgdata-util lookup-recipe <pkg>` | 包到 recipe 映射 |
| 读取包属性 | `oe-pkgdata-util read-value <VAR> <pkg>` | 读取包元数据 |

**示例：**

```bash
# 列出所有已构建的包
oe-pkgdata-util list-pkgs

# 搜索包含 "ssl" 的包
oe-pkgdata-util list-pkgs "*ssl*"

# 列出 busybox 包中的所有文件
oe-pkgdata-util list-pkg-files busybox

# 查找 /usr/bin/ls 属于哪个包
oe-pkgdata-util find-path /usr/bin/ls
# 输出: busybox: /usr/bin/ls

# 查找 /lib/libc.so.6 属于哪个包
oe-pkgdata-util find-path /lib/libc.so.6

# 查找 busybox 包对应的 recipe
oe-pkgdata-util lookup-recipe busybox

# 读取包大小
oe-pkgdata-util read-value PKGSIZE busybox

# 读取包依赖
oe-pkgdata-util read-value RDEPENDS busybox
```

---

## 7. 调试与分析技巧

### 7.1 构建错误排查

```bash
# 查看编译日志
cat tmp/work/<arch>/<recipe>/<version>/temp/log.do_compile

# 快速定位日志目录
cd $(bitbake -e busybox | grep ^T= | cut -d'"' -f2)
ls -la log.*

# 查看最近的错误日志
find tmp/work -name "log.do_compile" -mmin -30 | xargs tail -50

# 进入失败的构建环境手动调试
bitbake busybox -c devshell
# 在 devshell 中：
# source oe-init-build-env  (已自动配置)
# make                      (手动编译调试)
```

### 7.2 依赖分析

```bash
# 生成依赖图
bitbake core-image-minimal -g

# 查看构建列表
cat pn-buildlist

# 图形化查看依赖（需要 graphviz）
dot -Tpng task-depends.dot -o task-depends.png
dot -Tpng recipe-depends.dot -o recipe-depends.png

# 查看某个包为什么被构建
grep busybox task-depends.dot

# 使用 taskexp（图形化任务浏览器）
taskexp &
```

### 7.3 性能分析

```bash
# 生成构建统计
bitbake core-image-minimal -u taskexp

# 查看 sstate 缓存命中情况
bitbake busybox -DDD 2>&1 | grep -i sstate

# 计算构建时间
time bitbake core-image-minimal 2>&1 | tee build.log

# 使用 buildstats（构建统计）
# 在 local.conf 中启用：
# INHERIT += "buildstats"
# 查看统计：
cat tmp/buildstats/<timestamp>/<recipe>/do_compile
```

### 7.4 变量调试

```bash
# 追踪变量定义位置
bitbake -e busybox | grep -A 5 "^# \$CFLAGS"

# 使用 bitbake-diffsigs 比较签名
bitbake-diffsigs tmp/stamps/<path1> tmp/stamps/<path2>

# 检查为什么 sstate 未命中
bitbake-dumpsig tmp/stamps/<recipe>/do_compile.sigdata
```

---

## 8. 常用组合命令

### 8.1 开发工作流

```bash
# 完整重建某个包
bitbake <recipe> -c cleansstate && bitbake <recipe>

# 修改后快速重建
bitbake <recipe> -c compile -f && bitbake <recipe>

# 编辑 recipe 文件
$EDITOR $(bitbake -e <recipe> | grep ^FILE= | cut -d'"' -f2)

# 进入工作目录
cd $(bitbake -e <recipe> | grep ^WORKDIR= | cut -d'"' -f2)

# 进入源码目录
cd $(bitbake -e <recipe> | grep ^S= | cut -d'"' -f2)

# 查看安装目录
ls $(bitbake -e <recipe> | grep ^D= | cut -d'"' -f2)
```

### 8.2 镜像和 SDK

```bash
# 构建 SDK
bitbake core-image-minimal -c populate_sdk

# 构建可扩展 SDK (eSDK)
bitbake core-image-minimal -c populate_sdk_ext

# 只更新包索引
bitbake package-index

# 生成许可证清单
bitbake core-image-minimal -c populate_lic
```

### 8.3 批量操作

```bash
# 清理所有包的 sstate
bitbake world -c cleansstate

# 构建所有可构建的包
bitbake world

# 检查所有 recipe 的语法
bitbake -p world

# 只构建某个包及其依赖
bitbake --runall=fetch busybox
```

### 8.4 信息提取脚本

```bash
# 获取所有包的版本信息
for recipe in busybox dropbear; do
    ver=$(bitbake-getvar -r $recipe PV --value 2>/dev/null)
    echo "$recipe: $ver"
done

# 导出所有层的 recipe 列表
bitbake-layers show-recipes 2>/dev/null | grep -E "^[a-z]" > all-recipes.txt

# 查找使用特定许可证的包
bitbake-layers show-recipes 2>/dev/null | while read recipe; do
    lic=$(bitbake-getvar -r "$recipe" LICENSE --value 2>/dev/null)
    if echo "$lic" | grep -q "GPL"; then
        echo "$recipe: $lic"
    fi
done
```

---

## 9. 快速参考表

### 最常用命令

| 目的 | 命令 |
|------|------|
| 构建镜像 | `bitbake <image>` |
| 构建单个包 | `bitbake <recipe>` |
| 只编译 | `bitbake <recipe> -c compile` |
| 清理重建 | `bitbake <recipe> -c cleansstate && bitbake <recipe>` |
| 查看变量 | `bitbake -e <recipe> \| grep ^VAR=` |
| 查看变量（推荐） | `bitbake-getvar -r <recipe> VAR` |
| 进入开发 shell | `bitbake <recipe> -c devshell` |
| 列出任务 | `bitbake <recipe> -c listtasks` |
| 查看层 | `bitbake-layers show-layers` |
| 添加层 | `bitbake-layers add-layer <path>` |
| 搜索 recipe | `bitbake-layers show-recipes "*keyword*"` |
| 修改包（开发） | `devtool modify <recipe>` |
| 创建新 recipe | `devtool add <name> <url>` |
| 查找文件所属包 | `oe-pkgdata-util find-path <file>` |
| 列出包内文件 | `oe-pkgdata-util list-pkg-files <pkg>` |

### 目录变量速查

| 变量 | 说明 | 示例路径 |
|------|------|----------|
| `TMPDIR` | 临时目录根 | `tmp/` |
| `WORKDIR` | recipe 工作目录 | `tmp/work/<arch>/<recipe>/<ver>/` |
| `S` | 解压后的源码目录 | `${WORKDIR}/<name>-<ver>/` |
| `B` | 构建目录 | 通常等于 `${S}` |
| `D` | 安装目标目录 | `${WORKDIR}/image/` |
| `DEPLOY_DIR` | 部署目录 | `tmp/deploy/` |
| `DEPLOY_DIR_IMAGE` | 镜像输出目录 | `tmp/deploy/images/<machine>/` |

### 任务执行顺序

```
do_fetch → do_unpack → do_patch → do_configure → do_compile → do_install → do_package → do_rootfs
```

---

## 附录：环境变量

在使用 BitBake 命令前，需要先初始化构建环境：

```bash
# Yocto/Poky 环境初始化
source oe-init-build-env [build-dir]

# 指定构建目录
source oe-init-build-env mybuild

# 常用环境变量
export BB_NUMBER_THREADS=8        # 并行任务数
export PARALLEL_MAKE="-j 8"       # make 并行数
export DL_DIR=/path/to/downloads  # 下载缓存目录
export SSTATE_DIR=/path/to/sstate # sstate 缓存目录
```

