# Part 4: awk as a Lightweight Programming Language

## 4.1 Variables and Types

```
+------------------------------------------------------------------+
|                    AWK VARIABLE SYSTEM                            |
+------------------------------------------------------------------+
|                                                                   |
|  CORE PRINCIPLE: AWK variables are UNTYPED (dynamically typed)    |
|                                                                   |
|  +----------------------------------------------------------+    |
|  |  The SAME variable can be:                                |    |
|  |  - A number in numeric context                            |    |
|  |  - A string in string context                             |    |
|  |  - AWK converts automatically based on usage              |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  INITIALIZATION:                                                  |
|  ---------------                                                  |
|  - Uninitialized variables are "" (empty string) AND 0            |
|  - No declaration needed                                          |
|  - First use creates the variable                                 |
|                                                                   |
|  SCOPE:                                                           |
|  ------                                                           |
|  - All variables are GLOBAL by default                            |
|  - Function parameters and local vars are exceptions              |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**AWK 变量系统的特点：**

1. **无类型声明**：变量不需要声明类型，直接使用
2. **自动类型转换**：根据上下文自动在数字和字符串之间转换
3. **默认值**：未初始化的变量既是空字符串 `""` 又是数字 `0`
4. **全局作用域**：除函数参数外，所有变量默认全局

---

### Type Conversion Rules

```
+------------------------------------------------------------------+
|                 TYPE CONVERSION RULES                             |
+------------------------------------------------------------------+
|                                                                   |
|  STRING --> NUMBER:                                               |
|  ------------------                                               |
|  "123"      --> 123        (numeric prefix extracted)             |
|  "123abc"   --> 123        (numeric prefix extracted)             |
|  "abc123"   --> 0          (no numeric prefix)                    |
|  ""         --> 0          (empty string)                         |
|  "  42  "   --> 42         (leading/trailing space ignored)       |
|                                                                   |
|  NUMBER --> STRING:                                               |
|  ------------------                                               |
|  123        --> "123"                                             |
|  3.14159    --> "3.14159"  (OFMT controls format, default %.6g)   |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  CONVERSION TRIGGERS:                                             |
|  --------------------                                             |
|  +----------------------+--------------------+                    |
|  | Operation            | Context            |                    |
|  +----------------------+--------------------+                    |
|  | + - * / % ^          | Numeric            |                    |
|  | < > <= >=            | Depends on operands|                    |
|  | == !=                | Depends on operands|                    |
|  | string concatenation | String             |                    |
|  | array subscript      | String             |                    |
|  +----------------------+--------------------+                    |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLES:                                                        |
|  ---------                                                        |
|  x = "42"                                                         |
|  y = x + 0        # y = 42 (number)                               |
|  z = x ""         # z = "42" (string, forced)                     |
|                                                                   |
|  a = "10"                                                         |
|  b = "9"                                                          |
|  print (a < b)    # 0 (FALSE - numeric comparison: 10 < 9)        |
|  print (a"" < b"")# 1 (TRUE - string comparison: "10" < "9")      |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**类型转换规则：**

| 转换方向 | 规则 | 示例 |
|----------|------|------|
| 字符串→数字 | 提取前导数字部分 | `"123abc"` → `123` |
| 字符串→数字 | 无数字前缀则为 0 | `"abc"` → `0` |
| 数字→字符串 | 按 OFMT 格式化 | `3.14` → `"3.14"` |

**强制类型转换技巧：**
```bash
# 强制数值运算
x = "42"
y = x + 0       # y 是数字 42

# 强制字符串
z = x ""        # z 是字符串 "42"
```

---

### Practical Variable Usage

