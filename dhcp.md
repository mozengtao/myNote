- 参考文档
	- [Dynamic Host Configuration Protocol](https://www.rfc-editor.org/rfc/rfc1541)
	- [DHCP Lease Time – What is it and How does it work?](https://lazyadmin.nl/home-network/dhcp-lease-time/)
	- [Understanding DHCP Relay Agents](https://www.netmanias.com/en/?m=view&id=techdocs&no=6000&xtag=dhcp-network-protocol&xref=understanding-dhcp-relay-agents)
	- [Network Address Translation (NAT) Overview](https://www.netmanias.com/en/?m=view&id=techdocs&no=6061&tag=4205&page=2)
	- [NAT Behavioral Requirements](https://www.netmanias.com/en/?m=view&id=techdocs&no=6058&tag=4205)
	- [Understanding of the Multicast Protocol: IGMP & PIM-SM](https://www.netmanias.com/en/?m=view&id=techdocs&no=5915&tag=4205&page=2)

	- [dhcp-options](https://man.developpez.com/man5/dhcp-options/#L5)
		```bash
		# /etc/dhcp/dhcpd.conf
		# /etc/dhcp/dhcpd6.conf

		# DEFINING NEW OPTIONS
		option new-name code new-code = definition ; 

		示例：
		option nokia.option61-ctrlr-ip code 61 = array of ip-address;


		# suboption
		示例：
		option space v43;
		option local_vendor43   code 43 = encapsulate v43;
		option v43.dev_type     code 2  = text;
		option v43.ecm          code 3  = text;
		option v43.serial_num   code 4  = text;
		option v43.hw_ver       code 5  = text;
		option v43.sw_ver       code 6  = text;
		option v43.boot_ver     code 7  = text;
		option v43.oui          code 8  = text;
		option v43.model_num    code 9  = text;
		option v43.vendor_name  code 10 = text;
		option v43.ccap_cores   code 61 = array of ip-address;
		
		option v43.dev_type     = "RDP";
		option v43.ecm          = "ECM:eSAFE";
		option v43.serial_num   = "1234";
		option v43.hw_ver       = "1.1.2";
		option v43.sw_ver       = "2.2.3";
		option v43.boot_ver     = "3.3.4";
		option v43.oui          = "00:01:02";
		option v43.model_num    = "RDP_TEST";
		option v43.vendor_name  = "Vecima Networks Inc.";
		#option v43.ccap_cores   192.168.1.1, 192.168.1.2, 192.168.1.3;
		```