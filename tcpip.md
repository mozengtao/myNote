[**An Introduction to Computer Networks**](https://intronetworks.cs.luc.edu/current/uhtml/index.html)  
[**TCP/IP Tutorial and Technical Overview**](https://www.redbooks.ibm.com/redbooks/pdfs/gg243376.pdf)  
[**TCP-IP_Illustrated**](https://github.com/WeitaoZhu/TCP-IP_Illustrated/tree/master)  
[TCP/IP FOR DUMMIES](https://edu.anarcho-copy.org/TCP%20IP%20-%20Network/TCP-IP%20For%20Dummies.pdf)  
[TCP/IP Protocol Suite](https://vaibhav2501.wordpress.com/wp-content/uploads/2012/02/tcp_ip-protocol-suite-4th-ed-b-forouzan-mcgraw-hill-2010-bbs.pdf)  
[Active Queue Management (AQM)](https://www.excentis.com/blog/blog-6/active-queue-management-aqm-the-key-to-reducing-latency-81)  
[Awesome Networking](https://github.com/facyber/awesome-networking)  
[COMPUTER NETWORKS](https://csc-knu.github.io/sys-prog/books/Andrew%20S.%20Tanenbaum%20-%20Computer%20Networks.pdf)  
[Internet-Routing-Architectures](https://github.com/camoverride/lit/tree/master)  
[Proportional Integral Controller Enhanced (PIE)](https://datatracker.ietf.org/doc/rfc8033/)  
[]()  
[]()  
[]()  
[]()  
[]()  
[Enterprise Numbers](https://www.iana.org/assignments/enterprise-numbers/)  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  


[如何学习 Linux 内核网络协议栈](https://segmentfault.com/a/1190000021227338)  
[IP Fragmentation](https://homepages.uc.edu/~thomam/Net1/Misc/ip_fragmentation.html)  
[一文理解 K8s 容器网络虚拟化](https://www.0xffffff.org/2022/03/20/43-k8s/)  
[Monitoring and Tuning the Linux Networking Stack: Receiving Data](https://blog.packagecloud.io/monitoring-tuning-linux-networking-stack-receiving-data/)  
[Monitoring and Tuning the Linux Networking Stack: Sending Data](https://blog.packagecloud.io/monitoring-tuning-linux-networking-stack-sending-data/)  
[Illustrated Guide to Monitoring and Tuning the Linux Networking Stack: Receiving Data](https://blog.packagecloud.io/illustrated-guide-monitoring-tuning-linux-networking-stack-receiving-data/)  



[Reserved IP addresses](https://en.wikipedia.org/wiki/Reserved_IP_addresses)
```bash
Address block	Address range				Scope			Description
0.0.0.0/8		0.0.0.0–0.255.255.255		Software		Current (local, "this") network
127.0.0.0/8		127.0.0.0–127.255.255.255	Host			Used for loopback addresses to the local host
......
```

[Difference Between IP Address 127.0.0.1 and 0.0.0.0](https://www.baeldung.com/linux/difference-ip-address)
```bash
# 127.0.0.1	The Loopback Address

# 0.0.0.0		The Unspecified Address
In the official RFC 1122, it says that no packet should have its destination set as 0.0.0.0, and it’s only accepted to set 0.0.0.0 as its source address during the IP address initialization stage. Specifically, when a device first joins a network, it will send a packet indicating its source address as 0.0.0.0. This would let the DHCP server know that the device needs an IP address assignment.

the IP address 0.0.0.0 can take on different meanings depending on our context:
1. Server Listening Interface: binding our server to the 0.0.0.0 interface means we want to accept traffic from all of the available interfaces
2. Routing and Gateway: For a routing destination, the IP address 0.0.0.0 means the default route

```

[Monitoring and Tuning the Linux Networking Stack: Receiving Data](https://blog.packagecloud.io/monitoring-tuning-linux-networking-stack-receiving-data/)  
[Illustrated Guide to Monitoring and Tuning the Linux Networking Stack: Receiving Data](https://blog.packagecloud.io/illustrated-guide-monitoring-tuning-linux-networking-stack-receiving-data/)  
[Monitoring and Tuning the Linux Networking Stack: Sending Data](https://blog.packagecloud.io/monitoring-tuning-linux-networking-stack-sending-data/)  
[TCP/IP Tutorial and Technical Overview](https://www.redbooks.ibm.com/redbooks/pdfs/gg243376.pdf)  


## tcp存活机制
	- 通过函数setsockopt设置socket选项来达到设置keepalive参数的目的
	- ```bash
	  TCP_KEEPIDLE、TCP_KEEPINTVL 和 TCP_KEEPCNT 都是 TCP 协议中的选项，用于保持 TCP 连接的可靠性和稳定性。它们之间存在一定的关系。
	  
	  TCP_KEEPIDLE：指定最长的空闲时间，即在这个时间段内没有数据交互时，TCP 才开始发送心跳包探测对方是否还存活。
	  TCP_KEEPINTVL：表示两次心跳包之间的时间间隔，单位为秒。如果没有收到对方发回的确认应答包（ACK），则会重新发送心跳包。
	  TCP_KEEPCNT：表示在关闭连接之前，尝试发送几次心跳包探测对方是否还存活。默认情况下，Linux kernel 的 keepalive 心跳包探测次数为 9 次。
	  
	  在 Linux kernel 中，TCP_KEEPCNT 的作用是当探测几次都没有回应时，才会视为连接不可用。也就是说，只有经过 TCP_KEEPCNT 次心跳包探测，每次探测之间间隔 TCP_KEEPINTVL 秒，还是没有收到对方回应，才会认为连接失效，并主动关闭连接。
	  
	  同时，TCP_KEEPIDLE 和 TCP_KEEPINTVL 应该设置成合适的值，以控制心跳包探测的频率和持续时间。通常情况下，TCP_KEEPIDLE 应设置为大于 TCP_KEEPINTVL 的两倍，TCP_KEEPINTVL 应该设置得足够小，以保证及时探测连接是否可用。
	  ```

[Linux 内核源码剖析- TCP.IP 实现](https://pc.woozooo.com/mydisk.php) #pdf  
[Socket Options](https://notes.shichao.io/unp/ch7/#so_keepalive-socket-option)  
[Multicast over TCP/IP HOWTO](https://tldp.org/HOWTO/Multicast-HOWTO.html)  
[TCP Keepalive HOWTO](https://tldp.org/HOWTO/TCP-Keepalive-HOWTO/index.html)  
[TCP Implementation in Linux: A Brief Tutorial](https://cseweb.ucsd.edu/classes/fa09/cse124/presentations/TCPlinux_implementation.pdf)  
[RFC793](https://www.rfc-editor.org/rfc/rfc793.txt)  