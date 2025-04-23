## 使用代理
```bash
export HTTP_PROXY="http://135.242.60.169:58080"
export HTTPS_PROXY="http://135.242.60.169:58080"

```

[SSH Port Forwarding (SSH Tunneling) Explained](https://phoenixnap.com/kb/ssh-port-forwarding)  
[How to do SSH Tunneling (Port Forwarding)](https://www.ramkitech.com/2012/04/how-to-do-ssh-tunneling-port-forwarding.html)  
[SSH local port forwarding](http://michalszalkowski.com/security/pivoting-tunneling-port-forwarding/ssh-local-port-forwarding/)  
[SSH remote port forwarding](http://michalszalkowski.com/security/pivoting-tunneling-port-forwarding/ssh-remote-port-forwarding/)  
```bash
### SSH Port Forwarding

## Local Port Forwarding
ssh -L local_port:destination_server_ip:remote_port ssh_server_hostname

# all traffic sent to port 5901 on your local host is being forwarded to port 4492 on the remote server located at 188.17.0.5
ssh -L 5901:188.17.0.5:4492 pnap@ssh.server.com

# Forward one port
ssh -L 1234:localhost:3306 target@IP	# -L tells the SSH client to request the SSH server to forward all the data we send via the port 1234 to localhost:3306 on the Target server
# Forward multiple ports
ssh -L 1234:localhost:3306 8080:localhost:80 target@10.129.202.64

## Remote Port Forwarding
ssh -R remote_port:localhost:local_port ssh_server_hostname

# instruct the remote server ssh.server.com to forward any connections directed at port 8080 to the local resource listening on port 5534
# users with access to the SSH server are now able to access resources on the local machine
ssh –R 8080:localhost:5534 pnap@ssh.server.com
```

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
		  ssh -CNfR 0.0.0.0:58080:127.0.0.1:3128 root@135.251.92.240
		  
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
	- [Squid configuration directives](https://www.squid-cache.org/Doc/config/)
	- [Squid Documentation](https://www.squid-cache.org/Doc/)
	- [Squid Proxy Server 3.1 Beginner’s Guide.pdf](https://github.com/neverusedname/MyBooks/blob/master/Squid%20Proxy%20Server%203.1%20Beginner%E2%80%99s%20Guide%20.pdf)
	- [squid-proxy-configuration-linux](https://linuxhint.com/squid-proxy-configuration-linux/)
	- [Squid Users Guide](https://projects.horms.net/projects/redundant_linux_paper/related/squid/index.html)