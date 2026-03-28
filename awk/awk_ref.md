# AWK 编程语言参考指南（语法 / 函数 / 示例）

> 面向日常日志与文本数据处理的 AWK 速查与示例集合。
>
> - **兼容性说明**：文中标注“gawk 扩展”的条目需要 GNU awk（`gawk`）。如果你用的是 `mawk`/`nawk`/BusyBox `awk`，这些扩展可能不可用。
> - **示例约定**：命令行示例默认用 `awk`，需要扩展时可将 `awk` 替换为 `gawk`。

## 目录

- 1. 基本语法结构
  - 1.1 执行方式（命令行）
  - 1.2 程序结构组成（BEGIN / 主体 / END）
  - 1.3 记录、字段与分隔符（FS/OFS/RS/ORS）
- 2. 模式（Pattern）
  - 2.1 常见模式类型
  - 2.2 正则匹配（~ / !~）
- 3. 动作（Action）与语句组织
  - 3.1 Action 块规则（省略 pattern / 省略 action）
  - 3.2 语句分隔（换行 / 分号）
- 4. 控制结构
  - 4.1 条件语句
  - 4.2 循环结构
  - 4.3 跳转语句
- 5. 变量与数据类型
  - 5.1 常用内置变量
  - 5.2 变量操作与类型转换
- 6. 数组（关联数组 / 哈希表）
  - 6.1 数组基础
  - 6.2 数组遍历与排序（gawk 扩展）
- 7. 函数
  - 7.1 字符串函数
  - 7.2 数学函数
  - 7.3 输入输出与重定向
  - 7.4 时间函数（gawk 扩展）
  - 7.5 自定义函数（function）
- 8. 典型使用示例
  - 8.1 数据处理
  - 8.2 复杂逻辑
  - 8.3 实用单行命令
- 9. 最佳实践 / 常见陷阱 / 调试
  - 9.1 性能优化
  - 9.2 常见陷阱
  - 9.3 调试技巧（gawk）
- 10. 参考速查

---

## 1. 基本语法结构

### 1.1 执行方式（命令行）

```bash
awk 'pattern { action }' filename
awk -F: 'pattern { action }' filename          # 指定分隔符（FS）
awk -v var=value 'pattern { action }' filename # 定义变量
awk -f script.awk filename                     # 从脚本文件执行
```

### 1.2 程序结构组成（BEGIN / 主体 / END）

```awk
BEGIN {
    # 初始化代码：在处理任何输入前执行一次
}

pattern1 {
    # 匹配 pattern1 时执行的动作
}

pattern2 {
    # 匹配 pattern2 时执行的动作
}

END {
    # 收尾代码：处理完所有输入后执行一次
}
```

补充规则：

- **省略 pattern**：`{ action }` 会对每一行执行一次 action。
- **省略 action**：`pattern` 等价于 `pattern { print }`（默认打印整行 `$0`）。

### 1.3 记录、字段与分隔符（FS/OFS/RS/ORS）

- **记录（record）**：默认按行读取，一行是一条记录（由 `RS` 决定，默认 `\n`）。
- **字段（field）**：默认以空白分割字段（由 `FS` 决定；默认是“一个或多个空白字符”）。
- **字段访问**：`$0` 是整行；`$1..$NF` 是字段；`NF` 是字段数。
- **输出分隔**：`OFS` 控制 `print $1, $2` 中字段间分隔；`ORS` 控制记录结束符。

---

## 2. 模式（Pattern）

### 2.1 常见模式类型

| 模式类型 | 语法示例 | 说明 |
| --- | --- | --- |
| 空模式 | `{ action }` | 匹配所有行 |
| 正则表达式 | `/regex/` | 匹配正则的行 |
| 比较表达式 | `$3 > 100` | 字段/表达式比较 |
| 范围模式 | `/start/, /end/` | 从 start 到 end 的行范围（含两端） |
| BEGIN/END | `BEGIN {}` / `END {}` | 程序开始/结束时执行 |
| 复合条件 | `$1=="foo" && $2>50` | 逻辑组合（`&&`/`||`/`!`） |

### 2.2 正则匹配（`~` / `!~`）

下面是常见的 pattern 写法示例（通常后面还会跟 `{ action }`）：

