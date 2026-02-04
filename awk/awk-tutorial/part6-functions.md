# Part 6: awk Functions You Actually Use

## 6.1 Built-in Functions (With Daily Scenarios)

```
+------------------------------------------------------------------+
|                 AWK BUILT-IN FUNCTIONS                            |
+------------------------------------------------------------------+
|                                                                   |
|  CATEGORIES:                                                      |
|  +----------------------------------------------------------+    |
|  |  String Functions:                                        |    |
|  |  length, substr, index, split, sub, gsub, sprintf,        |    |
|  |  tolower, toupper, match                                  |    |
|  +----------------------------------------------------------+    |
|  |  Numeric Functions:                                       |    |
|  |  int, sqrt, sin, cos, log, exp, rand, srand               |    |
|  +----------------------------------------------------------+    |
|  |  I/O Functions:                                           |    |
|  |  getline, print, printf, close, system                    |    |
|  +----------------------------------------------------------+    |
|  |  Time Functions (gawk):                                   |    |
|  |  systime, mktime, strftime                                |    |
|  +----------------------------------------------------------+    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

AWK 内置函数分为四大类：

| 类别 | 常用函数 | 用途 |
|------|----------|------|
| 字符串 | `length`, `substr`, `split`, `gsub` | 文本处理 |
| 数值 | `int`, `sqrt`, `rand` | 数学计算 |
| I/O | `getline`, `print`, `system` | 输入输出 |
| 时间 | `systime`, `strftime` | 时间处理（gawk） |

---

### length()

```
+------------------------------------------------------------------+
|                    length()                                       |
+------------------------------------------------------------------+
|                                                                   |
|  SYNTAX:  length(string)                                          |
|           length(array)   # gawk: returns number of elements      |
|           length          # same as length($0)                    |
|                                                                   |
|  RETURNS: Number of characters in string                          |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLES:                                                        |
|  ---------                                                        |
|                                                                   |
|  # Get string length                                              |
|  awk '{print length($1)}'                                         |
|  Input:  "hello world"                                            |
|  Output: 5                                                        |
|                                                                   |
|  # Filter by length                                               |
|  awk 'length($0) > 80'   # Lines longer than 80 chars             |
|  awk 'length($1) == 3'   # First field exactly 3 chars            |
|                                                                   |
|  # Pad string to fixed width                                      |
|  awk '{                                                           |
|      len = length($1)                                             |
|      pad = 20 - len                                               |
|      printf "%s%*s|\n", $1, pad, ""                               |
|  }'                                                               |
|                                                                   |
|  # Count unique values (gawk)                                     |
|  awk '{seen[$1]++} END {print length(seen)}'                      |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  DAILY USE CASE: Validate field length                            |
|  awk -F, '$1 != "" && length($1) < 3 {                            |
|      print "Warning: short ID at line " NR ": " $1                |
|  }' data.csv                                                      |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**`length()` 的常见用法：**

```bash
# 获取字符串长度
length("hello")   # 返回 5

# 过滤长行
awk 'length > 80'

# 检查字段长度
awk 'length($1) < 3 {print "太短:", $1}'

# 统计唯一值数量（gawk）
awk '{a[$1]++} END {print length(a)}'
```

---

### substr()

