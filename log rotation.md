- log rotation 用来==压缩归档==过去的==日志文件==以==节省磁盘空间==，同时方便后续的 log 检查。
	- > The purpose of log rotation is to archive and compress old logs so that they consume less disk space, but are still available for inspection as needed.
-
- logrotate 命令
	- 配置文件
		- `/etc/logrotate.conf`
			- ==全局==配置文件，可以使用 `include` 语句包含 `/etc/logrotate.d/` 目录下的配置文件
		- `/etc/logrotate.d/`
			- 不同业务的 `logrotate` 配置文件，这些文件中任何==未设置的选项==都默认==继承自== `/etc/logrotate.conf`
	-
	- 配置示例
		- ```sh
		  /var/log/example-app/*.log {
		  	daily
		  	missingok
		  	rotate 14
		  	compress
		  	notifempty
		  	create 0640 www-data www-data
		  	sharedscripts
		  	postrotate
		  		systemctl reload example-app
		  	endscript
		  }
		  ```
			- `daily`: 每天rotate
			- `missingok`: log 缺失时不打印 error 消息.
			- `rotate 12`: 只保存 12 个旧的 log 文件
			- `compress`: 压缩 rotated 文件. 默认使用 `gzip` 压缩并生成 `.gz`文件. 压缩命令可以使用 `compresscmd` 选项进行指定.
			- `notifempty`: 如果文件为空则不 rotate 日志文件.
				- `create 0640 www-data www-data`: rotation 之后创建新的日志文件，并指定 permissions (`0640`), owner (`www-data`), and group (also `www-data`).
			- `sharedscripts`: 这个标志的意思是任何添加在该配置里的脚本每次运行时只运行一次，而不是针对每个 rotated 文件。因为该配置可能匹配 `example-app` 目录下两个及以上的日志文件 ，如果不指定该选项则 `postrotate` 指定的脚本可能会运行多次。
			- `postrotate` to `endscript`: 指定日志文件被 rotated 之后执行的脚本.  其中一个应用场景是使应用使用新的 log 文件，需要注意的是  `postrotate` 在日志被压缩之前运行，文件压缩可能会花费很长时间，应用应该立即切换使用新的日志文件。对于需要在日志被压缩之后运行的任务，应该使用  `lastaction` 命令。
	-
	- 帮助文档
		- [man logrotate](https://linux.die.net/man/8/logrotate)
-