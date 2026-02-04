# Part 5: Typical Daily Use Cases

## 5.1 Log Analysis

```
+------------------------------------------------------------------+
|                    LOG ANALYSIS WITH AWK                          |
+------------------------------------------------------------------+
|                                                                   |
|  WHY AWK FOR LOGS:                                                |
|  - Handles large files efficiently (streaming)                    |
|  - Field extraction is natural                                    |
|  - Aggregation (counts, sums) is built-in                         |
|  - Pattern matching + data processing in one tool                 |
|                                                                   |
|  COMMON LOG FORMATS:                                              |
|  +----------------------------------------------+                 |
|  | Apache/Nginx:                                |                 |
|  | IP - - [timestamp] "METHOD /path" status sz  |                 |
|  | $1   $4   $5-$6    $7      $8     $9    $10  |                 |
|  +----------------------------------------------+                 |
|  | Syslog:                                      |                 |
|  | Mon DD HH:MM:SS hostname service: message    |                 |
|  | $1  $2 $3       $4       $5       $6...      |                 |
|  +----------------------------------------------+                 |
|  | Application logs (varies):                   |                 |
|  | 2024-01-15 10:30:00 [LEVEL] component: msg   |                 |
|  | $1         $2       $3      $4         $5... |                 |
|  +----------------------------------------------+                 |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**AWK 分析日志的优势：**
1. **流式处理**：不需要把大文件全部加载到内存
2. **字段提取**：直接用 `$1, $2...` 访问各部分
3. **聚合统计**：计数、求和等操作内置支持
4. **模式+处理**：一个工具完成过滤和数据处理

---

### Count Error Types

```
+------------------------------------------------------------------+
|               COUNT ERROR TYPES                                   |
+------------------------------------------------------------------+
|                                                                   |
|  SAMPLE LOG (app.log):                                            |
|  2024-01-15 10:30:00 [ERROR] db: Connection timeout               |
|  2024-01-15 10:30:01 [INFO] api: Request completed                |
|  2024-01-15 10:30:02 [ERROR] db: Query failed                     |
|  2024-01-15 10:30:03 [WARN] cache: Miss rate high                 |
|  2024-01-15 10:30:04 [ERROR] api: Rate limit exceeded             |
|  2024-01-15 10:30:05 [ERROR] db: Connection timeout               |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 1: Count by log level                                       |
|  --------------------------                                       |
|  awk -F'[][]' '{count[$2]++} END {for (l in count) print l,       |
|      count[l]}' app.log                                           |
|                                                                   |
|  Output:                                                          |
|  ERROR 4                                                          |
|  INFO 1                                                           |
|  WARN 1                                                           |
|                                                                   |
|  Note: -F'[][]' splits on [ or ]                                  |
|        $2 becomes the level inside brackets                       |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 2: Count ERROR messages by component                        |
|  -----------------------------------------                        |
|  awk '/\[ERROR\]/ {count[$4]++}                                   |
|       END {for (c in count) print c, count[c]}' app.log           |
|                                                                   |
|  Output:                                                          |
|  db: 3                                                            |
|  api: 1                                                           |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 3: Top N error messages                                     |
|  ----------------------------                                     |
|  awk '/\[ERROR\]/ {                                               |
|      # Get everything after component:                            |
|      match($0, /: .+$/)                                           |
|      msg = substr($0, RSTART+2)                                   |
|      count[msg]++                                                 |
|  }                                                                |
|  END {for (m in count) print count[m], m}' app.log                |
|      | sort -rn | head -10                                        |
|                                                                   |
|  Output:                                                          |
|  2 Connection timeout                                             |
|  1 Query failed                                                   |
|  1 Rate limit exceeded                                            |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**日志错误统计技巧：**

1. **按日志级别统计**
   ```bash
   awk -F'[][]' '{count[$2]++} END {for(l in count) print l,count[l]}'
   ```
   - `-F'[][]'` 用 `[` 或 `]` 作为分隔符
   - `$2` 就是 `[LEVEL]` 中的 LEVEL

