- cpu基本概念
	- socket
		- a hardware package (a “socket”), a package of processors sharing a physical connection to the motherboard
		- #+BEGIN_QUOTE
		  A CPU socket, also known as a CPU slot, is a component in computer hardware that provides both mechanical and electrical connections between a microprocessor and a Printed Circuit Board (PCB). 
		  #+END_QUOTE
	- core
		- an independent processor (execution unit (a “core”)) within that package; multiple processors in the same package: **think of this as two or more CPUs sharing the same socket**.
		- #+BEGIN_QUOTE
		  CPU cores are the processing units that are the heart of a CPU. A multi-core processor has 2 or more cores
		  #+END_QUOTE
	- thread
		- a largely dependent unit (a “thread”) within that core that is more logical abstraction than anything else. **Processor threads are “logical processors”**: the CPU reports more processors than physically exist.
		- #+BEGIN_QUOTE
		  A CPU thread, in particular, is a virtual representation of a physical CPU core, which is why it is often referred to as a “virtual processor.” A single CPU core can execute multiple threads, allowing for multiple flows of execution to occur simultaneously.
		  #+END_QUOTE
	- Example1
		- ```bash
		  morrism@r660atc:~$ lscpu
		  ......
		      Thread(s) per core:  2
		      Core(s) per socket:  32
		      Socket(s):           1
		  
		  The 1/32/64-processor computer is a one-socket server with 32-core CPUs in each socket, 
		  each core having two threads. It has:
		  1 processor sockets,
		  32 total processor cores, or
		  64 total processor threads.
		  
		  If you multiply the number of socket, cores and threads, i.e. 1*32*2, then you get the 
		  number of "CPUs": 64. These aren't real CPUs, but the number of possible parallel threads
		  of execution your system can do.
		  ```