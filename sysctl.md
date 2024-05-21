- 配置运行时的linux内核运行时参数，适用的内核参数列表在[[proc sys]]目录下
- 示例
	```sh

	-a, --all
			Display all values currently available.

	$ sysctl -a  | grep ipv4
	net.ipv4.cipso_cache_bucket_size = 10
	net.ipv4.cipso_cache_enable = 1
	net.ipv4.cipso_rbm_optfmt = 0
	net.ipv4.cipso_rbm_strictvalid = 1
	...

	$ cat /proc/sys/net/ipv4/cipso_cache_bucket_size
	10

	cat /proc/sys/kernel/core_pattern
	# or
	sysctl kernel.core_pattern
	
	# disable printing the key name
	sysctl -n kernel.core_pattern
	
	
	morrism@localhost$ sysctl -a --pattern hung_task
	kernel.hung_task_check_count = 4194304
	kernel.hung_task_panic = 0
	kernel.hung_task_timeout_secs = 120
	kernel.hung_task_warnings = 10
	```
-
- 参考文档
	- [man sysctl](https://man7.org/linux/man-pages/man8/sysctl.8.html)
	- [Sysctl Explorer](https://sysctl-explorer.net/)
	- [Documentation for /proc/sys](https://www.kernel.org/doc/html/next/admin-guide/sysctl/)