2. **只统计 ERROR 按组件分类**
   ```bash
   awk '/\[ERROR\]/ {count[$4]++} END {for(c in count) print c,count[c]}'
   ```

3. **Top N 错误消息**
   ```bash
   awk '/ERROR/ {count[$0]++} END {for(m in count) print count[m],m}' \
       | sort -rn | head -10
   ```

---

### Extract Timestamps

```
+------------------------------------------------------------------+
|               TIMESTAMP EXTRACTION                                |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 1: Extract hour from logs                                   |
|  ------------------------------                                   |
|  Log: 2024-01-15 10:30:45 [INFO] message                          |
|                                                                   |
|  awk '{split($2, t, ":"); print t[1]}' app.log | sort | uniq -c   |
|                                                                   |
|  Or all in awk:                                                   |
|  awk '{split($2, t, ":"); hours[t[1]]++}                          |
|       END {for (h in hours) print h, hours[h]}' app.log           |
|       | sort -n                                                   |
|                                                                   |
|  Output (activity by hour):                                       |
|  09 150                                                           |
|  10 340                                                           |
|  11 280                                                           |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 2: Filter logs by time range                                |
|  ---------------------------------                                |
|  # Logs between 10:00 and 11:00                                   |
|  awk '$2 >= "10:00:00" && $2 < "11:00:00"' app.log                |
|                                                                   |
|  # Using range pattern                                            |
|  awk '/10:00:00/,/10:59:59/' app.log                              |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 3: Calculate request duration                               |
|  ----------------------------------                               |
|  Log format: timestamp [INFO] Request ID completed in 150ms       |
|                                                                   |
|  awk '/completed in/ {                                            |
|      match($0, /[0-9]+ms/)                                        |
|      ms = substr($0, RSTART, RLENGTH-2)                           |
|      total += ms                                                  |
|      count++                                                      |
|  }                                                                |
|  END {printf "Avg: %.2fms\n", total/count}'                       |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 4: Find slow requests (> 1000ms)                            |
|  -------------------------------------                            |
|  awk '/completed in/ {                                            |
|      match($0, /[0-9]+ms/)                                        |
|      ms = substr($0, RSTART, RLENGTH-2) + 0                       |
|      if (ms > 1000) print                                         |
|  }' app.log                                                       |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**时间戳处理技巧：**

1. **提取小时统计**
   ```bash
   awk '{split($2, t, ":"); h[t[1]]++} END {for(x in h) print x,h[x]}'
   ```
   - `split($2, t, ":")` 把时间字符串按 `:` 分割
   - `t[1]` 是小时部分

2. **时间范围过滤**
   ```bash
   awk '$2 >= "10:00:00" && $2 < "11:00:00"'
   ```
   - 字符串比较对 `HH:MM:SS` 格式有效

3. **提取数值计算**
   ```bash
   awk '/pattern/ {match($0, /[0-9]+ms/); ms=substr($0,RSTART,RLENGTH-2)+0; ...}'
   ```
   - `match()` 找到位置，`substr()` 提取，`+0` 转数值

---

### Find Top-N Entries

```
+------------------------------------------------------------------+
|                 TOP-N ANALYSIS                                    |
+------------------------------------------------------------------+
|                                                                   |
|  ACCESS LOG ANALYSIS (Apache/Nginx format):                       |
|  192.168.1.1 - - [15/Jan/2024:10:30:00] "GET /api/users" 200 1024 |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TOP 10 IPs by request count:                                     |
|  ----------------------------                                     |
|  awk '{count[$1]++}                                               |
|       END {for (ip in count) print count[ip], ip}' access.log    |
|       | sort -rn | head -10                                       |
|                                                                   |
|  Output:                                                          |
|  1523 192.168.1.100                                               |
|  1102 192.168.1.50                                                |
|  890 10.0.0.25                                                    |
|  ...                                                              |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TOP 10 most requested URLs:                                      |
|  ---------------------------                                      |
|  awk '{                                                           |
|      # $7 contains "GET /path" - extract path                     |
|      split($7, req, " ")                                          |
|      url = req[2]                                                 |
|      count[url]++                                                 |
|  }                                                                |
|  END {for (u in count) print count[u], u}' access.log            |
|      | sort -rn | head -10                                        |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TOP 10 URLs by total bytes transferred:                          |
|  ---------------------------------------                          |
|  awk '{bytes[$7] += $10}                                          |
|       END {for (u in bytes) print bytes[u], u}' access.log       |
|       | sort -rn | head -10                                       |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TOP 10 slowest endpoints (if response time in log):              |
|  --------------------------------------------------              |
|  # Assuming last field is response time in ms                     |
|  awk '{sum[$7] += $NF; count[$7]++}                               |
|       END {for (u in sum) print sum[u]/count[u], u}' access.log  |
|       | sort -rn | head -10                                       |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**Top-N 分析模式：**

