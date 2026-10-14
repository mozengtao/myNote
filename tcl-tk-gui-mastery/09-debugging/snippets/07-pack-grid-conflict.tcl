#!/usr/bin/env wish
#
# 07-pack-grid-conflict.tcl —— Layer 9 故障 7：
#   pack 和 grid 冲突（同一父容器下混用）
#
# Tk 的规则很硬：【同一个父容器】下的所有直接子控件，必须自始至终只用
# 同一种 geometry manager（pack 或 grid 二选一，place 是另一回事）。
# 一旦某个父容器下已经有子控件被 pack 管理，再对该父容器下的另一个子
# 控件调用 grid，会直接报错终止，而不是"部分生效"或"自动兼容"。
#
# 这个运行时错误只有在真正连接到显示环境、widget 真实存在时才会触发，
# 所以本文件没有纯 Tcl 部分可演示；headless 下只能验证脚本在
# package require Tk 之前的语法完全合法。
#
# 运行方式：
#   headless 环境： tclsh 07-pack-grid-conflict.tcl   （只能验证到 package require Tk）
#   有图形环境：   wish  07-pack-grid-conflict.tcl
#     会在创建 .form.pwd 之后、对其调用 grid 时立即报错，错误信息类似：
#       cannot use geometry manager grid inside .form which already
#       has slaves managed by pack

puts "=== 以下进入真实 Tk 部分，headless 环境将在 package require Tk 处报错 ==="
package require Tk

wm title . "Bug 7: pack/grid conflict"

frame .form
pack .form -padx 10 -pady 10 -fill x

label .form.lblName -text "Name:"
entry .form.entName
label .form.lblPwd  -text "Password:"
entry .form.entPwd  -show "*"

# 错误写法：.form 下的前两个子控件（.form.lblName / .form.entName）
# 已经用 pack 安排了布局，紧接着却想用 grid 把 .form.lblPwd/.form.entPwd
# 摆成"标签在左、输入框在右对齐"的两列网格——这就是典型的
# "先 pack 了几个，又想用 grid 微调对齐"的冲突场景。
pack .form.lblName -side left
pack .form.entName -side left -padx {4 10}

# 下面这一行在真实 Tk 环境下会抛出错误并中止脚本：
# "cannot use geometry manager grid inside .form which already has
#  slaves managed by pack"
grid .form.lblPwd -row 0 -column 2
grid .form.entPwd -row 0 -column 3

# --------------------------------------------------------------------------
# 正确写法：要么这一整个父容器统一用 pack，要么统一用 grid。
# 下面给出"整体改用 grid"的修复版本，注意它是一个独立的 frame，
# 不会和上面冲突的 .form 互相影响（上面的代码在真实环境下已经报错中止，
# 不会执行到这里；这里单独列出来是为了让你对照修复后的写法）。
# --------------------------------------------------------------------------
frame .formFixed
pack .formFixed -padx 10 -pady 10 -fill x

label .formFixed.lblName -text "Name:"
entry .formFixed.entName
label .formFixed.lblPwd  -text "Password:"
entry .formFixed.entPwd  -show "*"

# 统一用 grid：两行两列，天然对齐，不再需要 -side left 这种手工拼接。
grid .formFixed.lblName -row 0 -column 0 -sticky e -padx 4 -pady 2
grid .formFixed.entName -row 0 -column 1 -sticky we -padx 4 -pady 2
grid .formFixed.lblPwd  -row 1 -column 0 -sticky e -padx 4 -pady 2
grid .formFixed.entPwd  -row 1 -column 1 -sticky we -padx 4 -pady 2
grid columnconfigure .formFixed 1 -weight 1
