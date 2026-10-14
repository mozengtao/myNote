#!/usr/bin/env wish
#
# grid_example.tcl —— Layer 2 Complete Example (2/2): grid 几何管理器
#
# 场景：一个"可缩放的配置窗口"。
#   表单区（Name / Port / Host）内部使用 grid 布局；
#   按钮区（Save / Exit）内部使用 pack 布局；
#   —— 演示"不同父 widget 内部可以分别使用不同的几何管理器"。
#
# 布局树（详见 README.md 中的 ASCII 图）：
#   .
#   ├── .form      (frame, 内部用 grid)
#   │   ├── .form.nameLabel / .form.nameEntry
#   │   ├── .form.portLabel / .form.portEntry
#   │   └── .form.hostLabel / .form.hostEntry
#   └── .buttons   (frame, 内部用 pack)
#       ├── .buttons.save
#       └── .buttons.exit
#
# 运行方式：
#   有图形环境：   wish grid_example.tcl
#   headless 环境： tclsh grid_example.tcl   （只能校验到 package require Tk 这一步）

package require Tk

wm title . "Grid Example"
wm geometry . 360x180
wm minsize . 280 140

# --------------------------------------------------------------------------
# 阶段 1：划分区域 —— 用两个 frame 把窗体切成"表单区"和"按钮区"
# --------------------------------------------------------------------------
# frame 本身没有任何可见内容，它唯一的作用是充当"子容器"：
#   - 对 "."（它的父控件）来说，.form / .buttons 只是两个普通子控件，
#     "." 可以用任意一种几何管理器（这里用 pack）去摆放它们；
#   - 对 .form / .buttons 自己来说，它们又是独立的父容器，
#     各自的子控件可以用另一种、互不相关的几何管理器去摆放。
# 这正是"同一棵 widget 树里混用多种几何管理器"的正确做法——
# 混用发生在不同的父子层级之间，而不是在同一个父控件内部。
frame .form
frame .buttons

# --------------------------------------------------------------------------
# 阶段 2：表单区内部 —— 用 grid 搭建一张"不可见表格"
# --------------------------------------------------------------------------
# grid 的核心思路：把父容器想象成一张由若干行、若干列组成的表格；
# 每个子控件占据表格里的一个单元格（cell），单元格的宽/高由
# 该行/列里"最大的那个控件"决定。
label .form.nameLabel -text "Name:"
entry .form.nameEntry

label .form.portLabel -text "Port:"
entry .form.portEntry

label .form.hostLabel -text "Host:"
entry .form.hostEntry

# -sticky 决定控件在"自己所在单元格内部"如何对齐/拉伸：
#   e  = 贴紧单元格右边缘（标签右对齐，和输入框左边缘视觉对齐更整齐）
#   ew = 水平方向拉伸，同时贴紧单元格左右两边
#        （输入框会随着"所在单元格变宽"而一起变宽）
# 注意：-sticky 只回答"控件如何填满自己的单元格"；
#       单元格本身能不能变大，是下面 -weight 的职责——两者缺一不可。
grid .form.nameLabel -row 0 -column 0 -padx 6 -pady 6 -sticky e
grid .form.nameEntry -row 0 -column 1 -padx 6 -pady 6 -sticky ew

grid .form.portLabel -row 1 -column 0 -padx 6 -pady 6 -sticky e
grid .form.portEntry -row 1 -column 1 -padx 6 -pady 6 -sticky ew

grid .form.hostLabel -row 2 -column 0 -padx 6 -pady 6 -sticky e
grid .form.hostEntry -row 2 -column 1 -padx 6 -pady 6 -sticky ew

# -weight 决定"行/列"如何分配父容器里多余的空间：
#   column 0（标签列）：不需要随窗口变宽而变宽，保持默认 weight 0。
#   column 1（输入框列）：weight 1，表示 .form 出现的多余宽度
#   （因为窗口被拉宽了）全部分配给这一列。
# 行方向这里不需要随窗口变高而变高（三行输入框变高没有意义），
# 所以不设置 rowconfigure -weight，保持默认值 0。
grid columnconfigure .form 0 -weight 0
grid columnconfigure .form 1 -weight 1

# --------------------------------------------------------------------------
# 阶段 3：按钮区内部 —— 用 pack 把按钮靠右排列
# --------------------------------------------------------------------------
button .buttons.save -text "Save"
button .buttons.exit -text "Exit" -command {destroy .}

# -side right：从右边缘开始依次 claim 空间，所以先 pack 的 .exit 最靠右，
# 后 pack 的 .save 排在 .exit 左边——这是对话框按钮区常见的排布方式。
pack .buttons.exit -side right -padx 4 -pady 6
pack .buttons.save -side right -padx 4 -pady 6

# --------------------------------------------------------------------------
# 阶段 4：根窗口 "." 内部 —— 用 pack 把两个区域纵向排列
# --------------------------------------------------------------------------
# .form 和 .buttons 都是 "." 的子控件，它们之间用 pack 摆放：
#   .form    : -fill both -expand 1
#              表单区占据所有"多余空间"；窗口变宽/变高时，
#              多余空间先分给 .form，.form 内部再按 grid 的
#              -weight 规则，把多余宽度进一步分给 column 1。
#   .buttons : -fill x（没有 -expand）
#              按钮区只在水平方向填满宽度，高度保持自身内容的
#              自然高度，不参与垂直方向的多余空间分配。
pack .form -side top -fill both -expand 1 -padx 10 -pady 10
pack .buttons -side bottom -fill x -padx 10 -pady {0 10}
