- > enter messages into the system log
- 示例
	- ```bash
	  # 运行命令(如ls aaa)并把标准输出和错误输出重定向到系统日志中
	  ls aaa 2>&1 | logger -i -s -p local1.info -t ntp
	  
	  
	  logger -p "local3.info" "This is just a simple log line"  
	  logger -n 192.168.1.10 "This is just a simple log line"    // syslog 协议使用udp协议  
	  logger -n 192.168.1.10 -T "This is just a simple log line"    // syslog 协议使用tcp协议  
	  logger -n 192.168.1.10  -P 1514 "This is just a simple log line"  
	  logger $(who)    // 重定向命令输出作为log输入  
	  logger -f hostnames    // 重定向文件输出作为log输入  
	  ```
- 参考文档
	- [man logger](https://man7.org/linux/man-pages/man1/logger.1.html)