#!/usr/bin/env tclsh
#
# 09-after-timer-restart.tcl —— Layer 9 故障 9：
#   after 定时器重复启动（没有记录/取消已存在的 after id）
#
# after 本身是 Tcl 核心命令，不属于 Tk，只要有事件循环在跑
# （vwait，或者 wish 的隐式事件循环）就能完整工作。
# 所以这个故障可以【完全】用 tclsh + vwait 复现，不需要 Tk，
# 不需要任何图形环境。
#
# 运行方式：
#   tclsh 09-after-timer-restart.tcl

puts "=== 错误版本：每次调用 startPollingWrong 都会新开一个 after 定时器 ==="
puts ""

set tickCountWrong 0

proc tickWrong {} {
    global tickCountWrong
    incr tickCountWrong
    puts "  \[wrong\] tick #$tickCountWrong"
    # 每次 tick 自己再 after 一次，制造"周期定时器"的效果——
    # 但注意：这个 afterId 同样没有被记录下来。
    after 100 tickWrong
}

# startPollingWrong 模拟"用户点击了一个 Start 按钮"：
# 它没有检查、也没有记录"是不是已经有一个定时器在跑了"，
# 每调用一次就无条件新开一个 after 100 tickWrong。
proc startPollingWrong {} {
    after 100 tickWrong
}

puts "-- 模拟用户连续点击 3 次 Start 按钮（错误版本） --"
startPollingWrong
startPollingWrong
startPollingWrong

puts ""
puts "此刻 after info 应该显示 3 个独立的、都指向 tickWrong 的待执行事件："
foreach id [after info] {
    puts "  pending: $id -> [after info $id]"
}
puts "（如果看到 3 行 pending，说明 3 个定时器已经同时在跑，"
puts " 之后 tickWrong 会被并发触发 3 次、6 次、9 次……越滚越多）"
puts ""

# 让事件循环跑一小段时间，观察 tickWrong 是不是真的被并发调用了多次。
after 350 {set ::doneWrong 1}
vwait ::doneWrong

puts ""
puts "350ms 内 tickWrong 一共被调用了 $tickCountWrong 次"
puts "（正常意义上的单个 100ms 定时器，350ms 内大约应该只触发 3~4 次；"
puts " 这里因为一开始就重复启动了 3 份，实际触发次数会明显更多，"
puts " 而且还会越积越多，这正是\"重复启动\"的典型症状）"

# 清理：防止下面的"正确版本"演示时，这些历史定时器继续干扰输出。
foreach id [after info] {
    after cancel $id
}

puts ""
puts "=========================================================="
puts "=== 正确版本：用一个全局变量记录 afterId，重启前先 cancel ==="
puts "=========================================================="
puts ""

set tickCountRight 0
set pollAfterId ""

proc tickRight {} {
    global tickCountRight pollAfterId
    incr tickCountRight
    puts "  \[right\] tick #$tickCountRight"
    set pollAfterId [after 100 tickRight]
}

proc startPollingRight {} {
    global pollAfterId
    # 关键修复：启动新定时器之前，先检查并取消旧的（如果存在）。
    if {$pollAfterId ne ""} {
        after cancel $pollAfterId
        puts "  (检测到已有定时器 $pollAfterId，先取消它再重新启动)"
    }
    set pollAfterId [after 100 tickRight]
}

puts "-- 模拟用户连续点击 3 次 Start 按钮（正确版本） --"
startPollingRight
startPollingRight
startPollingRight

puts ""
puts "此刻 after info 应该只显示 1 个待执行事件："
foreach id [after info] {
    puts "  pending: $id -> [after info $id]"
}

after 350 {set ::doneRight 1}
vwait ::doneRight

puts ""
puts "350ms 内 tickRight 一共被调用了 $tickCountRight 次（应该是正常的 3~4 次左右）"

# 最终清理，避免进程退出前还有残留的 after 事件。
if {$pollAfterId ne ""} {
    after cancel $pollAfterId
}
