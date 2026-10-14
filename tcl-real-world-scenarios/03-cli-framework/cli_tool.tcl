#!/usr/bin/env tclsh
# ============================================================
# 场景 3：命令行子命令框架
#
# 真实使用方式：
#   tclsh cli_tool.tcl greet Morris
#   tclsh cli_tool.tcl sum 1 2 3 4
#   tclsh cli_tool.tcl counter 5
#   tclsh cli_tool.tcl retry-demo
#
# 不带参数直接运行 `tclsh cli_tool.tcl` 时，会依次演示全部子命令。
#
# 对应 tcl_mental_model.md 的章节：
#   - 21  foreach  /  27  switch
#   - 34  proc（含 args 可变参数）
#   - 35  upvar（实现写调用者变量的工具 proc）
#   - 36  uplevel（实现自定义控制结构 retry）
#   - 38  namespace（组织子命令）
#   - 39  package（provide，模块化的第一步）
#   - {*}（argument expansion，作为 eval 的更安全替代）
# ============================================================

namespace eval cli {
    namespace export run
}

# ------------------------------------------------------------
# upvar 示例：实现一个通用的"写调用者变量"工具 proc。
#
# 对比 tcl_mental_model.md 第 35 节的 setValue 例子：这里的
# cli::bump 本质上是一个自定义的、带步长的 incr。
# ------------------------------------------------------------
proc cli::bump {varName {step 1}} {
    upvar 1 $varName ref
    if {![info exists ref]} {
        set ref 0
    }
    incr ref $step
    return $ref
}

# ------------------------------------------------------------
# uplevel 示例：实现一个"看起来像内置语法"的自定义控制结构 retry。
#
# retry n { script } 在调用者的作用域里最多执行 script n 次，
# 一旦不报错就立即返回其结果；全部失败则把最后一次错误抛出。
#
# 用 uplevel（而不是 eval）执行 script，是为了让 script 里出现的
# 变量读写发生在"调用 retry 的那一层"，而不是 retry 自己的局部作用域。
# ------------------------------------------------------------
proc retry {times script} {
    set result ""
    for {set i 1} {$i <= $times} {incr i} {
        if {![catch {uplevel 1 $script} result]} {
            return $result
        }
        puts "  (retry $i/$times failed: $result)"
    }
    error "retry: exceeded $times attempts, last error: $result"
}

# ------------------------------------------------------------
# 子命令实现
# ------------------------------------------------------------

proc cli::cmdGreet {args} {
    if {[llength $args] < 1} {
        error "usage: greet <name>"
    }
    set name [lindex $args 0]
    puts "Hello, $name!"
}

proc cli::cmdSum {args} {
    set total 0
    foreach n $args {
        incr total $n
    }
    puts "sum = $total"
}

proc cli::cmdCounter {args} {
    set n 1
    if {[llength $args] >= 1} {
        set n [lindex $args 0]
    }

    set counter 0
    for {set i 0} {$i < $n} {incr i} {
        # 每次调用 cli::bump，counter 在"调用者（这里就是本 proc）
        # 的作用域"里被真正修改——这正是 upvar 的意义。
        cli::bump counter
    }
    puts "counter = $counter"
}

proc cli::cmdRetryDemo {args} {
    set ::cli::attempt 0

    set outcome [retry 3 {
        incr ::cli::attempt
        puts "  attempt $::cli::attempt"
        if {$::cli::attempt < 3} {
            error "simulated transient failure"
        }
        set result "succeeded on attempt $::cli::attempt"
    }]

    puts $outcome
}

# ------------------------------------------------------------
# 子命令分发：switch 按第一个参数决定调用哪个 cmd*
# ------------------------------------------------------------
proc cli::run {argv} {
    if {[llength $argv] < 1} {
        puts "usage: cli_tool.tcl <greet|sum|counter|retry-demo> \[args...\]"
        return
    }

    set subcommand [lindex $argv 0]
    set rest [lrange $argv 1 end]

    switch -- $subcommand {
        greet {
            cli::cmdGreet {*}$rest
        }
        sum {
            cli::cmdSum {*}$rest
        }
        counter {
            cli::cmdCounter {*}$rest
        }
        retry-demo {
            cli::cmdRetryDemo {*}$rest
        }
        default {
            puts "unknown subcommand: $subcommand"
        }
    }
}

# 把整个工具声明成一个可被其他脚本 `package require clitool` 复用的模块。
# （本例作为单文件脚本运行，省略了配套的 pkgIndex.tcl；真正打包分发时，
#  通常会把本文件放进独立目录，再写一个 pkgIndex.tcl 指向它。）
package provide clitool 1.0

# ============================================================
# 主流程
# ============================================================
if {[llength $::argv] > 0} {
    cli::run $::argv
} else {
    puts "===== demo: greet ====="
    cli::run {greet Morris}

    puts "\n===== demo: sum ====="
    cli::run {sum 1 2 3 4}

    puts "\n===== demo: counter ====="
    cli::run {counter 5}

    puts "\n===== demo: retry-demo ====="
    cli::run {retry-demo}

    puts "\n===== demo: unknown subcommand ====="
    cli::run {bogus}
}