```
+------------------------------------------------------------------+
|              DAILY VARIABLE USAGE PATTERNS                        |
+------------------------------------------------------------------+
|                                                                   |
|  PATTERN 1: Running Sum                                           |
|  -----------------------                                          |
|  awk '{sum += $3} END {print sum}'                                |
|                                                                   |
|  How it works:                                                    |
|  - sum starts as 0 (uninitialized)                                |
|  - += adds $3 (converted to number) to sum                        |
|  - END prints final total                                         |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  PATTERN 2: Running Count                                         |
|  ------------------------                                         |
|  awk '/error/ {count++} END {print count+0}'                      |
|                                                                   |
|  Why count+0?                                                     |
|  - If no matches, count is uninitialized (prints "")              |
|  - count+0 ensures 0 is printed instead of blank                  |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  PATTERN 3: Tracking Min/Max                                      |
|  ---------------------------                                      |
|  awk 'NR==1 {min=max=$1}                                          |
|       $1<min {min=$1}                                             |
|       $1>max {max=$1}                                             |
|       END {print "min:", min, "max:", max}'                       |
|                                                                   |
|  Why initialize on NR==1?                                         |
|  - Avoids picking arbitrary initial values                        |
|  - Works for any data range (negative, positive, etc.)            |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  PATTERN 4: String Accumulation                                   |
|  ------------------------------                                   |
|  awk '{result = result $1 ","}                                    |
|       END {print substr(result, 1, length(result)-1)}'            |
|                                                                   |
|  What happens:                                                    |
|  - result starts as ""                                            |
|  - Each $1 is concatenated with comma                             |
|  - END removes trailing comma                                     |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  PATTERN 5: External Variables                                    |
|  -----------------------------                                    |
|  awk -v threshold=100 '$3 > threshold'                            |
|  awk -v name="$USER" '$1 == name'                                 |
|                                                                   |
|  The -v option:                                                   |
|  - Sets awk variable BEFORE BEGIN block runs                      |
|  - Properly handles shell variable expansion                      |
|  - Safer than embedding shell vars in awk code                    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**常用变量模式：**

1. **累加求和**
   ```bash
   awk '{sum += $3} END {print sum}' file
   ```
   - `sum` 自动初始化为 0
   - `+=` 进行数值累加

2. **计数统计**
   ```bash
   awk '/error/ {count++} END {print count+0}'
   ```
   - `count+0` 确保无匹配时输出 0 而不是空行

3. **最大最小值**
   ```bash
   awk 'NR==1 {min=max=$1} $1<min{min=$1} $1>max{max=$1} END{print min,max}'
   ```
   - 用第一个值初始化，避免假设数据范围

4. **外部变量传入**
   ```bash
   awk -v limit="$LIMIT" '$3 > limit'
   ```
   - `-v` 选项安全传递 shell 变量

---

## 4.2 Control Flow

```
+------------------------------------------------------------------+
|                    AWK CONTROL FLOW                               |
+------------------------------------------------------------------+
|                                                                   |
|  STATEMENT         SYNTAX                                         |
|  =========         ======                                         |
|                                                                   |
|  if-else           if (condition) { ... }                         |
|                    else if (condition) { ... }                    |
|                    else { ... }                                   |
|                                                                   |
|  while             while (condition) { ... }                      |
|                                                                   |
|  do-while          do { ... } while (condition)                   |
|                                                                   |
|  for               for (init; condition; increment) { ... }       |
|                                                                   |
|  for-in            for (key in array) { ... }                     |
|                                                                   |
|  break             Exit innermost loop                            |
|                                                                   |
|  continue          Skip to next iteration                         |
|                                                                   |
|  next              Skip to next input record                      |
|                                                                   |
|  exit              End program (run END block)                    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

AWK 支持完整的 C 风格控制流语句：

| 语句 | 用途 |
|------|------|
| `if/else` | 条件分支 |
| `while` | 条件循环 |
| `for` | 计数循环 |
| `for-in` | 遍历数组 |
| `break` | 跳出循环 |
| `continue` | 跳过本次迭代 |
| `next` | 跳过当前行，读下一行 |
| `exit` | 结束程序（会执行 END 块） |

---

### if-else Examples