```bash
# 通用模式
awk '{count[KEY]++} END {for(k in count) print count[k],k}' | sort -rn | head -N
```

**常见分析：**

| 分析目标 | KEY 字段 | 聚合方式 |
|----------|----------|----------|
| 访问最多的 IP | `$1` | `count++` |
| 最热门 URL | `$7` | `count++` |
| 传输最大的资源 | `$7` | `sum += $size` |
| 平均响应最慢 | `$7` | `sum/count` |

---

## 5.2 Config and Structured Text

### Key-Value Parsing

```
+------------------------------------------------------------------+
|               KEY-VALUE PARSING                                   |
+------------------------------------------------------------------+
|                                                                   |
|  CONFIG FILE (app.conf):                                          |
|  # Database settings                                              |
|  db_host = localhost                                              |
|  db_port = 5432                                                   |
|  db_name = myapp                                                  |
|                                                                   |
|  # Cache settings                                                 |
|  cache_enabled = true                                             |
|  cache_ttl = 3600                                                 |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 1: Extract specific value                                   |
|  ------------------------------                                   |
|  awk -F' *= *' '/^db_host/ {print $2}' app.conf                   |
|                                                                   |
|  Output: localhost                                                |
|                                                                   |
|  Note: -F' *= *' handles spaces around =                          |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 2: Load all settings into array                             |
|  ------------------------------------                             |
|  awk -F' *= *' '!/^#/ && NF==2 {                                  |
|      gsub(/^[[:space:]]+|[[:space:]]+$/, "", $1)                  |
|      gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2)                  |
|      config[$1] = $2                                              |
|  }                                                                |
|  END {                                                            |
|      print "Host:", config["db_host"]                             |
|      print "Port:", config["db_port"]                             |
|  }' app.conf                                                      |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 3: Convert to environment variables                         |
|  ----------------------------------------                         |
|  awk -F' *= *' '!/^#/ && NF==2 {                                  |
|      gsub(/^[[:space:]]+|[[:space:]]+$/, "", $1)                  |
|      gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2)                  |
|      printf "export %s=\"%s\"\n", toupper($1), $2                 |
|  }' app.conf                                                      |
|                                                                   |
|  Output:                                                          |
|  export DB_HOST="localhost"                                       |
|  export DB_PORT="5432"                                            |
|  ...                                                              |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**配置文件解析技巧：**

1. **提取单个值**
   ```bash
   awk -F' *= *' '/^KEY/ {print $2}' config
   ```
   - `-F' *= *'` 处理 `=` 两边的空格

2. **加载所有配置**
   ```bash
   awk -F'=' '!/^#/ && NF==2 {config[$1]=$2} END {print config["key"]}'
   ```
   - `!/^#/` 跳过注释
   - `NF==2` 确保格式正确

3. **转换为环境变量**
   ```bash
   awk -F'=' '{printf "export %s=\"%s\"\n", toupper($1), $2}'
   ```

---

### INI File Processing

