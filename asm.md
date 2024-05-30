
- [**Linux Assembly Tutorial**](https://montcs.bloomu.edu/Information/LowLevel/Assembly/assembly-tutorial.html)
- [Linux Assembly Tutorial Step-by-Step Guide](https://web.archive.org/web/20120822144129/http://www.cin.ufpe.br/%7Eif817/arquivos/asmtut/index.html)
- [**Assembly Basics**](https://cs61.seas.harvard.edu/site/2018/Asm1/) #harvard cs61

- ![x86 Assembly, 32 bit](./assets/x86-32bit-asm.pdf)
- ![The 32 bit x86 C Calling Convention](./assets/x86-32bit-calling-convention.pdf)
- ![x86 Assembly, 64 bit](./assets/x86-64bit-asm.pdf)
- ![The 64 bit x86 C Calling Convention](./assets/x86-64bit-calling-convention.pdf)
- [Program and Data Representation](https://aaronbloomfield.github.io/pdr/readme.html) #pdf

- [X86-64 Architecture Guide](http://6.s081.scripts.mit.edu/sp18/x86-64-architecture-guide.html) #online

- [Intel® 64 and IA-32 Architectures Software Developer’s Manual Volume 1:Basic Architecture](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-1-manual.pdf) #online
- [Intel ® 64 and IA-32 Architectures Software Developer’s Manual](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developers-manual.pdf) #online
> GENERAL-PURPOSE INSTRUCTIONS
> 5.1.1 Data Transfer Instructions
> 	  MOV Move data between general-purpose registers; move data between memory and general-purpose or segment registers; move immediates to general-purpose registers
>     CMOVE/CMOVZ Conditional move if equal/Conditional move if zero.
>     CMOVNE/CMOVNZ Conditional move if not equal/Conditional move if not zero.
>     CMOVA/CMOVNBE Conditional move if above/Conditional move if not below or equal.
>     CMOVAE/CMOVNB Conditional move if above or equal/Conditional move if not below.
>     CMOVB/CMOVNAE Conditional move if below/Conditional move if not above or equal.
>     CMOVBE/CMOVNA Conditional move if below or equal/Conditional move if not above.
>     CMOVG/CMOVNLE Conditional move if greater/Conditional move if not less or equal.
>     CMOVGE/CMOVNL Conditional move if greater or equal/Conditional move if not less.
>     CMOVL/CMOVNGE Conditional move if less/Conditional move if not greater or equal.
>     CMOVLE/CMOVNG Conditional move if less or equal/Conditional move if not greater.
>     CMOVC Conditional move if carry.
>     CMOVNC Conditional move if not carry.
>     CMOVO Conditional move if overflow.
>     CMOVNO Conditional move if not overflow.
>     CMOVS Conditional move if sign (negative).
>     CMOVNS Conditional move if not sign (non-negative).
>     CMOVP/CMOVPE Conditional move if parity/Conditional move if parity even.
>     CMOVNP/CMOVPO Conditional move if not parity/Conditional move if parity odd.
>     XCHG Exchange.
>     BSWAP Byte swap.
>     XADD Exchange and add.
>     CMPXCHG Compare and exchange.
>     CMPXCHG8B Compare and exchange 8 bytes.
>     PUSH Push onto stack.
>     POP Pop off of stack.
>     PUSHA/PUSHAD Push general-purpose registers onto stack.
>     POPA/POPAD Pop general-purpose registers from stack.
>     CWD/CDQ Convert word to doubleword/Convert doubleword to quadword.
>     CBW/CWDE Convert byte to word/Convert word to doubleword in EAX register.
>     MOVSX Move and sign extend.
>     MOVZX Move and zero extend.
> 5.1.2 Binary Arithmetic Instructions
>     ADCX Unsigned integer add with carry.
>     ADOX Unsigned integer add with overflow.
>     ADD Integer add.
>     ADC Add with carry.
>     SUB Subtract.
>     SBB Subtract with borrow.
>     IMUL Signed multiply.
>     MUL Unsigned multiply.
>     IDIV Signed divide.
>     DIV Unsigned divide.
>     INC Increment.
>     DEC Decrement.
>     NEG Negate.
>     CMP Compare.
> 5.1.3 Decimal Arithmetic Instructions
>     DAA Decimal adjust after addition.
>     DAS Decimal adjust after subtraction.
>     AAA ASCII adjust after addition.
>     AAS ASCII adjust after subtraction.
>     AAM ASCII adjust after multiplication.
>     AAD ASCII adjust before division.
> 5.1.4 Logical Instructions
>     AND Perform bitwise logical AND.
>     OR Perform bitwise logical OR.
>     XOR Perform bitwise logical exclusive OR.
>     NOT Perform bitwise logical NOT.
> 5.1.5 Shift and Rotate Instructions
>     SAR Shift arithmetic right.
>     SHR Shift logical right.
>     SAL/SHL Shift arithmetic left/Shift logical left.
>     SHRD Shift right double.
>     SHLD Shift left double.
>     ROR Rotate right.
>     ROL Rotate left.
>     RCR Rotate through carry right.
>     RCL Rotate through carry left.
> 5.1.6 Bit and Byte Instructions
>     BT Bit test.
>     BTS Bit test and set.
>     BTR Bit test and reset.
>     BTC Bit test and complement.
>     BSF Bit scan forward.
>     BSR Bit scan reverse.
>     SETE/SETZ Set byte if equal/Set byte if zero.
>     SETNE/SETNZ Set byte if not equal/Set byte if not zero.
>     SETA/SETNBE Set byte if above/Set byte if not below or equal.
>     SETAE/SETNB/SETNC Set byte if above or equal/Set byte if not below/Set byte if not carry.
>     SETB/SETNAE/SETC Set byte if below/Set byte if not above or equal/Set byte if carry.
>     SETBE/SETNA Set byte if below or equal/Set byte if not above.
>     SETG/SETNLE Set byte if greater/Set byte if not less or equal.
>     SETGE/SETNL Set byte if greater or equal/Set byte if not less.
>     SETL/SETNGE Set byte if less/Set byte if not greater or equal.
>     SETLE/SETNG Set byte if less or equal/Set byte if not greater.
>     SETS Set byte if sign (negative).
>     SETNS Set byte if not sign (non-negative).
>     SETO Set byte if overflow.
>     SETNO Set byte if not overflow.
>     SETPE/SETP Set byte if parity even/Set byte if parity.
>     SETPO/SETNP Set byte if parity odd/Set byte if not parity.
>     TEST Logical compare.
>     CRC32  Provides hardware acceleration to calculate cyclic redundancy checks for fast and efficient implementation of data integrity protocols.
>     POPCNT This instruction calculates of number of bits set to 1 in the second operand (source) and returns the count in the first operand (a destination register).
> 5.1.7 Control Transfer Instructions
>     JMP Jump.
>     JE/JZ Jump if equal/Jump if zero.
>     JNE/JNZ Jump if not equal/Jump if not zero.
>     JA/JNBE Jump if above/Jump if not below or equal.
>     JAE/JNB Jump if above or equal/Jump if not below.
>     JB/JNAE Jump if below/Jump if not above or equal.
>     JBE/JNA Jump if below or equal/Jump if not above.
>     JG/JNLE Jump if greater/Jump if not less or equal.
>     JGE/JNL Jump if greater or equal/Jump if not less.
>     JL/JNGE Jump if less/Jump if not greater or equal.
>     JLE/JNG Jump if less or equal/Jump if not greater.
>     JC Jump if carry.
>     JNC Jump if not carry.
>     JO Jump if overflow.
>     JNO Jump if not overflow.
>     JS Jump if sign (negative).
>     JNS Jump if not sign (non-negative).
>     JPO/JNP Jump if parity odd/Jump if not parity.
>     JPE/JP Jump if parity even/Jump if parity.
>     JCXZ/JECXZ Jump register CX zero/Jump register ECX zero.
>     LOOP Loop with ECX counter.
>     LOOPZ/LOOPE Loop with ECX and zero/Loop with ECX and equal.
>     LOOPNZ/LOOPNE Loop with ECX and not zero/Loop with ECX and not equal.
>     CALL Call procedure.
>     RET Return.
>     IRET Return from interrupt.
>     INT Software interrupt.
>     INTO Interrupt on overflow.
>     BOUND Detect value out of range.
>     ENTER High-level procedure entry.
>     LEAVE High-level procedure exit.
> 5.1.8 String Instructions
>     MOVS/MOVSB Move string/Move byte string.
>     MOVS/MOVSW Move string/Move word string.
>     MOVS/MOVSD Move string/Move doubleword string.
>     CMPS/CMPSB Compare string/Compare byte string.
>     CMPS/CMPSW Compare string/Compare word string.
>     CMPS/CMPSD Compare string/Compare doubleword string.
>     SCAS/SCASB Scan string/Scan byte string.
>     SCAS/SCASW Scan string/Scan word string.
>     SCAS/SCASD Scan string/Scan doubleword string.
>     LODS/LODSB Load string/Load byte string.
>     LODS/LODSW Load string/Load word string.
>     LODS/LODSD Load string/Load doubleword string.
>     STOS/STOSB Store string/Store byte string.
>     STOS/STOSW Store string/Store word string.
>     STOS/STOSD Store string/Store doubleword string.
>     REP Repeat while ECX not zero.
>     REPE/REPZ Repeat while equal/Repeat while zero.
>     REPNE/REPNZ Repeat while not equal/Repeat while not zero.
> 5.1.9 I/O Instructions
>     IN Read from a port.
>     OUT Write to a port.
>     INS/INSB Input string from port/Input byte string from port.
>     INS/INSW Input string from port/Input word string from port.
>     INS/INSD Input string from port/Input doubleword string from port.
>     OUTS/OUTSB Output string to port/Output byte string to port.
>     OUTS/OUTSW Output string to port/Output word string to port.
>     OUTS/OUTSD Output string to port/Output doubleword string to port.
> 5.1.10 Enter and Leave Instructions
>     ENTER High-level procedure entry.
>     LEAVE High-level procedure exit.
> 5.1.11 Flag Control (EFLAG) Instructions
>     STC Set carry flag.
>     CLC Clear the carry flag.
>     CMC Complement the carry flag.
>     CLD Clear the direction flag.
>     STD Set direction flag.
>     LAHF Load flags into AH register.
>     SAHF Store AH register into flags.
>     PUSHF/PUSHFD Push EFLAGS onto stack.
>     POPF/POPFD Pop EFLAGS from stack.
>     STI Set interrupt flag.
>     CLI Clear the interrupt flag.
> 5.1.12 Segment Register Instructions
>     LDS Load far pointer using DS.
>     LES Load far pointer using ES.
>     LFS Load far pointer using FS.
>     LGS Load far pointer using GS.
>     LSS Load far pointer using SS.
> 5.1.13 Miscellaneous Instructions
>     LEA Load effective address.
>     NOP No operation.
>     UD2 Undefined instruction.
>     XLAT/XLATB Table lookup translation.
>     CPUID Processor identification.
>     MOVBE 1 Move data after swapping data bytes.
>     PREFETCHW Prefetch data into cache in anticipation of write.
>     PREFETCHWT1 Prefetch hint T1 with intent to write.
>     CLFLUSH Flushes and invalidates a memory operand and its associated cache line from all levels of the processor’s cache hierarchy.
>     CLFLUSHOPT Flushes and invalidates a memory operand and its associated cache line from all levels of the processor’s cache hierarchy with optimized memory system throughput.
> 5.1.14 User Mode Extended Sate Save/Restore Instructions
>     XSAVE Save processor extended states to memory.
>     XSAVEC Save processor extended states with compaction to memory.
>     XSAVEOPT Save processor extended states to memory, optimized.
>     XRSTOR Restore processor extended states from memory.
>     XGETBV Reads the state of an extended control register.
> 5.1.15 Random Number Generator Instructions
>     RDRAND Retrieves a random number generated from hardware.
>     RDSEED Retrieves a random number generated from hardware.
> 5.1.16 BMI1, BMI2
>     ANDN Bitwise AND of first source with inverted 2nd source operands.
>     BEXTR Contiguous bitwise extract.
>     BLSI Extract lowest set bit.
>     BLSMSK Set all lower bits below first set bit to 1.
>     BLSR Reset lowest set bit.
>     BZHI Zero high bits starting from specified bit position.
>     LZCNT Count the number leading zero bits.
>     MULX Unsigned multiply without affecting arithmetic flags.
>     PDEP Parallel deposit of bits using a mask.
>     PEXT Parallel extraction of bits using a mask.
>     RORX Rotate right without affecting arithmetic flags.
>     SARX Shift arithmetic right.
>     SHLX Shift logic left.
>     SHRX Shift logic right.
>     TZCNT Count the number trailing zero bits

- [TheArtOfAssemblyLanguage](https://github.com/media-lib/prog_lib/blob/master/general/Randall%20Hyde%20-%20The%20Art%20of%20Assembly%20Language%2C%202nd%20Edition.pdf) #github
- ![TheArtOfAssemblyLanguage](./assets/TheArtOfAssemblyLanguage.pdf)
- ![ReverseEngineeringforBeginners](./assets/ReverseEngineeringforBeginners.pdf)

- [**Reverse Engineering For Everyone**](https://0xinfection.github.io/reversing/) #online
- [Reversing: Secrets of Reverse Engineering](https://github.com/media-lib/prog_lib/blob/master/general/Eldad%20Eilam%20-%20Reversing%20-%20Secrets%20of%20Reverse%20Engineering.pdf) #github

- [RISC vs CISC](https://cs.stanford.edu/people/eroberts/courses/soco/projects/risc/risccisc/)


- [**Online Compiler**](https://www.mycompiler.io/new/asm-x86_64) #online
- [**Compiler Explorer**](https://godbolt.org/) #online

- [x86 and amd64 instruction reference](https://www.felixcloutier.com/x86/)

- [Brennan's Guide to Inline Assembly](https://www.cs.utexas.edu/users/dahlin/Classes/UGOS/reading/djgpp.html)

- [**x86 Assembly Guide**](https://www.cs.virginia.edu/~evans/cs216/guides/x86.html)

	![x86 registers](x86-registers.png)

- [GCC-Inline-Assembly-HOWTO](https://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html)
```asm
Intel syntax and AT&T syntax

+------------------------------+------------------------------------+
|       Intel Code             |      AT&T Code                     |
+------------------------------+------------------------------------+
| mov     eax,1                |  movl    $1,%eax                   |   
| mov     ebx,0ffh             |  movl    $0xff,%ebx                |   
| int     80h                  |  int     $0x80                     |   
| mov     ebx, eax             |  movl    %eax, %ebx                |
| mov     eax,[ecx]            |  movl    (%ecx),%eax               |
| mov     eax,[ebx+3]          |  movl    3(%ebx),%eax              | 
| mov     eax,[ebx+20h]        |  movl    0x20(%ebx),%eax           |
| add     eax,[ebx+ecx*2h]     |  addl    (%ebx,%ecx,0x2),%eax      |
| lea     eax,[ebx+ecx]        |  leal    (%ebx,%ecx),%eax          |
| sub     eax,[ebx+ecx*4h-20h] |  subl    -0x20(%ebx,%ecx,0x4),%eax |
+------------------------------+------------------------------------+
```

- [How to Use Inline Assembly Language in C Code](https://gcc.gnu.org/onlinedocs/gcc/extensions-to-the-c-language-family/how-to-use-inline-assembly-language-in-c-code.html)

Powerpc ASM
- [**PowerPC Microprocessor Family: The Programming Environments**](https://www.nxp.com/docs/en/user-guide/MPCFPE_AD_R1.pdf)
	- [POWER family and PowerPC architecture overview](https://www.ibm.com/docs/en/aix/7.3?topic=storage-power-family-powerpc-architecture-overview)
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
	- [64-bit PowerPC ELF Application Binary Interface Supplement](https://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi-1.7.1.html)
	- [gcc PowerPC Assembly Quick Reference](https://www.cs.uaf.edu/2011/fall/cs301/lecture/11_21_PowerPC.html)
	- [PowerPC User-Level Instruction Set Quick Reference Card](http://www.tentech.ca/downloads/other/PPC_Quick_Ref_Card-Rev1_Oct12_2010.pdf)
	- ![PowerPC Microprocessor Family:The Programming Environments for 32-Bit Microprocessors](./assets/PowerPC_TheProgrammingEnvironments.pdf)

- [Writing x86_64 assembly language for Linux](https://www.cs.fsu.edu/~langley/CNT5605/2017-Summer/assembly-example/assembly.html)
- [**Linux System Call Table**](https://faculty.nps.edu/cseagle/assembly/sys_call.html)
- [**LINUX SYSTEM CALL TABLE FOR X86 64**](https://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/)

- [The Netwide Assembler: NASM](http://www.cburch.com/csbsju/cs/350/docs/nasm/nasmdoc0.html)
- [NASM - The Netwide Assembler](https://www.nasm.us/doc/nasmdoc0.html)
- [NASM Tutorial](https://cs.lmu.edu/~ray/notes/nasmtutorial/)
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