```
+------------------------------------------------------------------+
|                    IF-ELSE EXAMPLES                               |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE 1: Simple condition                                      |
|  ---------------------------                                      |
|  awk '{                                                           |
|      if ($3 > 100)                                                |
|          print $1, "HIGH"                                         |
|      else                                                         |
|          print $1, "LOW"                                          |
|  }'                                                               |
|                                                                   |
|  EXAMPLE 2: Multiple conditions                                   |
|  ------------------------------                                   |
|  awk '{                                                           |
|      if ($3 > 100)                                                |
|          grade = "A"                                              |
|      else if ($3 > 70)                                            |
|          grade = "B"                                              |
|      else if ($3 > 50)                                            |
|          grade = "C"                                              |
|      else                                                         |
|          grade = "F"                                              |
|      print $1, grade                                              |
|  }'                                                               |
|                                                                   |
|  EXAMPLE 3: Ternary operator (one-liner friendly)                 |
|  ------------------------------------------------                 |
|  awk '{print $1, ($3 > 100 ? "HIGH" : "LOW")}'                    |
|                                                                   |
|  EXAMPLE 4: Pattern-action vs if inside action                    |
|  ---------------------------------------------                    |
|  # These are equivalent:                                          |
|  awk '$3 > 100 {print "HIGH"}'      # Pattern-action style        |
|  awk '{if ($3 > 100) print "HIGH"}' # If inside action            |
|                                                                   |
|  PREFERENCE: Use pattern-action for simple filters                |
|              Use if-else for complex logic                        |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**if-else 使用场景：**

1. **简单条件** → 用模式-动作更清晰
   ```bash
   awk '$3 > 100 {print "HIGH"}'
   ```

2. **多分支判断** → 用 if-else
   ```bash
   awk '{if($3>100) g="A"; else if($3>70) g="B"; else g="C"; print g}'
   ```

3. **三元运算符** → 一行内简洁表达
   ```bash
   awk '{print ($3>100 ? "HIGH" : "LOW")}'
   ```

---

### Loop Examples

```
+------------------------------------------------------------------+
|                    LOOP EXAMPLES                                  |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE 1: for loop - iterate over fields                        |
|  -----------------------------------------                        |
|  awk '{                                                           |
|      for (i = 1; i <= NF; i++)                                    |
|          print "Field", i, "=", $i                                |
|  }'                                                               |
|                                                                   |
|  Input: "a b c"                                                   |
|  Output:                                                          |
|  Field 1 = a                                                      |
|  Field 2 = b                                                      |
|  Field 3 = c                                                      |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE 2: while loop - read until condition                     |
|  --------------------------------------------                     |
|  awk '{                                                           |
|      i = 1                                                        |
|      while (i <= NF && $i != "STOP") {                            |
|          print $i                                                 |
|          i++                                                      |
|      }                                                            |
|  }'                                                               |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE 3: for-in loop - iterate over array                      |
|  -------------------------------------------                      |
|  awk '{count[$1]++}                                               |
|       END {                                                       |
|           for (key in count)                                      |
|               print key, count[key]                               |
|       }'                                                          |
|                                                                   |
|  IMPORTANT: for-in order is UNDEFINED (not insertion order!)      |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE 4: Find maximum with for loop                            |
|  -------------------------------------                            |
|  awk '{                                                           |
|      if (NR == 1 || $1 > max) max = $1                            |
|  } END {print "Max:", max}'                                       |
|                                                                   |
|  OR equivalently:                                                 |
|                                                                   |
|  awk 'BEGIN {max = -999999999}                                    |
|       {if ($1 > max) max = $1}                                    |
|       END {print "Max:", max}'                                    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**循环使用场景：**

| 循环类型 | 适用场景 |
|----------|----------|
| `for (i=1; i<=NF; i++)` | 遍历所有字段 |
| `while (condition)` | 条件控制的迭代 |
| `for (k in array)` | 遍历关联数组键 |

