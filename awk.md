
[GAWK](https://www.gnu.org/software/gawk/manual/gawk.html)  
[The GNU Awk User's Guide](https://ftp.gnu.org/old-gnu/Manuals/gawk-3.1.1/html_node/)  
[Effective AWK Programming](https://www.mssl.ucl.ac.uk/swift/om/sw/help/man/gawk.html)  
[Awk Quick Reference](https://www.grymoire.com/Unix/AwkRef.html)  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  


[sed & awk](https://docstore.mik.ua/orelly/unix/sedawk/index.htm)  
[awk reference](https://www3.physnet.uni-hamburg.de/physnet/Tru64-Unix/HTML/APS32DTE/WKXXXXXX.HTM)  
[awk learnbyexample](https://learnbyexample.github.io/learn_gnuawk/awk-introduction.html)  
[sed & awk](https://doc.lagout.org/operating%20system%20/linux/Sed%20%26%20Awk.pdf)  
[CLI text processing with GNU awk](https://learnbyexample.github.io/learn_gnuawk/cover.html)  
[Understanding Regular Expressions in Awk](https://tecadmin.net/awk-regular-expressions/)  

## Tips
```bash
By default every variable inside a function is global in awk, to avoid name conflicts and bugs, for awk function, we declare them as extra params, this is a standard awk idiom for creating local variables, so the paramters are 2 parts:
1. the real parameters (you pass them when calling)
2. the local scratch variables (only used inside the function)

for example:
function msplit(str, arr, seps, joinstr, tmp, raw, i, n, c) {
    ...
}
str, arr, seps, joinstr are intended parameters you pass when calling
tmp, raw, i, n, c are local variables

# match
# how match works
match(string, regex, array) # array is optional
1. Looks in string for the first substring matching regex.
2. If found:
    Returns the position (1-based) of the match.
    Sets special variables:
        RSTART → position of the match.
        RLENGTH → length of the match.
    If array is provided, fills it with captured groups.
3. If no match:
    Returns 0
    RSTART = 0, RLENGTH = -1

# Find first match
echo "hello world" | awk '{
    if (match($0, /world/)) {
        print "Found at:", RSTART, "length:", RLENGTH
    }
}'

# Extract substring using RSTART and RLENGTH
echo "user=alice,uid=1001" | awk '{
    if (match($0, /uid=[0-9]+/)) {
        print substr($0, RSTART, RLENGTH)
    }
}'

# Use array to capture groups
echo "2025-09-20" | awk '{
    if (match($0, /([0-9]{4})-([0-9]{2})-([0-9]{2})/, m)) {
        print "Year:", m[1], "Month:", m[2], "Day:", m[3]
    }
}'

# Multiple matches with a loop
echo "cat dog cat dog" | awk '{
    str=$0
    while (match(str, /cat/)) {
        print "Found cat at:", RSTART
        str = substr(str, RSTART + RLENGTH)  # move past match
    }
}'

# match Search a string with a regex
# index Find literal substring, no regex.
# gsub Replace all matches of regex with replacement

# example match + index + gsub
echo "user=alice uid=1001" | awk '{
    # match: extract the uid
    if (match($0, /uid=[0-9]+/))
        print "match() →", substr($0, RSTART, RLENGTH)

    # index: check where "alice" starts
    print "index() →", index($0, "alice")

    # gsub: replace alice with bob
    s=$0
    gsub(/alice/, "bob", s)
    print "gsub() →", s
}'

#
echo '90:58:51:5c:9c:5c   [ 64.150.41.18 2001:64:150:100::ff:a ]  operational(oA)' \
| awk '{
    if (match($0, /\[[^]]*\]/)) {
        ipblock = substr($0, RSTART+1, RLENGTH-2)  # contents inside [ ]
        n = split(ipblock, arr, " ")

        ipv4=""; ipv6=""
        for (i=1; i<=n; i++) {
            if (arr[i] ~ /^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$/) {
                ipv4 = arr[i]
            } else if (arr[i] ~ /:/) {
                ipv6 = arr[i]
            }
        }

        if (ipv4 != "") print "IPv4: " ipv4
        if (ipv6 != "") print "IPv6: " ipv6
    }
}'


# split
awk '{
    n = split($0, parts, /[:\-,]/)
    for (i = 1; i <= n; i++) print parts[i]
}' <<< "aa:bb-cc,dd"

awk '{
    n = split($0, parts, /[[:space:],]+/)
    for (i=1; i<=n; i++) print parts[i]
}' <<< "one, two   three"

# gsub changes the text in place
echo "apple banana apple" | awk '{ gsub("apple", "orange"); print }'
echo "1:2:3" | awk -F: '{ gsub("2","20",$2); print $2 }'
echo "cat bat mat" | awk '{ gsub(/[bm]at/, "xxx"); print }'
echo "hello hello world" | awk '{
    n = gsub(/hello/, "hi")
    print $0, "→ replaced", n, "times"
}'
echo "foo" | awk '{ gsub(/foo/, "[&]"); print }'

# gsub + split (multiple seperators -> one delimiter, then split)
echo "apple,banana;cherry" | awk '{
    gsub(/[;,]/, " ", $0)
    n = split($0, arr, " ")
    for (i=1; i<=n; i++) print arr[i]
}'

echo "a   b\t\tc" | awk '{
    gsub(/[ \t]+/, " ")
    n = split($0, arr, " ")
    for (i=1; i<=n; i++) print arr[i]
}'

echo "k1=v1;k2=v2" | awk '{
    gsub(";", " ", $0)          # turn ; into spaces
    n = split($0, pairs, " ")
    for (i=1; i<=n; i++) {
        split(pairs[i], kv, "=")
        print kv[1] " → " kv[2]
    }
}'

# use a function for gsub + split
# Trim helper
function trim(s) {
    sub(/^[ \t\r\n]+/, "", s)
    sub(/[ \t\r\n]+$/, "", s)
    return s
}

# msplit:
#   str      → input string
#   arr      → output array
#   seps     → regex for separators
#   joinstr  → optional join separator (if non-empty returns joined string)
#
#   (tmp, raw, i, n, c are locals)
function msplit(str, arr, seps, joinstr,
                tmp, raw, i, n, c, joined) {

    tmp = str
    gsub(seps, " ", tmp)            # replace all separators with space
    n = split(tmp, raw, " ")

    c = 0
    for (i = 1; i <= n; i++) {
        raw[i] = trim(raw[i])
        if (raw[i] != "") {
            c++
            arr[c] = raw[i]
        }
    }

    if (joinstr != "") {            # if join requested
        joined = arr[1]
        for (i = 2; i <= c; i++) joined = joined joinstr arr[i]
        return joined
    }

    return c                         # otherwise return count
}

echo "apple ;; banana ||| cherry ,, pear" | awk '
# include msplit & trim here
{
    n = msplit($0, parts, "[;|,]")
    for (i = 1; i <= n; i++) print parts[i]
}'

echo "apple ;; banana ||| cherry ,, pear" | awk '
# include msplit & trim here
{
    result = msplit($0, parts, "[;|,]", "|")
    print result
}'




# convert hex str to dec str
echo "00:11:80:68:48:ee" | awk -F: '{for(i=1;i<=NF;i++){printf "%d%s", strtonum("0x"$i), (i<NF?".":"\n")}}'
Output:0.17.128.104.72.238
# script version
BEGIN {
	FS = ":"
}

{
	for(i = 1; i <= NF; i++) {
		printf "%d%s", strtonum("0x"$i), (i == NF ? "\n" : ".")
	}
}

# structured according to awk’s definition of a program: condition { actions }
awk '{if ($0 ~ /pattern/) print $0}'	# not idiomatic for awk, so not recommended
awk '$0 ~ /pattern/ {print $0}'			# condition { actions }
awk '/pattern/ {print $0}'				# implicitly applies to $0
awk '/pattern/ {print}'					# print alone, by default, prints $0
awk '/pattern/'							# the default action is print

awk '(NR%2 && /pattern/) || (!(NR%2) && /anotherpattern/)'
awk '{sub(/pattern/, "foobar")} 1'		# the same as "sed 's/pattern/foobar/'"

awk 'NR % 6'            # prints all lines except lines 6,12,18...
awk 'NR > 5'            # prints from line 6 onwards (like tail -n +6, or sed '1,5d')
awk '$2 == "foo"'       # prints lines where the second field is "foo"
awk 'NF >= 6'           # prints lines with 6 or more fields
awk '/foo/ && /bar/'    # prints lines that match /foo/ and /bar/, in any order
awk '/foo/ && !/bar/'   # prints lines that match /foo/ but not /bar/
awk '/foo/ || /bar/'    # prints lines that match /foo/ or /bar/ (like grep -e 'foo' -e 'bar')
awk '/foo/,/bar/'       # prints from line matching /foo/ to line matching /bar/, inclusive
awk 'NF'                # prints only nonempty lines (or: do not print empty lines, where NF==0)
awk 'NF--'              # removes last field and prints the line
awk '$0 = NR" "$0'      # prepends line numbers (assignments are valid in conditions)
awk '!a[$0]++'          # suppresses duplicated lines! (figure out how it works)

seq 1 30 | awk 'ORS = NR % 5 ? FS : RS'

# Self-assignments
awk -v FS=';' -v OFS=',' 1   # doesn't work!,  awk does not rebuild $0 (that is, replacing FS with OFS) until some field is modified
awk -v FS=';' -v OFS=',' '$1=$1'
awk '$1=$1' FS=\; OFS=,		 # any argument that has a "=" in it is treated as a variable assignment instead of a file to read

# Build strings with separators
string = string ";" somedata				# not expected
string = string sep somedata; sep = ";"		# right way to Build strings with separators

$ cat matrix.txt
a1;a2;a3;a4;a5
b1;b2;b3;b4;b5
c1;c2;c3;c4;c5

awk -F\; '{
for(i=1;i<=NF;i++)
	r[i]=r[i] sep $i;
	sep=FS
}
END {
	for(i=1;i<=NF;i++)
		print r[i]
}' matrix.txt

# Two-file processing
awk 'NR == FNR { # some actions; next} # other condition {# other actions}' file1.txt file2.txt

# prints lines that are both in file1.txt and file2.txt (intersection)
awk 'NR == FNR{a[$0];next} $0 in a' file1.txt file2.txt
# prints lines that are only in file1.txt and not in file2.txt
awk 'NR == FNR{a[$0];next} !($0 in a)' file2.txt file1.txt
# prints lines that are only in fil2.txt and not in file1.txt
awk 'NR == FNR{a[$0];next} !($0 in a)' file1.txt file2.txt

cat data.txt
20081010 1123 xxx
20081011 1234 def
20081012 0933 xyz

cat map.txt
def payment
xyz deposit
xxx balance

# use information from a map file to modify a data file
awk 'NR == FNR{a[$1]=$2;next} {$3=a[$3]}1' map.txt data.txt

# replace each number with its difference from the maximum
awk 'NR == FNR{if($0 > max) max = $0;next} {$0 = max - $0}1' file.txt file.txt


# 打印进程的环境变量
awk '{print $0}' FS="=" RS="\000" /proc/1648/environ

# gawk 支持 正则RS (传统的 awk 不支持 正则RS)
data.txt:
Name:Alice,Age:30;Name:Bob,Age:25
awk 'BEGIN { RS = "[,;]"; FS=":" } { print "Record:", NR, "-> Name:", $2, "Age:", $4 }' data.txt

# 函数调用的一般规则​ (除 getline 函数外，所有函数调用建议始终加括号)
1. 对于内置函数的调用，推荐始终加括号​​ 以增强可读性和兼容性
2. 对于自定义函数的调用，必须加括号
3. 对于数学函数的调用，必须加括号
4. getline 函数语法特殊，不需要加括号

sub(/old/, "new", $0)
gsub(/[0-9]/, "#", $0)

x = sqrt(100)
y = sin(3.14)

n = length($0)

while (getline line < "file.txt") { ... }

# AWK 是弱类型语言，会根据上下文自动转换数据类型
1. 字符串转数字​​：如果字符串以数字开头（如 "123"），会转换为对应的数字；否则转换为 0
​2. ​数字转字符串​​：在需要字符串的上下文中，数字会自动转为字符串形式

awk 'BEGIN {print "123abc" + 0}				# 123
awk 'BEGIN {print "" + 0}'					# 0
awk 'BEGIN {if ("hello") print "true"}'		# true

awk '{total += $1 + 0} END {print total}' data.txt				# 处理可能含非数字的字段
awk '{if ($1 + 0 == $1) print "Valid number:", $1}' data.txt	# 过滤非数字数据
```

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

```
