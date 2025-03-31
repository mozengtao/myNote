```bash
# 显示进程相关的线程信息
root@slot-120:/proc/3097 [P1]# ps -T -p $(pgrep cms)
PID  SPID TTY          TIME CMD
3097  3097 ?        00:00:18 cms
3097  3413 ?        00:00:02 rdk:main
3097  3414 ?        00:00:00 rdk:broker-1
3097  3415 ?        00:00:00 rdk:broker-1
3097  3416 ?        00:00:01 rdk:broker0

# 查看进程相关的线程
root@weir01e5f0:/proc/28846 []# top -H -p $(pgrep cms)
PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
3329 root      20   0  658024  91348  75428 S   0.3   6.2   4:12.21 cms
18915 root      20   0  658024  91348  75428 S   0.0   6.2   0:18.48 rdk:main
18916 root      20   0  658024  91348  75428 S   0.0   6.2   0:00.68 rdk:broker-1
18917 root      20   0  658024  91348  75428 S   0.0   6.2   0:00.75 rdk:broker-1
18926 root      20   0  658024  91348  75428 S   0.0   6.2   0:08.47 rdk:broker0

$ pstree -p | grep sleep
	|-bash(31207)---sleep(31800)

# 查询匹配的进程
ps -ef | grep "135.242.60.169" | grep -v grep
ps -ef | grep "[1]35.242.60.169"
pgrep -af "135.242.60.169"
```

[[pstree]]  
[man ps](https://man7.org/linux/man-pages/man1/ps.1.html)  
[How to Use the ps Command on Linux](https://pimylifeup.com/ps-command-linux/)  
[Getting a Process’ Child Processes](https://www.baeldung.com/linux/get-process-child-processes)  
```bash
# 1
pgrep -P 6245
6246
6247

# 2
$ pgrep -lP 6245
6246 child.sh
6247 sleep

# 3
pstree -p 6245
parent.sh(6245)---child.sh(6246)---sleep(6248)
				|-sleep(6247)

# 4
ps --ppid 6245
PID TTY          TIME CMD
6246 pts/1    00:00:00 child.sh
6247 pts/1    00:00:00 sleep

# 5
ps --ppid 6245 -o pid,ppid,cmd
PID  PPID CMD
6246  6245 /bin/bash ./child.sh
6247  6245 sleep infinity

# 6
ls /proc/6245/task
6245
cat /proc/6245/task/6245/children
6246 6247
```
	
## 相关命令
- grep
- top
- ps
- pstree