```awk
/^#/              # 以 # 开头的行
/foo|bar/         # 包含 foo 或 bar 的行
/[0-9]+/          # 包含数字的行
$2 ~ /^[A-Z]/     # 第 2 字段以大写字母开头
$0 !~ /error/     # 整行不包含 error
```

---

## 3. 动作（Action）与语句组织

### 3.1 Action 块规则（省略 pattern / 省略 action）

Action 必须写在 `{ ... }` 内（除非 action 被省略以使用默认的 `print`）。

```awk
{ print }                 # 空 pattern：每行都执行
/ERROR/ { print $0 }      # 匹配 ERROR 的行打印整行
$3 > 100 { print $1, $3 } # 条件匹配后输出指定字段
```

默认行为：

- **只有 pattern**：`pattern` 等价于 `pattern { print }`
- **只有 action**：`{ action }` 等价于对每条记录执行一次

### 3.2 语句分隔（换行 / 分号）

规则要点：

- **换行**默认就是语句分隔符
- **同一行多条语句**需要用分号 `;` 分隔
- **块内最后一个分号**可省略

```awk
{
    print $1      # 换行分隔
    print $2      # 新语句
    x = 5         # 赋值语句
}
```

```awk
{ print $1; print $2; x = 5 }  # 同一行多语句：用分号分隔
```

控制结构中的花括号规则：

```awk
# if-else 结构：单条语句时花括号可省略
if (condition)
    action1
else
    action2

# 多条语句必须使用花括号
if (condition) {
    action1
    action2
} else {
    action3
    action4
}
```

---

## 4. 控制结构

### 4.1 条件语句

```awk
# if
if (condition)
    action

# if / else if / else
if (condition) {
    action1
    action2
} else if (condition2) {
    action3
} else {
    action4
}

# 三元运算符（条件表达式）
result = (condition) ? value_if_true : value_if_false
max = (a > b) ? a : b
```

### 4.2 循环结构

```awk
# while
while (condition) {
    statements
    # break 跳出循环
    # continue 跳过当前迭代
}

# do-while（至少执行一次）
do {
    statements
} while (condition)

# for（C 风格）
for (i = 1; i <= 10; i++) {
    print i
}

# for-in：遍历数组（无序）
for (idx in array) {
    print idx, array[idx]
}
```

数组排序遍历（gawk 扩展）：

```awk
# 按索引字符串升序（还有：@ind_num_asc, @ind_num_desc, @val_str_asc 等）
PROCINFO["sorted_in"] = "@ind_str_asc"
for (idx in array) {
    print idx, array[idx]
}
```

### 4.3 跳转语句

```awk
break        # 跳出当前循环
continue     # 跳过当前迭代，进入下一次
next         # 跳过当前行，读取下一行输入
nextfile     # 跳过当前文件，处理下一个文件（gawk 扩展）
exit [code]  # 退出程序，可选返回状态码
```

---

## 5. 变量与数据类型

### 5.1 常用内置变量

| 变量 | 说明 | 默认值 / 备注 |
| --- | --- | --- |
| `$0` | 当前整行记录 | - |
| `$1..$NF` | 第 1..NF 个字段 | - |
| `NF` | 当前行字段数 | - |
| `NR` | 当前行号（累计） | - |
| `FNR` | 当前文件的行号 | - |
| `FS` | 输入字段分隔符 | 默认：空白（一个或多个空格/Tab） |
| `OFS` | 输出字段分隔符 | 默认：空格 |
| `RS` | 输入记录分隔符 | 默认：`\n` |
| `ORS` | 输出记录分隔符 | 默认：`\n` |
| `FILENAME` | 当前文件名 | - |
| `ARGC` | 命令行参数个数 | - |
| `ARGV` | 命令行参数数组 | - |

### 5.2 变量操作与类型转换

```awk
# 变量无需声明，首次使用自动创建
count = 0
name = "Alice"
price = 19.99

# 字符串连接：相邻即连接（无显式运算符）
full = first " " last

# 数字 → 字符串：自动转换
num = 42
str = "Value: " num   # "Value: 42"

# 字符串 → 数字：参与算术运算时转换
n = "42" + 0          # 42
```

---

## 6. 数组（关联数组 / 哈希表）

### 6.1 数组基础

