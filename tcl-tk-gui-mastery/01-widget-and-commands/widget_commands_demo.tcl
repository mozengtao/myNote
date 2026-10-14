#!/usr/bin/env wish
#
# widget_commands_demo.tcl —— Layer 1 Complete Example
#
# 核心问题：为什么 Tk 的控件可以通过 Tcl 命令创建、配置、查询和操作？
#
# 本文件要演示的核心机制只有一句话：
#   每创建一个 widget（例如 button .save ...），Tk 内部就会调用类似
#   Tcl_CreateCommand 的 C API，把这个 widget 的路径（例如 ".save"）
#   注册成当前 Tcl 解释器里的一个新命令。
#   之后 ".save configure ..." / ".save cget ..." / ".save invoke" 这些写法，
#   本质上都是在调用同一个命令 ".save"，第一个参数（subcommand）决定具体行为，
#   这是一种"对象风格"的命令分派，和普通 proc 完全不同（见 README 第 4 点）。
#
# 运行方式：
#   有图形环境：   wish widget_commands_demo.tcl
#   headless 环境： tclsh widget_commands_demo.tcl
#       本文件从第一行可执行语句开始就需要真正的 Tk（因为要创建 widget），
#       所以在没有 $DISPLAY 的机器上，tclsh 只能校验到 "package require Tk"
#       这一行——如果报错点恰好停在这一行（提示连接不上显示），而不是更早
#       出现 "missing close-brace" / "invalid command name" 之类的语法错误，
#       就说明这一行之前（含这一行本身）的所有 Tcl 语法都是合法的。
#
# 重要说明：下面演示的 "创建 -> configure -> cget -> invoke" 这套使用模式，
# 在任何真正支持 Tk 的环境下（Linux + X11、Windows、macOS）语义完全相同。
# 本文件在编写、校验时所在的终端看不到真实窗口，但代码本身与在有图形界面
# 的机器上用 wish 运行时逐字相同——这正是"控件即命令"思想的好处之一：
# 控件的命令接口是跨平台一致的，变化的只是底层窗口系统如何渲染它。
package require Tk

wm title    . "Layer 1 - Widget and Commands Demo"
wm geometry . 480x420

# ===========================================================================
# Part A：最小复现 —— widget 路径即命令
# ===========================================================================
# 这是 Layer 1 要讲的核心模式，先用最少的代码把它完整过一遍。
puts "===== Part A: .save pattern ====="

# 创建：button .save ... 同时完成两件事——
#   1) 在 widget 树里挂一个新的按钮控件，路径是 ".save"（父窗口是根窗口 "."）；
#   2) 在 Tcl 解释器的命令表里注册一个新命令，名字就是这个路径 ".save"。
# -command 的值 {puts "Saved"} 只是被原样存成一段 Tcl 脚本，此刻不会执行。
button .save -text "Save" -command {puts "Saved"}

# cget 的职责：查询"单个"选项的当前值，返回值就是那个值本身（不是列表）。
puts "A1 create -> \[.save cget -text\] = [.save cget -text]"

# configure 不带任何 "-选项" 参数时，返回该选项的完整信息列表：
#   {选项名 数据库名 数据库类 默认值 当前值}
# 这是 configure 与 cget 最大的职责区别：
#   configure 面向"设置 + 查全部元信息"，cget 面向"只要当前值"。
puts "A2 configure -text (query full info) -> [.save configure -text]"

# configure 带 "-选项 新值" 时，用于"设置"一个或多个选项，没有返回值。
# 这里改掉按钮上显示的文字。
.save configure -text "Save Changes"

# 再次用 cget 确认刚才的 configure 生效了。
puts "A3 cget -text (after configure) -> [.save cget -text]"

# invoke 的职责：直接调用 -command 关联的脚本，效果等价于"用户点击了这个按钮"，
# 但它是程序主动触发的，不经过真实的鼠标事件、也不经过 Tk 的事件循环——
# 这意味着 invoke 在 headless 环境下（没有鼠标、没有事件循环）同样可用，
# 常被用来做自动化测试或者给按钮"编程触发"一次点击。
puts "A4 calling .save invoke, expect \"Saved\" on the next line ->"
.save invoke

