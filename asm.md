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