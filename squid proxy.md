- squid代理的应用场景
	- 在支持访问外网的主机host上安装squid服务
	- 在安装squid服务的主机host上设置ssh端口转发，使得local的computer1流量可以转发到访问外网的主机上
	- 在local的其他主机如computer2上设置代理为computer1，这样computer2发出的流量可以通过computer1转发给支持外网访问的主机host，从而可以访问外网
		- ```bash
		  # 1. 在支持访问外网的主机host上安装squid服务
		  sudo yum install -y squid
		  编辑squid的配置文件，设置squid监听端口为3128
		  # Squid normally listens to port 3128
		  http_port 3128
		  
		  # 2. 在host上设置端口转发，使得来自主机135.251.92.240端口58080的流量可以转发给host上的squid服务（监听端口为3128）
		  sudo sshpass -p root.123 ssh -CNfR 0.0.0.0:58080:127.0.0.1:3128 root@135.251.92.240
		  
		  # 3.在ip地址为135.251.92.240的主机上修改sshd配置/etc/ssh/sshd_config如下，之后重启sshd服务使之生效
		  #GatewayPorts no
		  GatewayPorts yes
		  
		  # 4. 在本地可以访问135.251.92.240的其他主机上设置代理服务器为135.251.92.240:58080，这样该主机就可以访问外网
		  
		  
		  
		  On host on which squid is installed:
		  root     3091739       1  0 May05 ?        00:00:05 ssh -CNfR 0.0.0.0:58080:127.0.0.1:3128 root@135.251.92.240
		  
		  On 主机135.251.92.240
		  ...
		  
		  
		  ```
			- ![image.png](../assets/image_1672210455265_0.png)
- 查看squid服务log
	- `sudo tail -f /var/log/squid/access.log`
- 参考文档
	- [squid-proxy-configuration-linux](https://linuxhint.com/squid-proxy-configuration-linux/)
	- [Squid Users Guide](https://projects.horms.net/projects/redundant_linux_paper/related/squid/index.html)