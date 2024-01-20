- linux下的网卡类型
	- 物理网卡
	- 子网卡
		- `eth0:1`
		- 依赖于物理网卡，有独立的配置文件
	- 虚拟网卡
		- `eth0.1`
		- 将物理网卡加入不同的vlan而生成的vlan虚拟网卡
		- 创建虚拟网卡
			- ```bash
			  # 依赖内核模块8021q
			  [root@linuxtechi ~]# lsmod | grep -i 8021q
			  8021q                  29022  0
			  garp                   14384  1 8021q
			  mrp                    18542  1 8021q
			  ```
			- 临时添加虚拟网卡
				- ```bash
				  ip link add link eth1 name eth1.2 type vlan id 2
				  ip link set dev eth1.2 up
				  ip addr add 172.168.10.51/24 dev eth1.2
				  ```
			- 永久添加虚拟网卡
				- ```bash
				  # eth1 configuration in /etc/sysconfig/network-scripts/ifcfg-eth1
				  TYPE=Ethernet
				  DEVICE=enp0s3
				  BOOTPROTO=none
				  ONBOOT=yes
				  
				  # 创建文件 /etc/sysconfig/network-scripts/ifcfg-eth1.2
				  DEVICE=eth1.2
				  BOOTPROTO=none
				  ONBOOT=yes
				  IPADDR=172.168.10.51
				  PREFIX=24
				  NETWORK=172.168.10.0
				  VLAN=yes
				  
				  # 最后需要重启网络
				  systemctl restart network
				  ```
			- 查看虚拟网卡vlan设置
				- ```bash
				  root@slot-120:~ [P2]# cat /proc/net/vlan/config
				  VLAN Dev name    | VLAN ID
				  Name-Type: VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD
				  fm1-mac1.p1    | 23  | fm1-mac1
				  fm1-mac1.p2    | 24  | fm1-mac1
				  fm1-mac1.umgmt | 30  | fm1-mac1
				  fm1-mac1.v1    | 31  | fm1-mac1
				  ```
		-
- 参考文档
	- [TUN/TAP Interface](https://hechao.li/2018/05/21/Tun-Tap-Interface/)
	- [Introduction to Linux interfaces for virtual networking](https://developers.redhat.com/blog/2018/10/22/introduction-to-linux-interfaces-for-virtual-networking#)
	- [**TUN/TAP interface (on Linux)**](https://www.gabriel.urdhr.fr/2021/05/08/tuntap/)
	- [Universal TUN/TAP device driver](https://www.kernel.org/doc/Documentation/networking/tuntap.txt)
	- [What Is the TUN Interface Used For?](https://www.baeldung.com/linux/tun-interface-purpose)
	- [Creating Network Interfaces and Checking Interface Types](https://www.baeldung.com/linux/create-check-network-interfaces)