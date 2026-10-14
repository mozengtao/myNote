#
# ui.tcl —— Layer 8 综合项目：界面层（依赖 Tk，依赖 model.tcl + config.tcl）
#
# 职责（严格限定，不能越界）：
#   - 创建 Widget（Entry、Checkbutton、Button、Label）。
#   - 管理布局（frame + grid 组织出表单区/按钮区/状态区）。
#   - 注册事件：Save/Load/Reset 按钮的 -command 只调用
#     model::* / config::* 暴露出来的公开过程，本文件里不出现任何
#     裸的 open/read/dict get 之类的"业务逻辑"，也不出现任何
#     "host 是否合法"这类校验逻辑——那些都委托给下层模块。
#   - 展示状态/错误信息，并用 after 实现非阻塞的状态提示自动清除。
#   - 用 ::ui 命名空间隔离自己的 widget 句柄和内部状态（hostVar/
#     portVar/enabledVar/statusVar/statusAfterId），不直接读写
#     ::model::state 这个内部变量，只通过 model::getConfig /
#     model::setConfig / model::resetToDefault 交互。
#
# 本文件假设 model.tcl 和 config.tcl 已经在它之前被 source 过
# （main.tcl 负责保证这个顺序），这里不重复 source，避免重复定义。
#

package require Tk

namespace eval ::ui {
    # ------------------------------------------------------------------
    # 和表单字段双向绑定的 Tcl 变量。
    # 注意：这些变量只属于"界面层"，是用户当前正在编辑、尚未确认保存
    # 的临时状态；它们和 ::model::state 是两份独立的数据，只有在
    # 用户点击 Save/Load/Reset 之后，才会通过 model::setConfig 或
    # model::getConfig 做一次"同步"——这正是本层要演示的
    # "表单临时态 vs 应用真实态"的分离，呼应 Layer 6 的校验思路：
    # 用户输入不可信，必须经过显式的"提交"动作才会影响真实状态。
    # ------------------------------------------------------------------
    variable hostVar    ""
    variable portVar    ""
    variable enabledVar 0

    # 状态/错误提示文字，绑定到状态区的 label。
    variable statusVar ""

    # 当前挂起的 after 定时器 id（用来清空状态提示）。
    # 保存下来是为了：如果用户连续快速点击 Save/Load，
    # 能先用 after cancel 取消上一次还没触发的清空操作，
    # 避免"旧的定时器清空了新的提示文字"这种时序 bug。
    variable statusAfterId ""
}

# ----------------------------------------------------------------------------
# ui::refreshFieldsFromModel —— 用 model 当前状态覆盖表单字段
# ----------------------------------------------------------------------------
# 典型调用时机：程序刚启动、Load 成功之后、Reset 之后——
# 任何"model 的真实状态发生了变化，界面需要跟上"的时刻。
proc ::ui::refreshFieldsFromModel {} {
    variable hostVar
    variable portVar
    variable enabledVar

    set cfg [model::getConfig]
    set hostVar    [dict get $cfg host]
    set portVar    [dict get $cfg port]
    set enabledVar [dict get $cfg enabled]
}

# ----------------------------------------------------------------------------
# ui::collectFieldsAsDict —— 把当前表单字段打包成一个配置 dict
# ----------------------------------------------------------------------------
# 这里打包出来的 dict 不保证合法（比如用户可能把 Port 删空了），
# 合法性检查完全交给 config::validate / config::save 内部再做一次，
# ui.tcl 自己不重复实现校验规则。
proc ::ui::collectFieldsAsDict {} {
    variable hostVar
    variable portVar
    variable enabledVar

    return [dict create \
        host    $hostVar \
        port    $portVar \
        enabled $enabledVar \
    ]
}

# ----------------------------------------------------------------------------
# ui::hasUnsavedChanges —— 判断表单当前内容是否偏离了 model 的真实状态
# ----------------------------------------------------------------------------
# 用于关闭窗口前的确认提示。注意这里只调用 model::getConfig（公开接口），
# 没有直接访问 ::model::state，符合"不依赖对方内部数据结构"的约束。
proc ::ui::hasUnsavedChanges {} {
    set current  [collectFieldsAsDict]
    set baseline [model::getConfig]
    return [expr {$current ne $baseline}]
}

