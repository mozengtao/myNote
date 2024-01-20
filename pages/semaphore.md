- semaphore基本概念
	- Semaphore is simply a variable that is non-negative and shared between threads. This variable is used to solve the critical section problem and to achieve process synchronization in the multiprocessing environment.
	- A semaphore S is an integer variable that, apart from initialization, is accessed only through two standard atomic operations: wait() and signal()
	  collapsed:: true
		- wait
		  collapsed:: true
			- P means "to test"
			- ```c
			  P (Semaphore S){
			      while (S<=0)
			      ;  // no operation
			      S--;
			  }
			  ```
		- signal
		  collapsed:: true
			- V means "to increment"
				- ```c
				  V (Semaphore S) {
				      S++;
				  }
				  ```
	- This semaphore is called a shared variable as it is shared between different processes
- semaphore分类
	- binary semaphore
	  collapsed:: true
		- > The value of a binary semaphore can range only between 0 and 1. On some systems, binary semaphores are known as mutex locks, as they are locks that provide mutual exclusion.
		  If the S = 0, that means some other process is in the critical section and the requesting process has to wait. If the value is 1, then the critical section is free and the requesting process can access the shared resource
		- 工作原理
			- Let's say, process P1 wants to access the shared resource and currently the critical section is free. The semaphore value is always initialized to 1.
			- To enter the critical section, firstly, P1 has to check whether it is free or not. For this, the wait() function is used. The process P1 enters wait() operation with S=1. The condition while(S<=0) is false. Therefore, it does not enter the while loop and decrements by 1 S--;. Now, S=0.
			- Now, it will enter the critical section. Meanwhile, process P2 wants to access the shared resource by entering into the critical section. So process P2 enters the wait() operation to check whether the critical section is free or not.
			- We know the value of S=0 because already the process P1 is in the critical section and is not free. Therefore, while(S<=0) is true, enter the while loop and get stuck by not entering the critical section which is already occupied.
			- Now, coming to the process P1, it has completed its job of accessing the shared resource and wants to exit. The process P1, calls the signal() operation and increments the value of S S++;. Therefore, the value of S becomes 1.
			- The process P1, is signalling the other process that it has completed using the shared resource, now others can access it.
			- The process P2, which is stuck in the while loop in the wait() operation, will come out of the while loop as soon as the value of S is incremented by the process P1 in signal function because S=1 which makes while(S<=0) false.
			- The process P2 decrements the value of S again to 0 and block the other processes enter, and P2 itself enters the critical section.
			- In this way, the binary semaphore works
	- counting semaphore
	  collapsed:: true
		- > Its value can range over an unrestricted domain. It is used to control access to a resource that has multiple instances
		- 工作原理
		  collapsed:: true
			- Let's say, there are processes P1, P2, and P3. The resource that having two instances R1, R2. That means the resource can be used by two processes at the same time because there are two instances of the same resource.
			- Therefore, we set the value of the counting semaphore S=2. S=2 represents the number of instances of the shared resource.
			- Initially, the critical section is free that means S=2. Now, process P1 wants to access the shared resource, then P1 enters wait() operation, while(S<=0) is false, S is decremented by 1, S = 1.
			- The process P1 is in the critical section. Meanwhile, the process P2 wants to access the shared resource. It enters the wait() operation, while(S<=0) is false, S is decremented by 1, S=0.
			- The process P1 and P2 are accessing two instances of the shared resource. While P1 and P2 are in the critical section, P3 enters the wait() operation. Since S=0, while(S<=0) is true, the process P3 will get stuck in the while loop.
			- If the process P1 has completed its job and before exiting the critical section, P3 enters the signal() operation and increments S by 1. S=1.
			- Now, the process P3 which is stuck in the while loop will come out of the while loop as P1 has incremented S by 1 and while(S<=0) is false.
			- P3 again decrements the value of S by 1, S=0 and enters the critical section.
			- In this way the counting semaphore works.
- 参考文档
	- [semaphore](https://pages.mtu.edu/~shene/NSF-3/e-Book/SEMA/basics.html)
	- [Semaphores and Mutexes](https://faculty.cs.niu.edu/~hutchins/csci480/semaphor.htm)
	- [Semaphore Interface](https://www3.physnet.uni-hamburg.de/physnet/Tru64-Unix/HTML/APS33DTE/DOCU_010.HTM)
	- [synchronization problem](https://eric-lo.gitbook.io/synchronization/)