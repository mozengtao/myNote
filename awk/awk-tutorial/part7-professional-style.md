# Part 7: Writing awk Like a Professional

## 7.1 One-Liner vs Script

```
+------------------------------------------------------------------+
|                ONE-LINER vs SCRIPT                                |
+------------------------------------------------------------------+
|                                                                   |
|  ONE-LINER (command line):                                        |
|  +----------------------------------------------------------+    |
|  |  awk '{sum+=$3} END {print sum}' data.txt                 |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  SCRIPT FILE (process_data.awk):                                  |
|  +----------------------------------------------------------+    |
|  |  #!/usr/bin/awk -f                                        |    |
|  |                                                           |    |
|  |  BEGIN {                                                  |    |
|  |      FS = ","                                             |    |
|  |      print "Processing data..."                           |    |
|  |  }                                                        |    |
|  |                                                           |    |
|  |  NR == 1 { next }                                         |    |
|  |                                                           |    |
|  |  {                                                        |    |
|  |      sum += $3                                            |    |
|  |      count++                                              |    |
|  |  }                                                        |    |
|  |                                                           |    |
|  |  END {                                                    |    |
|  |      printf "Total: %.2f\n", sum                          |    |
|  |      printf "Average: %.2f\n", sum/count                  |    |
|  |  }                                                        |    |
|  +----------------------------------------------------------+    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**一行式 vs 脚本文件：**

| 形式 | 适用场景 |
|------|----------|
| 一行式 | 快速探索、简单提取、管道操作 |
| 脚本文件 | 复杂逻辑、需要复用、多人协作 |

---

### When One-Liner is Enough

```
+------------------------------------------------------------------+
|             WHEN ONE-LINER IS ENOUGH                              |
+------------------------------------------------------------------+
|                                                                   |
|  GOOD FOR ONE-LINERS:                                             |
|  +----------------------------------------------------------+    |
|  |  - Simple field extraction                                |    |
|  |  - Basic filtering                                        |    |
|  |  - Quick sums/counts                                      |    |
|  |  - Single transformation                                  |    |
|  |  - Pipeline glue between commands                         |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  EXAMPLES:                                                        |
|                                                                   |
|  # Extract and count - perfect one-liner                          |
|  awk '{print $1}' | sort | uniq -c                                |
|                                                                   |
|  # Quick sum - perfect one-liner                                  |
|  awk '{sum+=$3} END {print sum}'                                  |
|                                                                   |
|  # Simple filter - perfect one-liner                              |
|  awk '$3 > 100'                                                   |
|                                                                   |
|  # Field transformation - OK as one-liner                         |
|  awk -F, 'BEGIN{OFS=","} {$2=$2*1.1; print}'                      |
|                                                                   |
|  RULE OF THUMB:                                                   |
|  If it fits comfortably on one line and you can understand        |
|  it at a glance, keep it as a one-liner.                          |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**一行式的黄金法则：**
- 一眼能看懂
- 不需要多个条件分支
- 不需要复杂的状态管理
- 不需要复用

**典型一行式场景：**
```bash
# 提取字段
awk '{print $1}'

# 简单过滤
awk '$3 > 100'

# 快速统计
awk '{sum+=$1} END {print sum}'

# 格式转换
awk -F, '{print $1 "\t" $2}'
```

---

### When to Switch to Script

