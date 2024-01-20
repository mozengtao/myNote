- 交互模式命令
	- 在top命令运行时可以通过按键^^h^^查看交互模式命令的帮助文档
	  collapsed:: true
		- ```bash
		  Help for Interactive Commands - procps-ng 3.3.16
		  Window 1:Def: Cumulative mode Off.  System: Delay 3.0 secs; Secure mode Off.
		  
		    Z,B,E,e   Global: 'Z' colors; 'B' bold; 'E'/'e' summary/task memory scale
		    l,t,m     Toggle Summary: 'l' load avg; 't' task/cpu stats; 'm' memory info
		    0,1,2,3,I Toggle: '0' zeros; '1/2/3' cpus or numa node views; 'I' Irix mode
		    f,F,X     Fields: 'f'/'F' add/remove/order/sort; 'X' increase fixed-width
		  
		    L,&,<,> . Locate: 'L'/'&' find/again; Move sort column: '<'/'>' left/right
		    R,H,J,C . Toggle: 'R' Sort; 'H' Threads; 'J' Num justify; 'C' Coordinates
		    c,i,S,j . Toggle: 'c' Cmd name/line; 'i' Idle; 'S' Time; 'j' Str justify
		    x,y     . Toggle highlights: 'x' sort field; 'y' running tasks
		    z,b     . Toggle: 'z' color/mono; 'b' bold/reverse (only if 'x' or 'y')
		    u,U,o,O . Filter by: 'u'/'U' effective/any user; 'o'/'O' other criteria
		    n,#,^O  . Set: 'n'/'#' max tasks displayed; Show: Ctrl+'O' other filter(s)
		    V,v     . Toggle: 'V' forest view; 'v' hide/show forest view children
		  
		    k,r       Manipulate tasks: 'k' kill; 'r' renice
		    d or s    Set update interval
		    W,Y       Write configuration file 'W'; Inspect other output 'Y'
		    q         Quit
		            ( commands shown with '.' require a visible task display window )
		  
		  ```
	- 常用交互命令
		- ```bash
		  Toggle: 
		  	'R' Sort; 
		      'H' Threads;
		  	'c' Cmd name/line;
		  ```
- 常用选项
	- ```bash
	         -H  :Threads-mode operation
	              Instructs  top  to display individual threads.  Without this command-line option a summation of all threads in each process is shown.  Later this can be changed with the `H'
	              interactive command.
	  
	         -p  :Monitor-PIDs mode as:  -pN1 -pN2 ...  or  -pN1,N2,N3 ...
	              Monitor  only processes with specified process IDs.  This option can be given up to 20 times, or you can provide a comma delimited list with up to 20 pids.  Co-mingling both
	              approaches is permitted.
	  
	  ```
- 参考文档
	- [man top](https://man7.org/linux/man-pages/man1/top.1.html)
	- [A Guide to the Linux “Top” Command](https://www.booleanworld.com/guide-linux-top-command/)
	- [16 Top Command Examples in Linux](https://www.tecmint.com/12-top-command-examples-in-linux/)
	- [How to Use the top Command in Linux](https://phoenixnap.com/kb/top-command-in-linux)
-