#!/usr/bin/env wish
#
# 06-callback-stale-value.tcl —— Layer 9 故障 6：
#   回调读取到旧值（循环里创建多个控件，-command/bind 捕获变量的时机不对）
#
# 根因是 Tcl 没有"闭包"：{...} 这种花括号脚本只是原样存一段文本，
# 真正的变量替换要等到【将来被调用的那一刻】才发生，而不是【创建时】。
# 如果脚本里引用的是一个循环变量（比如 foreach 的 i），
# 等循环结束后它早就停在最后一个值上了——所有按钮点击时读到的都是
# 同一个"旧"（其实是"最终残留"）值，而不是各自创建时那个值。
#
# 这个现象完全是 Tcl 求值时机问题，不需要真的点击按钮也能复现：
# 我们可以在 headless 环境里直接 `uplevel #0 $storedScript`，
# 模拟 Tk 在"用户点击按钮"那一刻对 -command 脚本做的事情。
#
# 运行方式：
#   headless 环境： tclsh 06-callback-stale-value.tcl
#   有图形环境：   wish  06-callback-stale-value.tcl

puts "=== 纯 Tcl 演示：{} 脚本里的循环变量在调用时才被替换 ==="
puts ""

# 模拟"给 3 个按钮分别绑定 -command"，但用错误写法：
# 花括号里直接写 $i，替换被推迟到真正调用的那一刻。
set wrongCommands {}
foreach i {1 2 3} {
    # {puts "clicked button $i"} 在这里只是被原样存成字符串，
    # 字符串里的 $i 【没有】被替换成 1/2/3，而是保留成字面量 "$i"。
    lappend wrongCommands [list button $i {puts "clicked button $i"}]
}

puts "-- 循环结束后，变量 i 停留在最后一次迭代的值上 --"
puts "i (循环结束后的残留值) = $i"
puts ""

puts "-- 现在模拟依次点击 button 1 / button 2 / button 3（错误版本） --"
foreach entry $wrongCommands {
    lassign $entry label idx script
    puts "点击 $label $idx 对应的按钮，执行存储的脚本：$script"
    # uplevel #0 模拟 Tk 执行 -command 脚本的方式：在全局作用域求值。
    # 此时 $i 才第一次被替换，取的是【此刻】全局变量 i 的值，
    # 而不是这个按钮创建时 foreach 迭代到的那个值。
    uplevel #0 $script
}
puts "=== 可以看到：不管点哪个按钮，打印出来的都是 \"clicked button 3\"，"
puts "    因为三个按钮存的 -command 脚本字符串完全相同，都只在点击时才查一次 \$i ==="
puts ""

# 正确写法：用 list 构造命令，list 会在"构造这一刻"就把 $i 的当前值
# 替换进去并冻结成一个确定的元素，而不是留一段"将来再替换"的文本。
puts "=== 正确写法：用 \[list proc_name \$i\] 在创建时就把值冻结进去 ==="
proc clickedRight {n} {
    puts "clicked button $n (frozen value)"
}

set rightCommands {}
foreach i {1 2 3} {
    # [list clickedRight $i] 在 lappend 这一行执行时就已经把当前的 $i
    # （1、然后 2、然后 3）替换完毕，生成的是一条已经写死参数的命令，
    # 比如 {clickedRight 1}、{clickedRight 2}、{clickedRight 3}。
    lappend rightCommands [list clickedRight $i]
}

puts "-- 依次点击 button 1 / button 2 / button 3（正确版本） --"
foreach script $rightCommands {
    puts "执行存储的脚本：$script"
    uplevel #0 $script
}
puts "=== 可以看到：每个按钮点击时都打印出了自己创建时那个 n 的值 ==="
puts ""

# --------------------------------------------------------------------------
# 真实 Tk 版本：在循环里创建 3 个按钮，分别用错误/正确两种写法绑定 -command。
# headless 下脚本会在 package require Tk 这一行才报错，这是预期行为。
# --------------------------------------------------------------------------
puts "=== 以下进入真实 Tk 部分，headless 环境将在 package require Tk 处报错 ==="
package require Tk

wm title . "Bug 6: callback stale value"

label .hint -text "左列按钮（错误写法）无论点哪个都报告 3；右列按钮（正确写法）各自报告自己的编号" \
    -wraplength 420 -justify left
pack .hint -padx 10 -pady 10

frame .wrongFrame
frame .rightFrame
pack .wrongFrame -side left -padx 10 -anchor n
pack .rightFrame -side left -padx 10 -anchor n

label .wrongFrame.title -text "Wrong"
pack .wrongFrame.title

foreach i {1 2 3} {
    # 错误写法：{} 里的 $i 要等点击时才替换，三个按钮点击时都读到
    # 循环结束后残留的 i 值（这里是 3）。
    button .wrongFrame.b$i -text "Button $i" -command {puts "clicked button $i"}
    pack .wrongFrame.b$i -pady 2
}

label .rightFrame.title -text "Right"
pack .rightFrame.title

foreach i {1 2 3} {
    # 正确写法：用 list 在创建这一刻就把 $i 的值冻结进命令里。
    button .rightFrame.b$i -text "Button $i" -command [list clickedRight $i]
    pack .rightFrame.b$i -pady 2
}
