#!/usr/bin/env wish
#
# checkbutton_state.tcl —— Layer 3 示例 B：Checkbutton 与布尔 / 自定义状态绑定
#
# 核心问题：用户勾选/取消一个复选框，程序怎么拿到"当前是勾选还是没勾选"？
# 答案和 Entry 一样：Checkbutton 通过 -variable（不是 -textvariable）
# 把自己的"勾选状态"绑定到一个 Tcl 变量上；区别在于 Entry 绑定的是
# "文本内容"，Checkbutton 绑定的是一个"两态值"（默认是 0/1，
# 但可以用 -onvalue / -offvalue 自定义成任意两个值）。
#
# 运行方式：
#   有图形环境：   wish checkbutton_state.tcl
#   headless 环境： tclsh checkbutton_state.tcl   （只能校验到 package require Tk 这一步）
package require Tk

# --------------------------------------------------------------------------
# 阶段 0：两个"状态变量"，初始值必须和 Checkbutton 的默认外观对应
# --------------------------------------------------------------------------
# agreeTerms 使用 Checkbutton 的默认约定：0 表示未勾选，1 表示已勾选。
set agreeTerms 0

# notifyPref 演示 -onvalue / -offvalue：变量里存的不是 0/1，
# 而是业务上更有意义的字符串 "enabled" / "disabled"。
# 这说明 Checkbutton 绑定的"两态值"可以是任意两个你指定的值，
# 不是只能用布尔 0/1——Tk 并不关心这两个值具体是什么，
# 它只负责"勾选时把变量设成 onvalue，取消勾选时设成 offvalue"。
set notifyPref "disabled"

# statusText 是一个派生状态：它不直接绑定到任何 Checkbutton，
# 而是由下面的 proc 在"被显式调用时"重新计算出来，
# 用来对照"自动同步的变量绑定"和"需要显式触发的派生显示"两种模式。
set statusText "Status: not checked yet"

wm title    . "Checkbutton State Demo"
wm geometry . 380x260

# --------------------------------------------------------------------------
# 阶段 1：第一个 Checkbutton —— 标准布尔用法
# --------------------------------------------------------------------------
# checkbutton .agreeCheck -text "I agree to the terms" -variable agreeTerms
#
# 不写 -onvalue / -offvalue 时，Tk 使用内置默认值：
#   勾选  -> 变量被 set 为 1
#   取消  -> 变量被 set 为 0
# 和 Entry 的 -textvariable 一样，这个绑定也是通过在 agreeTerms 上
# 注册读/写 trace 实现的，只是 Checkbutton 写入变量的值只有两种可能，
# 而不是像 Entry 那样是任意文本。
checkbutton .agreeCheck -text "I agree to the terms" -variable agreeTerms

# --------------------------------------------------------------------------
# 阶段 2：第二个 Checkbutton —— 自定义 onvalue / offvalue
# --------------------------------------------------------------------------
checkbutton .notifyCheck -text "Enable email notifications" \
    -variable notifyPref -onvalue "enabled" -offvalue "disabled"

# --------------------------------------------------------------------------
# 阶段 3：用一个 trace 实时打印 agreeTerms 的变化（方式一：trace）
# --------------------------------------------------------------------------
# 这里提前用到了 Layer 3 示例 C（variable_trace.tcl）要详细讲的机制：
# trace add variable <name> write <callback>
# 效果：只要 agreeTerms 被 set（不管是用户点击触发，还是程序自己
# set），onAgreeChanged 就会被自动调用一次，不需要我们在 Checkbutton
# 上额外挂 -command。
proc onAgreeChanged {varName index op} {
    # varName/index/op 是 trace 回调的标准三个参数：
    #   varName —— 被追踪的变量名（这里恒为 "agreeTerms"）
    #   index   —— 如果追踪的是数组元素，这里是元素名；普通变量为空字符串
    #   op      —— 触发原因，这里恒为 "write"
    global agreeTerms
    puts "\[trace\] agreeTerms changed -> $agreeTerms"
}
trace add variable agreeTerms write onAgreeChanged

# --------------------------------------------------------------------------
# 阶段 4：用一个按钮显式查询两个状态变量（方式二：主动查询 + 回调）
# --------------------------------------------------------------------------
# 这个按钮演示的是和"变量绑定自动同步"完全不同的另一种获取状态的方式：
# 不依赖任何自动刷新，而是在用户点击这个特定按钮的那一刻，
# 主动读一次 agreeTerms 和 notifyPref 的当前值，拼成一段文字。
# 对比：
#   .agreeCheck / .notifyCheck 的状态同步是"声明式"的，
#       只要绑定关系建立好，后续完全不需要额外代码。
#   .queryButton 的状态查询是"命令式"的，
#       必须显式写代码在某个时间点去读取变量当前值。
proc showStatus {} {
    global agreeTerms notifyPref statusText
    set statusText "agreeTerms=$agreeTerms, notifyPref=$notifyPref"
    puts "\[query\] $statusText"
}
button .queryButton -text "Show Status" -command showStatus

label .statusLabel -textvariable statusText -fg blue -anchor w

label .hint1 -text "勾选 'I agree' 会在终端打印 trace 日志" -fg gray40
label .hint2 -text "点击 'Show Status' 才会刷新下面这一行" -fg gray40

pack .agreeCheck .notifyCheck -anchor w -padx 10 -pady 5
pack .queryButton -pady 10
pack .statusLabel -padx 10 -pady 5 -anchor w -fill x
pack .hint1 .hint2 -padx 10 -anchor w

# --------------------------------------------------------------------------
# 阶段 5：进入事件循环
# --------------------------------------------------------------------------
# 之后：
#   - 勾选/取消任意 Checkbutton -> 对应变量被 set -> 如果该变量上有
#     trace（agreeTerms 有），trace 立刻打印；.statusLabel 不会变，
#     因为它绑定的是 statusText，而 statusText 只会在 showStatus 被
#     调用时才重新计算。
#   - 点击 "Show Status" -> 显式调用 showStatus -> 重新计算 statusText
#     -> statusText 被 set -> .statusLabel（textvariable 绑定）自动刷新。
