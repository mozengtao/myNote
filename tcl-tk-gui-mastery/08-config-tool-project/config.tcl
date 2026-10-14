#
# config.tcl —— Layer 8 综合项目：配置文件 IO + 校验层（不依赖 Tk）
#
# 职责（严格限定，不能越界）：
#   - 读取配置：从磁盘上的一个简单文本文件解析出配置 dict。
#   - 验证配置：复用 Layer 6 表单验证的思路（host 非空、port 是
#     1-65535 的合法整数、enabled 是 0/1），但这里验证的对象是
#     "即将写入磁盘/刚从磁盘读出"的 dict，而不是 widget 里的
#     -textvariable；因此本文件同样不出现任何 Tk 命令，可以单独用
#     tclsh 加载和测试。
#   - 保存配置：把一个已校验合法的 dict 写回磁盘文件。
#   - 处理文件错误：所有文件操作都用 catch 包住，向上返回结构化的
#     {ok 0 error "..."} 而不是让 Tcl 的 error 异常直接抛到 ui.tcl
#     （ui.tcl 不应该出现任何裸的 catch 文件 IO 的代码，这正是
#     config.tcl 存在的意义）。
#
# 配置文件格式的选择：key=value 文本行格式，而不是 Tcl 原生的
# dict/list 序列化格式。原因：
#   1. 人类可读、可以直接用普通文本编辑器手工修改，不需要懂 Tcl 语法。
#   2. 方便版本控制 diff——每个字段独占一行，改一个字段只影响一行。
#   3. 解析逻辑极其简单可靠：按行 split，按第一个 "=" 切开 key/value，
#      不需要引入额外的 Tcl 解析器信任边界（不会出现"配置文件里混入
#      可执行 Tcl 代码"的安全顾虑，这正是故意不用 source 读配置的原因）。
#

namespace eval ::config {
    # ------------------------------------------------------------------
    # 关键技巧：必须在 namespace eval 的"顶层"（source 这个文件时立刻
    # 执行的那部分代码），而不是在某个 proc 内部，调用 [info script]
    # 来确定本文件自己的目录。
    #
    # 原因（这是一个真实存在的 Tcl 陷阱，也会在 README 的
    # "Common Failure" 一节详细展开）：
    #   [info script] 返回的是"当前正在被 source 的文件路径"，
    #   这个值在 source 命令执行期间是正确的；但如果把
    #   [info script] 写在一个 proc 定义里面，等到将来这个 proc
    #   被别的脚本（例如 main.tcl）调用时，[info script] 返回的会是
    #   "当前最外层正在运行的脚本路径"（通常是 main.tcl 自己），
    #   而不是 config.tcl 的路径——因为 proc 调用时并没有新的
    #   source 帧。所以必须趁 source 这个文件的那一刻，把目录"冻结"
    #   进一个 namespace 变量里，后面所有 proc 都只读这个变量，
    #   不再重新调用 [info script]。
    # ------------------------------------------------------------------
    variable ScriptDir [file dirname [info script]]

    # 默认配置文件路径：和 config.tcl 自己放在同一目录下的 config.sample。
    # 用 config.sample 而不是以点开头的隐藏文件名，是为了在教程里
    # 方便用 ls 直接看到它、方便在文中截图展示，工程上你也可以按需
    # 改成 .myapp-config 之类的隐藏文件。
    variable DefaultPath [file join $ScriptDir "config.sample"]
}

# ----------------------------------------------------------------------------
# config::defaultPath —— 查询默认配置文件的完整路径
# ----------------------------------------------------------------------------
# 暴露成一个公开过程，而不是让 ui.tcl 直接访问 ::config::DefaultPath 这个
# namespace 变量，是为了保持"只通过公开过程交互"的边界——即使将来
# DefaultPath 的计算方式变了（比如改成从环境变量读路径），ui.tcl 的
# 调用方式也完全不用变。
proc ::config::defaultPath {} {
    variable DefaultPath
    return $DefaultPath
}

