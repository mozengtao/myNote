- Tips
```bitbake
1.DEPENDS
Lists a recipe’s build-time dependencies. These are dependencies on other recipes whose contents (e.g. headers and shared libraries) are needed by the recipe at build time.

// build-time dependency
DEPENDS = "bar"
The practical effect of the previous assignment is that all files installed by bar will be available in the appropriate staging sysroot, given by the STAGING_DIR* variables, by the time the do_configure task for foo runs.

 // build 过程需要用到(依赖于) 宿主机 的 utilities
 DEPENDS = "codegen-native"
 DEPENDS can also be used to add utilities that run on the build machine during the build. 

 https://docs.yoctoproject.org/ref-manual/variables.html#term-DEPENDS
 https://docs.yoctoproject.org/ref-manual/classes.html#ref-classes-native
```
- [Images](https://docs.yoctoproject.org/ref-manual/images.html#images)
	- ```bash
	  #1 bitbake 执行命令时出现 "Resource Temporarily Unavailable"
	  通过如下命令进行恢复
	  rm build/bitbake.lock
	  
	  #display the list of directories within the Source Directory that contain image recipe files
	  morrism@morrism-kirkstone ~/vcmos/poky ((HEAD detached at 1520bf97aa)) $ pwd
	  /home/morrism/vcmos/poky
	  morrism@morrism-kirkstone ~/vcmos/poky ((HEAD detached at 1520bf97aa)) $ ls meta*/recipes*/images/*.bb
	  meta-selftest/recipes-test/images/error-image.bb                      meta/recipes-core/images/core-image-tiny-initramfs.bb
	  meta-selftest/recipes-test/images/oe-selftest-image.bb                meta/recipes-extended/images/core-image-full-cmdline.bb
	  meta-selftest/recipes-test/images/test-empty-image.bb                 meta/recipes-extended/images/core-image-kernel-dev.bb
	  meta-selftest/recipes-test/images/wic-image-minimal.bb                meta/recipes-extended/images/core-image-testcontroller-initramfs.bb
	  meta-skeleton/recipes-multilib/images/core-image-multilib-example.bb  meta/recipes-extended/images/core-image-testcontroller.bb
	  meta/recipes-core/images/build-appliance-image_15.0.0.bb              meta/recipes-graphics/images/core-image-weston-sdk.bb
	  meta/recipes-core/images/core-image-base.bb                           meta/recipes-graphics/images/core-image-weston.bb
	  meta/recipes-core/images/core-image-minimal-dev.bb                    meta/recipes-graphics/images/core-image-x11.bb
	  meta/recipes-core/images/core-image-minimal-initramfs.bb              meta/recipes-rt/images/core-image-rt-sdk.bb
	  meta/recipes-core/images/core-image-minimal-mtdutils.bb               meta/recipes-rt/images/core-image-rt.bb
	  meta/recipes-core/images/core-image-minimal.bb                        meta/recipes-sato/images/core-image-sato-dev.bb
	  meta/recipes-core/images/core-image-ptest-all.bb                      meta/recipes-sato/images/core-image-sato-sdk.bb
	  meta/recipes-core/images/core-image-ptest-fast.bb                     meta/recipes-sato/images/core-image-sato.bb
	  
	  ```
- yocto project
	- ![image.png](../assets/image_1671676755810_0.png){:height 515, :width 776}
- 构建系统工作流程
	- ![_images/yp-how-it-works-new-diagram.png](https://docs.yoctoproject.org/_images/yp-how-it-works-new-diagram.png)
		- Fetch – 获取源代码
		- Extract – 解压源代码
		- Patch – 应用补丁
		- Configure – 配置编译环境
		- Build – 编译和链接
		- Install – 拷贝文件到目标目录
		- Package – 打包安装文件
- Yocto项目核心组件
	- BitBake
		- 构建引擎，任务调度器，它负责解释配置文件和recipes并执行一系列的任务，来下载，配置，编译特定的应用和文件系统镜像。
	- OpenEmbedded-Core
		- 一系列基础层的集合，它包含了一系列基础的recipes，layers，classess，这些组件在所有的嵌入式系统之间是共享的。
	- Poky
		- 参考发行系统，它是一系列项目和工具的集合，用来引导构建基于Yocto项目的发行版。
- OpenEmbedded-Core组织结构
	- Recipe
		- 描述如何获取，配置，编译和打包应用和镜像，有特定的语法
	- Layer
		- Layer由一系列的Recipe组成，通常为了实现一个通用的目的，如meta-ti-bsp
		- 同一个发行版根据需求可能存在多个Layer
	- 支持ARM，MIPS，PowerPC，RISC-V和x86结构
	- 支持QEMU模拟的以上架构
- 配置编译系统
	- build/conf
		- bblayers.conf
			- 显式指定需要用到的Layer列表
		- local.conf
			- 设置当前用户相关的配置变量，配置变量可以在这里进行覆写。
- 变量
	- OpenEmbedded使用配置变量保存信息
	- 通常变量名称用大写字母表示，如CONF_VERSION
	- 变量的值是字符串
	- 可以对变量进行添加，追加和定义
	- 定义在配置文件(.conf)中的变量具有全局作用域
	  background-color:: yellow
	- 定义在Recipe(.bb, .bbappend, .bbclass)中的变量聚有本地作用域
	  background-color:: yellow
	- Recipe可以访问全局变量
	- 所有的变量值可以被`$BUILDDIR/conf/local.conf`覆写或修改
	- 变量Override
	  background-color:: red
		- `<VARIABLE>:<override> = "some_value"`
		- append override
			- 变量末尾追加(不包含空格)
			- `IMAGE_INSTALL:append = " dropbear"` adds dropbear to the packages installed
			  on the image
		- prepend override
			- 变量开头追加(不包含空格)
			- `FILESEXTRAPATHS:prepend := "${THISDIR}${PN}` adds the folder to the set of
			  paths where files are located (in a recipe).
		- remove override
			- 移除变量里出现的所有指定值
			- `IMAGE_INSTALL:remove = "i2c-tools"`
		- 有条件的override
			- 追加machine名称只会定义针对特定machine的配置变量
			- 它尝试匹配OVERRIDES中包含的MACHINE，SOC_FAMILY等定义的值
			- 如果该override在OVERRIDES中，则应用赋值，否则赋值被忽略
				- ```bash
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
	- 变量赋值顺序
		- ![image.png](../assets/image_1671694224440_0.png)
- Package
	- virtual package
		- 一些package有相同的目的，同时只能使用其中一个
		- virtual package 用来描述功能，若干个package都可以用来提供该功能
		- 只有一个提供该功能的package会被编译并集成到最终的image当中
		- virtual package的形式如下`virtual/<name>`
			- virtual/bootloader: u-boot, u-boot-ti-staging…
			- virtual/kernel: linux-yocto, linux-yocto-tiny, linux-yocto-rt, linux-ti-staging…
			- virtual/libc: glibc, musl, newlib
			- virtual/xserver: xserver-xorg
		- 具体哪一个package variant被指定提供virtual package功能，通过PREFERRED_PROVIDER来指定，package name作为该变量后缀来指定variant
			- ```bash
			  PREFERRED_PROVIDER_virtual/kernel ?= "linux-ti-staging"
			  PREFERRED_PROVIDER_virtual/libgl = "mesa
			  ```
		- Bitbake默认会选取最高优先级的Layer，最高version number的provider进行编译，除非该recipe定义了变量`DEFAULT_PREFERENCE ="-1"`
		- 当多个package version可用时，可以通过变量`PREFERRED_VERSION`来显式指定一个provider
		- %可以用作通配符
			- ```bash
			  PREFERRED_VERSION_nginx = "1.20.1"
			  PREFERRED_VERSION_linux-yocto = "5.14%"
			  ```
		- 最终镜像所安装的package通过target来指定(例如core-image-minimal)
		- 具体安装的package可以通过IMAGE_INSTALL配置变量的来指定
		- 需要安装的package可以通过PACKAGE_EXCLUDE来过滤，如果使用该变量来指定不安装某个package，但是其他package又依赖该package，则会出现错误。该规则只适合于RPM和IPK安装包
- Recipe
	- ![image.png](../assets/image_1671698126913_0.png)
		- Recipe定义了如何处理一个特定的应用
		- Recipe是一一系列指令，用来描述针对特定应用如何retrieve, patch, compile, install和产生二进制安装包
		- Recipe定义了编译和运行时依赖
		- Recipe文件名称通常为`<application-name>_<version>.bb`
		- Recipe可以包含称为task的函数如fetch, configure, compile…
		- Recipe自动定义了一些变量
		  background-color:: pink
			- PN
				- package name，在recipe文件名称中指定
			- BPN
				- 移除PN的前缀和后缀之后的结果，如nativesdk-，或者-native
			- PV
				- package version，在recipe文件名称中指定
			- PR
				- package revision，默认为r0
			- BP
				- 定义为`${BPN}-${PV}`
			- ```bash
			  如果recipe名称为bash_5.1.bb
			  则
			  ${PN} = "bash"
			  ${PV} = "5.1"
			  ```
		- Recipe的name和version通常和上游的名称相匹配
- Recipe组织结构
	- `<application>.inc`
		- 公共的metadata
	- `<application>_<version>.bb`
		- `require <application>.inc`，以及定义特定版本的metadata
	- recipe可以分为3个部分
		- header
			- `what/who`
		- souces
			- where
		- tasks
			- how
	- SRC_URI用来定义从哪里以及如果获取源文件，URI scheme语法如下`scheme://url;param1;param2`，用来指向本地或者远端的资源路径
		- ```bash
		  SRC_URI = "http://example.com/src.tar.bz2;name=tarball \
		  		   http://example.com/fixes.patch;name=patch"
		  SRC_URI[tarball.md5sum] = "97b2c3fb082241ab5c56..."
		  SRC_URI[patch.md5sum] = "b184acf9eb39df794ffd..."
		  ```
	- 源文件默认被放在`$BUILDDIR/downloads`目录，可以通过`conf/local.conf`文件中的DL_DIR变量进行更改
	- 所有通过SRC_URI指定的本地文件都被拷贝到工作目录`$BUILDDIR/tmp/work/`
	- FILESPATH变量用来指定本地文件的搜索路径
		- ```bash
		  FILESPATH = "${@base_set_filespath(["${FILE_DIRNAME}/${BP}",
		  		"${FILE_DIRNAME}/${BPN}","${FILE_DIRNAME}/files"], d)}
		  FILESOVERRIDES = "${TRANSLATED_TARGET_ARCH}:
		  		${MACHINEOVERRIDES}:${DISTROOVERRIDES}"
		  ```
		- `base_set_filespath(path)`函数使用path参数，以及FILESEXTRAPATHS 和FILESOVERRIDES 来填充 FILESPATH 变量
		- 用户自定义的文件或者路径可以通过FILESEXTRAPATHS 和FILESOVERRIDES进行添加，注意路径顺序
		- 当提取tarball文件时，Bitbake期望在目录名称为`<application>-<version>`的目录找到解压文件，它是通过S变量来指定的，如果目录为别的名称，可以通过显式指定S变量来指定查找目录
	- license file
		- license文件有自己的checksum，用来跟踪license文件的更新
			- ```bash
			  LIC_FILES_CHKSUM = "file://gpl.txt;md5=393a5ca..."
			  LIC_FILES_CHKSUM = \
			  	"file://main.c;beginline=3;endline=21;md5=58e..."
			  LIC_FILES_CHKSUM = \
			  	"file://${COMMON_LICENSE_DIR}/MIT;md5=083..."
			  ```
	- recipe的依赖
		- 编译时依赖
			- DEPENDS
				- ```bash
				  DEPENDS = "recipe-b"
				  本地do_prepare_recipe_sysroot任务依赖于recipe-b的do_populate_sysroot任务
				  ```
		- 运行时依赖
			- RDEPENDS，必须作用域特定的package(e.g. with: ${PN})
				- ```bash
				  RDEPENDS:${PN} = "package-b"
				  本地do_build任务依赖recipe b的do_package_write_<archive-format>任务
				  ```
		- recipe依赖于另一个recipe的特定版本
			- ```bash
			  DEPENDS = "recipe-b (>= 1.2)"
			  RDEPENDS:${PN} = "recipe-b (>= 1.2)"
			  ```
		- 检查依赖或者反向依赖的图形化工具
			- ```bash
			  bitbake -g -u taskexp core-image-minimal
			  ```
	- task
		- 默认存在的task，它们被定义在class中
			- do_fetch
			- do_unpack
			- do_patch
			- do_configure
			- do_compile
			- do_install
			- do_package
			- do_rootfs
		- 使用bitbake命令列出recipe存在的task
			- `bitbake <recipe> -c listtasks`
		- recipe中的函数使用sh shell语法
			- 常用变量
				- WORKDIR
					- recipe的工作目录
				- S
					- 源码的提取或者解压目录
				- B
					- bitbake构建期间放置产生的对象的目录
				- D
					- 产生镜像之前，文件安装的目的根目录
				- ```bash
				  do_install() {
				  	install -d ${D}${bindir}
				  	install -m 0755 hello ${D}${bindir}
				  }
				  ```
			- 主要的task
				- ![image.png](../assets/image_1671777418204_0.png)
			- task扩展
				- 通过:prepend或者:append进行task扩展
					- ```bash
					  do_install:append() {
					  	install -d ${D}${sysconfdir}
					  	install -m 0644 hello.conf ${D}${sysconfdir}
					  }
					  ```
				- 通过addtask进行任务添加
					- ```bash
					  do_mkimage () {
					  	uboot-mkimage ...
					  }
					  
					  addtask do_mkimage after do_compile before do_install
					  ```
	- 应用patch
		- 文件以.patch，.diff结尾，或者有`apply=yes`参数设置，则在do_patch任务当中会被当做patch进行应用
			- ```bash
			  SRC_URI += "file://joystick-support.patch \
			  			file://smp-fixes.diff \
			  			"
			  ```
		- patch按照在SRC_URI出现的顺序进行应用
		- 通过在SRC_URI变零当中指定PATCHTOOL对应用patch的工具进行指定，可选的工具有git，patch，和quilt，默认的工具为`PATCHTOOL = 'quilt'`
		- PATCHRESOLVE变量用来决定当应用patch出现冲突时如何处理
			- noop
				- 构建失败如果出现冲突
			- user
				- 在新的shell下用户手动解决冲突
			- 默认在meta-poky下，`PATCHRESOLVE = "noop"`
- Recipe示例
	- ```bash
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
	  ```
	- ```bash
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
	  ```
- 扩展已有的Recipe
	- ![image.png](../assets/image_1672021163653_0.png){:height 299, :width 594}
	- recipe扩展文件以.bbappend结尾
	- recipe扩展文件必须与它要扩展的recipe文件root name相同，但是可以使用通配符
		- ```bash
		  example_0.1.bbappend是example_0.1的扩展文件
		  example_0.%.bbappend适用于example_0.1.bb和exmaple_0.2.bb，但是不适用于example_1.0.bb
		  ```
	- recipe扩展文件适合于特定的版本，如果recipe更新到一个新的版本，则扩展文件必须同时更新
	- 如果需要添加新的文件，则文件所在的目录必须追加到FILESEXTRAPATHS变量里
		- 文件的查找顺序按照FILESEXTRAPATHS出现的从左到右的顺序进行查找
		- 追加到FILESEXTRAPATHS的文件路径必须具有较高的优先级来保证之前的文件被覆盖
			- ```bash
			  FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
			  
			  SRC_URI += "file://custom-modification-0.patch \
			  	    file://custom-modification-1.patch \
			  	    "
			  ```
- recipe高级配置
	- 现实世界中的recipe比较复杂
		- 可能包含virtual package
			- BitBake允许使用virtual package name代替实际的package name
			- virtual package 名称通过PROVIDES变量指定
			- 若干个recipe可以提供相同的 virtual name，但是只有一个会被构建并安装到产生的image当中
				- `PROVIDES = "virtual/kernel"`
		- 可能从class继承通用的处理函数
- Class
	- class提供公共代码的抽象实现，可以在多个recipe内重复使用
	- class扩展.bbclass
	- class文件放在layer的classes目录
	- recipe可以通过继承class来使用公共的code
		- `inherit <class>`
	- 公共的class文件位于`meta/classes/`目录下
		- base.bbclass
		- kernel.bbclass
		- autotools.bbclass
		- autotools-brokensep.bbclass
		- cmake.bbclass
		- native.bbclass
		- systemd.bbclass
		- update-rc.d.bbclass
		- useradd.bbclass
		- …
	- 每个recipe都自动继承自base class
	- 用默认实现定义基本的公共任务
		- fetch，unpack，patch
		- configure，compile，install
		- 工具箱任务如clean，listtasks
	- 自动应用SRC_URI中列出的patch文件
	- 定义mirrors SOURCEFORGE_MIRROR DEBIAN_MIRROR, GNU_MIRROR,
	  KERNELORG_MIRROR…
	- 定义oe_runmake, 使用 EXTRA_OEMAKE 来使用自定义参数
	- 用来编译linux kernel
	- 定义task来配置，编译，安装linux kernel以及内核模块
	- kernel被分为若干个package：kernel, kernel-base, kernel-dev,
	  kernel-modules…
	- 自动提供 virtual package `virtual/kernel`
	- 可用的配置变量
		- KERNEL_IMAGETYPE, 默认为 zImage
		- KERNEL_EXTRA_ARGS
		- INITRAMFS_IMAGE
	- 定义 task 和 metadata 来使用自动化编译工具(autoconf, automake and libtool)
		- do_configure: 使用autoreconf 产生配置脚本并以标准参数和交叉编译载入
		- do_compile: 运行make
		- do_install: 运行make install
	- 额外的参数可以通过EXTRA_OECONF指定
	- 编译选项可以通过EXTRA_OEMAKE指定
		- ```bash
		  DESCRIPTION = "Print a friendly, customizable greeting"
		  HOMEPAGE = "https://www.gnu.org/software/hello/"
		  PRIORITY = "optional"
		  SECTION = "examples"
		  LICENSE = "GPL-3.0-or-later"
		  
		  SRC_URI = "${GNU_MIRROR}/hello/hello-${PV}.tar.gz"
		  SRC_URI[md5sum] = "67607d2616a0faaf5bc94c59dca7c3cb"
		  SRC_URI[sha256sum] = "ecbb7a2214196c57ff9340aa71458e1559abd38f6d8d169666846935df191ea7"
		  LIC_FILES_CHKSUM = "file://COPYING;md5=d32239bcb673463ab874e80d47fae504"
		  
		  inherit autotools
		  ```
	- useradd class
		- useradd class帮助用户添加自定义用户到最终的image，从而避免以root用户运行
		- 当继承useradd class时，必须定义USERADD_PACKAGES来包含需要该用户的package列表
		- 用户和组在package执行do_install之前被创建
		- 需要至少设置如下两个参数之一
			- USERADD_PARAM
				- 传递给useradd的参数
			- GROUPADD_PARAM
				- 传递给groupadd的参数
		- ```bash
		  DESCRIPTION = "useradd class usage example"
		  PRIORITY = "optional"
		  SECTION = "examples"
		  LICENSE = "MIT"
		  
		  SRC_URI = "file://file0"
		  LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/MIT;md5=0835ade698e0bc..."
		  
		  inherit useradd
		  
		  USERADD_PACKAGES = "${PN}"
		  USERADD_PARAM = "-u 1000 -d /home/user0 -s /bin/bash user0"
		  
		  do_install() {
		  	install -m 644 file0 ${D}/home/user0/
		  	chown user0:user0 ${D}/home/user0/file0
		  }
		  ```
- Binary package
	- 用于只安装已经编译好的二进制文件到最终的根文件系统中
	- bin_package.bbclass
		- Disables do_configure 和 do_compile
		- 提供一个默认的do_install 用来拷贝S中的所有文件
	- 额外可能需要做的
		- 设置LICENSE to CLOSED if applicable
		- 可能需要`inherit allarch`
- [[Bitbake]]文件包含
	- Metadata可以通过被包含在不同文件之间共享
	- BitBake使用BBPATH和当前目录来查找被包含的文件
	- 包含文件的关键字
		- inherit
			- 在recipe和class中使用，用来继承class中定义的功能
				- `inherit kernel`用来进程kernel classh中定义的功能
			- inherit在BBPATH中找到的classes目录中查找以.bbclass结尾的文件
			- 可以使用使用变量有条件的包含class如`inherit ${FOO}`
		- include
		- require
			- include和require可以在所有文件中使用，用来插入另一个文件的内容
			- 如果文件路径是相对的，BitBake会插入在BBPATH找到的第一个文件
			- 当文件找不到时include不会产生错误，require会触发解析错误
			- 包含本地文件`include ninvaders.inc`
			- 从其他位置包含文件`require path/to/file.inc`
- Recipe Debugging
	- 对于每一个task，在temp目录下都有关于这个recipe的log
	- bitbake可以dump整个环境
		- `bitbake -e ninvaders`
	- 开发shell可以用来到处整个环境来debug编译失败的场景
		- `bitbake -c devshell <recipe>`
	- 为了理解recipe修改前后的含义，可以在local.conf文件中使能build history
		- ```bash
		  INHERIT += "buildhistory"
		  BUILDHISTORY_COMMIT = "1"
		  ```
		- 之后使用buildhistory-diff工具来检查两次构建的差异
			- `buildhistory-diff`
	-
- 配置变量
	- [BBPATH](https://docs.yoctoproject.org/bitbake/bitbake-user-manual/bitbake-user-manual-ref-variables.html#term-BBPATH)
	- [BBFILES](https://docs.yoctoproject.org/bitbake/bitbake-user-manual/bitbake-user-manual-ref-variables.html#term-BBFILES)
	- [LAYERDIR](https://docs.yoctoproject.org/bitbake/bitbake-user-manual/bitbake-user-manual-ref-variables.html#term-LAYERDIR)
	- [BBFILE_COLLECTIONS](https://docs.yoctoproject.org/bitbake/bitbake-user-manual/bitbake-user-manual-ref-variables.html#term-BBFILE_COLLECTIONS)
	-
### Anonymous Python Functions
	- [Anonymous Python Functions](https://docs.yoctoproject.org/bitbake/bitbake-user-manual/bitbake-user-manual-metadata.html#anonymous-python-functions)
	- ```python
	  Anonymous Python functions always run at the end of parsing, regardless of 
	  where they are defined. If a recipe contains many anonymous functions, they 
	  run in the same order as they are defined within the recipe.
	  
	  python () {
	      if d.getVar('SOMEVAR') == 'value':
	          d.setVar('ANOTHERVAR', 'value2')
	  }
	  ```
### Inline Python Variable Expansion
	- You can use inline Python variable expansion to set variables, the most common use of this feature is to extract the value of variables from BitBake’s internal data dictionary, d.
	- ```python
	  DATE = "${@time.strftime('%Y%m%d',time.gmtime())}"
	  
	  PN = "${@bb.parse.vars_from_file(d.getVar('FILE', False),d)[0] or 'defaultpkgname'}"
	  PV = "${@bb.parse.vars_from_file(d.getVar('FILE', False),d)[1] or '1.0'}"
	  ```
- 参考文档
	- [Poky recipes](https://git.yoctoproject.org/poky/plain/meta/recipes-extended/) #online
	- [Yocto: Part 1 – A Definitive Introduction](https://kickstartembedded.com/2021/12/19/yocto-part-1-a-definitive-introduction/)
	- [Debugging Tools and Techniques](https://docs.yoctoproject.org/dev-manual/debugging.html)
	- [build/tmp/work/](https://docs.yoctoproject.org/3.2.3/ref-manual/ref-structure.html#build-tmp-work)
	- [Variables Glossary](https://docs.yoctoproject.org/ref-manual/variables.html#variables-glossary)
	- [OpenEmbedded User Manual](http://www.embeddedlinux.org.cn/OEManual/index.html)
	- [Yocto Project Reference Manual](https://docs.yoctoproject.org/3.2.3/ref-manual/ref-manual.html)
	- [Yocto project reference manual in 1 page](https://docs.yoctoproject.org/2.2/ref-manual/ref-manual.html)
	- [yocto Technical Overview](https://yoctoproject.org/development/technical-overview/#getting-started)
	- [Using   devtool   in Your SDK Workflow](https://docs.yoctoproject.org/sdk-manual/extensible.html#using-devtool-in-your-sdk-workflow)
	- [devtool   Quick Reference](https://docs.yoctoproject.org/ref-manual/devtool-reference.html)
	- [Yocto Source Repositories](https://git.yoctoproject.org/) #online
	- [poky plain](https://git.yoctoproject.org/poky/plain) #online
	- [poky source tree](https://git.yoctoproject.org/poky/tree/?h=kirkstone) #online
	- [Override syntax changes](https://docs.yoctoproject.org/migration-guides/migration-3.4.html#override-syntax-changes)
	- [Yocto Project Documentation](https://docs.yoctoproject.org/)
	- [BitBake User Manual](https://docs.yoctoproject.org/bitbake/)
	- [A practical guide to BitBake](https://a4z.gitlab.io/docs/BitBake/guide.html)
	- [Variables Glossary](https://docs.yoctoproject.org/3.2.3/ref-manual/ref-variables.html)
	- [bitbake.conf(常用变量定义的位置)](https://git.yoctoproject.org/poky/plain/meta/conf/bitbake.conf)
	- recipe中用到的[[install]]命令
	- [Building your own recipes from first principles](https://wiki.yoctoproject.org/wiki/Building_your_own_recipes_from_first_principles)
	- [yocto TipsAndTricks](https://wiki.yoctoproject.org/wiki/TipsAndTricks)
	- [Yocto Project Development Tasks Manual](https://docs.yoctoproject.org/dev/dev-manual/index.html)
	- [Introduction of Yocto Project](https://www.codeinsideout.com/blog/yocto/introduction/)
	- [What I wish I’d known about Yocto Project](https://docs.yoctoproject.org/what-i-wish-id-known.html)
	- [bootlin yocto slides](https://bootlin.com/doc/training/yocto/yocto-slides.pdf)
	- [yocto ref manual](https://docs.yoctoproject.org/ref-manual/index.html)
	- [poky ref manual](https://docs.yoctoproject.org/1.0/poky-ref-manual/poky-ref-manual.html)
	- [Beginners Guide to Writing a Recipe](https://www.wolfssl.com/docs/yocto-openembedded-recipe-guide/)
	- [Directories: Installation variables](http://www.embeddedlinux.org.cn/OEManual/directories_installation.html)
	- [man install](https://man7.org/linux/man-pages/man1/install.1.html)
	- [First Yocto Project Build For QEMU (Quick Emulator)](https://tutorialadda.com/yocto/quick-start-your-first-yocto-project-build)
	- [Yocto Project Overview and Concepts Manual](https://docs.yoctoproject.org/overview-manual/index.html)
	- [Yocto Project Linux Kernel Development Manual](https://docs.yoctoproject.org/kernel-dev/index.html#yocto-project-linux-kernel-development-manual)
	- classes
		- [autotools class](http://www.embeddedlinux.org.cn/OEManual/chapter_reference.html#autotools_class)
		- [update-rc.d class](http://www.embeddedlinux.org.cn/OEManual/update-rc-d_class.html)
			- [[update-rc.d]]
	- [embedded_linux_projects_using_yocto_project_cookbook](https://digiwiki.eccee.com/_media/digi/arm-embedded/linux/dey/embedded_linux_projects_using_yocto_project_cookbook.pdf) #pdf
- Vecima
	- [yocto artifactory](https://artifactory.corp.vecima.com/ui/native/ext-release-local/yocto/downloads/)