```
+------------------------------------------------------------------+
|                    substr()                                       |
+------------------------------------------------------------------+
|                                                                   |
|  SYNTAX:  substr(string, start)         # From start to end       |
|           substr(string, start, length) # From start, length chars|
|                                                                   |
|  NOTE: Position starts at 1 (not 0!)                              |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLES:                                                        |
|  ---------                                                        |
|                                                                   |
|  # Extract substring                                              |
|  awk '{print substr($0, 1, 10)}'  # First 10 characters           |
|  awk '{print substr($0, 5)}'      # From position 5 to end        |
|                                                                   |
|  # Extract date parts                                             |
|  Input: "2024-01-15 10:30:00"                                     |
|                                                                   |
|  awk '{                                                           |
|      year  = substr($1, 1, 4)                                     |
|      month = substr($1, 6, 2)                                     |
|      day   = substr($1, 9, 2)                                     |
|      print year, month, day                                       |
|  }'                                                               |
|  Output: 2024 01 15                                               |
|                                                                   |
|  # Remove first N characters                                      |
|  awk '{print substr($0, 4)}'      # Remove first 3 chars          |
|                                                                   |
|  # Remove last N characters                                       |
|  awk '{print substr($0, 1, length($0)-3)}'  # Remove last 3       |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  DAILY USE CASE: Extract timestamp components                     |
|  awk '{                                                           |
|      ts = $1 " " $2                                               |
|      hour = substr($2, 1, 2)                                      |
|      count[hour]++                                                |
|  }                                                                |
|  END {for (h in count) print h ":00", count[h]}' app.log          |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**`substr()` 要点：**

| 调用方式 | 结果 |
|----------|------|
| `substr(s, 5)` | 从第 5 个字符到末尾 |
| `substr(s, 5, 3)` | 从第 5 个字符取 3 个 |
| `substr(s, 1, len-1)` | 去掉最后一个字符 |

**注意：** 位置从 1 开始，不是从 0！

```bash
# 提取固定位置的数据
awk '{print substr($0, 1, 8)}'  # 前 8 个字符

# 截取日期部分
awk '{print substr($1, 1, 10)}' # YYYY-MM-DD
```

---

### index()

```
+------------------------------------------------------------------+
|                    index()                                        |
+------------------------------------------------------------------+
|                                                                   |
|  SYNTAX:  index(string, target)                                   |
|                                                                   |
|  RETURNS: Position of first occurrence of target in string        |
|           0 if not found                                          |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLES:                                                        |
|  ---------                                                        |
|                                                                   |
|  awk 'BEGIN {print index("hello world", "wor")}'                  |
|  Output: 7                                                        |
|                                                                   |
|  awk 'BEGIN {print index("hello world", "xyz")}'                  |
|  Output: 0  (not found)                                           |
|                                                                   |
|  # Check if substring exists                                      |
|  awk 'index($0, "ERROR") > 0'  # Lines containing "ERROR"         |
|                                                                   |
|  # Extract after delimiter                                        |
|  awk '{                                                           |
|      pos = index($0, ":")                                         |
|      if (pos > 0)                                                 |
|          print substr($0, pos+1)                                  |
|  }'                                                               |
|  Input:  "key: value here"                                        |
|  Output: " value here"                                            |
|                                                                   |
|  # Split on first occurrence only                                 |
|  awk '{                                                           |
|      pos = index($0, "=")                                         |
|      key = substr($0, 1, pos-1)                                   |
|      val = substr($0, pos+1)                                      |
|      print "KEY:", key, "VAL:", val                               |
|  }'                                                               |
|  Input:  "name=John=Doe"                                          |
|  Output: KEY: name VAL: John=Doe                                  |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  DAILY USE CASE: Parse key=value with = in value                  |
|  awk '{                                                           |
|      eq = index($0, "=")                                          |
|      if (eq > 0) {                                                |
|          key = substr($0, 1, eq-1)                                |
|          val = substr($0, eq+1)                                   |
|          config[key] = val                                        |
|      }                                                            |
|  }' config.txt                                                    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**`index()` 的用途：**

```bash
# 查找子串位置
index("hello", "ll")  # 返回 3

# 判断是否包含
awk 'index($0, "ERROR") > 0'  # 类似 grep

# 只在第一个分隔符处分割
pos = index($0, "=")
key = substr($0, 1, pos-1)
val = substr($0, pos+1)
```

**与 split 的区别：** `index` 找位置，`split` 做分割。当分隔符可能出现在值中时，用 `index` + `substr` 更安全。

---

### split()

