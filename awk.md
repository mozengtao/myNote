
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

## extract sections
```bash
# input.txt:
collectd_us_ofdma_partial_reason_count{us_ofdma="port->0;pchidx->12;reason->low-snr",instance="docsis_mac_320ca5a1"} 2 1762848325000
collectd_us_ofdma_partial_reason_count{us_ofdma="port->0;pchidx->12;reason->reg-ack-tccerr",instance="docsis_mac_320ca5a1"} 1 1762848325000

# process.sh:
awk '{
  pch = ""; reason = "";
  if (match($0, /pchidx->([0-9]+)/, a)) pch = a[1];
  if (match($0, /reason->([^";,}]+)/, b)) reason = b[1];
  value = $2;
  print pch, reason, value
}' input.txt

# process.sh:
# for busybox awk, it does not support the GNU awk extension of match(string, regex, array)
# it only supports match(string, regex)
awk '{
  pch = ""; reason = "";
  if (match($0, /pchidx->[0-9]+/)) {
    tmp = substr($0, RSTART, RLENGTH);
    sub(/^.*->/, "", tmp);
    pch = tmp;
  }
  if (match($0, /reason->[^";,}]+/)) {
    tmp = substr($0, RSTART, RLENGTH);
    sub(/^reason->/, "", tmp);
    reason = tmp;
  }
  value = $2;
  print pch, reason, value
}' input.txt

# OUTPUT:
12 low-snr 2
12 reg-ack-tccerr 1

# input.txt
morris-dentist_md1_0  90:58:51:5c:9c:5c  0/13  0  0  0  0  2  1  0  1  0  0  0
                                         0/12  0  0  0  0  2  1  0  1  0  0  0
                      0c:b9:37:a2:14:40  0/13  0  0  0  0  0  0  0  1  0  0  1
                                         0/12  0  0  0  0  0  0  0  1  0  0  1

# process.sh:
awk '
{
  ucid_field = 0
  for (i = 1; i <= NF; i++) {
    if ($i ~ /^[0-9]+\/[0-9]+$/) {
      ucid = $i
      ucid_field = i
      break
    }
  }
  if (ucid_field > 0) {
    for (i = ucid_field + 1; i <= NF; i++) {
      col = i - ucid_field + 1
      sum[ucid, col] += $i
      if (col > max_col) max_col = col
    }
    ucid_list[ucid] = 1
  }
}
END {
  for (ucid in ucid_list) {
    printf "%s", ucid
    for (i = 2; i <= max_col; i++)
      printf " %d", sum[ucid, i] + 0
    printf "\n"
  }
}' input.txt | sort -V

# some explanation
if (i > max_col) max_col = i    # keep track of the maximum number of columns seen
sum[ucid, i] + 0                # forces numeric output (avoids printing nothing if unset)
ucid_list[ucid] = 1             # clean, independent list of UCIDs to iterate later
sort -V                         # sort output for deterministic order

# OUTPUT:
0/12 0 0 0 0 2 1 0 2 0 0 1
0/13 0 0 0 0 2 1 0 2 0 0 1


```


## array
```bash
# 1
awk 'BEGIN {
    arr["apple"] = 3
    arr["banana"] = 5
    arr[10] = "ten"
    print arr["apple"]
    print arr[10]
}'

# 2
awk 'BEGIN {
    fruits["apple"] = 3
    fruits["banana"] = 5

    # odrder is arbitary
    for (k in fruits) {
        print k, fruits[k]
    }
}'

# 3
awk 'BEGIN {
    fruits["apple"] = 3
    fruits["banana"] = 5

    delete fruits["banana"]
    for (k in fruits) {
        print k, fruits[k]
    }
}'

# 5
awk 'BEGIN {
    fruits["apple"] = 3
    
    if ("apple" in fruits)
        print "apple exists"
    
    if(!("banana" in fruits))
        print "banana missing"
}'

# 6
awk 'BEGIN {
    fruits["apple"] = 3
    fruits["banana"] = 5

    print length(fruits)
}'

# 7
awk 'BEGIN {
    n = split("read,green,blue", colors, ",")
    for(i = 1; i <= n; i++)
        print i, colors[i]
}'

# 8 (gawk only)
awk 'BEGIN {
    arr[1, "x"] = 10
    arr[2, "y"] = 20

    print arr[1, "x"], arr[2, "y"]
}'

# example
awk '
/^NSI_BOND/ {
    bond["NSI","RxB"]=$3; bond["NSI","TxB"]=$4
    bond["NSI","RxP"]=$5; bond["NSI","TxP"]=$6
}
/^NSI_SLAVE/ {
    slave["NSI","RxB"]+=$3; slave["NSI","TxB"]+=$4
    slave["NSI","RxP"]+=$5; slave["NSI","TxP"]+=$6
}
/^SSI_BOND/ {
    bond["SSI","RxB"]=$3; bond["SSI","TxB"]=$4
    bond["SSI","RxP"]=$5; bond["SSI","TxP"]=$6
}
/^SSI_SLAVE/ {
    slave["SSI","RxB"]+=$3; slave["SSI","TxB"]+=$4
    slave["SSI","RxP"]+=$5; slave["SSI","TxP"]+=$6
}
END {
    for (b in bond) {
        split(b, k, SUBSEP)
        iface=k[1]; metric=k[2]
        printf "%s %s: bond=%s slavesum=%s -> %s\n",
            iface, metric, bond[b], slave[b],
            (bond[b]==slave[b] ? "OK" : "MISMATCH")
    }
}' interfaces.txt

# or

awk '
# Bond interfaces
/^NSI_BOND/ {
    bond["NSI"]["RxB"]=$3; bond["NSI"]["TxB"]=$4
    bond["NSI"]["RxP"]=$5; bond["NSI"]["TxP"]=$6
}
/^SSI_BOND/ {
    bond["SSI"]["RxB"]=$3; bond["SSI"]["TxB"]=$4
    bond["SSI"]["RxP"]=$5; bond["SSI"]["TxP"]=$6
}

# Slave interfaces
/^NSI_SLAVE/ {
    slave["NSI"]["RxB"]+=$3; slave["NSI"]["TxB"]+=$4
    slave["NSI"]["RxP"]+=$5; slave["NSI"]["TxP"]+=$6
}
/^SSI_SLAVE/ {
    slave["SSI"]["RxB"]+=$3; slave["SSI"]["TxB"]+=$4
    slave["SSI"]["RxP"]+=$5; slave["SSI"]["TxP"]+=$6
}

END {
    for (iface in bond) {
        for (metric in bond[iface]) {
            printf "%s %s: bond=%s slavesum=%s -> %s\n",
                iface, metric,
                bond[iface][metric], slave[iface][metric],
                (bond[iface][metric]==slave[iface][metric] ? "OK" : "MISMATCH")
        }
    }
}' interfaces.txt

# or
awk '
# Match bond interfaces (anything ending with _BOND)
$1 ~ /_BOND$/ {
    prefix = substr($1, 1, length($1)-5)   # remove "_BOND"
    bond[prefix]["RxB"]=$3; bond[prefix]["TxB"]=$4
    bond[prefix]["RxP"]=$5; bond[prefix]["TxP"]=$6
}

# Match slave interfaces (anything starting with same prefix + "_SLAVE")
$1 ~ /_SLAVE[0-9]*$/ {
    n = index($1, "_SLAVE")
    prefix = substr($1, 1, n-1)            # get prefix before "_SLAVE"
    slave[prefix]["RxB"]+=$3; slave[prefix]["TxB"]+=$4
    slave[prefix]["RxP"]+=$5; slave[prefix]["TxP"]+=$6
}

END {
    for (iface in bond) {
        for (metric in bond[iface]) {
            printf "%s %s: bond=%s slavesum=%s -> %s\n",
                iface, metric,
                bond[iface][metric], slave[iface][metric],
                (bond[iface][metric]==slave[iface][metric] ? "OK" : "MISMATCH")
        }
    }
}' interfaces.txt


```

