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