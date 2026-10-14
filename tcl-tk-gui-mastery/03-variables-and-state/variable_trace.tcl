#!/usr/bin/env wish
#
# variable_trace.tcl —— Layer 3 示例 C：trace add variable 的机制与时机
#
# 核心问题：除了 widget 自带的 -textvariable / -variable 绑定之外，
# 我们能不能自己监控一个普通 Tcl 变量的变化？能——这就是 trace。
# 事实上，-textvariable / -variable 在 Tk 内部就是靠 trace 实现的，
# 本文件要把这个"黑盒"打开，手写一遍同样的机制。
#
# 运行方式：
#   有图形环境：   wish variable_trace.tcl
#   headless 环境： tclsh variable_trace.tcl
#
# 注意：本文件分成两部分——
#   第一部分是纯 Tcl 逻辑，完全不依赖 Tk，在 tclsh 下会【完整执行完】
#     （不会在这部分报错），用来证明 trace 是 Tcl 核心能力，
#     和有没有图形界面毫无关系。
#   第二部分才 package require Tk，构建一个 Entry 把同一套 trace
#     机制接到真实的 GUI 输入上；在 headless 环境下，脚本会执行到
#     这一行才报错（"无法连接到显示"），这是预期行为。

# ============================================================================
# 第一部分：纯 Tcl 环境下的 trace（不需要 Tk，也不需要 $DISPLAY）
# ============================================================================

puts "=== Part 1: pure Tcl trace demo (no Tk needed) ==="

# counter 是一个普通变量，这里先验证最朴素的 Mental Model：
# "变量就是一个名字到值的映射"，和任何 widget 都没有关系。
set counter 0

# --------------------------------------------------------------------------
# 关键认知点：trace 回调拿不到"旧值"，只能拿到触发时变量的"新值"
# --------------------------------------------------------------------------
# trace add variable 的 write 回调被调用时，变量其实已经被写入完成了。
# Tcl 不会主动帮你保留"修改前的值"。如果业务上需要同时知道旧值和新值
# （这是本示例要求做到的），就必须自己在回调外维护一份"上一次见到的值"，
# 在回调里先打印，再更新这份影子记录。
set ::lastCounterValue $counter

proc onCounterWrite {varName index op} {
    # 使用 upvar #0 把全局变量 $varName 关联到本地名字 newVal，
    # 这样写法不依赖 varName 恒为某个固定名字，更通用。
    upvar #0 $varName newVal
    global lastCounterValue

    puts "\[trace\] var=$varName op=$op old=$lastCounterValue new=$newVal"

    # 打印完之后，把这次的新值记录成"下一次触发时的旧值"。
    set lastCounterValue $newVal
}

# trace add variable <name> write <callback>
#   <name>     —— 要监控的变量名（必须已经存在，或者之后会被创建）
#   write      —— 监控的操作类型，这里只关心"写入"（还有 read / unset）
#   <callback> —— 变量被写入时，Tcl 会以
#                   callback varName index op
#                 的形式自动调用它（index 对数组元素追踪才有意义，
#                 普通标量变量这里恒为空字符串）
trace add variable counter write onCounterWrite

puts "-- 连续修改 counter，观察 trace 同步触发 --"
set counter 1
set counter 5
incr counter          ;# incr 本质上也是"写入"操作，同样会触发 write trace
incr counter -3

# --------------------------------------------------------------------------
# 关键认知点：trace 的触发时机 —— 同步、在修改变量的那次调用内部完成
# --------------------------------------------------------------------------
# 上面四行 set/incr 执行完，终端应该已经依次打印出 4 行 trace 日志。
# 这证明了 trace 回调不是"异步排队、等事件循环空闲了再执行"，
# 而是【同步】发生的：`set counter 5` 这一行代码本身，在返回之前，
# 就已经把 onCounterWrite 调用完了。调用栈大致是：
#   main script
#     -> set counter 5          (Tcl 核心命令)
#       -> onCounterWrite ...   (trace 回调，在 set 返回前执行)
#   (set 命令真正返回，main script 继续往下走)
# 这一点和事件循环无关，因此这部分代码不需要 wish、不需要 vwait、
# 甚至不需要 package require Tk 就能完整跑通——这是本文件特意把
# "纯 Tcl 部分"放在 package require Tk 之前的原因。
puts "最终 counter = $counter"
puts ""

