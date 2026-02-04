# Part 8: Tool Choice Guide & Checklist

## Tool Comparison

```
+------------------------------------------------------------------+
|                   TOOL COMPARISON MATRIX                         |
+------------------------------------------------------------------+
|                                                                  |
|  TASK                          | grep | sed | cut | awk | Python |
|  ==============================|======|=====|=====|=====|========|
|  Find lines matching pattern   |  ++  |  +  |     |  +  |   +    |
|  Simple text replacement       |      |  ++ |     |  +  |   +    |
|  Extract specific columns      |      |     |  +  |  ++ |   +    |
|  Field-based filtering         |      |     |     |  ++ |   +    |
|  Numeric calculations          |      |     |     |  ++ |   ++   |
|  Aggregations (sum, count)     |      |     |     |  ++ |   ++   |
|  Associative arrays            |      |     |     |  ++ |   ++   |
|  Complex multi-file processing |      |  +  |     |  +  |   ++   |
|  Binary data handling          |      |     |     |     |   ++   |
|  HTTP/API calls                |      |     |     |     |   ++   |
|                                                                  |
|  ++ = Best choice    + = Capable    (blank) = Not suitable       |
+------------------------------------------------------------------+
```

**中文解释：**

**工具选择速查表：**

| 任务 | 最佳工具 |
|------|----------|
| 查找包含模式的行 | grep |
| 简单文本替换 | sed |
| 提取固定位置列 | cut |
| 字段过滤/计算 | awk |
| 复杂数据处理 | Python |

---

## grep vs awk

```
+------------------------------------------------------------------+
|                     grep vs awk                                   |
+------------------------------------------------------------------+
|                                                                   |
|  USE grep WHEN:                                                   |
|  - Just finding lines that match a pattern                        |
|  - Speed is critical (grep is faster for simple match)            |
|  - Need grep-specific features (-r, -l, -c, --color)              |
|                                                                   |
|  EXAMPLES WHERE grep WINS:                                        |
|  grep "error" log.txt              # Find error lines             |
|  grep -c "error" log.txt           # Count matches                |
|  grep -r "TODO" src/               # Recursive search             |
|  grep -l "pattern" *.txt           # List matching files          |
|                                                                   |
|  USE awk WHEN:                                                    |
|  - Need to extract/manipulate specific fields                     |
|  - Need to perform calculations on matches                        |
|  - Need conditional logic beyond simple match                     |
|                                                                   |
|  EXAMPLES WHERE awk WINS:                                         |
|  awk '/error/ {print $1, $3}'      # Fields from matching lines   |
|  awk '$3 > 100 && /error/'         # Combined conditions          |
|  awk '/error/ {sum+=$5} END{print sum}'    # Sum from matches     |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**grep vs awk 选择：**

| 场景 | 选择 | 原因 |
|------|------|------|
| 简单模式搜索 | grep | 更快、更简洁 |
| 递归搜索文件 | grep -r | 专门功能 |
| 提取匹配行的字段 | awk | 字段处理能力 |
| 对匹配行计算 | awk | 计算能力 |

---

## sed vs awk

```
+------------------------------------------------------------------+
|                      sed vs awk                                   |
+------------------------------------------------------------------+
|                                                                   |
|  USE sed WHEN:                                                    |
|  - Simple search and replace                                      |
|  - In-place file editing (-i)                                     |
|  - Line-based transformations                                     |
|                                                                   |
|  EXAMPLES WHERE sed WINS:                                         |
|  sed 's/old/new/g' file            # Global replace               |
|  sed -i 's/foo/bar/g' file         # In-place edit                |
|  sed '/pattern/d' file             # Delete matching lines        |
|                                                                   |
|  USE awk WHEN:                                                    |
|  - Need field-aware replacement                                   |
|  - Conditional replacement based on field values                  |
|                                                                   |
|  EXAMPLES WHERE awk WINS:                                         |
|  awk '{$3 = "new"; print}'         # Replace only in column 3     |
|  awk '$4 > 100 {$5 = "HIGH"} {print}'  # Conditional replacement  |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

| 场景 | 选择 | 原因 |
|------|------|------|
| 简单全局替换 | sed | 简洁、可原地编辑 |
| 字段内替换 | awk | 不影响其他列 |
| 条件替换 | awk | 灵活的条件逻辑 |

---

## cut vs awk

```
+------------------------------------------------------------------+
|                      cut vs awk                                   |
+------------------------------------------------------------------+
|                                                                   |
|  USE cut WHEN:                                                    |
|  - Extract fixed position columns                                 |
|  - Simple delimiter-based extraction                              |
|                                                                   |
|  EXAMPLES WHERE cut WINS:                                         |
|  cut -d: -f1 /etc/passwd           # First field                  |
|  cut -d: -f1,3 /etc/passwd         # Fields 1 and 3               |
|                                                                   |
|  USE awk WHEN:                                                    |
|  - Delimiter might vary (multiple spaces)                         |
|  - Need to reference last field ($NF)                             |
|                                                                   |
|  EXAMPLES WHERE awk WINS:                                         |
|  ps aux | awk '{print $1}'         # Handles multiple spaces      |
|  awk -F/ '{print $NF}'             # Always last field            |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

| 场景 | 选择 | 原因 |
|------|------|------|
| 简单固定分隔符提取 | cut | 更快、更简洁 |
| 多空格分隔 | awk | cut 无法处理 |
| 取最后一个字段 | awk | $NF 动态获取 |

---

## When awk is NOT the Right Tool

```
+------------------------------------------------------------------+
|            WHEN AWK IS NOT THE RIGHT TOOL                         |
+------------------------------------------------------------------+
|                                                                   |
|  1. Binary file processing --> Use xxd, od, or Python             |
|  2. JSON/XML/YAML parsing  --> Use jq, xmllint, yq                |
|  3. HTTP requests/API calls --> Use curl + jq, or Python          |
|  4. Complex data structures --> Use Python                        |
|  5. Interactive programs --> Use Python, Bash with read           |
|  6. Very large aggregations --> Use sort + uniq, databases        |
|                                                                   |
|  RULE: If awk script grows beyond 50 lines, consider Python       |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

