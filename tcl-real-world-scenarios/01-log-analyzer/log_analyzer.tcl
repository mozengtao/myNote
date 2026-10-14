#!/usr/bin/env tclsh
# ============================================================
# 场景 1：日志文件分析与统计报表
#
# 运行方式：
#   tclsh log_analyzer.tcl
#
# 对应 tcl_mental_model.md 的章节：
#   - 10  String
#   - 11-12 List
#   - 13-15 Dict / Nested Dict
#   - 21  foreach
#   - 22  foreach + Dict
#   - 23  split / join
#   - 24  format
#   - 26  regexp
#   - 27  switch
#   - 28-30 String -> List -> Dict -> Business Logic 数据流
#   - 34  proc
# ============================================================

# ------------------------------------------------------------
# 1. 模拟日志数据源
#
# 真实项目中，这一段通常是：
#   set fp [open $logfile r]
#   set rawLog [read $fp]
#   close $fp
#
# 这里为了让示例可以脱离文件系统直接运行，把日志内容写成一个
# 多行字符串（{} 阻止 substitution，原样保留文本）。
# ------------------------------------------------------------
set rawLog {
2024-01-15 10:23:01 [INFO] Server started on port 8080
2024-01-15 10:23:05 [INFO] Connected to database
2024-01-15 10:24:12 [WARN] Slow query detected: 1200ms
2024-01-15 10:25:00 [ERROR] Failed to connect to cache: timeout
2024-01-15 10:25:30 [INFO] Retrying cache connection
2024-01-15 10:25:31 [INFO] Cache connected
2024-01-15 10:26:40 [ERROR] Unhandled exception in handler /api/users
2024-01-15 10:27:00 [WARN] Memory usage above 80%
2024-01-15 10:28:15 [INFO] Request processed in 45ms
}

# ------------------------------------------------------------
# 2. 解析单行日志
#
# 用 regexp 把一行文本拆成 timestamp / level / message 三个字段，
# 打包成一个 Dict 返回，调用方用 dict get 按字段名取值，
# 而不用记位置（对比 lindex $fields 0 这种位置依赖的写法）。
#
# 解析失败（格式不符）时返回空 dict，调用方据此跳过该行。
# ------------------------------------------------------------
proc parseLogLine {line} {
    set pattern {^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) \[(\w+)\] (.*)$}
    if {![regexp $pattern $line -> timestamp level message]} {
        return [dict create]
    }
    return [dict create timestamp $timestamp level $level message $message]
}

# ------------------------------------------------------------
# 3. 遍历所有行，做统计
#
# 用两个 Dict 分别记录：
#   counts   : level -> 出现次数
#   messages : level -> 具体消息组成的 List（只记录 WARN / ERROR）
#
# 最终把这两个 Dict 装进一个外层 Dict 返回（Nested Dict）。
# ------------------------------------------------------------
proc analyzeLog {rawText} {
    set counts   [dict create]
    set messages [dict create]

    set lines [split [string trim $rawText] "\n"]
    foreach line $lines {
        set entry [parseLogLine $line]
        if {[dict size $entry] == 0} {
            continue
        }

        set level [dict get $entry level]

        # counts(level) += 1
        dict incr counts $level

        # 只对 WARN / ERROR 收集详细消息，INFO 只计数、不记录细节
        switch -- $level {
            WARN - ERROR {
                dict lappend messages $level [dict get $entry message]
            }
            default {
                # INFO、DEBUG 等级别：计数已经在上面完成，这里不用做事
            }
        }
    }

    return [dict create counts $counts messages $messages]
}

# ------------------------------------------------------------
# 4. 把统计结果格式化成报表字符串
# ------------------------------------------------------------
proc formatReport {stats} {
    set report ""
    append report "===== Log Summary =====\n"

    set counts [dict get $stats counts]
    # 固定顺序输出，而不是依赖 dict 的内部遍历顺序
    foreach level {ERROR WARN INFO} {
        if {![dict exists $counts $level]} {
            continue
        }
        append report [format "%-6s : %3d 条\n" $level [dict get $counts $level]]
    }

    append report "\n===== WARN / ERROR 详情 =====\n"
    set messages [dict get $stats messages]
    foreach level {ERROR WARN} {
        if {![dict exists $messages $level]} {
            continue
        }
        append report "\[$level\]\n"
        foreach msg [dict get $messages $level] {
            append report "  - $msg\n"
        }
    }

    return $report
}

# ------------------------------------------------------------
# 5. 主流程
# ------------------------------------------------------------
set stats [analyzeLog $rawLog]
puts [formatReport $stats]
