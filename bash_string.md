```bash
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
[Matching regex in bash](https://thedukh.com/2022/10/matching-regex-in-bash/)