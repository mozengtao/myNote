#!/usr/bin/env wish
#
# pack_example.tcl —— Layer 2 Complete Example (1/2): pack 几何管理器
#
# 场景：一个垂直工具栏（Open / Save / Exit）+ 底部状态栏。
# 用来演示 pack 的核心思路——"顺序留白分配"（ordered cavity allocation）：
#   每次调用 pack，都是从父容器当前剩余的那块矩形空间（cavity）里，
#   按 -side 指定的方向切下一条，分配给当前控件；
#   切完之后，剩下的空间会继续作为新的 cavity，留给后面的 pack 调用。
#   整个过程只关心"从哪个边切、切多少、剩多少"，不涉及行列坐标。
#
# 运行方式：
#   有图形环境：   wish pack_example.tcl
#   headless 环境： tclsh pack_example.tcl   （只能校验到 package require Tk 这一步）

package require Tk

wm title . "Pack Example"
wm geometry . 220x160
wm minsize . 160 120

# --------------------------------------------------------------------------
# 阶段 1：创建控件（只是在内存里建立对象，和"摆在哪里"完全无关）
# --------------------------------------------------------------------------
button .open -text "Open"
button .save -text "Save"
button .exit -text "Exit" -command {destroy .}

# 底部状态栏：用 -relief sunken 让它看起来像一条凹陷的状态条
label .status -text "Ready" -anchor w -relief sunken

# --------------------------------------------------------------------------
# 阶段 2：布局（pack：side-based 顺序留白分配）
# --------------------------------------------------------------------------
# 先 claim 底部：无论这一行写在文件里的哪个位置，只要 -side 是 bottom，
# 它就会从"当前剩余 cavity"的底边切一块下来。
# 这里特意把它放在三个按钮之前调用，用来证明：
#   最终布局结果取决于"每次调用用的是哪个 -side"，
#   而不是单纯取决于"这一行代码写在文件里的先后顺序"。
pack .status -side bottom -fill x

# 再从剩余 cavity 的顶部，依次 claim 三条横向空间给三个按钮。
#   -side top   : 每次都从"当前剩余 cavity"的顶边切一条
#   -fill x     : 控件在水平方向被拉伸，填满它分配到的那条空间的宽度；
#                 注意这里没有写 -fill y，所以按钮高度仍是自身的"自然高度"，
#                 不会被拉伸去填满那条空间的高度。
#   -padx/-pady : 控件外侧留白（像素），这部分空间会被预留，
#                 不会被其它控件占用，但也不计入"剩余 cavity"分配逻辑之外。
pack .open .save .exit -side top -fill x -padx 8 -pady 4
