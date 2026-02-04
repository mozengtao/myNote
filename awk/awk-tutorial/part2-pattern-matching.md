# Part 2: Pattern Matching (What Triggers Actions)

## 2.1 Pattern Types

```
+------------------------------------------------------------------+
|                    AWK PATTERN TYPES                             |
+------------------------------------------------------------------+
|                                                                  |
|  TYPE              SYNTAX                 WHEN IT MATCHES        |
|  ====              ======                 ===============        |
|                                                                  |
|  No Pattern        {action}               Every line             |
|                                                                  |
|  Regex             /regex/ {action}       Lines matching regex   |
|                                                                  |
|  Expression        expr {action}          Lines where expr != 0  |
|                                                                  |
|  Range             /start/,/end/          Lines from start to    |
|                    {action}               end (inclusive)        |
|                                                                  |
|  BEGIN             BEGIN {action}         Before first line      |
|                                                                  |
|  END               END {action}           After last line        |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

AWK 支持多种模式类型：

| 模式类型 | 示例 | 匹配时机 |
|----------|------|----------|
| 无模式 | `{print}` | 每一行都匹配 |
| 正则模式 | `/error/` | 行内包含 "error" |
| 表达式模式 | `$3 > 100` | 第三字段大于 100 |
| 范围模式 | `/START/,/END/` | 从 START 到 END 之间的行 |
| BEGIN | `BEGIN {...}` | 读取输入前 |
| END | `END {...}` | 读取输入后 |

---

### No Pattern (Always True)

```
+------------------------------------------------------------------+
|                    NO PATTERN                                    |
+------------------------------------------------------------------+
|                                                                  |
|  Syntax:   awk '{action}' file                                   |
|                                                                  |
|  Behavior: Action executes for EVERY line                        |
|                                                                  |
|  Example:                                                        |
|  --------                                                        |
|  awk '{print $1}' file.txt                                       |
|                                                                  |
|  Input:           Output:                                        |
|  hello world      hello                                          |
|  foo bar          foo                                            |
|  one two          one                                            |
|                                                                  |
|  IMPORTANT: No pattern means "always match"                      |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

省略模式时，动作块对每一行都执行。这是最常见的用法之一。

---

### Regex Pattern

```
+------------------------------------------------------------------+
|                    REGEX PATTERN                                 |
+------------------------------------------------------------------+
|                                                                  |
|  Syntax:   awk '/regex/ {action}' file                           |
|                                                                  |
|  Behavior: Action executes when $0 matches regex                 |
|                                                                  |
|  EXAMPLES:                                                       |
|  ---------                                                       |
|                                                                  |
|  # Lines containing "error"                                      |
|  awk '/error/' log.txt                                           |
|                                                                  |
|  # Lines starting with "#"                                       |
|  awk '/^#/' config.txt                                           |
|                                                                  |
|  # Lines ending with number                                      |
|  awk '/[0-9]$/' data.txt                                         |
|                                                                  |
|  # Case insensitive (GNU awk)                                    |
|  awk 'BEGIN{IGNORECASE=1} /error/' log.txt                       |
|                                                                  |
|  REGEX OPERATORS:                                                |
|  +--------+---------------------------+                          |
|  | .      | Any single character      |                          |
|  | *      | Zero or more of previous  |                          |
|  | +      | One or more of previous   |                          |
|  | ?      | Zero or one of previous   |                          |
|  | ^      | Start of line             |                          |
|  | $      | End of line               |                          |
|  | [abc]  | Character class           |                          |
|  | [^abc] | Negated character class   |                          |
|  | (a|b)  | Alternation               |                          |
|  +--------+---------------------------+                          |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

正则模式匹配整行（`$0`）：
- `/error/` —— 行内任意位置包含 "error"
- `/^#/` —— 行以 `#` 开头
- `/[0-9]$/` —— 行以数字结尾

**注意：** 正则匹配的是整行 `$0`，不是某个字段。如果要匹配特定字段，使用 `~` 运算符。

---

### Expression Pattern

