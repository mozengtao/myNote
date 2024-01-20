- ```bash
  Tcl是一种基于字符串的命令式语言，基本的工作原理都和字符串和字符串替换相关，Tcl命令（built-in command or procedure）的基本语法
  command arg1 arg2 arg3 ...
  除了substitution之外，命令的参数都是string-valued
  在Tcl中，everything is a string，你需要explicitly ask for evaluation of variables and nested commands
  grouping 和 substitutions 是Tcl解释器运行命令之前采取的仅有的处理
  
  输出
  puts stdout {Hello, World!}
  (Curly braces are used to group words together into a single argument)
  
  变量赋值
  set var 5
  set b $var ($符号用来进行substitution)
  set len [string length foobar] ([...]用来进行command substitution)
  Tcl解释器把从左括号[开始到右括号]结束的所有字符看作一整块，之后用嵌套命令的结果对这一整块进行替换
  
  expr 用来计算 math expressions
  expr 7.2 / 3
  set tcl_precision用来设置数学表达式的计算精度
  set tcl_precision 17
  expr 1 / 3.0
  
  Backslash Substitution用来表示字符的literal meaning
  set dollar \$
  set x $dollar
  需要注意的是Tcl解释器只会进行一轮interpretation，这一点很重要
  set escape \0x1b
  set escape \033
  
  backslash可以用来跨行长命令的输入，即backslach作为一行的最后一个字符会被转换为空格，除此之外，下一行开头的空格会被substitution同时消化掉
  set totalLength [expr [string length $one] + \
  	[string length $two]]
  
  
  ```
- 参考文档
	- [TCL培训教程](https://jerkwin.github.io/2016/10/28/TCL%E5%9F%B9%E8%AE%AD%E6%95%99%E7%A8%8B/)
	- [Tcl/Tk 官网文档](https://www.w3cschool.cn/doc_tcl_tk/)
	- [Tcl Reference Manual](https://tmml.sourceforge.net/doc/tcl/index.html)
	- [Tcl Tutorial](https://www.tcl.tk/man/tcl8.5/tutorial/tcltutorial.html)
	- [Tcl Tutorial](https://wiki.tcl-lang.org/page/Tcl+Tutorial+Index)
	- [TCL 语言简介](https://yearn.xyz/posts/techs/tcl-%E8%AF%AD%E8%A8%80%E7%AE%80%E4%BB%8B/)
	- [Tcl Basics](http://www.beedub.com/book/2nd/booktoc.mif.html)
	- [Tcl tutorial](https://zetcode.com/lang/tcl/)
- [Practical Programming in Tcl and Tk](http://www.beedub.com/book/tkbook.pdf) #online #pdf