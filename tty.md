[Linux TTY Driver](https://yannik520.github.io/tty/tty_driver.html)  
[Linux TTY framework框架](http://www.wowotech.net/tty_framework/tty_concept.html)  
[What do PTY and TTY Mean?](https://www.baeldung.com/linux/pty-vs-tty)  
[tty (/dev/tty ) vs pts (/dev/pts) in Linux](https://linuxways.net/centos/tty-dev-tty-vs-pts-dev-pts-in-linux/)  
[The TTY demystified](https://www.linusakesson.net/programming/tty/)  
[TTY](https://www.kernel.org/doc/html/latest/driver-api/tty/index.html)  
[Linux设备模型之tty驱动架构分析](http://www.uml.org.cn/embeded/201209071.asp)  
[tty驱动分析](http://www.wowotech.net/tty_framework/435.html)  
[TTY Line Discipline](https://docs.kernel.org/driver-api/tty/tty_ldisc.html#tty-line-discipline)  

> change and print terminal line settings
[stty (Set Teletype)](https://www.mankier.com/1/stty)
```bash
> stty -a
speed 38400 baud; rows 51; columns 188; line = 0;
intr = ^C; quit = ^\; erase = ^?; kill = ^U; eof = ^D; eol = <undef>; eol2 = <undef>; swtch = <undef>; start = ^Q; stop = ^S; susp = ^Z; rprnt = ^R; werase = ^W; lnext = ^V; discard = ^O;
min = 1; time = 0;
-parenb -parodd -cmspar cs8 -hupcl -cstopb cread -clocal -crtscts
-ignbrk -brkint -ignpar -parmrk -inpck -istrip -inlcr -igncr icrnl -ixon -ixoff -iuclc -ixany -imaxbel -iutf8
opost -olcuc -ocrnl onlcr -onocr -onlret -ofill -ofdel nl0 cr0 tab0 bs0 vt0 ff0
isig icanon iexten echo echoe echok -echonl -noflsh -xcase -tostop -echoprt echoctl echoke -flusho -extproc
```