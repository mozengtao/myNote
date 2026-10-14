#!/usr/bin/env tclsh
# ============================================================
# 场景 4：插件化任务执行引擎
#
# 运行方式：
#   tclsh task_runner.tcl
#
# 对应 tcl_mental_model.md 的章节：
#   - 13-15 Dict / Nested Dict（任务注册表）
#   - 21  foreach
#   - 37  eval（把动态拼出来的命令 List 当作命令执行）
#   - 38  namespace（每个插件独立的命名空间，互不冲突）
#   - 39  package（provide/require 的思想：本例用 source 做简化演示）
#   - catch（mental model 未单独讲，这里作为必要的错误处理补充）
# ============================================================

# ------------------------------------------------------------
# 1. 动态加载插件目录下的所有插件文件
#
# 主程序不需要在代码里写死"有哪些插件"，新增一个插件只需要往
# plugins/ 目录里放一个新文件——这是"插件化"的核心：主程序和
# 插件之间只通过约定（namespace::run）耦合，不是直接的代码依赖。
# ------------------------------------------------------------
set pluginDir [file join [file dirname [info script]] plugins]
foreach pluginFile [lsort [glob -nocomplain -directory $pluginDir *.tcl]] {
    source $pluginFile
}

# ------------------------------------------------------------
# 2. 任务注册表：一个 Dict，描述"要执行哪些任务，每个任务用哪个
#    插件、传什么参数"。
#
#    taskName -> dict(plugin = 插件 namespace, params = 传给插件的 Dict)
# ------------------------------------------------------------
set tasks [dict create \
    backup-db    [dict create \
        plugin ::plugins::backup \
        params [dict create source users.db target /backup/users.db]] \
    backup-logs  [dict create \
        plugin ::plugins::backup \
        params [dict create source app.log target /backup/app.log.bak]] \
    daily-report [dict create \
        plugin ::plugins::report \
        params [dict create title "Daily Report" rows {10 20 30 40}]] \
    broken-task  [dict create \
        plugin ::plugins::nonexistent \
        params [dict create]] \
]

# ------------------------------------------------------------
# 3. 执行单个任务
#
# 把 "插件 namespace + run" 和 "参数" 拼成一条命令（List 形式），
# 用 eval 执行。因为 runCmd 本身已经是一个结构良好的 List
# （[list cmdName arg1 arg2 ...]），eval 在这里只是把它当作一条
# 命令调用，不存在拼接字符串带来的注入风险。
#
# 用 catch 包住，保证"某一个任务失败"不会让整个任务执行引擎崩溃。
# ------------------------------------------------------------
proc runTask {spec} {
    set pluginNs [dict get $spec plugin]
    set params   [dict get $spec params]

    set runCmd [list ${pluginNs}::run $params]

    if {[catch {eval $runCmd} message]} {
        return [dict create ok 0 message $message]
    }
    return [dict create ok 1 message $message]
}

# ------------------------------------------------------------
# 4. 依次执行所有任务，收集结果
# ------------------------------------------------------------
set results [dict create]
foreach name [dict keys $tasks] {
    dict set results $name [runTask [dict get $tasks $name]]
}

# ------------------------------------------------------------
# 5. 汇总报表
# ------------------------------------------------------------
set okCount 0
set failCount 0

puts "===== Task Execution Report ====="
foreach name [dict keys $tasks] {
    set r [dict get $results $name]
    if {[dict get $r ok]} {
        set status "OK"
        incr okCount
    } else {
        set status "FAIL"
        incr failCount
    }
    puts [format "%-4s | %-12s : %s" $status $name [dict get $r message]]
}

puts "\n总计：$okCount 个成功，$failCount 个失败"
