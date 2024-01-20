- wget 用来从指定的 URL 下载文件
-
- wget 通过指定代理下载文件
	- ~/.wgetrc
		- 用户启动文件
	- /etc/wgetrc
		- 默认全局启动文件
			- ```bash
			  http_proxy = http://[Proxy_Server]:[port]
			  https_proxy = http://[Proxy_Server]:[port]
			  ftp_proxy = http://[Proxy_Server]:[port]
			  ```
	- 从 shell 或者当前终端设置的 proxy 变量
		- ```bash
		  export http_proxy=http://[Proxy_Server]:[port]
		  export https_proxy=$http_proxy
		  export ftp_proxy=$http_proxy
		  
		  # verify the variable values
		  $ env | grep proxy
		  http_proxy=http://[Proxy_Server]:[port]
		  https_proxy=http://[Proxy_Server]:[port]
		  ftp_proxy=http://[Proxy_Server]:[port]
		  ```
	- ~/.bash_profile
		- 用户特定环境变量
	- /etc/profile
		- 系统全局环境变量
		- ```bash
		  export http_proxy=http://[Proxy_Server]:[port]
		  export https_proxy=http://[Proxy_Server]:[port]
		  export ftp_proxy=http://[Proxy_Server]:[port]
		  ```
-
- 帮助文档
	- [wgetrc commands](https://www.gnu.org/software/wget/manual/html_node/Wgetrc-Commands.html)