#!/usr/bin/env wish
#
# network_config_form.tcl —— Layer 6 Complete Example
#
# 场景：一个"网络配置表单"——用户填 Host / Port，勾选 Enable，点 Save。
# 这个例子要回答的核心问题是：
#   用户在 Entry 里敲的字符串，怎么才能安全地变成程序内部可信的数据？
#
# 本文件分成四个部分，顺序和依赖关系很重要（这正是本 Layer 要讲的东西）：
#   第一部分：纯校验逻辑（validate_host / validate_port / validate_config）
#             —— 完全不依赖 Tk，可以单独用 tclsh 测试。
#   第二部分：业务逻辑入口（submit_config）与"内部健全性检查"
#             （assert_internal_consistency）—— 同样不依赖 Tk。
#   第三部分：package require Tk —— 到这里才第一次需要图形环境。
#   第四部分：UI 层（widget 创建、布局、事件回调）
#             —— 只负责"调用业务逻辑 + 把结果显示出来"，
#                本身不包含任何一条校验规则。
#
# 运行方式：
#   有图形环境：   wish network_config_form.tcl
#   headless 环境： tclsh network_config_form.tcl
#       （会一直执行到 package require Tk 这一行才报错——
#        这证明前面所有 Tcl 语法、所有纯逻辑 proc 都是合法的；
#        具体自测方法见 README.md 的 "Execution Walkthrough"。）

# ==========================================================================
# 第一部分：业务状态校验（Business State Validation）—— 不依赖任何 widget
# ==========================================================================
#
# "业务状态"指的是：这份配置数据现在到底有效还是无效，原因是什么。
# 下面这几个 proc 只接受字符串参数、返回 dict，不读取任何 Tk 变量、
# 不引用任何 widget 路径——所以可以脱离 GUI、单独用 tclsh 测试。

# 校验 host 字段：目前规则很简单——不能是空字符串。
# 真实项目里这里还可以加格式校验（IPv4 / IPv6 / hostname 正则），
# 但本 Layer 的重点是"校验链路怎么搭"，不是"校验规则有多复杂"。
proc validate_host {host} {
    if {$host eq ""} {
        return [dict create valid 0 field "Host" reason "不能为空"]
    }
    return [dict create valid 1 field "Host" reason ""]
}

# 校验 port 字段，分三层检查，顺序很重要：
#   1. 空值检查（最常见的"什么都没填"）
#   2. 格式检查：必须是严格整数（string is integer -strict）
#      —— "-strict" 很关键：不加它，空字符串 "" 也会被判定为"合法整数"，
#         这会让空值检查形同虚设，所以两层检查都必须保留。
#   3. 范围检查：TCP/UDP 端口号的合法范围是 1-65535
proc validate_port {port} {
    if {$port eq ""} {
        return [dict create valid 0 field "Port" reason "不能为空"]
    }
    if {![string is integer -strict $port]} {
        return [dict create valid 0 field "Port" \
            reason "必须是合法整数（当前输入：\"$port\"）"]
    }
    # 到这里 $port 已确定是合法整数字符串，可以安全地做数值比较。
    if {$port < 1 || $port > 65535} {
        return [dict create valid 0 field "Port" \
            reason "必须在 1-65535 范围内（当前输入：$port）"]
    }
    return [dict create valid 1 field "Port" reason ""]
}

# 组合校验：对外的唯一入口。
# 采用"短路"策略——先出错的字段先报，不会把两个字段的错误揉进一条消息，
# 这样 UI 上一次只会看到一条明确、可定位到具体字段的错误。
proc validate_config {host port} {
    set hostResult [validate_host $host]
    if {![dict get $hostResult valid]} {
        return $hostResult
    }
    set portResult [validate_port $port]
    if {![dict get $portResult valid]} {
        return $portResult
    }
    return [dict create valid 1 field "" reason ""]
}

