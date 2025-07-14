[Yocto Project Documentation](https://docs.yoctoproject.org/)  
[Yocto Project Reference Manual](https://docs.yoctoproject.org/ref-manual/index.html)  
[Using the devtool command-line tool](https://docs.yoctoproject.org/dev-manual/devtool.html)  
[]()  
[]()  
[]()  
[]()  



[**A practical guide to BitBake**](https://a4z.gitlab.io/docs/BitBake/guide.html)  
[**Variables Glossary**](https://docs.yoctoproject.org/ref-manual/variables.html)  
[**Classes**](https://docs.yoctoproject.org/ref-manual/classes.html#)  
[Images](https://docs.yoctoproject.org/ref-manual/images.html#images)  
[**Recipe Syntax**](https://docs.yoctoproject.org/dev-manual/new-recipe.html#recipe-syntax)  
[**Writing a New Recipe**](https://docs.yoctoproject.org/dev-manual/new-recipe.html)  
[**Syntax and Operators**](https://docs.yoctoproject.org/bitbake/2.2/bitbake-user-manual/bitbake-user-manual-metadata.html#syntax-and-operators)  
[Recipe Style Guide](https://docs.yoctoproject.org/contributor-guide/recipe-style-guide.html)  

[**BitBake User Manual**](https://docs.yoctoproject.org/bitbake/)  
[**BitBake User Manual**](https://docs.yoctoproject.org/bitbake/2.2/index.html)  
[**BitBake User Manual**](https://docs.yoctoproject.org/1.6/bitbake-user-manual/bitbake-user-manual.html)  
[Source Directory Structure](https://docs.yoctoproject.org/ref-manual/structure.html#source-directory-structure)  
[Building your own recipes from first principles](https://wiki.yoctoproject.org/wiki/Building_your_own_recipes_from_first_principles)  
[Writing BitBake Recipes](https://www.multitech.net/developer/software/corecdp/development/writing-bitbake-recipes/)  
[Understanding and creating your first custom recipe](https://kickstartembedded.com/2022/01/21/yocto-part-6-understanding-and-creating-your-first-custom-recipe/)  
[]()  
[]()  
[]()  

## Setup BitBake environment
[Yocto 入门文档](https://pages.openeuler.openatom.cn/embedded/docs/build/html/openEuler-23.09/yocto/getting_started.html)  
[Hello World Example](https://docs.yoctoproject.org/bitbake/2.0/bitbake-user-manual/bitbake-user-manual-hello.html)  
[**BitBake User Manual**](https://docs.yoctoproject.org/bitbake/index.html)  
[Yocto Project Reference Manual](https://docs.yoctoproject.org/ref-manual/index.html)  
[]()  
[]()  

## Variables Glossary
[**Variables Glossary**](https://docs.yoctoproject.org/ref-manual/variables.html)  
```bash
# EXTRA_IMAGE_FEATURES
    A list of additional features to include in an image. When listing more than one feature, separate them with a space.
    ""tools-debug" — adds debugging tools such as gdb and strace.
    ...

```

## bitbake commands
```
bitbake -c listtasks <recipe_name>

bitbake -e vmc-device-yang2fxs | grep ^FILE=
FILE="/home/morrism/workspace/vcmts/vcmos/meta-vcore/meta-vcore-base/recipes-vmc/vmc-device-yang2fxs/vmc-device-yang2fxs.bb"

bitbake -b <path_to_bb_file> -c listtasks

```

## Classes
### rm_work
> The rm_work class supports deletion of temporary workspace, which can ease your hard drive demands during builds.
```bash
# you can enable rm_work by adding the following to your local.conf file, which is found in the Build Directory
INHERIT += "rm_work"

# To exclude some recipes from having their work directories deleted by rm_work, you can add the names of the recipe or recipes you are working on to the RM_WORK_EXCLUDE variable, which can also be set in your local.conf file. Here is an example
RM_WORK_EXCLUDE += "busybox glibc"
or
RM_WORK_EXCLUDE:append = " busybox glibc"
```

## Miscs
```bash
# recipe-sysroot/sysroot-only 目录
sysroot-only/ 子目录专门用于存放​​仅需在编译阶段使用、但无需安装到目标设备​​的文件

假设一个配方的构建工作目录为：
tmp/work/<arch>/<recipe>/<version>/
该目录下看到的结构如下：
recipe-sysroot/              # 完整的 sysroot 内容
├── usr/include/             # 头文件
├── usr/lib/                 # 动态库、静态库
└── sysroot-only/            # 仅构建所需的文件（不打包到镜像）
    ├── usr/lib/libfoo.a     # 静态库（仅编译时使用）
    └── usr/share/pkgconfig/ # pkg-config 文件（仅编译时使用）

# bitbake-layers
bitbake-layers show-layers
    show current configured layers.

bitbake-layers show-recipes
    Lists available recipes and the layers that provide them.

bitbake -s
    Show current and preferred versions of all recipes.
```

## Recipe
```bash
DESCRIPTION = "Hello world program"
HOMEPAGE = "http://example.net/hello/"
PRIORITY = "optional"
SECTION = "examples"
LICENSE = "GPL-2.0-or-later"

SRC_URI = "git://git.example.com/hello;protocol=https;branch=master"
SRCREV = "2d47b4eb66e705458a17622c2e09367300a7b118"
S = "${WORKDIR}/git"
LIC_FILES_CHKSUM = "file://hello.c;beginline=3;endline=21;md5=58e..."

do_compile() {
    oe_runmake
}
do_install() {
    install -d ${D}${bindir}
    install -m 0755 hello ${D}${bindir}
}

# tar.inc
SUMMARY = "GNU file archiving program"
HOMEPAGE = "https://www.gnu.org/software/tar/"
SECTION = "base"

SRC_URI = "${GNU_MIRROR}/tar/tar-${PV}.tar.bz2"

do_configure() { ... }

do_compile() { ... }

do_install() { ... }


# tar_1.17.bb
require tar.inc

LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = \
"file://COPYING;md5=59530bdf33659b29e73d4adb9f9f6552"

SRC_URI += "file://avoid_heap_overflow.patch"

SRC_URI[md5sum] = "c6c4f1c075dbf0f75c29737faa58f290"


# tar_1.26.bb
require tar.inc

LICENSE = "GPL-3.0-only"
LIC_FILES_CHKSUM = \
"file://COPYING;md5=d32239bcb673463ab874e80d47fae504"

SRC_URI[md5sum] = "2cee42a2ff4f1cd4f9298eeeb2264519"


#verbose mode
bitbake -vDDD your-recipe

#list recipes
bitbake -s

#list recipe tasks
bitbake -c listtasks recipe

#run only 1 task of a recipe
bitbake -c your-task your-recipe

#run all tasks for all recipes
bitbake world

#bitbake-layers command help
bitbake-layers -h

#add new layer
bitbake-layers add-layer

#remove layer
bitbake-layers remove-layer

#show layer
bitbake-layers show-layers

#show recipe
bitbake-layers show-recipes

#dump task order of a recipe
cat build_directory/tmp/work/machine_toolchain/package_name/package_version/temp/log.task_order

# Variable Override
# example 1
OVERRIDES="arm:armv7a:ti-soc:ti33x:beaglebone:poky"

KERNEL_DEVICETREE:beaglebone = "am335x-bone.dtb" # This is applied
KERNEL_DEVICETREE:dra7xx-evm = "dra7-evm.dtb" # This is ignored

# example 2
IMAGE_INSTALL = "busybox mtd-utils"
IMAGE_INSTALL:append = " dropbear"
IMAGE_INSTALL:append:beaglebone = " i2c-tools"

如果machine是beaglebone
IMAGE_INSTALL = "busybox mtd-utils dropbear i2c-tools"
否则
IMAGE_INSTALL = "busybox mtd-utils dropbear"

# example 3
IMAGE_INSTALL:beaglebone = "busybox mtd-utils i2c-tools"
IMAGE_INSTALL = "busybox mtd-utils"

如果machine是beaglebone
IMAGE_INSTALL = "busybox mtd-utils i2c-tools"
否则
IMAGE_INSTALL = "busybox mtd-utils"
```

[BBPATH](https://docs.yoctoproject.org/bitbake/bitbake-user-manual/bitbake-user-manual-ref-variables.html#term-BBPATH)  
[BBFILES](https://docs.yoctoproject.org/bitbake/bitbake-user-manual/bitbake-user-manual-ref-variables.html#term-BBFILES)  
[LAYERDIR](https://docs.yoctoproject.org/bitbake/bitbake-user-manual/bitbake-user-manual-ref-variables.html#term-LAYERDIR)  
[BBFILE_COLLECTIONS](https://docs.yoctoproject.org/bitbake/bitbake-user-manual/bitbake-user-manual-ref-variables.html#term-BBFILE_COLLECTIONS)  

### Anonymous Python Functions
[Anonymous Python Functions](https://docs.yoctoproject.org/bitbake/bitbake-user-manual/bitbake-user-manual-metadata.html#anonymous-python-functions)
```python
# Anonymous Python functions always run at the end of parsing, regardless of  where they are defined. If a recipe contains many anonymous functions, they  run in the same order as they are defined within the recipe.

python () {
	if d.getVar('SOMEVAR') == 'value':
		d.setVar('ANOTHERVAR', 'value2')
}
```

### Inline Python Variable Expansion
```python
# You can use inline Python variable expansion to set variables, the most common use of this feature is to extract the value of variables from BitBake’s internal data dictionary, d.
DATE = "${@time.strftime('%Y%m%d',time.gmtime())}"

PN = "${@bb.parse.vars_from_file(d.getVar('FILE', False),d)[0] or 'defaultpkgname'}"
PV = "${@bb.parse.vars_from_file(d.getVar('FILE', False),d)[1] or '1.0'}"
```

[Poky recipes](https://git.yoctoproject.org/poky/plain/meta/recipes-extended/) #online  
[Yocto: Part 1 – A Definitive Introduction](https://kickstartembedded.com/2021/12/19/yocto-part-1-a-definitive-introduction/)  
[Debugging Tools and Techniques](https://docs.yoctoproject.org/dev-manual/debugging.html)  
[build/tmp/work/](https://docs.yoctoproject.org/3.2.3/ref-manual/ref-structure.html#build-tmp-work)  
[Variables Glossary](https://docs.yoctoproject.org/ref-manual/variables.html#variables-glossary)  
[OpenEmbedded User Manual](http://www.embeddedlinux.org.cn/OEManual/index.html)  
[Yocto Project Reference Manual](https://docs.yoctoproject.org/3.2.3/ref-manual/ref-manual.html)  
[Yocto project reference manual in 1 page](https://docs.yoctoproject.org/2.2/ref-manual/ref-manual.html)  
[yocto Technical Overview](https://yoctoproject.org/development/technical-overview/#getting-started)  
[Using   devtool   in Your SDK Workflow](https://docs.yoctoproject.org/sdk-manual/extensible.html#using-devtool-in-your-sdk-workflow)  
[devtool   Quick Reference](https://docs.yoctoproject.org/ref-manual/devtool-reference.html)  
[Yocto Source Repositories](https://git.yoctoproject.org/) #online  
[poky plain](https://git.yoctoproject.org/poky/plain) #online  
[poky source tree](https://git.yoctoproject.org/poky/tree/?h=kirkstone) #online  
[Override syntax changes](https://docs.yoctoproject.org/migration-guides/migration-3.4.html#override-syntax-changes)  
[Yocto Project Documentation](https://docs.yoctoproject.org/)  
[BitBake User Manual](https://docs.yoctoproject.org/bitbake/)  
[A practical guide to BitBake](https://a4z.gitlab.io/docs/BitBake/guide.html)  
[Variables Glossary](https://docs.yoctoproject.org/3.2.3/ref-manual/ref-variables.html)  
[bitbake.conf(常用变量定义的位置)](https://git.yoctoproject.org/poky/plain/meta/conf/bitbake.conf)  
recipe中用到的[[install]]命令  
[Building your own recipes from first principles](https://wiki.yoctoproject.org/wiki/Building_your_own_recipes_from_first_principles)  
[yocto TipsAndTricks](https://wiki.yoctoproject.org/wiki/TipsAndTricks)  
[Yocto Project Development Tasks Manual](https://docs.yoctoproject.org/dev/dev-manual/index.html)  
[Introduction of Yocto Project](https://www.codeinsideout.com/blog/yocto/introduction/)  
[What I wish I’d known about Yocto Project](https://docs.yoctoproject.org/what-i-wish-id-known.html)  
[bootlin yocto slides](https://bootlin.com/doc/training/yocto/yocto-slides.pdf)  
[yocto ref manual](https://docs.yoctoproject.org/ref-manual/index.html)  
[poky ref manual](https://docs.yoctoproject.org/1.0/poky-ref-manual/poky-ref-manual.html)  
[Beginners Guide to Writing a Recipe](https://www.wolfssl.com/docs/yocto-openembedded-recipe-guide/)  
[Directories: Installation variables](http://www.embeddedlinux.org.cn/OEManual/directories_installation.html)  
[man install](https://man7.org/linux/man-pages/man1/install.1.html)  
[First Yocto Project Build For QEMU (Quick Emulator)](https://tutorialadda.com/yocto/quick-start-your-first-yocto-project-build)  
[Yocto Project Overview and Concepts Manual](https://docs.yoctoproject.org/overview-manual/index.html)  
[Yocto Project Linux Kernel Development Manual](https://docs.yoctoproject.org/kernel-dev/index.html#yocto-project-linux-kernel-development-manual)  
[autotools class](http://www.embeddedlinux.org.cn/OEManual/chapter_reference.html#autotools_class)  
[update-rc.d class](http://www.embeddedlinux.org.cn/OEManual/update-rc-d_class.html)  
[embedded_linux_projects_using_yocto_project_cookbook](https://digiwiki.eccee.com/_media/digi/arm-embedded/linux/dey/embedded_linux_projects_using_yocto_project_cookbook.pdf) #pdf  
[yocto artifactory](https://artifactory.corp.vecima.com/ui/native/ext-release-local/yocto/downloads/)  
[Tasks](https://docs.yoctoproject.org/bitbake/2.6/bitbake-user-manual/bitbake-user-manual-metadata.html#tasks)  
[bitbake Multiconfig](https://elinux.org/images/a/a5/002-1500-SLIDES-multiconfig_inception.pdf)  
[BBMULTICONFIG](https://docs.yoctoproject.org/bitbake/2.6/bitbake-user-manual/bitbake-user-manual-ref-variables.html#term-BBMULTICONFIG)  
[Creating a Custom Template Configuration Directory](https://docs.yoctoproject.org/4.0.15/dev-manual/custom-template-configuration-directory.html#creating-a-custom-template-configuration-directory)  
[Embedded-Linux-Development-Using-Yocto-Project-Cookbook-Second-Edition code](https://github.com/PacktPublishing/Embedded-Linux-Development-Using-Yocto-Project-Cookbook-Second-Edition)  
[Useful bitbake commands](https://community.nxp.com/t5/i-MX-Processors-Knowledge-Base/Useful-bitbake-commands/ta-p/1128559)  
[Useful Bitbake commands](https://docs.ota.here.com/ota-client/latest/useful-bitbake-commands.html)  
[BitBake cheat sheet](https://wiki.stmicroelectronics.cn/stm32mpu/wiki/BitBake_cheat_sheet)  
[Bitbake Commands](https://backstreetcoder.com/bitbake-commands/)  
[oe-setup-builddir source code](https://git.yoctoproject.org/poky/plain/scripts/oe-setup-builddir)  