```
+------------------------------------------------------------------+
|                    split()                                        |
+------------------------------------------------------------------+
|                                                                   |
|  SYNTAX:  split(string, array, separator)                         |
|           split(string, array)  # Uses FS                         |
|                                                                   |
|  RETURNS: Number of elements created                              |
|  EFFECT:  Populates array[1], array[2], ...                       |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLES:                                                        |
|  ---------                                                        |
|                                                                   |
|  # Split IP address                                               |
|  awk '{                                                           |
|      n = split("192.168.1.100", octets, ".")                      |
|      print "Octets:", n                                           |
|      print "Third:", octets[3]                                    |
|  }'                                                               |
|  Output:                                                          |
|  Octets: 4                                                        |
|  Third: 1                                                         |
|                                                                   |
|  # Split time string                                              |
|  awk '{                                                           |
|      split($2, t, ":")                                            |
|      hour = t[1]                                                  |
|      min  = t[2]                                                  |
|      sec  = t[3]                                                  |
|      print hour "h " min "m " sec "s"                             |
|  }'                                                               |
|  Input:  "2024-01-15 10:30:45"                                    |
|  Output: 10h 30m 45s                                              |
|                                                                   |
|  # Split with regex separator (gawk)                              |
|  awk '{                                                           |
|      n = split($0, parts, /[,;:]/)                                |
|      for (i=1; i<=n; i++) print parts[i]                          |
|  }'                                                               |
|  Input:  "a,b;c:d"                                                |
|  Output: a b c d (one per line)                                   |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  DAILY USE CASE: Parse path components                            |
|  awk '{                                                           |
|      n = split($0, parts, "/")                                    |
|      filename = parts[n]                                          |
|      print "File:", filename                                      |
|  }'                                                               |
|  Input:  "/home/user/docs/file.txt"                               |
|  Output: File: file.txt                                           |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**`split()` 要点：**

```bash
# 基本用法
n = split("a:b:c", arr, ":")
# arr[1]="a", arr[2]="b", arr[3]="c", n=3

# 分割 IP 地址
split(ip, octets, ".")
# octets[1]="192", octets[2]="168", ...

# 分割路径
n = split(path, parts, "/")
filename = parts[n]  # 最后一部分是文件名
```

**注意：** `split()` 会清空目标数组，然后从 1 开始填充。

---

### tolower() / toupper()

```
+------------------------------------------------------------------+
|                 tolower() / toupper()                             |
+------------------------------------------------------------------+
|                                                                   |
|  SYNTAX:  tolower(string)   # Convert to lowercase                |
|           toupper(string)   # Convert to uppercase                |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLES:                                                        |
|  ---------                                                        |
|                                                                   |
|  # Case-insensitive matching                                      |
|  awk 'tolower($0) ~ /error/'                                      |
|  # Matches: ERROR, Error, error, eRrOr                            |
|                                                                   |
|  # Normalize case before comparison                               |
|  awk 'tolower($1) == "yes" {print "Confirmed"}'                   |
|                                                                   |
|  # Convert entire field                                           |
|  awk '{$1 = toupper($1); print}'                                  |
|  Input:  "hello world"                                            |
|  Output: "HELLO world"                                            |
|                                                                   |
|  # Case-insensitive counting                                      |
|  awk '{count[tolower($1)]++}                                      |
|       END {for (w in count) print w, count[w]}'                   |
|                                                                   |
|  Input:                   Output:                                 |
|  Apple                    apple 3                                 |
|  APPLE                    banana 2                                |
|  apple                                                            |
|  Banana                                                           |
|  banana                                                           |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  DAILY USE CASE: Normalize log levels                             |
|  awk '{                                                           |
|      level = toupper($3)                                          |
|      if (level == "ERROR" || level == "ERR")                      |
|          errors++                                                 |
|  }                                                                |
|  END {print "Total errors:", errors+0}'                           |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**大小写转换的应用：**

```bash
# 大小写不敏感匹配
awk 'tolower($0) ~ /error/'

# 统计时归一化
awk '{count[tolower($1)]++}'

# 输出规范化
awk '{print toupper($1)}'
```

---

### sub() / gsub()

