## python 在线环境
[Python Playground](https://programiz.pro/ide/python)  

## Python Projects
[python beginner projects](https://github.com/Mrinank-Bhowmick/python-beginner-projects/tree/main)  
[Python Projects You Can Build](https://realpython.com/tutorials/projects/)  
[python mini projects](https://github.com/Python-World/python-mini-projects/tree/master)  

## string 方法
[String Methods](https://docs.python.org/3/library/stdtypes.html#string-methods)  

## F-string
[Python's F-String for String Interpolation and Formatting](https://realpython.com/python-f-strings/)  
[Python f-string: A Complete Guide](https://www.datacamp.com/tutorial/python-f-string)  

## argparse
[Build Command-Line Interfaces With Python's argparse](https://realpython.com/command-line-interfaces-python-argparse/)  
[argparse](https://docs.python.org/3/library/argparse.html#module-argparse)  

## python 2 c converter
[Online Python to C Converter](https://www.codeconvert.ai/python-to-c-converter)  


[The Python return Statement: Usage and Best Practices](https://realpython.com/python-return-statement/)  

## 类型提示
[typing — Support for type hints](https://docs.python.org/3/library/typing.html)  
[How to Use Type Hints for Multiple Return Types in Python](https://realpython.com/python-type-hints-multiple-types/)  

## Python for Network Automation
[Python for Network Automation](https://python-automation-book.readthedocs.io/en/stable/index.html)  

## modules
[Python Module Index](https://docs.python.org/3/py-modindex.html)  
[NCS Python API](https://developer.cisco.com/docs/nso/api/ncs/#package-ncs)  

```python
def greeting(name: str) -> str:
	return 'Hello ' + name

# Type aliases are useful for simplifying complex type signatures. For example:
from collections.abc import Sequence

type ConnectionOptions = dict[str, str]
type Address = tuple[str, int]
type Server = tuple[Address, ConnectionOptions]

def broadcast_message(message: str, servers: Sequence[Server]) -> None:
	...

# The static type checker will treat the previous type signature as
# being exactly equivalent to this one.
def broadcast_message(
		message: str,
		servers: Sequence[tuple[tuple[str, int], dict[str, str]]]) -> None:
```
- [Python Tips](https://book.pythontips.com/en/latest/index.html#) #online
- [Context Managers and Python's with Statement](https://realpython.com/python-with-statement/)
- [Python: Context Manager to Simplify Resource Handling](https://pravash-techie.medium.com/python-context-manager-to-simplify-resource-handling-5959a36a0f58)
- [contextlib   — Utilities for   with -statement contexts](https://docs.python.org/3/library/contextlib.html)
- 常用环境变量
	- [PYTHONPATH](https://docs.python.org/3/using/cmdline.html#envvar-PYTHONPATH)
	- [Using PYTHONPATH](https://bic-berkeley.github.io/psych-214-fall-2016/using_pythonpath.html)
	- [Python import, sys.path, and PYTHONPATH Tutorial](https://www.devdungeon.com/content/python-import-syspath-and-pythonpath-tutorial)
- 常用函数
	- ```python
	  # 执行shell命令
	  # run subprocess and returns out, err, errcode
	  import os
	  import subprocess
	  
	  def run_shell2(dir, ctx, args):
	  	cwd = os.getcwd()
	  	os.chdir(dir)
	  
	  	process = subprocess.Popen(args, shell=True,
	  			stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	  	out, err = process.communicate()
	  	errcode = process.returncode
	  	print('OK: [%s] %s' % (os.getcwd(), args))
	  
	  	os.chdir(cwd)
	  
	  	return out, err, errcode
	  
	  # Simple Way of Python's subprocess.Popen with a Timeout Option
	  from time import sleep
	  from subprocess import Popen, PIPE
	  
	  def popen_timeout(command, timeout):
	      p = Popen(command, stdout=PIPE, stderr=PIPE)
	      for t in xrange(timeout):
	          sleep(1)
	          if p.poll() is not None:
	              return p.communicate()
	      p.kill()
	      return "timeout"
	  
	  >>> print(popen_tmeout(["ping","192.168.109.165"],10))
	  timeout
	  >>> print(popen_tmeout(["ls","/tmp/x"],10))
	  (b'5516\nb\nhardlink\nt1.py\nt1.sh\nt2.sh\nt3.sh\ntxt1\ntxt2\nweir.run\n', b'')
	  ```
- 安装python环境
	- 安装时注意勾选**“添加python到path环境变量”**的选项，方便之后在任何地方使用python
- 数据类型
	- 数字
		- ```python
		  123
		  12.3
		  # 数字可进行算术运算
		  ```
	- 字符串
		- ```python
		  "hello world"
		  'hello world'
		  "测试语句"
		  
		  # 字符串连接
		  "hello " + "world"
		  "hello " "world"
		  
		  # 多行的字符串
		  "hello\nworld"
		  """hello
		  world
		  """
		  # 字符串重复
		  "hello" * 3
		  
		  # 获取字符串内的字元
		  s = "hello"
		  s[0]
		  s[1:4]
		  s[1]
		  s[:4]
		  
		  ```
	- 布尔值 Boolean
		- ```python
		  True
		  False
		  ```
	- 可变列表 List
		- 有序，可变动
		- ```python
		  ["hello", "world"]
		  list = [1, 2, 3, 4, 5]
		  
		  # 列表的元素
		  list[0]
		  list[1:4]
		  
		  # 更新列表元素
		  list[0] = 4
		  
		  # 清空列表的部分元素
		  list[1:4] = []
		  
		  # 列表的元素添加
		  list + [6, 7]
		  
		  # 列表长度
		  len(list)
		  
		  # 列表的嵌套
		  list = [[1, 2, 3], [4, 5, 6]]
		  list[0][0]
		  list[0]
		  list[0][0:2]
		  list[0][0:2] = [5, 6, 7, 8]
		  ```
	- 固定列表 Tuple
		- 有序，不可变动
		- ```python
		  (1, 2, 3)
		  ("hello", "world")
		  
		  tuple = (1, 2, 3, 4)
		  tuple[0]
		  tuple[0:2]
		  ```
	- 集合 Set
		- 无序，可变动
		- ```python
		  {1, 2, 3}
		  {"hello", "world"}
		  
		  set = {1, 2, 3, 4}
		  
		  # 判断元素是否在集合中
		  if 1 in set:
		    print("1 is in set")
		  
		  if 10 not in set:
		    print("10 is not in set")
		    
		  # 集合的运算
		  set1 = {1, 2, 3}
		  set2 = {4, 5, 6, 7}
		  # 交集
		  s1 & s2
		  # 或集
		  s1 | s2
		  # 差集
		  s1 - s2
		  s2 - s1
		  # 反交集
		  s1 ^ s2
		  
		  # 集合的建立(字符串的拆解)
		  s3 = set("hello")
		  ```
	- 字典 Dictionary
		- 键值对的集合
		- ```python
		  {"apple": "苹果"， "data": 数据}
		  {1: "hello", 2: "world"}
		  
		  dict = {"apple": "苹果"， "data": 数据}
		  
		  # 字典的元素
		  dict["apple"]
		  
		  # 修改字典元素
		  dict["apple"] = "小苹果"
		  
		  # 判断元素(注意是key)是否在字典中
		  if "apple" in dict:
		    print("apple is in dict")
		  
		  if "pear" not in dict:
		    print("pear is not in dict")
		  
		  # 删除键值对
		  del dict["apple"]
		  
		  # 根据列表建立键值对
		  dict1 = { x: x*2 for x in [1, 2, 3] }
		  ```
	- 变量
		- 存储不同类型的数据，进行后续的处理
		- ```python
		  x = 3
		  x = "hello world"
		  ```
- 流程控制
	- if
		- ```python
		  # 基本结构
		  if condition1:
		    action1
		  elif condition2:
		    action2
		  else:
		    default action
		  
		  # 示例
		  score = int(input("please input a number: "))
		  if score < 60:
		    print("score is less than 60")
		  elif score < 90:
		    print("score is between 60 and 90")
		  elif score <= 100:
		    print("score is between 90 and 100")
		  else:
		    print("invalid score")
		  ```
	- while
		- ```python
		  # 基本结构
		  while condition:
		    action1
		    action2
		    ...
		    condition change
		  
		  # 示例
		  n = 1
		  sum = 0
		  while n <= 10:
		    sum += n
		    n++
		  print sum
		  
		  
		  ```
	- for
		- ```python
		  # 基本结构
		  for var in range:
		    action1
		    action2
		    ...
		    
		  # 示例 1
		  num = 1
		  sum = 0
		  for in in range(1, 11):
		    sum += i
		  print(sum)
		  
		  # 示例 2
		  num = int(input("please input a positive number: "))
		  for i in range(num):
		    if i * i == num:
		      print("整数平方根 ", i)
		      break	# 强制结束循环，不执行 else 语句
		  else:
		    print("没有整数平方根")
		  
		  ```
		- break
			- 跳出当前循环
		- continue
			- 跳过当次循环，继续下一轮循环
- 函数
	- ```python
	  # 基本结构
	  def function_name(param1, param2, ...):
	    statments
	  
	  # 示例 1
	  def sum(max):
	    sum = 0
	    for i in range(max + 1):
	      sum += i
	    return sum
	  
	  print(sum(10))
	  print(sum(100))
	  ```
		- 函数的参数
			- 默认参数
				- ```python
				  def power(base, exp = 0):
				    return base ** exp
				  
				  power(10, 2)
				  power(5)
				  ```
			- 指定参数名称
				- ```python
				  def divide(n1, n2):
				    return n1 / n2
				  
				  power(2, 4)
				  power(n2=2, n1=4)
				  ```
			- 不定参数
				- 不定参数的类型为 Tuple
				- ```python
				  def average(*nums):
				    sum = 0
				    for n in nums:
				      sum += n
				    return sum / len(nums)
				  
				  print(average(1, 2))
				  print(average(1, 2, 3))
				  ```
- 模块 module
	- 将函数实现写入一个独立的 python 文件，该文件称为模块，模块可以被载入多次重复使用
	- 模块的载入与使用
		- 直接载入模块
			- ```python
			  import sys
			  
			  print(sys.platform)
			  print(sys.path)
			  ```
		- 模组别名
			- ```python
			  import sys as s
			  
			  print(s.platform)
			  print(s.path)
			  ```
	- 内建模块
		- random
			- ```python
			  import random
			  
			  # 获取 0.0 ~ 1.0 随机数据
			  random.random()
			  
			  # 获取指定范围的随机数据
			  random.uniform(0.0, 1.0)
			  
			  # 正态分布随机数据
			  # 获取平均数 100, 标准差 10 的正态分布随机数
			  random.normalvariate(100, 10)
			  
			  # 从列表中随机选取 1 个元素
			  random.choice([1, 2, 3, 4])
			  
			  # 从列表中随机选取多个元素
			  random.sample([1, 2, 3, 4], 2)
			  
			  # 将列表中的元素 就地 随机调换顺序
			  data = [1, 2, 3, 4]
			  random.shuffle(data)
			  ```
			- 正态分布
				- ![image.png](../assets/image_1668400164635_0.png)
		- statistics
			- ```python
			  import statistics
			  
			  # 平均数
			  statistics.mean([1, 2, 3, 4])
			  
			  # 中位数
			  statistics.median([1, 2, 3, 4])
			  
			  # 标准差
			  statistics.stdev([1, 2, 3, 4])
			  ```
	- 自定义模块
		- 新建模块目录例如`modules`，在目录下新建多个python文件(例如 geometry.py, math.py)分别实现对应的模块功能
		- 使用时需要将模块目录路径加入到系统的path环境变量
			- ```python
			  import sys
			  sys.path.append("modules")
			  
			  import geometry
			  distance = geometry.distance(1, 1, 5, 5)
			  ```
	- [[pexpect]]
		- [man pexpect](https://pexpect.sourceforge.net/pexpect.html)
		- [man pxssh](https://pexpect.sourceforge.net/pxssh.html)
		- pxssh实现ssh自动登陆
			- 示例1
				- ```python
				  #!/usr/bin/python
				  
				  from pexpect import pxssh
				  
				  def check_ls(ssh):
				  	cmd = "ls"
				  	ssh.sendline(cmd)
				  	ssh.prompt()
				  	val = ssh.before
				  	if 'test' in val.decode("utf8"):
				  		print("test is in %s" % val)
				  		return True
				  	print("test is not in %s" % val)
				  	return False
				  
				  def login(hostip, username, password):
				  	try:
				  		ssh = pxssh.pxssh()
				  		ssh.login(hostip, username, password, port=5617)
				  		print("login successfully")
				  		res = check_ls(ssh)
				  		if res:
				  			print("True")
				  		else:
				  			print("False")
				  	except pxssh.ExceptionPxssh as e:
				  		print("pxssh failed on login.")
				  	except pexpect.exceptions.EOF as e:
				  		print("Connection reset.")
				  	except pexpect.exceptions.TIMEOUT as e:
				  		print("Connection Timeout.")
				  
				  if __name__ == "__main__":
				  	login("135.251.92.219", "vcapuser", "eacadmin")
				  ```
		- 示例
			- ```python
			  #!/usr/bin/python
			  
			  import pexpect
			  
			  class SSHClient:
			          def __init__(self, user, host, pwd):
			                  self.user = user
			                  self.host = host
			                  self.pwd = pwd
			                  self.ssh = None
			          def send_command(self, cmd):
			                  self.ssh.sendline(cmd)
			                  #self.ssh.buffer="" // otherwise it will cause "TypeError: can't concat str to bytes"
			                  self.ssh.expect('support@', timeout=10)
			                  return self.ssh.after + self.ssh.before
			          def close(self):
			                  self.ssh.close()
			          def connect(self):
			                  ssh_newkey = "Are you sure you want to continue connecting"
			                  constr = "ssh " + "-p5617 " + self.user + "@" + self.host
			                  self.ssh = pexpect.spawn(constr, encoding='utf-8')
			                  ret = self.ssh.expect([pexpect.TIMEOUT, ssh_newkey, '[P|p]assword:'])
			                  if ret == 0:
			                          return False, '[%s@%s] Error Connecting' % (self.user, self.host)
			                  if ret == 1:
			                          self.ssh.sendline("yes")
			                          ret = self.ssh.expect([pexpect.TIMEOUT, ssh_newkey, '[P|p]assword:'])
			                          if ret == 0:
			                                  return False, '[%s@%s] Error Connecting' % (self.user, self.host)
			                  self.ssh.sendline(self.pwd)
			                  self.ssh.expect('gs_cli')
			                  self.ssh.sendline('diag-shell\r')
			                  self.ssh.expect('support')
			                  return True, '[%s@%s] Success Connecting' % (self.user, self.host)
			  
			  if __name__ == "__main__":
			          ssh = SSHClient(user="support", host="169.254.32.200", pwd="Gainsp33")
			          ret, msg = ssh.connect()
			          if not ret:
			                  print(msg)
			                  exit(0)
			  
			          cmd = 'ls /tmp'
			          result = ssh.send_command(cmd)
			          print(result)
			  
			          cmd = 'df -h'
			          result = ssh.send_command(cmd)
			          print(result)
			          ssh.close()
			  
			  ```
	- [[str]]
	- [[list]]
	- [[python file]]
	- [[dictionary]]
	- os
		- [Miscellaneous operating system interfaces](https://docs.python.org/3/library/os.html)
		- `os.path`
			- [Common pathname manipulations](https://docs.python.org/3/library/os.path.html#module-os.path)
	- [[re]]
		- [Regular expression operations](https://docs.python.org/3/library/re.html?highlight=re#module-re)
	- glob
		- [Unix style pathname pattern expansion](https://docs.python.org/3/library/glob.html?highlight=glob#module-glob)
	- subprocess
		- [Subprocess management](https://docs.python.org/3/library/subprocess.html)
	- sys
		- [System-specific parameters and functions](https://docs.python.org/3/library/sys.html)
	- datetime
		- [Basic date and time types](https://docs.python.org/3/library/datetime.html)
	- time
		- [Time access and conversions](https://docs.python.org/3/library/time.html)
	- logging
		- [Logging facility for Python](https://docs.python.org/3/library/logging.html?highlight=logging#module-logging)
	- logging.handlers
		- [logging.handlers](https://docs.python.org/3/library/logging.handlers.html?highlight=logging%20handlers#module-logging.handlers)
	- platform
		- [Access to underlying platform ’s identifying data](https://docs.python.org/3/library/platform.html?highlight=platform#module-platform)
	- telnetlib
		- [Telnet client](https://docs.python.org/3/library/telnetlib.html?highlight=telnetlib#module-telnetlib)
	- getpass
		- [Portable password input](https://docs.python.org/3/library/getpass.html)
	- shutil
		- [High-level file operations](https://docs.python.org/3.8/library/shutil.html)
- package
	- 用来整理和分类模块
	- 文件夹结构
		- ```
		  - 项目文件夹
		  	- 主程序.py
		      - 封包文件夹
		      	- __init__.py
		          - 模块1.py
		          - 模块2.py
		  
		  # 例如
		  - python_learn
		  	- main.py
		      - geometry
		      	- __init__.py
		          - point.py
		          - line.py
		  
		  import geometry.point
		  distance = geometry.point.distance(3, 4)
		  
		  import geomegry.point as point
		  distance = point.distance(3, 4)
		  ```
	- 使用封包
		- `import 封包名称.模组名称`
		- `import 封包名称.模组名称 as 模组别名`
- 文件读写
	- 普通文本文件的读写
		- ```python
		  # 文件写
		  with open("data.txt", mode="w", encoding="utf-8") as file:
		    file.write("1\n2\n3")
		    
		  # 文件读
		  
		  # 一次读取文件全部内容
		  with open("data.txt", mode="r", encoding="utf-8") as file:
		    data = file.read()
		  print(data)
		  
		  # 逐行读取
		  sum = 0
		  with open("data.txt", mode="r", encoding="utf-8") as file:
		    for line in file:
		      sum += int(line)
		  print(sum)
		  ```
	- json 文件的读写
		- ```python
		  import json
		  
		  # 读取
		  with open("config.json", mode="r", encoding="utf-8") as file:
		    data = json.load(file)
		  print(data)
		  print("name: ", data["name"])
		  print("version: ", data["version"])
		  
		  # 修改并写入
		  data["name"] = "New name"
		  with open("config.json", mode="w", encoding="utf-8") as file:
		    json.dump(data, file)
		  ```
- 网络连接
	- ```python
	  import urllib.request as request
	  
	  url = "http://news.baidu.com/"
	  with request.urlopen(url) as reponse:
	    data = reponse.read().decode("utf-8")
	  print(data)
	  
	  # jason 格式网页数据的获取
	  url = "http://xxx"
	  with request.urlopen(url) as reponse:
	    data = json.load(response):
	  
	  clist = data["result"]["results"]
	  with open("company.txt", "w", encoding="utf-8") as file:
	    for company in clist:
	      file.write(company["公司名称"] + "\n")
	  ```
- 类 class
	- [Classes](https://docs.python.org/3/tutorial/classes.html)
		- ```python
		  # 示例 1
		  class Point:
		  	def __init__(self, x, y):
		  		self.x = x
		  		self.y = y
		  
		  	def distance(self, point):
		  		return ((self.x - point.x) ** 2 + (self.y - point.y) ** 2) ** 0.5
		  
		  	def show(self):
		  		print(self.x, self.y)
		  
		  p1 = Point(0, 0)
		  p2 = Point(3, 4)
		  print(p1.distance(p2))
		  
		  # 示例 2
		  import io
		  
		  class File1:
		  	def __init__(self, name):
		  		self.name = name
		  		self.file = io.open(self.name, mode="r", encoding="utf-8")
		  	
		  	def read(self):
		  		return self.file.read()
		  	
		  	def close(self):
		  		self.file.close()
		  
		  f1 = File1("data.txt")
		  data = f1.read()
		  print(data)
		  f1.close()
		  ```
- 网络爬虫
	- 关键：尽可能地让程序模仿一个普通使用者的样子
		- 网页页面右键`View page source`可以查看HTML文件源码。F12可以进入开发者工具
		- ![image.png](../assets/image_1668405216789_0.png){:height 308, :width 746}
			- ```python
			  import urllib.request as req
			  
			  # 抓取网页HTML源码
			  url = "https://www.baidu.com/"
			  # 建立一个Request对象并附件上Request Headers信息
			  request = req.Request(url, headers={
			    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/107.0.0.0 Safari/537.36"
			  })
			  
			  with req.urlopen(request) as response:
			    data = response.read().decode("utf-8")
			  
			  # 解析原始码，取得关心的标题
			  import bs4
			  root = bs4.BeautifulSoup(data, "html.parser")
			  print(root.title.string)
			  
			  titles = root.find_all("span", class_="title-content-title")
			  for title in titles:
			    print(title.string)
			  ```
			- 注意：需要在终端 Terminal 安装第3方套件，`pip install beautifulsoup4`
- python 技巧
	- F string
		- ```python
		  name = "Tim"
		  str = f'Hello {name} {2 +3} {[1, 2, 3].}'
		  print(str)
		  ```
	- unpacking
		- ```python
		  tup = (1, 2, 3)
		  lst = [1, 2, 3]
		  dic = {"a": 1, "b":2}
		  str = "hello"
		  a, b, c = tup
		  a, b = dic # keys
		  a, b = dic.values()
		  a, b = dic.items()
		  
		  print(a, b, c)
		  ```
	- 多变量赋值
		- ```python
		  width, height = 400, 500
		  width, height = height, width
		  ```
	- 列表推导
		- ```python
		  x = [i for i in range(100) if i % 2 == 0]
		  x = [i * j for i in range(10) for j in range(10)]
		  x = [[_ for _ in range(5)] for _ in range(5)]
		  
		  chars = (c for c in "hello")
		  print(tuple(chars))
		  
		  sentence = "hello my name is tim"
		  x = {char: sentence.count(char) for char in set(sentence)}
		  print(x)
		  ```
	- 对象重复
		- ```python
		  x = "hello" * 3
		  x = [1, 2, 3] * 3
		  x = [[1, 2, 3]] * 3
		  x = (1, 2) * 3
		  ```
	- 内置条件
		- ```python
		  x = 1 if 2 > 3 else 0
		  ```
	- zip
		- ```python
		  names = ['tim', 'joe', 'billy', 'sally']
		  ages = [21, 19, 18, 43]
		  eye_colors = ['blue', 'brown', 'brown', 'green', 'test']
		  
		  for name, age, eye_color in zip(names, ages, eye_colors):
		    if age > 20:
		      print(name)
		      print(eye_color)
		  ```
	- \*args and **kwargs
		- ```python
		  def func1(arg1, arg2, arg3):
		    print(arg1, arg2, arg3)
		  
		  def func2(arg1=None, arg2=None, arg3=None):
		    print(arg1, arg2, arg3)
		  
		    
		  args = [1, 2, 3]
		  kwargs = {"arg2": 2, "arg1": 1, "arg3": 3}	# key word args
		  
		  func1(*args)
		  
		  func2(*kwargs)
		  等价于
		  func2(arg2=2, arg1=1, arg3=3)
		  ```
	- For 和 While 的 else 语句
		- ```python
		  search = [1, 2, 3]
		  target = 4
		  
		  for elem in search:
		    if elem == target:
		      print("I found it")
		      break
		  else:
		    print("I did not find it")
		  ```
	- 关键字排序
		- ```python
		  lst = [[1, 2], [3, 4], [4, 2], [-1, 3], [4, 5], [2, 3]]
		  lst.sort(key=lambda x: x[1] + x[0])
		  ```
- 内置函数
	- map
		- map(fun, iterable)
			- ```python
			  numbers = (1, 2, 3)
			  result = map(lambda x: x + 1, numbers)
			  print(list(result))
			  ```
			- ```python
			  words = ('paris', 'xiaobai', 'love')
			  test = list(map(list, words))
			  print(test)
			  
			  Out: [['p', 'a', 'r', 'i', 's'], ['x', 'i', 'a', 'o', 'b', 'a', 'i'], ['l', 'o', 'v', 'e']]
			  ```
	- filter
		- filter(fun, iterable)
			- ```python
			  # 打印 10 以内的奇数
			  print(list(filter(lambda x: x % 2,range(10))))
			  ```
	- reduce
		- reduce(function, iterable)
		- 工作流程
			- 1. 获取可迭代对象iterable的第1个和第2个元素，执行函数function产生并保存结果
			  2. 对第一步返回的结果与可迭代对象的第3个元素应用函数function产生并保存结果
			  3. 重复步骤2知道可迭代对象中元素耗尽为止
		- ```python
		  from functools import reduce
		  print(reduce(lambda x, y: x * y, range(1, 5)))
		  ```
	- zip
		- zip(*iterators)
			- ```python
			  # zip
			  names = [ "xiaobai", "john", "mike", "alpha" ]
			  ages = [ 4, 1, 3, 2 ]
			  marks = [ 40, 50, 60, 70 ]
			  
			  mapped = list(zip(names, ages, marks))
			  
			  # unzip
			  names, ages, marks = zip(*mapped)
			  ```
- using an assignment expression
	- ```python
	  import hashlib
	  
	  def get_sha256_hash(filename, buffer_size=2**10*8):
	      file_hash = hashlib.sha256()
	      with open(filename, mode="rb") as f:
	          while chunk := f.read(buffer_size):
	              file_hash.update(chunk)
	      return file_hash.hexdigest()
	  ```
- contextlib
	```python
	# context manager: 在一个类里，实现了__enter__和__exit__的方法，这个类的实例就是一个上下文管理器
	class Resource():
		def __enter__(self):
			print('===connect to resource===')
			return self
		def __exit__(self, exc_type, exc_val, exc_tb):
			print('===close resource connection===')

		def operate(self):
			print('===in operation===')

	with Resource() as res:
		res.operate()

	# 使用 contextlib 简化上下文管理器的实现
	import contextlib

	@contextlib.contextmanager
	def operate():
		print("__enter__")
		yield "yielded value"
		print("__exit__")

	with operate() as o:
		print(o)

	# 为什么需要context manager
		# 1.可以以一种更加优雅的方式，操作（创建/获取/释放）资源，如文件操作、数据库连接；
		# 2.可以以一种更加优雅的方式，处理异常
	class Resource():
		def __enter__(self):
			print('===connect to resource===')
			return self

		def __exit__(self, exc_type, exc_val, exc_tb):
			print('===close resource connection===')
			return True	# 在__exit__ 里返回 True 相当于告诉 Python解释器，这个异常我们已经捕获了，不需要再往外抛了

		def operate(self):
			1/0

	with Resource() as res:
		res.operate()


	写__exit__ 函数时，需要注意的事，它必须要有这三个参数：
	exc_type：异常类型
	exc_val：异常值
	exc_tb：异常的错误栈信息
	当主逻辑代码没有报异常时，这三个参数将都为None

	# contextlib是一个装饰器，你只要按照它的代码协议来实现函数内容，就可以将这个函数对象变成一个上下文管理器
	import contextlib

	@contextlib.contextmanager
	def open_func(file_name):
		# __enter__方法
		print('open file:', file_name, 'in __enter__')
		file_handler = open(file_name, 'r')

		# 【重点】：yield
		yield file_handler

		# __exit__方法
		print('close file:', file_name, 'in __exit__')
		file_handler.close()
		return

	with open_func('/Users/MING/mytest.txt') as file_in:
		for line in file_in:
			print(line)
	# 被装饰函数里，必须是一个生成器（带有yield）
		#1 yield之前的代码，就相当于__enter__里的内容
		#2 yield 之后的代码，就相当于__exit__ 里的内容

	# 处理异常
	import contextlib

	@contextlib.contextmanager
	def open_func(file_name):
		# __enter__方法
		print('open file:', file_name, 'in __enter__')
		file_handler = open(file_name, 'r')

		try:
			yield file_handler
		except Exception as exc:
			# deal with exception
			print('the exception was thrown')
		finally:
			print('close file:', file_name, 'in __exit__')
			file_handler.close()

			return

	with open_func('/Users/MING/mytest.txt') as file_in:
		for line in file_in:
			1/0
			print(line)

	// 示例
	@contextlib.contextmanager
	def tempdir(**kwargs):
		argdict = kwargs.copy()
		if 'dir' not in argdict:
			argdict['dir'] = CONF.tempdir
		tmpdir = tempfile.mkdtemp(**argdict)
		try:
			yield tmpdir
		finally:
			try:
				shutil.rmtree(tmpdir)
			except OSError as e:
				LOG.error(LE('Cound not remove tmpdir: %s'), e)


	使用上下文管理器有三个好处：
	1.提高代码的复用率；
	2.提高代码的优雅度；
	3.提高代码的可读性；
	```
	- [using an assignment expression](https://www.pythonmorsels.com/reading-binary-files-in-python/)
- 参考文档
	- [www.python.org](https://www.python.org/)
	- [Python 标准库](https://docs.python.org/zh-cn/3/library/)
	- [**The Python Standard Library**](https://docs.python.org/3/library/)
	- [**Python黑魔法手册**](https://magic.iswbm.com/index.html)
	- [**Python 3 Trinkets(python3 online)**](https://trinket.io/features/python3)
	- [online python](https://www.online-python.com/)
	- [Online Compiler](https://www.programiz.com/python-programming/online-compiler/)
	- [The Big Book of Small Python Projects](https://inventwithpython.com/bigbookpython/) #online
	- [Python args and kwargs: Demystified](https://realpython.com/python-kwargs-and-args/)
	- [How to Use the Unpacking Operators (*, **) in Python?](https://geekflare.com/python-unpacking-operators/)
	- [Python Code Examples](https://www.programcreek.com/python/)
	- [**20-python-libraries-you-arent-using-but-should.pdf**](https://github.com/ffisk/books/blob/master/20-python-libraries-you-arent-using-but-should.pdf)
	- [Python 3 Cheat Sheet](https://perso.limsi.fr/pointal/_media/python:cours:mementopython3-english.pdf)
	- [pythoncheatsheet](https://www.pythoncheatsheet.org/) #online
	- [The Hitchhiker’s Guide to Python!](https://docs.python-guide.org/)
	- [Python 101](https://www.python101.pythonlibrary.org/index.html#) #online
	- [python101code](https://github.com/driscollis/python101code) #github
	- [python201bookcode](https://github.com/driscollis/python201bookcode) #github
	- [wxpythoncookbookcode](https://github.com/driscollis/wxpythoncookbookcode) #github
	- [Automate the Boring Stuff with Python](https://automatetheboringstuff.com/)
	- [阿尔法的Python笔记](https://segmentfault.com/blog/alpha94511)
	- [realpython](https://realpython.com/tutorials/all/)
	- [python3 cookbook](https://python3-cookbook.readthedocs.io/zh_CN/latest/index.html)
	- [Python Documentation contents](https://docs.python.org/3/contents.html)
	- [Python for network engineers](https://pyneng.readthedocs.io/en/latest/index.html)
	- [Python Tricks: The Book](http://www.kalfaoglu.com/ceng113/Python-Programming/pythontricks.pdf)
	- [Programming in Python 3](https://cs.smu.ca/~porter/csc/227/ProgrammingInPython3.pdf)
	- [Object-Oriented Programming (OOP) in Python 3](https://realpython.com/python3-object-oriented-programming/)
	- [Elements of Programming Interviews in Python](https://elementsofprogramminginterviews.com/sample/epilight_python_new.pdf)
	- [Elements of Programming Interviews in Python](https://github.com/qqqil/ebooks/blob/master/algorithms/Elements%20of%20Programming%20Interviews.pdf)
	- [PYTHON CRASH COURSE](https://bedford-computing.co.uk/learning/wp-content/uploads/2015/10/No.Starch.Python.Oct_.2015.ISBN_.1593276036.pdf)
	- [The Big Book of Small Python Projects](https://inventwithpython.com/bigbookpython/) #online
	- [Primer on Python Decorators](https://realpython.com/primer-on-python-decorators/)
	- [Logging in Python](https://realpython.com/python-logging/)
	- [Logging HOWTO](https://docs.python.org/3/howto/logging.html)
	- [The subprocess Module: Wrapping Programs With Python](https://realpython.com/python-subprocess/)
	- [Awesome_Python_Scripts](https://prathimacode-hub.github.io/Awesome_Python_Scripts/) #github
	- [Simple Way of Python's subprocess.Popen with a Timeout Option](https://www.gungorbudak.com/blog/2015/08/30/simple-way-of-pythons-subprocesspopen/)
	- [Python 101: How to timeout a subprocess](https://www.blog.pythonlibrary.org/2016/05/17/python-101-how-to-timeout-a-subprocess/)
	- [Python 101: How to Timeout a Subprocess](https://dzone.com/articles/python-101-how-to-timeout-a-subprocess)
	- [Python eval(): Evaluate Expressions Dynamically](https://realpython.com/python-eval-function/)
	- [Your Guide to the Python print() Function](https://realpython.com/python-print/)
	- [Python's map(): Processing Iterables Without a Loop](https://realpython.com/python-map-function/)
	- [Working With JSON Data in Python](https://realpython.com/python-json/)
	- [How to Use the JSON Module in Python](https://www.freecodecamp.org/news/how-to-use-the-json-module-in-python/)
- [Python mmap: Improved File I/O With Memory Mapping](https://realpython.com/python-mmap/)
- [Understanding Python re(gex)?](https://learnbyexample.github.io/py_regular_expressions/cover.html)
- [python projects](https://bedford-computing.co.uk/learning/wp-content/uploads/2015/10/Python-Projects.pdf)
- [The big book of small python projects](https://edu.anarcho-copy.org/Programming%20Languages/Python/BigBookSmallPythonProjects.pdf)
- [learning Python](https://cfm.ehu.es/ricardo/docs/python/Learning_Python.pdf)
- [practice_python_projects](https://github.com/learnbyexample/practice_python_projects)
- [How to use decorators Part 2](https://pythonforthelab.com/blog/how-to-use-decorators-part-2/)
- [How to Use Decorators to Validate Input](https://pythonforthelab.com/blog/how-to-use-decorators-to-validate-input/)
- [A Primer on Classes in Python](https://pythonforthelab.com/blog/a-primer-on-classes-in-python/)
- [Implementing Threads for Measurements](https://pythonforthelab.com/blog/implementing-threads-for-measurements/)
- [Learning (not) to Handle Exceptions](https://pythonforthelab.com/blog/learning-not-to-handle-exceptions/)
- [Storing Binary Data and Serializing](https://pythonforthelab.com/blog/storing-binary-data-and-serializing/)
- [What are Hashable Objects](https://pythonforthelab.com/blog/what-are-hashable-objects/)
- [The with command and custom classes](https://pythonforthelab.com/blog/the-with-command-and-custom-classes/)
- [What are args and kwargs and when to use them](https://pythonforthelab.com/blog/what-are-args-and-kwargs-and-when-to-use-them/)
- [Introduction to Python Lambda Functions](https://pythonforthelab.com/blog/intro-to-python-lambda-functions/)
- [Python and PyQt: Building a GUI Desktop Calculator](https://realpython.com/python-pyqt-gui-calculator/)
- [PEP](https://peps.python.org/)
- [How to Use Generators and yield in Python](https://realpython.com/introduction-to-python-generators/)
	```python
	在python中，generator functions是一类特殊的函数，它返回一个 lazy iterator(delays the evaluation of an expression until its value is needed),lazy iterators不在内存中保存它们的内容
	应用场景：
	1.大文件读取
	2.函数需要维护内部的状态，但是又不值得用一个单独的类来实现
	
	Generator 函数和普通函数的差异是 使用了 yield 关键字 而不是 return
	yield 关键字表明 yield 后的 value 被发送给了调用者，但是之后函数并不退出，即函数的状态被记下来了


	yield 会返回一个 generator object，generator 是一个特殊的 iterator，当对 generator 调用特殊的函数时，例如 next()，代码会执行到函数中的 yield 语句, yield 语句会挂起当前执行的函数并返回 yielded value 给调用者，当函数被挂起时，函数的状态被保存了下来，函数的状态包括 any variable bindings local to the generator, the instruction pointer, the internal stack, and any exception handling，这使得再次调用 generator 的方法时，函数可以继续执行

	generator expression (also called a generator comprehension) 用于快速产生 generator object
	nums_squared_gc = (num**2 for num in range(5))

	def infinite_sequence():
		num = 0
		while True:
			yield num
			num += 1

	next() 方法可以用来获取 generator 的下一个结果

	gen = infinite_sequence()

	next(gen)	// 0
	next(gen)	// 1


	import sys
	nums_squared_lc = [i ** 2 for i in range(10000)]
	print(sys.getsizeof(nums_squared_lc))	// 87624

	nums_squared_gc = (i ** 2 for i in range(10000))
	print(sys.getsizeof(nums_squared_gc))	// 120

	需要注意的是
	在有足够内存的情况下 list comprehension 比 generator expression 要快
	>>> import cProfile
	>>> cProfile.run('sum([i * 2 for i in range(10000)])')
			5 function calls in 0.001 seconds
	>>> cProfile.run('sum((i * 2 for i in range(10000)))')
			10005 function calls in 0.003 seconds


	letters = ["a", "b", "c", "y"]
	it = iter(letters)
	while True:
		try:
			letter = next(it)
		except StopIteration:
			break
		print(letter)

	Generator的方法
	.next()
	.send()
	.throw()
	.close()
	```
- [Python's filter(): Extract Values From Iterables](https://realpython.com/python-filter-function/)
- [Iterators and Iterables in Python: Run Efficient Iterations](https://realpython.com/python-iterators-iterables/)
- [argparse](https://docs.python.org/3/library/argparse.html)
- [Build Command-Line Interfaces With Python's argparse](https://realpython.com/command-line-interfaces-python-argparse/)
	> Parser for command-line options, arguments and sub-commands
	```python
	# 1 最简单的 argparse 的例子
	$ cat t1.py
	import argparse

	parser = argparse.ArgumentParser()

	$ python t1.py
	$ python t1.py --help
	usage: t1.py [-h]

	optional arguments:
	-h, --help  show this help message and exit
	$ python t1.py --verbose
	usage: t1.py [-h]
	t1.py: error: unrecognized arguments: --verbose
	$ python t1.py xxx
	usage: t1.py [-h]
	t1.py: error: unrecognized arguments: xxx
	# 没有任何选项的情况下运行脚本不会在标准输出显示任何内容
	# --help 选项，也可缩写为 -h，是唯一一个可以直接使用的选项（即不需要指定该选项的内容）。指定任何内容都会导致错误

	# 2 位置参数
	cat t1.py
	import argparse

	parser = argparse.ArgumentParser()
	parser.add_argument("echo", help="echo the string you use here")
	args = parser.parse_args()
	print(args.echo)

	$ python3 t1.py
	usage: t1.py [-h] echo
	t1.py: error: the following arguments are required: echo
	$ python3 t1.py echo
	echo
	$ python3 t1.py echo1
	echo1

	# 3 argparse 会把我们传递给它的选项视作为字符串，除非我们告诉它别这样
	cat t1.py
	import argparse

	parser = argparse.ArgumentParser()
	parser.add_argument("square", help="display a square of a given number", type=int)
	args = parser.parse_args()
	print(args.square ** 2)

	$ python3 t1.py 4
	16

	# 可选参数
	cat t1.py
	import argparse

	parser = argparse.ArgumentParser()
	parser.add_argument("--verbose", help="increase output verbosity",
						action="store_true")	# action 赋值为 "store_true"，意味着，如果指定了该选项，则将值 True 赋给 args.verbose
	args = parser.parse_args()
	if args.verbose:
		print("verbosity turned on")

	$ python3 t1.py
	$ python3 t1.py  --verbose
	verbosity turned on

	# 短选项
	parser.add_argument("-v", "--verbose", help="increase output verbosity",
						action="store_true")	# 添加 "-v"，用来支持短选项
	
	# 结合位置参数和可选参数
	cat t1.py
	import argparse

	parser = argparse.ArgumentParser()
	parser.add_argument("square", type=int, help="display a square of a given number")
	parser.add_argument("-v", "--verbose", help="increase output verbosity", action="store_true")
	args = parser.parse_args()
	answer = args.square ** 2
	if args.verbose:
		print(f"the square of {args.square} equals {answer}")
	else:
		print(answer)
	
	$ python3 t1.py  4
	16
	$ python3 t1.py  4 --verbose
	the square of 4 equals 16
	$ python3 t1.py  --verbose 4
	the square of 4 equals 16

	# 改进 1, 给程序加上接受多个冗长度的值
	parser.add_argument("-v", "--verbose", type=int, help="increase output verbosity")

	if args.verbose == 2:
		print(f"the square of {args.square} equals {answer}")
	elif args.verbose == 1:
		print(f"{args.square}^2 == {answer}")
	else:
		print(answer)

	$ python3 t1.py  4
	16
	$ python3 t1.py  4 -v
	usage: t1.py [-h] [-v VERBOSE] square
	t1.py: error: argument -v/--verbose: expected one argument
	$ python3 t1.py  4 -v 1
	4^2 == 16
	$ python3 t1.py  4 -v 2
	the square of 4 equals 16
	$ python3 t1.py  4 -v 3		# not as expected
	16

	# 改进 2， 限制 --verbosity 选项可以接受的值
	parser.add_argument("-v", "--verbose", type=int, choices=[0, 1, 2],
						help="increase output verbosity")
	
	$ python3 t1.py  4 -v 3
	usage: t1.py [-h] [-v {0,1,2}] square
	t1.py: error: argument -v/--verbose: invalid choice: 3 (choose from 0, 1, 2)

	# 改进 3，改变冗长度
	parser.add_argument("-v", "--verbose", action="count",		# "count" action 用来统计特定选项出现的次数
						help="increase output verbosity")
	
	$ python3 t1.py  4 -v
	4^2 == 16
	$ python3 t1.py  4 -vv
	the square of 4 equals 16
	$ python3 t1.py  4 -vvv		# not as expected
	16

	# 改进 4
	if args.verbose >= 2:		# == 改为 >=
		print(f"the square of {args.square} equals {answer}")
	elif args.verbose == 1:
		print(f"{args.square}^2 == {answer}")
	else:
		print(answer)

	$ python3 t1.py  4 -vvv
	the square of 4 equals 16
	$ python3 t1.py  4
	Traceback (most recent call last):
	File "t1.py", line 10, in <module>
		if args.verbose >= 2:
	TypeError: '>=' not supported between instances of 'NoneType' and 'int' # 默认情况下如果一个可选参数没有被指定，它的值会是 None，不能和整数值相比较

	# 改进 5
	parser.add_argument("-v", "--verbose", action="count", default=0,		# 关键字 default 设置为 0 来让它可以与其他整数值进行比较
						help="increase output verbosity")
	
	# 扩展执行其他幂次的运算
	cat t1.py
	import argparse

	parser = argparse.ArgumentParser()
	parser.add_argument("x", type=int, help="the base")
	parser.add_argument("y", type=int, help="the exponent")
	parser.add_argument("-v", "--verbosity", action="count", default=0)
	args = parser.parse_args()
	answer = args.x**args.y
	if args.verbosity >= 2:
		print(f"{args.x} to the power {args.y} equals {answer}")
	elif args.verbosity >= 1:
		print(f"{args.x}^{args.y} == {answer}")
	else:
		print(answer)
	
	$ python3 t1.py  4 2
	16
	$ python3 t1.py  4 2 -v
	4^2 == 16
	$ python3 t1.py  4 2 -vv
	4 to the power 2 equals 16

	# 使用详细级别来显示 更多的 文本
	if args.verbosity >= 2:
		print(f"Running '{__file__}'")
	if args.verbosity >= 1:
		print(f"{args.x}^{args.y} == ", end="")
	print(answer)

	$ python3 t1.py  4 2
	16
	$ python3 t1.py  4 2 -v
	4^2 == 16
	$ python3 t1.py  4 2 -vv
	Running 't1.py'
	4^2 == 16

	# 处理 矛盾的选项 (--quiet 选项 与 --verbose 的作用相反)
	import argparse

	parser = argparse.ArgumentParser()
	group = parser.add_mutually_exclusive_group()
	group.add_argument("-v", "--verbose", action="store_true")
	group.add_argument("-q", "--quiet", action="store_true")
	parser.add_argument("x", type=int, help="the base")
	parser.add_argument("y", type=int, help="the exponent")
	args = parser.parse_args()
	answer = args.x**args.y

	if args.quiet:
		print(answer)
	elif args.verbose:
		print(f"{args.x} to the power {args.y} equals {answer}")
	else:
		print(f"{args.x}^{args.y} == {answer}")
	
	$ python3 t1.py  4 2
	4^2 == 16
	$ python3 t1.py  4 2 -q
	16
	$ python3 t1.py  4 2 -v
	4 to the power 2 equals 16
	$ python3 t1.py  4 2 -vq
	usage: t1.py [-h] [-v | -q] x y
	t1.py: error: argument -q/--quiet: not allowed with argument -v/--verbose
	$ python3 t1.py  4 2 -v --quiet
	usage: t1.py [-h] [-v | -q] x y
	t1.py: error: argument -q/--quiet: not allowed with argument -v/--verbose

	# 描述程序的主要目标
	parser = argparse.ArgumentParser(description="calculate X to the power of Y")
	```
- Python 面向对象
	```python
	面向对象编程 提供了一种组织程序的方法， 它把个体的属性和行为绑定在特定的对象上。

	Class
	Class 用来创建用户自定义的数据结构，在类中定义的函数称为方法 Method，Methods 定义了 Class 所定义的对象的行为，类只是用来定义对象的模板，不包含任何数据
	instance 实例是Class所创建的对象，它包含真正的数据

	Class 的定义
	# dog.py

	class Dog:
		species = "Canis familiaris"	# species 是 class attributes

		def __init__(self, name, age):
			self.name = name		# name 和 age 是 instance attributes
			self.age = age

		def __str__(self):
			return f"{self.name} is {self.age} years old"
		
		# Instance method
		def description(self):
			return f"{self.name} is {self.age} years old"

		# Another instance method
		def speak(self, sound):
			return f"{self.name} says {sound}"

	instance attributes ：在 __init__() 方法中定义的属性，称为实例属性，实例属性是类的特定实例的属性
	class attributes：类属性所有类的实例所具有的相同的属性

	Creating a new object from a class is called instantiating a class

	Methods like .__init__() and .__str__() are called dunder methods because they begin and end with double underscores
	__str__() 是一个特殊的实例方法， 用来自定义对象的print输出

	__repr__() provides the official string representation of an object, aimed at the programmer.
	__str__() provides the informal string representation of an object, aimed at the user.


	import datetime
	today = datetime.datetime.now()

	today	# datetime.datetime(2023, 2, 18, 18, 40, 2, 160890)
	print(today)	# 2023-02-18 18:40:02.160890


	类的继承
	# inheritance.py (attribute overridden)

	class Parent:
		hair_color = "brown"

	class Child(Parent):
		hair_color = "purple"


	# inheritance.py (attribute extended)

	class Parent:
		speaks = ["English"]

	class Child(Parent):
		def __init__(self):
			super().__init__()
			self.speaks.append("German")

	父类 与 子类
	# dog.py

	class Dog:
		species = "Canis familiaris"

		def __init__(self, name, age):
			self.name = name
			self.age = age

		def __str__(self):
			return f"{self.name} is {self.age} years old"

		def speak(self, sound):
			return f"{self.name} says {sound}"

	class JackRussellTerrier(Dog):
		pass

	class Dachshund(Dog):
		pass

	class Bulldog(Dog):
		pass

	>>> miles = JackRussellTerrier("Miles", 4)
	>>> buddy = Dachshund("Buddy", 9)
	>>> jack = Bulldog("Jack", 3)
	>>> jim = Bulldog("Jim", 5)

	>>> miles.species
	'Canis familiaris'

	>>> buddy.name
	'Buddy'

	>>> print(jack)
	Jack is 3 years old

	>>> jim.speak("Woof")
	'Jim says Woof'

	object 所属的 class
	>>> type(miles)
	<class '__main__.JackRussellTerrier'>

	某个具体对象 是否是 某个特定 类 的实例
	>>> isinstance(miles, Dog)
	True

	扩展父类的功能
	# dog.py

	# ...

	class JackRussellTerrier(Dog):
		def speak(self, sound="Arf"):
			return f"{self.name} says {sound}"

	# ...

	# dog.py

	# ...
	使用 super() 在子类的方法里访问父类的方法
	class JackRussellTerrier(Dog):
		def speak(self, sound="Arf"):
			return super().speak(sound)

	# ...
	```
- Executing Shell Commands with Python
	- [Run Bash Commands in Python: Subprocess Module Guide](https://ioflood.com/blog/python-run-bash-command/)
	- [The Right Way to Run Shell Commands From Python](https://martinheinz.dev/blog/98)
	```python
	Operating system programs are of two types, called shell and kernel programs.
	Kernel programs are the ones who perform the actual tasks, like creating a file or sending interrupts. 
	Shell is another program, whose job is to take input and decide and execute the required kernel program to do the job and show the output.
	The shell is also called the command processor.

	The terminal is the program that interacts with the shell and allows us to communicate with it via text-based commands. This is why it's also called the command line.

	Calling Shell Commands from Python:
	Option 1: OS.system (deprecated)
	Option 2: subprocess.call (recommended for Python<3.5)
	Option 3: subprocess.run (recommended since Python 3.5)


	# 示例 1
	#!/usr/bin/env python3                                                                            
	import subprocess

	command = "ls"
	options = "-l"

	args=[]
	args.append(command)
	args.append(options)

	output = subprocess.run(args,capture_output=True)

	print('Return code:', output.returncode)

	print('Output:',output.stdout.decode("utf-8"))
	```
## argparse
```python
Four straightforward steps to use Python’s argparse:
1. Import the argparse module.
2. Create an argument parser by instantiating ArgumentParser.
3. Add arguments and options to the parser using the .add_argument() method.
4. Call .parse_args() on the parser to get the Namespace of arguments.


Two types of command-line arguments:
1. Positional arguments, which you know as arguments
2. Optional arguments, which you know as options, flags, or switches.(Optional arguments aren’t mandatory. They allow you to modify the behavior of the command)

The first argument to the .add_argument() method sets the difference between arguments and options. This argument is identified as either name or flag. 
if you provide a name, then you’ll be defining an argument.
if you use a flag, then you’ll add an option.

import argparse
import datetime
from pathlib import Path

parser = argparse.ArgumentParser()

parser.add_argument("path")

parser.add_argument("-l", "--long", action="store_true")

args = parser.parse_args()

target_dir = Path(args.path)

if not target_dir.exists():
    print("The target directory doesn't exist")
    raise SystemExit(1)

def build_output(entry, long=False):
    if long:
        size = entry.stat().st_size
        date = datetime.datetime.fromtimestamp(
            entry.stat().st_mtime).strftime(
            "%b %d %H:%M:%S"
        )
        return f"{size:>6d} {date} {entry.name}"
    return entry.name

for entry in target_dir.iterdir():
    print(build_output(entry, long=args.long))
```