## Tips
```bash
# Rule of Thumb
Use sed for simple edits (substitution, deletion, insertion).
Use awk when you need to process fields, add conditions, or compute.


By default every variable inside a function is global in awk, to avoid name conflicts and bugs, for awk function, we declare them as extra params, this is a standard awk idiom for creating local variables, so the paramters are 2 parts:
1. the real parameters (you pass them when calling)
2. the local scratch variables (only used inside the function)

for example:
function msplit(str, arr, seps, joinstr, tmp, raw, i, n, c) {
    ...
}
str, arr, seps, joinstr are intended parameters you pass when calling
tmp, raw, i, n, c are local variables

# substitue
echo '[ 64.150.80.45 2001:64:150:100:80::3a ]' \
| awk '{ gsub(/^\[ *| *\]$/, ""); print }'

echo "3c:c4:4f:21:00:88" \
| awk '/^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$/ {print}'

# write to multiple files based on conditions
awk '
{
    mac = $1
    status = $4

    # Write full line to status-specific file
    if (status == "operational") {
        print $0 >> "operational.txt"
    } else if (status == "offline") {
        print $0 >> "offline.txt"
    }

    # Always write summary to a separate file
    print mac, status >> "summary.txt"

    # Optional: close files to avoid too many open files
    close("operational.txt")
    close("offline.txt")
    close("summary.txt")
}
' input.txt

# how to execute external cmd
# read one line of output from cmd
awk 'BEGIN {
	cmd = "date +%Y-%m-%d"
	cmd | getline result
	close(cmd)					# always close the pipe
	print "Today is:", result
}'

# just execute cmd, ignore output
awk 'BEGIN { system("echo hello from system") }'

# capture multiple lines into an array
awk 'BEGIN {
    cmd = "ls /etc | head -3"

    n = 0
    # getline returns >0 while there are lines
    while ((cmd | getline line) > 0) {
        files[++n] = line
    }
    close(cmd)
}
{
    print $1, " | Sample file:", files[(NR % n) + 1]
}' /etc/passwd

# send awk output to external cmd
awk '{ print $0 | "wc -c" }' /etc/passwd

# read multiple files inw awk simultaneously
# 2 files
awk 'FNR==NR {			# while reading first file
	a[$1] = $2			# store 1st col as key, 2nd col as value in array a
	next				# skip to next line
}
{
	if ($1 in a) {		# while reading second file, check if 1st col exists in array a
		print $0, a[$1]	# if exists, print the whole line and the corresponding value from array a
	}
}' file1.txt file2.txt

# 3 files
awk '
# file1
FNR==NR {
	arr1[$1]=$2
	next
}
# file2
FNR!=NR && NR<=FNR+NR1 {
	arr2[$1]=$2
	next
}
# file3
{
	print $1, arr1[$1], arr2[$1], $2
}
' file1 file2 file3

# 3 files
awk '
# first file
ARGIND==1 {
	arr1[$1]=$2
	next
}
# second file
ARGIND==2 {
	arr2[$1]=$2
	next
}
# third file
ARGIND==3 {
	print $1, arr1[$1], arr2[$1], $2
}
' file1.txt file2.txt file3.txt

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

# find all matches
{
    text = $0
    start = 1
    while ((start = match(text, /[0-9]+/)) > 0) {
        found = substr(text, start, RLENGTH)
        print "Found number:", found
        text = substr(text, start + RLENGTH)
    }
}

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
