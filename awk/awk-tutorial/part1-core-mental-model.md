# Part 1: How awk Works (Core Mental Model)

## 1.1 Streaming Execution Model

```
+------------------------------------------------------------------+
|                    AWK EXECUTION MODEL                           |
+------------------------------------------------------------------+
|                                                                  |
|   INPUT STREAM          AWK ENGINE              OUTPUT STREAM    |
|   ============          ==========              =============    |
|                                                                  |
|   +----------+      +----------------+         +----------+      |
|   |  Line 1  | ---> |  Split into    | ------> | Result 1 |      |
|   +----------+      |  Fields        |         +----------+      |
|                     |                |                           |
|   +----------+      |  Match Pattern |         +----------+      |
|   |  Line 2  | ---> |       |        | ------> | Result 2 |      |
|   +----------+      |       v        |         +----------+      |
|                     |  Execute Action|                           |
|   +----------+      |                |         +----------+      |
|   |  Line 3  | ---> |                | ------> | Result 3 |      |
|   +----------+      +----------------+         +----------+      |
|       ...                                          ...           |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

AWK 的核心执行模型是**流式处理**：
- 输入数据逐行读入（每行称为一条"记录" record）
- 每条记录被自动分割成多个"字段"（field）
- 对每条记录执行"模式-动作"匹配
- 结果直接输出到标准输出

这种设计使得 awk 非常适合处理大文件，因为：
1. 内存占用恒定（只需要保存当前行）
2. 不需要等待文件全部读完就可以开始输出
3. 天然适配 Unix 管道（pipeline）

---

### Pattern → Action Model

```
+------------------------------------------------------------------+
|              PATTERN-ACTION STRUCTURE                            |
+------------------------------------------------------------------+
|                                                                  |
|    awk 'pattern1 { action1 }  pattern2 { action2 }' file         |
|         --------   -------    --------   -------                 |
|            |          |          |          |                    |
|            v          v          v          v                    |
|         [WHEN]     [WHAT]     [WHEN]     [WHAT]                  |
|                                                                  |
|    For EACH input line:                                          |
|    +--------------------------------------------------+          |
|    |  if (pattern1 matches)  --> execute action1      |          |
|    |  if (pattern2 matches)  --> execute action2      |          |
|    |  ...                                             |          |
|    +--------------------------------------------------+          |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

AWK 程序的基本单元是"模式-动作"对：
- **模式（Pattern）**：决定"何时"执行动作（类似 if 条件）
- **动作（Action）**：决定"执行什么"操作

关键理解点：
- 如果省略模式，动作对**每一行**都执行
- 如果省略动作，默认动作是 `{print $0}`（打印整行）
- 多个模式-动作对会**依次**对每一行求值

---

### Example: `ps aux | awk '{print $1, $2}'`