```
+------------------------------------------------------------------+
|                 EXPRESSION PATTERN                               |
+------------------------------------------------------------------+
|                                                                  |
|  Syntax:   awk 'expression {action}' file                        |
|                                                                  |
|  Behavior: Action executes when expression evaluates to TRUE     |
|            (non-zero number or non-empty string)                 |
|                                                                  |
|  EXAMPLES:                                                       |
|  ---------                                                       |
|                                                                  |
|  # Numeric comparison                                            |
|  awk '$3 > 100 {print}'             # Field 3 greater than 100   |
|  awk 'NR > 1 {print}'               # Skip first line            |
|  awk 'NF == 5 {print}'              # Lines with exactly 5 fields|
|                                                                  |
|  # String comparison                                             |
|  awk '$1 == "root" {print}'         # First field equals "root"  |
|  awk '$2 != "" {print}'             # Second field not empty     |
|                                                                  |
|  # Regex match on specific field                                 |
|  awk '$1 ~ /^[A-Z]/ {print}'        # Field 1 starts with upper  |
|  awk '$0 !~ /^#/ {print}'           # Lines not starting with #  |
|                                                                  |
|  # Compound conditions                                           |
|  awk '$3 > 100 && $5 < 10 {print}'  # AND                        |
|  awk '$1 == "A" || $1 == "B"'       # OR                         |
|  awk '!($3 > 50) {print}'           # NOT                        |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

表达式模式是最灵活的模式类型：

**数值比较：**
- `$3 > 100` —— 第三字段大于 100
- `NR > 1` —— 跳过第一行
- `NF >= 3` —— 至少有 3 个字段

**字符串比较：**
- `$1 == "root"` —— 精确匹配
- `$2 != ""` —— 非空判断

**正则匹配字段：**
- `$1 ~ /pattern/` —— 字段匹配正则
- `$1 !~ /pattern/` —— 字段不匹配正则

**逻辑组合：**
- `&&` —— 与
- `||` —— 或
- `!` —— 非

---

### Range Pattern

```
+------------------------------------------------------------------+
|                    RANGE PATTERN                                 |
+------------------------------------------------------------------+
|                                                                  |
|  Syntax:   awk '/start/,/end/ {action}' file                     |
|            awk 'NR==5,NR==10 {action}' file                      |
|                                                                  |
|  Behavior: Action executes from first match of start pattern     |
|            to first match of end pattern (INCLUSIVE)             |
|                                                                  |
|  EXAMPLE 1: Regex range                                          |
|  ----------------------                                          |
|  Input:                     awk '/BEGIN/,/END/' file             |
|  line 1                                                          |
|  BEGIN                      Output:                              |
|  line 2                     BEGIN                                |
|  line 3                     line 2                               |
|  END                        line 3                               |
|  line 4                     END                                  |
|                                                                  |
|  EXAMPLE 2: Line number range                                    |
|  ---------------------------                                     |
|  awk 'NR==2,NR==4 {print NR, $0}' file                           |
|                                                                  |
|  Input:        Output:                                           |
|  line 1        2 line 2                                          |
|  line 2        3 line 3                                          |
|  line 3        4 line 4                                          |
|  line 4                                                          |
|  line 5                                                          |
|                                                                  |
|  EXAMPLE 3: Mixed range                                          |
|  ----------------------                                          |
|  awk '/start/,NR==10'       # From "start" to line 10            |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  IMPORTANT: Range resets after end, can match multiple times     |
|                                                                  |
|  Input:                     Output with '/A/,/B/':               |
|  A                          A                                    |
|  x                          x                                    |
|  B                          B                                    |
|  y                          (not printed)                        |
|  A                          A                                    |
|  z                          z                                    |
|  B                          B                                    |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

范围模式匹配从起始模式到结束模式之间的所有行：

**特点：**
1. **包含边界**：起始行和结束行都会被匹配
2. **可重复**：结束后如果再次遇到起始模式，会开始新的范围
3. **灵活组合**：可以混用正则和表达式

**常见用法：**
```bash
# 提取配置文件的某个 section
awk '/\[database\]/,/\[/' config.ini

# 提取特定行范围
awk 'NR==100,NR==200' large_file.txt

# 从某个标记到文件末尾
awk '/START/,0' file    # 0 永远为假，所以到文件末尾
```

---

## 2.2 Conditions and Filtering

### Numeric vs String Comparison

