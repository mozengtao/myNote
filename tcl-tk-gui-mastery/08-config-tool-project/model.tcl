#
# model.tcl —— Layer 8 综合项目：纯数据层（不依赖 Tk）
#
# 职责（严格限定，不能越界）：
#   - 用一个 namespace 变量（::model::state）保存"当前内存中的配置"。
#   - 提供数据查询/更新接口：model::getConfig、model::setConfig、
#     model::resetToDefault。
#   - 不出现任何 Tk 命令（pack/grid/button/label/...），
#     因此本文件可以单独用 tclsh 加载和测试，不需要图形环境，
#     也不需要知道"配置最终会被画成什么样的 widget"。
#   - 不负责文件 IO（那是 config.tcl 的职责），
#     也不负责"host 是否合法""port 是否在 1-65535 之间"这类业务校验
#     （那也是 config.tcl 的职责）——model.tcl 只做"数据形状"层面的
#     最基本防御：保证 state 里永远有且只有约定好的那几个 key，
#     防止上层因为拼写错误等 bug 把残缺的 dict 存进来。
#

namespace eval ::model {
    # ----------------------------------------------------------------------
    # 约定：配置 dict 的"形状"——所有合法配置必须且只能包含这三个 key。
    # 值在这一层全部按字符串保存（host 本身是字符串；port/enabled
    # 虽然语义上是数字/布尔，但统一存成字符串可以避免 Tcl 内部
    # "字符串 vs 数字"表示形式不一致导致的比较陷阱，交给 config.tcl
    # 在校验时用 string is integer -strict 等方式按语义检查）。
    # ----------------------------------------------------------------------
    variable RequiredKeys {host port enabled}

    # 默认配置：一个全新项目、从未加载过任何文件时的初始状态。
    variable DefaultConfig [dict create \
        host    "127.0.0.1" \
        port    "8080" \
        enabled "0" \
    ]

    # 当前内存中的配置状态，初始值直接复制默认配置。
    # 注意：Tcl 的 dict 是值类型（value semantics），这里的赋值是深拷贝，
    # 之后修改 DefaultConfig 不会影响已经写入 state 的内容，反之亦然。
    variable state $DefaultConfig
}

# ----------------------------------------------------------------------------
# model::shapeIsValid —— 内部辅助过程：检查一个 dict 是否"形状合法"
# ----------------------------------------------------------------------------
# 只检查 key 是否齐全（不多不少），不检查值的语义是否合法。
# 这是故意设计成私有的（命名上没有特殊前缀，但文档里声明"内部使用"），
# 调用方应该始终走 model::setConfig，而不是直接调用本过程。
proc ::model::shapeIsValid {cfg} {
    variable RequiredKeys

    # dict keys 返回的顺序不保证和 RequiredKeys 一致，
    # 所以用排序后比较，而不是直接比较列表相等。
    set gotKeys [lsort [dict keys $cfg]]
    set wantKeys [lsort $RequiredKeys]
    return [expr {$gotKeys eq $wantKeys}]
}

# ----------------------------------------------------------------------------
# model::getConfig —— 查询当前内存中的配置
# ----------------------------------------------------------------------------
# 返回值：一个 dict 的拷贝（因为 dict 是值类型，调用方拿到的是独立副本，
# 对返回值的修改不会"穿透"回 ::model::state，必须显式调用 setConfig
# 才能真正更新内部状态——这是故意的，避免外部代码绕过接口直接改内部数据）。
proc ::model::getConfig {} {
    variable state
    return $state
}

# ----------------------------------------------------------------------------
# model::setConfig —— 用一个新的 dict 整体替换当前配置
# ----------------------------------------------------------------------------
# 典型调用方：ui.tcl 在"保存成功"或"加载成功"之后，用 config.tcl
# 已经校验过的合法 dict 来同步内存状态；或者 config.tcl 自己在
# load 流程里间接触发。
#
# 返回值：1 表示设置成功；0 表示传入的 cfg 形状不合法，设置被拒绝
# （state 保持不变）。调用方应该用 catch 或直接检查返回值处理失败情况，
# 本过程本身不抛异常，这样上层可以用统一的"返回值风格"而不是
# "异常风格"来处理这种可预期的失败。
proc ::model::setConfig {cfg} {
    variable state

    if {![shapeIsValid $cfg]} {
        return 0
    }

    set state $cfg
    return 1
}

# ----------------------------------------------------------------------------
# model::resetToDefault —— 把内存配置重置为出厂默认值
# ----------------------------------------------------------------------------
# 返回值：重置后的配置 dict（方便调用方立刻拿到新值去刷新界面，
# 不用再额外调一次 getConfig）。
proc ::model::resetToDefault {} {
    variable state
    variable DefaultConfig

    set state $DefaultConfig
    return $state
}