```
+------------------------------------------------------------------+
|                    sub() / gsub()                                 |
+------------------------------------------------------------------+
|                                                                   |
|  SYNTAX:  sub(regex, replacement, target)    # Replace first      |
|           gsub(regex, replacement, target)   # Replace all        |
|           sub(regex, replacement)            # target = $0        |
|           gsub(regex, replacement)           # target = $0        |
|                                                                   |
|  RETURNS: Number of substitutions made                            |
|  EFFECT:  Modifies target in place                                |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  IMPORTANT: sub/gsub MODIFY the target, unlike C's strstr         |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLES:                                                        |
|  ---------                                                        |
|                                                                   |
|  # Replace first occurrence                                       |
|  awk '{sub(/old/, "new"); print}'                                 |
|  Input:  "old old old"                                            |
|  Output: "new old old"                                            |
|                                                                   |
|  # Replace all occurrences                                        |
|  awk '{gsub(/old/, "new"); print}'                                |
|  Input:  "old old old"                                            |
|  Output: "new new new"                                            |
|                                                                   |
|  # Replace in specific field                                      |
|  awk '{gsub(/-/, "/", $1); print}'                                |
|  Input:  "2024-01-15 data"                                        |
|  Output: "2024/01/15 data"                                        |
|                                                                   |
|  # Delete pattern (replace with empty)                            |
|  awk '{gsub(/[^0-9]/, ""); print}'                                |
|  Input:  "abc123def456"                                           |
|  Output: "123456"                                                 |
|                                                                   |
|  # Use & for matched text                                         |
|  awk '{gsub(/[0-9]+/, "[&]"); print}'                             |
|  Input:  "item 42 costs 100"                                      |
|  Output: "item [42] costs [100]"                                  |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  DAILY USE CASES:                                                 |
|                                                                   |
|  # Clean whitespace                                               |
|  awk '{gsub(/^[[:space:]]+|[[:space:]]+$/, ""); print}'           |
|                                                                   |
|  # Normalize separators                                           |
|  awk '{gsub(/[[:space:]]+/, " "); print}'                         |
|                                                                   |
|  # Remove comments                                                |
|  awk '{sub(/#.*/, ""); print}'                                    |
|                                                                   |
|  # Escape for CSV                                                 |
|  awk '{gsub(/"/, "\"\""); print "\"" $0 "\""}'                    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**`sub()` vs `gsub()`：**

| 函数 | 替换范围 | 返回值 |
|------|----------|--------|
| `sub()` | 仅第一个匹配 | 替换次数（0或1） |
| `gsub()` | 所有匹配 | 替换次数 |

**特殊用法：**
```bash
# & 代表匹配到的文本
gsub(/[0-9]+/, "[&]")  # 给数字加括号

# 删除模式
gsub(/pattern/, "")    # 用空字符串替换

# 清理空白
gsub(/^[ \t]+|[ \t]+$/, "")  # trim
```

---

### match()

```
+------------------------------------------------------------------+
|                    match()                                        |
+------------------------------------------------------------------+
|                                                                   |
|  SYNTAX:  match(string, regex)                                    |
|           match(string, regex, array)  # gawk: capture groups     |
|                                                                   |
|  RETURNS: Position of match (0 if no match)                       |
|  SETS:    RSTART = start position                                 |
|           RLENGTH = length of match                               |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLES:                                                        |
|  ---------                                                        |
|                                                                   |
|  # Find and extract matched portion                               |
|  awk '{                                                           |
|      if (match($0, /[0-9]+/)) {                                   |
|          num = substr($0, RSTART, RLENGTH)                        |
|          print "Found number:", num                               |
|      }                                                            |
|  }'                                                               |
|  Input:  "Order ID: 12345"                                        |
|  Output: Found number: 12345                                      |
|                                                                   |
|  # Extract email domain                                           |
|  awk '{                                                           |
|      if (match($0, /@[^[:space:]]+/)) {                           |
|          domain = substr($0, RSTART+1, RLENGTH-1)                 |
|          print domain                                             |
|      }                                                            |
|  }'                                                               |
|  Input:  "Contact: user@example.com for info"                     |
|  Output: example.com                                              |
|                                                                   |
|  # Extract with capture groups (gawk)                             |
|  gawk '{                                                          |
|      if (match($0, /([0-9]+)-([0-9]+)-([0-9]+)/, arr)) {          |
|          print "Year:", arr[1]                                    |
|          print "Month:", arr[2]                                   |
|          print "Day:", arr[3]                                     |
|      }                                                            |
|  }'                                                               |
|  Input:  "Date: 2024-01-15"                                       |
|  Output:                                                          |
|  Year: 2024                                                       |
|  Month: 01                                                        |
|  Day: 15                                                          |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  DAILY USE CASE: Extract specific pattern from logs               |
|  awk '{                                                           |
|      if (match($0, /response_time=([0-9.]+)ms/, arr)) {           |
|          times[NR] = arr[1]                                       |
|      }                                                            |
|  }                                                                |
|  END {                                                            |
|      for (i in times) sum += times[i]                             |
|      print "Avg response:", sum/length(times), "ms"               |
|  }' access.log                                                    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**`match()` 的强大之处：**

