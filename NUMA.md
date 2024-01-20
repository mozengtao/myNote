### UMA vs NUMA
Modern **micro-processors contain integrated memory controllers** that are connected via channels to the memory. 
Accessing the memory can be organized in two kinds:  
1. Uniform Memory Access (UMA) 
2. Non-Uniform Memory Access ([NUMA](https://www.boost.org/doc/libs/1_65_1/libs/fiber/doc/html/fiber/numa.html#ftn.fiber.numa.f0)).

![[Pasted image 20221018170010.png]]
#### UMA
provides a **centralized pool of memory** (and thus does not scale after a certain number of processors)
#### NUMA 
divides the memory into **local** and **remote** memory **relative to the micro-processor**
- local memory
  > directly attached to the processor's integrated memory controller.
- remote memory
  > Memory connected to the memory controller of another micro-processor (multi-socket systems)  If a memory controller access remote memory it has to traverse the interconnect and connect to the remote memory controller.  
  
  Thus accessing remote memory adds additional latency overhead to local memory access. Because of the different memory locations, a NUMA-system experiences _non-uniform_ memory access time.  
  
  As a consequence the best performance is achieved by keeping the memory access local.
- From [NUMA-Aware Systems](http://docs.adaptivecomputing.com/torque/6-0-0/Content/topics/torque/11-Using%20NUMA/NUMAawareTorque.htm)
	- #+BEGIN_QUOTE
	  The NUMA-aware architecture is a hardware design which separates its cores into multiple clusters where each cluster has its own local memory region and still allows cores from one cluster to access all memory in the system. However, if a processor needs to use memory that is not its own memory region, it will take longer to access that (remote) memory. For applications where performance is crucial, preventing the need to access memory from other clusters is critical.
	  
	  Torque uses cgroups to better manage cpu and memory accounting, memory enforcement, cpuset management, and binding jobs to devices such as MICs and GPUs. Torque will try to place jobs which request GPUs or MICs on NUMA nodes next to the GPU or MIC device to be used.
	  
	  PCIe devices are similar to cores in that these devices will be closer to the memory of one NUMA node than another. Examples of PCIe devices are GPUs, NICs, disks, etc.
	  
	  The resources of a processor chip have a hierarchy. The largest unit is a **socket**. A socket can contain one or more NUMA **nodes** with its **cores** and **memory**. A NUMA node will contain a set of **cores** and **threads** and **memory** which is **local** to the NUMA **node**. A core may have 0 or more **threads**.
	  
	  A **socket** refers to the physical location where a processor package plugs into a motherboard. The processor that plugs into the motherboard is also known as a socket. The socket can contain one or more NUMA nodes.
	  
	  A **core** is an individual execution unit within a processor that can independently execute a software execution thread and maintains its execution state separate from the execution state of any other cores within a processor.
	  
	  A **thread** refers to a hardware-based thread execution capability. For example, the Intel Xeon 7560 has eight cores, each of which has hardware that can effectively execute two software execution threads simultaneously, yielding 16 threads.
	  #+END_QUOTE
- [What is NUMA?](https://seniordba.wordpress.com/2015/12/23/what-is-numa/)
- [What is NUMA?](https://www.kernel.org/doc/html/v4.19/vm/numa.html)
- [non-uniform memory access (NUMA)](https://www.techtarget.com/whatis/definition/NUMA-non-uniform-memory-access)
- [NUMA (Non-Uniform Memory Access): An Overview](https://queue.acm.org/detail.cfm?id=2513149)