| 任务 | 更好的选择 |
|------|-----------|
| JSON/XML 解析 | jq, xmllint |
| HTTP API 调用 | curl + jq, Python |
| 二进制文件 | xxd, Python |
| 复杂数据结构 | Python |

---

## awk vs Python

```
+------------------------------------------------------------------+
|                    awk vs Python                                  |
+------------------------------------------------------------------+
|                                                                   |
|  USE awk WHEN:                                                    |
|  - Quick ad-hoc text processing                                   |
|  - Pipeline integration                                           |
|  - Log file analysis                                              |
|  - Field extraction and basic aggregation                         |
|                                                                   |
|  USE Python WHEN:                                                 |
|  - Complex business logic                                         |
|  - Error handling matters                                         |
|  - Need to interface with APIs/databases                          |
|  - Team needs to maintain the code                                |
|                                                                   |
|  HYBRID APPROACH:                                                 |
|  awk -F, '{print $1, $3}' huge.csv | python process.py            |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

| 维度 | awk 占优 | Python 占优 |
|------|---------|-------------|
| 启动速度 | Y | |
| 管道集成 | Y | |
| 复杂逻辑 | | Y |
| 可维护性 | | Y |

---

## Daily Use Checklist

### Before Writing awk
- [ ] Do I need field extraction? --> If no, maybe grep/sed
- [ ] Do I need calculations? --> If yes, awk is good
- [ ] Is this JSON/XML? --> Use jq/xmllint instead
- [ ] Will this grow complex? --> Consider Python

### Essential Syntax
```bash
awk 'pattern {action}'               # Basic structure
awk -F: '{...}'                      # Set field separator
awk -v var=val '{...}'               # Pass variable
awk 'BEGIN {...} {...} END {...}'    # Full structure
```

### Common Patterns (Copy-Paste Ready)

```bash
# Print specific columns
awk '{print $1, $3}'

# Filter by condition
awk '$3 > 100'

# Sum a column
awk '{sum+=$3} END {print sum}'

# Count occurrences
awk '{count[$1]++} END {for(k in count) print k, count[k]}'

# Skip header
awk 'NR > 1'
awk 'FNR > 1'                 # For multiple files

# Deduplicate
awk '!seen[$0]++'

# Find unique values count
awk '{a[$1]++} END {print length(a)}'

# Average
awk '{sum+=$1; n++} END {print sum/n}'

# Min/Max
awk 'NR==1||$1>max{max=$1} END{print max}'
awk 'NR==1||$1<min{min=$1} END{print min}'

# Change delimiter
awk 'BEGIN{FS=","; OFS="\t"} {$1=$1; print}'

# Top N by count
awk '{c[$1]++} END {for(k in c) print c[k],k}' | sort -rn | head
```

### Field Separator Quick Reference
```
-F:              Colon
-F,              Comma
-F'\t'           Tab
-F'[ \t]+'       One or more spaces/tabs
-F'[][]'         Square brackets
```

### Variables to Remember
```
$0   Entire line              NF   Number of fields
$1   First field              NR   Line number (global)
$NF  Last field               FNR  Line number (per file)
FS   Input separator          OFS  Output separator
```

### Debugging Quick Commands
```bash
# See how line is split
awk '{print "NF=" NF; for(i=1;i<=NF;i++) print i": "$i}'

# Check for hidden characters
awk '{print "[" $0 "]"}'

# Test with small input
head -10 file | awk 'your script'
```

### Common Mistakes to Avoid
- [ ] Setting FS after BEGIN (use -F or BEGIN{FS=...})
- [ ] Using NR instead of FNR for multi-file
- [ ] Using = instead of == for comparison
- [ ] Not handling empty input (check count before divide)
- [ ] Assuming for-in order (pipe to sort if order matters)

### Performance Tips
- [ ] Use exit early when possible
- [ ] Use next to skip lines instead of if-else
- [ ] Let awk read files (vs cat file | awk)

---

## Final Summary

**AWK Mental Model:**
```
INPUT  -->  [PATTERN] --> [ACTION]  -->  OUTPUT
(lines)     (when?)       (what?)       (results)
```

**Core Concepts:**
1. Records (lines) and Fields (columns)
2. Pattern-Action pairs
3. BEGIN/END blocks
4. Associative arrays
5. Built-in variables (NR, NF, FS, etc.)

**When to Use awk:**
- Log analysis
- Config file parsing
- Field extraction and transformation
- Quick data aggregation
- Pipeline glue

**When NOT to Use awk:**
- Binary data
- Structured formats (JSON/XML) - use jq/xmllint
- Complex multi-step processing - use Python
- Need external libraries/APIs

**Progression Path:**
1. Master field extraction: `awk '{print $1}'`
2. Add filtering: `awk '$3 > 100 {print $1}'`
3. Learn aggregation: `awk '{sum+=$3} END {print sum}'`
4. Use arrays: `awk '{c[$1]++} END {for(k in c) print k,c[k]}'`
5. Write scripts for complex tasks

---

**"awk is a domain-specific language for field-oriented text processing."**

**Think in patterns and fields. Let the data flow through. Keep it simple, compose with other tools.**
