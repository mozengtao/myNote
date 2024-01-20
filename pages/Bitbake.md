- `source oe-init-build-env buildDir`
	- ```bash
	  How it works...
	  The oe-init-build-env script calls scripts/oe-setup-builddir script inside the Poky directory to 
	  create the build directory.
	  On creation, the build directory contains a conf directory with the following three files:
	  	bblayers.conf: This file lists the metadata layers to be considered for this project.
	  	local.conf: This file contains the project-specific configuration variables. You can set 
	      			common configuration variables to different projects with a site.conf file, 
	                  but this is not created by default. Similarly, there is also an auto.conf 
	                  file which is used by autobuilders. BitBake will first read site.conf, then 
	                  auto.conf, and finally local.conf.
	  	templateconf.cfg: This file contains the directory that includes the template ...
	  ```
- [oe-setup-builddir source code](https://git.yoctoproject.org/poky/plain/scripts/oe-setup-builddir)
- commands
	- ```bash
	  #通常把image的定义放在recipes-images目录下
	  morrism@morrism-kirkstone ~/vcmos (kirkstone) $ find . -name recipes-images
	  ./meta-saltspring/recipes-images
	  ./meta-dorado/recipes-images
	  ./meta-neptune/recipes-images
	  ./meta-java/recipes-images
	  ./meta-training/recipes-images
	  ./meta-vcore/recipes-images
	  ./meta-xpon-controller/recipes-images
	  ./meta-rphy/recipes-images
	  ./meta-vpon/recipes-images
	  
	  
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
	  ```
- BitBake is a generic ^^task execution engine^^ that allows ^^shell^^ and ^^Python^^ tasks to be run efficiently and in parallel while working within complex inter-task dependency constraints.
- BitBake is a program ^^written in the Python^^ language. At the highest level, BitBake interprets metadata, decides what tasks are required to run, and executes those tasks.
- Concepts
	- [[recipe]]
		- BitBake Recipes are the most basic metadata files.
	- Configuration files
		- Configuration files, which are denoted by the .conf extension, define various configuration variables that govern the project’s build process.
	- Classes
		- Class files contain information that is useful to share between metadata files.
		- The BitBake source tree currently comes with one class metadata file called base.bbclass.
		- The ^^base.bbclass^^  class files is special since it is ^^always included automatically for all recipes and classes^^.
		- The base.bbclass class contains definitions for standard basic tasks such as fetching, unpacking, configuring (empty by default), compiling (runs any Makefile present), installing (empty by default) and packaging (empty by default). These tasks are ^^often overridden or extended by other classes^^ added during the project development process
	- Layers
		- Layers allow you to ^^isolate^^ different types of customizations from each other.
	- Append files
		- Append files ^^extend or override^^ information in an existing recipe file
- Bitbake tasks
	- [Tasks](https://docs.yoctoproject.org/bitbake/2.6/bitbake-user-manual/bitbake-user-manual-metadata.html#tasks)
	- `bitbake package -c listtasks`
	- ```bash
	  $ bitbake trapd -c listtasks
	  
	  do_build                     Default task for a recipe - depends on all other normal tasks required to 'build' a recipe
	  do_buildclean                Call 'make clean' or equivalent in /home/morrism/vcmos_build/tmp-busybox/work/corei7-64-vcm-linux/trapd/0.0+git999-r0/trapd-0.0+git999
	  do_checkuri                  Validates the SRC_URI value
	  do_clean                     Removes all output files for a target
	  do_cleanall                  Removes all output files, shared state cache, and downloaded source files for a target
	  do_cleansstate               Removes all output files and shared state cache for a target
	  do_compile                   Compiles the source in the compilation directory
	  do_configure                 Configures the source by enabling and disabling any build-time and configuration options for the software being built
	  do_deploy_source_date_epoch
	  do_devshell                  Starts a shell with the environment set up for development/debugging
	  do_fetch                     Fetches the source code
	  do_generate_toolchain_file
	  do_install                   Copies files from the compilation directory to a holding area
	  do_listtasks                 Lists all defined tasks for a target
	  do_package                   Analyzes the content of the holding area and splits it into subsets based on available packages and files
	  do_package_qa                Runs QA checks on packaged files
	  do_package_write_rpm         Creates the actual RPM packages and places them in the Package Feed area
	  do_packagedata               Creates package metadata used by the build system to generate the final packages
	  do_populate_lic              Writes license information for the recipe that is collected later when the image is constructed
	  do_populate_sysroot          Copies a subset of files installed by do_install into the sysroot in order to make them available to other recipes
	  do_prepare_recipe_sysroot
	  do_pydevshell                Starts an interactive Python shell for development/debugging
	  do_test                      Saves the unit test reports
	  do_unpack                    Unpacks the source code into a working directory
	  
	  ```
- [bitbake Multiconfig](https://elinux.org/images/a/a5/002-1500-SLIDES-multiconfig_inception.pdf)
- [BBMULTICONFIG](https://docs.yoctoproject.org/bitbake/2.6/bitbake-user-manual/bitbake-user-manual-ref-variables.html#term-BBMULTICONFIG)
- [Creating a Custom Template Configuration Directory](https://docs.yoctoproject.org/4.0.15/dev-manual/custom-template-configuration-directory.html#creating-a-custom-template-configuration-directory)
- [Embedded-Linux-Development-Using-Yocto-Project-Cookbook-Second-Edition code](https://github.com/PacktPublishing/Embedded-Linux-Development-Using-Yocto-Project-Cookbook-Second-Edition)
- [Useful bitbake commands](https://community.nxp.com/t5/i-MX-Processors-Knowledge-Base/Useful-bitbake-commands/ta-p/1128559)
- [Useful Bitbake commands](https://docs.ota.here.com/ota-client/latest/useful-bitbake-commands.html)
- [BitBake cheat sheet](https://wiki.stmicroelectronics.cn/stm32mpu/wiki/BitBake_cheat_sheet)
- [Bitbake Commands](https://backstreetcoder.com/bitbake-commands/)