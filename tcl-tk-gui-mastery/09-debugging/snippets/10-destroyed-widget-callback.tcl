#!/usr/bin/env tclsh
#
# 10-destroyed-widget-callback.tcl —— Layer 9 故障 10：
#   widget 已销毁但回调仍然访问其路径（invalid command name 错误）
#
# 根本原因和"widget"这个概念本身关系不大：在 Tcl 里，每个 widget
# 就是一个普通的 Tcl 命令（Layer 1 的核心认知）；destroy 一个 widget，
# 本质上就是把这个同名命令从解释器里删掉（等价于 rename .foo {}）。
# 如果之前用 after/bind 注册了一段"将来才执行"的脚本，而那段脚本里
# 引用了这个 widget 的路径，等它真正被执行的时候，这个命令可能已经
# 不存在了——报错信息就是 Tcl 核心的 "invalid command name"，
# 和是否有 Tk 没有必然关系（任何被删除的命令都会有这个错误）。
#
# 所以这里用一个【假装是 widget 的普通 proc】就能完整复现同一类错误，
# 不需要真正的 Tk 控件，纯 tclsh + after + vwait 即可。
#
# 运行方式：
#   tclsh 10-destroyed-widget-callback.tcl

# 自定义 bgerror：Tk 事件循环（这里用 after/vwait 模拟）里，
# 未被 catch 捕获的错误不会让进程崩溃，而是转交给 bgerror。
# tclsh 的默认 bgerror 会把错误打印到 stderr，这里自定义一份更清晰的输出，
# 方便对照"回调出错后，程序并没有真的停下来"这件事。
proc bgerror {msg} {
    puts stderr "\[bgerror\] 捕获到未处理的回调错误：$msg"
}

puts "=== 错误版本：destroy 之后，延迟执行的 after 脚本仍然尝试访问它 ==="
puts ""

# 用一个普通 proc 模拟 widget 命令：".fakeLabel" 本身就是一个合法的
# Tcl 命令名字（以 "." 开头在 Tcl 里没有特殊语法含义，纯粹是约定）。
proc .fakeLabel {subcmd args} {
    switch -- $subcmd {
        configure { puts "  .fakeLabel configure $args" }
        default   { puts "  .fakeLabel: unknown subcommand $subcmd" }
    }
}

puts "-- 创建 .fakeLabel，并用 after 安排 200ms 后更新它的文字 --"
after 200 {.fakeLabel configure -text "updated later"}

puts "-- 但在 100ms 时，.fakeLabel 就已经被销毁了（模拟用户提前关闭了窗口） --"
after 100 {
    puts "  (模拟 destroy .fakeLabel)"
    rename .fakeLabel {}
}

after 300 {set ::doneWrong 1}
vwait ::doneWrong
puts ""
puts "可以看到：200ms 那个 after 脚本执行时，.fakeLabel 这个命令已经在 100ms 时"
puts "被 rename 成空（等价于 destroy）了，于是触发 \"invalid command name \\\".fakeLabel\\\"\"，"
puts "该错误被转交给上面自定义的 bgerror 打印出来，而不是让整个脚本崩溃退出。"
puts ""

puts "=========================================================="
puts "=== 正确版本：执行前先用 winfo exists 风格的检查守卫 ==="
puts "=========================================================="
puts ""

# 用一个全局关联数组模拟 "widget 是否还存在"，
# 真实 Tk 代码里这一步就是调用 winfo exists .foo。
array set ::fakeWidgetExists {}

proc fakeWidgetCreate {path} {
    set ::fakeWidgetExists($path) 1
    puts "  创建 $path"
}

proc fakeWidgetDestroy {path} {
    set ::fakeWidgetExists($path) 0
    puts "  销毁 $path"
}

proc fakeWinfoExists {path} {
    expr {[info exists ::fakeWidgetExists($path)] && $::fakeWidgetExists($path)}
}

fakeWidgetCreate .safeLabel

# 修复点：延迟脚本在真正操作 widget 之前，先检查它是否还存在
# （真实 Tk 代码写法是 `if {[winfo exists .safeLabel]} { ... }`）。
proc updateSafeLabelLater {} {
    if {[fakeWinfoExists .safeLabel]} {
        puts "  .safeLabel 仍然存在，安全地更新它"
    } else {
        puts "  .safeLabel 已经不存在了，跳过这次更新（没有报错）"
    }
}

after 200 updateSafeLabelLater
after 100 {fakeWidgetDestroy .safeLabel}

after 300 {set ::doneRight 1}
vwait ::doneRight

puts ""
puts "=== 另一种同样有效的修复思路：销毁 widget 的同时，取消还没触发的 after/bind ==="
puts "    （见 README 对应条目里的代码示例：destroy 前先 after cancel \$afterId）"
