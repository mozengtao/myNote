- kill - terminate a process
- ```bash
  The command kill sends the specified signal to the specified processes or process groups
  
  If signal is 0, then no actual signal is sent, but error checking is still performed.
  
  
  Example:
  # bash实现spin功能
  #！/usr/bin/bash
  SPIN='-\|/'
  function spin {
          i=0
          while kill -0 $1 2> /dev/null
          do
                  i=$(( (i+1)%4 ))
                  printf "\b${SPIN:$i:1}"
                  sleep .1
          done
          printf "\bDONE\n"
  }
  
  echo 1
  sleep 20 &
  spin $!
  ```
- 参考
	- [kill(1)](https://man7.org/linux/man-pages/man1/kill.1.html)
	- [Termination Signals](https://www.gnu.org/software/libc/manual/html_node/Termination-Signals.html)
	- [SIGKILL signal handling](https://stackoverflow.com/questions/15766036/sigkill-signal-handling)
	- [What does a program do when it's sent SIGKILL signal?](https://unix.stackexchange.com/questions/485644/what-does-a-program-do-when-its-sent-sigkill-signal)
	- [killall5(8)](https://linux.die.net/man/8/killall5)