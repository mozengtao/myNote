- [**Debugging by Patrick Horgan**](http://dbp-consulting.com/tutorials/debugging/)
- [Basic Assembler Debugging with GDB](http://dbp-consulting.com/tutorials/debugging/basicAsmDebuggingGDB.html)
- [**Programming From The Ground Up**](http://savannah.nongnu.org/projects/pgubook/) #pdf

![Programming From The Ground Up](./assets/ProgrammingGroundUp.pdf)

- [onlinegdb](https://www.onlinegdb.com/)

- [gdb tips](https://breezetemple.github.io/tags/gdb/)

- [Extending GDB using Python](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Python.html)
- [GDB 自动化操作的技术](https://segmentfault.com/a/1190000005367875)
	- [用 Python 拓展 GDB 1](https://segmentfault.com/a/1190000005718889)
	- [用 Python 拓展 GDB 2](https://segmentfault.com/a/1190000005732816)
	- [用 Python 拓展 GDB 3](https://segmentfault.com/a/1190000005750456)
	- [用 Python 拓展 GDB 4](https://segmentfault.com/a/1190000005772472)

- 用 python 拓展 GDB
	```python
	# 启用 python 扩展
	(gdb) python import gdb
	(gdb)

	# example.py
	import gdb

	# Define a new GDB command
	class ExampleCommand(gdb.Command):
		def __init__(self):
			super(ExampleCommand, self).__init__("example-command", gdb.COMMAND_USER)

		def invoke(self, args, from_tty):
			print("Example command executed with arguments:", args)

	# Add the new command to GDB
	ExampleCommand()

	# Execute some custom action before program runs
	def my_init():
		print("Debugger initialized")

	gdb.events.new_objfile.connect(my_init)

	# Execute some custom action when program execution stops
	def my_exit(event):
		print("Debugger exiting")

	gdb.events.exited.connect(my_exit)

	# 加载 python 脚本 example.py
	(gdb) source example.py
	(gdb)

	# 执行 debug 命令
	(gdb) example-command hello
	Example command executed with arguments: hello
	```

- [GDB扩展脚本](https://breezetemple.github.io/2020/05/07/Extending-GDB/)
- gdb函数
	```gdb
	1.使用关键字 define xxx 定义名称为 xxx 的函数
	2.在函数的 body 里定义 xxx 执行的 gdb 命令
	3.函数的第 1 个参数为 arg0，第 2 个参数为 arg1, ...
	4.关键字 end 用来结束函数定义

	# 1
	define adder
		print $arg0 + $arg1 + $arg2
	end

	document adder
	Syntax: adder arg1 arg2 arg3
	end

	(gdb) help adder
	Syntax: adder arg1 arg2

	(gdb) adder 1 2 3

	# 2
	define adder
		set $i = 0
		set $sum = 0

		while $i < $argc
			eval "set $sum = $sum + $arg%d", $i
			set $i = $i + 1
		end

		print $sum
	end

	使用 set $var = xxx 定义变量，变量的类型不受约束，可以是调试代码中的结构体等复杂数据类型
	对于自增操作，只能使用 $var = $var + 1
	eval template, expression 先对 template 做格式化处理，之后调用 eval 进行执行

	```

- 本地使用交叉编译的gdb
	- ```bash
	  /opt/fsl/3.1/sysroots/x86_64-fslsdk-linux/usr/bin/powerpc-fsl-linux/powerpc-fsl-linux-gdb 
	  	-iex "add-auto-load-safe-path /opt/fsl/3.1/sysroots/ppce6500-fsl-linux/lib" 
	  	-iex "set sysroot /opt/fsl/3.1/sysroots/ppce6500-fsl-linux" 
	  	cms 
	  	core.cms.3327
	  ```
- 常用命令
	```gdb
		# 查看当前寄存器的值
		info registers
		info registers rbp
		info registers rsp

		# 设置gdb prompt msg
		set prompt \033[31mgdb$ \033[0m
		
		#统计函数的执行时间，判断程序执行的瓶颈
		(gdb) disassemble /m cpecntrefresh
		之后在函数的开始和结束地址处添加断点(或者通过在函数的入口{和出口处添加断点})
		break *addrStart
		break *addrEnd
		之后通过对指点断点添加命令来统计函数的执行时间
		command 1
		shell echo $(date +%s.%N) > /tmp/funcStartTime
		continue
		end
		
		command 2
		shell echo $(date +%s.%N) > /tmp/funcEndTime
		continue
		end
		
		
		backtrace full: Complete backtrace with local variables
		up, down, frame: Move through frames
		watch: Suspend the process when a certain condition is met
		set print pretty on: Prints out prettily formatted C source code
		set logging on: Log debugging session to show to others for support
		set print array on: Pretty array printing
		finish: Continue till end of function
		enable and disable: Enable/disable breakpoints
		tbreak: Break once, and then remove the breakpoint
		where: Line number currently being executed
		info locals: View all local variables
		info args: View all function arguments
		list: view source
		rbreak: break on function matching regular expression
		
		info functions [REGEXP] to list all defined functions or whose matching REGEXP
		info address SYMBOL to find address of SYMBOL
		info symbol ADDR to display the name of the symbol residing at a given address ADDR
		
		(gdb) info line send_control_packets
		Line 1017 of "upstream_upper_mac.c" starts at address 0x127cf22 <us_upper_mac_run+146> and ends at 0x127cf25 <us_upper_mac_run+149>.
		Line 822 of "downstream_lower_mac.c" starts at address 0x1236934 <ds_lower_mac_run+148> and ends at 0x123693d <ds_lower_mac_run+157>.
		
		(gdb) info source
		Current source file is /usr/include/rte_ring_generic_pvt.h
		......
	```
	```bash
	  # 查看变量类型
	  (gdb) ptype i
	  
	  # Set pretty formatting of structures (美化打印输出)
	  (gdb) set print pretty
	  
	  # 重新从头开始下一次的debug
	  (gdb) start
	  
	  # 查看当前代码执行的语句
	  (gdb) where
	  
	  # 显示所有栈帧
	  (gdb) bt
	  
	  # 切换栈帧
	  (gdb) frame 1
	  
	  # 查看当前栈帧的局部变量
	  (gdb) info locals
	  
	  # 运行到从当前函数返回为止
	  (gdb) finish
	  
	  
	  # 修改变量值
	  (gdb) set var sum=0
	  # 也可以用 print 命令修改变量的值或者调用函数（因为 print 命令后面跟的是表达式，
	  而我们知道赋值和函数调用也都是表达式，所以也可以用 print 命令修改变量的值或者调用函数）
	  (gdb) p result[2]=33
	  $5 = 33
	  (gdb) p printf("result[2]=%d\n", result[2])
	  result[2]=33
	  $6 = 13
	  (gdb) p (size_t)printf("hello\n")
	  hello
	  $2 = 6
	  
	  
	  # 断点（当程序执行到某一行代码行时中断）
	  (gdb) break 行号
	  (gdb) break 函数名
	  # 条件断点
	  (gdb) break 9 if sum != 0
	  # 断点信息
	  (gdb) info breakpoints
	  # 断点 disable
	  (gdb) disable breakpoints 3
	  # 断点 enable
	  (gdb) enable 3
	  # 删除断点
	  (gdb) delete breakpoints 2
	  # 断点命令列表（GDB支持断点触发后，自动执行用户预设的一组调试命令）
	  # 格式
	  commands [bp_id...]
	  	command-list
	  end
	  # commands为GDB内置关键字
	  # bp_id为断点id（info命令显示的断点num），可以指定多个，不指定时默认只对最近一次设置的断点有效
	  # commad-list为预设的一组命令，当bp_id指定的断点被触发时，GDB会自动执行这些命令
	  # end表示结束
	  
	  # 动态打印结合了断点和打印，效果相当于在程序运行过程中插入了一条printf语句
	  dprintf locspec,template,expression[,expression…]
	  当程序执行到locspec时，expression会被执行且程序不会中断
	  
	  
	  # 观察点（当程序访问某个存储单元时中断）
	  # 如果我们不知道某个存储单元是被哪一行代码改动的，观察点就非常有用了
	  (gdb) watch input[8]
	  (gdb) info watchpoints
	  # set hardware watchpoint(on address)
	  watch *0x10793ad0
	  
	  # 跟踪查看某个变量，每次停下来都显示它的值
	  (gdb) display 变量名
	  # 取消跟踪显示
	  (gdb) undisplay 跟踪显示号
	  
	  
	  # x命令打印指定的存储单元里保存的内容（从某个位置开始打印存储单元的内容，
	  全部当成字节来看，而不区分哪个字节属于哪个变量）
	  # 12表示打印12组，b表示每个字节一组，x表示按十六进制格式打印
	  (gdb) x/12bx input
	  0x804a024 <input>:   0x31    0x32    0x33    0x34    0x35    0x36    0x37    0x38
	  0x804a02c <i>:       0x02    0x00    0x00    0x00
	```
	- 启动调试
		- ```bash
		  调试二进制文件
		  gdb <program>
		  
		  调试进程
		  gdb <program> <PID>
		  
		  调试 core 文件
		  gdb <program> <core>
		  ```
	- 设置参数
		- ```bash
		  # 设置被调试程序的运行参数
		  (gdb) set args [arguments] 
		  
		  # 显示被调试程序的运行参数
		  (gdb) show args
		  
		  # 当 set args 后不跟参数列表时，表示清空所设置的运行参数
		  (gdb) set args
		  
		  # 进入 GDB 调试器的时候，设置被调试程序的运行参数
		  gdb --args <program> <args>
		  ```
	- 执行程序
		- ```bash
		  # （重新）开始执行程序并停在main函数中变量定义之后的第一条语句，不退出gdb重新开始下一次的debug
		  (gdb) start
		  
		  # 运行被调试程序，缩写为 r
		  (gdb) run 
		  (gdb) r 
		  
		  # 继续运行被调试程序
		  (gdb) continue
		  (gdb) c
		  
		  单步调试
		  # 单步执行程序，不进入函数体内部
		  (gdb) next
		  (gdb) n 
		  
		  # 单步执行程序，进入函数体内部，并在函数第一行代码出停止执行
		  (gdb) step 
		  (gdb) s 
		  ```
	- 流程控制
		- ```bash
		  # 执行程序直至当前函数返回，并打印函数返回时的堆栈地址和返回值及参数值等信息
		  (gdb) finish
		  
		  # 立即返回，也可以使用 expression 指定函数返回值 
		  (gdb) return 
		  (gdb) return expression
		  
		  # 执行程序直至退出当前循环体
		  (gdb) until
		  (gdb) u
		  
		  # 执行到42行停止
		  (gdb) until 42
		  (gdb) u 42
		  
		  
		  # 跳转到42行
		  (gdb) jump 42
		  ```
	- 设置断点
		- ```bash
		  # 在 main.cpp 文件 42 行设置断点
		  (gdb) break main.cpp:42 
		  
		  
		  # 在当前文件42行设置断点
		  
		  (gdb) break 42
		  
		  # 在函数入口处设置断点
		  (gdb) break [func] 
		  
		  # 设置条件断点，如果 num == 2012 ，在main.cpp中42行设置断点
		  (gdb) b main.cpp:42 if num == 2012
		  
		  
		  # 设置的断点仅生效一次，之后被删除
		  (gdb) tbreak 42 
		  
		  查看断点
		  (gdb) info b
		  (gdb) info breakpoints
		  
		  删除断点
		  # 删除所有断点
		  (gdb) clear 
		  
		  # 删除42行的断点
		  (gdb) clear 42
		  
		  # 删除函数func内所有断点
		  (gdb) clear <func>
		  
		  # 删除编号为Num的断点
		  
		  (gdb) delete <Num>
		  
		  # 删除所有断点
		  (gdb) delete breakpoints
		  
		  
		  禁用和启用断点
		  # 禁用编号为Num的断点
		  (gdb) disable <Num>
		  
		  # 启用编号为Num的断点
		  (gdb) enable <Num>
		  
		  ```
	- 输出信息
		- ```bash
		  (gdb) print val
		  (gdb) p val
		  (gdb) p ptr
		  $1 = (int *) 0x704210
		  
		  # 打印 ptr[0]
		  (gdb) p *ptr
		  $2 = 0
		  
		  # 打印 ptr[0 - 10]，符号@后追加的数字表示打印的长度
		  (gdb) p *ptr@6
		  $3 = {0, 1, 2, 3, 4, 5}
		  ```
	- 函数调用栈帧
		- ```bash
		  # 显示所有栈帧
		  (gdb) bt
		  
		  # 显示函数调用栈从栈顶算起的n帧信息（n 表示一个正整数）。
		  (gdb) bt n
		  
		  # 显示函数调用栈从栈底算起的n帧信息。
		  (gdb) bt -n
		  
		  # 显示栈中所有信息如：函数参数，本地变量等。
		  (gdb) bt full 
		  
		  # 显示函数调用栈从栈顶算起的n帧的所有信息。
		  (gdb) bt full n
		  
		  # 显示函数调用栈从栈底算起的n帧的所有信息。
		  (gdb) bt full -n
		  
		  查看栈中某一帧的信息
		  # 进入指定的栈帧中，然后可以查看当前栈帧中的局部变量，以及栈帧内容等信息
		  (gdb) frame <栈帧编号>
		  
		  # 可以查看指定栈帧的详细信息
		  (gdb) info frame <栈帧编号>
		  
		  # 进入上层栈帧
		  (gdb) up
		  
		  # 进入下层栈帧
		  (gdb) down
		  ```
	- 查看各种变量的值
		- ```bash
		  # 指令的缩写形式为 i f ，查看函数调用帧的所有信息
		  (gdb) info frame
		  
		  # 查看函数变量的值
		  (gdb) info args 
		  
		  # 查看本地变量的信息
		  (gdb) info locals 
		  
		  # 查看寄存器的情况（除了浮点寄存器）
		  (gdb) info registers
		  
		  # 查看所有寄存器的情况（包括浮点寄存器）
		  (gdb) info all-registers
		  
		  ```
	- 显示源码和窗口命令
		- ```bash
		  # 显示当前断点处代码，默认 10 行
		  (gdb) list
		  (gdb) l
		  
		  # 显示区间内源码
		  (gdb) list <begin>,<end>
		  
		  # 向前显示源码
		  (gdb) list +
		  
		  # 向后显示源码
		  (gdb) list -
		  
		  # 显示函数源码
		  (gdb) list <func>
		  
		  # 显示指定位置源码
		  (gdb) list <location>
		  ```
- [Extending GDB](https://clouds.eos.ubc.ca/~phil/docs/gdb/onlinedocs/gdb_22.html#SEC241)
	- ```bash
	  #1 User-defined Commands
	  define adder
	    if $argc == 2
	      print $arg0 + $arg1
	    end
	    if $argc == 3
	      print $arg0 + $arg1 + $arg2
	    end
	  end
	  
	  #2 User-defined Command Hooks
	  define hook-echo
	  echo <<<---
	  end
	  
	  define hookpost-echo
	  echo --->>>\n
	  end
	  
	  (gdb) echo Hello World
	  <<<---Hello World--->>>
	  (gdb)
	  
	  #3 Command Files
	  source [-v] filename
	      Execute the command file filename.
	  ```
- [gdbcomm.txt](https://gist.github.com/shekkbuilder/8b52e2cf4142c35ba6de)
- 用户自定义gdb命令
	- [**Canned Sequences of Commands**](https://web.mit.edu/gnu/doc/html/gdb_toc.html#SEC139)
	- [Commands for Controlled Output](https://sourceware.org/gdb/onlinedocs/gdb/Output.html)
	- [User-defined Commands](https://sourceware.org/gdb/onlinedocs/gdb/Define.html#Define)
	- [gdbinit参考文件](https://github.com/gdbinit/Gdbinit/blob/master/gdbinit)
	- [如何编写 gdb 自定义命令](https://csstormq.github.io/blog/%E4%BB%A3%E7%A0%81%E8%B0%83%E8%AF%95%E7%AF%87%EF%BC%882%EF%BC%89%EF%BC%9A%E5%A6%82%E4%BD%95%E7%BC%96%E5%86%99%20gdb%20%E8%87%AA%E5%AE%9A%E4%B9%89%E5%91%BD%E4%BB%A4.html)
	- [用户自定义gdb命令](https://sourceware.org/gdb/onlinedocs/gdb/Define.html#Define)
	- 示例
		- ```bash
		  define adder
		    if $argc == 2
		      print $arg0 + $arg1
		    end
		    if $argc == 3
		      print $arg0 + $arg1 + $arg2
		    end
		  end
		  
		  define adder
		    set $i = 0
		    set $sum = 0
		    while $i < $argc
		      eval "set $sum = $sum + $arg%d", $i
		      set $i = $i + 1
		    end
		    print $sum
		  end
		  
		  define search_byte
		      if $argc != 3
		          help search_byte
		      else
		          set $begin_addr = $arg0
		          set $end_addr = $arg1
		  
		          while $begin_addr <= $end_addr
		              if *((unsigned char*)$begin_addr) == $arg2
		                  printf "Find it！The address is 0x%x\n", $begin_addr
		                  loop_break
		              else
		                  set $begin_addr = $begin_addr + 1
		              end
		          end
		  
		          if $begin_addr > $end_addr
		              printf "Can't find it!\n"
		          end
		      end
		  end
		  
		  document search_byte
		      search a specified byte value(0 ~ 255) during a memory
		      usage: search_byte begin_addr end_addr byte
		  end
		  ```
- 参考文档
	- [GCC online documentation](https://gcc.gnu.org/onlinedocs/)
	- [**Shell Commands**](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Shell-Commands.html)
		```bash
		(gdb) pipe info registers | grep rax
		rax            0x0                 0
		```
	- [Setting Breakpoints](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Breaks.html)
	- [How does gdb call functions?](https://jvns.ca/blog/2018/01/04/how-does-gdb-call-functions/)
	- [GDB 入门笔记](https://imageslr.com/2023/gdb.html)
	- **[Gdbinit](https://github.com/gdbinit/Gdbinit)**
	- [GDB commands by function](https://web.cecs.pdx.edu/~jrb/cs201/lectures/handouts/gdbcomm.txt)
	- [GDB - Call Stack](https://www.cse.unsw.edu.au/~learn/debugging/modules/gdb_call_stack/)
	- [Stopping and Continuing](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Stopping.html#Stopping)
	- [**Debugging with GDB**](https://developer.apple.com/library/archive/documentation/DeveloperTools/gdb/gdb/gdb_toc.html)
	- [Debugging with   GDB](https://sourceware.org/gdb/current/onlinedocs/gdb.html/)
	- [GDB Command Reference](https://visualgdb.com/gdbreference/commands/)
	- [Examining Data](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Data.html#Data)
		- [Copy Between Memory and a File](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Dump_002fRestore-Files.html#Dump_002fRestore-Files)
		  id:: 65939f78-7dec-427f-afb5-a4d919107041
	- [examining the Symbol Table](https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_toc.html#TOC114)
	- **[GDB-Python-Utils](https://github.com/crossbowerbt/GDB-Python-Utils)**
	- [Extending   GDB   using Python](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Python.html#Python)
	- [**Automate Debugging with GDB Python API**](https://interrupt.memfault.com/blog/automate-debugging-with-gdb-python-api)
	- [interrupt](https://github.com/memfault/interrupt/tree/master) #github
	- [PythonGdbTutorial](https://sourceware.org/gdb/wiki/PythonGdbTutorial)
	- [gdbcomm.txt](https://gist.github.com/shekkbuilder/8b52e2cf4142c35ba6de)
	- [Short-list of useful gdb commands](https://www.cs.sfu.ca/~ashriram/Courses/CS295/tutorials/gdb/gdb.html#short-list-of-useful-gdb-commands)
	- [LD_PRELOAD Hacks](https://eklitzke.org/ld-preload-hacks)
	- [Setting breakpoints](https://web.mit.edu/gnu/doc/html/gdb_toc.html#SEC30)
	- [GDB 脚本命令](https://www.adacore.com/gems/gem-119-gdb-scripting-part-1)
	- [GDB脚本命令2](https://www.adacore.com/gems/gem-120-gdb-scripting-part-2)
	- [GDB高级技巧](https://www.jianshu.com/p/8761701a6b97)
	- [100个gdb小技巧](https://github.com/hellogcc/100-gdb-tips/blob/master/src/index.md)
	- [100个gdb小技巧](https://wizardforcel.gitbooks.io/100-gdb-tips/content/)
	- [用 GDB 调试程序](https://pengfeixc.com/blogs/developer-handbook/gdb-debug-1)
	- [Breakpoint Command Lists](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Break-Commands.html#Break-Commands)
	- [GDB高级技巧](https://www.eet-china.com/mp/a91230.html)
	- [GDB实战](https://www.cnblogs.com/guochaoxxl/p/10598119.html)
	- [自定义GDB命令](https://sourceware.org/gdb/onlinedocs/gdb/Sequences.html#Sequences)
	- [gdb Cheatsheet](https://cs.brown.edu/courses/cs033/docs/guides/gdb.pdf)
	- [Debugging Programs with Multiple Threads](https://sourceware.org/gdb/onlinedocs/gdb/Threads.html)
	- [Debugging Multithreaded Programs with GDB](https://diveintosystems.org/book/C3-C_debug/gdb_pthreads.html)
	- [Scripting GDB](https://sdimitro.github.io/post/scripting-gdb/)
	- [Beej's Quick Guide to GDB](https://beej.us/guide/bggdb/)
	- [Automatically loading associated files](https://sourceware.org/gdb/onlinedocs/gdb/Auto_002dloading.html)
	- [GDB cheetsheet](https://gist.github.com/integeruser/0c436a64e087b1c43b278761434cbbfa)
	- [GDB cheetsheet](https://gist.github.com/rkubik/b96c23bd8ed58333de37f2b8cd052c30)
- [VS Code与GDB Server远程调试](https://zhuanlan.zhihu.com/p/295099630)
- [gdb+gdbserver远程调试](https://blog.csdn.net/zhaoxd200808501/article/details/77838933)
- [How to Work with User-Defined Commands in GDB?](https://undo.io/resources/gdb-watchpoint/how-work-user-defined-commands-gdb)
	- ```bash
	  #1
	  define bugreport
	  	set pagination off
	  	set logging file /tmp/bugreport.txt
	  	set logging enabled on
	  	thread apply all backtrace full
	  	shell uname -a
	  	set logging off
	  end
	  
	  #2
	  struct rte_mbuf {
	  	void *buf_addr; /**< Virtual address of segment buffer. */
	  	uint16_t data_off;
	  	uint32_t pkt_len; /**< Total pkt len: sum of all segments. */
	  	uint16_t data_len; /**< Amount of data in segment buffer. */
	  	uint16_t buf_len
	  	......
	  }
	  
	  define pmbuf
	      set $m = (struct rte_mbuf *)$arg0
	      printf "dump mbuf at %p, buf_addr:%p, buf_len=%hu, data_off=%u\n", $m, $m->buf_addr, $m->buf_len, $m->data_off
	      printf "data_len=%hu, pkt_len=%d\n", $m->data_len, $m->pkt_len
	      printf "ol_flags=0x%x, nb_segs=%u\n", $m->ol_flags, $m->nb_segs
	      while $m
	          set $data_len = $m->data_len
	          set $data = $m->buf_addr + $m->data_off
	  
	          dump binary memory /tmp/dump.bin $data $data+$data_len
	          shell hexdump -n 100 -C /tmp/dump.bin
	          set $m = $m->next
	      end
	  end
	  document pmbuf
	      print mbuf
	      usage: pmbuf batch->packets[0]
	  end
	  ```
- [gdb Debugging Full Example](https://www.brendangregg.com/blog/2016-08-09/gdb-example-ncurses.html)