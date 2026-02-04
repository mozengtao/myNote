# Part 3: Core awk Actions You Use Daily

## 3.1 Printing and Formatting

### print vs printf

```
+------------------------------------------------------------------+
|                    print vs printf                               |
+------------------------------------------------------------------+
|                                                                  |
|  COMMAND    BEHAVIOR                                             |
|  =======    ========                                             |
|                                                                  |
|  print      - Simple output                                      |
|             - Automatic newline at end                           |
|             - Items separated by OFS (when using comma)          |
|             - No format control                                  |
|                                                                  |
|  printf     - Formatted output                                   |
|             - NO automatic newline (must add \n)                 |
|             - C-style format specifiers                          |
|             - Full control over alignment, width, precision      |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  print EXAMPLES:                                                 |
|  ---------------                                                 |
|  print                    # Prints $0 (entire line)              |
|  print $1                 # Prints field 1                       |
|  print $1, $2             # Prints field 1, OFS, field 2         |
|  print $1 $2              # Prints field 1 directly joined to 2  |
|  print $1 " - " $2        # Prints with custom separator         |
|  print "text"             # Prints literal string                |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  printf FORMAT SPECIFIERS:                                       |
|  -------------------------                                       |
|  %s     String                                                   |
|  %d     Integer (decimal)                                        |
|  %f     Floating point                                           |
|  %e     Scientific notation                                      |
|  %x     Hexadecimal                                              |
|  %o     Octal                                                    |
|  %%     Literal percent sign                                     |
|                                                                  |
|  WIDTH AND PRECISION:                                            |
|  %10s   Right-align string in 10-char field                      |
|  %-10s  Left-align string in 10-char field                       |
|  %10d   Right-align integer in 10-char field                     |
|  %8.2f  Float: 8 chars total, 2 decimal places                   |
|  %08d   Zero-padded integer in 8-char field                      |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**print 和 printf 的区别：**

| 特性 | print | printf |
|------|-------|--------|
| 自动换行 | 是 | 否（需手动加 `\n`） |
| 格式控制 | 无 | C 风格格式化 |
| 分隔符 | 逗号 → OFS | 格式字符串控制 |
| 复杂度 | 简单 | 灵活 |

**选择原则：**
- 简单输出用 `print`
- 需要对齐、格式化数字时用 `printf`

---

### Formatting Examples

```
+------------------------------------------------------------------+
|                 PRINTF FORMATTING EXAMPLES                       |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 1: Basic column alignment                               |
|  ---------------------------------                               |
|  awk '{printf "%-20s %10d\n", $1, $2}' data.txt                  |
|                                                                  |
|  Input:                  Output:                                 |
|  apple 100               apple                     100           |
|  banana 50               banana                     50           |
|  cherry 200              cherry                    200           |
|                                                                  |
|  NOTE: %-20s = left-align in 20 chars                            |
|        %10d  = right-align number in 10 chars                    |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 2: Floating point precision                             |
|  -----------------------------------                             |
|  awk '{printf "%.2f\n", $1}' numbers.txt                         |
|                                                                  |
|  Input:        Output:                                           |
|  3.14159       3.14                                              |
|  2.71828       2.72                                              |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 3: Zero padding                                         |
|  -----------------------                                         |
|  awk '{printf "%05d\n", $1}'                                     |
|                                                                  |
|  Input:        Output:                                           |
|  42            00042                                             |
|  7             00007                                             |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 4: Table-like output                                    |
|  ---------------------------                                     |
|  awk 'BEGIN {printf "%-15s %8s %8s\n", "NAME", "SIZE", "COUNT"}  |
|       {printf "%-15s %8d %8d\n", $1, $2, $3}'                    |
|                                                                  |
|  Output:                                                         |
|  NAME               SIZE    COUNT                                |
|  fileA.txt           1024       10                               |
|  fileB.txt           2048       25                               |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 5: Percentage formatting                                |
|  --------------------------------                                |
|  awk '{printf "%s: %.1f%%\n", $1, $2}' stats.txt                 |
|                                                                  |
|  Input:            Output:                                       |
|  CPU 75.5          CPU: 75.5%                                    |
|  Memory 82.3       Memory: 82.3%                                 |
|                                                                  |
|  NOTE: Use %% to print literal % sign                            |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**格式化技巧：**

1. **左对齐**：`%-20s`（负号表示左对齐）
2. **右对齐**：`%20s`（默认右对齐）
3. **小数精度**：`%.2f`（保留两位小数）
4. **零填充**：`%05d`（不足 5 位用 0 填充）
5. **百分号**：`%%`（打印字面 % 符号）