**注意事项：**
- `for-in` 遍历顺序是**不确定的**
- 如需排序输出，需配合外部 `sort` 或 gawk 的 `asorti()`

---

### next and exit

```
+------------------------------------------------------------------+
|                    next AND exit                                  |
+------------------------------------------------------------------+
|                                                                   |
|  next: Skip remaining actions, read next line                     |
|  ===========================================                      |
|                                                                   |
|  EXAMPLE: Skip comment lines early                                |
|  awk '/^#/ {next}                                                 |
|       {                                                           |
|           # Complex processing here                               |
|           # Only runs for non-comment lines                       |
|           print "Processing:", $0                                 |
|       }'                                                          |
|                                                                   |
|  WHY USE next:                                                    |
|  - More efficient than wrapping code in if-else                   |
|  - Makes code flow clearer (guard clause pattern)                 |
|  - Especially useful with multiple pattern-action blocks          |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE: Process first N lines only                              |
|  awk 'NR > 100 {exit}                                             |
|       {print}'                                                    |
|                                                                   |
|  This is MORE EFFICIENT than:                                     |
|  awk 'NR <= 100 {print}'    # Still reads entire file!            |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  exit: Stop processing, run END block                             |
|  ====================================                             |
|                                                                   |
|  EXAMPLE: Find first match and stop                               |
|  awk '/ERROR/ {print; exit}'                                      |
|                                                                   |
|  EXAMPLE: Exit with status code                                   |
|  awk '/FATAL/ {exit 1}  END {exit 0}'                             |
|                                                                   |
|  BEHAVIOR:                                                        |
|  - exit INSIDE pattern-action: END block still runs               |
|  - exit INSIDE END block: Terminates immediately                  |
|  - exit code can be retrieved with $? in shell                    |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  PATTERN: Efficient early termination                             |
|  ------------------------------------                             |
|  awk 'NR == 1 {header = $0; next}                                 |
|       /ERROR/ {                                                   |
|           print header                                            |
|           print $0                                                |
|           exit                                                    |
|       }'                                                          |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**`next` 的作用：**
- 跳过当前行的剩余处理，立即读取下一行
- 类似于循环中的 `continue`
- 用于提前过滤掉不需要处理的行

**`exit` 的作用：**
- 停止读取输入，跳转到 END 块
- 在 END 块中使用 `exit` 会立即终止程序
- 可以指定退出码：`exit 1`

**效率优势：**
```bash
# 高效：遇到条件立即停止
awk 'NR > 100 {exit} {print}'

