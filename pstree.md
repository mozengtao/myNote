- > display a tree of processes
- 示例
	- ```bash
	  $ pstree -p | grep sleep
	       |-bash(31207)---sleep(31800)
	  
	  morrism@PC2886:/tmp$ pstree -p
	  init(1)─┬─init(7)───init(8)─┬─bash(9)───tmux: client(12)
	          │                   └─tmux: server(14)─┬─bash(15)
	          │                                      └─bash(19)───pstree(3507)
	          └─{init}(6)
	  
	  root@slot-18:~ [P1]# pstree $(pidof mcp)
	  mcp-+-bpimgr
	      |-cms-+-ptp4l
	      |     `-4*[{cms}]
	      |-confd.smp-+-erl_child_setup
	      |           `-17*[{confd.smp}]
	      |-docsis_mac
	      |-dproxy
	      |-4*[dsnpu]
	      |-fwdmgr
	      |-hal---{hal}
	      |-log
	      |-netmgr
	      |-si5345drv
	      |-spectrum
	      |-swmgr
	      |-synce
	      |-usnpu
	      `-usspectrum
	  ```
- 参考文档
	- [man pstree](https://man7.org/linux/man-pages/man1/pstree.1.html)