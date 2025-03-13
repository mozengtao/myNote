## 传递 shell 变量
```bash
# -v 选项
awk -v today="$(date)" -v weekday="Friday" 'BEGIN { print today " is " weekday}'
# 命令行参数
awk 'BEGIN { print ARGV[1] " " ARGV[2]}' $arg1 $arg2
seq 3 | awk '{ print var }' var="hello"
# shell环境变量
awk 'BEGIN { print ENVIRON["HOME"] }
```
## 注释
```bash
## 单行注释
print("Hello Comments!"); # 单行注释
## 多行注释
# 1
0 {
	use me 4 comment in head
	but only in English 
	no awk keyword can appear in there
}
# 2
{
	if(0) {
			# ...
			print("不会被执行");
	}
	print("会被执行");
}
```
## 使用外部的 shell 命令
```bash
echo "Test string" | awk '{ cmd=sprintf("echo %s", $0); system(cmd); }'
```

## 正则表达式作为域分隔符
```bash
echo "xxAA xxBxx C" | awk -F '(^x+)|( x)' '{ for(i = 1; i <= NF; i++) printf "-->%s<--\n", $i }'
```

## 打印两个pattern之间的行
```bash
# 包含 BEGIN 和 END pattern的边界
awk '/DATA BEGIN/, /DATA END/' input.txt

# 只包含 BEGIN 边界
awk '/DATA BEGIN/{ flag = 1 } /DATA END/{ flag = 0 } flag' input.txt

# 只包含 END 边界
awk 'flag; /DATA BEGIN/{ flag = 1 } /DATA END/{ flag = 0 }' input.txt

# 不包含 BEGIN 和 END pattern 的边界
awk '/DATA BEGIN/{ flag = 1; next } /DATA END/{ flag = 0 } flag' input.txt
# 
```
## getline
```bash
getline 				读取下一条记录保存到`$0`，同时更新 NF, NR, FNR, RT
getline var 			读取下一条记录保存到var，同时更新 NR, FNR, RT
getline <file			从文件读取下一条记录保存到$0，同时更新NF，RT
getline var <file 		从文件读取下一条记录保存到var，同时更新  RT
command | getline 		运行命令并将管道的输出保存到 $0
command | getline var	运行命令并将管道的输出保存到 var

# 1
BEGIN {
		command = "date"
		if ((command | getline date_now) < 0) {
		print "Can't get system date" > "/dev/stderr"
		exit 1
		}
		close(command)
		print "current date is", date_now
}
```
## 正则匹配
```bash
Metacharacter	Description
.				匹配 单个字符
[]				匹配 [] 中的任意字符
^				匹配 行起始位置
$				匹配 行结束位置
*				匹配 0个或多个字符
+				匹配 1个或多个字符
?				匹配 0个或1个字符

# match 匹配
if (match($0, /\.com$/)) {
	print $0
}
# replace 替换
sub(/555/, "666", $0)

# Backreferences 后向引用
sub(/\((\d{3})\) (\d{3})-(\d{4})/, "\1-\2-\3", $0)

# Grouping 匹配分组
if (match($0, /^(\w+)\s+(\d+)$/)) {
	name = substr($0, RSTART, RLENGTH)
	salary = substr($0, RSTART+length(name)+1, length($0)-RSTART-length(name))
	print name
	print salary
}

# Lookahead and Lookbehind
if (match($0, /(?<=http:\/\/)[^\/]+/)) {
	print substr($0, RSTART, RLENGTH)
}
# we use lookahead (?<=) to match the regular expression "/(?<=http://)[^/]+/" (which matches any characters that come after "http://" and before the next "/" character) and extract the domain name

# Negated character classes
if (match($0, /^[^@]+@example\.com$/)) {
	print $0
}
# we use a negated character class ([^@]+) to match any characters that are not "@" and extract the username, and then match the literal string "@example.com" to ensure that the address belongs to the specified domain


# Alternation
if (match($0, /\((\d{3})\) (\d{3})-(\d{4})|(\d{3})-(\d{3})-(\d{4})/)) {
	print substr($0, RSTART, RLENGTH)
}
# we use alternation (|) to match either the regular expression "/(\d3)(\d3) (\d{3})-(\d{4})/" (which matches a phone number in the format (XXX) XXX-XXXX) or the regular expression "/(\d{3})-(\d{3})-(\d{4})/" (which matches a phone number in the format XXX-XXX-XXXX)
```
## 字符串操作
```bash
length(string)
substr(string, start)
substr(string, start, length)
index(string, substring)
split(string, array, separator)
split(string, array, regexp)
# split(str, arr, ","); for (i in arr) print arr[i]
sub(regexp, replacement, string)	# first occurrence substitution
gsub(regexp, replacement, string)	# global substitution
match(string, regexp)
match(str, regex [, array])
tolower(string)
toupper(string)

# 1
str = "hello, world"
pos = match(str, "world")
if (pos > 0)
	print "Found at position", pos
else
	print "Not found"
# 2
str = "hello, world"
if (match(str, "world", arr))
	print "Found at position", arr[0], "with length", arr[1]
else
	print "Not found"

# 3
str = "The quick brown fox jumps over the lazy dog."
if (match(str, "the ([a-z]+) dog.", arr))
	print "The word after 'the' is:", substr(str, arr[2], arr["length",1])
else
	print "No match found"
```
[AWK: String functions](https://tecadmin.net/awk-string-functions/)  

[Idiomatic awk](https://backreference.org/2010/02/10/idiomatic-awk/index.html)  
[awk](http://awk.freeshell.org/)  
[Sed and Awk 101 Hacks](https://vds-admin.ru/sed-and-awk-101-hacks)  
[AWK实战](https://book.saubcy.com/AwkInAction/HOWTO.html)  
[The GNU Awk User's Guide](https://ftp.gnu.org/old-gnu/Manuals/gawk-3.1.1/html_node/)  
[sed & awk](https://docstore.mik.ua/orelly/unix/sedawk/index.htm)  
[awk reference](https://www3.physnet.uni-hamburg.de/physnet/Tru64-Unix/HTML/APS32DTE/WKXXXXXX.HTM)  
[awk learnbyexample](https://learnbyexample.github.io/learn_gnuawk/awk-introduction.html)  
[sed & awk](https://doc.lagout.org/operating%20system%20/linux/Sed%20%26%20Awk.pdf)  
[CLI text processing with GNU awk](https://learnbyexample.github.io/learn_gnuawk/cover.html)  
[Understanding Regular Expressions in Awk](https://tecadmin.net/awk-regular-expressions/)  