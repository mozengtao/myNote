- systemd 以 unit 为基本对象对资源进行管理和配置，unit 代表了可被 systemd 管理的一种资源
- systemd 架构
	- ![](https://www.ruanyifeng.com/blogimg/asset/2016/bg2016030703.png)
-
- unit 类型
	- service
	- socket
	- target
	- device
	- mount
	- automount
	- swap
	- timer
	- path
	- slice
	- scope
-
- target
	- target 是一组相关的 unit 集合，用来方便unit的管理
-
- 示例
	- ```sh
	  systemctl get-default
	  systemctl list-units --type=service --state=failed
	  systemctl list-unit-files --type=target
	  systemctl cat docker.service
	  systemctl show httpd.service
	  ```
- [[sysvinit]]
- 创建systemd timer
	- ```bash
		#To run a unit at specified times or intervals you need two units:
			1. a service unit that defines what to run
			2. a timer unit that defines when to run the service unit
		参考(https://askubuntu.com/questions/1083537/how-do-i-properly-install-a-systemd-timer-and-service)

		#1 创建 example.service
		[Unit]
		Description=An example oneshot service that runs a program

		[Service]
		Type=oneshot
		ExecStart=/tmp/test/timer/mytimer.sh

		[Install]
		WantedBy=multi-user.target

		#2 创建 example.timer
		[Unit]
		Description=A timer that runs our example service

		[Timer]
		OnCalendar=*-*-* *:00/3:00

		[Install]
		WantedBy=timers.target

		#3 拷贝 example.service 和 example.timer到系统路径
		sudo cp example.service /lib/systemd/system
		sudo cp example.timer /lib/systemd/system

		#4 相关命令
		systemctl status example.timer
		sudo systemctl start example.timer
		sudo systemctl status example.timer
		systemctl list-timers
		systemctl status example.timer
		systemd-analyze calendar '*-*-* *:00/3:00'  # success
		systemd-analyze calendar '*-* zz*:00/3:00'  # fail
		sudo systemd-analyze verify /lib/systemd/system/logrotate.timer
	```
- 参考文档
	- [archlinux systemd help](https://wiki.archlinux.org/title/Systemd)
	- [How do I properly install a systemd timer and service]((https://askubuntu.com/questions/1083537/how-do-i-properly-install-a-systemd-timer-and-service)
	- [**systemd Documentation**](https://0pointer.de/blog/projects/systemd-docs.html)
	- [**systemd* help doc**](https://www.freedesktop.org/software/systemd/man/latest/)
	- ![LinuxServiceManagementMadeEasyWithSystemd.pdf](./assets/LinuxServiceManagementMadeEasyWithSystemd.pdf)
	- [systemd doc](http://0pointer.de/blog/projects/systemd-docs.html)
	- [man systemd](https://man7.org/linux/man-pages/man1/init.1.html)
	- [man systemctl](https://man7.org/linux/man-pages/man1/systemctl.1.html)
	- [man systemd.unit](https://man7.org/linux/man-pages/man5/systemd.unit.5.html)
	- [systemd blog series](https://www.freedesktop.org/wiki/Software/systemd/)
	- [man journalctl](https://man7.org/linux/man-pages/man1/journalctl.1.html)
	- [The systemd for Administrators Blog Series](https://www.freedesktop.org/wiki/Software/systemd/)
	- [**Working with systemd Timers**](https://documentation.suse.com/smart/systems-management/html/systemd-working-with-timers/index.html)
	- [Working with systemd Timers pdf](https://documentation.suse.com/smart/systems-management/pdf/systemd-working-with-timers_en.pdf)
	- [Systemd timers onCalendar (cron) format explained](https://silentlad.com/systemd-timers-oncalendar-(cron)-format-explained)
		- Systemd Timer OnCalendar Format
			```bash
			Format: * *-*-* *:*:*
			3 parts:
			1. *
				signify the day of the week eg:- Sat,Thu,Mon
			2. *-*-*
				signify the calendar date. Which means it breaks down to - year-month-date
			3. *:*:*
				signify the time component of the calnedar event. So it is - hour:minute:second

			Examples:
				Explaination			Systemd timer
				Every Minute			*-*-* *:*:00
				Every 2 minute			*-*-* *:*/2:00
				Every 5 minutes			*-*-* *:*/5:00
				Every 15 minutes		*-*-* *:*/15:00
				Every quarter hour		*-*-* *:*/15:00
				Every 30 minutes		*-*-* *:*/30:00
				Every half an hour		*-*-* *:*/30:00
				Every 60 minutes		*-*-* */1:00:00
				Every 1 hour			*-*-* *:00:00
				Every 2 hour			*-*-* */2:00:00
				Every 3 hour			*-*-* */3:00:00
				Every other hour		*-*-* */2:00:00
				Every 6 hour			*-*-* */6:00:00
				Every 12 hour			*-*-* */12:00:00
				Hour Range				*-*-* 9-17:00:00
				Between certain hours	*-*-* 9-17:00:00
				Every day				*-*-* 00:00:00
				Daily					*-*-* 00:00:00
				Once A day				*-*-* 00:00:00
				Every Night				*-*-* 01:00:00
				Every Day at 1am		*-*-* 01:00:00
				Every day at 2am		*-*-* 02:00:00
				Every morning			*-*-* 07:00:00
				Every midnight			*-*-* 00:00:00
				Every day at midnight		*-*-* 00:00:00
				Every night at midnight		*-*-* 00:00:00
				Every sunday				Sun *-*-* 00:00:00
				Every friday				Fri *-*-* 01:00:00
				Every friday at midnight	Fri *-*-* 00:00:00
				Every saturday				Sat *-*-* 00:00:00
				Every weekday				Mon...Fri *-*-* 00:00:00
				weekdays only				Mon...Fri *-*-* 00:00:00
				monday to friday			Mon...Fri *-*-* 00:00:00
				Every weekend				Sat,Sun *-*-* 00:00:00
				weekends only				Sat,Sun *-*-* 00:00:00
				Every 7 days				* *-*-* 00:00:00
				Every week					Sun *-*-* 00:00:00
				weekly	Sun 				*-*-* 00:00:00
				once a week					Sun *-*-* 00:00:00
				Every month					* *-*-01 00:00:00
				monthly						* *-*-01 00:00:00
				once a month				* *-*-01 00:00:00
				Every quarter				* *-01,04,07,10-01 00:00:00
				Every 6 months				* *-01,07-01 00:00:00
				Every year					* *-01-01 00:00:00
			```
	- [systemd-analyze](https://www.freedesktop.org/software/systemd/man/latest/systemd-analyze.html#)
	- [How to use systemd timers (cronjob alternative)](https://silentlad.com/how-to-use-systemd-timers-(cronjob-alternative))