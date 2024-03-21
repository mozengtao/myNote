- What is the magic SysRq key?
	- It is a 'magical' key combo you can hit which the kernel will respond to regardless of whatever else it is doing, unless it is completely locked up.
  ```bash
  # trigger kernel panic
  echo c > /proc/sysrq-trigger

  在putty下
  telnet 10.254.22.223 2003
  telnet> send brk (之后快速按 c 字符，可以触发 linux 立即重启)
  ```
- [Linux Magic System Request Key Hacks](https://www.kernel.org/doc/html/latest/admin-guide/sysrq.html#what-are-the-command-keys)