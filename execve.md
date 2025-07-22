`ps -ef`的结果与`/proc/pid/cmdline`的关系，其中cmdline的内容可由execve中的参数argv指定
```bash
root@slot-120:~ [P1]# ps -ef|grep cms
root      3099  2946  0 Jun24 ?        00:01:24 /opt/berwick/build/armstrong/powerpc/bin/cms cpu=0 ndsnpu=4
```
linux下运行新的程序
	![image.png](../assets/image_1675212361056_0.png)
	![image.png](../assets/image_1675212608503_0.png)
	![image.png](../assets/image_1675212746708_0.png)

execve相关库函数
```c
	int execl(const char *path, const char *arg, ...);
	int execle(const char *path, const char *arg, ..., char * const envp[]);
	int execlp(const char *file, const char *arg, ...);
	int execv(const char *path, char *const argv[]);
	int execvp(const char *file, char *const argv[]);
	int execvpe(const char *file, char *const argv[], char *const envp[]);
	// 这6个函数的返回值：若出错则返回-1，若成功则不返回值
	// l -> list 「l和v互斥」
	// v -> vector 「l和v互斥」
	// p -> path
	// e -> env
```
![image.png](../assets/image_1675142263517_0.png)
execve与[[环境变量]]
	进程获取环境变量的方式
		如果新的进程是通过 [[fork]] 系统调用创建，则子进程会继承父进程的环境变量
		如果进程通过execve系统调用来执行，此时它之前的内存空间会被覆写，原来的环境变量会丢失，execve的第3个参数envp可用来从一个进程给另外一个进程传递环境变量参数

```c
	#include <unistd.h>
	
	extern char **environ;
	
	int main(int argc, char *argv[], char *envp[])
	{
			//int i = 0;
			char *v[2];
			char *newenv[3];
	
			if(argc < 2)
					return 0;
	
			// construct argument array
			v[0] = "/usr/bin/env"; // env 命令用来打印当前进程的环境变量
			v[1] = NULL;
	
			// construct environment variable array
			newenv[0] = "AAA";
			newenv[1] = "BBB";
			newenv[2] = NULL;
	
			switch(argv[1][0]) {
			case '1': // passing no args
					execve(v[0], v, NULL);
			case '2': // passing new env variables
					execve(v[0], v, newenv);
			case '3': // passing all the env variables from parent process
					execve(v[0], v, environ);
			default:
					execve(v[0], v, NULL);
			}
	
			return 0;
	}
```
环境变量的内存位置
	![image.png](../assets/image_1675670198130_0.png)
	envp和environ起始时指向相同的位置
	envp只能在main函数内部进行访问，environ是一个全局变量
	当环境变量发生变化时(如增加新的环境变量)，存储环境变量的位置可能会移动到堆上，因此environ的位置可能会发生改变，而envp则不会

Shell变量与环境变量
	![image.png](../assets/image_1675670686181_0.png)
在shell下运行的进程的环境变量来自shell的如下两个部分
	shell下从环境变量拷贝而来的shell变量
	shell下用户通过export导出的shell变量
```bash
	root@slot-18:/var/log [P2]# strings /proc/3119/environ
	SHELL=/bin/sh
	RUNLEVEL=5
	PWD=/opt/berwick/build/armstrong/powerpc/bin
	runlevel=5
	HOME=/
	CCAPARCH=powerpc
	CCAPROOT_OTH=/opt_oth/berwick
	TERM=linux
	CCAPROOT=/opt/berwick
	INIT_VERSION=sysvinit-
	SHLVL=1
	MNT=/opt/berwick
	LD_LIBRARY_PATH=/opt/berwick/tailf-confd-pkg/powerpc/lib:/opt/berwick/build/armstrong/powerpc/lib
	CONSOLE=/dev/console
	qportals=s0
	PATH=/opt/berwick/tailf-confd-pkg/powerpc/bin:/sbin:/usr/sbin:/bin:/usr/bin
```

[Environment Variables & Attacks](https://www.ics.uci.edu/~goodrich/teach/cs201P/notes/02_Environment_Variables.pdf)  
[execve 相关库函数](https://binhack.readthedocs.io/zh/latest/os/linux/syscall/execve.html)  
[加载 ELF过程](https://binhack.readthedocs.io/zh/latest/os/linux/elf.html)  
[程序是怎么样运行的](https://www.cnblogs.com/gaoxingnjiagoutansuo/p/15434581.html)  
[man 3 exec](https://man7.org/linux/man-pages/man3/exec.3.html)  
[man 2 execve](https://man7.org/linux/man-pages/man2/execve.2.html)  
[exec 系列函数的使用](https://www.cnblogs.com/mickole/p/3187409.html)  
[How programs get run](https://lwn.net/Articles/630727/)  
[How does the Linux kernel run a program](https://0xax.gitbooks.io/linux-insides/content/SysCall/linux-syscall-4.html)  