```
+------------------------------------------------------------------+
|               INI FILE PROCESSING                                 |
+------------------------------------------------------------------+
|                                                                   |
|  INI FILE (settings.ini):                                         |
|  [database]                                                       |
|  host = localhost                                                 |
|  port = 5432                                                      |
|                                                                   |
|  [cache]                                                          |
|  enabled = true                                                   |
|  ttl = 3600                                                       |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 1: Extract value from specific section                      |
|  ------------------------------------------                       |
|  awk -F' *= *' '                                                  |
|      /^\[database\]/ {in_section=1; next}                         |
|      /^\[/ {in_section=0}                                         |
|      in_section && /^host/ {print $2; exit}                       |
|  ' settings.ini                                                   |
|                                                                   |
|  Output: localhost                                                |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 2: List all keys in a section                               |
|  ----------------------------------                               |
|  awk -F' *= *' '                                                  |
|      /^\[cache\]/ {in_section=1; next}                            |
|      /^\[/ {in_section=0}                                         |
|      in_section && NF==2 {print $1}                               |
|  ' settings.ini                                                   |
|                                                                   |
|  Output:                                                          |
|  enabled                                                          |
|  ttl                                                              |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 3: Parse entire INI into section.key format                 |
|  ------------------------------------------------                 |
|  awk -F' *= *' '                                                  |
|      /^\[.+\]$/ {                                                 |
|          gsub(/[\[\]]/, "")                                       |
|          section = $0                                             |
|          next                                                     |
|      }                                                            |
|      NF==2 && !/^[#;]/ {                                          |
|          printf "%s.%s = %s\n", section, $1, $2                   |
|      }                                                            |
|  ' settings.ini                                                   |
|                                                                   |
|  Output:                                                          |
|  database.host = localhost                                        |
|  database.port = 5432                                             |
|  cache.enabled = true                                             |
|  cache.ttl = 3600                                                 |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**INI 文件处理模式：**

**核心思路：** 用状态变量跟踪当前 section

```bash
awk '
    /^\[section\]/ {in_sec=1; next}   # 进入目标 section
    /^\[/          {in_sec=0}          # 离开 section
    in_sec         {process...}        # 在 section 内处理
'
```

**应用场景：**
- 提取特定 section 的配置
- 验证配置文件格式
- 转换配置格式

---

### Block-Based Processing

```
+------------------------------------------------------------------+
|              BLOCK-BASED PROCESSING                               |
+------------------------------------------------------------------+
|                                                                   |
|  PROBLEM: Process multi-line records separated by blank lines     |
|                                                                   |
|  INPUT (contacts.txt):                                            |
|  Name: John Doe                                                   |
|  Email: john@example.com                                          |
|  Phone: 555-1234                                                  |
|                                                                   |
|  Name: Jane Smith                                                 |
|  Email: jane@example.com                                          |
|  Phone: 555-5678                                                  |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  SOLUTION: Set RS to empty string (paragraph mode)                |
|  awk 'BEGIN {RS=""; FS="\n"}                                      |
|       {                                                           |
|           for (i=1; i<=NF; i++) {                                 |
|               split($i, kv, ": ")                                 |
|               data[kv[1]] = kv[2]                                 |
|           }                                                       |
|           print data["Name"], data["Email"]                       |
|           delete data                                             |
|       }' contacts.txt                                             |
|                                                                   |
|  Output:                                                          |
|  John Doe john@example.com                                        |
|  Jane Smith jane@example.com                                      |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  HOW IT WORKS:                                                    |
|  +----------------------------------------------------------+    |
|  |  RS=""  : Records separated by blank lines               |    |
|  |  FS="\n": Fields are individual lines within record      |    |
|  |                                                          |    |
|  |  $0 = "Name: John Doe\nEmail: john@...\nPhone: 555-..."  |    |
|  |  $1 = "Name: John Doe"                                   |    |
|  |  $2 = "Email: john@example.com"                          |    |
|  |  $3 = "Phone: 555-1234"                                  |    |
|  +----------------------------------------------------------+    |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  EXAMPLE: Filter blocks by condition                              |
|  awk 'BEGIN {RS=""; FS="\n"; ORS="\n\n"}                          |
|       /Email:.*@company\.com/ {print}' contacts.txt               |
|                                                                   |
|  (Prints only records with company email)                         |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**多行记录处理：**

**关键设置：**
```bash
BEGIN {
    RS=""      # 空行分隔记录
    FS="\n"    # 行内换行分隔字段
}
```

**效果：**
- 每个"段落"成为一条记录
- 段落内的每一行成为一个字段

**典型应用：**
- 解析联系人/地址簿格式
- 处理 /etc/passwd 风格的块数据
- 提取多行日志条目

---

## 5.3 awk as Pipeline Glue

```
+------------------------------------------------------------------+
|                AWK IN PIPELINES                                   |
+------------------------------------------------------------------+
|                                                                   |
|  WHY AWK IN PIPELINES:                                            |
|  - Transforms structured data between commands                    |
|  - Extracts specific columns for next tool                        |
|  - Adds conditional filtering with data extraction                |
|  - Reformats output for downstream processing                     |
|                                                                   |
|  +----------------------------------------------------------+    |
|  |                                                          |    |
|  |    cmd1  -->  awk '{transform}'  -->  cmd2  -->  ...     |    |
|  |      |              |                   |                |    |
|  |   Raw data    Field extraction    Further processing     |    |
|  |               Filtering                                  |    |
|  |               Reformatting                               |    |
|  |                                                          |    |
|  +----------------------------------------------------------+    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**AWK 在管道中的角色：**
- 从上游命令提取特定字段
- 过滤和转换数据
- 为下游命令格式化输出
- 在多个命令之间做"胶水"工作

---

### Process Monitoring

```
+------------------------------------------------------------------+
|              PROCESS MONITORING                                   |
+------------------------------------------------------------------+
|                                                                   |
|  ps aux output fields:                                            |
|  $1=USER $2=PID $3=%CPU $4=%MEM $5=VSZ $6=RSS $7=TTY              |
|  $8=STAT $9=START $10=TIME $11...=COMMAND                         |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 1: High CPU processes                                       |
|  --------------------------                                       |
|  ps aux | awk 'NR>1 && $3>50 {print $1, $2, $3, $11}'             |
|                                                                   |
|  Output: processes using >50% CPU                                 |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 2: Total memory by user                                     |
|  ----------------------------                                     |
|  ps aux | awk 'NR>1 {mem[$1]+=$6}                                 |
|                END {for (u in mem)                                |
|                    printf "%s: %.2f MB\n", u, mem[u]/1024}'       |
|                | sort -t: -k2 -rn                                 |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 3: Count processes per user                                 |
|  --------------------------------                                 |
|  ps aux | awk 'NR>1 {count[$1]++}                                 |
|                END {for (u in count) print u, count[u]}'          |
|                | sort -k2 -rn                                     |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 4: Find zombie processes                                    |
|  ----------------------------                                     |
|  ps aux | awk '$8 ~ /Z/ {print $2, $11}'                          |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 5: Kill processes matching pattern                          |
|  --------------------------------------                           |
|  ps aux | awk '/pattern/ && !/awk/ {print $2}' | xargs kill       |
|                                                                   |
|  Note: !/awk/ excludes the awk process itself                     |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**进程监控常用命令：**

1. **找高 CPU 进程**
   ```bash
   ps aux | awk 'NR>1 && $3>50 {print $2,$11}'
   ```

2. **按用户统计内存**
   ```bash
   ps aux | awk 'NR>1{m[$1]+=$6} END{for(u in m) print u,m[u]/1024,"MB"}'
   ```

3. **统计进程数**
   ```bash
   ps aux | awk 'NR>1{c[$1]++} END{for(u in c) print u,c[u]}'
   ```

4. **找僵尸进程**
   ```bash
   ps aux | awk '$8~/Z/'
   ```

---

### Disk Space Analysis

```
+------------------------------------------------------------------+
|              DISK SPACE ANALYSIS                                  |
+------------------------------------------------------------------+
|                                                                   |
|  df -h output:                                                    |
|  Filesystem      Size  Used Avail Use% Mounted on                 |
|  /dev/sda1       100G   60G   40G  60% /                          |
|  /dev/sdb1       500G  400G  100G  80% /data                      |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 1: Find filesystems over 80% full                           |
|  --------------------------------------                           |
|  df -h | awk 'NR>1 {                                              |
|      gsub(/%/, "", $5)                                            |
|      if ($5+0 > 80) print $6, $5"%"                               |
|  }'                                                               |
|                                                                   |
|  Note: gsub(/%/,"") removes % for numeric comparison              |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 2: Alert format                                             |
|  -------------------                                              |
|  df -h | awk 'NR>1 {                                              |
|      gsub(/%/, "", $5)                                            |
|      if ($5 > 90) level="CRITICAL"                                |
|      else if ($5 > 80) level="WARNING"                            |
|      else next                                                    |
|      printf "[%s] %s is %s%% full\n", level, $6, $5               |
|  }'                                                               |
|                                                                   |
|  Output:                                                          |
|  [WARNING] /data is 85% full                                      |
|  [CRITICAL] /backup is 95% full                                   |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 3: Total disk usage                                         |
|  ------------------------                                         |
|  df | awk 'NR>1 && !/tmpfs/ {                                     |
|      total += $2                                                  |
|      used += $3                                                   |
|  }                                                                |
|  END {printf "Total: %.1fG, Used: %.1fG (%.1f%%)\n",              |
|       total/1024/1024, used/1024/1024, used*100/total}'           |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**磁盘分析技巧：**

1. **找高占用分区**
   ```bash
   df -h | awk 'NR>1{gsub(/%/,"",$5); if($5>80) print $6,$5"%"}'
   ```
   - `gsub(/%/,"")` 去掉百分号以便数值比较

2. **生成告警**
   ```bash
   df -h | awk '{
       gsub(/%/,"",$5)
       if($5>90) print "[CRITICAL]",$6
       else if($5>80) print "[WARNING]",$6
   }'
   ```

3. **排除虚拟文件系统**
   ```bash
   df | awk '!/tmpfs|devtmpfs/ {...}'
   ```

---

### Network Information

```
+------------------------------------------------------------------+
|              NETWORK INFORMATION                                  |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 1: Extract IP addresses                                     |
|  ----------------------------                                     |
|  ip addr | awk '/inet / && !/127.0.0.1/ {                         |
|      split($2, ip, "/")                                           |
|      print ip[1]                                                  |
|  }'                                                               |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 2: Show interface and IP                                    |
|  -----------------------------                                    |
|  ip addr | awk '                                                  |
|      /^[0-9]+:/ {iface = $2; gsub(/:/, "", iface)}                |
|      /inet / && !/127/ {                                          |
|          split($2, ip, "/")                                       |
|          print iface, ip[1]                                       |
|      }'                                                           |
|                                                                   |
|  Output:                                                          |
|  eth0 192.168.1.100                                               |
|  docker0 172.17.0.1                                               |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 3: Parse netstat/ss output                                  |
|  -------------------------------                                  |
|  # Count connections by state                                     |
|  ss -tan | awk 'NR>1 {count[$1]++}                                |
|           END {for (s in count) print s, count[s]}'               |
|                                                                   |
|  Output:                                                          |
|  LISTEN 15                                                        |
|  ESTAB 42                                                         |
|  TIME-WAIT 8                                                      |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK 4: Top talkers by connection count                          |
|  ---------------------------------------                          |
|  ss -tan | awk 'NR>1 && $1=="ESTAB" {                             |
|      split($5, addr, ":")                                         |
|      count[addr[1]]++                                             |
|  }                                                                |
|  END {for (ip in count) print count[ip], ip}'                     |
|      | sort -rn | head -10                                        |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**网络信息提取：**

