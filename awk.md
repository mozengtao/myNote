
[GAWK](https://www.gnu.org/software/gawk/manual/gawk.html)  
[The GNU Awk User's Guide](https://ftp.gnu.org/old-gnu/Manuals/gawk-3.1.1/html_node/)  
[Effective AWK Programming](https://www.mssl.ucl.ac.uk/swift/om/sw/help/man/gawk.html)  
[Awk Quick Reference](https://www.grymoire.com/Unix/AwkRef.html)  
[AWK 组合多个条件（Multiple Conditions）心智模型](awk/multi_condition.md)  
[AWK 中 `{}` 与 `;` 的使用规则、最佳实践及心智模式](awk/brace_colon.md)  
[]()  
[]()  
[awk 内部执行模型（Pattern–Action VM）总结](awk_pattern_action.md)  
[awk 语法格式最佳实践](awk_format_practice.md)  
[AWK "锚点定位 + 相对偏移取值" 心法](awk/awk-anchor-offset-pattern.md)  
[AWK「锚点定位 + 相对偏移取值」核心思想与心智模式](awk/awk-anchor-offset-guide.md)  
[AWK 核心心智模型：Anchor → Boundary → Extract](awk/awk-anchor-boundary-extrac.md)  
[AWK Anchor → Boundary → Extract 示例](awk/awk-anchor-boundary-extract-example.md)  


- [AWK 系统化掌握手册](awk/awk_mastery.md)
- [AWK 编程语言参考指南](awk/awk_ref.md)
- [AWK 常用内置函数与复杂逻辑配方](awk/awk_functions.md)
- [AWK "Buffer-and-Judge" 模式](awk/Buffer-and-Judge.md)


[sed & awk](https://docstore.mik.ua/orelly/unix/sedawk/index.htm)  
[awk reference](https://www3.physnet.uni-hamburg.de/physnet/Tru64-Unix/HTML/APS32DTE/WKXXXXXX.HTM)  
[awk learnbyexample](https://learnbyexample.github.io/learn_gnuawk/awk-introduction.html)  
[sed & awk](https://doc.lagout.org/operating%20system%20/linux/Sed%20%26%20Awk.pdf)  
[CLI text processing with GNU awk](https://learnbyexample.github.io/learn_gnuawk/cover.html)  
[Understanding Regular Expressions in Awk](https://tecadmin.net/awk-regular-expressions/)  

awk/awk-tutorial/
├── [part1-core-mental-model.md](awk/awk-tutorial/part1-core-mental-model.md)  
├── [part2-pattern-matching.md](awk/awk-tutorial/part2-pattern-matching.md)  
├── [part3-core-actions.md](awk/awk-tutorial/part3-core-actions.md)  
├── [part4-programming-language.md](awk/awk-tutorial/part4-programming-language.md)  
├── [part5-daily-use-cases.md](awk/awk-tutorial/part5-daily-use-cases.md)  
├── [part6-functions.md](awk/awk-tutorial/part6-functions.md)  
├── [part7-professional-style.md](awk/awk-tutorial/part7-professional-style.md)  
└── [part8-tool-choice-checklist.md](awk/awk-tutorial/part8-tool-choice-checklist.md)  

```bash
# awk 的心智模型
for each line {
    if (pattern) {
        action
    }
}

# 经典模式
- 状态开关模式 (The State Machine / Toggle)
```Awk
场景： 只打印 SQL 文件中 CREATE TABLE 到 ; 之间的内容

/^CREATE TABLE/, /;/ { print }

手动控制标志位（更灵活）：

/^START/ { inside = 1; next }
/^END/   { inside = 0; next }
inside   { print }
```

- 数组计数与去重模式 (Associative Array Aggregation)
```Awk
场景： 统计日志文件中每个 IP 地址出现的次数

{ count[$1]++ }
END {
    for (ip in count) {
        print ip, count[ip]
    }
}
```

- 多文件关联模式 (The Two-File Join)
```Awk
场景： file1 是白名单，file2 是原始数据，只打印在白名单中的行

NR == FNR { whitelist[$1]; next } # 处理第一个文件，存入数组
$1 in whitelist { print }         # 处理第二个文件，检查是否存在
```

- 动态字段/转置模式 (Dynamic Field Transformation)
```Awk
场景： 计算每一行的总和（无论有多少列）

{
    sum = 0
    for (i = 1; i <= NF; i++) sum += $i
    print "Row", NR, "Sum:", sum
}
```

- 段落模式 (Paragraph Mode / RS)
```Awk
场景： 处理类似 cat /proc/cpuinfo 这种以空行分隔的配置块

BEGIN { RS = "" ; FS = "\n" }
/model name/ { 
    print "Processor " ++count ": " $3 
}
```

- The "Buffer-and-Judge" Logic

It’s particularly useful for configuration files (like HCL, JSON, or YAML) where you need to act on a multi-line block based on a specific value found somewhere inside it.

When awk processes a file line-by-line, it doesn't naturally know what's coming next. To handle a block based on its internal content, you follow these four logical stages:

1. Detection (Start): Use a regex to identify the start of the block. Once found, set a "state flag" (e.g., in_block = 1) and clear any previous temporary storage.
2. Buffering (Accumulation): While the flag is active, instead of printing lines immediately, save them into an array (e.g., buffer[]). This puts the lines in "waiting" while you scan for the criteria.
3. Identification (The Target): As you buffer each line, check if it contains the specific "trigger" pattern. If it does, set a "target flag" (e.g., target_block = 1).
4. Action (End/Flush): When you hit the closing brace (}), the block is complete. You now look at your "target flag":
    If the flag is true: Loop through the buffer and modify the lines.
    If the flag is false: Loop through the buffer and print the lines as-is.
    Reset: Clear the flags and the buffer for the next potential block.


```Awk
/START_PATTERN/ { in_block=1; found=0; i=0 }

in_block {
    buffer[++i] = $0
    if ($0 ~ /TARGET_CRITERIA/) { found=1 }

    if (/END_PATTERN/) {
        for (j=1; j<=i; j++) {
            if (found) {
                # Action: Modify line
                print "#" buffer[j]
            } else {
                # Action: Leave alone
                print buffer[j]
            }
        }
        in_block=0
    }
    next
}
{ print } # Print everything outside of blocks
```


```bash
# length([s]) --- 获取字符串长度，若省略参数s则默认处理整行$0
awk '{print length($0)}' <<< "Hello World"
# 输出：11 (空格算作1个字符)

# index(s, t) --- 查找子串t在源字符串s中首次出现的位置（从1开始计数），未找到则返回0
awk '{print index($0, "World")}' <<< "Hello World"
# 输出：7 (因为'W'位于第7个字符)

# substr(s, i [, n]) --- 从源字符串s的第i个位置开始截取，若指定n则截取n个字符，否则截取至末尾
awk '{print substr($0, 7, 5)}' <<< "Hello World"
# 输出：World (从第7位开始取5个字符)

# 查定位+截取
echo "[INFO] Code: 200" | awk '
{
    pos = index($0, ":")
    if (pos > 0) {
        code = substr($0, pos + 2)
        print "提取编号: " code
    }
}'
# 输出：提取编号: 200

# split(s, a [, r]) --- 按分隔符r将字符串s分割并存入数组a，返回数组元素个数；若省略r则默认以FS（字段分隔符）分割
awk '{n = split($0, a, ","); print a[2]}' <<< "apple,banana,orange"
# 输出：banana (分割后数组第2个元素)

# match(s, r [, a]) --- 在字符串s中匹配正则表达式r，匹配成功时设置内置变量RSTART（起始位置）和RLENGTH（匹配长度），返回匹配起始位置，否则返回0
awk '{if(match($0, /[0-9]+/)) print substr($0, RSTART, RLENGTH)}' <<< "id:12345"
# 输出：12345 (提取出连续的数字部分)

# sub(r, s [, t]) --- 在目标字符串t（默认为$0）中，用s替换第一次出现的正则表达式r，返回替换成功的次数（0或1）
awk '{sub(/o/, "0"); print}' <<< "Hello World"
# 输出：Hell0 World (仅替换了第一个字母'o')

# gsub(r, s [, t]) --- 在目标字符串t（默认为$0）中，用s全局替换所有匹配正则表达式r的内容，返回替换的总次数
awk '{gsub(/o/, "0"); print}' <<< "Hello World"
# 输出：Hell0 W0rld (替换了全部的字母'o')

awk '{count = gsub(/l/, "l", $0); print "字母l出现了 " count " 次"}' <<< "Hello World"
# 输出：字母l出现了 3 次

# tolower(s) --- 将字符串s中的所有大写字母转换为小写，并返回转换后的新字符串
awk '{print tolower($0)}' <<< "Hello World"
# 输出：hello world

# toupper(s) --- 将字符串s中的所有小写字母转换为大写，并返回转换后的新字符串
awk '{print toupper($0)}' <<< "Hello World"
# 输出：HELLO WORLD

# printf(format, expr-list) --- 按 format 字符串格式化表达式并输出到标准输出（不自动换行，需显式添加 \n）
awk '{printf "Name: %-8s | Age: %03d | Score: %.1f\n", $1, $2, $3}' <<< "Alice 7 88.75"
# 输出：Name: Alice    | Age: 007 | Score: 88.8

# sprintf(fmt, expr-list) --- 按格式化字符串fmt的规则，将表达式列表expr-list进行格式化，返回格式化后的字符串（不直接打印）
awk '{result = sprintf("%s-%d", $1, length($1)); print result}' <<< "File"
# 输出：File-4 (将单词与长度拼接成固定格式)

# printf 与 sprintf 的联合使用（先构造再输出）
awk '{msg = sprintf("Time: %02d:%02d", $1, $2); printf "[INFO] %s\n", msg}' <<< "9 5"
# 输出：[INFO] Time: 09:05

# strtonum(s) --- 将字符串s转换为十进制数字，支持识别十六进制（0x开头）和八进制（0开头）
awk '{print strtonum($0)}' <<< "0xFF"
# 输出：255 (十六进制FF转为十进制255)

# 基于行号的偏移：提取匹配行的相邻行
# 场景：从日志文件中，找出包含“Begin final coordinates”的行，并提取其后的第4行到第8行
# 示例数据 (data.txt)
Line 1
Line 2
Begin final coordinates
Line 4
Line 5
Line 6
Line 7
Line 8
Line 9
Line 10

awk '/Begin final coordinates/ {n=NR+4; m=NR+8} (n < NR) && (NR < m)' data.txt

# 基于字段位置的偏移：提取锚点字段附近的字段
# 场景：处理一个用 * 或 ~ 分隔的数据文件，找到字段值为 N4 的位置，并输出其后的第2个字段
# 示例数据 (data.csv)
N4*WALTER*WHITE~DMG*D8*19630625~N4*CORA*PA*15108~

identifier="N4"
position=2
awk -F '[*~]' -v id="$identifier" -v pos="$position" '
{
    for (i=1; i<=NF; i++) {
        if ($i == id) {
            if (i+pos <= NF)
                print $(i+pos)
            else
                print "invalid position"
        }
    }
}' data.csv

# 基于字符位置的偏移：提取锚点字符周围的子串
# 场景：从一个字符串中，提取以第6个字符为中心，左右各延伸5个字符的子串
# 示例数据
str="54321#123456789"

awk -v i=6 -v o=5 '
{
    start = (i - o > 0) ? i - o : 1
    length = 2 * o + (i - o > 0 ? 1 : (i - 1))
    print substr($0, start, length)
}' <<< "$str"

# 动态循环偏移：查找并输出所有匹配位置
# 场景：在文件中，找出所有单词 "to" 出现的位置（行号和列号）
awk -v str='to' '
{
    off = 0
    while (pos = index(substr($0, off + 1), str)) {
        printf("%d: %d\n", NR, pos + off)
        off += length(str) + pos
    }
}' file.txt
```
