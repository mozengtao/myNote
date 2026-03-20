## string operations
[Bash String Manipulation](https://earthly.dev/blog/bash-string/)  
[Advanced String Operations in Bash: Building Custom Functions](https://karandeepsingh.ca/posts/advanced-string-operations-in-bash-building-custom-functions/)  
[String Manipulation in Bash Script: A Linux Shell Guide](https://ioflood.com/blog/bash-string-manipulation/)  
[Bash string manipulation cheatsheet](https://gist.github.com/magnetikonline/90d6fe30fc247ef110a1)  
[Manipulating Strings](https://tldp.org/LDP/abs/html/string-manipulation.html)  
[]()  
[]()  
[]()  
[]()  
[Regular expressions](https://computing.stat.berkeley.edu/tutorial-using-bash/regex.html)  
[How to Master String Manipulation in Bash Scripting](https://labex.io/tutorials/shell-how-to-master-string-manipulation-in-bash-scripting-392539)  
[**Manipulating Strings**](https://tldp.org/LDP/abs/html/string-manipulation.html)  
[How to Master Bash Regex Patterns](https://labex.io/tutorials/shell-how-to-master-bash-regex-patterns-390416)  
[How to Use Regex in Bash Scripting](https://labex.io/tutorials/shell-how-to-use-regex-in-bash-scripting-392579)  
```bash
#  bash 原生字符串替换：${变量/匹配串/替换串}

- 关键点说明：
  单斜杠 /：只替换第一个匹配
  双斜杠 //：替换所有匹配
  #：只在开头匹配时替换
  %：只在结尾匹配时替换

# 1. 替换第一次出现的模式
filename="test_file.txt.txt"
echo "${filename/.txt/.log}"   # 输出: test_file.log.txt

# 2. 替换所有出现的模式（使用双斜杠 //）
filename="test_file.txt.txt"
echo "${filename//.txt/.log}"  # 输出: test_file.log.log

# 3. 删除模式（替换为空字符串）
filename="test.file.name.txt"
echo "${filename/.}"           # 输出: testfile.name.txt
echo "${filename//.}"          # 输出: testfilenametxt

# 4. 从开头匹配并替换
filename="prefix_filename.ext"
echo "${filename/#prefix_/new_}"  # 输出: new_filename.ext

# 5. 从结尾匹配并替换
filename="file.bak"
echo "${filename/%.bak/.backup}"  # 输出: file.backup

# 6. 实际应用：修改文件扩展名
fullpath="/path/to/document.pdf"
newpath="${fullpath/%.pdf/.docx}"
echo "$newpath"  # 输出: /path/to/document.docx


# ​​语法​​	​                 ​方向​​	​​          示例输入​​	        ​输出​​	​​  说明​
# ${var%pattern}	    结尾最短匹配	file.txt.bak	file.txt	删除最短匹配后缀
# ${var%%pattern}	    结尾最长匹配	file.txt.bak	file	    删除最长匹配后缀
# ${var#pattern}	    开头最短匹配	/usr/bin/ls	    usr/bin/ls	删除最短匹配前缀
# ${var##pattern}	    开头最长匹配	/usr/bin/ls	    ls	        删除最长匹配前缀
# ​​${var/%pattern/str}​​	​​结尾精确替换​​	eth1np0	        eth1	    替换结尾匹配部分

### 1. 获取字符串长度
string="hello"
echo ${#string} # 5

### 2. 提取子字符串 ${string:position}, ${string:position:length}
string="hello world"
echo ${string:6:5}  # world

### 3. 查找字符串中的字符是否在字符集中出现，返回第一个出现在字符集中的字符的索引
string="hello world"
echo $(expr index "$string" "world")  # 3

### 4. 替换子字符串
#### 替换第一个匹配的子字符串 ${string/pattern/replacement}
string="hello world"
echo ${string/world/universe} # hello universe

#### 替换所有匹配的子字符串 ${string//pattern/replacement}
string="hello world world"
echo ${string//world/universe}  # hello universe universe

### 5. 删除子字符串
#### 从开头删除最短匹配
string="path/to/file.txt"
echo ${string#*/}   # to/file.txt

#### 从开头删除最长匹配
string="path/to/file.txt"
echo ${string##*/}  # file.txt

#### 从结尾删除最短匹配
string="path/to/file.txt"
echo ${string%.txt} # path/to/file

#### 从结尾删除最长匹配
string="path/to/file.txt"
echo ${string%%/*}  # path

interface="eth0np0"
base_if=${interface/%np[0-9]/}  # 得到 "eth0"

### 6. 转换大小写
#### 转换为小写
string="HELLO WORLD"
echo ${string,,}    # hello world

#### 转换为大写
string="hello world"
echo ${string^^}    # HELLO WORLD

### 7. 拼接字符串
string1="hello"
string2="world"
echo "$string1 $string2"  # hello world

string1+=" world"
echo $string1 # hello world

### 8. 检查字符串是否为空
string=""
if [ -z "$string" ]; then
  echo "String is empty"  # 
else
  echo "String is not empty"
fi

### 9. 检查字符串是否非空
string="hello"
if [ -n "$string" ]; then
  echo "String is not empty"  # 
else
  echo "String is empty"
fi

### 替换从左开始的匹配 ${string/#pattern/replacement}
str="aa bb bb aa"
echo ${str/#aa/cc}	# cc bb bb aa

### 替换从右开始的匹配 ${string/%pattern/replacement}
str="aa bb bb aa"
echo ${str/%aa/cc}	# aa bb bb cc

### 字符串删除
str="aa bb bb aa"
# 删除首次匹配
echo ${str/aa}	# bb bb aa
# 删除所有匹配
echo ${str//aa}	# bb bb

### 字符串匹配删除
filename="bash.string.txt"
## 最短匹配
# 从前开始匹配
echo ${filename#*.}	# string.txt
# 从后开始匹配
echo ${filename%.*}	# bash.string

## 最长匹配
# 从前开始匹配
echo ${filename##*.}	# txt
# 从后开始匹配
echo ${filename%%.*}	# bash

### 字符串大小写转换
legend="john nash"
actor="JULIA ROBERTS"

echo ${legend^^}		# JOHN NASH
echo ${actor,,}			# julia roberts
echo ${legend^}			# John nash
echo ${actor,}			# jULIA ROBERTS
echo ${legend^^[jn]}	# JohN Nash

### 正则匹配
file="repair-report-12.5.pdf"
pattern='([0-9]*\.[0-9]*\.pdf)'

if  [[ $file =~ $pattern ]]; then
	echo ${BASH_REMATCH[1]}		# 12.5.pdf
else
	echo "No version found"
fi

```