1. **提取 IP 地址**
   ```bash
   ip addr | awk '/inet /{split($2,a,"/"); print a[1]}'
   ```

2. **统计连接状态**
   ```bash
   ss -tan | awk 'NR>1{c[$1]++} END{for(s in c) print s,c[s]}'
   ```

3. **找连接最多的远程 IP**
   ```bash
   ss -tan | awk '/ESTAB/{split($5,a,":"); c[a[1]]++} END{for(i in c) print c[i],i}' | sort -rn
   ```

---

## 5.4 Safe Text Transformation

```
+------------------------------------------------------------------+
|              SAFE TEXT TRANSFORMATION                             |
+------------------------------------------------------------------+
|                                                                   |
|  WHY AWK IS SAFER THAN sed FOR COMPLEX EDITS:                     |
|  +----------------------------------------------------------+    |
|  |                                                          |    |
|  |  sed:  Pattern-based, operates on each line              |    |
|  |        No concept of fields or structured data           |    |
|  |        Tricky to handle conditional multi-line edits     |    |
|  |                                                          |    |
|  |  awk:  Field-aware, can accumulate context               |    |
|  |        Supports variables and complex conditions         |    |
|  |        Natural for record-based transformations          |    |
|  |                                                          |    |
|  +----------------------------------------------------------+    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**AWK vs sed 在转换任务中的对比：**

| 特性 | sed | awk |
|------|-----|-----|
| 字段感知 | 否 | 是 |
| 条件逻辑 | 有限 | 完整 |
| 状态保持 | 困难 | 自然 |
| 多行处理 | 复杂 | 简单 |

**原则：** 简单替换用 sed，涉及字段/条件/状态用 awk。

---

### Conditional Block Modification

```
+------------------------------------------------------------------+
|              CONDITIONAL BLOCK EDITING                            |
+------------------------------------------------------------------+
|                                                                   |
|  TASK: Comment out a block only if condition matches              |
|                                                                   |
|  INPUT (config.txt):                                              |
|  server web1 {                                                    |
|      listen 80;                                                   |
|      root /var/www;                                               |
|  }                                                                |
|                                                                   |
|  server api1 {                                                    |
|      listen 8080;                                                 |
|      root /var/api;                                               |
|  }                                                                |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  SOLUTION: Comment out server blocks listening on 8080            |
|                                                                   |
|  awk '                                                            |
|      /^server/ {                                                  |
|          block = $0 "\n"                                          |
|          in_block = 1                                             |
|          next                                                     |
|      }                                                            |
|      in_block {                                                   |
|          block = block $0 "\n"                                    |
|          if (/listen 8080/) should_comment = 1                    |
|          if (/^}/) {                                              |
|              if (should_comment) {                                |
|                  gsub(/^/, "# ", block)                           |
|                  gsub(/\n/, "\n# ", block)                        |
|              }                                                    |
|              printf "%s", block                                   |
|              in_block = 0                                         |
|              should_comment = 0                                   |
|              block = ""                                           |
|          }                                                        |
|          next                                                     |
|      }                                                            |
|      {print}                                                      |
|  ' config.txt                                                     |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**条件性块编辑的思路：**

