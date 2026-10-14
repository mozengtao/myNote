#!/usr/bin/env wish
#
# button_callback.tcl —— Layer 4 示例 A：Widget Command Callback（-command）
#
# 本例要回答一个新手最容易搞错的问题：
#   -command greet / -command {greet} / -command [greet]
#   这三种写法里，哪些是"正确地延迟执行回调"，哪些会"立刻执行"？
#
# 运行方式：
#   有图形环境：   wish button_callback.tcl
#   headless 环境： tclsh button_callback.tcl  （只能校验到 package require Tk 这一步）
#
# 运行后观察终端输出的顺序，配合 README.md 的"执行顺序图"一起看效果最好。

package require Tk

# --------------------------------------------------------------------------
# 回调函数本体：greet
# --------------------------------------------------------------------------
# greet 只是一个普普通通的 proc，它本身并不知道自己会被当成"回调"使用。
# "回调"这个身份，是由"谁在什么时候调用它"决定的，不是由定义方式决定的。
proc greet {} {
    puts "Hello from callback"
    # 注意：puts 的返回值永远是空字符串 ""
    # 这一点在下面解释 -command [greet] 的坑时非常关键。
}

# --------------------------------------------------------------------------
# 写法 1：-command greet（裸字——正确、最常见的写法）
# --------------------------------------------------------------------------
# button 命令在被求值时，"greet" 只是一个普通字符串词（bare word），
# 没有任何 $ 变量替换、没有任何 [ ] 命令替换需要处理，
# 所以它会被原样当作 -command 选项的值存起来。
# 这一行执行完，greet 这个 proc 完全没有被调用。
puts {===== creating .b1  (-command greet) ====}
button .b1 -text "Click 1: -command greet" -command greet
puts {===== .b1 created; greet was NOT called =====}

# --------------------------------------------------------------------------
# 写法 2：-command {greet}（花括号分组——结果和写法 1 完全一样）
# --------------------------------------------------------------------------
# {greet} 只是用花括号把 "greet" 这个词组合起来，
# 花括号在这里唯一的作用是"分词/分组"，并不会触发任何额外的替换，
# 花括号内部的内容本来就不会被 Tcl 做变量替换或命令替换。
# 所以 {greet} 求值出来的词，和裸字 greet 求值出来的词，是同一个字符串 "greet"。
puts {===== creating .b2  (-command {greet}) ====}
button .b2 -text "Click 2: -command {greet}" -command {greet}
puts {===== .b2 created; greet was NOT called =====}

# --------------------------------------------------------------------------
# 写法 3：-command [greet]（命令替换——经典新手陷阱，会立刻执行）
# --------------------------------------------------------------------------
# [ ] 是命令替换：Tcl 在对 button 这条命令本身的参数做求值的阶段，
# 就会先执行方括号里的 greet，并把它的*返回值*替换到这个位置，
# 然后才用替换后的结果去调用 button。
# greet 的返回值是空字符串 ""（因为 puts 返回 ""），
# 所以这一行等价于：button .b3 -text ... -command ""
# 也就是说：
#   1. greet 在"创建按钮"这一步就已经执行完了（副作用：打印了一行字）。
#   2. .b3 真正存下来的 -command 值是空字符串，点击 .b3 什么都不会发生。
puts {===== creating .b3  (-command [greet]) -- WATCH CAREFULLY ====}
button .b3 -text "Click 3: -command \[greet\] (buggy)" -command [greet]
puts {===== .b3 created; notice greet ALREADY ran above, before any click! =====}

# --------------------------------------------------------------------------
# 用 cget 把三个按钮真正存下来的 -command 值打印出来，验证上面的分析
# --------------------------------------------------------------------------
puts "---- actual stored -command values ----"
puts [format {.b1 -command = "%s"} [.b1 cget -command]]
puts [format {.b2 -command = "%s"} [.b2 cget -command]]
puts [format {.b3 -command = "%s"  (empty! greet's return value, not a callback)} \
    [.b3 cget -command]]

# --------------------------------------------------------------------------
# 布局
# --------------------------------------------------------------------------
label .hint -justify left -text \
    "点击上面三个按钮，对比终端输出：\n- 按钮 1、2 点击后才会打印 Hello\n- 按钮 3 点击后什么都不会发生（因为 -command 是空字符串）"
pack .hint -padx 10 -pady {10 6} -anchor w
pack .b1 .b2 .b3 -padx 20 -pady 4 -anchor w

button .quit -text "Quit" -command {destroy .}
pack .quit -pady 10