```
+------------------------------------------------------------------+
|                    EXECUTION TRACE                               |
+------------------------------------------------------------------+
|                                                                  |
|  ps aux output (stdin):                                          |
|  +---------------------------------------------------------+     |
|  | USER       PID %CPU %MEM    VSZ   RSS TTY   STAT ...    |     |
|  | root         1  0.0  0.1 169436 13112 ?     Ss   ...    |     |
|  | root         2  0.0  0.0      0     0 ?     S    ...    |     |
|  +---------------------------------------------------------+     |
|                          |                                       |
|                          v                                       |
|  AWK Processing:                                                 |
|  +---------------------------------------------------------+     |
|  | Line 1: $1="USER"    $2="PID"   --> print "USER PID"    |     |
|  | Line 2: $1="root"    $2="1"     --> print "root 1"      |     |
|  | Line 3: $1="root"    $2="2"     --> print "root 2"      |     |
|  +---------------------------------------------------------+     |
|                          |                                       |
|                          v                                       |
|  Output:                                                         |
|  +---------------------------------------------------------+     |
|  | USER PID                                                |     |
|  | root 1                                                  |     |
|  | root 2                                                  |     |
|  +---------------------------------------------------------+     |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

执行过程分解：
1. `ps aux` 的输出通过管道传给 awk
2. awk 逐行读取，每行按空白字符自动分割
3. `$1` 是第一个字段（USER/root），`$2` 是第二个字段（PID/1/2）
4. `{print $1, $2}` 打印这两个字段，逗号表示用 OFS（默认空格）分隔

---

### awk vs grep vs sed

```
+------------------------------------------------------------------+
|                TOOL COMPARISON                                   |
+------------------------------------------------------------------+
|                                                                  |
|  TOOL      PRIMARY PURPOSE        TYPICAL USE                    |
|  ====      ===============        ===========                    |
|                                                                  |
|  grep      Pattern MATCHING       Find lines containing pattern  |
|            (filter lines)         grep "error" log.txt           |
|                                                                  |
|  sed       Stream EDITING         Transform text in-place        |
|            (line transforms)      sed 's/old/new/g' file         |
|                                                                  |
|  awk       Field PROCESSING       Extract/compute on columns     |
|            (structured data)      awk '{sum+=$3} END{print sum}' |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  CAPABILITY MATRIX:                                              |
|                                                                  |
|                    grep    sed     awk                           |
|                    ----    ---     ---                           |
|  Regex match        YES    YES     YES                           |
|  Field extraction   NO     NO      YES  <-- awk's strength       |
|  Arithmetic         NO     NO      YES  <-- awk's strength       |
|  Variables          NO     limited YES                           |
|  Conditionals       NO     limited YES                           |
|  Associative arrays NO     NO      YES                           |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

三者的定位差异：
- **grep**：只做一件事——过滤匹配的行，速度最快
- **sed**：流编辑器，擅长正则替换，但不理解"字段"概念
- **awk**：理解数据结构，可以按列操作，支持变量和计算

选择原则：
- 只需要找出包含某模式的行 → `grep`
- 需要简单的文本替换 → `sed`
- 需要按列提取、计算、条件判断 → `awk`

---

## 1.2 Records and Fields

```
+------------------------------------------------------------------+
|                 RECORD AND FIELD STRUCTURE                       |
+------------------------------------------------------------------+
|                                                                  |
|  Input Line (Record):                                            |
|  +----------------------------------------------------------+    |
|  |  root:x:0:0:root:/root:/bin/bash                         |    |
|  +----------------------------------------------------------+    |
|                              |                                   |
|                              | (FS = ":")                        |
|                              v                                   |
|  Field Breakdown:                                                |
|  +------+---+---+---+------+-------+-----------+                 |
|  | root | x | 0 | 0 | root | /root | /bin/bash |                 |
|  +------+---+---+---+------+-------+-----------+                 |
|    $1    $2  $3  $4   $5     $6       $7                         |
|    |                                  |                          |
|    +----------------------------------+                          |
|                     |                                            |
|                    NF = 7 (Number of Fields)                     |
|                                                                  |
|  $0 = entire record (the whole line)                             |
|  $NF = last field ($7 in this case)                              |
|  $(NF-1) = second to last field ($6)                             |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**记录（Record）与字段（Field）的概念：**
- `$0`：整行内容
- `$1, $2, ... $N`：第 1、2、...N 个字段
- `NF`：当前行的字段总数
- `$NF`：最后一个字段（动态引用）

**字段分割规则：**
- 默认按连续空白字符（空格/Tab）分割
- 可以通过 `FS` 变量或 `-F` 选项自定义分隔符

---

### Field Separator Examples

```
+------------------------------------------------------------------+
|                 FIELD SEPARATOR (FS)                             |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 1: Default (whitespace)                                 |
|  --------------------------------                                |
|  Input:  "hello   world    test"                                 |
|                                                                  |
|  awk '{print $2}'  -->  "world"                                  |
|                                                                  |
|  (Multiple spaces treated as single delimiter)                   |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 2: Colon separator                                      |
|  ---------------------------                                     |
|  Input:  "root:x:0:0:root:/root:/bin/bash"                       |
|                                                                  |
|  awk -F: '{print $1, $3}'  -->  "root 0"                         |
|  awk -F':' '{print $1, $3}'  -->  "root 0"  (same)               |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 3: Multiple character separator                         |
|  ----------------------------------------                        |
|  Input:  "field1::field2::field3"                                |
|                                                                  |
|  awk -F'::' '{print $2}'  -->  "field2"                          |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 4: Regex separator                                      |
|  --------------------------                                      |
|  Input:  "a1b2c3d"                                               |
|                                                                  |
|  awk -F'[0-9]' '{print $1, $2, $3}'  -->  "a b c"                |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**设置字段分隔符的方法：**