```awk
# 创建数组（无需声明）
arr[1] = "one"
arr["name"] = "John"
arr[$1] = $2          # 使用字段作为键

# 统计元素个数（POSIX awk：手动计数）
cnt = 0
for (k in arr) cnt++

# gawk 扩展：length(arr) 返回元素个数（与 length("str") 区分）
# cnt = length(arr)

# 删除元素 / 删除整个数组
delete arr["key"]
delete arr

# 检查键是否存在
if ("key" in arr) {
    # ...
}

# 多维数组（用复合下标模拟）
matrix[1, 2] = 100    # 实际键为 "1\0342"
```

### 6.2 数组遍历与排序（gawk 扩展）

```awk
# 基本遍历（无序）
for (key in arr) {
    print key, arr[key]
}

# gawk：按指定方式排序后遍历
PROCINFO["sorted_in"] = "@ind_str_asc"
for (key in arr) {
    print key " -> " arr[key]
}
```

---

## 7. 函数

### 7.1 字符串函数

```awk
# length([string])：返回字符串长度；省略参数等价于 length($0)
length("hello")       # 5
length($0)            # 当前行长度
length               # 等价于 length($0)

# index(string, substring)：查找子串位置（1-based，找不到返回 0）
index("hello", "ll")  # 3

# match(string, regex [, array])：正则匹配；返回匹配起始位置
match($0, /[0-9]+/, a)  # a[0] 为匹配到的文本（gawk）；也可用 RSTART/RLENGTH

# split(string, array [, regex])：分割字符串；返回分段数量
split("a:b:c", parts, ":")  # 返回 3，parts[1]="a"

# substr(string, start [, length])：截取子串
substr("hello", 2, 3)  # "ell"

# sub / gsub：替换（sub 只替换第一个；gsub 替换所有）
sub(/foo/, "bar", $0)      # 修改 $0
gsub(/[0-9]/, "*", $0)     # 所有数字变为 *

# gensub(regex, replacement, how [, target])（gawk 扩展）
gensub(/([a-z]+)/, "<\\1>", "g", $0)  # \\1 是第一个捕获组

# tolower / toupper：大小写转换
tolower("HELLO")  # "hello"

# sprintf(format, ...)：格式化为字符串（不直接输出）
sprintf("%s: %d", name, score)
```

### 7.2 数学函数

```awk
int(x)                 # 整数部分（向 0 取整）
sqrt(x)                # 平方根
exp(x)                 # e^x
log(x)                 # 自然对数
sin(x); cos(x); atan2(y, x)  # 三角函数（弧度）
rand()                 # 0~1 随机数
srand([seed])          # 设置随机种子
```

### 7.3 输入输出与重定向

```awk
# print：输出（字段之间使用 OFS，末尾自动追加 ORS）
print $1, $2
print $1 "," $2
print "Total:", total

# printf：格式化输出（注意自行写 \n）
printf "Name: %s, Score: %.2f\n", name, score

# 重定向
print $0 >  "output.txt"    # 覆盖写入
print $0 >> "output.txt"    # 追加写入
print $0 |  "command"       # 管道到命令

# getline：读取输入
getline                   # 读取下一行到 $0，更新 NF/NR 等
getline var               # 读取到变量 var，不更新 $0
getline < "file"          # 从文件读取
"command" | getline var   # 从命令输出读取

# close：关闭文件或管道（尤其是管道/大量文件句柄时建议关闭）
close("output.txt")
close("command")
```

### 7.4 时间函数（gawk 扩展）

```awk
systime()                         # 当前时间戳（秒）
strftime("%Y-%m-%d %H:%M:%S")     # 格式化时间
mktime("2024 01 15 10 30 00")     # 时间字符串 → 时间戳
```

### 7.5 自定义函数（function）

```awk
function process_block(block,    i) {
    # 参数列表后可用额外局部变量（这里的 i 仅在函数内可见）
    for (i = 1; i <= 3; i++) {
        # ...
    }
    return 0
}
```

---

## 8. 典型使用示例

### 8.1 数据处理

计算平均值（第 3 列）：

```bash
awk '{ sum += $3; count++ } END { print "Average:", sum / count }' data.txt
```

列求和与格式化输出（对每列求和）：

```bash
awk '{
    for (i = 1; i <= NF; i++) {
        sum[i] += $i
    }
}
END {
    for (i = 1; i <= NF; i++) {
        printf "Col %d: %10.2f\n", i, sum[i]
    }
}' matrix.txt
```