```
+------------------------------------------------------------------+
|             WHEN TO SWITCH TO SCRIPT                              |
+------------------------------------------------------------------+
|                                                                   |
|  SWITCH TO SCRIPT WHEN:                                           |
|  +----------------------------------------------------------+    |
|  |  1. Multiple pattern-action blocks                        |    |
|  |  2. Complex conditional logic (nested if/else)            |    |
|  |  3. User-defined functions needed                         |    |
|  |  4. Will be reused or shared                              |    |
|  |  5. Needs documentation/comments                          |    |
|  |  6. One-liner exceeds ~80 characters                      |    |
|  |  7. Debugging is needed                                   |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  WARNING SIGNS (one-liner getting too complex):                   |
|  +----------------------------------------------------------+    |
|  |  - Escaping quotes becomes painful                        |    |
|  |  - Multiple semicolons in sequence                        |    |
|  |  - Can't tell what it does without careful reading        |    |
|  |  - You're copying it to multiple places                   |    |
|  +----------------------------------------------------------+    |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  SCRIPT FILE BENEFITS:                                            |
|  - Easier to read and maintain                                    |
|  - Can add comments                                               |
|  - Version control friendly                                       |
|  - Easier to debug                                                |
|  - Can be tested independently                                    |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE: Too complex for one-liner                               |
|                                                                   |
|  BAD (hard to read, error-prone):                                 |
|  awk 'BEGIN{FS=",";OFS="\t"}/^#/{next}NR==1{for(i=1;i<=NF;i++)    |
|  h[i]=$i;next}{for(i=1;i<=NF;i++)if($i~/error/)c[h[i]]++}END{     |
|  for(k in c)print k,c[k]}' file.csv                               |
|                                                                   |
|  GOOD (as script file):                                           |
|  See next section for proper formatting                           |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**转为脚本的信号：**

| 信号 | 说明 |
|------|------|
| 超过 80 字符 | 可读性下降 |
| 多个分号 | 逻辑复杂 |
| 引号嵌套 | 容易出错 |
| 需要注释 | 逻辑不直观 |
| 要复用 | 值得文件化 |

**从一行式到脚本的演进：**
```bash
# 开始：简单一行式
awk '{sum+=$3} END {print sum}'

# 需求增加：变复杂
awk 'BEGIN{FS=","} NR>1{sum+=$3;count++} END {printf "Sum: %.2f\nAvg: %.2f\n",sum,sum/count}'

# 此时应该转为脚本文件
```

---

## 7.2 Readability Rules

```
+------------------------------------------------------------------+
|                  READABILITY RULES                                |
+------------------------------------------------------------------+
|                                                                   |
|  RULE 1: Clear Variable Names                                     |
|  +----------------------------------------------------------+    |
|  |  BAD:   {a+=$3; b++}                                      |    |
|  |  GOOD:  {total_sales += $3; line_count++}                 |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  RULE 2: One Responsibility Per Block                             |
|  +----------------------------------------------------------+    |
|  |  BAD:                                                     |    |
|  |  {                                                        |    |
|  |      if ($3 > 100) { high++; high_sum += $3 }             |    |
|  |      else { low++; low_sum += $3 }                        |    |
|  |      total += $3; count++                                 |    |
|  |  }                                                        |    |
|  |                                                           |    |
|  |  GOOD:                                                    |    |
|  |  # Categorize                                             |    |
|  |  $3 > 100 { high++; high_sum += $3 }                      |    |
|  |  $3 <= 100 { low++; low_sum += $3 }                       |    |
|  |                                                           |    |
|  |  # Always count                                           |    |
|  |  { total += $3; count++ }                                 |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  RULE 3: Consistent Formatting                                    |
|  +----------------------------------------------------------+    |
|  |  - Indent with spaces (4 is common)                       |    |
|  |  - Braces on same line or consistently on new line        |    |
|  |  - Blank lines between logical sections                   |    |
|  |  - Comments for non-obvious logic                         |    |
|  +----------------------------------------------------------+    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**可读性原则：**

1. **变量命名清晰**
   ```awk
   # 差
   {a+=$3; b++}
   
   # 好
   {total_sales += $3; order_count++}
   ```

2. **每个块做一件事**
   ```awk
   # 分类统计
   $3 > 100 { high_count++ }
   $3 <= 100 { low_count++ }
   
   # 总计（独立块）
   { total += $3 }
   ```

3. **格式一致**
   - 统一缩进（4 空格）
   - 大括号风格一致
   - 逻辑段落间空行

---

### Script Formatting Example