# 低效：仍然读完整个文件
awk 'NR <= 100'
```

---

## 4.3 Associative Arrays (Critical for Daily Work)

```
+------------------------------------------------------------------+
|                 ASSOCIATIVE ARRAYS                                |
+------------------------------------------------------------------+
|                                                                   |
|  CONCEPT: Arrays indexed by STRING keys (like hash/dict/map)      |
|                                                                   |
|  +----------------------------------------------------------+    |
|  |                                                          |    |
|  |   count["apple"] = 5                                     |    |
|  |   count["banana"] = 3                                    |    |
|  |   count["cherry"] = 7                                    |    |
|  |                                                          |    |
|  |   +--------+-------+                                     |    |
|  |   |  KEY   | VALUE |                                     |    |
|  |   +--------+-------+                                     |    |
|  |   | apple  |   5   |                                     |    |
|  |   | banana |   3   |                                     |    |
|  |   | cherry |   7   |                                     |    |
|  |   +--------+-------+                                     |    |
|  |                                                          |    |
|  +----------------------------------------------------------+    |
|                                                                   |
|  KEY PROPERTIES:                                                  |
|  ---------------                                                  |
|  - Keys are always STRINGS (numbers converted to string)          |
|  - Order is UNDEFINED (don't rely on insertion order)             |
|  - Access creates entry if not exists (value = 0/"")              |
|  - No declaration needed                                          |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**关联数组是 AWK 最强大的特性之一：**

1. **键值存储**：类似于其他语言的 HashMap/Dictionary
2. **字符串键**：所有键都是字符串（数字会自动转换）
3. **自动创建**：访问不存在的键会自动创建（值为 0 或 ""）
4. **无序性**：遍历顺序不确定

---

### Array Operations

```
+------------------------------------------------------------------+
|                 ARRAY OPERATIONS                                  |
+------------------------------------------------------------------+
|                                                                   |
|  CREATE/UPDATE:                                                   |
|  arr["key"] = value                                               |
|  arr["key"]++                   # Increment (starts at 0)         |
|  arr["key"] += 10               # Add to existing                 |
|                                                                   |
|  READ:                                                            |
|  x = arr["key"]                 # Get value                       |
|                                                                   |
|  CHECK EXISTENCE:                                                 |
|  if ("key" in arr) { ... }     # True if key exists              |
|                                                                   |
|  DELETE:                                                          |
|  delete arr["key"]              # Remove single entry             |
|  delete arr                     # Clear entire array (gawk)       |
|                                                                   |
|  ITERATE:                                                         |
|  for (key in arr) {                                               |
|      print key, arr[key]                                          |
|  }                                                                |
|                                                                   |
|  LENGTH (gawk):                                                   |
|  len = length(arr)              # Number of elements              |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  MULTI-DIMENSIONAL ARRAYS:                                        |
|  -------------------------                                        |
|  AWK simulates multi-dim with SUBSEP (default: "\034")            |
|                                                                   |
|  arr[1,2] = "value"             # Actually arr["1\0342"]          |
|  arr["x","y"] = "value"         # Actually arr["x\034y"]          |
|                                                                   |
|  # Check multi-dimensional existence:                             |
|  if ((i,j) in arr) { ... }                                        |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**数组操作速查：**

| 操作 | 语法 | 说明 |
|------|------|------|
| 赋值 | `arr["k"] = v` | 创建或更新 |
| 递增 | `arr["k"]++` | 自动从 0 开始 |
| 读取 | `x = arr["k"]` | 不存在则为 0/"" |
| 检查存在 | `if ("k" in arr)` | 不会创建条目 |
| 删除 | `delete arr["k"]` | 删除单个 |
| 遍历 | `for (k in arr)` | 顺序不确定 |

**注意：** 直接访问 `arr["k"]` 会创建该键（如果不存在）。用 `"k" in arr` 检查才不会创建。

---

### Counting Occurrences (Most Common Use)

```
+------------------------------------------------------------------+
|               COUNTING PATTERN                                    |
+------------------------------------------------------------------+
|                                                                   |
|  BASIC PATTERN:                                                   |
|  awk '{count[$1]++} END {for (k in count) print k, count[k]}'     |
|                                                                   |
|  EXECUTION TRACE:                                                 |
|  ----------------                                                 |
|  Input:          Processing:                Output:               |
|  apple           count["apple"]++           apple 3               |
|  banana          count["banana"]++          banana 2              |
|  apple           count["apple"]++           cherry 1              |
|  cherry          count["cherry"]++                                |
|  banana          count["banana"]++                                |
|  apple           count["apple"]++                                 |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  REAL EXAMPLES:                                                   |
|  --------------                                                   |
|                                                                   |
|  # Count HTTP status codes                                        |
|  awk '{count[$9]++} END {for(c in count) print c, count[c]}'      |
|      access.log                                                   |
|                                                                   |
|  # Count unique IPs                                               |
|  awk '{count[$1]++} END {print length(count)}' access.log         |
|                                                                   |
|  # Count processes per user                                       |
|  ps aux | awk 'NR>1 {count[$1]++} END {for(u in count)            |
|      print u, count[u]}'                                          |
|                                                                   |
|  # Count file extensions                                          |
|  ls -1 | awk -F. '{if(NF>1) count[$NF]++}                         |
|      END {for(e in count) print e, count[e]}'                     |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**计数模式是关联数组最常用的场景：**

