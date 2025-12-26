- linux 下的^^周期定时任务管理^^工具，同时可以解释为 cron table，即 cron 的任务列表
-
- 命令格式
	- ```shell
	  MIN HOUR DOM MON DOW CMD
	  ```
-
- 语法格式
	- ```
	  Field    Description    Allowed Value
	  MIN      Minute field    0 to 59
	  HOUR     Hour field      0 to 23
	  DOM      Day of Month    1-31
	  MON      Month field     1-12
	  DOW      Day Of Week     0-6
	  CMD      Command         Any command to be executed.
	  ```
-
- crontab 命令
	- 编辑 crontab 文件
		- crontab -e
	- 显示 crontab 文件内容
		- crontab -l
	- 删除当前 crontab 文件
		- crontab -r
	- 带提示确认的删除当前 crontab 文件
		- crontab -i
	- 编辑其他用户的 crontab 文件
		- crontab -u <username>
-
- 应用举例
	- |Cron Job|Command|
	  |executes at 7 AM and 5 PM daily|0 7,17 * * * /scripts/script.sh|
	  |execute a cron after every 5 minutes|*/5* * * * *  /scripts/script.sh|
	  |execute the task every Monday at 5 AM|0 5 * * mon  /scripts/script.sh|
	  |run your script at 3 minutes intervals|*/3 * * * * /scripts/monitor.sh|
	  |Run Cron Job Every Minute|* * * * * /root/backup.sh|
	  |Run Cron Job Every 30 Minutes|30 * * * * /root/backup.sh|
	  |Run Cron Job Every Hour|0 * * * */root/backup.sh|
	  |Run Cron Job Every Day at Midnight|0 0 * * * /root/backup.sh|
	  |Run Cron Job at 2 am Every Day|0 2 * * * /root/backup.sh|
	  |Run Cron Job Every 1st of the Month|0 0 1 * * /root/backup.sh|
	  |Run Cron Job Every 15th of the Month|0 0 15 * * /root/backup.sh|
	  |Run Cron Job on December 1st – Midnight|0 0 0 12 * /root/backup.sh|
	  |Run Cron Job on Saturday at Midnight|0 0 * * 6 /root/backup.sh|
	  |Run task in Feb, June, Sep|* * * feb,jun,sep * /script/script.sh|
	  |Run task each Mon and Wed at 5 PM|0 17 * * mon,wed  /script/script.sh|
	  |Run task on first Saturday of every month|0 2 * * sat  [ $(date +%d) -le 06 ] && /script/script.sh|
	  |Run task for 6 hours interval|0 */6 * * * /scripts/script.sh|
	  |Run task twice on Monday and Tuesday|0 4,17 * * mon,tue /scripts/script.sh|
	  |Run task on a daily basis|@daily /scripts/script.sh|
	  |Run task on system reboot|@reboot /scripts/script.sh|
	  |Run multiple tasks using a single cron|* * * * * /scripts/script.sh; /scripts/scrit2.sh|
-
- cron
	- cron 是linux上的一个系统守护进程，主要负责检测和执行周期性的定时任务
	- cron ^^每1分钟^^执行的完整周期
		- ![cron-cycle-2](https://www.junosnotes.com/wp-content/uploads/2021/07/cron-cycle-2.png)
-
- cron job 语法
	- ```
	  .---------------- minute (0 - 59)
	  |  .------------- hour (0 - 23)
	  |  |  .---------- day of month (1 - 31)
	  |  |  |  .------- month (1 - 12) OR jan,feb,mar,apr ...
	  |  |  |  |  .---- day of week (0 - 6) (Sunday=0 or 7) OR sun,mon,tue,wed,thu,fri,sat
	  |  |  |  |  |
	  *  *  *  *  *  user-name  command to be executed
	  
	  @yearly (or @annually) Equivalent to 0 0 1 1 *.
	  @monthly  Equivalent to 0 0 1 * *.
	  @weekly   Equivalent to 0 0 * * 0.
	  @daily    Equivalent to 0 0 * * *.
	  @hourly   Equivalent to 0 * * * *.
	  @reboot   Run the specified task at the system startup (boot-time).
	  ```
- 系统相关的 cron jobs 文件
	- ```shell
	  root@slot-120:/etc [P1]# ls -l | grep cron
	  drwxr-xr-x 2 root root     0 Oct  8 15:41 cron.d
	  drwxr-xr-x 2 root root     0 Nov 21 15:49 cron.daily
	  -rw-r--r-- 1 root root     0 Mar  9  2018 cron.deny
	  drwxr-xr-x 2 root root     0 Mar  9  2018 cron.hourly
	  drwxr-xr-x 2 root root     0 Mar  9  2018 cron.monthly
	  drwxr-xr-x 2 root root     0 Mar  9  2018 cron.weekly
	  -rw------- 1 root root   653 Mar  9  2018 crontab
	  ```
		- **anacrontab**
			- linux 上的 anacron 服务所用到的文件
		- **cron.d**
		  background-color:: red
			- 该目录包含了 cron job 的文件列表，cron service 会读取该目录下的文件列表
			  background-color:: red
		- **cron.daily**
			- 该目录包含了每天都会执行的 cron job 的文件列表
		- **cron.hourly, cron.monthly, cron.weekly**
		- **crontab**:
			- 该文件指定 user 来运行周期性的定时任务。
- Anacron cron 文件
	- Anacron cron 文件在 ^^7:00 到 11pm 之间每半个小时执行一次^^
	- Anacron service 负责运行 cron jobs 以防cron job没有在第一时间被执行，即 Anacron service 会自动检测上一次的 job 是否执行，如果没有执行的话它会自动执行
- 帮助文档
	- [man crontab](https://www.man7.org/linux/man-pages/man5/crontab.5.html)
	- [crontab online editor](https://crontab.guru/)