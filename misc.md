- [**An online IDE to edit, compile and run code**]https://www.mycompiler.io/)
- [online compiler](https://rextester.com/)
- [online compiler](https://www.studytonight.com/code/playground/go/)
- [online compiler](https://www.programiz.com/golang/online-compiler/)
- [Try It Online](https://tio.run/#) #a family of online interpreters
- [learnbyexample](https://learnbyexample.github.io/tags/regular-expressions/)
- [learnbyexample tips](https://learnbyexample.github.io/tips/)
- [learnbyexample tags](https://learnbyexample.github.io/tags/)
- [Structure and Interpretation of Computer Programs](https://web.mit.edu/6.001/6.037/sicp.pdf) #pdf #online
- [snippet](https://github.com/xgfone/snippet) #online #github
- eklitzke.org
	- [Using Ptrace For Fun And Profit](https://eklitzke.org/ptrace)
- [第4章 ext文件系统机制原理剖析](https://www.cnblogs.com/f-ck-need-u/p/7016077.html)
- changes `tmpfs` size **without need to unmount partition and hence not disturbing running apps**
	- ```bash
	  sudo mount -o remount,size=1048576 /tmp
	  
	  root@slot-120:/var/volatile [P2]# cat /etc/fstab
	  # gainspeed fstab
	  
	  /dev/root            /                    auto       defaults              1  1
	  proc                 /proc                proc       defaults              0  0
	  devpts               /dev/pts             devpts     mode=0620,gid=5       0  0
	  usbdevfs             /proc/bus/usb        usbdevfs   noauto                0  0
	  tmpfs                /run                 tmpfs      mode=0755,nodev,nosuid,strictatime,size=400M 0  0
	  tmpfs                /var/volatile        tmpfs      defaults,size=400M              0  0
	  
	  
	  root@slot-120:/var/volatile [P2]# df -h
	  Filesystem      Size  Used Avail Use% Mounted on
	  ...
	  tmpfs           400M  143M  258M  36% /var/volatile
	  ...
	  
	  root@slot-120:/var/volatile [P2]# mount -o remount,size=524288000 /var/volatile/
	  root@slot-120:/var/volatile [P2]# df -h
	  Filesystem      Size  Used Avail Use% Mounted on
	  ...
	  tmpfs           500M  144M  357M  29% /var/volatile
	  ...
	  ```
- # [Essential Programming Books](https://www.programming-books.io/) #online
- [books](https://github.com/ffisk/books) #online
- [Programming Books](https://www.theinsaneapp.com/programming) #online
- [free-programming-books](https://github.com/EbookFoundation/free-programming-books) #online
- [源码解读poll/select内核机制](http://gityuan.com/2019/01/05/linux-poll-select/)
- [Additional C/C++ Tooling](https://nickdesaulniers.github.io/blog/2015/07/23/additional-c-slash-c-plus-plus-tooling/)
- [The True Story of   Hello World](https://lisha.ufsc.br/teaching/os/exercise/hello.html)
- [Let's Write Some X86-64](https://nickdesaulniers.github.io/blog/2014/04/18/lets-write-some-x86-64/)
- [Intro to Debugging X86-64 Assembly](https://nickdesaulniers.github.io/blog/2016/01/20/debugging-x86-64-assembly-with-lldb-and-dtrace/)
- [Object Files and Symbols](https://nickdesaulniers.github.io/blog/2016/08/13/object-files-and-symbols/)
- [Directly Access Your Physical Memory](https://bakhi.github.io/devmem/)
- [bootlin docs](https://bootlin.com/doc/legacy/)
- [linux文件权限](https://www.cnblogs.com/LittleHann/p/3862652.html)
- [Dynamic Tracing with DTrace & SystemTap](https://myaut.github.io/dtrace-stap-book/index.html)
- [Linux 阅码场公众号文章收集](https://github.com/yifengyou/LinuxDev)
- [What is the Time](https://www.itdev.co.uk/blog/what-time-part-1)
- [Kernel modules](https://linux-kernel-labs.github.io/refs/heads/master/labs/kernel_modules.html) #lkm #linux
- [The Linux Kernel Module Programming Guide](https://tldp.org/LDP/lkmpg/2.6/html/index.html) #linux #lkm
- [The Linux Kernel Module Programming Guide](https://tldp.org/LDP/lkmpg/2.4/lkmpg.pdf) #linux #lkm
- [Operating Systems 2](https://linux-kernel-labs.github.io/refs/heads/master/so2/index.html) #linux
- [Linux kernel coding style](https://www.kernel.org/doc/html/v4.10/process/coding-style.html) #kernel
- [helpmanual.io](https://helpmanual.io/) #manual #linux
- [The Linux Kernel API]([https://kernelbook.sourceforge.net/](https://kernelbook.sourceforge.net/kernel-api.html/)) #manual #kernel
- pdf 资源
	- [Computer-PDF](https://www.computer-pdf.com/) #pdf
- 博客
	- [图解系统](https://xiaolincoding.com/os/)
	- [jerry_chg 的博客](https://jerry-cheng.blog.csdn.net/?type=blog)#csdn #kernel
	- [LINUX阅码场](https://cloud.tencent.com/developer/column/77185)
	- [泰晓科技](https://tinylab.org/categories/)
	- [宋宝华](https://blog.csdn.net/21cnbao?type=blog)
	- [裸机思维](https://cloud.tencent.com/developer/column/88423)
	- [编程珠玑](https://cloud.tencent.com/developer/column/78593) #tolearn
	- [CSAPP](https://wdxtub.com/categories/CSAPP/) #tolearn
	- [linux 相关](https://fanlv.fun/tags/Linux/) #tolearn
	- [StormQ's Blog](https://csstormq.github.io/) #linux
	- [Many But Finite](https://manybutfinite.com/archives/)
	- [linux-insides](https://0xax.gitbooks.io/linux-insides/content/)
	- [全栈参考](https://he.zhiqiang.cloud/)
	- [二进制安全学习笔记](https://binhack.readthedocs.io/zh/latest/index.html)
	- [Lan switching fundamentals](http://etutorials.org/Networking/Lan+switching+fundamentals/)
	- [Embedded Linux Systems](http://etutorials.org/Linux+systems/embedded+linux+systems/)
	- [**蜗窝科技**](http://www.wowotech.net/)
	- [Jason note](https://jasonblog.github.io/note/index.html) #linux #driver
	- [Lecture Slides and Videos](https://ops-class.org/slides/#_top) #linux
	- [linux kernel wiki](https://github.com/0voice/linux_kernel_wiki) #linux
	- [酷壳-CoolShell](https://coolshell.cn/tag/linux) #linux
	- [开发者内功修炼](https://github.com/yanfeizhang/coder-kung-fu)
- 书籍
	- [Linux Kernel Development (3rd Edition)](https://www.doc-developpement-durable.org/file/Projets-informatiques/cours-&-manuels-informatiques/Linux/Linux%20Kernel%20Development,%203rd%20Edition.pdf)
	- [Linux内核完全注释(修正版v3.0)](https://github.com/Yuandong-Chen/Linux-0.11/blob/master/Linux%E5%86%85%E6%A0%B8%E5%AE%8C%E5%85%A8%E6%B3%A8%E9%87%8A(%E4%BF%AE%E6%AD%A3%E7%89%88v3.0).pdf)
	- [嵌入式Linux应用开发完全手册](http://book.100ask.net/) #linux
	- [嵌入式 Linux 应用开发完全手册](http://www.wenqujingdian.com/Public/editor/attached/file/20180326/20180326211028_64401.pdf) #linux
	- [Multithreaded Programming Guide](http://ccfit.nsu.ru/~fat/pthreads/816-5137.pdf) #pdf
	- [Introduction to RAW-sockets](https://tuprints.ulb.tu-darmstadt.de/6243/1/TR-18.pdf) #pdf
	- [深入理解计算机系统（Computer Systems A Programmer’s Perspective）](https://hansimov.gitbook.io/csapp/) #online
	- [Linux 内核揭密](https://www.cntofu.com/book/114/index.html)
	- [操作系统思考](https://www.cntofu.com/book/31/index.html)
	- [程序员杂谈](https://www.cntofu.com/book/142/free-programming-books-zh.md#%E7%A8%8B%E5%BA%8F%E5%91%98%E6%9D%82%E8%B0%88)
	- [Mastering Embedded Linux Programming](http://centaur.sch.bme.hu/~holcsik_t/upload/Mastering%20Embedded%20Linux%20Programming%20-%20Second%20Edition.pdf) #pdf
	- [Essential Linux Device Drivers](http://www.staroceans.org/kernel-and-driver/2008%20Essential%20Linux%20Device%20Drivers%3B%20Sreekrishnan%20Venkateswaran%20(Prentice%20Hall%2C%202008%2C%20CHMtoPDF).pdf) #pdf #driver
	- [Essential Linux Device Drivers](http://www.embeddedlinux.org.cn/EssentialLinuxDeviceDrivers/) #online
	- [linux device drivers](https://www.nxp.com/files-static/soft_dev_tools/doc/ref_manual/Linux%20Device%20Drivers.pdf) #pdf
	- [C++ Primer](https://zhjwpku.com/assets/pdf/books/C++.Primer.5th.Edition_2013.pdf) #c++ #pdf #online
	- [LDD3](https://www.iitg.ac.in/asahu/cs421/books/LDD3e.pdf)
	- [Essential C++](http://jxz1.j9p.com/pc/dfhghgh.pdf) #online #pdf
	- [The Linux Programming Interface](https://static1.squarespace.com/static/59c4375b8a02c798d1cce06f/t/59cfb6a032601e11ca5b1cbe/1506784947301/The+Linux+Programming+Interface.pdf) #linux #online
	- [Computer Science from the Bottom Up](https://www.cs.swarthmore.edu/~kwebb/cs31/s15/bucs/index.html)
	- [Operating Systems: Three Easy Pieces](https://pages.cs.wisc.edu/~remzi/OSTEP/)
	- [**Operating Systems: Three Easy Pieces中文版**](https://pages.cs.wisc.edu/~remzi/OSTEP/Chinese/)
	- [Python 3 Object Oriented Programming](https://psi1.ir/wp-content/uploads/2021/09/Python-3-Object-Oriented-Programming-Dusty-Phillips-2010.pdf) #python
	- [The Object-Oriented Thought Process](https://coddyschool.com/upload/Addison_Wesley_The_Object_Orient.pdf)
	- [Linux内核设计与实现](https://awesome-programming-books.github.io/linux/Linux%E5%86%85%E6%A0%B8%E8%AE%BE%E8%AE%A1%E4%B8%8E%E5%AE%9E%E7%8E%B0%EF%BC%88%E7%AC%AC%E4%B8%89%E7%89%88%EF%BC%89.pdf)
	- [Understanding Linux Network Internals](http://www.embeddedlinux.org.cn/linux_net/0596002556/toc.html) #tcpip
	- [TCP/IP Architecture, Design, and Implementation in Linux](https://www.academia.edu/39957211/TCP_IP_ARCHITECTURE_DESIGN_AND_IMPLEMENTATION_IN_LINUX) #tcpip
	- Linux Kernel Networking: Implementation and Theory #tcpip
	- [The Linux Kernel Module Programming Guide](http://www.embeddedlinux.org.cn/linuxmod/) #lkm
	- [Embedded Linux Primer: A Practical, Real-World Approach](http://www.embeddedlinux.org.cn/EmbeddedLinuxPrimer/) #linux
	- [Embedded Linux Primer](http://www.mosaic-industries.com/embedded-systems/_media/pdfs/white-papers/embedded-linux-primer.pdf) #linux
	- [[Linux 环境编程：从应用到内核]] #linux
	- [Understanding the Linux Kernel](https://doc.lagout.org/operating%20system%20/linux/Understanding%20Linux%20Kernel.pdf) #linux
	- [Understanding the Linux Kernel](http://gauss.ececs.uc.edu/Courses/e4022/code/memory/understanding.pdf) #linux
	- [程序员的自我修养：链接、装载与库](https://awesome-programming-books.github.io/others/%E7%A8%8B%E5%BA%8F%E5%91%98%E7%9A%84%E8%87%AA%E6%88%91%E4%BF%AE%E5%85%BB%EF%BC%9A%E9%93%BE%E6%8E%A5%E3%80%81%E8%A3%85%E8%BD%BD%E4%B8%8E%E5%BA%93.pdf)
- 文章
	- [深入理解 Linux 虚拟内存管理](https://www.cnblogs.com/binlovetech/p/16824522.html)
	- [深入浅出计算机组成原理](https://fanlv.fun/2020/07/16/computer-organization/)
	- [Linux内核虚拟地址空间](https://fanlv.fun/2021/07/25/linux-mem/)
	- [Linux0.11启动过程](https://fanlv.fun/2022/12/25/linux-start-up/)
	- [eBPF 核心技术与实战](https://fanlv.fun/2022/11/05/study-ebpf/)
	- [内存管理、寻址方式那些事](https://fanlv.fun/2018/11/08/memory-page-and-addressing/)
	- [Page Cache](https://fanlv.fun/2020/09/13/linux-in-action/)
	- [Linux性能优化实战](https://fanlv.fun/2020/09/13/linux-optimize/)
	- [LLVM编译器基础知识](https://fanlv.fun/2022/11/27/study-llvm/)
	- [从内核角度看IO模型](https://www.cnblogs.com/binlovetech/p/16439838.html)
	- [Linux小记](https://www.zhyingkun.com/markdown/linuxnote/)
	- [How Computers Boot Up](https://manybutfinite.com/post/how-computers-boot-up/)
- 知识点
	- [C语言FAQ](https://c-faq.com/index.html)
	- [Linux Storage](http://linux-training.be/storage/index.html)
- 文档
	- [Linux工具快速教程](https://linuxtools-rst.readthedocs.io/zh_CN/latest/index.html)
	- [Linux 命令大全](https://hezhiqiang.gitbook.io/linux/)
	- [OpenBSD manual page server](https://man.openbsd.org/)
	- [Coreutils](https://www.gnu.org/software/coreutils/) #sourcecode
	- [Decoded: GNU coreutils](https://www.maizure.org/projects/decoded-gnu-coreutils/)
- Others
	- [Reversing the VKSI2000 Hand-Held Trading Device](https://sockpuppet.org/issue-79-file-0xb-foxport-hht-hacking.txt.html)
- 在线资源
	- [microcorruption online debugger](https://microcorruption.com/debugger/Tutorial) #online
	- [Online Source Code for The Linux Programming Interface](https://man7.org/tlpi/code/online/index.html) #sourcecode
- 其他
	- [KernelNewbies](https://kernelnewbies.org/Linux_Kernel_Newbies)
	- [Linux Kernel Teaching](https://linux-kernel-labs.github.io/refs/heads/master/)
	- [Linux Documentation Project Guides](https://tldp.org/guides.html)