```bash
# 基本用法
if (match($0, /pattern/)) {
    matched = substr($0, RSTART, RLENGTH)
}

# RSTART: 匹配开始位置
# RLENGTH: 匹配长度

# gawk 捕获组
match($0, /(\d+)-(\d+)/, arr)
# arr[1] = 第一个括号匹配
# arr[2] = 第二个括号匹配
```

**与 `index()` 的区别：**
- `index()` 查找固定字符串
- `match()` 查找正则表达式

---

### sprintf()

```
+------------------------------------------------------------------+
|                    sprintf()                                      |
+------------------------------------------------------------------+
|                                                                   |
|  SYNTAX:  sprintf(format, expr1, expr2, ...)                      |
|                                                                   |
|  RETURNS: Formatted string (does NOT print)                       |
|                                                                   |
|  DIFFERENCE FROM printf:                                          |
|  - printf: prints to output                                       |
|  - sprintf: returns string for further use                        |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLES:                                                        |
|  ---------                                                        |
|                                                                   |
|  # Format number as currency                                      |
|  awk '{                                                           |
|      price = sprintf("$%.2f", $2)                                 |
|      print $1, price                                              |
|  }'                                                               |
|  Input:  "apple 1.5"                                              |
|  Output: "apple $1.50"                                            |
|                                                                   |
|  # Zero-pad numbers                                               |
|  awk '{                                                           |
|      id = sprintf("%05d", NR)                                     |
|      print id, $0                                                 |
|  }'                                                               |
|  Output:                                                          |
|  00001 first line                                                 |
|  00002 second line                                                |
|                                                                   |
|  # Build formatted key                                            |
|  awk '{                                                           |
|      key = sprintf("%s_%s_%04d", $1, $2, $3)                      |
|      data[key] = $4                                               |
|  }'                                                               |
|                                                                   |
|  # Conditional formatting                                         |
|  awk '{                                                           |
|      if ($2 >= 1000)                                              |
|          formatted = sprintf("%.1fK", $2/1000)                    |
|      else                                                         |
|          formatted = sprintf("%d", $2)                            |
|      print $1, formatted                                          |
|  }'                                                               |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  DAILY USE CASE: Generate SQL values                              |
|  awk 'BEGIN {ORS=",\n"}                                           |
|       {                                                           |
|           val = sprintf("('\''%s'\'', %d, %.2f)", $1, $2, $3)     |
|           print val                                               |
|       }                                                           |
|       END {print ""}' data.txt                                    |
|                                                                   |
|  Output:                                                          |
|  ('apple', 100, 1.50),                                            |
|  ('banana', 200, 0.75),                                           |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**`sprintf()` vs `printf()`：**

| 函数 | 作用 |
|------|------|
| `printf()` | 格式化并**打印** |
| `sprintf()` | 格式化并**返回字符串** |

**典型用法：**
```bash
# 存储格式化结果
formatted = sprintf("%.2f", price)

# 构建复合键
key = sprintf("%s_%s", $1, $2)

# 条件格式化
result = sprintf($2>1000 ? "%.1fK" : "%d", $2>1000 ? $2/1000 : $2)
```

---

## 6.2 User-Defined Functions

```
+------------------------------------------------------------------+
|                USER-DEFINED FUNCTIONS                             |
+------------------------------------------------------------------+
|                                                                   |
|  SYNTAX:                                                          |
|  function name(parameters) {                                      |
|      # function body                                              |
|      return value                                                 |
|  }                                                                |
|                                                                   |
|  PLACEMENT: Usually in BEGIN block or before first rule           |
|                                                                   |
|  SCOPE:                                                           |
|  - Parameters are LOCAL                                           |
|  - Other variables are GLOBAL by default                          |
|  - Extra parameters act as local variables (awk idiom)            |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  LOCAL VARIABLE TRICK:                                            |
|  function myfunc(param,    local1, local2) {                      |
|      # local1 and local2 are effectively local                    |
|      # (extra spaces before them is convention)                   |
|  }                                                                |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**自定义函数的语法：**

