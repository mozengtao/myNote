- > columnate lists
- 示例
	- ```bash
	  morrism@localhost ~/repos/fsl-u-boot (morris-test) $ mount | column -t
	  sysfs                on  /sys                             type  sysfs            (rw,nosuid,nodev,noexec,relatime,seclabel)
	  proc                 on  /proc                            type  proc             (rw,nosuid,nodev,noexec,relatime)
	  ......
	  
	  mount | column –t                   # 默认空格进行制表
	  cat /etc/passwd | column -t -s:     # 指定冒号进行制表
	  ```
- 参考文档
	- [man column](https://man7.org/linux/man-pages/man1/column.1.html)