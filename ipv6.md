- ipv6路由
	- 显示路由
	  ip -6 route show [dev <device>]
	  route -A inet6
	  route -n6
	  
	  添加路由
	  ip -6 route add <ipv6network>/<prefixlength> via <ipv6address> [dev <device>]
	  route -A inet6 add <ipv6network>/<prefixlength> gw <ipv6address> [dev <device>]
	  example: route -A inet6 add default gw 2001:0db8:0:f101::1
	  
	  ip -6 route add <ipv6network>/<prefixlength> dev <device> metric 1
	  example: ip -6 route add default dev eth0 metric 1
	  route -A inet6 add <ipv6network>/<prefixlength> dev <device>
	  example:route -A inet6 add default dev eth0
	  
	  删除路由
	  ip -6 route del <ipv6network>/<prefixlength> via <ipv6address> [dev <device>]
	  route -A inet6 del <network>/<prefixlength> gw <ipv6address> [dev <device>]
	  example: route -A inet6 del default gw 2001:0db8:0:f101::1
	  
	  ip -6 route del <ipv6network>/<prefixlength> dev <device>
	  example: ip -6 route del default dev eth0
	  route -A inet6 del <network>/<prefixlength> dev <device>
	  example: route -A inet6 del default dev eth0
- 参考文档
	- [Linux IPv6 HOWTO](https://tldp.org/HOWTO/Linux+IPv6-HOWTO/index.html)
	- [IPv6 in Practice](https://wiki.spoje.net/lib/exe/fetch.php/howto/ipv6_in_practice_springer-2007_.pdf)