**表格输出模式：**
```bash
awk 'BEGIN {
    printf "%-15s %8s %8s\n", "NAME", "SIZE", "COUNT"
    printf "%-15s %8s %8s\n", "----", "----", "-----"
}
{
    printf "%-15s %8d %8d\n", $1, $2, $3
}'
```

---

### When printf is Worth Using

```
+------------------------------------------------------------------+
|              WHEN TO USE printf vs print                         |
+------------------------------------------------------------------+
|                                                                  |
|  USE print WHEN:                                                 |
|  ---------------                                                 |
|  - Simple field extraction                                       |
|  - Output will be piped to another tool                          |
|  - Format doesn't matter                                         |
|  - Quick one-liners                                              |
|                                                                  |
|  Examples:                                                       |
|  awk '{print $1, $3}'                                            |
|  awk '{print $1}' | sort | uniq                                  |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  USE printf WHEN:                                                |
|  ----------------                                                |
|  - Creating human-readable reports                               |
|  - Column alignment matters                                      |
|  - Number formatting needed (decimals, padding)                  |
|  - Building fixed-format output files                            |
|  - Need inline formatting without newlines                       |
|                                                                  |
|  Examples:                                                       |
|  awk '{printf "%-20s %10.2f\n", $1, $2}'                         |
|  awk '{printf "%s", $1}' # No newline - building a line          |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**使用场景决策：**

| 场景 | 推荐 |
|------|------|
| 快速提取字段 | `print` |
| 管道传递数据 | `print` |
| 生成报表 | `printf` |
| 列对齐 | `printf` |
| 数字格式化 | `printf` |
| 构建不换行的输出 | `printf`（不加 `\n`） |

---

## 3.2 Modifying Fields

### Reassigning Fields

```
+------------------------------------------------------------------+
|                 FIELD MODIFICATION                               |
+------------------------------------------------------------------+
|                                                                  |
|  CORE CONCEPT:                                                   |
|  When you modify any field ($1, $2, etc.), awk REBUILDS $0       |
|  using the current OFS between fields.                           |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 1: Modify a field value                                 |
|  -------------------------------                                 |
|  Input: "apple 100 red"                                          |
|                                                                  |
|  awk '{$2 = $2 * 2; print}'                                      |
|  Output: "apple 200 red"                                         |
|                                                                  |
|  NOTE: $0 is rebuilt with default OFS (single space)             |
|        Original whitespace is NOT preserved!                     |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 2: What happens to original spacing                     |
|  ------------------------------------------                      |
|  Input: "apple    100    red"    (multiple spaces)               |
|                                                                  |
|  awk '{$2 = $2 * 2; print}'                                      |
|  Output: "apple 200 red"         (single spaces!)                |
|                                                                  |
|  WHY: Once you touch any field, $0 is rebuilt with OFS           |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 3: Using custom OFS                                     |
|  --------------------------                                      |
|  awk 'BEGIN{OFS=","} {$2 = $2 * 2; print}'                       |
|                                                                  |
|  Input:  "apple 100 red"                                         |
|  Output: "apple,200,red"                                         |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 4: Force rebuild without changing value                 |
|  -----------------------------------------------                 |
|  awk 'BEGIN{OFS="\t"} {$1=$1; print}'                            |
|                                                                  |
|  Input:  "a b c"                                                 |
|  Output: "a       b       c"   (tab-separated)                   |
|                                                                  |
|  WHY: $1=$1 triggers rebuild even though value is same           |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**字段修改的核心机制：**

当你修改任何字段（`$1`、`$2` 等），awk 会：
1. 使用新值替换该字段
2. 用 OFS 重新拼接所有字段为 `$0`
3. 原有的字段间空白会丢失

**常见用途：**
```bash
# 数值计算
awk '{$3 = $3 * 1024; print}'        # 字节转 KB

# 字符串处理
awk '{$1 = toupper($1); print}'      # 首字段大写

# 格式转换
awk 'BEGIN{OFS=","} {$1=$1; print}'  # 空格分隔 → 逗号分隔
```

---

### Dropping and Inserting Columns