# ==========================================================================
# 第二部分：业务逻辑入口 —— 把"校验结果"变成"结构化配置"或"错误描述"
# ==========================================================================
#
# 这一部分演示本 Layer 最关键的区分：
#   - 用户输入错误（user_error）：预期内会发生，validate_config 用返回值
#     （不是异常）表达，调用方按正常控制流处理即可，UI 上温和提示。
#   - 程序运行错误（internal_error）：不该发生、一旦发生就是 bug 或环境问题，
#     用 Tcl 的 error / catch 机制表达，调用方必须用 catch 兜底，
#     否则整个解释器会在这里中止；UI 上只给通用提示，技术细节留给 stderr。

# assert_internal_consistency 不是"业务校验"，是"程序自检"。
# 正常流程里它永远应该通过；之所以还要写它、还要演示它失败的样子，
# 是因为真实项目里 widget 的 -variable、trace、多处 set 很容易让状态
# 在你没注意的地方被改坏——这种 bug 不该用温和的 UI 提示掩盖，
# 而应该大声失败（fail loud），所以这里用 error 而不是返回 dict。
#
# 调试触发词：把 Host 填成 "CRASH"（大小写不敏感），
# 可以在不改代码的情况下手动触发一次"程序错误"路径，
# 用来对比 catch 分支和用户输入错误分支在 UI 上的表现差异。
proc assert_internal_consistency {host port enable} {
    if {[string equal -nocase $host "CRASH"]} {
        error "internal invariant violated: debug trigger 'CRASH' hit\
            assert_internal_consistency (host=$host port=$port enable=$enable)"
    }
    # enable 理论上只会是 "0" 或 "1"（Tk checkbutton 的 -onvalue/-offvalue 保证）。
    # 如果出现别的值，说明是代码里某处直接 set 了这个变量、绕开了 widget，
    # 这是程序 bug，不是用户输入问题，所以同样用 error 而不是返回 dict。
    if {$enable ne "0" && $enable ne "1"} {
        error "internal invariant violated: enable expected 0 or 1, got \"$enable\""
    }
    return 1
}

# submit_config 是 UI 层唯一允许调用的"业务逻辑总入口"。
# 它的返回值永远是一个结构固定的 dict，UI 层只需要读这个 dict，
# 完全不需要知道里面发生过几层校验、有没有触发 catch。
#
# 返回 dict 的字段约定：
#   ok      : 1 = 成功，0 = 失败
#   kind    : success / user_error / internal_error
#   field   : 出错字段名（仅 user_error 时有意义），其余情况是 ""
#   message : 给 UI 显示的文案（user_error / success 都是温和友好的措辞；
#             internal_error 时是一句不暴露内部细节的通用提示）
#   cfg     : 仅 ok=1 时存在，是最终生成的配置 dict
proc submit_config {host port enable} {
    # ---- 第一步：用户输入校验（预期内的错误，走正常返回值，不走异常） ----
    set result [validate_config $host $port]
    if {![dict get $result valid]} {
        set field  [dict get $result field]
        set reason [dict get $result reason]
        return [dict create \
            ok 0 kind user_error field $field \
            message "$field $reason"]
    }

    # ---- 第二步：内部自检 + 构造结构化配置（可能抛出"程序错误"） ----
    if {[catch {
        assert_internal_consistency $host $port $enable
        set cfg [dict create host $host port $port enable $enable]
    } errMsg]} {
        # 这里才是真正的"意外情况"：不是用户的锅。
        # 给开发者的技术细节打到 stderr（终端可见，用户不需要看）；
        # 给用户的 UI 文案保持通用、不技术化，避免暴露内部实现细节。
        puts stderr "\[INTERNAL ERROR\] submit_config: $errMsg"
        return [dict create \
            ok 0 kind internal_error field "" \
            message "内部错误：操作未完成，详情已记录到终端日志，请联系开发者"]
    }

    # ---- 第三步：全部通过——这就是本次提交唯一的"业务状态"产出 ----
    # 用 puts 模拟"把结构化数据交给下一层业务逻辑"
    # （真实项目里，这里可能是写配置文件、发 HTTP 请求、下发到设备等）。
    puts "提交给业务逻辑的配置：$cfg"
    return [dict create \
        ok 1 kind success field "" \
        message "配置已保存：host=$host, port=$port, enable=$enable" \
        cfg $cfg]
}

