 [tcpdump & libpcap](https://www.tcpdump.org/)  
 [Analyse a tcpdump capture using libpcap in C](https://elf11.github.io/2017/01/22/libpcap-in-C.html)  
 [The Sniffer's Guide to Raw Traffic](https://yuba.stanford.edu/~casado/pcap/section1.html)  
 [Using libpcap in C](https://www.devdungeon.com/content/using-libpcap-c)  
 []()  

 ```bash
	  tcpdump -vv -i eth1 '( vlan and ( ether[14:2] & 0xfff == 1000 or ether[14:2] & 0xfff == 501 )
	  ) and ( ip host 10.1.1.98 or ip host 10.1.1.99 )'
	  
	  sudo tcpdump -vv -i fm1-mac1.umgmt -n \('proto 1'\) or \('icmp6 && ip6[40] == 128'\)
	  注意：对于不同版本的tcpdump, 'or'和'||'的行为可能会有差异，并且加括号'\(\)'和不加括号'\(\)'也可能
	  会有差异，需要根据实际情况进行验证
	  即
	  sudo tcpdump -vv -i fm1-mac1.umgmt -n \('proto 1'\) or \('icmp6 && ip6[40] == 128'\)
	  sudo tcpdump -vv -i fm1-mac1.umgmt -n \('proto 1'\) || \('icmp6 && ip6[40] == 128'\)
	  sudo tcpdump -vv -i fm1-mac1.umgmt -n 'proto 1' or 'icmp6 && ip6[40] == 128'
	  三者的行为可能会有差异
```


[20 Advanced Tcpdump Examples in Linux](https://www.howtouselinux.com/post/20-tcpdump-advanced-examples-on-linux)  
[Tcpdump Command in Linux](https://linuxize.com/post/tcpdump-command-in-linux/)  
[man tcpdump](https://www.tcpdump.org/manpages/tcpdump.1.html)  
[**man pcap-filter**](https://linux.die.net/man/7/pcap-filter)  
[**man pcap-filter**](https://www.tcpdump.org/manpages/pcap-filter.7.html)  
[Packet Filtering HOWTO](https://www.netfilter.org/documentation/HOWTO/packet-filtering-HOWTO.html#toc3)  
[Linux Socket Filtering aka Berkeley Packet Filter (BPF)]https://www.kernel.org/doc/html/latest/networking/filter.html) #bpf
[PROGRAMMING WITH PCAP](https://www.tcpdump.org/pcap.html)  
[Tcpdump advanced filters](https://blog.wains.be/2007/2007-10-01-tcpdump-advanced-filters/)  
[Using libpcap in C](https://www.devdungeon.com/content/using-libpcap-c)  
[a libpcap tutorial in c language](http://yuba.stanford.edu/~casado/pcap/)  
[PCAP(3PCAP) MAN PAGE](https://www.tcpdump.org/manpages/pcap.3pcap.html)  
[libpcap man pages](https://www.tcpdump.org/manpages/)  
[WinPcap Documentation](https://www.winpcap.org/docs/docs_41b5/html/main.html)  
[Using ethtool in Linux](https://www.baeldung.com/linux/using-ethtool)  
[Hex Packet Decoder](https://hpd.gasmi.net/) #online  
[Packetor](https://packetor.com/) #online  
[Scapy](https://scapy.readthedocs.io/en/latest/index.html)  
