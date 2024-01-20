- Application Binary Interface 定义了两个协同工作的**二进制**程序模块之间的接口规范，**它是编译器和链接器遵守的一组规则**，目的是让编译后的程序可以正常工作，ABI 包含很多方面的内容
	- 规定函数的调用顺序，即”调用约定“，调用约定标准化了如何将函数”转换“为汇编代码
	- 规定了公开的函数名称如`printf`如何表示，以便于在链接后可以正确的调用库函数并接收参数
	- 规定了可以使用的数据类型，以及如何对齐及其他细节
	- 规定了可执行文件格式，[[虚拟地址空间]]布局，以及程序加载及动态链接等内容
- [System V ABI](https://wiki.osdev.org/System_V_ABI)
  > The **System V Application Binary Interface** is a set of specifications that detail [calling conventions](https://wiki.osdev.org/Calling_Conventions), [object file formats](https://wiki.osdev.org/Object_Files), [executable file formats](https://wiki.osdev.org/Executable_Formats), dynamic linking semantics, and much more for systems that complies with the *X/Open Common Application Environment Specification* and the *System V Interface Definition*. It is today the standard ABI used by the major Unix operating systems such as Linux, the BSD systems, and many others. The [Executable and Linkable Format](https://wiki.osdev.org/Executable_and_Linkable_Format) (ELF) is part of the System V ABI.
-