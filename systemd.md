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
	- ![LinuxServiceManagementMadeEasyWithSystemd.pdf](./assets/LinuxServiceManagementMadeEasyWithSystemd.pdf)
	- [systemd doc](http://0pointer.de/blog/projects/systemd-docs.html)
	- [man systemd](https://man7.org/linux/man-pages/man1/init.1.html)
	- [man systemctl](https://man7.org/linux/man-pages/man1/systemctl.1.html)
	- [man systemd.unit](https://man7.org/linux/man-pages/man5/systemd.unit.5.html)
	- [systemd blog series](https://www.freedesktop.org/wiki/Software/systemd/)
	- [man journalctl](https://man7.org/linux/man-pages/man1/journalctl.1.html)
	- [The systemd for Administrators Blog Series](https://www.freedesktop.org/wiki/Software/systemd/)
	- [Working with systemd Timers](https://documentation.suse.com/smart/systems-management/html/systemd-working-with-timers/index.html)