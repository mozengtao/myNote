- What is the magic SysRq key?
	- #+BEGIN_QUOTE
	  It is a 'magical' key combo you can hit which the kernel will respond to regardless of whatever else it is doing, unless it is completely locked up.
	  #+END_QUOTE
- ```bash
  # trigger kernel panic
  echo c > /proc/sysrq-trigger
  ```
- [Linux Magic System Request Key Hacks](https://www.kernel.org/doc/html/latest/admin-guide/sysrq.html#what-are-the-command-keys)