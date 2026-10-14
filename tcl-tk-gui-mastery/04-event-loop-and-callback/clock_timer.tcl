#!/usr/bin/env wish
#
# clock_timer.tcl —— Layer 4 示例 C：Timer Callback（after）与周期性调度
#
# after ms script 是"一次性"的定时器：
#   注册之后，script 只会在大约 ms 毫秒之后被调用一次，然后这个定时器就结束了。
#   要做出"每秒走一次"的时钟效果，必须让 script 在自己执行的最后一步，
#   重新调用一次 after，把"下一次触发"重新安排进事件循环——这叫"递归调度"。
#   这和写一个 while 死循环去反复检查时间完全不同：
#   递归调度每次都会"返回"，把控制权交还给事件循环；
#   阻塞循环永远不返回，会把整个程序冻住（见 README "Common Failure"）。
#
# 运行方式：
#   有图形环境：   wish clock_timer.tcl
#   headless 环境： tclsh clock_timer.tcl  （只能校验到 package require Tk 这一步）
#
# 本文件里的 formatNow 是一个完全不依赖 Tk、不依赖事件循环的纯逻辑 proc，
# 可以在 tclsh 里单独定义并调用来验证逻辑是否正确，
# 验证方式见 README.md 的 "Execution Walkthrough" 一节。

package require Tk

wm title . "Clock Timer Demo"

# --------------------------------------------------------------------------
# 纯逻辑部分：只用核心 Tcl 的 clock 命令，不涉及任何 widget / 事件循环
# --------------------------------------------------------------------------
proc formatNow {} {
    # clock seconds  —— 返回当前时间，Unix epoch 整数秒（自 1970-01-01 起的秒数）
    # clock format   —— 把整数秒转换成人类可读的字符串，-format 控制输出格式
    #                    %Y-%m-%d 年-月-日，%H:%M:%S 时:分:秒（24 小时制）
    return [clock format [clock seconds] -format "%Y-%m-%d %H:%M:%S"]
}

# --------------------------------------------------------------------------
# 界面状态
# --------------------------------------------------------------------------
set ::clockText   [formatNow]
set ::timerStatus "stopped"

# afterId 为空字符串表示"当前没有已安排的定时器"。
# 这是避免"重复启动定时器"故障的关键状态变量：
# 每次准备安排新的 after 之前，都必须能判断"是不是已经有一个在排队了"。
set ::timerAfterId ""

label .clock  -textvariable ::clockText   -font {Helvetica 20} -padx 20 -pady 10
label .status -textvariable ::timerStatus -foreground gray
pack .clock
pack .status -pady {0 10}

# --------------------------------------------------------------------------
# tick：周期性定时器回调本体
# --------------------------------------------------------------------------
proc tick {} {
    set ::clockText [formatNow]
    # 关键点：tick 在更新完界面状态之后，"重新安排自己"作为下一次触发，
    # 而不是用 while/for 阻塞等待。
    # after 1000 tick 把"1 秒后调用 tick"这件事注册进事件循环的定时器队列，
    # 然后 tick 立刻返回——把控制权还给事件循环；
    # 大约 1 秒后，事件循环才会真正地把 tick 当作一条命令调用一次。
    # 这里必须把新的 afterId 存回 ::timerAfterId，否则 stopTimer 就找不到
    # "最新这一次"排队的定时器，取消不掉。
    set ::timerAfterId [after 1000 tick]
}

# --------------------------------------------------------------------------
# startTimer / stopTimer：对外暴露的状态切换操作，绑定在按钮上
# --------------------------------------------------------------------------
proc startTimer {} {
    # 防止重复启动：如果 timerAfterId 不是空字符串，说明已经有一个 tick 排队中了。
    # 这时如果再调用一次 tick（或者再 after 1000 tick 一次），
    # 就会同时存在两条独立的定时器链，各自每秒触发一次，
    # 界面上的时钟会变成"一秒跳两次甚至更多次"——这正是本教程第 12 章
    # （故障排查）要专门分析的"after 定时器重复启动"问题，这里先把正确的
    # 防重复写法留给你。
    if {$::timerAfterId ne ""} {
        return
    }
    set ::timerStatus "running"
    tick
}

proc stopTimer {} {
    # after cancel 取消一个"已经安排、还没有触发"的 after 事件。
    # 必须用 startTimer/tick 里存下来的 afterId 才能精确取消对应的那一次安排；
    # 取消之后要把 timerAfterId 清回空字符串，
    # 这样 startTimer 才知道"现在确实没有定时器在跑了，可以重新开始"。
    if {$::timerAfterId ne ""} {
        after cancel $::timerAfterId
        set ::timerAfterId ""
        set ::timerStatus "stopped"
    }
}

button .start -text "Start" -command startTimer
button .stop  -text "Stop"  -command stopTimer
pack .start -side left -padx 10 -pady 10
pack .stop  -side left -padx 10 -pady 10

button .quit -text "Quit" -command {destroy .}
pack .quit -side left -padx 10 -pady 10