1. 命令行选项：`awk -F: '...'`
2. BEGIN 块中设置：`awk 'BEGIN{FS=":"} {...}'`

**重要细节：**
- 默认 FS 是特殊的"空白模式"，连续空白被视为单个分隔符
- 设置 `FS=" "` 和默认行为相同
- 设置 `FS="[ ]"` 则每个空格都是分隔符（空字段会产生）
- FS 可以是正则表达式

---

### Output Field Separator (OFS)

```
+------------------------------------------------------------------+
|                OUTPUT FIELD SEPARATOR (OFS)                      |
+------------------------------------------------------------------+
|                                                                  |
|  RULE: OFS is inserted between fields when you use:              |
|        - print $1, $2, $3   (comma-separated)                    |
|        - Rebuild $0 by modifying any field                       |
|                                                                  |
|  EXAMPLE 1:                                                      |
|  ----------                                                      |
|  awk '{print $1, $2}'       # Output: "field1 field2"            |
|                              #        (space = default OFS)      |
|                                                                  |
|  EXAMPLE 2:                                                      |
|  ----------                                                      |
|  awk 'BEGIN{OFS=","} {print $1, $2}'                             |
|                              # Output: "field1,field2"           |
|                                                                  |
|  EXAMPLE 3: Rebuilding $0                                        |
|  ------------------------                                        |
|  Input: "a b c"                                                  |
|                                                                  |
|  awk 'BEGIN{OFS="-"} {$1=$1; print}'                             |
|                              # Output: "a-b-c"                   |
|                                                                  |
|  WHY: Assigning $1=$1 forces awk to rebuild $0 with OFS          |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**OFS 生效的场景：**
1. `print $1, $2` 中的逗号会被替换为 OFS
2. 修改任何字段后，`$0` 会用 OFS 重新拼接

**常见技巧：**
- `{$1=$1; print}` —— 强制用 OFS 重建整行（不改变内容，只改格式）
- 注意：`print $1 $2`（无逗号）会直接拼接，没有分隔符

---

## 1.3 Built-in Control Variables

```
+------------------------------------------------------------------+
|               ESSENTIAL BUILT-IN VARIABLES                       |
+------------------------------------------------------------------+
|                                                                  |
|  VARIABLE    MEANING                    SCOPE                    |
|  ========    =======                    =====                    |
|                                                                  |
|  NR          Total record number        Global (across files)    |
|              (line number so far)                                |
|                                                                  |
|  FNR         Record number in           Per-file (resets)        |
|              current file                                        |
|                                                                  |
|  NF          Number of fields           Per-record               |
|              in current record                                   |
|                                                                  |
|  FS          Input field separator      Global                   |
|                                                                  |
|  OFS         Output field separator     Global                   |
|                                                                  |
|  RS          Input record separator     Global (default: \n)     |
|                                                                  |
|  ORS         Output record separator    Global (default: \n)     |
|                                                                  |
|  FILENAME    Current input filename     Per-file                 |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**必须掌握的变量：**