```
+------------------------------------------------------------------+
|               SCRIPT FORMATTING EXAMPLE                           |
+------------------------------------------------------------------+

#!/usr/bin/awk -f
#
# analyze_logs.awk - Analyze application log files
#
# Usage: awk -f analyze_logs.awk app.log
#

# --- Configuration ---
BEGIN {
    # Set field separator for log format: timestamp [LEVEL] component: message
    FS = "[][]"
    
    # Initialize counters
    total_lines = 0
    error_count = 0
    warn_count = 0
}

# --- Skip empty lines ---
/^[[:space:]]*$/ { next }

# --- Count by level ---
{
    total_lines++
    level = $2
    
    if (level == "ERROR") {
        error_count++
        # Store error details for later reporting
        errors[NR] = $0
    }
    else if (level == "WARN") {
        warn_count++
    }
}

# --- Generate report ---
END {
    print "==================================="
    print "       Log Analysis Report"
    print "==================================="
    print ""
    printf "Total lines processed: %d\n", total_lines
    printf "Errors: %d (%.1f%%)\n", error_count, pct(error_count, total_lines)
    printf "Warnings: %d (%.1f%%)\n", warn_count, pct(warn_count, total_lines)
    
    if (error_count > 0) {
        print ""
        print "--- Error Details ---"
        for (line_num in errors) {
            print errors[line_num]
        }
    }
}

# --- Helper function ---
function pct(part, whole) {
    return (whole > 0) ? (part * 100 / whole) : 0
}

+------------------------------------------------------------------+
```

**中文解释：**

**脚本结构最佳实践：**

```awk
#!/usr/bin/awk -f
#
# 文件头注释：描述用途、用法
#

# --- 初始化 ---
BEGIN {
    # 设置和初始化
}

# --- 跳过规则 ---
/pattern/ { next }

# --- 主处理逻辑 ---
{
    # 核心处理
}

# --- 特殊条件处理 ---
condition { ... }

# --- 输出结果 ---
END {
    # 汇总报告
}

# --- 辅助函数 ---
function helper(...) { ... }
```

---

### Avoid Clever Tricks

```
+------------------------------------------------------------------+
|                AVOID CLEVER TRICKS                                |
+------------------------------------------------------------------+
|                                                                   |
|  CLEVER BUT FRAGILE:                                              |
|  +----------------------------------------------------------+    |
|  |                                                           |    |
|  |  # Cryptic deduplication                                  |    |
|  |  !a[$0]++                                                 |    |
|  |                                                           |    |
|  |  # Better (self-documenting):                             |    |
|  |  !seen[$0]++ { print }                                    |    |
|  |  # or even:                                               |    |
|  |  {                                                        |    |
|  |      if (!seen[$0]) {                                     |    |
|  |          print                                            |    |
|  |          seen[$0] = 1                                     |    |
|  |      }                                                    |    |
|  |  }                                                        |    |
|  |                                                           |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  TRICKS TO AVOID:                                                 |
|  +----------------------------------------------------------+    |
|  |                                                           |    |
|  |  # Assignment in pattern (confusing)                      |    |
|  |  (x=$3) > 100                                             |    |
|  |                                                           |    |
|  |  # Better:                                                |    |
|  |  {x = $3} x > 100                                         |    |
|  |  # or:                                                    |    |
|  |  $3 > 100 { x = $3; ... }                                 |    |
|  |                                                           |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  PRINCIPLES:                                                      |
|  +----------------------------------------------------------+    |
|  |  1. Clarity over cleverness                               |    |
|  |  2. Future you will thank present you                     |    |
|  |  3. If you need a comment to explain, maybe rewrite       |    |
|  |  4. One-liners can be clever; scripts should be clear     |    |
|  +----------------------------------------------------------+    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**避免过于"聪明"的写法：**

| 聪明写法 | 清晰写法 |
|----------|----------|
| `!a[$0]++` | `!seen[$0]++ {print}` |
| `(x=$3)>100` | `$3>100 {x=$3; ...}` |
| `x?x:y` | `if(x) x; else y` |

**原则：**
- 代码是给人读的，其次才是给机器执行
- 一行式可以耍小聪明，脚本要清晰
- 如果需要注释解释，考虑重写

---

## 7.3 Debugging awk

```
+------------------------------------------------------------------+
|                   DEBUGGING AWK                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TECHNIQUE 1: Print intermediate values                           |
|  +----------------------------------------------------------+    |
|  |                                                           |    |
|  |  # Add debug prints                                       |    |
|  |  {                                                        |    |
|  |      print "DEBUG: NR=" NR " $0=" $0 > "/dev/stderr"      |    |
|  |      print "DEBUG: $3=" $3 " NF=" NF > "/dev/stderr"      |    |
|  |      # ... rest of processing                             |    |
|  |  }                                                        |    |
|  |                                                           |    |
|  |  # Output to stderr so it doesn't mix with results        |    |
|  |                                                           |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  TECHNIQUE 2: Use NR, FNR, NF for context                         |
|  +----------------------------------------------------------+    |
|  |                                                           |    |
|  |  # Print line context with output                         |    |
|  |  {                                                        |    |
|  |      printf "[%d:%d] ", NR, NF                            |    |
|  |      print $0                                             |    |
|  |  }                                                        |    |
|  |                                                           |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  TECHNIQUE 3: Reduce input for testing                            |
|  +----------------------------------------------------------+    |
|  |                                                           |    |
|  |  # Test with first few lines                              |    |
|  |  head -20 large_file.txt | awk 'your_script'              |    |
|  |                                                           |    |
|  |  # Or use NR in awk                                       |    |
|  |  awk 'NR <= 20 {your_logic} NR > 20 {exit}'               |    |
|  |                                                           |    |
|  +----------------------------------------------------------+    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**调试技术：**