```awk
function funcname(arg1, arg2,    local1, local2) {
    # arg1, arg2 是参数
    # local1, local2 是局部变量（利用多余参数的特性）
    return result
}
```

**局部变量技巧：**
- AWK 没有真正的局部变量声明
- 多余的函数参数自动成为局部变量
- 惯例是在局部变量前加多个空格以示区分

---

### Function Examples

```
+------------------------------------------------------------------+
|                FUNCTION EXAMPLES                                  |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE 1: Trim whitespace                                       |
|  --------------------------                                       |
|  function trim(s) {                                               |
|      gsub(/^[[:space:]]+|[[:space:]]+$/, "", s)                   |
|      return s                                                     |
|  }                                                                |
|                                                                   |
|  {print trim($1)}                                                 |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE 2: Normalize string                                      |
|  ---------------------------                                      |
|  function normalize(s) {                                          |
|      gsub(/[[:space:]]+/, " ", s)  # Multiple spaces to single    |
|      gsub(/^[[:space:]]+|[[:space:]]+$/, "", s)  # Trim           |
|      return tolower(s)                                            |
|  }                                                                |
|                                                                   |
|  {count[normalize($0)]++}                                         |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE 3: Check if numeric                                      |
|  ---------------------------                                      |
|  function is_numeric(s) {                                         |
|      return s ~ /^-?[0-9]*\.?[0-9]+$/                             |
|  }                                                                |
|                                                                   |
|  is_numeric($3) {print $1, $3 * 2}                                |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE 4: Safe division                                         |
|  ------------------------                                         |
|  function safe_div(num, den,    result) {                         |
|      if (den == 0) return "N/A"                                   |
|      result = num / den                                           |
|      return sprintf("%.2f", result)                               |
|  }                                                                |
|                                                                   |
|  {print $1, safe_div($2, $3)}                                     |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE 5: Format bytes                                          |
|  ------------------------                                         |
|  function human_bytes(bytes,    units, i) {                       |
|      split("B KB MB GB TB", units, " ")                           |
|      i = 1                                                        |
|      while (bytes >= 1024 && i < 5) {                             |
|          bytes /= 1024                                            |
|          i++                                                      |
|      }                                                            |
|      return sprintf("%.1f%s", bytes, units[i])                    |
|  }                                                                |
|                                                                   |
|  {print $1, human_bytes($2)}                                      |
|  Input:  "file.txt 1536000"                                       |
|  Output: "file.txt 1.5MB"                                         |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**实用函数示例：**

1. **trim** - 去除首尾空白
   ```awk
   function trim(s) {
       gsub(/^[ \t]+|[ \t]+$/, "", s)
       return s
   }
   ```

2. **is_numeric** - 判断是否为数字
   ```awk
   function is_numeric(s) {
       return s ~ /^-?[0-9.]+$/
   }
   ```

3. **safe_div** - 安全除法
   ```awk
   function safe_div(a, b) {
       return b == 0 ? "N/A" : a/b
   }
   ```

4. **human_bytes** - 人类可读的字节大小
   ```awk
   function human_bytes(n) {
       # 转换为 KB/MB/GB 格式
   }
   ```

---

### When to Use Functions

```
+------------------------------------------------------------------+
|              WHEN TO USE FUNCTIONS                                |
+------------------------------------------------------------------+
|                                                                   |
|  USE FUNCTIONS WHEN:                                              |
|  +----------------------------------------------------------+    |
|  |  1. Same logic repeated multiple times                    |    |
|  |  2. Complex transformation that deserves a name           |    |
|  |  3. Logic that might change (single point of change)      |    |
|  |  4. Code that's hard to read inline                       |    |
|  |  5. You need local variables                              |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  AVOID FUNCTIONS WHEN:                                            |
|  +----------------------------------------------------------+    |
|  |  1. One-liner is clear enough                             |    |
|  |  2. Logic is used only once                               |    |
|  |  3. You're writing a quick ad-hoc command                 |    |
|  +----------------------------------------------------------+    |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  FUNCTION FILE PATTERN:                                           |
|  ----------------------                                           |
|  # functions.awk                                                  |
|  function trim(s) { ... }                                         |
|  function is_numeric(s) { ... }                                   |
|  function human_bytes(n) { ... }                                  |
|                                                                   |
|  # Usage:                                                         |
|  awk -f functions.awk -f myscript.awk data.txt                    |
|  # or                                                             |
|  awk -f functions.awk '{print trim($1)}' data.txt                 |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**何时使用函数：**