```
+------------------------------------------------------------------+
|              DROPPING AND INSERTING FIELDS                       |
+------------------------------------------------------------------+
|                                                                  |
|  DROP FIRST FIELD:                                               |
|  -----------------                                               |
|  awk '{$1=""; print}'                   # Leaves leading OFS     |
|  awk '{$1=""; print substr($0,2)}'      # Clean (removes space)  |
|  awk '{for(i=2;i<=NF;i++) printf "%s ", $i; print ""}'  # Loop   |
|                                                                  |
|  Input:  "drop keep1 keep2"                                      |
|  Output: "keep1 keep2"                                           |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  DROP LAST FIELD:                                                |
|  ----------------                                                |
|  awk '{NF--; print}'        # Decrement NF, rebuild $0           |
|  awk '{$NF=""; print}'      # Set last field to empty            |
|                                                                  |
|  Input:  "keep1 keep2 drop"                                      |
|  Output: "keep1 keep2"                                           |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  DROP SPECIFIC FIELD (e.g., field 3):                            |
|  ------------------------------------                            |
|  awk '{for(i=1;i<=NF;i++) if(i!=3) printf "%s ", $i; print ""}'  |
|                                                                  |
|  Input:  "a b c d"                                               |
|  Output: "a b d"                                                 |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  INSERT FIELD AT BEGINNING:                                      |
|  --------------------------                                      |
|  awk '{print "new", $0}'                # Simple prepend         |
|  awk 'BEGIN{OFS=","} {print "new", $0}' # With OFS               |
|                                                                  |
|  Input:  "a b c"                                                 |
|  Output: "new a b c"                                             |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  INSERT FIELD AT END:                                            |
|  --------------------                                            |
|  awk '{print $0, "new"}'                                         |
|  awk '{$(NF+1)="new"; print}'           # Extend NF              |
|                                                                  |
|  Input:  "a b c"                                                 |
|  Output: "a b c new"                                             |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  INSERT FIELD IN MIDDLE:                                         |
|  -----------------------                                         |
|  # Insert after field 2                                          |
|  awk '{$2 = $2 " new"; print}'          # Append to field 2      |
|                                                                  |
|  # Proper insertion (shift fields)                               |
|  awk 'BEGIN{OFS=" "} {                                           |
|      for(i=NF;i>2;i--) $(i+1)=$i        # Shift right            |
|      $3="new"                           # Insert at position 3   |
|      print                                                       |
|  }'                                                              |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**字段操作技巧：**

| 操作 | 方法 | 注意事项 |
|------|------|----------|
| 删除第一列 | `$1=""; print` | 会留下前导空格 |
| 删除最后一列 | `NF--; print` | 直接减少字段数 |
| 删除指定列 | 循环跳过 | 需要重建输出 |
| 在开头插入 | `print "new", $0` | 逗号产生 OFS |
| 在末尾插入 | `print $0, "new"` | 或 `$(NF+1)="new"` |

**清理删除第一列后的空格：**
```bash
# 方法 1：substr
awk '{$1=""; print substr($0,2)}'

# 方法 2：sub
awk '{$1=""; sub(/^ /, ""); print}'

# 方法 3：循环重建
awk '{for(i=2;i<=NF;i++) printf "%s%s", $i, (i<NF?" ":"\n")}'
```

---

### Why Field Modification Works This Way

```
+------------------------------------------------------------------+
|           INTERNAL MECHANISM OF FIELD MODIFICATION               |
+------------------------------------------------------------------+
|                                                                  |
|  STEP 1: Read line into $0                                       |
|  +----------------------------------------+                      |
|  | $0 = "apple    100    red"             |                      |
|  +----------------------------------------+                      |
|                                                                  |
|  STEP 2: Split $0 into fields using FS                           |
|  +--------+-------+-------+                                      |
|  | $1     | $2    | $3    |                                      |
|  | apple  | 100   | red   |                                      |
|  +--------+-------+-------+                                      |
|  NF = 3                                                          |
|                                                                  |
|  STEP 3: You modify a field                                      |
|  $2 = 200                                                        |
|  +--------+-------+-------+                                      |
|  | $1     | $2    | $3    |                                      |
|  | apple  | 200   | red   |  <-- $2 changed                      |
|  +--------+-------+-------+                                      |
|                                                                  |
|  STEP 4: $0 is REBUILT using OFS                                 |
|  +----------------------------------------+                      |
|  | $0 = $1 OFS $2 OFS $3                  |                      |
|  | $0 = "apple" " " "200" " " "red"       |                      |
|  | $0 = "apple 200 red"                   |                      |
|  +----------------------------------------+                      |
|                                                                  |
|  IMPORTANT: Original whitespace is LOST after any field change   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**内部工作原理：**

1. **读取行**：整行存入 `$0`
2. **分割字段**：用 FS 分割，结果存入 `$1, $2, ... $NF`
3. **修改字段**：改变某个 `$i` 的值
4. **重建 $0**：awk 自动用 OFS 重新拼接所有字段

**关键理解：**
- 原始的多余空白在分割时就已经丢失
- `$0` 和字段是双向同步的
- 修改 `$0` 会触发重新分割
- 修改任何 `$i` 会触发重新拼接 `$0`

**保留原始格式的技巧：**
```bash
# 如果必须保留原始格式，直接操作 $0
awk '{gsub(/old/, "new"); print}'  # 替换不会触发字段重建
```
