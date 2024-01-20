- pexpect 是一个 python 模块，用来产生并控制子进程使其对特定的输出 pattern 进行响应。
- > From https://pexpect.readthedocs.io/en/stable/overview.html
  There are two important methods in Pexpect – [expect()](https://pexpect.readthedocs.io/en/stable/api/pexpect.html#pexpect.spawn.expect) and [send()](https://pexpect.readthedocs.io/en/stable/api/pexpect.html#pexpect.spawn.send) (or [sendline()](https://pexpect.readthedocs.io/en/stable/api/pexpect.html#pexpect.spawn.sendline) which is like [send()](https://pexpect.readthedocs.io/en/stable/api/pexpect.html#pexpect.spawn.send) with a linefeed). The [expect()](https://pexpect.readthedocs.io/en/stable/api/pexpect.html#pexpect.spawn.expect) method waits for the child application to return a given string. The string you specify is a regular expression, so you can match complicated patterns. The [send()](https://pexpect.readthedocs.io/en/stable/api/pexpect.html#pexpect.spawn.send) method writes a string to the child application. From the child’s point of view it looks just like someone typed the text from a terminal. ^^After each call to [expect()](https://pexpect.readthedocs.io/en/stable/api/pexpect.html#pexpect.spawn.expect) the before and after properties will be set to the text printed by child application. The before property will contain all text up to the expected string pattern. The after string will contain the text that was matched by the expected pattern.^^ The match property is set to the [re match object](http://docs.python.org/3/library/re#match-objects).
- > pexpect中最重要的两个方法是expect和send(sendline基于send，多了一个换行符)。expect方法等待子应用返回给定的字符串，用户指定的字符串是一个正则表达式。send方法发送特定的字符串给子应用，从子应用的角度来看，就像有用户从子应用的终端输入了相应的字符串。每次调用expect，before和after属性都会被设置，设置的内容为子应用所打印的输出文本。before属性会包含所有直到期望的字符串pattern的文本，after属性会包含期望pattern所匹配的文本。
- Tips
	- 在某些环境下通过sendline函数给子应用发送命令时需要添加`\r`才能成功
		- ```bash
		  ssh.sendline('diag-shell\r')
		  ```
- 参考文档
	- [Pexpect](https://pexpect.readthedocs.io/en/stable/)