
## sed 和 awk
```bash
## sed 的适用场景: 以 行 为单位的快速文本转换
1.简单替换
sed -i 's/foo/bar/g' file.txt		# 替换所有 "foo" 为 "bar"，直接修改文件
2.删除/保留特定行
sed '/^$/d' file.txt				# 删除空行
sed -n '5,10p' file.txt				# 保留第5到10行
3.插入/追加文本
sed '3i Hello' file.txt				# 在第3行前插入一行 "Hello"
sed '/end/a World' file.txt			# 在匹配 "end" 的行后追加一行 "World"
4.批量行级操作
sed = file.txt | sed 'N; s/\n/ /'	# 为每行添加行号


## awk 的适用场景: 结构化数据处理, 基于 列 的逻辑处理，支持数学运算、条件判断、自定义变量等复杂操作
1. 按列提取/处理数据
awk '{sum += $3; print $1, $3} END {print "Total:", sum}' data.txt		# 提取第1列和第3列，并计算总和
2. 条件过滤与统计
awk '$2 > 100 {count++} END {print count}' data.txt			# 统计第2列大于100的行数
3. 字段重组与格式化
awk 'BEGIN {FS=","; OFS="\t"; print "Name\tAge\tScore"} {print $1, $2, $3}' data.csv	# 将逗号分隔转为制表符分隔，并添加表头
4. 关联数据处理
awk 'NR==FNR {a[$1]=$2; next} $1 in a {print $0, a[$1]}' file1.txt file2.txt	# 合并两个文件（按第1列匹配）
5. 复杂文本生成
awk '{printf "INSERT INTO table VALUES (\"%s\", %d);\n", $1, $2}' data.txt		# 生成SQL插入语句

```

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
tolower(string)
toupper(string)

match(string, regexp)
match(string, regexp, array)	# string: 待搜索的字符串, regexp: 字符串或者/pattern/, array: 用于保存匹配结果的数组(仅GNU awk等支持)
# 关键特性
1. RSTART: 匹配的起始位置(等同于返回值)
2. RLENGTH: 匹配的子串长度(未匹配时为-1)
# 数组捕获分组（仅部分支持，如GNU awk）
array[0]:
array[1], array[2], ... : 正则表达式中括号对应的子串

# 1
INPUT: error 404: Not found
{
    if (match($0, /error [0-9]+/)) {
        print "Match Location:", RSTART, "Length:", RLENGTH
        print "Match Content:", substr($0, RSTART, RLENGTH)
    }
}
Output:
Match Location: 1 Length: 9
Match Content: error 404

# 2
INPUT: error 404: Not found
{
    if (match($0, /error ([0-9]+): (.+)/, arr)) {
        print "Error Code:", arr[1]
        print "Error Msg:", arr[2]
    }
}
OUTPUT:
Error Code: 404
Error Msg: Not found

# 3
INPUT: username=alice age=30
{
    if (match($0, /([a-z]+)=([0-9]+)/, arr)) {
        print "Key:", arr[1], "Value:", arr[2]
    }
}
OUTPUT:
Key: age Value: 30
```

[GAWK](https://www.gnu.org/software/gawk/manual/gawk.html)  

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