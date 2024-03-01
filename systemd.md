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