# trace 和按钮 -command 的本质区别（用一句话先立住，Execution Walkthrough
# 里会结合 GUI 部分再展开）：
#   trace    回调触发的理由是"这个变量被写入了"，它不关心是谁写的、
#            为什么写——可能是用户敲键盘，也可能是另一段业务逻辑调用
#            set，甚至可能是另一个 widget 的内部实现在写。
#   -command 回调触发的理由是"用户对这个特定 widget 做了这个特定动作"
#            （比如点击了这个按钮），它和任何变量是否改变没有直接关系；
#            哪怕 -command 脚本里一次 set 都不做，它照样会被执行。
puts "=== Part 1 done. 以上输出不依赖任何 GUI 环境 ==="
puts ""

# 良好习惯：不再需要的 trace 应该显式移除，避免变量在后续代码里
# 被意外修改时仍然触发已经"过期"的回调逻辑（典型场景：窗口被关闭、
# 对应的业务上下文已经不存在了，但变量本身因为是全局的，依然存活）。
trace remove variable counter write onCounterWrite
puts "已移除 counter 上的 trace（trace remove variable counter write onCounterWrite）"
puts "-- 验证：下面这次 set 不会再打印 trace 日志 --"
set counter 999
puts "counter 现在是 $counter，但没有 \[trace\] 行输出，说明回调确实被移除了"
puts ""

# ============================================================================
# 第二部分：把同一套 trace 机制接到真实的 Tk Entry 上
# ============================================================================
# 在 headless（无 $DISPLAY）环境下，脚本会执行到下面这一行才报错，
# 这正是本教程校验方式要求确认的"唯一报错点"。
puts "=== Part 2: 接入 Tk GUI，正在尝试连接显示... ==="
package require Tk

wm title    . "Variable Trace Demo"
wm geometry . 420x240

# inputValue 是绑定给 Entry 的变量；用户在 Entry 里敲字，
# 本质上就是在反复对 inputValue 执行 set（和示例 A 的 username 一样）。
set inputValue ""
set ::lastInputValue $inputValue

label .promptLabel -text "输入内容，终端会打印 trace 日志：" -anchor w
entry .inputEntry -textvariable inputValue

# logText 用来把 trace 日志同时镜像一份到窗口里，方便没有盯着终端的人
# 也能看到效果；注意 logText 本身不是被 trace 追踪的对象，
# 它只是 onInputWrite 回调里"顺手"更新的另一个变量。
set logText "(尚无输入)"
label .logLabel -textvariable logText -fg blue -anchor w -wraplength 380 -justify left

proc onInputWrite {varName index op} {
    upvar #0 $varName newVal
    global lastInputValue logText

    set line "old=\"$lastInputValue\" new=\"$newVal\""
    puts "\[trace\] $line"
    set logText "Last change: $line"

    set lastInputValue $newVal
}
trace add variable inputValue write onInputWrite

label .hint -text "对比：下面的按钮用 -command，点击才会打印；上面的 Entry 靠 trace，一改就打印" -fg gray40 -wraplength 380 -justify left

button .printButton -text "Print current value (via -command)" -command {
    # 这是 -command 风格：只有点击这个按钮的瞬间才会执行一次，
    # 和 inputValue 是否被修改过没有任何直接关系。
    puts "\[-command\] button clicked, inputValue is currently \"$inputValue\""
}

pack .promptLabel -padx 10 -pady {10 5} -anchor w -fill x
pack .inputEntry -padx 10 -pady 5 -fill x
pack .logLabel -padx 10 -pady 5 -anchor w -fill x
pack .hint -padx 10 -pady 5 -anchor w -fill x
pack .printButton -pady 10

# --------------------------------------------------------------------------
# 进入事件循环之后：
#   - 每敲一个字符 -> inputValue 被 set -> onInputWrite 同步触发
#     -> 终端打印一行，.logLabel 同步刷新。
#   - 点击 .printButton -> 执行一次 -command 脚本 -> 终端打印一行，
#     但 .logLabel 不会变化（因为这条路径完全没有碰 inputValue 之外
#     的任何会被 trace 的变量，也没有调用 onInputWrite）。
# --------------------------------------------------------------------------
