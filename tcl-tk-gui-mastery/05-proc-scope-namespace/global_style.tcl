#!/usr/bin/env wish
#
# global_style.tcl —— Layer 5 Complete Example (1/3): 全局变量 + 普通 proc
#
# 场景：一个"配置窗口" —— 两个输入框 (Name / Port) + 一个 Save 按钮。
# 这是最朴素的组织方式：状态是顶层 set 出来的全局变量，
# 行为是几个普通 proc，proc 内部用 global 声明去读写这些全局变量。
#
# 本文件要演示的不是"这样写有多方便"，而是"这样写在工程上会遇到什么问题"：
#   1. 全局命名空间污染 —— configName / configPort 这两个名字，
#      任何其它文件、其它人写的代码，都可能不小心用同名变量把它覆盖掉。
#   2. 无法共存多个实例 —— 如果要同时打开两个配置窗口（例如同时编辑
#      两台设备的配置），两个窗口会共享同一组全局变量，后一个窗口的输入
#      会直接覆盖/污染前一个窗口的状态，没有办法做到"互不干扰"。
#
# 运行方式：
#   有图形环境：   wish global_style.tcl
#   headless 环境： tclsh global_style.tcl
#
# 关于下面的结构安排：
#   本文件故意把 "package require Tk" 推迟到 BuildConfigWindow 这个 proc
#   内部才调用，而不是像 Layer 0/2 那样写在文件最前面。这是为了让
#   "状态管理逻辑"（SaveConfig / LoadDefaults）和"界面构建逻辑"
#   （BuildConfigWindow）在物理上分开：前者完全不涉及任何 Tk 命令，
#   可以在没有图形环境的 tclsh 下完整运行并自测；后者才真正依赖 Tk。
#   所以在 headless 环境下运行本文件时，你会先看到自测部分的 puts 输出，
#   最后才会在尝试真正建窗口时，于 "package require Tk" 处报错退出——
#   这正好证明：这一行之前的所有 Tcl 语法和逻辑都是合法、可执行的。

# --------------------------------------------------------------------------
# 状态：两个全局变量，没有任何命名空间保护
# --------------------------------------------------------------------------
set configName "device-01"
set configPort 8080

# --------------------------------------------------------------------------
# 行为：普通 proc，内部用 global 显式声明要读写哪些全局变量
# --------------------------------------------------------------------------
# Tcl 里每个 proc 调用都会建立一个全新的"局部作用域"，默认情况下
# proc 内部看不到外面 set 过的任何变量——即使外面已经 set configName了，
# proc 内部直接写 "set configName xxx" 操作的也是一个全新的局部变量，
# 和外面那个全局变量完全无关，proc 返回后这个局部变量就被销毁。
#
# global configName configPort 这条语句的作用，不是"赋值"，而是
# "把当前这个局部作用域里 configName / configPort 这两个名字，
#  重新链接到全局作用域（:: 命名空间）里的同名变量"——
# 执行这条语句之后，在这个 proc 内对 configName/configPort 的读写，
# 都会直接作用在全局变量本身上，proc 返回后修改依然保留。
proc SaveConfig {} {
    global configName configPort
    # 这一段纯粹是"状态管理"逻辑：读全局变量，组织成一份数据，打印/返回。
    # 它完全不涉及任何 widget 命令，所以可以在没有 Tk 的 tclsh 下单独验证。
    set result [dict create name $configName port $configPort]
    puts "SaveConfig -> $result"
    return $result
}

proc LoadDefaults {} {
    global configName configPort
    set configName "device-01"
    set configPort 8080
}

# --------------------------------------------------------------------------
# 纯逻辑自测（不依赖 Tk，可以在 headless tclsh 下完整跑通）
# --------------------------------------------------------------------------
puts "=== pure logic self-test (no Tk) ==="
set configName "edge-07"
set configPort 9090
set saved [SaveConfig]
if {[dict get $saved name] eq "edge-07" && [dict get $saved port] == 9090} {
    puts "self-test PASS: SaveConfig correctly reads current global state"
} else {
    puts "self-test FAIL: SaveConfig did not read expected global state"
}
LoadDefaults
puts "after LoadDefaults -> configName=$configName configPort=$configPort"

# --------------------------------------------------------------------------
# 演示"无法共存多个实例"的问题（这里只写注释说明，不实际执行）：
#
# 如果想做两个独立的配置窗口，各自保存各自的 name/port，用这种全局变量
# 风格，你唯一的办法是手写两套变量名（configName1/configPort1、
# configName2/configPort2）和两套 proc（SaveConfig1/SaveConfig2），
# 因为 proc 内部 global 绑定的是写死在代码里的固定名字，不能参数化成
# "绑定到调用者指定的那一组变量"。这就是本文件要暴露的核心问题——
# 下一个文件 namespace_style.tcl 先解决"命名污染"，
# 再下一个文件 frame_component.tcl 才真正解决"多实例互不干扰"。
# --------------------------------------------------------------------------

# --------------------------------------------------------------------------
# 行为：真正建窗口的 proc —— 只有调用这个 proc 时才会第一次用到 Tk
# --------------------------------------------------------------------------
proc BuildConfigWindow {} {
    package require Tk
    global configName configPort

    wm title . "Config (global style)"
    wm geometry . 260x120

    label .lname -text "Name:"
    entry .ename -textvariable configName
    label .lport -text "Port:"
    entry .eport -textvariable configPort
    # -command SaveConfig：按钮点击时执行的是全局命令 SaveConfig，
    # 它内部用 global 读取的也是同一组全局变量——这里再次体现"单例"特性。
    button .bsave -text "Save" -command SaveConfig

    grid .lname .ename -sticky w -padx 4 -pady 2
    grid .lport .eport -sticky w -padx 4 -pady 2
    grid .bsave -columnspan 2 -pady 6
}

puts "=== building real Tk window (will fail here under headless tclsh) ==="
BuildConfigWindow