```
+------------------------------------------------------------------+
|           NUMERIC vs STRING COMPARISON                           |
+------------------------------------------------------------------+
|                                                                  |
|  RULE: awk decides comparison type based on context              |
|                                                                  |
|  +------------------+------------------+------------------------+ |
|  | Expression       | Type             | Result                 | |
|  +------------------+------------------+------------------------+ |
|  | "10" < "9"       | STRING           | TRUE  (lexical)        | |
|  | "10" < 9         | NUMERIC          | FALSE (10 > 9)         | |
|  | 10 < "9"         | NUMERIC          | FALSE (10 > 9)         | |
|  | $1 < $2          | DEPENDS on data  | See below              | |
|  +------------------+------------------+------------------------+ |
|                                                                  |
|  FIELD COMPARISON BEHAVIOR:                                      |
|  --------------------------                                      |
|  If BOTH fields look like numbers --> NUMERIC comparison         |
|  If EITHER field is non-numeric   --> STRING comparison          |
|                                                                  |
|  Example:                                                        |
|  Input: "10 9"     $1 < $2  --> FALSE (10 < 9 is false)          |
|  Input: "10 abc"   $1 < $2  --> FALSE ("10" < "abc" is false)    |
|  Input: "abc def"  $1 < $2  --> TRUE  ("abc" < "def" is true)    |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  FORCE NUMERIC: Add 0 to the value                               |
|  awk '$1+0 < $2+0'                                               |
|                                                                  |
|  FORCE STRING: Concatenate with ""                               |
|  awk '$1"" < $2""'                                               |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**AWK 的比较规则：**

1. **两边都是数字或数字字符串** → 数值比较
2. **任一边是非数字字符串** → 字符串比较（字典序）

**常见陷阱：**
```bash
# 这可能不是你想要的！
awk '$1 < $2' file

# 如果数据是 "10 9"，结果是 FALSE（数值比较 10 < 9）
# 如果数据是 "2 10"，结果是 TRUE（数值比较 2 < 10）
# 如果数据是 "a2 a10"，结果是... 字符串比较！"a2" > "a10"
```

**强制类型转换：**
- 强制数值：`$1 + 0` 或 `$1 * 1`
- 强制字符串：`$1 ""` 或 `sprintf("%s", $1)`

---

### Regex Matching with ~ and !~

```
+------------------------------------------------------------------+
|                 REGEX MATCH OPERATORS                            |
+------------------------------------------------------------------+
|                                                                  |
|  OPERATOR    MEANING                                             |
|  ========    =======                                             |
|                                                                  |
|  ~           Matches (left operand contains pattern)             |
|  !~          Does not match                                      |
|                                                                  |
|  SYNTAX:     field ~ /regex/                                     |
|              field !~ /regex/                                    |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLES:                                                       |
|  ---------                                                       |
|                                                                  |
|  # Match $1 against a pattern                                    |
|  awk '$1 ~ /^[0-9]+$/ {print "numeric:", $1}'                    |
|                                                                  |
|  # Lines where $2 does NOT contain "test"                        |
|  awk '$2 !~ /test/'                                              |
|                                                                  |
|  # Match entire line (same as /pattern/)                         |
|  awk '$0 ~ /error/'        # equivalent to: awk '/error/'        |
|                                                                  |
|  # Dynamic pattern (from variable)                               |
|  awk -v pat="error" '$0 ~ pat'                                   |
|                                                                  |
|  # Case insensitive (gawk)                                       |
|  awk 'tolower($1) ~ /error/'                                     |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**`~` 和 `!~` 的用途：**

当需要对**特定字段**进行正则匹配时，使用这两个运算符：

```bash
# /pattern/ 等价于 $0 ~ /pattern/
awk '/error/'              # 整行匹配
awk '$0 ~ /error/'         # 同上

# 匹配特定字段
awk '$3 ~ /^[0-9]+$/'      # 第三字段是纯数字
awk '$1 !~ /^#/'           # 第一字段不以 # 开头
```

**动态正则：**
```bash
# 正则来自变量
awk -v pattern="error" '$0 ~ pattern' log.txt
```

---

### Real Scenarios