# ----------------------------------------------------------------------------
# ui::clearStatus —— 清空状态提示文字（由 after 延迟调用）
# ----------------------------------------------------------------------------
proc ::ui::clearStatus {} {
    variable statusVar
    variable statusAfterId

    set statusVar ""
    set statusAfterId ""
}

# ----------------------------------------------------------------------------
# ui::setStatus —— 更新状态提示文字和颜色，并安排 3 秒后自动清空
# ----------------------------------------------------------------------------
# isError 为真时显示红色（失败提示），否则显示绿色（成功提示）。
# 用 after 而不是任何形式的阻塞等待（例如 sleep）来实现"3 秒后消失"，
# 这样事件循环完全不会被卡住，用户在这 3 秒内依然可以正常操作界面。
proc ::ui::setStatus {text {isError 0}} {
    variable statusVar
    variable statusAfterId

    set statusVar $text
    if {$isError} {
        .statusFrame.statusLabel configure -fg red
    } else {
        .statusFrame.statusLabel configure -fg {#1a7f37}
    }

    # 如果上一次的自动清空还没触发，先取消它，避免互相打断。
    if {$statusAfterId ne ""} {
        after cancel $statusAfterId
    }
    set statusAfterId [after 3000 ::ui::clearStatus]
}

# ----------------------------------------------------------------------------
# ui::onSave —— Save 按钮回调
# ----------------------------------------------------------------------------
# 流程：收集表单 -> 交给 config::save 校验并写盘 -> 成功则同步回 model
#       （这样 hasUnsavedChanges 之后会判定为"无未保存改动"）。
# 返回值：1 表示保存成功，0 表示失败（校验不通过或文件 IO 失败）。
# 返回值被 onClose 用来决定"用户选择保存后退出"时是否真的可以退出。
proc ::ui::onSave {} {
    set cfg    [collectFieldsAsDict]
    set result [config::save $cfg]

    if {[dict get $result ok]} {
        model::setConfig $cfg
        setStatus "已保存到 [config::defaultPath]" 0
        return 1
    }

    setStatus "保存失败: [dict get $result error]" 1
    return 0
}

# ----------------------------------------------------------------------------
# ui::onLoad —— Load 按钮回调
# ----------------------------------------------------------------------------
# 流程：交给 config::load 读盘并校验 -> 成功则同步进 model，
#       再用 refreshFieldsFromModel 把新值刷到表单上；
#       失败（文件不存在/内容不合法/IO 错误）只更新状态提示，
#       不改变当前表单内容，让用户的未保存编辑不会被意外冲掉。
proc ::ui::onLoad {} {
    set result [config::load]

    if {[dict get $result ok]} {
        model::setConfig [dict get $result data]
        refreshFieldsFromModel
        setStatus "已从 [config::defaultPath] 加载" 0
        return 1
    }

    setStatus "加载失败: [dict get $result error]" 1
    return 0
}

# ----------------------------------------------------------------------------
# ui::onReset —— Reset 按钮回调
# ----------------------------------------------------------------------------
# 把 model 重置为出厂默认值，再刷新表单。注意这一步只影响内存状态，
# 不会自动写盘——"重置"和"保存"是两个独立动作，用户仍然需要再点一次
# Save 才会把默认值落盘，这是故意的，避免一次误触就覆盖磁盘上的文件。
proc ::ui::onReset {} {
    model::resetToDefault
    refreshFieldsFromModel
    setStatus "已重置为默认值（尚未保存到磁盘）" 0
}

# ----------------------------------------------------------------------------
# ui::onClose —— 响应 WM_DELETE_WINDOW（用户点窗口关闭按钮）
# ----------------------------------------------------------------------------
# 和 Layer 7 的"关闭前检查未保存改动"是同一套模式：
#   - 没有未保存改动：直接关闭。
#   - 有未保存改动：弹出三选一确认框（Yes/No/Cancel）：
#       Yes    -> 先尝试保存，保存成功才真正关闭；保存失败则停留在
#                 窗口里，让用户看到错误提示并有机会修正。
#       No     -> 放弃改动，直接关闭。
#       Cancel -> 什么都不做，回到窗口继续编辑。
proc ::ui::onClose {} {
    if {![hasUnsavedChanges]} {
        destroy .
        return
    }

    set answer [tk_messageBox \
        -type yesnocancel \
        -icon warning \
        -title "未保存的更改" \
        -message "当前配置有未保存的更改，是否在退出前保存？"]

    switch -- $answer {
        yes {
            if {[onSave]} {
                destroy .
            }
            # 保存失败时不关闭窗口，状态区已经显示了失败原因。
        }
        no {
            destroy .
        }
        cancel {
            # 什么都不做，回到窗口。
        }
    }
}

# ----------------------------------------------------------------------------
# ui::build —— 创建主窗口的全部 widget 并完成初始布局
# ----------------------------------------------------------------------------
# main.tcl 在完成 source 之后，只需要调用这一个过程就能把整个界面
# 建起来，不需要知道任何 widget 路径或布局细节。
proc ::ui::build {} {
    variable hostVar
    variable portVar
    variable enabledVar
    variable statusVar

    wm title    . "Engineering Config Tool"
    wm geometry . 420x260
    wm resizable . 0 0

    # ------------------------------------------------------------------
    # 阶段 1：表单区（frame .formFrame），用 grid 做两列对齐布局——
    # 左列是字段标签，右列是对应的输入控件，每个字段占一行。
    # ------------------------------------------------------------------
    frame .formFrame -padx 12 -pady 12
    grid .formFrame -row 0 -column 0 -sticky nsew

    label .formFrame.hostLabel -text "Host:" -anchor w
    entry .formFrame.hostEntry -textvariable ::ui::hostVar -width 24
    grid .formFrame.hostLabel -row 0 -column 0 -sticky w -pady 4
    grid .formFrame.hostEntry -row 0 -column 1 -sticky w -pady 4

    label .formFrame.portLabel -text "Port:" -anchor w
    entry .formFrame.portEntry -textvariable ::ui::portVar -width 24
    grid .formFrame.portLabel -row 1 -column 0 -sticky w -pady 4
    grid .formFrame.portEntry -row 1 -column 1 -sticky w -pady 4

    checkbutton .formFrame.enabledCheck \
        -text "Enabled" \
        -variable ::ui::enabledVar \
        -onvalue 1 -offvalue 0
    grid .formFrame.enabledCheck -row 2 -column 0 -columnspan 2 -sticky w -pady 4

    # ------------------------------------------------------------------
    # 阶段 2：按钮区（frame .buttonFrame），三个按钮水平排列，
    # -command 全部只调用 ::ui 自己的回调过程（上面定义的
    # onLoad/onSave/onReset），不直接出现 config::* / model::* 调用，
    # 这样将来如果回调逻辑要加日志、加确认弹窗，只需要改一处。
    # ------------------------------------------------------------------
    frame .buttonFrame -padx 12 -pady 4
    grid .buttonFrame -row 1 -column 0 -sticky ew

    button .buttonFrame.loadButton  -text "Load"  -width 10 -command ::ui::onLoad
    button .buttonFrame.saveButton  -text "Save"  -width 10 -command ::ui::onSave
    button .buttonFrame.resetButton -text "Reset" -width 10 -command ::ui::onReset
    grid .buttonFrame.loadButton  -row 0 -column 0 -padx 4
    grid .buttonFrame.saveButton  -row 0 -column 1 -padx 4
    grid .buttonFrame.resetButton -row 0 -column 2 -padx 4

    # ------------------------------------------------------------------
    # 阶段 3：状态区（frame .statusFrame），一个单行 label，
    # 文字内容绑定 ::ui::statusVar，颜色在 setStatus 里按成功/失败切换。
    # ------------------------------------------------------------------
    frame .statusFrame -padx 12 -pady 8
    grid .statusFrame -row 2 -column 0 -sticky ew

    label .statusFrame.statusLabel \
        -textvariable ::ui::statusVar \
        -anchor w \
        -fg {#1a7f37}
    grid .statusFrame.statusLabel -row 0 -column 0 -sticky w

    # ------------------------------------------------------------------
    # 阶段 4：用 model 当前状态初始化表单（第一次打开时就是默认配置）。
    # ------------------------------------------------------------------
    refreshFieldsFromModel

    # ------------------------------------------------------------------
    # 阶段 5：接管窗口关闭按钮，交给 onClose 做"未保存改动"检查，
    # 而不是让窗口管理器直接销毁窗口。
    # ------------------------------------------------------------------
    wm protocol . WM_DELETE_WINDOW ::ui::onClose
}
