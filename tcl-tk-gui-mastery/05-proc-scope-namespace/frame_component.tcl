#!/usr/bin/env wish
#
# frame_component.tcl —— Layer 5 Complete Example (3/3): frame 封装可复用组件
#
# 场景：还是同一个"配置窗口"（Name / Port + Save 按钮），但这次把它做成
# 一个真正"可复用的组件"——可以在同一个程序里创建任意多个互不干扰的实例，
# 每个实例各自记住各自的 name/port，互相之间不会覆盖或污染。
#
# 核心技巧：
#   namespace_style.tcl 里 ::configwin::name 之所以只能有一份，
#   是因为它是一个"标量变量"，一个名字只能对应一份存储。
#   这里改用"数组"，并且用 widget 路径本身（例如 ".panel1" / ".panel2"）
#   当数组的下标（key）。widget 路径在 Tk 里天然保证唯一
#   （兄弟控件不能同名，不同父路径下的同名子控件路径也不同），
#   所以它是一个现成的、不需要额外设计的"实例 ID"。
#   这样，同一个 namespace 下的同一组 proc，就可以通过"传入哪个
#   widget 路径"来区分到底在操作哪一个实例的状态——
#   proc（行为）只写一份，状态按 key 分开存，这就是"实例化"的本质。
#
# 这也回答了一个常见疑问：Tcl 没有 class/object 语法，要怎么做到
# "面向对象"里"每个对象有自己的状态"？答案是：
#   - "状态"用 namespace 数组（以 widget 路径为 key）保存；
#   - "行为"用这个 namespace 下的 proc 提供（所有实例共享同一份代码）；
#   - "实例标识"直接复用 widget 路径（Tk 已经保证了它的唯一性）。
#   这三者合在一起，已经完整表达了"对象"概念里最核心的三要素，
#   不需要再引入一套类/继承/多态的语法层来解决这个问题。
#
# 运行方式：
#   有图形环境：   wish frame_component.tcl
#   headless 环境： tclsh frame_component.tcl
#
# 关于验证方式的重要说明（请在校验本文件时特别注意）：
#   下面的 InitState / getData / setData / onSave 这四个 proc
#   只操作 namespace 数组，不调用任何 Tk 命令，可以直接用一个
#   "假装是 widget 路径的普通字符串"（比如字符串 ".panel1"）当 key 来调用，
#   在 tclsh 下完整验证其正确性——不需要真的创建出一个 frame 控件。
#   只有 ConfigWidget::create 这个 proc 内部会调用 package require Tk
#   和真正的 frame/label/entry/button 命令，这一步必须要有图形环境才能
#   跑完整。所以本文件把 "纯逻辑自测"（用假 key，不建真控件）和
#   "真实建两个组件实例"（用 create，需要 Tk）明确分成了两段，
#   在 headless tclsh 下运行时，前一段会完整跑完并打印验证结果，
#   后一段才会在 package require Tk 处报错退出。

# ==========================================================================
# 组件定义：namespace ConfigWidget
# ==========================================================================
namespace eval ConfigWidget {
    namespace export create getData setData onSave destroy

    # variable state 以外，这里用三个并列的数组，分别存：
    #   name($w)      —— 这个实例的 Name 字段
    #   port($w)      —— 这个实例的 Port 字段
    #   saveCb($w)    —— 这个实例注册的"保存回调"（可能是空字符串）
    # 下标 $w 永远是这个实例对应的 widget 路径，例如 ".panel1"。
    # 之所以拆成三个数组而不是一个"数组存 dict"，是为了方便
    # entry 控件直接用 -textvariable 绑定到 name($w) / port($w) 这样的
    # 数组元素上，用户在输入框里打字时会自动同步进这里，不需要额外代码。
    variable name
    variable port
    variable saveCb
    array set name   {}
    array set port   {}
    array set saveCb {}
}

# --------------------------------------------------------------------------
# 初始化状态：给一个新实例的 name/port/saveCb 设置默认值
# --------------------------------------------------------------------------
# 这个 proc 完全不涉及 Tk —— 它只是往三个数组里写值，
# 所以可以直接传一个任意字符串当 w 来测试，不需要先有真实控件存在。
proc ConfigWidget::InitState {w {initName "Unnamed"} {initPort 8080}} {
    variable name
    variable port
    variable saveCb
    set name($w)   $initName
    set port($w)   $initPort
    set saveCb($w) {}
}