1. **打印中间值**
   ```awk
   {
       print "DEBUG: $3=" $3 > "/dev/stderr"
       # 正常处理...
   }
   ```

2. **显示上下文**
   ```awk
   {print "[行" NR ", " NF "列]", $0}
   ```

3. **减少测试数据**
   ```bash
   head -10 bigfile | awk 'script'
   ```

---

### Common Debugging Scenarios

```
+------------------------------------------------------------------+
|             COMMON DEBUGGING SCENARIOS                            |
+------------------------------------------------------------------+
|                                                                   |
|  SCENARIO 1: Fields not splitting correctly                       |
|  ------------------------------------------                       |
|  SYMPTOM: $1 contains entire line                                 |
|                                                                   |
|  DEBUG:                                                           |
|  awk '{print "NF=" NF; for(i=1;i<=NF;i++) print i": ["$i"]"}'     |
|                                                                   |
|  COMMON CAUSES:                                                   |
|  - Wrong FS (check: is it comma? tab? multiple spaces?)           |
|  - FS set after first line processed (use BEGIN or -F)            |
|  - File has Windows line endings (\r\n)                           |
|                                                                   |
|  FIX for Windows line endings:                                    |
|  awk '{gsub(/\r$/, ""); ...}'                                     |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  SCENARIO 2: Numbers not comparing correctly                      |
|  -------------------------------------------                      |
|  SYMPTOM: $3 > 100 not matching expected lines                    |
|                                                                   |
|  DEBUG:                                                           |
|  awk '{print "[$3]=" $3 " type=" ($3+0==$3?"num":"str")}'         |
|                                                                   |
|  COMMON CAUSES:                                                   |
|  - Hidden whitespace in field                                     |
|  - String comparison instead of numeric                           |
|  - Field has non-numeric characters                               |
|                                                                   |
|  FIX:                                                             |
|  awk '{gsub(/[^0-9.]/, "", $3); if ($3+0 > 100) print}'           |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  SCENARIO 3: Pattern not matching                                 |
|  --------------------------------                                 |
|  SYMPTOM: /pattern/ doesn't match expected lines                  |
|                                                                   |
|  DEBUG:                                                           |
|  awk '{print "[" $0 "]"}'  # Check for hidden characters          |
|  awk '{for(i=1;i<=length($0);i++) printf "%d ", ord(substr($0,i,1))}' |
|                                                                   |
|  COMMON CAUSES:                                                   |
|  - Case sensitivity (use tolower() or IGNORECASE)                 |
|  - Hidden characters (tabs vs spaces, \r, etc.)                   |
|  - Regex escaping issues                                          |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  SCENARIO 4: END block shows wrong totals                         |
|  ----------------------------------------                         |
|  SYMPTOM: Sum/count is 0 or wrong in END                          |
|                                                                   |
|  DEBUG:                                                           |
|  awk '{sum += $3; print "After line " NR ": sum=" sum}'           |
|                                                                   |
|  COMMON CAUSES:                                                   |
|  - Variable scoping issue (typo in variable name)                 |
|  - Numeric conversion issue ($3 is not a number)                  |
|  - next/exit called before accumulation                           |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**常见问题排查：**

| 问题 | 调试方法 | 常见原因 |
|------|----------|----------|
| 字段没分开 | 打印 NF 和各字段 | FS 设置错误、Windows 换行符 |
| 数值比较异常 | 打印字段和类型 | 字段有空格、非数字字符 |
| 模式不匹配 | 打印行内容检查 | 大小写、隐藏字符 |
| END 结果错误 | 逐行打印累计值 | 变量名拼写、转换失败 |

**Windows 换行符处理：**
```bash
awk '{gsub(/\r$/, ""); ...}'
```

---

### Debug-Friendly Script Template

```
+------------------------------------------------------------------+
|           DEBUG-FRIENDLY SCRIPT TEMPLATE                          |
+------------------------------------------------------------------+