```bash
# 基本模式
awk '{count[$字段]++} END {for (k in count) print k, count[k]}'
```

**实际应用：**

1. **统计日志中的状态码分布**
   ```bash
   awk '{c[$9]++} END {for(k in c) print k,c[k]}' access.log
   ```

2. **统计每个用户的进程数**
   ```bash
   ps aux | awk 'NR>1{c[$1]++} END {for(u in c) print u,c[u]}'
   ```

3. **统计唯一值数量**
   ```bash
   awk '{seen[$1]++} END {print length(seen)}'
   ```

---

### Grouping by Column

```
+------------------------------------------------------------------+
|                 GROUPING PATTERN                                  |
+------------------------------------------------------------------+
|                                                                   |
|  PATTERN: Group data by key, aggregate values                     |
|                                                                   |
|  EXAMPLE: Sum sales by product                                    |
|  -----------------------------                                    |
|  Input (product, quantity):                                       |
|  apple 10                                                         |
|  banana 5                                                         |
|  apple 7                                                          |
|  banana 3                                                         |
|  apple 8                                                          |
|                                                                   |
|  awk '{sum[$1] += $2}                                             |
|       END {for (p in sum) print p, sum[p]}'                       |
|                                                                   |
|  Output:                                                          |
|  apple 25                                                         |
|  banana 8                                                         |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  MULTIPLE AGGREGATIONS:                                           |
|  ----------------------                                           |
|  awk '{                                                           |
|      sum[$1] += $2                                                |
|      count[$1]++                                                  |
|      if (!($1 in max) || $2 > max[$1]) max[$1] = $2               |
|  }                                                                |
|  END {                                                            |
|      for (k in sum)                                               |
|          printf "%s: sum=%d, count=%d, avg=%.2f, max=%d\n",       |
|                 k, sum[k], count[k], sum[k]/count[k], max[k]      |
|  }'                                                               |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  COMPOSITE KEYS:                                                  |
|  ---------------                                                  |
|  # Group by multiple columns (date + type)                        |
|  awk '{key = $1 SUBSEP $2; sum[key] += $3}                        |
|       END {for (k in sum) {                                       |
|           split(k, parts, SUBSEP)                                 |
|           print parts[1], parts[2], sum[k]                        |
|       }}'                                                         |
|                                                                   |
|  # Simpler: concatenate with delimiter                            |
|  awk '{sum[$1":"$2] += $3}                                        |
|       END {for (k in sum) print k, sum[k]}'                       |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**分组聚合模式：**

```bash
# 按第一列分组，对第二列求和
awk '{sum[$1] += $2} END {for (k in sum) print k, sum[k]}'
```

**多重聚合：**
```bash
awk '{
    sum[$1] += $2
    count[$1]++
}
END {
    for (k in sum)
        printf "%s: total=%d, avg=%.2f\n", k, sum[k], sum[k]/count[k]
}'
```

**复合键分组：**
```bash
# 按 日期+类型 分组
awk '{sum[$1":"$2] += $3} END {for(k in sum) print k, sum[k]}'
```

---

### Deduplication

```
+------------------------------------------------------------------+
|               DEDUPLICATION PATTERN                               |
+------------------------------------------------------------------+
|                                                                   |
|  BASIC: Print unique lines (first occurrence)                     |
|  awk '!seen[$0]++' file                                           |
|                                                                   |
|  HOW IT WORKS:                                                    |
|  +------------------------------------------------------------+  |
|  | Step | $0      | seen[$0] | !seen[$0] | Action | seen[$0]++ | |
|  +------+---------+----------+-----------+--------+------------+ |
|  | 1    | "apple" | 0 (undef)| TRUE (1)  | PRINT  | 1          | |
|  | 2    | "banana"| 0 (undef)| TRUE (1)  | PRINT  | 1          | |
|  | 3    | "apple" | 1        | FALSE (0) | skip   | 2          | |
|  | 4    | "cherry"| 0 (undef)| TRUE (1)  | PRINT  | 1          | |
|  | 5    | "banana"| 1        | FALSE (0) | skip   | 2          | |
|  +------------------------------------------------------------+  |
|                                                                   |
|  EXPLANATION:                                                     |
|  - !seen[$0] is TRUE when $0 is NOT yet seen                      |
|  - ++ happens AFTER the test (post-increment)                     |
|  - No explicit print needed (pattern-action with no action =      |
|    {print})                                                       |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  VARIATIONS:                                                      |
|  -----------                                                      |
|                                                                   |
|  # Unique based on specific field                                 |
|  awk '!seen[$1]++' file                                           |
|                                                                   |
|  # Unique based on multiple fields                                |
|  awk '!seen[$1,$2]++' file                                        |
|                                                                   |
|  # Print last occurrence instead of first                         |
|  awk '{data[$1]=$0} END {for (k in data) print data[k]}' file     |
|                                                                   |
|  # Count and then filter                                          |
|  awk '{count[$0]++}                                               |
|       END {for (line in count) if (count[line] == 1) print line}'|
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**去重的经典技巧：**