| 变量 | 含义 | 典型用途 |
|------|------|----------|
| `NR` | 全局行号 | 打印行号、跳过前 N 行 |
| `FNR` | 当前文件内行号 | 多文件处理时区分文件 |
| `NF` | 当前行字段数 | 获取最后一个字段、检查格式 |
| `FS` | 输入分隔符 | 解析 CSV、/etc/passwd 等 |
| `OFS` | 输出分隔符 | 格式化输出 |
| `FILENAME` | 当前文件名 | 多文件处理 |

---

### NR vs FNR: Critical Difference

```
+------------------------------------------------------------------+
|                    NR vs FNR                                     |
+------------------------------------------------------------------+
|                                                                  |
|  Processing: awk '{print NR, FNR, FILENAME}' file1.txt file2.txt |
|                                                                  |
|  file1.txt:         file2.txt:                                   |
|  +--------+         +--------+                                   |
|  | line A |         | line X |                                   |
|  | line B |         | line Y |                                   |
|  | line C |         | line Z |                                   |
|  +--------+         +--------+                                   |
|                                                                  |
|  OUTPUT:                                                         |
|  +------+------+------------+                                    |
|  |  NR  | FNR  |  FILENAME  |                                    |
|  +------+------+------------+                                    |
|  |   1  |   1  | file1.txt  |  <-- NR and FNR both start at 1    |
|  |   2  |   2  | file1.txt  |                                    |
|  |   3  |   3  | file1.txt  |                                    |
|  |   4  |   1  | file2.txt  |  <-- FNR resets, NR continues      |
|  |   5  |   2  | file2.txt  |                                    |
|  |   6  |   3  | file2.txt  |                                    |
|  +------+------+------------+                                    |
|                                                                  |
|  PATTERN: Detect new file start with: FNR == 1                   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**NR vs FNR 的区别：**
- `NR`：全局行计数，处理多个文件时**不会**重置
- `FNR`：当前文件的行计数，每换一个文件**就会**重置为 1

**实际应用：**
```bash
# 只处理每个文件的第一行
awk 'FNR == 1 {print FILENAME, $0}'

# 跳过所有文件的第一行（表头）
awk 'FNR > 1'

# 检测新文件开始
awk 'FNR == 1 {print "--- New file:", FILENAME}'
```

---

### Common Mistakes with Variables

```
+------------------------------------------------------------------+
|               COMMON VARIABLE MISTAKES                           |
+------------------------------------------------------------------+
|                                                                  |
|  MISTAKE 1: Confusing NR and FNR                                 |
|  --------------------------------                                |
|  WRONG:  awk 'NR > 1' file1.txt file2.txt                        |
|          (Skips header only in first file!)                      |
|                                                                  |
|  RIGHT:  awk 'FNR > 1' file1.txt file2.txt                       |
|          (Skips header in each file)                             |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  MISTAKE 2: Using $NF before understanding it                    |
|  --------------------------------------------                    |
|  Input: "a b c"      $NF = "c"  (NF=3, so $NF=$3)                |
|  Input: "a b"        $NF = "b"  (NF=2, so $NF=$2)                |
|  Input: ""           $NF = ""   (NF=0, $NF=$0="")                |
|                                                                  |
|  GOTCHA: Empty lines have NF=0, $NF references $0                |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  MISTAKE 3: Not setting FS in BEGIN                              |
|  ----------------------------------                              |
|  WRONG:  awk '{FS=":"} {print $1}' /etc/passwd                   |
|          (First line already split before FS takes effect!)     |
|                                                                  |
|  RIGHT:  awk 'BEGIN{FS=":"} {print $1}' /etc/passwd              |
|          (FS set before any line is read)                        |
|                                                                  |
|  RIGHT:  awk -F: '{print $1}' /etc/passwd                        |
|          (-F sets FS before processing starts)                   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**常见错误总结：**

1. **NR/FNR 混淆**：处理多文件时，如果想跳过每个文件的表头，必须用 `FNR`，而不是 `NR`