1. **缓冲整个块**：遇到块开始时，开始收集行
2. **检测条件**：在块内查找条件（如 `listen 8080`）
3. **块结束时处理**：根据条件决定是否修改
4. **输出结果**：打印（可能修改过的）块

**这种模式 sed 很难实现，因为需要：**
- 跨行状态保持
- 条件判断
- 批量修改缓冲内容

---

### Column-Safe Modifications

```
+------------------------------------------------------------------+
|              COLUMN-SAFE MODIFICATIONS                            |
+------------------------------------------------------------------+
|                                                                   |
|  PROBLEM: Modify specific column without breaking others          |
|                                                                   |
|  INPUT (data.csv):                                                |
|  name,price,quantity                                              |
|  apple,1.50,100                                                   |
|  banana,0.75,200                                                  |
|  cherry,2.00,50                                                   |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK: Double the price column (column 2)                         |
|  ----------------------------------------                         |
|  awk -F, 'BEGIN{OFS=","}                                          |
|       NR==1 {print; next}                                         |
|       {$2 = $2 * 2; print}' data.csv                              |
|                                                                   |
|  Output:                                                          |
|  name,price,quantity                                              |
|  apple,3,100                                                      |
|  banana,1.5,200                                                   |
|  cherry,4,50                                                      |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK: Add 10% to prices over 1.00                                |
|  ---------------------------------                                |
|  awk -F, 'BEGIN{OFS=","}                                          |
|       NR==1 {print; next}                                         |
|       {                                                           |
|           if ($2 > 1.00) $2 = $2 * 1.10                           |
|           print                                                   |
|       }' data.csv                                                 |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TASK: Conditional column formatting                              |
|  -----------------------------------                              |
|  awk -F, 'BEGIN{OFS=","}                                          |
|       NR==1 {print; next}                                         |
|       {                                                           |
|           $2 = sprintf("$%.2f", $2)   # Format as currency        |
|           print                                                   |
|       }' data.csv                                                 |
|                                                                   |
|  Output:                                                          |
|  name,price,quantity                                              |
|  apple,$1.50,100                                                  |
|  banana,$0.75,200                                                 |
|  cherry,$2.00,50                                                  |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**列安全修改的关键：**

```bash
awk -F, 'BEGIN{OFS=","} {$2=NEW_VALUE; print}'
```

1. **设置 FS 和 OFS**：确保分隔符一致
2. **保留表头**：`NR==1 {print; next}`
3. **只修改目标列**：其他列自动保留

**优势：**
- sed 替换可能误伤其他列
- awk 明确操作特定字段
- 条件修改更安全

---

### Adding/Removing Columns

```
+------------------------------------------------------------------+
|              ADDING AND REMOVING COLUMNS                          |
+------------------------------------------------------------------+
|                                                                   |
|  INPUT:                                                           |
|  name,price,quantity                                              |
|  apple,1.50,100                                                   |
|  banana,0.75,200                                                  |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  ADD COLUMN: Calculate total (price * quantity)                   |
|  awk -F, 'BEGIN{OFS=","}                                          |
|       NR==1 {print $0, "total"; next}                             |
|       {print $0, $2*$3}' data.csv                                 |
|                                                                   |
|  Output:                                                          |
|  name,price,quantity,total                                        |
|  apple,1.50,100,150                                               |
|  banana,0.75,200,150                                              |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  REMOVE COLUMN: Drop column 2 (price)                             |
|  awk -F, 'BEGIN{OFS=","}                                          |
|       {                                                           |
|           for (i=1; i<=NF; i++)                                   |
|               if (i != 2)                                         |
|                   printf "%s%s", $i, (i<NF && i+1!=2 ? OFS : "")  |
|           print ""                                                |
|       }' data.csv                                                 |
|                                                                   |
|  SIMPLER (if you know column count):                              |
|  awk -F, 'BEGIN{OFS=","} {print $1, $3}' data.csv                 |
|                                                                   |
|  Output:                                                          |
|  name,quantity                                                    |
|  apple,100                                                        |
|  banana,200                                                       |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  INSERT COLUMN: Add index as first column                         |
|  awk -F, 'BEGIN{OFS=","}                                          |
|       NR==1 {print "id", $0; next}                                |
|       {print NR-1, $0}' data.csv                                  |
|                                                                   |
|  Output:                                                          |
|  id,name,price,quantity                                           |
|  1,apple,1.50,100                                                 |
|  2,banana,0.75,200                                                |
|                                                                   |
+------------------------------------------------------------------+
```

**中文解释：**

**列操作技巧：**

| 操作 | 方法 |
|------|------|
| 添加列 | `{print $0, NEW_COL}` |
| 删除列 | 只打印需要的列 |
| 插入列 | `{print NEW_COL, $0}` |
| 计算列 | `{print $0, $2*$3}` |

**注意事项：**
- 设置 `OFS` 以保持格式一致
- 表头行需特殊处理
- 复杂删除可用循环跳过指定列
