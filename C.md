- [[常用函数]]
	- ioctl, fcntl
	- mmap, munmap
	- fopen, fread, fwrite, fclose, ferror
	- popen, pclose
	- open, read, write, close
	- fprintf,  sprintf, snprintf
	- fscanf, scanf
	- strchr, strrchr, strstr, strtok, strtok_r
	- strdup
	- strlen
	- access
	- strncmp, strcasecmp
	- fgets
	- system
	- isalpha, isxdigit
	- atoi, strtoul, strtol
	- toupper, tolower
	- malloc, free
	- getenv
	- regcomp, regexec, regfree
	- fork, execve
	- waitpid
	- memset, memmove, memcpy
		- ```bash
		  void *memcpy(void *dest, const void *src, size_t n);
		  The memcpy() function copies n bytes from memory area src to memory area dest.  
		  The memory areas must not overlap.  Use memmove(3) if the memory areas do overlap.
		  
		  void *memmove(void *dest, const void *src, size_t n);
		  The  memmove()  function  copies n bytes from memory area src to memory area dest.  
		  The memory areas may overlap: copying takes place as though the bytes in src are first 
		  copied into a temporary array that does not overlap src or dest, and the bytes are then
		  copied from the temporary array to dest.
		  ```
	- kill
	- reboot
	- select, poll, epoll
		- #+BEGIN_QUOTE
		         int poll(struct pollfd *fds, nfds_t nfds, int timeout);
		  
		             struct pollfd {
		                 int   fd;         /* file descriptor */
		                 short events;     /* requested events */
		                 short revents;    /* returned events */
		             };
		  
		         The caller should specify the number of items in the fds array in
		         nfds.
		  
		         The field fd contains a file descriptor for an open file.
		  
		         The field events is an input parameter, a bit mask specifying the
		         events the application is interested in for the file descriptor
		         fd.
		  
		         The field revents is an output parameter, filled by the kernel
		         with the events that actually occurred. 
		  #+END_QUOTE
	- socket, getsockopt, setsockopt
	- pthread_mutex_init, pthread_mutex_destroy
	- pthread_mutex_lock， pthread_mutex_unlock
	- pthread_mutexattr_init, pthread_mutexattr_setpshared
	- shm_open, shm_unlink
	- truncate, ftruncate
	- pthread_self, pthread_setaffinity_np, pthread_getaffinity_np
	- strerror
		- 检查系统调用或库函数失败原因
		  collapsed:: true
			- ```c
			  #include <stdio.h>
			  #include <string.h>
			  #include <errno.h>
			  
			  int main(void)
			  {
			          FILE *fp;
			          char filename[128];
			  
			          snprintf(filename, sizeof(filename), "test.txt");
			          fp = fopen(filename, "r");
			          if(fp == NULL) {
			                          printf("fopen %s failed: %s\n", filename, strerror(errno));
			            				//printf("fopen %s failed: %m\n", filename);
			          }
			  
			          return 0;
			  }
			  ```
	- waitpid
		- 检查进程状态改变
		  collapsed:: true
			- ```c
			  	// 利用 timer 或者别的机制周期性 check 进程状态，之后进行自动重启进程等操作
			  	// return immediately if no child has exited
			  	pid = waitpid(0, &status, WNOHANG);
			  	if(pid <= 0)
			  		return;
			  	
			  	// ...
			  ```
	- raise
		- 发送信号给当前进程自己
		  collapsed:: true
			- ```c
			  #include <signal.h>
			  
			  void sig_handler(int signo, siginfo_t *info, void *ucontext)
			  {
			  	struct sigaction act;
			  
			  	memset(&act, 0, sizeof(act));
			  	// inside first-level signal handler, set signal handler to default action
			  	act.sa_handler = SIG_DFL;
			  
			  	sigaction(signo, &act, NULL);
			  
			  	/* Take appropriate actions according to needs */
			  	......
			  
			  	// send a signal to the caller, 在本例中会触发系统的默认的信号处理
			  	raise(signo);
			  }
			  
			  
			  int set_sighandler()
			  {
			  	int r;
			  	struct sigaction sa;
			  
			  	memset(&sa, 0, sizeof(sa));
			  	sa.sa_sigaction = sig_handler;
			  	sa.sa_flags = SA_SIGINFO;
			  
			  	// assume signo is SIGUSR1
			  	r = sigaction(SIGUSR1, &sa, NULL);
			  	if(r < 0) {
			  		printf("sigaction failed: %s", strerror(errno));
			  		return -1;
			  	}
			  	/* Further code */
			  	
			    	return 0;
			  }
			  ```
	- statvfs
		- 获取文件系统统计信息
		  collapsed:: true
			- ```c
			  #include <stdio.h>
			  #include <string.h>
			  #include <errno.h>
			  #include <sys/statvfs.h>
			  
			  #define SWM_TMP_SPACE 200
			  
			  int
			  chk_tmp_space(void)
			  {
			  	int r;
			  	int free;
			  	struct statvfs vfs;
			  
			  	memset(&vfs, 0, sizeof(vfs));
			  	r = statvfs("/", &vfs);
			  	if(r) {
			  		printf("statvfs failed: %s", strerror(errno));
			  		return -1;
			  	}
			  
			  	free = (vfs.f_bsize * vfs.f_bfree) / (1024 * 1024);
			  
			  	printf("tmp space required %dMB free %dMB\n",SWM_TMP_SPACE, free);
			  
			  	if(free < SWM_TMP_SPACE) {
			  		printf("Not enough space, required %dMB, available %dMB\n", SWM_TMP_SPACE, free);
			  		return -1;
			  	}
			  
			  	return 0;
			  }
			  ```
	- popen
	  collapsed:: true
		- 解析shell命令执行结果，并在程序中进行使用
			- [bond0.txt](../assets/bond0_1670403143750_0.txt)
			- ```c
			  #include <stdio.h>
			  #include <stdlib.h>
			  #include <string.h>
			  #include <errno.h>
			  
			  enum {
			  	/* lacp churn machine states */
			  	CHURN_MONITOR	= 0,	/* monitoring for churn */
			  	CHURNED		= 1,	/* churn detected (error) */
			  	NO_CHURN	= 2,	/* no churn (no error) */
			  
			  	BLK_AGGR	= 0,	/* parsing aggregator info */
			  	BLK_S1		= 1,	/* parsing slave 1 (fm1-mac1.p1) info */
			  	BLK_S1_ACTOR	= 2,	/* parsing slave 1 actor lacp pdu */
			  	BLK_S1_PARTNER	= 3,	/* parsing slave 1 partner lacp pdu */
			  	BLK_S2		= 4,	/* parsing slave 2 (fm1-mac1.p1) info */
			  	BLK_S2_ACTOR	= 5,	/* parsing slave 2 actor lacp pdu */
			  	BLK_S2_PARTNER	= 6,	/* parsing slave 2 partner lacp pdu */
			  };
			  
			  typedef struct slave_status_t slave_status_t;
			  struct slave_status_t
			  {
			  	int	actor_system_prio;
			  	int	actor_port_key;
			  	int	actor_port_prio;
			  	int	actor_port_number;
			  	int	actor_port_state;
			  	int	actor_churn_state;
			  	int	actor_churn_count;
			  	int	partner_system_prio;
			  	int	partner_oper_key;
			  	int	partner_port_prio;
			  	int	partner_port_number;
			  	int	partner_port_state;
			  	int	partner_churn_state;
			  	int	partner_churn_count;
			  };
			  
			  void print_slave_status(slave_status_t *slave)
			  {
			  	printf("actor_system_prio:%d\n", slave->actor_system_prio);
			  	printf("actor_port_key:%d\n", slave->actor_port_key);
			  	printf("actor_port_prio:%d\n", slave->actor_port_prio);
			  	printf("actor_port_number:%d\n", slave->actor_port_number);
			  	printf("actor_port_state:%d\n", slave->actor_port_state);
			  	printf("actor_churn_state:%d\n", slave->actor_churn_state);
			  	printf("actor_churn_count:%d\n", slave->actor_churn_count);
			  	printf("partner_system_prio:%d\n", slave->partner_system_prio);
			  	printf("partner_oper_key:%d\n", slave->partner_oper_key);
			  	printf("partner_port_prio:%d\n", slave->partner_port_prio);
			  	printf("partner_port_number:%d\n", slave->partner_port_number);
			  	printf("partner_port_state:%d\n", slave->partner_port_state);
			  	printf("partner_churn_state:%d\n", slave->partner_churn_state);
			  	printf("partner_churn_count:%d\n", slave->partner_churn_count);
			  }
			  
			  int main(void)
			  {
			  	FILE *fp;
			  	int blk, val;
			  	const char delim[] = ":";
			  	slave_status_t	slaves[2], *s1, *s2;
			  	char cmd[128], line[256], *fstr, *vstr;
			  
			    	s1 = &slaves[0];
			  	s2 = &slaves[1];
			  
			  	snprintf(cmd, sizeof(cmd), "cat bond0.txt");
			  
			  	fp = popen(cmd, "r");
			  	if(fp == NULL) {
			  		printf("popen failed: %s\n", strerror(errno));
			  		return -1;
			  	}
			  
			  	blk = BLK_AGGR;
			  	while(fgets(line, sizeof(line), fp) != NULL) {
			  		if(strstr(line, delim) == NULL)
			  			continue;
			  		
			  		fstr = strtok(line, delim);
			  		vstr = strtok(NULL, "");
			  
			  		if((fstr == NULL) ||
			  		   ((vstr == NULL) &&
			  		   (strcasestr(fstr, "details actor lacp pdu") == NULL) &&
			  		   (strcasestr(fstr, "details partner lacp pdu") == NULL)))
			  		   	continue;
			  		
			  		if(strcasestr(fstr, "Slave Interface") &&
			  			strcasestr(vstr, "fm1-mac1.p1")) {
			  				blk = BLK_S1;
			  			}
			  		else if(strcasestr(fstr, "Slave Interface") &&
			  			strcasestr(vstr, "fm1-mac1.p2")) {
			  			blk = BLK_S2;
			  		}
			  		else if(strcasestr(fstr, "details actor lacp pdu")) {
			  			if(blk == BLK_S1)
			  				blk = BLK_S1_ACTOR;
			  			else if(blk == BLK_S2)
			  				blk = BLK_S2_ACTOR;
			  		}
			  		else if(strcasestr(fstr, "details partner lacp pdu")) {
			  			if(blk == BLK_S1_ACTOR)
			  				blk = BLK_S1_PARTNER;
			  			else if(blk == BLK_S2_ACTOR)
			  				blk = BLK_S2_PARTNER;
			  		}
			  		else if(strcasestr(fstr, "Actor Churn State")) {
			  			if(strcasestr(vstr, "monitoring"))
			  				val = CHURN_MONITOR;
			  			else if(strcasestr(vstr, "churned"))
			  				val = CHURNED;
			  			else
			  				val = NO_CHURN;
			  
			  			if(blk == BLK_S1)
			  				s1->actor_churn_state = val;
			  			else if(blk == BLK_S2)
			  				s2->actor_churn_state = val;
			  		}
			  		else if(strcasestr(fstr, "Partner Churn State")) {
			  			if(strcasestr(vstr, "monitoring"))
			  				val = CHURN_MONITOR;
			  			else if(strcasestr(vstr, "churned"))
			  				val = CHURNED;
			  			else
			  				val = NO_CHURN;
			  
			  			if(blk == BLK_S1)
			  				s1->partner_churn_state = val;
			  			else if(blk == BLK_S2)
			  				s2->partner_churn_state = val;
			  		}
			  		else if(strcasestr(fstr, "Actor Churned Count")) {
			  			if(blk == BLK_S1)
			  				s1->actor_churn_count = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S2)
			  				s2->actor_churn_count = strtoul(vstr, NULL, 0);
			  		}
			  		else if(strcasestr(fstr, "Partner Churned Count")) {
			  			if(blk == BLK_S1)
			  				s1->partner_churn_count = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S2)
			  				s2->partner_churn_count = strtoul(vstr, NULL, 0);
			  		}
			  		else if(strcasestr(fstr, "system priority")) {
			  			if(blk == BLK_S1_ACTOR)
			  				s1->actor_system_prio = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S1_PARTNER)
			  				s1->partner_system_prio = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S2_ACTOR)
			  				s2->actor_system_prio = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S2_PARTNER)
			  				s2->partner_system_prio = strtoul(vstr, NULL, 0);
			  		}
			  		else if(strcasestr(fstr, "port key")) {
			  			if(blk == BLK_S1_ACTOR)
			  				s1->actor_port_key = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S2_ACTOR)
			  				s2->actor_port_key = strtoul(vstr, NULL, 0);
			  		}
			  		else if(strcasestr(fstr, "oper key")) {
			  			if(blk == BLK_S1_PARTNER)
			  				s1->partner_oper_key = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S2_PARTNER)
			  				s2->partner_oper_key = strtoul(vstr, NULL, 0);
			  		}
			  		else if(strcasestr(fstr, "port priority")) {
			  			if(blk == BLK_S1_ACTOR)
			  				s1->actor_port_prio = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S1_PARTNER)
			  				s1->partner_port_prio = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S2_ACTOR)
			  				s2->actor_port_prio = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S2_PARTNER)
			  				s2->partner_port_prio = strtoul(vstr, NULL, 0);
			  		}
			  		else if(strcasestr(fstr, "port number")) {
			  			if(blk == BLK_S1_ACTOR)
			  				s1->actor_port_number = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S1_PARTNER)
			  				s1->partner_port_number = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S2_ACTOR)
			  				s2->actor_port_number = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S2_PARTNER)
			  				s2->partner_port_number = strtoul(vstr, NULL, 0);
			  		}
			  		else if(strcasestr(fstr, "port state")) {
			  			if(blk == BLK_S1_ACTOR)
			  				s1->actor_port_state = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S1_PARTNER)
			  				s1->partner_port_state = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S2_ACTOR)
			  				s2->actor_port_state = strtoul(vstr, NULL, 0);
			  			else if(blk == BLK_S2_PARTNER)
			  				s2->partner_port_state = strtoul(vstr, NULL, 0);
			  		}
			  	}
			  
			  	print_slave_status(s1);
			  	printf("----------------------------\n");
			  	print_slave_status(s2);
			  
			  	pclose(fp);
			  
			  	return 0;
			  }
			  ```
	- strtok
		- 可以指定多个分隔符
		- strtok 内部维护 last token 的位置，因此 strtok 操作的字符串必须保存在 buffer 数组里，因为 strtok 会原地修改数组内容
		  collapsed:: true
			- 以 `char buf[] = "this,is,a,string";`为例，连续调用 strtok 之后 buffer 数组的内容如下
			- ```c
			  t  h  i  s  ,  i  s  ,  a  ,  s  t  r  i  n  g \0         this,is,a,string
			  
			  t  h  i  s  \0 i  s  ,  a  ,  s  t  r  i  n  g \0         this
			  ^
			  t  h  i  s  \0 i  s  \0 a  ,  s  t  r  i  n  g \0         is
			                 ^
			  t  h  i  s  \0 i  s  \0 a  \0 s  t  r  i  n  g \0         a
			                          ^
			  t  h  i  s  \0 i  s  \0 a  \0 s  t  r  i  n  g \0         string
			                                ^
			  ```
	- scanf, fscanf, sscanf, vscanf, vsscanf, vfscanf
		- 输入格式化转换
			- scanf使用空白符(newlines, tabs, and spaces)将输入分割成各个不同的域
			- > scanf matches up consecutive conversion specifications to consecutive fields, skipping over the whitespace in between.
		- [C scanf format specifier](https://www.demo2s.com/c/c-scanf-format-specifier.html)
		- [带过滤器的scanf读取字符和字符串](https://www.demo2s.com/c/c-reading-characters-and-string-using-scanf-with-filter.html)
	- kill
		- 发送信号给指定进程
		  collapsed:: true
			- [test.sh](../assets/test_1670551623156_0.sh)
			- ```c
			  #include <stdio.h>
			  #include <stdlib.h>
			  #include <unistd.h>
			  #include <string.h>
			  #include <sys/types.h>
			  #include <signal.h>
			  #include <sys/wait.h>
			  #include <errno.h>
			  
			  int check_process_stop(int pid)
			  {
			  	int r, status;
			  
			  	r = waitpid(pid, &status, WNOHANG);
			  	if(r == 0)
			  		return 1;
			  
			  	if(r < 0) {
			  		printf("waitpid failed: %s\n", strerror(errno));
			  		return 1;
			  	}
			  
			  	printf("process %d exited: r = %d\n", pid, r);
			  
			  	return 0;
			  }
			  
			  int main(void)
			  {
			  	int n, i, r, ai, my_pid;
			  	const char *av[10];
			  	char cmd[256];
			  
			  	av[0] = "/mnt/c/Users/morrism/Downloads/test.sh";
			  	av[1] = "param1";
			  	ai = 2;
			  	av[ai++] = "param2";
			  	av[ai] = NULL;
			  
			  	n = 0;
			  	for(i = 0; i < ai; i++)
			  		n += snprintf(cmd + n, sizeof(cmd) - n, " %s", av[i]);
			  	printf("cmd:%s\n", cmd);
			  
			  	my_pid = fork();
			  	switch(my_pid) {
			  	default:
			  		// in parent process, doSomething such as monitoring
			  		printf("This is parent process\n");
			  		break;
			  	case -1:
			  		printf("fork failed: %s\n", strerror(errno));
			  		my_pid = 0;
			  		break;
			  	case 0:
			  		setpgid(0, 0);
			  		execv(av[0], (char*const*)av);
			  		exit(-1);
			  	}
			  
			    	// sleep 5 secs for test purpose
			  	sleep(5);
			  
			  	// tear down child process
			  	r = kill(my_pid, SIGTERM);
			  	if(r < 0)
			  		printf("kill failed: %s\n", strerror(errno));
			  	
			  	for(i = 0; i < 100; i++) {
			  		r = check_process_stop(my_pid);
			  		if(r == 0) {
			  			my_pid = 0;
			  			break;
			  		}
			  		usleep(100);
			  	}
			  
			  	if(my_pid != 0) {
			  		printf("waitpid did not succeed: r = %d\n, pocess %d is now a zombie process", r, my_pid);
			  		my_pid = 0;
			  	}
			  
			  	return 0;
			  }
			  
			  ```
	- sysconf
		- 获取运行时配置信息
		  collapsed:: true
			- ```c
			  #include <sys/types.h>
			  #include <unistd.h>
			  #include <stdio.h>
			  #include <errno.h>
			  
			  int main()
			  {
			  	int ncore;
			  	errno = 0;
			  
			  	if ((ncore = sysconf(_SC_NPROCESSORS_ONLN)) == -1)
			  		if (errno == 0)
			  			printf("_SC_NPROCESSORS_ONLN not supported by this implementation.\n");
			  		else
			  			perror("sysconf error.");
			  	else
			  		printf("ncore = %d\n", ncore);
			  }
			  ```
	- sysinfo
		- 获取系统信息
		  collapsed:: true
			- ```c
			  #include <sys/sysinfo.h>
			  #include <stdio.h>
			  #include <string.h>
			  #include <errno.h>
			  
			  #define MB (1024 * 1024)
			  
			  int main(void)
			  {
			  	int r;
			  	struct sysinfo si;
			  	int totalram, freeram, percent_inuse;
			  
			  	r = sysinfo(&si);
			  	if(r < 0) {
			  		printf("sysinfo failed: %s", strerror(errno));
			  		return -1;
			  	}
			  
			  	totalram = si.totalram/MB;
			  	freeram = si.freeram/MB;
			  	percent_inuse = (int)((float)(totalram - freeram) / (float)totalram * 100.0);
			  
			  	printf("totalram: %d(MB), freeram:%d(MB), percent_inuse:%d\%\n",
			  		totalram, freeram, percent_inuse);
			  
			  	return 0;
			  }
			  ```
	- readlink
		- 读取符号链接对应的文件
		  collapsed:: true
			- ```c
			  #include <stdio.h>
			  #include <stdlib.h>
			  #include <unistd.h>
			  
			  #include <stdio.h>
			  
			  int main(void)
			  {
			  	int r;
			  	char *tz_path;
			  	char lpath[256], cmd[128];
			  	const char *symlink = "/etc/localtime";
			  
			  	r = readlink(symlink, lpath, sizeof(lpath));
			  	if(r < 0) {
			  		printf("readlink failed\n");
			  		return -1;
			  	}
			  
			  	printf("The value for symbol link %s is %s\n", symlink, lpath);
			  
			  	tz_path = "/usr/share/zoneinfo/Asia/Shanghai";
			  	snprintf(cmd, sizeof(cmd), "ln -sf %s /etc/localtime", tz_path);
			  
			  	r = system(cmd);
			  	if(r != 0) {
			  		printf("system failed\n");
			  		return -1;
			  	}
			  	printf("symlink %s to target %s succeed\n", symlink, tz_path);
			  
			  	return 0;
			  }
			  ```
	- stat, fstat
		- 获取文件状态
		  collapsed:: true
			- ```c
			  #include <stdio.h>
			  #include <stdlib.h>
			  #include <sys/types.h>
			  #include <sys/stat.h>
			  #include <unistd.h>
			  #include <fcntl.h>
			  
			  int main(void)
			  {
			  	struct stat st;
			  	int r, n, fd;
			  	char *buf;
			  
			  	fd = open("file.txt", O_RDONLY);
			  	if(fd < 0) {
			  		printf("open failed\n");
			  		return -1;
			  	}
			  
			  	r = fstat(fd, &st);
			  	if(r < 0 || st.st_size == 0)
			  		n = 1024;
			  	else
			  		n = st.st_size;
			  	
			  	buf = malloc(n);
			  
			  	n = read(fd, buf, n);
			  	if(n > 0)
			  		printf("buf:%s", buf);
			  	
			  	free(buf);
			  	close(fd);
			  
			  	return 0;
			  }
			  ```
		- [man 2 stat](https://man7.org/linux/man-pages/man2/lstat.2.html)
	- uname
		- 获取操作系统信息
		  collapsed:: true
			- ```c
			  #include <sys/types.h>
			  #include <sys/utsname.h>
			  #include <stdio.h>
			  
			  int main()
			  {
			  	struct utsname sysInfo;
			  	if (uname(&sysInfo) != -1) {
			  		puts(sysInfo.sysname);
			  		puts(sysInfo.nodename);
			  		puts(sysInfo.release);
			  		puts(sysInfo.version);
			  		puts(sysInfo.machine);
			  	}
			  	else
			  		perror("uname() error");
			  }
			  ```
- 参考文档
	- [C Operator Precedence](https://en.cppreference.com/w/c/language/operator_precedence)
	- [**The GNU C Library**](https://www.gnu.org/software/libc/manual/html_node/index.html#SEC_Contents) #online
	-[**The GNU C Library**](http://herbert.the-little-red-haired-girl.org/html/glibc/libc_toc.html)
	- [c函数使用参考实例](https://bytefreaks.net/category/programming-2/c-programming-2)
	- [POXIS Function Reference Example](https://support.sas.com/documentation/onlinedoc/sasc/doc/lr2/lrv2ch20.htm#lr2posix)
	- [C standard](https://www.open-std.org/jtc1/sc22/wg14/www/standards.html)
	- [**glibc source code**](https://elixir.bootlin.com/glibc/latest/source) #sourcecode
	- [The GNU C Library](https://www.gnu.org/software/libc/manual/html_mono/libc.html)
	- [Memory Layout Of A C Program](https://hackthedeveloper.com/memory-layout-c-program/)
	- [C Program Compilation Process](https://hackthedeveloper.com/c-program-compilation-process/)
	- [C 语言常见问题集](https://c-faq-chn.sourceforge.net/ccfaq/ccfaq.html)
	- [Notes for C and C++ Programming](https://www.codeinsideout.com/blog/c-cpp/notes/)
	- [c for dummies](https://c-for-dummies.com/)
	- [C and C++ Projects](https://www.codewithc.com/c-projects-with-source-code/)
	- [How to split/tokenize a string? (strtok() string tokenizer)](https://www.equestionanswers.com/c/c-strtok.php)
	- 柔性数组成员
		- flexible array memeber
			- [declaring a flexible array member](https://wiki.sei.cmu.edu/confluence/display/c/DCL38-C.+Use+the+correct+syntax+when+declaring+a+flexible+array+member)
	- 类型声明
		- [How to interpret complex C/C++ declarations](https://www.codeproject.com/Articles/7042/How-to-interpret-complex-C-C-declarations)
		- [How To Read C Declarations](https://parrt.cs.usfca.edu/doc/how-to-read-C-declarations.html)
		- [Reading C type declarations](http://unixwiz.net/techtips/reading-cdecl.html)
- [Awesome C](https://github.com/oz123/awesome-c?tab=readme-ov-file#awesome-c) #github
- [C](https://github.com/marcotrosi/C)
- [**Programming in C**](https://users.cs.cf.ac.uk/dave/C/)
- [HOWTO: Use Address Sanitizer](https://www.osc.edu/resources/getting_started/howto/howto_use_address_sanitizer)
- [Clang 19.0.0git documentation](https://clang.llvm.org/docs/index.html)
- [Signal Handling](https://www.gnu.org/software/libc/manual/html_node/Signal-Handling.html)
- [Introduction To Unix Signals Programming](https://www.cs.kent.edu/~ruttan/sysprog/lectures/signals.html)
- [CS 43203 : System Programming](https://www.cs.kent.edu/~ruttan/sysprog/)
 - [Unix Pthreads tutorial](https://www.cs.kent.edu/~ruttan/sysprog/)
 - [C program to implement linked list](https://www.programmingsimplified.com/c/data-structures)
 - [Using Templates and Generics in C](https://levelup.gitconnected.com/using-templates-and-generics-in-c-968da223154d)
 - [Tutorial: Generics in C](https://itnext.io/tutorial-generics-in-c-b3362b3376a3)
 - [Generic Programming in C](https://cs.boisestate.edu/~amit/teaching/253/handouts/07-c-generic-coding-handout.pdf)
 - [void * and Function Pointers](https://web.stanford.edu/class/archive/cs/cs107/cs107.1202/lab4/)
 - [Functional Pointer and Callback in C++](https://primerpy.medium.com/functional-pointer-and-callback-in-c-86c208df6b2f)
 - [C and C++ Language Syntax Reference](https://www.cprogramming.com/reference/)
 - [Understanding the LD_LIBRARY_PATH Environment Variable](https://tecadmin.net/understanding-the-ld_library_path-environment-variable/)
 - []()
 - []()
 - []()
 - []()