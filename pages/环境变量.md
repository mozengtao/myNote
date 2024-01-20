- 环境变量
	- > **Environment variables** are variables that are available system-wide and are inherited by all spawned child processes and shells.
	- ```bash
	  KEY=value
	  KEY="Some other value"
	  KEY=value1:value2
	  
	  环境变量名称大小写敏感，通常大写
	  如果环境变量包含多个值，则用冒号进行分割
	  赋值=后左右不包含空格
	  ```
	- 相关命令
		- env
			- 打印环境变量或者在不修改当前环境的情况下在自定义环境下运行应用
		- printenv
			- 打印所有环境变量或者指定的环境变量
				- ```bash
				  printenv HOME PWD
				  ```
		- set
			- 设置或者取消shell变量
			- 不带参数时set会打印包括环境变量，shell变量和shell函数在内的所有变量
		- unset
			- 删除shell变量或者环境变量
		- export
			- 设置环境变量
- Shell变量
	- > **Shell variables** are variables that apply only to the current shell instance. Each shell such as `zsh` and `bash`, has its own set of internal shell variables
- 参考文档
	- [How to Set and List Environment Variables in Linux](https://linuxize.com/post/how-to-set-and-list-environment-variables-in-linux/)
	- [Environment Variables & Attacks](https://www.ics.uci.edu/~goodrich/teach/cs201P/notes/02_Environment_Variables.pdf)