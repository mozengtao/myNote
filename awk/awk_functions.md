## AWK 常用内置函数与复杂逻辑配方（以 gawk 为主）

面向日志分析、数据清洗、聚合统计这类“流式处理”任务：**AWK 的复杂逻辑不是靠堆函数，而是靠“正则匹配 + 字符串处理 + 关联数组聚合 + 流式执行”的组合**。

本文件将原有内容整理为：

- **速查**：按类别汇总常用函数/语句与要点
- **配方**：高频组合模式（怎么拼起来写复杂逻辑）
- **实战**：可直接改造的完整示例
- **附录**：`printf/sprintf` 格式符、预定义变量、`getline` 用法、时间格式符

> 说明：文中标注 **(gawk)** 的为 GNU awk 扩展；若你用的是 `awk`/`mawk`/`busybox awk`，请先确认支持情况。

---

## 目录

- [函数优先级（先背这些）](#priority)
- [兼容性与版本提示](#compat)
- [函数速查（按类别）](#cheatsheet)
  - [字符串 / 正则 / 格式化](#string)
  - [数学函数](#math)
  - [时间函数（日志分析核心）](#time)
  - [数组 / 排序 / 类型](#array)
  - [I/O 与流程控制](#io)
  - [位操作（gawk 4.0+）](#bitwise)
  - [国际化（I18N，gawk）](#i18n)
- [复杂逻辑高频组合模式（配方）](#patterns)
- [综合实战示例](#examples)
- [附录](#appendix)
  - [`printf/sprintf` 格式符速查](#fmt)
  - [常用预定义变量速查](#vars)
  - [`getline` 用法速查](#getline)
  - [常用时间格式符速查](#timefmt)

---

<a id="priority"></a>
## 函数优先级（先背这些）

不必全记，按常见度分层：

- **第一梯队（必须熟练）**
  - `split`
  - `gsub` / `sub`
  - `match`
  - `sprintf` / `printf`
  - `length` / `substr`
- **第二梯队（常用）**
  - `index`
  - `tolower` / `toupper`
  - `systime` / `strftime` / `mktime`（做日志时间就很常用）
- **第三梯队（进阶）**
  - `getline`
  - `system`
  - `asort` / `asorti`（稳定排序输出、TopN）

---

<a id="compat"></a>
## 兼容性与版本提示

- **POSIX awk 基本都有**：`length` `substr` `index` `match` `sub` `gsub` `split` `int` `sqrt` `rand` `srand` `system` `strftime` `systime` `mktime`（时间相关在不少实现里也有，但细节差异要注意）
- **典型 gawk 扩展**：
  - 字符串/正则：`gensub` `strtonum` `patsplit`、`match(s, r, a)` 第三参数捕获组
  - 数组/类型：`asort` `asorti` `isarray` `typeof`、`length(array)`（部分 awk 不支持）
  - 控制：`nextfile`
  - 位操作：`and` `or` `xor` `lshift` `rshift` `compl`
  - 国际化：`bindtextdomain` `dcgettext` `dcngettext`

> 工程建议：如果你要用到大量 gawk 扩展，最好显式用 `gawk` 运行（而不是依赖系统默认 `awk` 指向哪一个实现）。

---

<a id="cheatsheet"></a>
## 函数速查（按类别）

<a id="string"></a>
### 字符串 / 正则 / 格式化

| 名称 | 语法 | 返回 | 要点（复杂逻辑里的用法） |
|---|---|---:|---|
| `length` | `length([s])` | number | `s` 省略则取 `$0`；常用于过滤异常行、空行判断 |
| `substr` | `substr(s, start[, len])` | string | 下标从 **1** 开始；提取固定格式字段/后缀 |
| `index` | `index(s, sub)` | number | 返回子串首次出现位置（1-based），未找到返回 0；适合做“包含/不包含”判断 |
| `match` | `match(s, r[, a])` | number | 成功返回起始位置，并设置 `RSTART/RLENGTH`；第三参数 `a` 捕获组 **(gawk)** |
| `sub` | `sub(r, repl[, s])` | number | 只替换第一个匹配；省略 `s` 则修改 `$0`；返回是否替换成功（1/0） |
| `gsub` | `gsub(r, repl[, s])` | number | 全局替换所有匹配；返回替换次数；做清洗标准化最常用 |
| `gensub` | `gensub(r, repl, how[, s])` | string | **(gawk)** 更强替换：`how` 可为 `"g"`/`"G"`（全局）或数字（第 n 次）；**不修改原串** |
| `split` | `split(s, a[, sep])` | number | 按 `sep`（省略则 `FS`）拆分；复杂解析几乎离不开 |
| `patsplit` | `patsplit(s, a[, re])` | number | **(gawk)** 按“字段模式”提取（默认 `FPAT`）；适合“分隔符不稳定”的文本 |
| `tolower/toupper` | `tolower(s)` / `toupper(s)` | string | 大小写归一；配合 `IGNORECASE` 做 case-insensitive 处理 |
| `sprintf` | `sprintf(fmt, ...)` | string | 生成结构化文本（日志/报表）；比手拼字符串可靠 |
| `strtonum` | `strtonum(s)` | number | **(gawk)** 安全转数值（十进制/八进制 `0` 前缀/十六进制 `0x` 前缀） |

常见提示：

- **`match` 精准定位**：`substr(s, RSTART, RLENGTH)` 可直接拿到匹配片段。
- **捕获组提取（gawk）**：`match(s, /ERROR: ([0-9]+)/, m)` 后 `m[1]` 就是捕获到的数字。
- **正则大小写**：`IGNORECASE=1` 时，正则匹配大小写不敏感（注意这会影响所有正则）。

示例：提取 `[...]` 内的时间戳（使用 `match` + `substr`）

```awk
{
    if (match($0, /\[[0-9:-]+ [0-9:]+\]/)) {
        # 去掉左右括号
        print substr($0, RSTART + 1, RLENGTH - 2)
    }
}
```

示例：替换第 2 个逗号（`gensub`，gawk）

```awk
{ print gensub(/,/, ";", 2, $0) }
```

---

<a id="math"></a>
### 数学函数

| 名称 | 语法 | 返回 | 要点 |
|---|---|---:|---|
| `int` | `int(x)` | number | **向零截断**（不是数学意义的 floor）；负数要注意：`int(-1.9)` 为 `-1` |
| `sqrt` | `sqrt(x)` | number | 平方根 |
| `exp/log` | `exp(x)` / `log(x)` | number | 自然指数 / 自然对数 |
| `sin/cos/atan2` | `sin(x)` / `cos(x)` / `atan2(y, x)` | number | 三角函数参数为**弧度**；角度→弧度：`deg * pi / 180` |
| `rand/srand` | `rand()` / `srand([seed])` | number | `srand()` 建议只在 `BEGIN` 调一次；`srand()` 返回旧种子值 |
| `abs` | `abs(x)` | number | **(gawk)** 绝对值；非 gawk 可用三元：`(x<0?-x:x)` |

示例：按概率采样（`rand` + `srand`）

```awk
BEGIN { srand() }
rand() > 0.5 { print }
```

---

<a id="time"></a>
### 时间函数（日志分析核心）

| 名称 | 语法 | 返回 | 要点 |
|---|---|---:|---|
| `systime` | `systime()` | number | 当前 Unix 时间戳（秒） |
| `strftime` | `strftime(fmt[, t])` | string | 格式化时间；省略 `t` 表示当前时间 |
| `mktime` | `mktime("YYYY MM DD HH MM SS [DST]")` | number | 把本地时间字符串转 Unix 时间戳（秒）；输入需要**空格分隔** |

示例：把 `2026-03-24 10:00:00` 转成时间戳并做差

```awk
{
    # 假设输入为两列：$1=2026-03-24  $2=10:00:00
    ts = $1 " " $2                              # 2026-03-24 10:00:00
    gsub(/[-:]/, " ", ts)                       # 2026 03 24 10 00 00
    t = mktime(ts)
    # ... 用 t 做计算
    print t
}
```

> 注意：`%s`（直接输出时间戳）常见于 gawk 的 `strftime`，但并非所有实现都支持，跨环境时要验证。

---

<a id="array"></a>
### 数组 / 排序 / 类型

AWK 的“复杂统计”基本都靠 **关联数组**（key→value）完成。

| 名称 | 语法 | 返回 | 要点 |
|---|---|---:|---|
| 删除元素 | `delete a[k]` | - | 删除单个键 |
| 清空数组 | `delete a` | - | 清空整个数组 |
| 数组长度 | `length(a)` | number | **(gawk)** 返回元素个数；部分 awk 不支持对数组使用 `length` |
| 值排序 | `asort(src[, dest[, how]])` | number | **(gawk)** 按 value 排序；排序后索引变成 `1..N`，原 key 会丢失（除非用 `dest`） |
| 键排序 | `asorti(src[, dest[, how]])` | number | **(gawk)** 按 key 排序；常用于稳定输出、TopN 预处理 |
| 判断是否数组 | `isarray(x)` | number | **(gawk)** 是数组返回 1 |
| 变量类型 | `typeof(x)` | string | **(gawk)** 返回 `"array" "string" "number" "regexp" ...`（不同版本略有差异） |

示例：词频统计（数组聚合）+ 稳定排序输出（`asorti`）

```awk
{ count[$1]++ }
END {
    n = asorti(count, keys)
    for (i = 1; i <= n; i++) {
        k = keys[i]
        printf "%-20s %8d\n", k, count[k]
    }
}
```

提示：

- `for (k in a)` 的遍历顺序**不保证稳定**；需要稳定排序时用 `asorti/asort`。
- **数组的数组（gawk）**：可写 `a[x][y] = v` 来模拟多维结构（或用 `a[x SUBSEP y]`）。

---

<a id="io"></a>
### I/O 与流程控制

> `print/printf/next/exit` 等严格来说是语句/控制关键字，但在工程写法上通常一起记。

| 名称 | 语法 | 返回 | 要点 |
|---|---|---:|---|
| `print` | `print expr...` | - | 默认用 `OFS/ORS`；适合快速输出 |
| `printf` | `printf(fmt, ...)` | - | 可控对齐/精度；做报表必备 |
| `getline` | `getline [var]` | number | **语句**：读入下一行到 `$0` 或 `var`；返回 1/0/-1；强大但易写出隐蔽 bug |
| `close` | `close(fileOrCmd[, how])` | number | 关闭文件/管道；循环里频繁打开不同目标时要 `close` 防止句柄耗尽 |
| `fflush` | `fflush([file])` | number | **(gawk)** 刷新缓冲；无参可刷新所有输出 |
| `system` | `system(cmd)` | number | 执行命令并返回退出码；复杂管道更推荐用 `cmd | getline`/`print | cmd` |
| `next` | `next` | - | 跳过当前记录，读下一行 |
| `nextfile` | `nextfile` | - | **(gawk)** 跳过当前文件，直接处理下一个文件 |
| `exit` | `exit [code]` | - | 退出 awk（可返回状态码） |

示例：从外部命令读取一行

```awk
BEGIN {
    "date +%F" | getline d
    close("date +%F")
    print "today=", d
}
```

示例：`getline` 从文件读取（注意关闭）

```awk
BEGIN {
    while ((getline line < "/etc/hosts") > 0) {
        if (line ~ /^[0-9]/) print line
    }
    close("/etc/hosts")
}
```

危险提示（经验规则）：

- `getline` 会改变 `$0/NF/$1..`（当不带变量时），也会影响 `NR/FNR`，写复杂脚本时要非常清楚输入流在哪。
- 若你用 `cmd | getline` 或对很多不同文件名做重定向，**记得 `close(cmdOrFile)`**。

---

<a id="bitwise"></a>
### 位操作（gawk 4.0+）

用于底层数据处理/掩码统计（不常见但在网络/协议字段解析时很有用）。

| 名称 | 语法 | 说明 |
|---|---|---|
| `and` | `and(v1, v2[, ...])` | 按位与 |
| `or` | `or(v1, v2[, ...])` | 按位或 |
| `xor` | `xor(v1, v2[, ...])` | 按位异或 |
| `lshift` | `lshift(v, n)` | 左移 |
| `rshift` | `rshift(v, n)` | 右移 |
| `compl` | `compl(v)` | 按位取反（补码） |

---

<a id="i18n"></a>
### 国际化（I18N，gawk）

不常用，但做本地化输出时可以用：

| 名称 | 语法 | 说明 |
|---|---|---|
| `bindtextdomain` | `bindtextdomain(dir[, domain])` | 设置消息目录 |
| `dcgettext` | `dcgettext(str[, domain[, category]])` | 查找当前 locale 的翻译 |
| `dcngettext` | `dcngettext(s1, s2, n[, domain[, category]])` | 复数形式翻译 |

---

<a id="patterns"></a>
## 复杂逻辑高频组合模式（配方）

这里是“怎么把函数拼起来”的套路（比单列函数更重要）。

### 模式 1：解析 + 聚合（split + array）

```awk
{
    n = split($0, f, ",")
    key = f[1]
    count[key]++
}
END {
    for (k in count) print k, count[k]
}
```

### 模式 2：正则提取 + 捕获组（match + groups）

```awk
{
    if (match($0, /ERROR: ([0-9]+)/, m)) {
        code = m[1]
        cnt[code]++
    }
}
END { for (c in cnt) print c, cnt[c] }
```

### 模式 3：数据清洗（gsub/sub + 标准化）

```awk
{
    gsub(/"/, "", $0)
    gsub(/[[:space:]]+/, " ", $0)
    sub(/^ /, "", $0); sub(/ $/, "", $0)
    print
}
```

### 模式 4：时间打点与时间窗计算（systime/strftime/mktime）

```awk
BEGIN { now = systime() }
{
    print strftime("%Y-%m-%d %H:%M:%S", now), $0
}
```

### 模式 5：结构化输出（sprintf/printf）

```awk
{
    msg = sprintf("user=%s action=%s ts=%s", $1, $2, strftime("%F %T"))
    print msg
}
```

### 工程化提示（组合使用）

- 复杂逻辑往往是：**先 `split`/`match` 提取字段 → 用数组聚合/去重 → 最后 `printf/sprintf` 输出**。
- 当内置函数不够用时：用 `function` 自定义函数实现模块化（让脚本可维护）。
- 如果你发现自己写了多层嵌套、状态机、大片函数封装：通常意味着已超过 AWK 的最佳边界，应该切到 Python/Go（见文末“上限认知”）。

---

<a id="examples"></a>
## 综合实战示例

### 示例 1：分析 Nginx 日志，统计慢请求 IP（多函数组合）

假设日志字段（示例约定）：

- `$1`：IP
- `$7`：URL
- `$11`：耗时（毫秒）

需求：

- 统计每个 IP 的总访问次数
- 找出出现过耗时 > 1000ms 的 IP，并把慢请求写入 `slow_requests.log`

```bash
gawk '{
    ip_count[$1]++

    if ($11 > 1000) {
        slow_ip[$1] = 1

        slow_log = sprintf("IP=%s URL=%s cost=%.2fs", $1, $7, $11/1000)
        print slow_log >> "slow_requests.log"
        fflush("slow_requests.log")
    }
}
END {
    print "=== slow ip summary ==="
    for (ip in slow_ip) {
        printf "%-15s total=%5d has_slow=1\n", ip, ip_count[ip]
    }
    close("slow_requests.log")
}' /var/log/nginx/access.log
```

### 示例 2：一行内的多条件统计 + 格式化输出

统计每行数字的个数、总和、平均值（忽略非数字字段）：

```awk
{
    sum = 0; count = 0
    for (i = 1; i <= NF; i++) {
        if ($i ~ /^[0-9]+(\.[0-9]+)?$/) {
            sum += $i
            count++
        }
    }
    avg = (count > 0) ? sum / count : 0
    printf "Line %d: Count=%d Sum=%.2f Avg=%.2f\n", NR, count, sum, avg
}
```

### 示例 3：数组去重与排序输出（稳定）

统计 IP 出现次数并按 IP 排序输出（`asorti`，gawk）：

```awk
{ ip_count[$1]++ }
END {
    n = asorti(ip_count, sorted_ips)
    for (i = 1; i <= n; i++) {
        ip = sorted_ips[i]
        printf "%-15s %5d\n", ip, ip_count[ip]
    }
}
```

### 示例 4：多文件处理与状态保持（对比两个文件差异）

对比两个文件：按第 1 列 key 匹配，输出匹配/仅在某一侧的记录：

```awk
FILENAME == ARGV[1] { file1[$1] = $0; next }
FILENAME == ARGV[2] {
    if ($1 in file1) {
        print "MATCH:", $0
        delete file1[$1]
    } else {
        print "ONLY_IN_FILE2:", $0
    }
}
END {
    for (k in file1) print "ONLY_IN_FILE1:", file1[k]
}
```

### 示例 5：复杂字段提取与替换（JSON-like 简化版）

把形如 `{ "name":"bob", "age":18 }` 的行做简化解析：

```awk
{
    gsub(/[{}"]/, "")
    n = split($0, pairs, ",")
    for (i = 1; i <= n; i++) {
        split(pairs[i], kv, ":")
        key = kv[1]; val = kv[2]
        data[key] = val
    }
    print "Name:", data["name"], "Age:", data["age"] + 0
    delete data
}
```

### 示例 6：时间处理与计算（计算日志时间差）

假设日志中包含 `HH:MM:SS`（在 `$2`），示例演示把它拼成当天时间并与起点做差：

```awk
BEGIN {
    start = mktime("2024 03 24 10 00 00")
}
/START/ {
    hhmmss = $2
    gsub(/:/, " ", hhmmss)                # HH MM SS
    cur = mktime("2024 03 24 " hhmmss)
    print "Elapsed:", cur - start, "seconds"
}
```

---

<a id="appendix"></a>
## 附录

<a id="fmt"></a>
### `printf/sprintf` 格式符速查

| 格式符 | 说明 | 示例 |
|---|---|---|
| `%s` | 字符串 | `printf "%s", $1` |
| `%d`/`%i` | 十进制整数 | `printf "%5d", 42` |
| `%f` | 浮点数 | `printf "%.2f", 3.14159` |
| `%e`/`%E` | 科学计数法 | `printf "%e", 12345` |
| `%g`/`%G` | 自动选择 `%f` 或 `%e` | `printf "%g", 0.00001` |
| `%o` | 八进制 | `printf "%o", 8` |
| `%x`/`%X` | 十六进制 | `printf "%x", 255` |
| `%c` | 字符（按数值解释） | `printf "%c", 65` |
| `%%` | 百分号 | `printf "100%%"` |

常用修饰：

- 左对齐：`%-10s`
- 前导零：`%010d`
- 精度：`%.2f`

---

<a id="vars"></a>
### 常用预定义变量速查

| 变量 | 说明 |
|---|---|
| `ARGC` / `ARGV` | 命令行参数个数 / 参数数组 |
| `FS` | 输入字段分隔符（默认空白） |
| `FPAT` | **(gawk)** 字段模式（正则）；设置后按“匹配模式”提取字段（适合分隔符不稳定的文本） |
| `OFS` | 输出字段分隔符（默认空格） |
| `RS` | 输入记录分隔符（默认换行） |
| `ORS` | 输出记录分隔符（默认换行） |
| `NF` | 当前记录字段数 |
| `NR` / `FNR` | 当前行号（全局 / 当前文件） |
| `FILENAME` | 当前输入文件名 |
| `SUBSEP` | 多维数组下标分隔符（用于 `a[i SUBSEP j]` 这类复合 key） |
| `IGNORECASE` | 非零时，正则匹配忽略大小写 |
| `CONVFMT` | 数字转字符串格式（默认 `%.6g`） |
| `OFMT` | `print` 输出数字格式 |
| `ENVIRON` | 环境变量数组 |
| `RSTART` / `RLENGTH` | `match()` 匹配起始位置与长度 |
| `PROCINFO` | **(gawk)** 进程信息数组 |

---

<a id="getline"></a>
### `getline` 用法速查

```awk
getline                    # 读取下一行到 $0
getline var                # 读取下一行到变量 var
getline < "file"           # 从文件读取到 $0
getline var < "file"       # 从文件读取到 var
command | getline          # 从命令管道读取到 $0
command | getline var      # 从命令管道读取到 var
```

返回值约定：

- `1`：成功读取
- `0`：EOF
- `-1`：错误

---

<a id="timefmt"></a>
### 常用时间格式符速查

`strftime` 常用：

- `%Y`：四位年份
- `%m`：月份（01-12）
- `%d`：日期（01-31）
- `%H`：小时（00-23）
- `%M`：分钟（00-59）
- `%S`：秒（00-59）
- `%A`：星期全称
- `%B`：月份全称
- `%F`：等价于 `%Y-%m-%d`（不少实现支持）
- `%T`：等价于 `%H:%M:%S`（不少实现支持）

---

## 工程视角总结（边界与上限认知）

如果你做 Linux / 网络 / 日志分析：AWK 的优势在于**一遍扫描、就地聚合、快速出结果**。

但当你开始在 AWK 里写：

- 多层嵌套逻辑
- 状态机
- 大量函数封装与复杂数据结构

通常意味着你已经超出 AWK 的最佳使用边界，应该切到 Python / Go 等更适合“复杂程序结构”的语言。