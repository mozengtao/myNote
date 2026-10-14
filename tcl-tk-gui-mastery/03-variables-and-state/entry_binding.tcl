#!/usr/bin/env wish
#
# entry_binding.tcl —— Layer 3 示例 A：Entry 与 Tcl 变量的双向绑定
#
# 核心问题：用户在输入框里敲字，程序怎么"知道"？
# 答案的雏形：程序根本不需要主动"去问"，因为 Entry 控件在创建时
# 就通过 -textvariable 把自己和一个 Tcl 变量"绑"在了一起——
# 之后不管是用户敲键盘，还是程序自己用 set 改这个变量，
# 两边的状态永远自动保持一致。这就是本层要建立的 Mental Model。
#
# 运行方式：
#   有图形环境：   wish entry_binding.tcl
#   headless 环境： tclsh entry_binding.tcl   （只能校验到 package require Tk 这一步）
package require Tk

# --------------------------------------------------------------------------
# 阶段 0：声明一个普通 Tcl 变量
# --------------------------------------------------------------------------
# 注意：这一行和"GUI"完全没有关系。此刻 username 只是 Tcl 解释器
# 全局变量表里的一个普通条目，值是字符串 "guest"。
# 任何还没创建的 widget 都不知道这个变量的存在。
set username "guest"

wm title    . "Entry Binding Demo"
wm geometry . 360x220

# --------------------------------------------------------------------------
# 阶段 1：创建一堆普通控件做视觉说明
# --------------------------------------------------------------------------
label .promptLabel -text "Username:" -anchor w

# --------------------------------------------------------------------------
# 阶段 2：关键一行——用 -textvariable 把 Entry 和 username 绑定
# --------------------------------------------------------------------------
# entry .usernameEntry -textvariable username
#
# 这一行背后真正发生的事情（这是本层要讲透的机制）：
#   1. Tk 在内部对变量 username 注册了一对"读/写 trace"
#      （trace 是 Tcl 核心提供的机制：当某个变量被 read/write/unset 时，
#       自动调用一个回调；这里的回调是 Tk 的 C 代码内部实现的，
#       对使用者不可见，但语义上等价于我们稍后在 variable_trace.tcl
#       里手写的 trace add variable ... write ... ）。
#   2. 当用户在这个 Entry 里敲键盘时，Tk 的事件绑定层会把按键事件
#      转换成"更新 Entry 显示内容"的动作，而这个动作的实现方式
#      就是对 username 执行一次 set —— 也就是说，用户打字本质上是
#      在间接地反复执行 `set username "新内容"`。
#   3. username 被 set 之后，Tk 之前注册的 write trace 立刻触发，
#      这个 trace 回调会让"所有绑定了 username 的 widget"重新读取
#      变量的最新值并刷新自己的显示——包括 Entry 自己（保持光标和
#      显示同步），也包括下面即将出现的 .previewLabel 和 .mirrorEntry。
entry .usernameEntry -textvariable username

# --------------------------------------------------------------------------
# 阶段 3：第二个、第三个 widget 绑定同一个变量
# --------------------------------------------------------------------------
# 这里特意用两个不同类型的 widget（Label 和另一个 Entry）绑定同一个
# username 变量，用来证明："绑定同一个变量"不是 Entry 独有的特性，
# 而是 Tk 里凡是支持 -textvariable 选项的 widget 都适用的通用机制。
label .previewLabel -textvariable username -fg blue
entry .mirrorEntry  -textvariable username

# --------------------------------------------------------------------------
# 阶段 4：两个按钮——证明"程序侧 set 变量"同样会驱动所有 widget 刷新
# --------------------------------------------------------------------------
# 这两个按钮的 -command 脚本都只做一件事：对 username 做一次 set。
# 点击按钮后，你会看到上面三个 widget（Entry、Label、第二个 Entry）
# 的显示会【同时】变化——这正好反向证明了"绑定"是双向的：
#   用户输入 -> 变量改变 -> 其余 widget 刷新（上面已经讲过）
#   程序 set 变量 -> 变量改变 -> 所有 widget（包括 Entry 本身）刷新（这里演示）
button .resetButton  -text "Reset to guest"   -command { set username "guest" }
button .upperButton  -text "Uppercase"        -command {
    # 注释：这里直接读写同一个全局变量 username。
    # 因为这段脚本是在全局作用域下被 -command 调用执行的，
    # 不需要 global 声明也能访问到最外层的 username。
    set username [string toupper $username]
}

label .hint -text "改任意一个输入框，另外两个会自动同步" -fg gray40

pack .promptLabel -padx 10 -pady {10 0} -anchor w
pack .usernameEntry -padx 10 -pady 5 -fill x
pack .previewLabel -padx 10 -pady 5 -anchor w
pack .mirrorEntry -padx 10 -pady 5 -fill x
pack .resetButton .upperButton -side left -padx 10 -pady 10
pack .hint -padx 10 -pady {0 10} -anchor w

# --------------------------------------------------------------------------
# 阶段 5：进入事件循环（wish 自动完成，这里不需要写 vwait）
# --------------------------------------------------------------------------
# 之后发生的一切（用户打字、点击按钮）都会沿着上面注释描述的路径
# 反复触发："widget 事件 -> set 变量 -> trace -> 其余 widget 刷新"。