#!/usr/bin/awk -f
#
# Template with debug support
#
# Usage: awk -v DEBUG=1 -f script.awk file   # Enable debug
#        awk -f script.awk file               # Normal run
#

function debug(msg) {
    if (DEBUG) print "DEBUG: " msg > "/dev/stderr"
}

function debug_fields(    i) {
    if (DEBUG) {
        printf "DEBUG: NR=%d NF=%d\n", NR, NF > "/dev/stderr"
        for (i = 1; i <= NF; i++)
            printf "DEBUG:   $%d = [%s]\n", i, $i > "/dev/stderr"
    }
}

BEGIN {
    debug("Starting processing")
    FS = ","
}

{
    debug("Processing line " NR)
    debug_fields()
    
    # Your processing logic here
    sum += $3
}

END {
    debug("Processing complete")
    debug("Total lines: " NR)
    print "Sum:", sum
}

+------------------------------------------------------------------+

# Usage examples:

# Normal run
awk -f script.awk data.csv

# Debug mode
awk -v DEBUG=1 -f script.awk data.csv

# Debug with limited input
head -5 data.csv | awk -v DEBUG=1 -f script.awk

+------------------------------------------------------------------+
```

**中文解释：**

**调试友好的脚本模板：**

```awk
# 调试函数
function debug(msg) {
    if (DEBUG) print "DEBUG: " msg > "/dev/stderr"
}

# 使用
{
    debug("处理第 " NR " 行")
    # 正常逻辑...
}
```

**运行方式：**
```bash
# 正常运行
awk -f script.awk data.csv

# 调试模式
awk -v DEBUG=1 -f script.awk data.csv

# 调试少量数据
head -5 data.csv | awk -v DEBUG=1 -f script.awk
```

---

### Gawk Debugger

```
+------------------------------------------------------------------+
|                   GAWK DEBUGGER                                   |
+------------------------------------------------------------------+
|                                                                   |
|  GNU awk has a built-in debugger (like gdb):                      |
|                                                                   |
|  gawk --debug -f script.awk data.txt                              |
|                                                                   |
|  BASIC COMMANDS:                                                  |
|  +----------------------------------------------------------+    |
|  | Command     | Description                                 |    |
|  +-------------+---------------------------------------------+    |
|  | b N         | Set breakpoint at line N                    |    |
|  | r           | Run the program                             |    |
|  | n           | Next line (step over)                       |    |
|  | s           | Step into function                          |    |
|  | c           | Continue to next breakpoint                 |    |
|  | p var       | Print variable value                        |    |
|  | p $0        | Print current record                        |    |
|  | p NR        | Print record number                         |    |
|  | bt          | Print backtrace                             |    |
|  | q           | Quit debugger                               |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  EXAMPLE SESSION:                                                 |
|  $ gawk --debug -f script.awk data.txt                            |
|  gawk> b 10           # Break at line 10                          |
|  gawk> r              # Run                                       |
|  Breakpoint 1 at line 10                                          |
|  gawk> p $0           # Print current line                        |
|  gawk> p sum          # Print sum variable                        |
|  gawk> n              # Next line                                 |
|  gawk> c              # Continue                                  |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**gawk 调试器使用：**

```bash
# 启动调试器
gawk --debug -f script.awk data.txt

# 常用命令
b 10      # 在第 10 行设断点
r         # 运行
n         # 下一行
p $3      # 打印 $3 的值
c         # 继续执行
q         # 退出
```

**适用场景：**
- 复杂脚本调试
- 难以定位的逻辑错误
- 学习 awk 执行流程