# --------------------------------------------------------------------------
# 获取组件数据：返回一个 dict，不关心 w 对应的控件是否真的存在
# --------------------------------------------------------------------------
proc ConfigWidget::getData {w} {
    variable name
    variable port
    return [dict create name $name($w) port $port($w)]
}

# --------------------------------------------------------------------------
# 更新组件数据：直接写数组元素。
# 如果这个实例确实对应一个真实存在的 entry 控件（-textvariable 已经绑定
# 到同一个数组元素），这里一改，界面上输入框的内容会立刻跟着变——
# 这是 Layer 3 讲过的"变量驱动界面"在组件场景下的自然延伸。
# 但即使控件根本不存在（纯逻辑测试场景），这个 proc 本身依然能正确工作。
# --------------------------------------------------------------------------
proc ConfigWidget::setData {w data} {
    variable name
    variable port
    if {[dict exists $data name]} {
        set name($w) [dict get $data name]
    }
    if {[dict exists $data port]} {
        set port($w) [dict get $data port]
    }
}

# --------------------------------------------------------------------------
# 注册保存回调：外部通过这个接口往实例里"注入"一段行为，
# 而不需要修改 ConfigWidget 本身的代码——这是组件封装的典型用法。
# callback 约定是一个 apply 风格的 lambda：{data {...}}。
# --------------------------------------------------------------------------
proc ConfigWidget::onSave {w callback} {
    variable saveCb
    set saveCb($w) $callback
}

# --------------------------------------------------------------------------
# 内部辅助：真正触发保存回调（由 Save 按钮的 -command 调用，
# 只有在真实 GUI 环境下、用户点击按钮时才会被执行；
# 在本文件的 headless 校验流程中不会被触发）。
# --------------------------------------------------------------------------
proc ConfigWidget::InvokeSave {w} {
    variable saveCb
    if {[info exists saveCb($w)] && $saveCb($w) ne {}} {
        uplevel #0 [list apply $saveCb($w) [getData $w]]
    }
}

# --------------------------------------------------------------------------
# 创建组件：这是唯一真正依赖 Tk 的 proc。
# parent 由调用者指定（例如 "."），组件自己的 widget 路径由调用者传入的
# 名字决定——这里简化为"调用者直接传完整路径"（例如 ".panel1"），
# 实际项目里更常见的做法是 create {parent name} 然后内部拼出
# "$parent.$name"，这里为了保持示例简单，直接把 w 当成完整路径使用。
# --------------------------------------------------------------------------
proc ConfigWidget::create {w} {
    # package require Tk 被推迟到这里：只有真正要创建控件时才需要 Tk，
    # 纯状态管理逻辑（上面几个 proc）完全不受这一行影响。
    package require Tk

    frame $w -borderwidth 2 -relief groove -padx 6 -pady 6

    label $w.lname -text "Name:"
    # -textvariable 的值必须是"完整限定的数组元素名"，
    # 格式是 命名空间路径::数组名(下标)，这里下标就是 w 本身。
    entry $w.ename -textvariable ::ConfigWidget::name($w)
    label $w.lport -text "Port:"
    entry $w.eport -textvariable ::ConfigWidget::port($w)
    button $w.bsave -text "Save" -command [list ConfigWidget::InvokeSave $w]

    grid $w.lname $w.ename -sticky w -padx 4 -pady 2
    grid $w.lport $w.eport -sticky w -padx 4 -pady 2
    grid $w.bsave -columnspan 2 -pady 6

    InitState $w
    return $w
}