# ===========================================================================
# Part B：Widget 树与路径命名 —— 父子关系如何体现在路径里
# ===========================================================================
# 对应 README 里的这棵树：
#   .
#   |-- .toolbar
#   |   |-- .toolbar.open
#   |   `-- .toolbar.save
#   `-- .content
#       |-- .content.input
#       `-- .content.output
#
# 命名规则很简单：子控件路径 = 父控件路径 + "." + 子控件名。
# 根窗口 "." 是特例——它自己就是一个点，所以它的直接子控件路径
# 是 "." 直接拼上子控件名（不会出现两个连续的点）。
puts "\n===== Part B: widget tree and path naming ====="

# 两个 frame 作为"分组容器"，父窗口都是根窗口 "."。
# frame 本身不显示任何内容，只是一个用来组织布局、承载子控件的容器。
frame .toolbar -borderwidth 1 -relief raised
frame .content -borderwidth 1 -relief flat

# .toolbar 下的两个按钮：路径前缀必须是 ".toolbar"，否则 Tk 找不到父窗口。
# 注意：这里的 ".toolbar.save" 和 Part A 的 ".save" 是两个完全不同的命令——
# Tcl 命令名是整条路径字符串，哪怕末尾的名字一样，路径不同就是不同的命令。
button .toolbar.open -text "Open" -command {puts "Open clicked"}
button .toolbar.save -text "Save" -command {puts "Save (toolbar) clicked"}

# .content 下的 label / entry / text：同样必须以 ".content" 为前缀。
label .content.label  -text "Name:"
entry .content.input
text  .content.output -width 32 -height 4

# 布局（几何管理器是 Layer 2 的主角，这里只做最少量使用，让控件能显示出来）。
pack .toolbar -side top -fill x
pack .content -side top -fill both -expand 1
pack .toolbar.open .toolbar.save -side left -padx 4 -pady 4
pack .content.label  -side top -anchor w
pack .content.input  -side top -fill x -padx 4
pack .content.output -side top -fill both -expand 1 -padx 4 -pady 4

# winfo parent 是 Tk 提供的"查询真实父窗口"的命令，用来验证：
# 路径里编码的父子关系，和 Tk 内部窗口层级（最终对应 X11 窗口层级）是一致的。
puts "B1 winfo parent .toolbar.save -> [winfo parent .toolbar.save]"
puts "B2 winfo parent .content.output -> [winfo parent .content.output]"
puts "B3 .toolbar.save cget -text -> [.toolbar.save cget -text]"

# ===========================================================================
# Part C：entry / text —— 没有 invoke 时，用"等价操作"验证同一套命令模式
# ===========================================================================
# invoke 是 button / checkbutton / radiobutton 特有的子命令。
# entry、text 这类"内容容器"控件没有 invoke，但同样遵循"控件即命令、
# 子命令决定行为"的规则：用 insert / get / delete 读写内容，原理完全一样。
puts "\n===== Part C: entry / text content operations ====="

# entry：configure 修改外观选项，cget 查询外观选项。
.content.input configure -width 20 -foreground blue
puts "C1 entry cget -width (after configure) -> [.content.input cget -width]"

# insert / get 是 entry 的"内容操作"，可以类比成它的"等价于 invoke"的动作。
.content.input insert 0 "Alice"
puts "C2 entry get (after insert) -> [.content.input get]"

# text 同理：configure/cget 管外观，insert/get 管内容。
.content.output configure -wrap word
puts "C3 text cget -wrap -> [.content.output cget -wrap]"
.content.output insert end "Hello from Layer 1.\n"
puts "C4 text get 1.0 end (after insert) -> [string trim [.content.output get 1.0 end]]"

# ===========================================================================
# Part D：listbox / checkbutton / radiobutton / scale / scrollbar
# ===========================================================================
# 这几种控件各自有专属的子命令，但都遵循同一套"路径即命令、子命令分派行为"
# 的模式——这正是本 Layer 想让你建立的直觉：记住这个模式，而不是死记每个
# 控件的选项表。
puts "\n===== Part D: listbox / checkbutton / radiobutton / scale / scrollbar ====="

# listbox：列表选择控件，典型用途是展示一组可滚动、可选中的候选项。
listbox .content.list -height 4
.content.list insert end "Item A" "Item B" "Item C"
pack .content.list -side top -fill x -padx 4 -pady 4
puts "D1 listbox size -> [.content.list size], item 0 -> [.content.list get 0]"

