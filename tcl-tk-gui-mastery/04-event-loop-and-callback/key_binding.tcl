#!/usr/bin/env wish
#
# key_binding.tcl —— Layer 4 示例 B：Event Binding（bind）与事件替代符号
#
# bind 和 -command 是两种不同的回调登记机制（详见 README "Architecture" 一节）：
#   -command 只能用在"控件自己定义好的、少数几个特定动作"上（比如按钮被点击）；
#   bind 可以把任意一类 X 事件（键盘、鼠标、窗口变化……）绑定到任意 widget
#   路径、任意 widget 类、或者 "all" 这种全局标签上，更通用、更底层。
#
# 本例演示：
#   1. 键盘事件绑定在主窗口 "." 这个具体 widget 实例上。
#   2. 鼠标事件绑定在一个具体的 frame 控件 .pad 上。
#   3. 四个常用事件替代符号：%W  %x  %y  %K  %A
#
# 运行方式：
#   有图形环境：   wish key_binding.tcl
#   headless 环境： tclsh key_binding.tcl  （只能校验到 package require Tk 这一步）

package require Tk

wm title    . "Event Binding Demo"
wm geometry . 380x280

label .hint -justify left -text \
    "把焦点留在这个窗口上，按任意键观察下方蓝色文字；\n在下面灰色方块内点击鼠标观察绿色文字。"
pack .hint -padx 10 -pady {10 6} -anchor w

# textvariable 把 Tcl 变量和 label 的显示内容绑定在一起（Layer 3 讲过的机制，这里复用）
set ::lastKeyInfo   "(no key event yet)"
set ::lastMouseInfo "(no mouse event yet)"

label .keyinfo   -textvariable ::lastKeyInfo   -foreground blue      -anchor w
label .mouseinfo -textvariable ::lastMouseInfo -foreground darkgreen -anchor w
pack .keyinfo   -fill x -padx 10 -pady 2
pack .mouseinfo -fill x -padx 10 -pady 2

# 一个专门用来接收鼠标点击的方块，大小固定，方便直观估算坐标
frame .pad -width 240 -height 100 -background #cccccc -relief sunken -borderwidth 2
pack .pad -padx 10 -pady 10
# frame 默认会把自己"收缩"到刚好包住子控件；因为 .pad 里没有任何子控件，
# 如果不加这一行，.pad 的实际尺寸会被压缩成几乎看不见的小方块。
# pack propagate <container> 0 的意思是：
# "不要根据子控件的需求反过来改变我（容器）自己的 -width/-height"。
pack propagate .pad 0

# --------------------------------------------------------------------------
# 把事件处理逻辑放进具名 proc，而不是直接把业务逻辑写进 bind 的脚本参数里
# --------------------------------------------------------------------------
# 这是工程实践上的推荐做法（见 README "Engineering Practice"）：
# bind 脚本只负责"把事件替代符号转发给 proc 当参数"，
# 真正的逻辑都在 proc 里，方便单独测试、阅读、复用。

proc onKeyPress {W K A} {
    # %K —— 按键的 keysym 名称（符号名，不是字符本身）。
    #        字母/数字键的 keysym 通常恰好等于字符本身，比如按 "a" 键，K 就是 "a"；
    #        但很多键没有对应的可打印字符，比如 Return、Escape、Left、F1，
    #        这些键的 keysym 是固定的符号名字符串："Return"、"Escape"、"Left"、"F1"。
    # %A —— 按键对应的"可插入的可打印字符"（如果这个键能产生字符）。
    #        例子对比：
    #          按下字母键 a        -> K="a"      A="a"
    #          按下 Return         -> K="Return"  A=一个不可见的回车字符（CR）
    #          按下 Escape / Left  -> K="Escape"/"Left"  A=""（空字符串，没有可打印字符）
    if {$A eq ""} {
        set shownA "(none)"
    } else {
        set shownA $A
    }
    set ::lastKeyInfo "KeyPress  W=$W  K=$K  A=$shownA"
}

proc onMouseClick {W x y} {
    # %W —— 事件实际发生的 widget 路径。
    #        这里恒为 ".pad"，因为这个 bind 只注册在 .pad 这一个具体实例上；
    #        如果把同一个 proc 绑定在 widget 类（比如 Frame）或 "all" 标签上，
    #        点击不同控件时，W 会变成"实际被点击的那个" widget 路径。
    # %x %y —— 鼠标指针相对于"事件发生的 widget"（即 W，不是屏幕、也不是主窗口）
    #        左上角的像素坐标。
    #        .pad 是 240x100：
    #          在左上角点击 -> x,y 接近 (0, 0)
    #          在正中间点击 -> x,y 接近 (120, 50)
    #          在右下角点击 -> x,y 接近 (240, 100)
    set ::lastMouseInfo "Button-1  W=$W  x=$x  y=$y"
}

# --------------------------------------------------------------------------
# 绑定 1：键盘事件，绑定在主窗口 "." 这个具体 widget 实例上
# --------------------------------------------------------------------------
# <KeyPress> 匹配任意按键按下（<Key> 是它的同义写法）。
# bind 脚本里的 %W %K %A 会在事件真正触发的那一刻，被 Tk 替换成具体的值，
# 然后整段替换后的文本才会作为一条 Tcl 命令被执行——
# 这正是 README 里要强调的"事件替代发生在命令执行之前"的顺序。
bind . <KeyPress> {onKeyPress %W %K %A}

# --------------------------------------------------------------------------
# 绑定 2：鼠标事件，绑定在 .pad 这个具体 widget 实例上
# --------------------------------------------------------------------------
# <Button-1> 匹配鼠标左键按下（按下的瞬间触发，不需要等松开）。
bind .pad <Button-1> {onMouseClick %W %x %y}

# 必须显式让主窗口获得键盘输入焦点，否则 <KeyPress> 绑定永远不会触发：
# X11 的键盘事件只会送给"当前持有输入焦点"的那个 widget，
# 窗口刚创建出来的时候，焦点归属并不确定（取决于窗口管理器策略）。
focus .

button .quit -text "Quit" -command {destroy .}
pack .quit -pady {4 10}
