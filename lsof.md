- 示例
	- ```bash
	  lsof  -i
	  lsof  -i 6
	  lsof  -iTCP
	  lsof  -i :22
	  lsof  -i@172.16.12.5
	  lsof  -i@172.16.12.5:22
	  lsof  -i -sTCP:LISTEN
	  lsof  -i -sTCP:ESTABLISHED
	  lsof  -u daniel
	  lsof  -u ^daniel
	  kill  -9  `lsof -t -u daniel`
	  lsof  -c syslog-ng
	  lsof  -c ssh -c init
	  lsof  -p 10075
	  lsof  /var/log/messages/
	  lsof  /home/daniel/firewall_whitelist.txt
	  lsof  -u daniel -i @1.1.1.1
	  kill  -HUP `lsof -t -c sshd`
	  lsof  +L1
	  lsof  +d /usr/lib
	  lsof  +D /var/log/
	  lsof  +D /home -u ^mary
	  lsof  -t /usr/share/mime/mime.cache
	  lsof  -u mary -c ssh -a
	  lsof  -u mary -c ssh -a -r5
	  lsof  -i -a -p 932650
	  lsof  -i -a -c ssh
	  lsof  -i tcp:25
	  lsof  -i udp:53
	  lsof  -i :1-1024
	  ```
- 参考文档
	- [lsof Command in Linux with Examples](https://phoenixnap.com/kb/lsof-command)
	- [man lsof](https://linux.die.net/man/8/lsof)
	- [Linux lsof Command Examples](https://www.thegeekstuff.com/2012/08/lsof-command-examples/)