```bash
awk '!seen[$0]++'
```

**原理解析：**
1. `seen[$0]` 首次访问时为 0（假）
2. `!seen[$0]` 为真，作为模式触发默认动作 `{print}`
3. `++` 后置递增，将 `seen[$0]` 设为 1
4. 再次遇到相同行时，`seen[$0]` 已为 1，取反后为假，不打印

**变体应用：**
```bash
# 按第一列去重
awk '!seen[$1]++'

# 保留最后一次出现
awk '{last[$1]=$0} END {for(k in last) print last[k]}'

# 只保留出现一次的行
awk '{c[$0]++} END {for(l in c) if(c[l]==1) print l}'
```

---

### Sorted Output from Arrays

```
+------------------------------------------------------------------+
|                 SORTED OUTPUT                                     |
+------------------------------------------------------------------+
|                                                                   |
|  PROBLEM: for-in order is undefined                               |
|                                                                   |
|  SOLUTION 1: Pipe to sort                                         |
|  awk '{count[$1]++} END {for (k in count) print k, count[k]}'     |
|      | sort                                                       |
|                                                                   |
|  SOLUTION 2: Sort by count (descending)                           |
|  awk '{count[$1]++} END {for (k in count) print count[k], k}'     |
|      | sort -rn | awk '{print $2, $1}'                            |
|                                                                   |
|  SOLUTION 3: gawk built-in sorting                                |
|  gawk 'BEGIN {PROCINFO["sorted_in"] = "@val_num_desc"}            |
|        {count[$1]++}                                              |
|        END {for (k in count) print k, count[k]}'                  |
|                                                                   |
|  GAWK SORT OPTIONS:                                               |
|  @ind_str_asc   - Keys, string ascending                          |
|  @ind_str_desc  - Keys, string descending                         |
|  @ind_num_asc   - Keys, numeric ascending                         |
|  @ind_num_desc  - Keys, numeric descending                        |
|  @val_str_asc   - Values, string ascending                        |
|  @val_str_desc  - Values, string descending                       |
|  @val_num_asc   - Values, numeric ascending                       |
|  @val_num_desc  - Values, numeric descending                      |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**排序输出方案：**

1. **管道 sort（通用）**
   ```bash
   awk '{c[$1]++} END {for(k in c) print k,c[k]}' | sort
   ```

2. **按值排序（通用）**
   ```bash
   awk '{c[$1]++} END {for(k in c) print c[k],k}' | sort -rn
   ```

3. **gawk 内置排序**
   ```bash
   gawk 'BEGIN{PROCINFO["sorted_in"]="@val_num_desc"} ...'
   ```

**推荐：** 日常使用管道 `sort` 更通用，gawk 方案仅在需要复杂排序时使用。