# ==========================================================================
# 第三部分：到这里才第一次需要图形环境
# ==========================================================================
# headless（tclsh）环境下，脚本会在下面这一行报错并中止——
# 这正好证明：上面所有校验逻辑、业务逻辑在语法和逻辑上都独立于 Tk，
# 不需要图形环境就能被完整测试（见 README.md 的自测示例）。
package require Tk

# ==========================================================================
# 第四部分：UI 状态与 widget —— 由业务结果驱动，绝不替业务逻辑做决定
# ==========================================================================
#
# "UI 状态"指的是：输入框里现在显示什么文字、状态标签是什么颜色/什么文案。
# 下面这几个全局变量只是"表现层缓存"：它们的值永远来自业务逻辑的返回结果，
# 从不被业务逻辑反向读取——submit_config 从头到尾都没有引用过它们。

set hostVar    ""
set portVar    ""
set enableVar  0
set statusText ""

wm title    . "Network Config Form"
wm geometry . 440x220
wm minsize  . 400 200

# ---- 表单区：Host / Port / Enable ----
label .lblHost -text "Host:" -anchor e
entry .entHost -textvariable hostVar -width 26

label .lblPort -text "Port:" -anchor e
entry .entPort -textvariable portVar -width 26

checkbutton .chkEnable -text "Enable" -variable enableVar \
    -onvalue 1 -offvalue 0

# 状态提示标签：颜色由 render_result 统一管理，这里只给一个初始默认值，
# 不在创建时写死"将来会用到的某种状态颜色"。
label .lblStatus -textvariable statusText -anchor w \
    -justify left -wraplength 400 -fg black

# ---- 按钮区 ----
button .btnSave  -text "Save"  -command on_save_clicked
button .btnReset -text "Reset" -command on_reset_clicked

grid .lblHost     -row 0 -column 0 -sticky e  -padx 10 -pady 8
grid .entHost     -row 0 -column 1 -columnspan 2 -sticky w -padx 10 -pady 8
grid .lblPort     -row 1 -column 0 -sticky e  -padx 10 -pady 8
grid .entPort     -row 1 -column 1 -columnspan 2 -sticky w -padx 10 -pady 8
grid .chkEnable   -row 2 -column 1 -sticky w  -padx 10 -pady 4
grid .btnSave     -row 3 -column 1 -sticky e  -padx 10 -pady 10
grid .btnReset    -row 3 -column 2 -sticky w  -padx 10 -pady 10
grid .lblStatus   -row 4 -column 0 -columnspan 3 -sticky we -padx 10 -pady 8

# ---- 事件回调：只做"读 Tk 变量 -> 调业务逻辑 -> 把结果喂给 UI" ----
# 回调本身不包含任何一条校验规则，这是故意的：
# 校验规则只应该存在于第一部分，回调只负责"接线"。
proc on_save_clicked {} {
    global hostVar portVar enableVar
    set result [submit_config $hostVar $portVar $enableVar]
    render_result $result
}

proc on_reset_clicked {} {
    global hostVar portVar enableVar
    set hostVar   ""
    set portVar   ""
    set enableVar 0
    clear_status
}

# render_result 是"业务状态 -> UI 状态"的唯一翻译点。
# 它只读 submit_config 返回的 dict，从不读 hostVar/portVar 等 widget 变量，
# 这样业务逻辑和 UI 之间就只通过这一个 dict 结构耦合，职责边界非常清楚：
# 业务逻辑不知道 widget 的存在，UI 不知道校验规则的存在。
proc render_result {result} {
    global statusText
    set statusText [dict get $result message]
    switch -- [dict get $result kind] {
        success {
            .lblStatus configure -fg darkgreen
        }
        user_error {
            .lblStatus configure -fg red
        }
        internal_error {
            .lblStatus configure -fg darkorange
        }
        default {
            .lblStatus configure -fg black
        }
    }
}

proc clear_status {} {
    global statusText
    set statusText ""
    .lblStatus configure -fg black
}
