#!/usr/bin/env wish
#
# namespace_style.tcl —— Layer 5 Complete Example (2/3): namespace eval 组织模块
#
# 场景：和 global_style.tcl 完全一样的"配置窗口"
# （Name / Port 两个输入框 + 一个 Save 按钮），但这次把状态变量和过程
# 都搬进一个具名命名空间 ::configwin 里，不再直接污染全局命名空间 ::。
#
# 本文件要演示：
#   1. namespace eval 把一组相关的变量和过程"打包"在一个独立的名字空间里，
#      外部要访问必须写完整路径 ::configwin::xxx（或先 namespace import）。
#   2. variable（在 namespace eval 内部使用）声明的是"这个命名空间自己的
#      持久变量"，和 proc 内部的局部变量、和 :: 全局变量是三套完全独立的
#      存储，互不冲突。
#   3. namespace export 声明"这个命名空间愿意公开给外部使用的接口"，
#      搭配 namespace import 可以让调用方用短名字访问这些接口。
#   4. 这样确实解决了 global_style.tcl 的"命名污染"问题，但仍然只能有
#      "一份"状态——::configwin::name / ::configwin::port 在整个进程里
#      只存在一份，如果要同时开两个独立的配置窗口，依然做不到。
#
# 运行方式：
#   有图形环境：   wish namespace_style.tcl
#   headless 环境： tclsh namespace_style.tcl
#     —— 和 global_style.tcl 一样，package require Tk 被推迟到
#        ::configwin::Build 内部，所以纯逻辑自测会先完整跑完。

# --------------------------------------------------------------------------
# 用 namespace eval 定义模块：状态变量 + 导出列表
# --------------------------------------------------------------------------
namespace eval ::configwin {
    # namespace export 只是"登记"这几个名字是公共接口，
    # 真正要生效（让外部可以用短名字调用）还需要调用方执行 namespace import。
    namespace export Save Load Build

    # variable name / variable port：
    # 这是在命名空间内部声明"命名空间变量"，它们的生命周期和整个
    # ::configwin 命名空间一样长（进程运行期间一直存在），
    # 可以被这个命名空间下的任何 proc 直接看到（只要那个 proc 里也
    # 写一句 variable name / variable port 来建立链接）。
    variable name "device-01"
    variable port 8080
}

# --------------------------------------------------------------------------
# proc 定义写在 ::configwin:: 前缀下，等价于把这个 proc "放进"该命名空间
# --------------------------------------------------------------------------
proc ::configwin::Save {} {
    # 这里的 variable（不加 namespace eval）是在 proc 内部使用，
    # 作用和 global 很像，但链接的目标不是 :: 全局变量，
    # 而是"当前 proc 所属命名空间"里的同名命名空间变量。
    variable name
    variable port
    set result [dict create name $name port $port]
    puts "configwin::Save -> $result"
    return $result
}

proc ::configwin::Load {} {
    variable name
    variable port
    set name "device-01"
    set port 8080
}

proc ::configwin::Build {} {
    package require Tk
    variable name
    variable port

    wm title . "Config (namespace style)"
    wm geometry . 260x120

    label .lname -text "Name:"
    # -textvariable 必须给完整路径 ::configwin::name，
    # 否则 Tk 会在全局命名空间下找一个叫 name 的变量，而不是我们想要的那个。
    entry .ename -textvariable ::configwin::name
    label .lport -text "Port:"
    entry .eport -textvariable ::configwin::port
    button .bsave -text "Save" -command ::configwin::Save

    grid .lname .ename -sticky w -padx 4 -pady 2
    grid .lport .eport -sticky w -padx 4 -pady 2
    grid .bsave -columnspan 2 -pady 6
}

# --------------------------------------------------------------------------
# 纯逻辑自测（不依赖 Tk）：直接操作命名空间变量，验证 Save/Load 的正确性
# --------------------------------------------------------------------------
puts "=== pure logic self-test (no Tk) ==="
set ::configwin::name "edge-09"
set ::configwin::port 7000
set saved [::configwin::Save]
if {[dict get $saved name] eq "edge-09" && [dict get $saved port] == 7000} {
    puts "self-test PASS: configwin::Save correctly reads namespace state"
} else {
    puts "self-test FAIL: configwin::Save did not read expected state"
}
::configwin::Load
puts "after Load -> name=$::configwin::name port=$::configwin::port"

# --------------------------------------------------------------------------
# 演示 namespace export + namespace import：
# 在另一个命名空间 ::caller 里，把 ::configwin 导出的接口"导入"进来，
# 之后就可以直接写 Save / Load（短名字），不用再写 ::configwin:: 前缀。
# --------------------------------------------------------------------------
namespace eval ::caller {
    namespace import ::configwin::Save
    namespace import ::configwin::Load
}
puts "=== namespace import demo ==="
puts "calling imported Save() from ::caller: [::caller::Save]"

# --------------------------------------------------------------------------
# 演示"仍然只能有一份状态"的局限（只写注释，不实际执行）：
#
# ::configwin::name / ::configwin::port 在整个进程里只有一份存储。
# 如果想同时打开两个独立的配置窗口，各自记住各自的 name/port，
# 用 namespace 这种写法唯一的办法是再复制一份代码出来，
# 变成 ::configwin2，这和 global_style.tcl 复制 configName1/configName2
# 本质上是同一个问题，只是换了一层包装——命名污染被解决了，
# 但"实例化"（instantiation）这个需求，namespace 本身并不提供。
# frame_component.tcl 会用"widget 路径当 key"的方式彻底解决这一点。
# --------------------------------------------------------------------------

puts "=== building real Tk window (will fail here under headless tclsh) ==="
::configwin::Build
