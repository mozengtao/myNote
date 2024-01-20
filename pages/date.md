- > print or set the system date and time
- 常用命令
	- ```bash
	  # 以指定格式显示当前时间
	  date +FORMAT
	  date +'%F'
	  -->  2023-02-03
	  date_now=$(date "+%F %H:%M:%S")
	  echo "${date_now}"
	  -->  2023-02-03 10:34:12
	  
	  # 按指定格式显示给定的字符串时间
	  date -d '2023-02-02 21:30:17' +'%c'
	  -->  Thu 02 Feb 2023 09:30:17 PM CST
	  date -d '2023-02-02 21:30:17' +'%s'
	  -->  1675344617
	  
	  # 显示Epoch time(显示POSIX时间到当前时间的间隔)
	  date +'%s'
	  -->  1675391206
	  
	  date -d @1675391206
	  -->  Fri Feb  3 10:26:46 CST 2023
	  
	  # 设置日期和时间
	  date --set='20230203 10:28'
	  -->  Fri Feb  3 10:28:00 CST 2023
	  
	  date --set='10:28'
	  -->  Fri Feb  3 10:28:00 CST 2023
	  
	  # 显示UTC时间
	  morrism@localhost ~ $ date -u
	  Tue Sep 12 06:27:03 UTC 2023
	  
	      -u, --utc, --universal
	           print or set Coordinated Universal Time (UTC)
	  
	  # 显示时区相关信息
	  root@slot-120:~ [P1]# TZ=America/New_York date +%z
	  -0400
	  root@slot-120:~ [P1]# TZ=America/New_York date +%Z
	  EDT
	  
	  ```
- 参考文档
	- [man date](https://man7.org/linux/man-pages/man1/date.1.html)