2. **$NF 的边界情况**：空行的 `NF` 是 0，此时 `$NF` 等于 `$0`（都是空字符串）

3. **FS 设置时机**：如果在普通动作块里设置 `FS`，第一行已经用默认分隔符分割过了。必须在 `BEGIN` 块或用 `-F` 选项

---

## 1.4 BEGIN / END Blocks

```
+------------------------------------------------------------------+
|                 BEGIN / END EXECUTION ORDER                      |
+------------------------------------------------------------------+
|                                                                  |
|  awk 'BEGIN {...}  pattern {action}  END {...}' file             |
|                                                                  |
|  EXECUTION TIMELINE:                                             |
|                                                                  |
|  +----------+     +-------------------+     +---------+          |
|  |  BEGIN   | --> | Process each line | --> |   END   |          |
|  |  block   |     | (pattern/action)  |     |  block  |          |
|  +----------+     +-------------------+     +---------+          |
|       |                    |                     |               |
|       |                    |                     |               |
|   Runs ONCE            Runs for              Runs ONCE           |
|   BEFORE any           EVERY line            AFTER all           |
|   input read           of input              input done          |
|                                                                  |
|  USE CASES:                                                      |
|  +----------+     +-------------------+     +---------+          |
|  | - Set FS |     | - Filter lines    |     | - Print |          |
|  | - Set OFS|     | - Extract fields  |     |   totals|          |
|  | - Print  |     | - Accumulate sums |     | - Print |          |
|  |   header |     | - Count records   |     |   stats |          |
|  | - Init   |     | - Transform data  |     | - Clean |          |
|  |   vars   |     |                   |     |   up    |          |
|  +----------+     +-------------------+     +---------+          |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**BEGIN 块：**
- 在读取任何输入**之前**执行一次
- 用途：设置 FS/OFS、初始化变量、打印表头

**END 块：**
- 在处理完所有输入**之后**执行一次
- 用途：打印汇总、统计结果、清理工作

**执行顺序保证：**
```
BEGIN → 逐行处理 → END
```

---

### Practical BEGIN/END Examples

```
+------------------------------------------------------------------+
|               BEGIN/END PRACTICAL EXAMPLES                       |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 1: Print header and data                                |
|  --------------------------------                                |
|  awk 'BEGIN {print "User\tPID"}                                  |
|       {print $1 "\t" $2}' <(ps aux)                              |
|                                                                  |
|  Output:                                                         |
|  User    PID                                                     |
|  USER    PID                                                     |
|  root    1                                                       |
|  root    2                                                       |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 2: Sum a column                                         |
|  -----------------------                                         |
|  awk '{sum += $3} END {print "Total:", sum}' data.txt            |
|                                                                  |
|  data.txt:          Output:                                      |
|  a b 10             Total: 60                                    |
|  c d 20                                                          |
|  e f 30                                                          |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 3: Count and average                                    |
|  ---------------------------                                     |
|  awk '{sum += $1; count++}                                       |
|       END {print "Avg:", sum/count}' numbers.txt                 |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
|  EXAMPLE 4: Initialize with non-zero default                     |
|  -------------------------------------------                     |
|  awk 'BEGIN {max = -999999}                                      |
|       {if ($1 > max) max = $1}                                   |
|       END {print "Max:", max}' numbers.txt                       |
|                                                                  |
+------------------------------------------------------------------+
```

**中文解释：**

**典型使用模式：**

1. **打印表头**：在 BEGIN 中打印列标题
2. **累加求和**：在主体中累加，在 END 中打印
3. **计数统计**：在主体中计数，在 END 中计算平均值
4. **初始化极值**：在 BEGIN 中初始化 max/min 为极端值

**注意：** 如果没有输入数据，END 块仍会执行，此时除 BEGIN 中初始化的变量外，其他变量为默认值（数字 0，字符串 ""）