| 场景 | 建议 |
|------|------|
| 重复逻辑 | ✓ 用函数 |
| 复杂转换 | ✓ 用函数命名 |
| 一次性简单操作 | ✗ 直接写 |
| 快速命令行 | ✗ 直接写 |

**函数库模式：**
```bash
# 创建函数库文件
cat > lib.awk << 'EOF'
function trim(s) { gsub(/^[ \t]+|[ \t]+$/, "", s); return s }
EOF

# 使用
awk -f lib.awk '{print trim($1)}' data.txt
```

---

### Complete Script Example

```
+------------------------------------------------------------------+
|              COMPLETE SCRIPT EXAMPLE                              |
+------------------------------------------------------------------+
|                                                                   |
|  TASK: Generate report from sales data                            |
|                                                                   |
|  INPUT (sales.csv):                                               |
|  date,product,quantity,price                                      |
|  2024-01-15,apple,100,1.50                                        |
|  2024-01-15,banana,50,0.75                                        |
|  2024-01-16,apple,80,1.50                                         |
|                                                                   |
+------------------------------------------------------------------+

#!/usr/bin/awk -f

# File: sales_report.awk

function trim(s) {
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", s)
    return s
}

function currency(n) {
    return sprintf("$%.2f", n)
}

function pct(part, whole) {
    if (whole == 0) return "0%"
    return sprintf("%.1f%%", part * 100 / whole)
}

BEGIN {
    FS = ","
    OFS = "\t"
    print "=== Sales Report ==="
    print ""
}

NR == 1 { next }  # Skip header

{
    product = trim($2)
    qty = $3 + 0
    price = $4 + 0
    revenue = qty * price
    
    total_qty += qty
    total_rev += revenue
    prod_qty[product] += qty
    prod_rev[product] += revenue
}

END {
    print "Product Summary:"
    print "----------------"
    printf "%-15s %10s %12s %8s\n", "Product", "Quantity", "Revenue", "Share"
    print ""
    
    for (p in prod_rev) {
        printf "%-15s %10d %12s %8s\n", 
               p, 
               prod_qty[p], 
               currency(prod_rev[p]),
               pct(prod_rev[p], total_rev)
    }
    
    print ""
    print "----------------"
    printf "%-15s %10d %12s\n", "TOTAL", total_qty, currency(total_rev)
}

+------------------------------------------------------------------+

Usage: awk -f sales_report.awk sales.csv

Output:
=== Sales Report ===

Product Summary:
----------------
Product          Quantity      Revenue    Share

apple                 180       $270.00    87.8%
banana                 50        $37.50    12.2%

----------------
TOTAL                 230       $307.50

+------------------------------------------------------------------+
```

**中文解释：**

**完整脚本的结构：**

```awk
#!/usr/bin/awk -f

# 1. 函数定义
function trim(s) { ... }
function currency(n) { ... }

# 2. BEGIN 块：初始化
BEGIN {
    FS = ","
    print "Header"
}

# 3. 主体规则：处理每行
NR == 1 { next }  # 跳过表头
{
    # 累加统计
}

# 4. END 块：输出结果
END {
    # 打印报表
}
```

**运行方式：**
```bash
awk -f script.awk data.csv
# 或
chmod +x script.awk
./script.awk data.csv
```
