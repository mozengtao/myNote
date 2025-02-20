- ```bash
  morrism@localhost ~/repos/uboot/u-boot-2023.01 $ make distclean
    CLEAN   spl/u-boot.cfg
    CLEAN   u-boot.cfg
    CLEAN   scripts/basic
    CLEAN   scripts/kconfig
    CLEAN   include/config include/generated spl
    CLEAN   .config .config.old include/autoconf.mk include/autoconf.mk.dep include/config.h
  
  morrism@localhost ~/repos/uboot/u-boot-2023.01 $ make zynq_cse_nand_defconfig
    HOSTCC  scripts/basic/fixdep
    HOSTCC  scripts/kconfig/conf.o
    YACC    scripts/kconfig/zconf.tab.c
    LEX     scripts/kconfig/zconf.lex.c
    HOSTCC  scripts/kconfig/zconf.tab.o
    HOSTLD  scripts/kconfig/conf
  #
  # configuration written to .config
  #
  
  通过如下命令
  make zynq_cse_nand_defconfig -p > /tmp/uboot_config.log
  可以将config的依赖关系及详细过程记录下来便于分析
  
  /tmp/uboot_config.log:
  /*
  morrism@localhost ~/repos/uboot/u-boot-2023.01 $ realpath Kconfig
  /home/morrism/repos/uboot/u-boot-2023.01/Kconfig
  */
  
  Kconfig := Kconfig
  
  %_defconfig: scripts/kconfig/conf
          $(Q)$< $(silent) --defconfig=arch/$(SRCARCH)/configs/$@ $(Kconfig)
  
  %_config: %_defconfig
          @:
  
  
  zynq_cse_nand_defconfig: scripts/kconfig/conf
          $(Q)$< $(silent) --defconfig=arch/$(SRCARCH)/configs/$@ $(Kconfig)
  
  scripts/kconfig/conf: FORCE scripts/kconfig/conf.o scripts/kconfig/zconf.tab.o
          $(call if_changed,host-cmulti)
  
  scripts/kconfig/conf.o: scripts/kconfig/conf.c FORCE
          $(call if_changed_dep,host-cobjs)
  
  scripts/kconfig/zconf.tab.o: scripts/kconfig/zconf.tab.c FORCE scripts/kconfig/zconf.lex.c
          $(call if_changed_dep,host-cobjs)
  
  ```
- [uboot顶层makefile走读](https://blog.csdn.net/alickr/article/details/124641722)
- [U-Boot 顶层 Makefile 分析](http://www.mrchen.love/Article/ID/49)
- [U-Boot 顶层 Makefile 详解](https://blog.nowcoder.net/n/f27bf274986443acbc4ebd414ab40dae)
- [uboot makefile构建分析](https://www.cnblogs.com/rongpmcu/p/7662791.html)
- [uboot makefile构建分析-续](https://www.cnblogs.com/rongpmcu/p/7662794.html)