# --------------------------------------------------------------------------
# 销毁组件：先清理这个实例在 namespace 数组里占的状态，再销毁 widget 本身。
#
# 注意这里为什么要写 "::destroy $w" 而不是直接写 "destroy $w"：
# 这个 proc 自己就叫 ConfigWidget::destroy。Tcl 对"未加前缀的命令名"的
# 查找顺序是：先查当前命名空间（这里是 ConfigWidget），再查全局命名空间。
# 如果在这个 proc 内部直接写 "destroy $w"，Tcl 会先在 ConfigWidget 这个
# 命名空间里找名叫 destroy 的命令——结果找到的就是它自己，
# 于是变成无限递归调用自己，而不是调用 Tk 内置的那个全局 destroy 命令。
# 这是命名空间里一个非常典型、容易踩到的"命令名遮蔽"（shadowing）陷阱，
# 解决办法就是显式写 ::destroy，强制从全局命名空间开始查找。
# --------------------------------------------------------------------------
proc ConfigWidget::destroy {w} {
    variable name
    variable port
    variable saveCb
    unset -nocomplain name($w)
    unset -nocomplain port($w)
    unset -nocomplain saveCb($w)
    if {[info commands $w] ne {}} {
        ::destroy $w
    }
}

# ==========================================================================
# 纯逻辑自测（不依赖 Tk）：用两个"假装是 widget 路径"的字符串当 key，
# 证明同一组 proc 可以同时服务两个互不干扰的实例。
# ==========================================================================
puts "=== pure logic self-test (no Tk, fake widget-path keys) ==="

ConfigWidget::InitState .panel1 "device-A" 8001
ConfigWidget::InitState .panel2 "device-B" 8002

puts "panel1 initial: [ConfigWidget::getData .panel1]"
puts "panel2 initial: [ConfigWidget::getData .panel2]"

ConfigWidget::setData .panel1 {name "device-A-renamed" port 9001}

puts "panel1 after setData: [ConfigWidget::getData .panel1]"
puts "panel2 after panel1's setData (should be unchanged): [ConfigWidget::getData .panel2]"

set p1 [ConfigWidget::getData .panel1]
set p2 [ConfigWidget::getData .panel2]
if {[dict get $p1 name] eq "device-A-renamed" && [dict get $p1 port] == 9001
        && [dict get $p2 name] eq "device-B" && [dict get $p2 port] == 8002} {
    puts "self-test PASS: two instances are fully independent"
} else {
    puts "self-test FAIL: instance isolation broken"
}

# onSave 注册 + 手动触发（不经过真实按钮点击，直接调用内部辅助 proc，
# 用来验证"回调注入"这一机制本身在没有 Tk 的情况下也是可用的）。
ConfigWidget::onSave .panel1 {data {puts "panel1 saved callback got: $data"}}
ConfigWidget::onSave .panel2 {data {puts "panel2 saved callback got: $data"}}
ConfigWidget::InvokeSave .panel1
ConfigWidget::InvokeSave .panel2

# destroy 的"状态清理"部分同样不依赖 Tk：这里故意不满足
# "[info commands $w] ne {}" 这个条件（因为 .panel1 根本不是真实命令），
# 所以只会清掉数组里的状态，不会尝试调用 ::destroy。
ConfigWidget::destroy .panel1
if {[catch {ConfigWidget::getData .panel1} errMsg]} {
    puts "panel1 state cleaned up correctly, getData now errors: $errMsg"
} else {
    puts "self-test FAIL: panel1 state should have been removed"
}
puts "panel2 still intact after panel1 destroyed: [ConfigWidget::getData .panel2]"

# ==========================================================================
# 真实创建两个组件实例（这一步需要 Tk，headless tclsh 会在这里报错退出）
# ==========================================================================
puts "=== creating two real widget instances (requires display) ==="

set panelA [ConfigWidget::create .panelA]
set panelB [ConfigWidget::create .panelB]

ConfigWidget::setData $panelA [dict create name "device-A" port 8001]
ConfigWidget::setData $panelB [dict create name "device-B" port 8002]

ConfigWidget::onSave $panelA {data {puts "panelA Save clicked: $data"}}
ConfigWidget::onSave $panelB {data {puts "panelB Save clicked: $data"}}

pack $panelA -side left -padx 8 -pady 8
pack $panelB -side left -padx 8 -pady 8

puts "panelA data: [ConfigWidget::getData $panelA]"
puts "panelB data: [ConfigWidget::getData $panelB]"
