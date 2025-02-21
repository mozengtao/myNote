- tcp存活机制
	- 通过函数setsockopt设置socket选项来达到设置keepalive参数的目的
	- ```bash
	  TCP_KEEPIDLE、TCP_KEEPINTVL 和 TCP_KEEPCNT 都是 TCP 协议中的选项，用于保持 TCP 连接的可靠性和稳定性。它们之间存在一定的关系。
	  
	  TCP_KEEPIDLE：指定最长的空闲时间，即在这个时间段内没有数据交互时，TCP 才开始发送心跳包探测对方是否还存活。
	  TCP_KEEPINTVL：表示两次心跳包之间的时间间隔，单位为秒。如果没有收到对方发回的确认应答包（ACK），则会重新发送心跳包。
	  TCP_KEEPCNT：表示在关闭连接之前，尝试发送几次心跳包探测对方是否还存活。默认情况下，Linux kernel 的 keepalive 心跳包探测次数为 9 次。
	  
	  在 Linux kernel 中，TCP_KEEPCNT 的作用是当探测几次都没有回应时，才会视为连接不可用。也就是说，只有经过 TCP_KEEPCNT 次心跳包探测，每次探测之间间隔 TCP_KEEPINTVL 秒，还是没有收到对方回应，才会认为连接失效，并主动关闭连接。
	  
	  同时，TCP_KEEPIDLE 和 TCP_KEEPINTVL 应该设置成合适的值，以控制心跳包探测的频率和持续时间。通常情况下，TCP_KEEPIDLE 应设置为大于 TCP_KEEPINTVL 的两倍，TCP_KEEPINTVL 应该设置得足够小，以保证及时探测连接是否可用。
	  ```
- 参考文档
	- [Linux 内核源码剖析- TCP.IP 实现](https://pc.woozooo.com/mydisk.php) #pdf
	- [Socket Options](https://notes.shichao.io/unp/ch7/#so_keepalive-socket-option)
	- [Multicast over TCP/IP HOWTO](https://tldp.org/HOWTO/Multicast-HOWTO.html)
	- [TCP Keepalive HOWTO](https://tldp.org/HOWTO/TCP-Keepalive-HOWTO/index.html)
	- [TCP Implementation in Linux: A Brief Tutorial](https://cseweb.ucsd.edu/classes/fa09/cse124/presentations/TCPlinux_implementation.pdf)
	- [RFC793](https://www.rfc-editor.org/rfc/rfc793.txt)