# ----------------------------------------------------------------------------
# config::validate —— 校验一个配置 dict 的业务语义是否合法
# ----------------------------------------------------------------------------
# 返回值：一个结构化的结果 dict，统一只有两种形态：
#   成功：[dict create ok 1]
#   失败：[dict create ok 0 error "人类可读的中文错误原因"]
#
# 规则（和 Layer 6 的表单验证思路一致）：
#   - host：trim 之后不能是空字符串。
#   - port：必须是严格意义上的整数（string is integer -strict），
#     并且落在 [1, 65535] 闭区间内。
#   - enabled：必须恰好是 "0" 或 "1"（对应 Checkbutton 的
#     -onvalue/-offvalue 约定）。
proc ::config::validate {cfg} {
    # 任何一个必需 key 缺失都直接算校验失败，而不是让 dict get
    # 抛出"key not found"异常——用 catch 兜底，统一走结构化返回。
    if {[catch {dict get $cfg host} host]} {
        return [dict create ok 0 error "缺少 host 字段"]
    }
    if {[catch {dict get $cfg port} port]} {
        return [dict create ok 0 error "缺少 port 字段"]
    }
    if {[catch {dict get $cfg enabled} enabled]} {
        return [dict create ok 0 error "缺少 enabled 字段"]
    }

    if {[string trim $host] eq ""} {
        return [dict create ok 0 error "Host 不能为空"]
    }

    if {![string is integer -strict $port]} {
        return [dict create ok 0 error "Port 必须是整数，当前值: $port"]
    }
    if {$port < 1 || $port > 65535} {
        return [dict create ok 0 error "Port 必须在 1-65535 之间，当前值: $port"]
    }

    if {$enabled ne "0" && $enabled ne "1"} {
        return [dict create ok 0 error "Enabled 字段必须是 0 或 1，当前值: $enabled"]
    }

    return [dict create ok 1]
}

# ----------------------------------------------------------------------------
# config::load —— 从磁盘文件读取并解析配置
# ----------------------------------------------------------------------------
# 参数：path 可省略，省略时使用 config::defaultPath 的返回值。
# 返回值：
#   成功：[dict create ok 1 data $cfg]
#   失败：[dict create ok 0 error "人类可读的中文错误原因"]
#     失败原因包括：文件不存在、无权限读取、文件内容解析出的字段
#     无法通过 config::validate 校验。
proc ::config::load {{path ""}} {
    if {$path eq ""} {
        set path [defaultPath]
    }

    # 文件不存在是"预期内"的常见情况（例如用户第一次运行工具），
    # 单独判断一次是为了给出比"无法打开文件"更清晰友好的提示，
    # 而不是让它混在下面的 catch open 的通用错误信息里。
    if {![file exists $path]} {
        return [dict create ok 0 \
            error "配置文件不存在: $path（可以先用 Save 创建一个）"]
    }

    if {[catch {
        set fh [open $path r]
        set content [read $fh]
        close $fh
    } openErr]} {
        return [dict create ok 0 error "读取配置文件失败: $openErr"]
    }

    # 按行解析 key=value，忽略空行和以 # 开头的注释行。
    array set parsed {}
    foreach line [split $content "\n"] {
        set trimmed [string trim $line]
        if {$trimmed eq "" || [string index $trimmed 0] eq "#"} {
            continue
        }

        set eqPos [string first "=" $trimmed]
        if {$eqPos < 0} {
            # 格式不对的行直接跳过，不让一行脏数据拖垮整个加载流程；
            # 工程上也可以选择在这里直接判失败，这里选择"宽容跳过"
            # 是因为最终还是会在下面用 validate 统一兜底检查字段齐全性。
            continue
        }

        set key   [string trim [string range $trimmed 0 [expr {$eqPos - 1}]]]
        set value [string trim [string range $trimmed [expr {$eqPos + 1}] end]]
        set parsed($key) $value
    }

    if {[catch {
        set cfg [dict create \
            host    $parsed(host) \
            port    $parsed(port) \
            enabled $parsed(enabled) \
        ]
    } shapeErr]} {
        return [dict create ok 0 \
            error "配置文件缺少必需字段 (host/port/enabled): $shapeErr"]
    }

    set result [validate $cfg]
    if {![dict get $result ok]} {
        return [dict create ok 0 \
            error "配置文件内容不合法: [dict get $result error]"]
    }

    return [dict create ok 1 data $cfg]
}

# ----------------------------------------------------------------------------
# config::save —— 校验并把配置 dict 写回磁盘文件
# ----------------------------------------------------------------------------
# 参数：path 可省略，省略时使用 config::defaultPath 的返回值。
# 返回值：
#   成功：[dict create ok 1]
#   失败：[dict create ok 0 error "人类可读的中文错误原因"]
#     失败原因包括：cfg 本身没通过业务校验（这种情况下根本不会
#     触碰磁盘文件，避免把半合法的数据落盘）、目录不存在、
#     没有写权限等文件系统层面的错误。
proc ::config::save {cfg {path ""}} {
    if {$path eq ""} {
        set path [defaultPath]
    }

    set result [validate $cfg]
    if {![dict get $result ok]} {
        return [dict create ok 0 error [dict get $result error]]
    }

    set host    [dict get $cfg host]
    set port    [dict get $cfg port]
    set enabled [dict get $cfg enabled]

    if {[catch {
        set fh [open $path w]
        puts $fh "# 本文件由 08-config-tool-project 自动生成，可以用普通文本编辑器手工修改"
        puts $fh "host=$host"
        puts $fh "port=$port"
        puts $fh "enabled=$enabled"
        close $fh
    } writeErr]} {
        return [dict create ok 0 error "写入配置文件失败: $writeErr"]
    }

    return [dict create ok 1]
}
