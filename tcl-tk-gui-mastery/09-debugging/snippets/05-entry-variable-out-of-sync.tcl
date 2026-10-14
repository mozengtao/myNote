#!/usr/bin/env wish
#
# 05-entry-variable-out-of-sync.tcl —— Layer 9 故障 5：
#   输入框和 Tcl 变量不同步
#
# 最常见的根因其实是纯 Tcl 的"变量作用域"问题，和 Entry 控件本身无关：
# Entry 靠 -textvariable 绑定的永远是【某个确定作用域里】的变量
# （通常是全局变量 ::username），但如果在 proc 内部写 `set username ...`
# 而忘了先 `global username`，Tcl 会在 proc 的局部作用域里新建一个
# 同名的局部变量，Entry 绑定的那个全局变量根本没有被改动。
#
# 运行方式：
#   headless 环境： tclsh 05-entry-variable-out-of-sync.tcl
#     （前半部分纯 Tcl，完整演示作用域问题；后半部分 package require Tk 处才报错）
#   有图形环境：   wish  05-entry-variable-out-of-sync.tcl

puts "=== 纯 Tcl 演示：忘记 global 导致\"看起来改了变量，其实改的是另一个变量\" ==="
puts ""

set ::username "alice"
puts "初始状态：::username = $username"
puts ""

# 错误写法：proc 内部直接 set username，没有声明 global。
proc resetUsernameWrong {} {
    # 这里的 username 是 resetUsernameWrong 这个 proc 的局部变量，
    # 和外层的 ::username 完全是两个不同的存储位置，只是恰好同名。
    set username "guest"
    puts "  \[resetUsernameWrong 内部\] username = $username（这是局部变量）"
}

puts "-- 调用 resetUsernameWrong（错误版本） --"
resetUsernameWrong
puts "调用之后：::username = $username   <- 完全没变，还是 alice！"
puts ""

# 正确写法：先 global username，再 set，这样 set 操作的就是同一个
# Entry 绑定着的那个全局变量。
proc resetUsernameRight {} {
    global username
    set username "guest"
    puts "  \[resetUsernameRight 内部\] username = $username（这是全局变量）"
}

puts "-- 调用 resetUsernameRight（正确版本） --"
resetUsernameRight
puts "调用之后：::username = $username   <- 变成了 guest，Entry 会同步刷新"
puts ""

# --------------------------------------------------------------------------
# 真实 Tk 版本：Entry 用 -textvariable username 绑定同一个全局变量，
# 两个按钮分别调用错误/正确版本的 reset 过程，用来对比实际界面表现。
# headless 下脚本会在 package require Tk 这一行才报错，这是预期行为。
# --------------------------------------------------------------------------
puts "=== 以下进入真实 Tk 部分，headless 环境将在 package require Tk 处报错 ==="
package require Tk

wm title . "Bug 5: entry/variable out of sync"

set username "alice"

label .hint -text "Entry 内容绑定的是全局变量 username：" -anchor w
entry .entryUsername -textvariable username
label .hint2 -text "点 Wrong 按钮：username 不会变，Entry 也不会变（proc 里忘了 global）" \
    -wraplength 360 -justify left -fg gray40
label .hint3 -text "点 Right 按钮：username 变成 guest，Entry 会同步刷新" \
    -wraplength 360 -justify left -fg gray40

button .wrongButton -text "Reset (wrong, no global)" -command resetUsernameWrong
button .rightButton -text "Reset (right, with global)" -command resetUsernameRight

pack .hint -padx 10 -pady {10 2} -anchor w -fill x
pack .entryUsername -padx 10 -pady 2 -fill x
pack .hint2 -padx 10 -pady 2 -anchor w -fill x
pack .wrongButton -pady 2
pack .hint3 -padx 10 -pady 2 -anchor w -fill x
pack .rightButton -pady 2
