- Linux Daemon（守护进程）是运行在后台的一种特殊进程。它==独立于控制终端==并且周期性地执行某种任务或等待处理某些发生的事件。不需要用户输入，对整个系统或某项业务提供服务。
- 守护进程特点
	- 无需控制终端(不需要与用户交互）
	  collapsed:: true
		- 守护进程为什么要脱离终端
		  collapsed:: true
			- 为了避免进程被任何终端所产生的信息所打断，其在执行过程中的信息也不在任何终端上显示。由于在 Linux 中，每一个系统与用户进行交流的界面称为终端，每一个从此终端开始运行的进程都会依附于这个终端，这个终端就称为这些进程的控制终端，当控制终端被关闭时，相应的进程都会自动关闭
	- 后台运行
	- 生命周期比较长，一般是随系统启动和关闭
- 如何查看守护进程
  
  ```sh
  morrism@PC2886:~$ ps axj
   PPID   PID  PGID   SID TTY      TPGID STAT   UID   TIME COMMAND
    0     1     0     0 ?           -1 Sl       0   0:00 /init
    1     8     7     7 ?           -1 S        0   0:00 /init
    8    31     9     9 ?           -1 S        0   0:00 /usr/sbin/xrdp-sesman
    8    34     9     9 ?           -1 S      105   0:00 /usr/sbin/xrdp
    8    83    83    83 ?           -1 Ss    1000   0:12 tmux
    1   232   232   232 ?           -1 Ss       0   0:00 /init
  232   233   232   232 ?           -1 S        0   0:01 /init
  233   234   234   234 pts/2      239 Ss    1000   0:00 -bash
  234   239   239   234 pts/2      239 S+    1000   0:00 tmux a
   83   240   240   240 pts/3      240 Ss+   1000   0:00 -bash
   83   261   261   261 pts/0     1123 Ss    1000   0:01 -bash
  261  1123  1123   261 pts/0     1123 R+    1000   0:00 ps axj
  ```
	- 守护进程特点：
		- 没有控制终端（TTY为?）
		- 基本上以超级用户启动（UID为0）
		- 终端进程组ID为-1（TPGID）
-
- 守护进程的启动方式
	- 在系统启动时由启动脚本启动，这些启动脚本通常放在 /etc/rc.d 目录下；
	- 利用 inetd 超级服务器启动，如 telnet 等；
	- 由 [[crontab]] 定时启动以及在终端用 [[nohup]] 启动的进程也是守护进程
-
- 编写守护进程
  
  > Given all this, we can see how it fits into the “standard” process for daemonising:
  1.  `fork()` and terminate the parent — this ensures the new process is an orphan (adopted by `init`) and also returns control to the calling shell.
  2.  `setsid()` to create a new process group and session — we can only do this after the `fork()` above because otherwise we’d be a process group leader. This has detached us from the controlling terminal, which is exactly what daemons should do.
  3.  `fork()` a second time — I believe this is simply so we’re not longer a session leader and can never re-acquire a controlling terminal. There may be additional, more subtle, reasons of which I’m unaware.
  4.  `chdir("/")` or some other directory on which the daemon relies — this is to avoid the daemon keeping a directory active which would prevent it being unmounted. If there’s some directory the daemon requires then it actually may be preferable for it to stay active to prevent accidental unmounting.
  5.  `umask(0)` just to clear any permissions mask we may have inherited.
  6.  `close()` standard file descriptors `0`, `1` and `2`, which are standard input, output and error respectively. Since we’re detached from our terminal it’s not clear where they’ve been directed to anyway. Note that some daemons determine the highest possible file descriptor using [`sysconf()`](http://man7.org/linux/man-pages/man3/sysconf.3.html) with `_SC_OPEN_MAX` and call `close()` on them all (ignoring errors) just in case the parent had any other open files — this may be overkill if you’re confident in the behaviour of your calling process, but if you’re at all uncertain it’s the safest course, to avoid wasting file descriptors (of which there’s a finite number available).
  7.  `open()` three times for each of the file descriptors, redirecting them to somewhere sensible. This could be `/dev/null` or `/dev/console`, or perhaps a log file you’ve already opened. Some code assumes file descriptors will be allocated sequentially so they just assume that the next three `open()` calls will get descriptors `0`-`2`, but to be doubly sure you can use [`dup2()`](http://man7.org/linux/man-pages/man2/dup.2.html) — in that case, however, you should have opened the replacement descriptor _before_ the previous step, otherwise you could have a clash.
  
  ```c
  #include <unistd.h> 
  #include <signal.h> 
  #include <fcntl.h>
  #include <sys/syslog.h>
  #include <sys/param.h> 
  #include <sys/types.h> 
  #include <sys/stat.h> 
  #include <stdio.h>
  #include <stdlib.h>
  #include <time.h>
  
  int init_daemon(void)
  { 
  	int pid; 
  	int i;
  
  	// 1）屏蔽一些控制终端操作的信号
  	signal(SIGTTOU,SIG_IGN); 
  	signal(SIGTTIN,SIG_IGN); 
  	signal(SIGTSTP,SIG_IGN); 
  	signal(SIGHUP ,SIG_IGN);
  
  	// 2）在后台运行
    if( pid=fork() ){ // 父进程
        exit(0); //结束父进程，子进程继续
  	}else if(pid< 0){ // 出错
  perror("fork");
  exit(EXIT_FAILURE);
  	}
  
  	// 3）脱离控制终端、登录会话和进程组
  	setsid();  
  
  	// 4）禁止进程重新打开控制终端
  	if( pid=fork() ){ // 父进程
  exit(0);      // 结束第一子进程，第二子进程继续（第二子进程不再是会话组长） 
  	}else if(pid< 0){ // 出错
  perror("fork");
  exit(EXIT_FAILURE);
  	}  
  
  	// 5）关闭打开的文件描述符
  	// NOFILE 为 <sys/param.h> 的宏定义
  	// NOFILE 为文件描述符最大个数，不同系统有不同限制
  	for(i=0; i< NOFILE; ++i){
  close(i);
  	}
  
  	// 6）改变当前工作目录
  	chdir("/tmp"); 
  
  	// 7）重设文件创建掩模
  	umask(0);  
  
  	// 8）处理 SIGCHLD 信号
  	signal(SIGCHLD,SIG_IGN);
  
  	return 0; 
  } 
  
  int main(int argc, char *argv[]) 
  {
  	init_daemon();
  
  	while(1) {
  // do something
  	}
  
  	return 0;
  }
  
  ```
- 关于以上过程的一些解释
	- 屏蔽一些控制终端操作的信号
		- 防止守护进行在没有运行起来前，控制终端受到干扰退出或挂起
	- 在后台运行
		- 避免挂起控制终端将守护进程放入后台执行。方法是在进程中调用 fork() 使父进程终止， 让守护进行在子进程中后台执行
	- 脱离控制终端、登录会话和进程组
		- Linux 中的 进程与控制终端，登录会话和进程组之间的关系：进程属于一个进程组，进程组号（GID）就是进程组长的进程号（PID）。登录会话可以包含多个进程组。这些进程组共享一个控制终端。这个控制终端通常是创建进程的 shell 登录终端。 控制终端、登录会话和进程组通常是从父进程继承下来的。 我们的目的就是要摆脱它们 ，使之不受它们的影响。因此需要调用 `setsid()` 使子进程成为新的会话组长。setsid() 调用成功后，进程成为新的会话组长和新的进程组长，并与原来的登录会话和进程组脱离。由于会话过程对控制终端的独占性，进程同时与控制终端脱离。
	- 禁止进程重新打开控制终端
		- 进程成为无终端的会话组长， 但它可以重新申请打开一个控制终端。可以通过使进程不再成为会话组长来禁止进程重新打开控制终端，采用的方法是再次创建一个子进程
	- 关闭打开的文件描述符
		- 进程从创建它的父进程那里继承了打开的文件描述符。如不关闭，将会浪费系统资源，造成进程所在的文件系统无法卸下以及引起无法预料的错误。
	- 改变当前工作目录
		- 进程活动时，其工作目录所在的文件系统不能卸下。一般需要将工作目录改变到根目录。对于需要转储核心，写运行日志的进程将工作目录改变到特定目录如 /tmp
	- 重设文件创建掩码
		- 进程从创建它的父进程那里继承了文件创建掩模。它可能修改守护进程所创建的文件的存取权限。为防止这一点，将文件创建掩模清除
	- 处理 SIGCHLD 信号
		- 对于某些进程，特别是服务器进程往往在请求到来时生成子进程处理请求。如果父进程不等待子进程结束，子进程将成为僵尸进程（zombie）从而占用系统资源。如果父进程等待子进程结束，将增加父进程的负担，影响服务器进程的并发性能。在 Linux 下可以简单地将 SIGCHLD 信号的操作设为 SIG_IGN。
- 参考
	- [[start-stop-daemon]]
	- [**Daemon Processes**](https://notes.shichao.io/apue/ch13/)
	- [1.7 How do I get my program to act like a daemon?](http://web.archive.org/web/20120914180018/http://www.steve.org.uk/Reference/Unix/faq_toc.html#TOC16)
	- [Run Bash Script as a Daemon](https://www.baeldung.com/linux/bash-daemon-script)
	- [daemon(3)](https://man7.org/linux/man-pages/man3/daemon.3.html)
	- [daemonize — A tool to run a command as a daemon](http://software.clapper.org/daemonize/)
	- [Linux daemonize](https://stackoverflow.com/questions/3095566/linux-daemonize)
		- ```bash
		  I suggest not writing your program as a daemon at all. Make it run in the foreground 
		  with the file descriptors, current directory, process group, etc as given to it.
		  
		  If you want to then run this program as a daemon, use start-stop-daemon(8), init(8), 
		  runsv (from runit), upstart, systemd, or whatever to launch your process as a daemon. 
		  That is, let your user decide how to run your program and don't enforce that it must run
		  as a daemon.
		  ```
	- [Basic skeleton of a linux daemon written in C](https://github.com/pasce/daemon-skeleton-linux-c)