# checkbutton：开/关两态的控件，典型用途是布尔型选项开关；
# -variable 把它和一个 Tcl 变量双向绑定（这是 Layer 3 的核心话题，这里先感受一下）。
set enableLogging 0
checkbutton .content.logging -text "Enable logging" \
    -variable enableLogging -command {puts "logging flag is now $enableLogging"}
pack .content.logging -side top -anchor w
puts "D2 checkbutton cget -text -> [.content.logging cget -text]"
# invoke 对 checkbutton 的效果：切换一次 -variable 绑定的值，并执行 -command。
.content.logging invoke
puts "D3 after invoke, enableLogging = $enableLogging"

# radiobutton：互斥单选控件，典型用途是"一组选项里选且只能选一个"。
# 同一组 radiobutton 共享同一个 -variable，谁被 invoke/点击，变量就变成谁的 -value。
set logLevel info
radiobutton .content.levelInfo  -text "Info"  -variable logLevel -value info
radiobutton .content.levelDebug -text "Debug" -variable logLevel -value debug
pack .content.levelInfo .content.levelDebug -side left -anchor w
.content.levelDebug invoke
puts "D4 after invoke levelDebug, logLevel = $logLevel"

# scale：滑块控件，典型用途是在一个数值范围内选值（音量、进度、阈值等）。
# scale 没有 invoke，但有 set / get 这对"等价操作"：set 改值，get 读当前值。
scale .content.volume -from 0 -to 100 -orient horizontal -length 150
pack .content.volume -side top -pady 4
.content.volume set 42
puts "D5 scale get (after set 42) -> [.content.volume get]"

# scrollbar：滚动条控件，典型用途是配合 listbox/text/canvas 等"内容可能超出
# 可视区域"的控件一起使用；它自己不存内容，只是和目标控件互相调用对方的
# yview/set 子命令来同步"看到哪一段内容"。
scrollbar .content.outputScroll -orient vertical \
    -command {.content.output yview}
.content.output configure -yscrollcommand {.content.outputScroll set}
pack .content.outputScroll -side right -fill y
puts "D6 scrollbar cget -orient -> [.content.outputScroll cget -orient]"

# ===========================================================================
# Part E：三种常见错误（用 catch 捕获，让演示脚本能继续往下跑）
# ===========================================================================
puts "\n===== Part E: common failures ====="

# 错误 1：路径名拼写错误 —— ".svae" 从来没被创建过，根本不是一个命令。
# Tcl 会报 "invalid command name" 这一类错误，和调用任何不存在的命令一样，
# 这恰好说明：widget 路径在 Tcl 眼里和普通命令名没有任何区别。
if {[catch {.svae invoke} errMsg]} {
    puts "E1 typo (.svae invoke) -> $errMsg"
}

# 错误 2：忘记给控件加父路径前缀 —— 这里引用的父路径 ".content.toolbar"
# 从未被创建过（我们只创建过 .content 和 .toolbar，没有 .content.toolbar），
# 所以创建子控件时 Tk 找不到父窗口，创建失败。
if {[catch {button .content.toolbar.hidden -text "Hidden"} errMsg]} {
    puts "E2 missing parent (.content.toolbar.hidden) -> $errMsg"
}

# 错误 3：对已销毁的 widget 继续调用命令 —— destroy 不仅删除了界面上的控件，
# 也把它对应的那个 Tcl 命令从命令表里一起删掉了，之后再调用就和错误 1 一样，
# 变成"调用一个不存在的命令"。
button .temp -text "Temporary"
destroy .temp
if {[catch {.temp cget -text} errMsg]} {
    puts "E3 destroyed widget (.temp cget) -> $errMsg"
}

# ===========================================================================
# 收尾：这是一个以终端输出为主的演示脚本，不是要你长期停留观察的 GUI 界面，
# 所以这里显式调用 exit，不像 Layer 0 的 hello.tcl 那样停在事件循环里等待
# 鼠标点击（事件循环本身是 Layer 4 才深入讲解的话题，这里先不展开）。
# ===========================================================================
puts "\nDemo complete."
exit 0