```
+------------------------------------------------------------------+
|                 REAL-WORLD FILTERING EXAMPLES                    |
+------------------------------------------------------------------+
|                                                                  |
|  SCENARIO 1: Skip CSV header                                     |
|  ---------------------------                                     |
|  awk 'NR > 1' data.csv                                           |
|  # or                                                            |
|  awk 'FNR > 1' file1.csv file2.csv    # For multiple files       |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  SCENARIO 2: Process only data lines (skip comments/empty)       |
|  ---------------------------------------------------------       |
|  awk '!/^#/ && NF > 0'  config.txt                               |
|  # Lines not starting with # AND having at least one field       |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  SCENARIO 3: Extract specific section of log                     |
|  ------------------------------------------                      |
|  awk '/2024-01-15 10:00/,/2024-01-15 11:00/' app.log             |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  SCENARIO 4: Lines where field matches multiple conditions       |
|  --------------------------------------------------------        |
|  awk '$3 > 100 && $3 < 500 && $5 ~ /active/' data.txt            |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  SCENARIO 5: Process specific file differently in multi-file     |
|  -----------------------------------------------------------     |
|  awk 'FILENAME == "header.txt" {header = $0; next}               |
|       {print header, $0}' header.txt data.txt                    |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  SCENARIO 6: Detect and handle malformed lines                   |
|  ---------------------------------------------                   |
|  awk 'NF != 5 {print "Bad line " NR ": " $0; next}               |
|       {process normally...}'                                     |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**实际场景总结：**

1. **跳过表头**
   - 单文件：`NR > 1`
   - 多文件：`FNR > 1`

2. **跳过注释和空行**
   - `!/^#/ && NF > 0`
   - `!/^[[:space:]]*#/ && !/^[[:space:]]*$/`

3. **提取时间范围**
   - 使用范围模式 `/start/,/end/`

4. **复合条件过滤**
   - 使用 `&&`、`||`、`!` 组合

5. **多文件差异处理**
   - 使用 `FILENAME` 变量判断

6. **格式验证**
   - 使用 `NF` 检查字段数量

---

### Common Pitfalls

```
+------------------------------------------------------------------+
|                    COMMON PITFALLS                               |
+------------------------------------------------------------------+
|                                                                  |
|  PITFALL 1: String "0" is TRUE in boolean context                |
|  ------------------------------------------------                |
|  awk 'BEGIN { if ("0") print "true"; else print "false" }'       |
|  Output: true                                                    |
|                                                                  |
|  WHY: Non-empty string is true. Only "" and 0 are false.         |
|  NOTE: The STRING "0" is NOT the NUMBER 0!                       |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  PITFALL 2: Uninitialized variable is 0 AND ""                   |
|  ---------------------------------------------                   |
|  awk 'BEGIN { print (x == 0); print (x == "") }'                 |
|  Output:                                                         |
|  1                                                               |
|  1                                                               |
|                                                                  |
|  WHY: Uninitialized variables act as 0 in numeric context        |
|       and "" in string context.                                  |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  PITFALL 3: Regex escaping in pattern                            |
|  ------------------------------------                            |
|  WRONG:  awk '/$HOME/' file        # $HOME is awk variable       |
|  RIGHT:  awk '/\/home\/user/' file # Escaped /                   |
|  RIGHT:  awk -v h="$HOME" '$0 ~ h' file  # From shell var        |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  PITFALL 4: Comparing with empty string                          |
|  --------------------------------------                          |
|  awk '$2 == ""'     # Matches if $2 is empty                     |
|  awk '!$2'          # Also matches if $2 is "0"! (0 is false)    |
|                                                                  |
|  SAFE:  awk '$2 == ""'  or  awk 'length($2) == 0'                |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  PITFALL 5: Assignment vs comparison                             |
|  -----------------------------------                             |
|  awk '$1 = "test"'   # ASSIGNS "test" to $1, always true!        |
|  awk '$1 == "test"'  # COMPARES $1 with "test"                   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**需要注意的陷阱：**

1. **字符串 "0" 是真值**
   - 在布尔上下文中，只有空字符串 `""` 和数字 `0` 是假
   - 字符串 `"0"` 是非空字符串，所以是真

2. **未初始化变量的双重身份**
   - 作为数值时是 `0`
   - 作为字符串时是 `""`

3. **正则中的特殊字符**
   - `/` 需要转义为 `\/`
   - Shell 变量 `$HOME` 不会被展开，需要用 `-v` 传入

4. **空字段判断**
   - `!$2` 会把 `"0"` 也当作假
   - 安全做法：`$2 == ""` 或 `length($2) == 0`

5. **赋值 vs 比较**
   - `=` 是赋值，`==` 是比较
   - `$1 = "test"` 会修改 `$1` 并返回 "test"（真值）
