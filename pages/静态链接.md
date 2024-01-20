- 示例
	- ```c
	  # sub.c 文件
	  int SubData = 100;
	  
	  int SubFunc(int num)
	  {
	          return num * num;
	  }
	  
	  
	  # main.c 文件
	  extern int SubData;
	  extern int SubFunc(int num);
	  
	  int main(void)
	  {
	          int result = SubFunc(SubData);
	          return 0;
	  }
	  
	  # 编译命令
	  gcc -m32 -c sub.c
	  gcc -m32 -c main.c
	  ```
	- 查看section header信息
		- ```bash
		  morrism@PC2886:~/x$ readelf -S sub.o
		  Section Headers:
		    [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
		  ......
		    [ 2] .text             PROGBITS        00000000 00003c 000016 00  AX  0   0  1
		  ......
		    [ 4] .data             PROGBITS        00000000 000054 000004 00  WA  0   0  4
		  ......
		  ```
			- 代码段.text和数据段.data的起始地址均为0(目标文件不安排地址)
			- 代码段.text在sub.o中的偏移量是0x3c，长度是0x16字节
			- 数据段.data在sub.o中的偏移量是0x54，长度是0x4字节
	- 查看elf header信息
		- ```bash
		  morrism@PC2886:~/x$ readelf -h sub.o
		  ELF Header:
		    Magic:   7f 45 4c 46 01 01 01 00 00 00 00 00 00 00 00 00
		    Class:                             ELF32
		    Data:                              2's complement, little endian
		    Version:                           1 (current)
		    OS/ABI:                            UNIX - System V
		    ABI Version:                       0
		    Type:                              REL (Relocatable file)
		    Machine:                           Intel 80386
		    Version:                           0x1
		    Entry point address:               0x0
		    Start of program headers:          0 (bytes into file)
		    Start of section headers:          648 (bytes into file)
		    Flags:                             0x0
		    Size of this header:               52 (bytes)
		    Size of program headers:           0 (bytes)
		    Number of program headers:         0
		    Size of section headers:           40 (bytes)
		    Number of section headers:         14
		    Section header string table index: 13
		  ```
			- elf header总共占用52(0x34)字节
				- 代码段.text(0x3c)在header之后8字节处，长度是0x16字节，空间方位0x3C ~ 0x51(0x51 = 0x3C + 0x16 - 1)
				- 数据段.data在代码段之后，占据着 0x54 ~ 0x57 这部分空间
	- 符号表信息
		- 符号表是文件中定义的所有符号，包括引用外部文件的符号，包括变量名，函数名，段名等
		- elf文件用来描述符号的结构体
			- ```c
			  // Symbol table entries for ELF32.
			  struct Elf32_Sym {
			     Elf32_Word st_name;     // Symbol name (index into string table)
			     Elf32_Addr st_value;    // Value or address associated with the symbol
			     Elf32_Word st_size;     // Size of the symbol
			     unsigned char st_info;  // Symbol's type and binding attributes
			     unsigned char st_other; // Must be zero; reserved
			     Elf32_Half st_shndx;    // Which section (header table index) it's defined in
			  };
			  ```
		- 查看符号表信息
			- ```bash
			  morrism@PC2886:~/x$ readelf -s sub.o
			  Symbol table '.symtab' contains 14 entries:
			     Num:    Value  Size Type    Bind   Vis      Ndx Name
			  ......
			      10: 00000000     4 OBJECT  GLOBAL DEFAULT    4 SubData
			      11: 00000000    22 FUNC    GLOBAL DEFAULT    2 SubFunc
			  ......
			  ```
				- SubData和SubFunc是sub.c文件中定义的全局变量和全局函数
					- SubData
						- Size=4:变量长度4字节
						- Type=OBJECT: 变量为数据对象
						- Bind=GLOBAL:变量全局可见，可在其他文件中使用
						- Ndx=4:符号属于第4个段，即数据段.data
					- SubFunc
						- Size=22:函数长度22字节
						- Type=FUNC: 符号类型为函数
						- Bind=GLOBAL:函数全局可见，可在其他文件中使用
						- Ndx=2:符号属于第2个段，即数据段.text
		- 对文件main.o的分析
			- ```bash
			  # 查看 section header 信息
			  morrism@PC2886:~/x$ readelf -S main.o
			  ......
			    [ 2] .text             PROGBITS        00000000 00003c 000044 00  AX  0   0  1
			  ......
			    [ 4] .data             PROGBITS        00000000 000080 000000 00  WA  0   0  1
			  ......
			  
			  # 查看符号表信息
			  morrism@PC2886:~/x$ readelf -s main.o
			     Num:    Value  Size Type    Bind   Vis      Ndx Name
			  ......
			      10: 00000000    68 FUNC    GLOBAL DEFAULT    2 main
			  ......
			      13: 00000000     0 NOTYPE  GLOBAL DEFAULT  UND SubData
			      14: 00000000     0 NOTYPE  GLOBAL DEFAULT  UND SubFunc
			  ```
				- 对于main.o中的符号SubData和SubFunc，Ndx=UND表明main.o用到了这两个符号，但是这两个符号定义在其他文件里，链接生成可执行文件时需要确定这两个符号在可执行文件中的地址
			- 查看main.o的反汇编代码
				- ```asm
				  morrism@localhost /tmp/x $ objdump -d main.o
				  ......
				     e:   83 ec 14                sub    $0x14,%esp
				    11:   a1 00 00 00 00          mov    0x0,%eax
				    16:   83 ec 0c                sub    $0xc,%esp
				    19:   50                      push   %eax
				    1a:   e8 fc ff ff ff          call   1b <main+0x1b>
				    1f:   83 c4 10                add    $0x10,%esp
				  ......
				  ```
					- main函数调用SubFunc，将SubFunc作为参数传递给main，只不过此时main.o无法确定链接器将SubFunc安排在什么地址，所有暂时用占位符00 00 00 00填充
					- 函数调用call的地址是fc ff ff ff，十进制是-4
					- 绝对寻址
						- 对于SubData符号是绝对寻址
							- 链接生成可执行文件时，这个地址在代码段中偏移0x12字节（当前指令偏移0x11 + 一个字节的指令码a1），这个地址的当前值是 00 00 00 00
							- 链接器在修正时会把这4个字节的值修改为SubData在可执行文件中的实际虚拟地址
					- 相对寻址
						- 当CPU执行到这条指令的时候，把PC寄存中的值加上这个偏移地址，就是被调用对象的实际地址
						- 对于SubFunc符号是绝对寻址，链接器在重定位时目的是计算出相对地址，替换掉fc ff ff ff字节
						- pc寄存器中的值是确定的，当call这条指令被CPU取到之后，PC寄存器被自动增加，指向下一条指令的开始地址（偏移0x1f地址处）
						- call的实际地址 = pc + xxx，xxx = 实际地址 - pc
						- pc值与xxx所在的地址是有关系的，即pc -4 = xxx，因此在main.o中预先将这个地址处的值填上 fc ff ff ff (-4)
						- 问题：链接器如何知道main.o中代码段的这两个地方需要修正？答案是重定位表
						- 重定位表
							- 重定位表标识了该目标文件中有哪些符号需要在链接时进行地址重定位
								- ```bash
								  morrism@localhost /tmp/x $ objdump -r main.o
								  RELOCATION RECORDS FOR [.text]:
								  OFFSET   TYPE              VALUE
								  00000012 R_386_32          SubData
								  0000001b R_386_PC32        SubFunc
								  
								  RELOCATION RECORDS FOR [.eh_frame]:
								  OFFSET   TYPE              VALUE
								  00000020 R_386_PC32        .text
								  ```
									- TYPE
										- R_386_32
											- 绝对寻址
												- SubData符号需要进行重定位，需要把这个符号在执行时刻的绝对寻址（虚拟地址），写入到 main可执行文件中代码段中偏移0x12字节处
										- R_386_PC32
											- 相对寻址
									- OFFSET
										- 标识需要重定位的符号在代码段中的偏移位置
			- main
				- `ld -m elf_i386 main.o sub.o -e main -o main`
				- ```bash
				  morrism@localhost /tmp/x $ readelf -S main
				  ......
				    [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
				  ......
				    [ 1] .text             PROGBITS        08048094 000094 00003e 00  AX  0   0  1
				  ......
				    [ 3] .data             PROGBITS        0804a000 001000 000004 00  WA  0   0  4
				  ......
				  ```
				- ```bash
				  morrism@localhost /tmp/x $ readelf -s main
				  ......
				     Num:    Value  Size Type    Bind   Vis      Ndx Name
				  ......
				       9: 0804a000     4 OBJECT  GLOBAL DEFAULT    3 SubData
				  ......
				  	11: 08048094    50 FUNC    GLOBAL DEFAULT    1 main
				      12: 080480c6    12 FUNC    GLOBAL DEFAULT    1 SubFunc
				  ......
				  ```
				- 链接器第一遍扫描所有的目标文件，合并相同类型的段，安排到相应的虚拟地址，链接器同时建立一个全局符号表，把每一个目标文件中的符号信息都复制到这个全局符号表中
				- 链接器第二遍扫描所有的目标文件，检查哪些目标文件中的符号需要进行重定位
					- 绝对地址重定位
						- 从main.o的重定位符号表中可知，SubData需要进行重定位，即需要把这个符号在执行时刻的绝对寻址（虚拟地址），写入到 main可执行文件中代码段中偏移0x12字节处
						- 需要解决的两个问题
							- 需要计算出在执行文件 main 中的什么位置来填写绝对地址（虚拟地址）
								- 从可执行文件的段表可知，目标文件main.o和sub.o的代码段被存放到可执行文件main中代码段的开始位置，先放main.o的代码段，再放sub.o的代码段
								- 代码段的开始地址距离文件开始的偏移量是0x94，再加上偏移量0x12，结果就是0xa6，即需要在main文件中偏移0xa6处填入SubData在执行时刻的绝对地址
							- 填写的绝对地址（虚拟地址）的值是多少
								- 链接器从全局符号表中发现SubData属于sub.o文件，已经被安排在了虚拟地址0x0804a000处，因此只需要把0x0804a000填写到可执行文件main中偏移0xa6的位置
								- 验证main文件中偏移0xa6处的虚拟地址是否正确
									- ```bash
									  morrism@localhost /tmp/x $ od -Ax -t x1 -j 166 -N 4 main
									  0000a6 00 a0 04 08
									  0000aa
									  ```
					- 相对地址重定位
						- 链接器需要把SunFunc符号在执行时刻的绝对地址（虚拟地址），减去call指令的下一条指令 (pc寄存器)之后的差值，填写到执行文件main中的main．o代码段偏移0x1b的地方
						- 需要解决 2 个问题
							- 在执行文件 main 中的什么位置来填写相对地址
								- 从main.o的重定位表可知，需要修正的位置距离main.o中代码段的偏移量是0x1b字节
								- 可执行文件main中代码段的开始地址距离文件开始的偏移量是0x94，再加上偏移量0x1b，就是0xaf，即需要在main文件中的0xaf偏移处填入一个相对地址，这个相对地址的值就是SubFunc在执行时刻的绝对地址距离call指令的下一条指令的偏移量
							- 填写的相对地址的值是多少
								- 链接器在第一遍扫描时，已经把sub.o中的符号SubFunc记录到全局符号表中，知道SubFunc被安排在0x080480c6地址处，但是此处是绝对地址，call指令需要的是相对地址(偏移地址)
								- 链接器把main代码段起始位置安排在0x08048094，那么偏移0x1b处的虚拟地址就是0x080480af，再跨过4个字节(执行到call指令时，pc的值自动增加到下一条指令的开始地址)才是此刻pc寄存器的值，即0x080480b3
								- 计算差值0x080480c6 - 0x080480b3 = 0x13，即在可执行文件main中偏移为0xaf的地方填入相对地址0x00000013就完成了对SubFunc符号的重定位。
								- 验证
									- ```bash
									  morrism@localhost /tmp/x $ od -Ax -t x1 -j 175 -N 4 main
									  0000af 13 00 00 00
									  0000b3
									  ```
					- main反汇编
						- ```bash
						  morrism@localhost /tmp/x $ objdump -d main
						  ......
						   80480a2:       83 ec 14                sub    $0x14,%esp
						   80480a5:       a1 00 a0 04 08          mov    0x804a000,%eax
						  ......
						   80480ae:       e8 13 00 00 00          call   80480c6 <SubFunc>
						  ......
						  
						  080480c6 <SubFunc>:
						   80480c6:       55                      push   %ebp
						  ......
						  ```
- 参考文档
	- [[elf]]
	- [GCC 链接过程中的重定位过程分析](https://zhuanlan.zhihu.com/p/482773180)
	- ((63c0ba01-becf-48b5-b022-812e6d01b694))