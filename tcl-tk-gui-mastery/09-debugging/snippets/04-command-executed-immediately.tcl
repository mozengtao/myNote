#!/usr/bin/env wish
#
# 04-command-executed-immediately.tcl —— Layer 9 故障 4：
#   按钮创建时就提前执行了命令（-command [proc_call] 误用）
#
# 这个坑的本质是 Tcl 的求值顺序，和 Tk 关系不大，所以不需要图形环境
# 也能用 tclsh 把"提前执行"这个现象本身完整复现出来。
#
# 运行方式：
#   headless 环境： tclsh 04-command-executed-immediately.tcl
#     （前半部分纯 Tcl，会完整跑完；后半部分 package require Tk 处才报错）
#   有图形环境：   wish  04-command-executed-immediately.tcl

puts "=== 演示：Tcl 在构造命令参数时，会先完成 \[...\] 里的替换，再调用外层命令 ==="
puts ""

# showMsg 模拟"用户点击按钮时才应该被执行"的动作
proc showMsg {} {
    puts "  >>> showMsg 被调用了！（如果这行在你还没点任何按钮时就打印出来，说明时机错了）"
    return "ignored-return-value"
}

puts "-- 错误写法：-command \[showMsg\] --"
puts "等价于：Tcl 先计算 \[showMsg\]（也就是立刻调用一次 showMsg），"
puts "再把它的返回值当成 -command 这个选项的值。"
# 用 list 组装参数，效果等价于真实的
#   button .b -command [showMsg]
# 这一行本身在被求值的过程中，[showMsg] 就会被立即执行一次。
set wrongArgs [list .b -command [showMsg]]
puts "wrongArgs = $wrongArgs"
puts "可以看到 -command 后面的值是字符串 \"ignored-return-value\"，而不是脚本 \"showMsg\"。"
puts ""

puts "-- 正确写法 1：-command {showMsg}（花括号，脚本字面量，不做任何替换） --"
set rightArgs1 [list .b -command {showMsg}]
puts "rightArgs1 = $rightArgs1"
puts "注意：上面这一行没有再打印 \">>> showMsg 被调用了\"，说明 {showMsg} 只是被原样存成了字符串。"
puts ""

puts "-- 正确写法 2：-command showMsg（不加任何括号，本身就是一个合法命令名字符串） --"
set rightArgs2 [list .b -command showMsg]
puts "rightArgs2 = $rightArgs2"
puts ""

puts "=== 结论：只有 Tk 真正触发 -command 对应的脚本（用户点击按钮）那一刻，"
puts "    showMsg 才应该被调用；错误写法让它在 button 命令执行的那一瞬间就先被调用了一次 ==="
puts ""

# --------------------------------------------------------------------------
# 下面是真实的 Tk 版本，用来在有图形环境下验证最终效果。
# headless 下脚本会执行到 package require Tk 这一行才报错，这是预期行为，
# 证明这一行之前（包括上面整段纯 Tcl 演示）语法和逻辑都是合法的。
# --------------------------------------------------------------------------
puts "=== 以下进入真实 Tk 部分，headless 环境将在 package require Tk 处报错 ==="
package require Tk

wm title . "Bug 4: premature -command"

label .hint -text "观察终端：窗口刚显示出来时，下面是不是已经打印过一次 showMsg？" \
    -wraplength 360 -justify left
pack .hint -padx 10 -pady 10

# 错误写法：下面这一行 button 命令被求值时，[showMsg] 会被立刻调用一次，
# 并把它的返回值（一个普通字符串）当成"点击时要执行的脚本"——
# 这意味着不管以后怎么点这个按钮，执行的都是这段固定、通常毫无意义的
# 字符串，而不是重新调用 showMsg。
button .wrongButton -text {Wrong: -command [showMsg]} -command [showMsg]
pack .wrongButton -pady 5

# 正确写法：用花括号包裹，-command 的值是脚本字面量 "showMsg"，
# 只有真正点击按钮、事件循环分发点击事件给 .rightButton 时，
# Tk 才会执行这个脚本（也就是调用 showMsg）。
button .rightButton -text {Right: -command {showMsg}} -command {showMsg}
pack .rightButton -pady 5
