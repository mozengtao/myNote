# C语言编程指南

本文档是一个综合性的C语言编程资源集合，涵盖了C语言的各个方面，从基础语法到高级技术，从数据结构到设计模式，从系统编程到库函数使用。

## 目录

- [学习资源](#学习资源)
- [子文档链接](#子文档链接)
- [数据结构](#数据结构)
- [设计模式](#设计模式)
- [C工程技巧](#c工程技巧)
- [模块化开发](#模块化开发)
- [常用库](#常用库)
- [系统编程](#系统编程)
- [字符串处理](#字符串处理)
- [参考资料](#参考资料)

## 学习资源

### 在线编译器和教程
- [C Online Compiler](https://www.programiz.com/c-programming/online-compiler/) - 在线C编译器
- [va_list in C: exploring ft_printf](https://medium.com/@turman1701/va-list-in-c-exploring-ft-printf-bb2a19fcd128) - va_list详解
- [SystemProgramming Wiki](https://csresources.github.io/SystemProgrammingWiki/SystemProgramming/C-Programming,-Part-1:-Introduction/) - 系统编程Wiki
- [**Beej's Guide to C Programming**](https://beej.us/guide/bgc/html/split/index.html) - C编程指南

### 面向对象编程在C中的应用
- [**Object-oriented Programming with Ansi-C**](http://ebook.pldworld.com/-huihoo-/book/pdf/object-oriented-programming-with-ansi-c/) - C语言面向对象编程
- [Sources for "Object-Oriented Programming with ANSI-C"](https://github.com/shichao-an/ooc/tree/master) - 源码
- [Application Note Object-Oriented Programming in C](https://www.state-machine.com/doc/AN_Simple_OOP_in_C.pdf) - C语言OOP应用笔记
- [OOP-in-C](https://github.com/QuantumLeaps/OOP-in-C) - C语言OOP示例
- [Object-oriented techniques in C](https://dmitryfrank.com/articles/oop_in_c) - C语言面向对象技术
- [OBJECT-ORIENTED PROGRAMMING IN C](https://home.cs.colorado.edu/~kena/classes/5448/f12/presentation-materials/srivastava.pdf) - C语言OOP
- [Key Concept: Object-Oriented Programming](https://www.state-machine.com/oop) - OOP核心概念

### 嵌入式开发和状态机
- [EmbedSummary](https://github.com/zhengnianli/EmbedSummary) - 嵌入式开发总结
- [QuantumLeaps](https://github.com/QuantumLeaps) - 状态机框架
- [**Practical UML Statecharts in C/C++**](https://www.state-machine.com/category/books) - UML状态图实践
- [**Practical Statecharts in C/C++**](https://www.state-machine.com/category/books) - 状态图实践

### 推荐书籍
- 专业嵌入式软件开发:全面走向高质高效编程
- 程序员修炼之道：通向务实的最高境界
- 七周七并发模型

## 子文档链接

### 基础概念和语法
- [C语言中二级指针](./C语言中二级指针.md) - 二级指针详解
- [C语言函数参数顺序](c/function_param_sequence.md) - 函数参数设计原则
- [函数指针](c/function_pointer.md) - 函数指针的使用
- [常用函数](c/useful_functions.md) - C语言常用函数库
- [pthread](./pthread.md) - 多线程编程

### 编程技巧和最佳实践
- [variable/function naming](c/naming.md) - 变量和函数命名规范
- [减少函数嵌套层级的方法](c/methods_to_denest.md) - 代码优化技巧
- [机制与策略分离](c/mechanism_strategy.md) - 架构设计原则
- [macro + struct + function pointer的应用](c/macro_struct_pointer.md) - 高级技巧应用

### 数据结构和算法
- [hash](c/hash.md) - 哈希算法
- [哈希table](c/hash_table.md) - 哈希表实现
- [ring buffer](c/ring_buffer.md) - 环形缓冲区

### 架构设计和模式
- [组合模式](c/composition.md) - 组合模式应用
- [设计模式](c/design_patterns.md) - 设计模式总结
- [OOP](c/OOP.md) - C语言面向对象编程
- [依赖注入](c/inject.md) - 依赖注入模式
- [简化版插件架构示例](c/plugin_arch.md) - 插件架构设计
- [opaque类型](c/opaque_handle.md) - 不透明指针应用

### 系统编程
- [socket](c/socket.md) - 网络编程
- [动态链接和静态链接](c/static_danymic.md) - 链接技术
- [C语言可执行程序的数据段大小是如何确定的](c/how_segment_size_decided.md) - 程序内存布局

### 内存管理
- [内存池](c/mem_pool.md) - 内存池技术

## 数据结构

### 系列一：基础数据结构
- [索引](./c/data_structure/00-index.md) - 数据结构索引
- [动态数组](./c/data_structure/01-dynamic-array.md) - 动态数组实现
- [链表](./c/data_structure/02-linked-list.md) - 链表详解
- [哈希表](./c/data_structure/03-hash-table.md) - 哈希表实现
- [红黑树](./c/data_structure/04-red-black-tree.md) - 红黑树算法
- [B+树](./c/data_structure/05-bplus-tree.md) - B+树实现
- [栈和队列](./c/data_structure/06-stack-queue.md) - 栈和队列
- [环形缓冲区](./c/data_structure/07-ring-buffer.md) - 环形缓冲区

### 系列二：详细实现
- [数组](./c/data_structures/ds_01_array.md) - 数组操作
- [单向链表](./c/data_structures/ds_02_singly_linked_list.md) - 单向链表
- [双向链表](./c/data_structures/ds_03_doubly_linked_list.md) - 双向链表
- [侵入式链表](./c/data_structures/ds_04_intrusive_linked_list.md) - 侵入式链表
- [栈](./c/data_structures/ds_05_stack.md) - 栈的实现
- [队列](./c/data_structures/ds_06_queue.md) - 队列的实现
- [环形缓冲区](./c/data_structures/ds_07_ring_buffer.md) - 环形缓冲区详解
- [二叉树](./c/data_structures/ds_08_binary_tree.md) - 二叉树操作
- [二叉搜索树](./c/data_structures/ds_09_binary_search_tree.md) - BST算法
- [平衡树](./c/data_structures/ds_10_balanced_trees.md) - 平衡树系列
- [哈希表](./c/data_structures/ds_11_hash_table.md) - 哈希表详解
- [高级数据结构](./c/data_structures/ds_12_advanced_structures.md) - 高级结构

### 专项实现

#### 定时器模块
- [main.c](./c/timer/main.c) - 定时器主程序
- [timer.c](./c/timer/timer.c) - 定时器实现
- [timer.h](./c/timer/timer.h) - 定时器头文件

#### 内存池系统
- [nginx内存池参考](https://github.com/nginx/nginx) - Nginx内存池实现参考
- [为什么需要内存池](./c/mempool/01-WHY.md) - 内存池的必要性
- [如何实现内存池](./c/mempool/02-HOW.md) - 内存池实现方法
- [内存池是什么](./c/mempool/03-WHAT.md) - 内存池概念
- [内存池应用场景](./c/mempool/04-WHERE.md) - 使用场景分析
- [内存池陷阱](./c/mempool/05-PITFALLS.md) - 常见问题
- [内存池最佳实践](./c/mempool/06-TRANSFER.md) - 实践经验

#### 链表专题
- [单向链表](./link_list/singly_linked_list.md) - 单向链表详解
- [双向链表](./link_list/doubly_linked_list.md) - 双向链表详解

#### 树形结构
- [树形数据结构](./tree/tree_data_structures.md) - 树结构总览

### C语言分层架构
- [嵌入式C可复用技巧](./c_tips/embedded_c_reusability.md) - 可复用代码技巧
- [C语言软件分层架构（第一部分）](./c_tips/c_layer_interaction_part1.md) - 分层架构基础
- [C语言软件分层架构（第二部分）](./c_tips/c_layer_interaction_part2.md) - 分层架构进阶

## 设计模式

### 创建型模式
- [单例模式](./c_tips/patterns/singleton.md) - 单例模式实现
- [工厂模式](./c_tips/patterns/factory.md) - 工厂模式应用
- [对象池模式](./c_tips/patterns/object_pool.md) - 对象池管理

### 结构型模式
- [装饰器模式](./c_tips/patterns/decorator.md) - 装饰器模式
- [适配器模式](./c_tips/patterns/adapter.md) - 适配器模式

### 行为型模式
- [观察者模式](./c_tips/patterns/observer.md) - 观察者模式
- [状态机模式](./c_tips/patterns/state_machine.md) - 状态机设计
- [命令模式](./c_tips/patterns/command.md) - 命令模式
- [责任链模式](./c_tips/patterns/chain_of_responsibility.md) - 责任链模式
- [模板方法模式](./c_tips/patterns/template_method.md) - 模板方法模式
- [策略模式](./c_tips/patterns/strategy.md) - 策略模式

### 高级模式和组合
- [服务定位器模式](./c_tips/patterns/service_locator.md) - 服务定位器
- [依赖注入模式](./c_tips/patterns/dependency_injection.md) - 依赖注入
- [依赖注入 + 服务定位器 + 策略模式](./c_tips/patterns/DI_ServiceLoader_Strategy.md) - 综合应用

## C工程技巧

### 设计模式在C中的实现
- [单例模式](./c_tips/01_单例模式_Singleton.md) - 单例模式基础
- [单例模式进阶](./c_tips/01_单例模式_Singleton_Pattern.md) - 单例模式高级应用
- [工厂模式](./c_tips/02_工厂模式_Factory.md) - 工厂模式实现
- [适配器模式](./c_tips/03_适配器模式_Adapter.md) - 适配器模式应用
- [观察者模式](./c_tips/04_观察者模式_Observer.md) - 观察者模式设计
- [策略模式](./c_tips/05_策略模式_Strategy.md) - 策略模式实现
- [状态模式](./c_tips/06_状态模式_State.md) - 状态模式设计
- [回调模式](./c_tips/07_回调模式_Callback.md) - 回调机制实现

### 软件工程实践
- [C语言软件工程技巧](./c_tips/08_C语言软件工程技巧.md) - 软件工程最佳实践
- [不透明指针](./c_tips/09_不透明指针_Opaque_Pointer.md) - 封装技术
- [X-Macro技术](./c_tips/10_X-Macro技术.md) - 高级宏技术

### 内存管理和性能优化
- [内存池](./c_tips/11_内存池_Memory_Pool.md) - 内存池设计
- [侵入式数据结构](./c_tips/14_侵入式数据结构_Intrusive.md) - 高效数据结构

### 错误处理和调试
- [错误处理策略](./c_tips/12_错误处理策略_Error_Handling.md) - 异常处理机制
- [契约式编程](./c_tips/13_契约式编程_Design_by_Contract.md) - 防御性编程
- [编译时断言](./c_tips/15_编译时断言_Static_Assert.md) - 编译期检查
- [防御式编程](./c_tips/18_防御式编程_Defensive.md) - 安全编程实践

### 高级编程技巧
- [属性清理](./c_tips/16_属性清理_RAII_Cleanup.md) - 资源管理
- [泛型宏编程](./c_tips/17_泛型宏编程_Generic.md) - 泛型编程


## 模块化开发

### 模块化设计理论和实践
- [真刀真枪模块化](https://mp.weixin.qq.com/mp/appmsgalbum?__biz=MzAxMzc2ODMzNg==&action=getalbum&album_id=1588719988331364355&scene=173&subscene=&sessionid=svr_839904b1464&enterid=1767926105&from_msgid=2656102469&from_itemidx=1&count=3&nolastread=1#wechat_redirect) - 模块化实战指南
- [GorgonMeducer](https://github.com/GorgonMeducer) - 模块化设计专家

### 嵌入式系统模块化
- [Device OS文档](https://docs.particle.io/getting-started/device-os/introduction-to-device-os/) - Particle设备操作系统
- [Device OS源码](https://github.com/particle-iot/device-os) - 嵌入式设备操作系统源码

## 常用库

### JSON处理库
- [jansson](https://github.com/akheron/jansson) - C语言JSON编解码库
- [json-c](https://github.com/json-c/json-c) - 引用计数的JSON对象模型库

### 网络编程库
- [libpcap](https://github.com/the-tcpdump-group/libpcap) - 数据包捕获库接口
- [czmq](https://github.com/zeromq/czmq) - ØMQ的高级C绑定
- [curl](https://github.com/curl/curl) - URL数据传输工具和库
- [grpc](https://github.com/grpc/grpc) - 基于C++的gRPC库
- [librdkafka](https://github.com/confluentinc/librdkafka) - Apache Kafka C/C++库

### 安全和加密库
- [openssl](https://github.com/openssl/openssl) - TLS/SSL和加密库

### 数据序列化库
- [protobuf](https://github.com/protocolbuffers/protobuf) - Google Protocol Buffers

### 系统工具库
- [collectd](https://github.com/collectd/collectd) - 系统信息收集守护进程
- [iproute2](https://github.com/iproute2/iproute2) - Linux网络工具集

### XML处理库
- [libxml2](https://github.com/GNOME/libxml2) - GNOME项目XML工具包

### 测试和调试库
- [cpputest](https://github.com/cpputest/cpputest) - C/C++单元测试和模拟框架
- [lcov](https://github.com/linux-test-project/lcov) - 代码覆盖率测试工具


### 编程规范和指南
- [C系统编程指南](./c/c_systems_programming_guidelines.md) - C语言系统编程最佳实践

### 架构学习案例

#### Raylib架构分析
- [Raylib项目](https://github.com/raysan5/raylib) - 清洁的C库分层子系统设计
- [从Raylib学习架构](./c/raylib_ARCHITECTURE_CASE_STUDY.md) - 游戏引擎架构案例研究

#### SQLite架构分析  
- [SQLite分层架构分析](./c/sqlite_LAYERED_ARCHITECTURE_ANALYSIS.md) - 数据库引擎架构分析
- [C架构中的边界和契约](./c/sqlite_boundaries_and_contracts_in_c.md) - 从SQLite学习边界设计

## 系统编程

### C运行时和启动过程
- [Understanding the C Runtime: crt0, crt1, crti, and crtn](https://www.inferara.com/en/blog/c-runtime/) - C运行时详解
- [C/C++ Runtime Startup](https://etherealwake.com/2021/09/crt-startup/) - 运行时启动过程
- [Explanation of crt0, crt1, crt1, and crtn](https://thejat.in/learn/explanation-of-crt0-crt1-crt1-and-crtn-c-runtime) - C运行时组件解释
- [CRT: C Run Time Before Starting main()](https://vishalchovatiya.com/posts/crt-run-time-before-starting-main/) - main函数前的运行时

### 动态链接和符号解析
- [Linux-dlsym详解](https://lambertxiao.github.io/posts/linux-dlsym/doc/) - 动态符号查找
- [真正理解RTLD_NEXT的作用](https://csstormq.github.io/) - 符号解析机制
- [Dynamic Linking Example](https://vishalchovatiya.com/posts/dynamic-linking-example/) - 动态链接示例
[How to make a system call in C](https://jameshfisher.com/2018/02/19/how-to-syscall-in-c/)  
[How do I call a program from C?](https://jameshfisher.com/2017/02/07/how-do-i-call-a-program-in-c/)  
[How do I call a program in C, setting up standard pipes?](https://jameshfisher.com/2017/02/17/how-do-i-call-a-program-in-c-with-pipes/)  
[How do I use fork in C?](https://jameshfisher.com/2017/02/06/how-do-i-use-fork-in-c/)  
[What is a “file descriptor”, really?](https://jameshfisher.com/2016/12/15/file-descriptor-misnomer/)  
[What does getaddrinfo do?](https://jameshfisher.com/2018/02/03/what-does-getaddrinfo-do/)  
[What is a a FIFO, or “named pipe”? What is mkfifo in C?](https://jameshfisher.com/2017/02/21/mkfifo-c/)  
[What is static linking in C?](https://jameshfisher.com/2017/08/26/static-linking/)  
[What are setjmp and longjmp in C?](https://jameshfisher.com/2017/02/18/what-are-setjmp-longjmp/)  
[How to generate Intel and AT&T assembly with clang](https://jameshfisher.com/2017/02/19/clang-assembly-att-intel/)  
[How do I duplicate a file descriptor in C?](https://jameshfisher.com/2017/02/15/c-dup-syscall/)  
[How do I use execve in C?](https://jameshfisher.com/2017/02/05/how-do-i-use-execve-in-c/)  
[How do I generate assembly from a C file?](https://jameshfisher.com/2017/02/03/c-generate-assembly/)  
[How can I read a file with mmap in C?](https://jameshfisher.com/2017/01/27/mmap-file-read/)  
[What is realloc in C?](https://jameshfisher.com/2016/12/01/c-realloc/)  
[What is mmap in C?](https://jameshfisher.com/2017/01/26/mmap/)  
[How can I write a file with mmap in C?](https://jameshfisher.com/2017/01/28/mmap-file-write/)  
[In what ways can processes communicate?](https://jameshfisher.com/2017/01/29/process-communication-methods/)  
[What are ‘signals’ in C?](https://jameshfisher.com/2017/01/09/c-signals/)  
[What does the C signal function return?](https://jameshfisher.com/2017/01/10/c-signal-return-value/)  
[How do I unregister a signal handler in C?](https://jameshfisher.com/2017/01/11/c-signal-unregister/)  
[What does void mean as a function parameter in C?](https://jameshfisher.com/2016/11/27/c-void-params/)  
[What are lvalue and rvalue in C?](https://jameshfisher.com/2017/01/21/c-lvalue-rvalue/)  
[What are the domain and type arguments to the socket system call?](https://jameshfisher.com/2017/02/27/socket-types/)  
[Variadic Function Working in C](https://www.thejat.in/blog/variadic-function-working-in-c)  
[Understanding Complex C/C++ Declarations](https://www.thejat.in/blog/understanding-complex-cc-declarations)  
[Exploring Singleton Pattern in C++: Ensuring Unique Instances](https://www.thejat.in/blog/exploring-singleton-pattern-in-c-ensuring-unique-instances)  
[]()  


## 字符串处理

### 标准库函数
- [Library Functions](https://www.ibm.com/docs/en/i/7.4.0?topic=functions-library) - IBM标准库函数文档
- [String and Array Utilities](https://www.gnu.org/software/libc/manual/html_node/String-and-Array-Utilities.html) - GNU C库字符串工具
- [Standard C Library Functions Table, By Name](https://www.ibm.com/docs/en/i/7.5.0?topic=extensions-standard-c-library-functions-table-by-name) - 标准库函数表

### 字符串分析和解析
- [The String Span Functions](https://c-for-dummies.com/blog/?p=5068) - 字符串范围函数
- [Parsing Words with the strspn() Function](https://c-for-dummies.com/blog/?p=5072) - 使用strspn()解析单词
- [Slicing Strings with strsep()](https://c-for-dummies.com/blog/?p=1769) - strsep()字符串切片
- [How to Use strtok and strtok_r in C](https://systems-encyclopedia.cs.illinois.edu/articles/c-strtok/) - strtok和strtok_r使用指南
- [strtok详解](https://icarus.cs.weber.edu/~dab/cs1410/textbook/8.Strings/more_cstring.html) - strtok函数详细说明

### 字符串处理资源
- [The Standard C Library for Linux, part 7: String Handling](https://linuxgazette.net/issue76/rogers.html) - Linux C库字符串处理
- [C Programming/String manipulation](https://en.wikibooks.org/wiki/C_Programming/String_manipulation) - C语言字符串操作Wiki
- [String Functions in C (Full List With Examples)](https://www.wscubetech.com/resources/c-programming/string-functions) - C字符串函数完整列表

## 专用C库

### 模板和容器库
- [ctl](https://github.com/glouw/ctl) - C模板库（快速编译、类型安全、仅头文件）
- [C CONTAINER TEMPLATE LIBRARY (CTL)](https://rurban.github.io/ctl/) - C容器模板库文档

### 配置文件解析库
- [iniparser](https://github.com/ndevilla/iniparser/tree/main) - INI文件解析器
- [inih](https://github.com/benhoyt/inih) - 简单的INI解析器

### 用户界面库
- [Notcurses: blingful TUIs and character graphics](https://github.com/dankamongmen/notcurses) - 终端用户界面和字符图形库

## 参考资料

### 官方文档和标准

[**Extensions to the C Language Family**](https://gcc.gnu.org/onlinedocs/gcc-12.2.0/gcc/C-Extensions.html)  
[GNU C Language Manual](https://www.gnu.org/software/c-intro-and-ref/manual/html_node/index.html#SEC_Contents)  
[Online Python to C Converter](https://www.codeconvert.ai/python-to-c-converter)  
[The GNU C Reference Manual](https://www.gnu.org/software/gnu-c-manual/gnu-c-manual.html)  
[C Operator Precedence](https://en.cppreference.com/w/c/language/operator_precedence)  
[**The GNU C Library**](https://www.gnu.org/software/libc/manual/html_node/index.html#SEC_Contents) #online  
[**The GNU C Library**](http://herbert.the-little-red-haired-girl.org/html/glibc/libc_toc.html)  
[**The GNU C Library (glibc) manual**](https://sourceware.org/glibc/manual/)  
[c函数使用参考实例](https://bytefreaks.net/category/programming-2/c-programming-2)  
[POXIS Function Reference Example](https://support.sas.com/documentation/onlinedoc/sasc/doc/lr2/lrv2ch20.htm#lr2posix)  
[C standard](https://www.open-std.org/jtc1/sc22/wg14/www/standards.html)  
[**glibc source code**](https://elixir.bootlin.com/glibc/latest/source) #sourcecode  
[The GNU C Library](https://www.gnu.org/software/libc/manual/html_mono/libc.html)  
[Memory Layout Of A C Program](https://hackthedeveloper.com/memory-layout-c-program/)  
[C Program Compilation Process](https://hackthedeveloper.com/c-program-compilation-process/)  
[C 语言常见问题集](https://c-faq-chn.sourceforge.net/ccfaq/ccfaq.html)  
[Notes for C and C++ Programming](https://www.codeinsideout.com/blog/c-cpp/notes/)  
[c for dummies](https://c-for-dummies.com/)  
[C and C++ Projects](https://www.codewithc.com/c-projects-with-source-code/)  
[How to split/tokenize a string? (strtok() string tokenizer)](https://www.equestionanswers.com/c/c-strtok.php)  
[declaring a flexible array member](https://wiki.sei.cmu.edu/confluence/display/c/DCL38-C.+Use+the+correct+syntax+when+declaring+a+flexible+array+member)  
[How to interpret complex C/C++ declarations](https://www.codeproject.com/Articles/7042/How-to-interpret-complex-C-C-declarations)  
[Reading C type declarations](http://unixwiz.net/techtips/reading-cdecl.html)  
[cdecl.org](https://cdecl.org/)  

### 系统编程教程
- [Signal Handling](https://www.gnu.org/software/libc/manual/html_node/Signal-Handling.html) - 信号处理
- [Introduction To Unix Signals Programming](https://www.cs.kent.edu/~ruttan/sysprog/lectures/signals.html) - Unix信号编程入门
- [CS 43203 : System Programming](https://www.cs.kent.edu/~ruttan/sysprog/) - 系统编程课程
- [Unix Pthreads tutorial](https://www.cs.kent.edu/~ruttan/sysprog/) - Unix线程教程

### 泛型编程和高级技术
- [Using Templates and Generics in C](https://levelup.gitconnected.com/using-templates-and-generics-in-c-968da223154d) - C语言模板和泛型
- [Tutorial: Generics in C](https://itnext.io/tutorial-generics-in-c-b3362b3376a3) - C语言泛型教程
- [Generic Programming in C](https://cs.boisestate.edu/~amit/teaching/253/handouts/07-c-generic-coding-handout.pdf) - C语言泛型编程
- [void * and Function Pointers](https://web.stanford.edu/class/archive/cs/cs107/cs107.1202/lab4/) - void指针和函数指针
- [Functional Pointer and Callback in C++](https://primerpy.medium.com/functional-pointer-and-callback-in-c-86c208df6b2f) - 函数指针和回调

### 高级语言特性
- [Arrays of Length Zero](https://gcc.gnu.org/onlinedocs/gcc/Zero-Length.html) - 零长度数组
- [Bounded Flexible Arrays in C](https://people.kernel.org/kees/bounded-flexible-arrays-in-c) - 有界灵活数组
- [Flexible Array Members for C++](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1039r0.html) - 灵活数组成员
- [Zero-length arrays in C have to go at the end of the struct](https://shivankaul.com/blog/zero-length-arrays) - 零长度数组位置规则
- [How to Use typeof, Statement Expressions and Block-Scope Label Names](https://www.oracle.com/solaris/technologies/c-type.html) - typeof和语句表达式

### 学习资源和教程
- [**Programming in C**](https://users.cs.cf.ac.uk/dave/C/) - C语言编程教程
- [The Unofficial C For Dummies Website](https://c-for-dummies.com/) - C语言学习网站

### 工具和实用程序
- [HOWTO: Use Address Sanitizer](https://www.osc.edu/resources/getting_started/howto/howto_use_address_sanitizer) - 地址净化器使用指南
- [Clang 19.0.0git documentation](https://clang.llvm.org/docs/index.html) - Clang编译器文档
- [C and C++ Language Syntax Reference](https://www.cprogramming.com/reference/) - C/C++语法参考
- [Understanding the LD_LIBRARY_PATH Environment Variable](https://tecladmin.net/understanding-the-ld_library_path-environment-variable/) - LD_LIBRARY_PATH环境变量

### 项目和资源集合
- [Awesome C](https://github.com/oz123/awesome-c?tab=readme-ov-file#awesome-c) - 优秀C语言资源集合
- [C Projects Repository](https://github.com/marcotrosi/C) - C语言项目集合
- [C program to implement linked list](https://www.programmingsimplified.com/c/data-structures) - 数据结构实现示例  
