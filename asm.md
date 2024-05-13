
- [Online Compiler](https://www.mycompiler.io/new/asm-x86_64)

- [GCC-Inline-Assembly-HOWTO](https://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html)
- [How to Use Inline Assembly Language in C Code](https://gcc.gnu.org/onlinedocs/gcc/extensions-to-the-c-language-family/how-to-use-inline-assembly-language-in-c-code.html)

- [PowerPC CODE Programming the PowerPC Chip in Assembly Language](http://www.bitsavers.org/components/motorola/PowerPC/Kacmarcik_Optimizing_PowerPC_Code_1995.pdf)

- [GCC inline assembler code notes for PowerPC](https://confluence.slac.stanford.edu/display/CCI/GCC+inline+assembler+code+notes+for+PowerPC)
- [Assembly Primer Part 1 — System Organization — PPC](https://brnz.org/hbr/?p=631)
- [Balance of Power: Introducing PowerPC Assembly Language](http://preserve.mactech.com/articles/develop/issue_21/21balance.html)
- [64-bit PowerPC ELF Application Binary Interface](https://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi.html)
- [Power Architecture 32-bit Application Binary Interface](https://example61560.wordpress.com/wp-content/uploads/2016/11/powerpc_abi.pdf)
- [Power PC Embedded Application Binary Interface(EABI): 32-Bit Implementation](https://www.nxp.com/docs/en/application-note/PPCEABI.pdf)
- [NXP e6500 Core Reference Manual](https://www.nxp.com/docs/en/reference-manual/E6500RM.pdf)
- [Freescale PowerPC Architecture Primer](https://www.nxp.com/docs/en/white-paper/POWRPCARCPRMRM.pdf)
- [PowerPC Architecture](http://www.bitsavers.org/components/ibm/powerpc/SR28-5124-00_PowerPC_Architecture_First_Edition_May93.pdf)
- [PowerPC Microprocessor Family: The Programming Environments](https://www.nxp.com/docs/en/user-guide/MPCFPE_AD_R1.pdf)
- [64-bit PowerPC ELF Application Binary Interface Supplement](https://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi-1.7.1.html)

- [Writing x86_64 assembly language for Linux](https://www.cs.fsu.edu/~langley/CNT5605/2017-Summer/assembly-example/assembly.html)
- [Linux System Call Table](https://faculty.nps.edu/cseagle/assembly/sys_call.html)
- [LINUX SYSTEM CALL TABLE FOR X86 64](https://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/)

- [https://www.cs.virginia.edu/~evans/cs216/guides/x86.html](http://www.cburch.com/csbsju/cs/350/docs/nasm/nasmdoc0.html)
- [NASM - The Netwide Assembler](https://www.nasm.us/doc/nasmdoc0.html)
- [x86 Assembly Guide](https://www.cs.virginia.edu/~evans/cs216/guides/x86.html)
- [NASM Tutorial](https://cs.lmu.edu/~ray/notes/nasmtutorial/)
- [Linux Assembly Tutorial](https://montcs.bloomu.edu/Information/LowLevel/Assembly/assembly-tutorial.html)
- [Assembly (NASM)](https://7h3w4lk3r.gitbook.io/the-hive/programming/assembly)
- [Assembly Programming Tutorial](https://www.tutorialspoint.com/assembly_programming/index.htm)

- nasm
	- NASM是一个针对intel x86处理器架构的汇编器
	  title:: asm
	  > NASM, the Netwide Assembler: an assembler targeting the Intel x86 series of processors, with portable source
	- 示例程序
		- ```asm
		  ; --------------------------------------------------------------------------------------
		  ; Writes "Hello, World" to the console using system calls. Runs on 64-bit Linux only.
		  ; To assemble and run:
		  ;     nasm -felf64 hello.asm && ld hello.o && ./a.out
		  ; --------------------------------------------------------------------------------------
		  
		          global    _start
		  
		          section   .text
		  _start: mov       rax, 1                  ; system call for write
		          mov       rdi, 1                  ; file handle 1 is stdout
		          mov       rsi, message            ; address of string to output
		          mov       rdx, 13                 ; number of bytes
		          syscall                           ; invoke operating system to do the write
		          mov       rax, 60                 ; system call for exit
		          xor       rdi, rdi                ; exit code 0
		          syscall                           ; invoke operating system to exit
		  
		          section   .data
		  message:  db        "Hello, World", 10      ; note the newline at the end
		  ```
- 参考文档
	- [x86 Assembly Guide](https://www.cs.virginia.edu/~evans/cs216/guides/x86.html)
	- [Extended Asm - Assembler Instructions with C Expression Operands](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html#Extended-Asm)
	- [nasm tutorial](https://cs.lmu.edu/~ray/notes/nasmtutorial/)
	- [NASM online document](https://www.nasm.us/xdoc/2.15.05/html/nasmdoc0.html)
	- [A Hacker’s Tour of the X86 CPU Architecture](https://www.secureideas.com/blog/2021/04/a-hackers-tour-of-the-x86-cpu-architecture.html)
	- [How to Build a Hello World Program in NASM](https://www.secureideas.com/blog/2021/05/linux-x86-assembly-how-to-build-a-hello-world-program-in-nasm.html)
	- [GCC-Inline-Assembly-HOWTO](https://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html)
	- [asm explanation](https://courses.cs.washington.edu/courses/cse351/17sp/lectures/)