去重（保留首次出现）：

```bash
awk '!seen[$0]++' file.txt
```

统计词频（示例：英文单词，忽略大小写；输出前 20）：

```bash
awk '{
    for (i = 1; i <= NF; i++) {
        word = tolower($i)
        gsub(/[^a-z]/, "", word)
        if (word != "") freq[word]++
    }
}
END {
    for (w in freq) print freq[w], w
}' file.txt | sort -rn | head -20
```

### 8.2 复杂逻辑

多文件处理：统计每个文件的行数（顺序不保证，与 awk 的数组遍历规则有关）：

```awk
{ files[FILENAME]++ }
END {
    for (f in files)
        printf "%6d %s\n", files[f], f
}
```

状态机模式处理日志（`process_block` 为自定义函数）：

```awk
/START/ { in_block = 1; block = "" }
in_block { block = block $0 "\n" }
/END/ {
    if (in_block) {
        process_block(block)
        in_block = 0
    }
}
```

关联数组实现分组统计（按第 4 列部门汇总第 5 列工资）：

```awk
{
    dept = $4
    salary[dept] += $5
    count[dept]++
}
END {
    for (d in salary) {
        avg = salary[d] / count[d]
        printf "%-10s: Total=%10.2f, Avg=%10.2f, Count=%d\n", \
               d, salary[d], avg, count[d]
    }
}
```

### 8.3 实用单行命令

打印第 2 列的唯一值：

```bash
awk '{ print $2 }' file | sort -u
```

条件过滤并格式化输出：

```bash
awk '$3 > 100 { printf "%-20s %8.2f\n", $1, $3 }' data.txt
```

替换分隔符（CSV → TSV）：

```bash
awk 'BEGIN { FS=","; OFS="\t" } { $1=$1; print }' file.csv
```

统计：总行数 / 总字数 / 总字符数：

```bash
awk '{ lines++; words+=NF; chars+=length($0)+1 } END { print lines, words, chars }' file.txt
```

每 10 行合并为 1 行：

```bash
awk '{ printf "%s%s", $0, (NR%10 ? " " : "\n") } END { print "" }' file.txt
```

---

## 9. 最佳实践 / 常见陷阱 / 调试

### 9.1 性能优化

- **尽早 `next`**：对不需要的行尽快跳过，减少后续规则计算。
- **避免重复计算**：例如把不变表达式放到循环外；大数组遍历尽量少做多次。
- **管道/文件句柄**：大量使用 `print | "cmd"` 或 `getline < file` 时，必要时配合 `close()` 释放资源。

### 9.2 常见陷阱

未初始化变量的双重语义（数值为 0、字符串为空）：

```awk
if (x == "") {
    # x 可能未赋值，也可能被显式赋了空字符串
}
```

浮点数比较不要直接用 `==`（示例给出 abs 的自定义实现）：

```awk
function abs(v) { return v < 0 ? -v : v }

if (abs(a - b) < 0.0001) {
    # a 近似等于 b
}
```

修改字段会触发 `$0` 重建（从而让 `OFS` 生效）：

```awk
$1 = "new"   # 修改后 $0 会按 OFS 重建
```

### 9.3 调试技巧（gawk）

- **启动调试器**：`gawk -D -f script.awk file`（或 `gawk --debug -f script.awk file`）。
- **变量转储**：`gawk -d vars.txt -f script.awk file`（或 `gawk --dump-variables=vars.txt -f script.awk file`）。
- **打印调试信息到 stderr**：

```awk
{ print "DEBUG: NR=" NR ", $0=[" $0 "]" > "/dev/stderr" }
```

---

## 10. 参考速查

| 任务 | 代码片段 |
| --- | --- |
| 打印特定行 | `NR==5` 或 `NR>=10 && NR<=20` |
| 修改分隔符 | `BEGIN{FS=":"; OFS="\t"}` |
| 跳过空行 | `NF > 0 { ... }` 或 `/./ { ... }` |
| 删除前 N 行 | `NR > 5 { print }` |
| 只处理最后 N 行 | 使用数组缓存，或先用 `tail` 预处理 |
| 多条件过滤 | `/pattern/ && $3 > 100 { ... }` |
