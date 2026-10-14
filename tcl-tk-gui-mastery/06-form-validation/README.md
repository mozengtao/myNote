# Layer 6：表单验证与错误处理

核心问题：**用户在 Entry 里敲的字符串，怎么安全地变成程序内部可信的数据？**

---

## 1. Why

前面几个 Layer（3、4、5）已经建立了"Tk 变量绑定 widget" + "事件回调驱动业务逻辑" + "proc/namespace 组织代码"的基础设施。但这些 Layer 都刻意回避了一个问题：**用户输入的字符串，内容是完全不可信的**。

一个 Entry 控件背后的 Tk 变量，可以被用户填成任何字符串——空字符串、带空格的字符串、非数字字符、超出业务范围的数字。如果业务逻辑直接拿这些字符串去做数值运算、拼接命令、下发配置，轻则程序崩溃，重则产生错误的系统行为（例如把 "abc" 当作端口号发给网络协议栈）。

同时还有另一类问题经常被新手混为一谈：**"用户填错了"和"程序本身出 bug 了"是两种性质完全不同的错误**，处理方式也应该完全不同：

- 用户填错 Port（比如填了 "abc"）：这是**意料之中**的，程序应该用校验逻辑**提前拦住**，返回明确、友好的提示，不应该让程序在拿到 "abc" 之后才在某个数值运算里意外崩溃。
- 程序内部状态被破坏（比如某个 proc 假设 `enable` 只会是 `0`/`1`，结果实际拿到了别的值）：这是**意料之外**的，说明代码或状态管理有 bug，应该用 `catch` 兜底防止整个解释器崩溃，但不应该用温柔的提示把这个 bug 悄悄掩盖掉——开发者需要在终端里看到技术细节才能定位问题。

Layer 6 要建立的 Mental Model 就是：**在"用户输入"和"业务逻辑"之间，必须插入一层独立的、可脱离 GUI 测试的校验逻辑；校验失败（用户错误）和程序异常（内部错误）要走两条不同的处理路径，但两条路径最终都要收敛成同一种"安全地反馈给 UI"的方式，不能让任何一种错误直接导致程序崩溃或界面卡死。**

---

## 2. Mental Model

一句话 Mental Model：

> **校验逻辑是业务逻辑的看门人，而不是 UI 的装饰；`catch` 是程序健壮性的最后一道防线，而不是用来掩盖校验逻辑该做的事。**

展开一点说：

- **业务状态**（这份配置到底有效还是无效、值是什么）应该是一组纯 Tcl 数据（字符串 + `dict`），完全不依赖任何 widget，可以单独用 `tclsh` 测试。
- **UI 状态**（输入框显示什么、状态标签是什么颜色/文案）永远是业务状态的"渲染结果"，由一个单一的"翻译函数"负责，不应该让校验规则散落在各个按钮回调里。
- **用户输入错误**走"返回值"（`dict` 里 `kind = user_error`），**程序运行错误**走"异常 + `catch`"（`kind = internal_error`）。两者在 Tcl 语义上的区别，恰好对应到"预期内的失败"和"预期外的失败"这两种现实世界里性质不同的问题。

---

## 3. Architecture

本 Layer 的完整示例 `network_config_form.tcl` 分成三层，层与层之间只通过明确的数据结构（字符串参数、`dict` 返回值）通信，没有任何一层直接伸手进另一层内部：

```text
+-------------------------------------------------------------+
| UI Layer (widgets + callbacks)                              |
|                                                             |
|  entry .entHost / entry .entPort / checkbutton .chkEnable   |
|  button .btnSave  -command on_save_clicked                  |
|  button .btnReset -command on_reset_clicked                 |
|  label  .lblStatus -textvariable statusText                 |
|                                                             |
|  on_save_clicked / on_reset_clicked / render_result /       |
|  clear_status  <-- 只接线，不包含任何校验规则                   |
+------------------------------+------------------------------+
                               |  dict (ok/kind/field/message)
                               v
+-------------------------------------------------------------+
| Business Logic Layer (submit_config)                        |
|                                                             |
|  Step 1: validate_config  -> user_error  (expected failure) |
|  Step 2: catch { assert_internal_consistency; build dict }  |
|          -> internal_error (unexpected failure)             |
|  Step 3: build cfg dict   -> success                        |
+------------------------------+------------------------------+
                               |  host/port strings
                               v
+------------------------------------------------------------+
| Pure Validation Layer (no Tk dependency at all)            |
|                                                            |
|  validate_host / validate_port / validate_config           |
|  只接受字符串参数、只返回 dict，可以直接用 tclsh 单独测试        |
+------------------------------------------------------------+
```

关键设计点：

- **Pure Validation Layer** 和 **Business Logic Layer** 在源码里都写在 `package require Tk` **之前**，这不是巧合——它们是故意设计成完全不依赖 Tk 的纯 Tcl 代码，这也是本 Layer 能在 headless 环境里被完整自测的原因（见第 5 节）。
- **UI Layer** 只在 `package require Tk` 之后出现，并且只调用 `submit_config` 这一个入口函数，不直接调用任何一个 `validate_*` 函数——校验规则对 UI 层是不可见的。

---

## 4. Complete Example

完整代码同时以独立文件存在：[`network_config_form.tcl`](network_config_form.tcl)，可直接运行：

```bash
# 有图形环境
wish network_config_form.tcl

# headless 环境（只能校验到 package require Tk 这一步，见第 5 节）
tclsh network_config_form.tcl
```

完整内容如下：

```tcl
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
```

---

## 5. Execution Walkthrough

### 5.1 headless 语法校验

```bash
tclsh network_config_form.tcl
```

实际执行结果：

```text
no display name and no $DISPLAY environment variable
    while executing
"load /usr/lib64/libtk8.6.so Tk"
    ("package ifneeded Tk 8.6.11" script)
    invoked from within
"package require Tk"
    (file "network_config_form.tcl" line 167)
```

报错发生在第 167 行的 `package require Tk`，而不是任何更早的语法错误——说明从文件开头到这一行之间的所有代码（包括全部校验逻辑、全部业务逻辑）在语法上都是合法的，可以被完整加载。

### 5.2 自测校验逻辑（不需要图形环境）

因为 `validate_host` / `validate_port` / `validate_config` / `submit_config` 全部写在 `package require Tk` **之前**，可以用 `catch` 把 `source` 这个文件时必然发生的 Tk 报错接住，让 `tclsh` 继续往下执行测试代码——这是本 Layer 推荐的最小自测方式：

```bash
tclsh <<'EOF'
catch {source network_config_form.tcl} srcErr
puts "---- source result ----"
puts $srcErr
puts ""
puts "---- validate_port self-test ----"
foreach p {abc -1 70000 8080} {
    puts "validate_port \"$p\" -> [dict get [validate_port $p] reason]"
}
EOF
```

实际输出：

```text
---- source result ----
no display name and no $DISPLAY environment variable

---- validate_port self-test ----
validate_port "abc" -> 必须是合法整数（当前输入："abc"）
validate_port "-1" -> 必须在 1-65535 范围内（当前输入：-1）
validate_port "70000" -> 必须在 1-65535 范围内（当前输入：70000）
validate_port "8080" ->
```

逐条解读：

| 输入 | `string is integer -strict` | 范围检查 | 结果 |
|---|---|---|---|
| `"abc"` | 不是整数 | （不会走到这一步） | 格式错误 |
| `"-1"` | 是整数 | `-1 < 1` | 范围错误 |
| `"70000"` | 是整数 | `70000 > 65535` | 范围错误 |
| `"8080"` | 是整数 | `1 <= 8080 <= 65535` | **合法**（`reason` 为空字符串，所以上面打印是空） |

完整校验+业务逻辑的自测（覆盖 `submit_config` 的三条路径：`user_error` / `internal_error` / `success`）：

```bash
tclsh <<'EOF'
catch {source network_config_form.tcl}
puts "空 Host:        [dict get [submit_config {} 8080 0] message]"
puts "非法 Port:      [dict get [submit_config 192.168.1.1 abc 0] message]"
puts "Port 超范围:    [dict get [submit_config 192.168.1.1 70000 1] message]"
puts "全部合法:       [dict get [submit_config 192.168.1.1 8080 1] message]"
puts "CRASH 调试触发: [dict get [submit_config CRASH 8080 1] message]"
EOF
```

实际输出（`stdout` + `stderr` 合并显示）：

```text
空 Host:        Host 不能为空
非法 Port:      Port 必须是合法整数（当前输入："abc"）
Port 超范围:    Port 必须在 1-65535 范围内（当前输入：70000）
提交给业务逻辑的配置：host 192.168.1.1 port 8080 enable 1
全部合法:       配置已保存：host=192.168.1.1, port=8080, enable=1
[INTERNAL ERROR] submit_config: internal invariant violated: debug trigger 'CRASH' hit assert_internal_consistency (host=CRASH port=8080 enable=1)
CRASH 调试触发: 内部错误：操作未完成，详情已记录到终端日志，请联系开发者
```

可以看到：`[INTERNAL ERROR] ...` 这一行实际上单独写到了 `stderr`（用 `tclsh ... 2>/tmp/err.log` 重定向可以验证只有这一行出现在 `err.log` 里），而真正给用户看的 `message` 字段始终是那句不含任何技术细节的"内部错误：操作未完成……"。这正是"用户看到的提示"和"开发者看到的日志"分离的具体体现。

### 5.3 图形环境下的交互流程（需要 `wish` + 显示环境）

1. 启动：`wish network_config_form.tcl`，表单显示，Host/Port 为空，状态标签为空。
2. 什么都不填直接点 **Save** → 状态标签变红，显示 "Host 不能为空"。
3. 填 Host = `10.0.0.1`，Port 留空，点 **Save** → 显示 "Port 不能为空"。
4. 填 Port = `abc`，点 **Save** → 显示 "Port 必须是合法整数（当前输入："abc"）"。
5. 填 Port = `99999`，点 **Save** → 显示 "Port 必须在 1-65535 范围内（当前输入：99999）"。
6. 填 Host = `10.0.0.1`，Port = `8080`，勾选 Enable，点 **Save** → 状态标签变绿，显示 "配置已保存：host=10.0.0.1, port=8080, enable=1"，同时终端 `stdout` 打印出完整的 `dict`。
7. 点 **Reset** → Host/Port 清空，Enable 取消勾选，状态标签清空、恢复黑色。
8. 把 Host 改成 `CRASH`，点 **Save** → 状态标签变橙色，显示通用的"内部错误"提示，同时终端 `stderr` 打印出技术细节（第 6 步讲的那一行）。这一步是专门为教学准备的"程序错误"演示入口。

---

## 6. Data Flow / Control Flow

基础数据流（一次表单提交的理想路径）：

```text
User Input
    |
    v
Tk Variables
    |
    v
Input Validation
    |
    v
Business Logic
    |
    v
Result / Error
    |
    v
UI Feedback
```

展开到本例的具体实现，加上分支（校验失败 / 内部异常 / 成功三条路径）：

```text
User Input (keyboard / mouse)
    |
    v
Tk Variables (hostVar, portVar, enableVar via -textvariable / -variable)
    |
    v
on_save_clicked  (callback: reads Tk variables, calls submit_config)
    |
    v
submit_config
    |
    +--> validate_config (Pure Validation Layer)
    |         |
    |         +-- invalid --> dict{ok 0 kind user_error field F message M}
    |         |
    |         v
    |       valid
    |
    +--> catch { assert_internal_consistency; build cfg dict }
              |
              +-- error  --> puts stderr (technical detail)
              |          --> dict{ok 0 kind internal_error field "" message M2}
              |
              +-- no error --> dict{ok 1 kind success field "" message M3 cfg CFG}
    |
    v
render_result (UI Layer: 唯一的 "业务状态 -> UI 状态" 翻译点)
    |
    v
statusText + .lblStatus -fg  (UI Feedback: 文案 + 颜色)
```

控制流上的关键事实：

- `validate_config` 永远**返回值**，不抛异常——因为"用户填错了"是业务逻辑设计时就该预料到的分支，用正常返回值表达最自然，调用方用 `if` 处理即可。
- `assert_internal_consistency` 永远**抛异常**（`error`），不返回错误码——因为它守护的是"这件事按设计永远不该发生"的内部约束，一旦违反，用异常机制可以保证调用栈立刻中断，不会带着已经损坏的状态继续往下执行。
- `catch` 只包裹"可能抛异常的那一小段"，不包裹整个 `submit_config`——这样校验失败这种正常分支不会被 `catch` 误吞，`catch` 只对真正意外的情况生效。
- `render_result` 是数据流图里 **Result / Error → UI Feedback** 这一步唯一的实现者；UI 层的其它代码（回调、widget 创建）都不直接处理错误细节。

---

## 7. Common Failure

### 7.1 忘记 `-strict`，空值绕过校验

```tcl
# 错误示范：没有 -strict
if {[string is integer $port]} { ... }
```

`string is integer ""` （不带 `-strict`）在 Tcl 里返回 `1`（因为空字符串被认为是"目前合法、还可以继续输入"的中间状态，这是给输入校验场景设计的宽松模式）。如果真的这么写，空 Port 会被误判为"合法整数"，直接跳过空值检查，是本例里最容易踩的坑。本例的修复方式是显式分两层检查（先 `eq ""`，再 `string is integer -strict`），两层都不能少。

### 7.2 用一个大 `catch` 把所有错误都吞掉

```tcl
# 反模式：把用户错误和程序错误混在一起兜底
proc submit_config_bad {host port enable} {
    if {[catch {
        set r [validate_config $host $port]
        if {![dict get $r valid]} { error [dict get $r reason] }
        ...
    } err]} {
        return "出错了：$err"
    }
}
```

这种写法表面上"更简单"，但代价是：

- 用户输错 Port 和程序本身有 bug，在 UI 上看到的是同一种"出错了：xxx"提示，用户和开发者都没法区分严重程度。
- 任何编码错误（比如拼错变量名）产生的 Tcl 内部报错，都会被这个大 `catch` 悄悄接住，包装成一句普通的用户提示——相当于把真正的 bug 伪装成了"你输入错误"，排查成本会直线上升。

本例坚持把 `catch` 收窄到只包 `assert_internal_consistency` + 构造 `cfg` 这一小段，就是为了避免这个反模式。可以用自测验证区别：

```bash
tclsh <<'EOF'
catch {source network_config_form.tcl}
puts [dict get [submit_config {} 8080 0] kind]       ;# user_error
puts [dict get [submit_config CRASH 8080 1] kind]    ;# internal_error
EOF
```

```text
user_error
internal_error
```

两种失败在 `kind` 字段上被明确区分，UI 层（`render_result`）据此选择不同的提示颜色和文案策略，这是本例故意设计出来、可以直接观察到的差异。

### 7.3 UI 回调里直接写校验规则

```tcl
# 错误示范：校验规则散落在回调里，而不是集中在独立 proc 中
button .btnSave -command {
    if {$portVar eq "" || ![string is integer -strict $portVar]} {
        set statusText "Port 不对"
    }
    ...
}
```

这样写有两个明确的坑：第一，`-command` 脚本里混入了业务规则，没法脱离按钮单独测试，只能点按钮才能触发（headless 环境完全没法验证）；第二，错误提示通常会因为"写得匆忙"而变得笼统（"Port 不对"而不是具体原因），这正是本次任务要求里强调"要给出具体字段、具体原因"的反面例子。

### 7.4 把 `enable` 当字符串与数字混用导致比较出错

Tk `checkbutton` 的 `-onvalue 1 -offvalue 0` 产生的其实是字符串 `"1"`/`"0"`，如果业务逻辑里写 `if {$enable == 1}`，在 Tcl 里数值比较会自动把字符串转成数字，这一步通常没问题；但如果写成 `if {$enable eq 1}`（字符串精确相等），某些场景下如果变量被其它代码写成了整数字面量以外的形式（比如 `"01"`），`eq` 比较就可能出现意料之外的结果。本例在 `assert_internal_consistency` 里用 `ne "0" && ne "1"` 的方式做字符串级别的健全性检查，正是为了在这类潜在不一致出现时能"大声失败"而不是悄悄产生错误的业务结果。

---

## 8. Engineering Practice

- **三层分离，单向依赖**：Pure Validation Layer 不知道 Business Logic Layer 的存在；Business Logic Layer 不知道任何 widget 路径；UI Layer 只认识 `submit_config` 这一个入口和它返回的 `dict` 结构。依赖方向永远是 UI → Business Logic → Validation，反过来永远不允许。
- **错误返回值用统一的 `dict` 结构**：本例里 `ok` / `kind` / `field` / `message` / `cfg` 这组字段约定，让 UI 层写 `render_result` 时不需要对每种错误单独写一套解析逻辑，`switch` 一次搞定。真实项目里建议把这类"结果结构约定"写成代码注释或者单独的"契约"文档，团队协作时少踩互相猜字段名的坑。
- **校验函数要能回答"哪个字段、为什么"**：不要只返回 `0`/`1`，而要返回"字段名 + 具体原因"，这是本次任务反复强调的点，也是用户体验和可维护性的分界线——笼统的"输入错误"几乎没有调试价值。
- **用户错误用返回值，程序错误用异常**：这不是风格偏好，而是语义选择。`error`/`catch` 的本质是"中断当前调用栈，强制调用方处理"，适合"这不该发生"的场景；普通返回值适合"这是业务逻辑里本来就该处理的分支"。混用会让调用方要么被迫到处写 `catch`（如果把正常分支也异常化），要么漏判一些真正的 bug（如果把异常也当返回值悄悄吞掉）。
- **给开发者的信息和给用户的信息要分开渠道**：`puts stderr` 记录技术细节（原始异常文本、变量快照），UI 上只展示脱敏后的通用提示。这个习惯在没有专门日志框架的小工具/脚本里尤其重要——`stderr` 基本上是 Tcl 原生环境里最轻量的"日志系统"。
- **调试触发词（如本例的 `"CRASH"`）只应该出现在教学/测试代码里**：生产代码中如果需要类似的故障注入能力，应该通过环境变量、专门的测试入口或配置开关来实现，不能让一个普通表单字段的某个具体取值变成隐藏的"后门"，这是安全和可维护性上的明确反模式，本例在 Exercises 里会让你亲自改掉它。
- **Reset 要同时清空业务输入和 UI 反馈**：很多实现只清空输入框，忘了同时清空状态标签，导致用户点了 Reset 之后屏幕上还残留着上一次操作的错误/成功提示，造成"表单到底是什么状态"的困惑。本例的 `clear_status` 和 `on_reset_clicked` 是分开两个关注点但总是一起调用的典型写法。

---

## 9. Exercises

1. **给 Host 加格式校验**：目前 `validate_host` 只检查"非空"。请扩展它，要求 Host 必须是合法的 IPv4 地址（四段 0-255 的数字，用 `.` 分隔）或者只包含字母、数字、`.`、`-` 的 hostname，并在不满足条件时返回具体原因（例如"第 2 段 300 超出 0-255"而不是笼统的"格式不对"）。提示：可以用 `split $host "."` 把 IPv4 拆成四段分别校验。

2. **新增一个 Timeout 字段**：在表单里加一个 "Timeout (seconds)" 输入框，要求是 1-300 之间的整数。你需要：新写一个 `validate_timeout` proc（遵循本例的三层检查模式：空值 / 格式 / 范围），把它接入 `validate_config` 和 `submit_config`，在 UI 上加对应的 `label`/`entry`，并保证 `Reset` 按钮也能把这个新字段恢复成默认值。完成后用 `tclsh` 自测 `validate_timeout` 对 `""`、`"abc"`、`"0"`、`"301"`、`"30"` 这几个输入的返回结果。

3. **去掉调试触发词，换成更安全的故障注入方式**：本例里 `assert_internal_consistency` 用字符串 `"CRASH"` 作为教学用的调试触发词，这在真实项目里是不该出现的反模式（第 8 节已经指出）。请改造它：改为只有当某个环境变量（例如 `NETWORK_CONFIG_FORCE_INTERNAL_ERROR`，用 `[info exists env(...)]` 判断）被设置时才会触发模拟的内部错误，普通用户无论往 Host 里填什么字符串都不会意外触发这条路径。改完后分别在"设置环境变量"和"不设置环境变量"两种情况下，用 `tclsh` 自测 `submit_config` 的行为差异。

---

## 10. Mental Model Summary

1. 用户输入在成为"业务状态"之前，必须先经过一层完全独立于 widget 的校验函数。
2. 校验函数只做三件事：判空、判格式、判范围——而且顺序通常也应该是这个顺序。
3. 校验结果要带"哪个字段、什么原因"，不能只是一个布尔值。
4. 用户输入错误是"预期内的失败"，应该用返回值（`dict`）表达，不该用异常。
5. 程序内部状态被破坏是"预期外的失败"，应该用 `error` + `catch` 表达，并且要"大声失败"。
6. `catch` 应该尽量只包裹"真正可能意外出错"的那一小段代码，不要用来兜底整段业务逻辑。
7. 给用户看的提示要友好、不技术化；给开发者看的日志（`stderr`）要具体、可排查。
8. "业务状态"（配置数据是否有效、值是什么）可以脱离 widget 单独用 `tclsh` 测试，这正是分层设计是否成功的检验标准。
9. "UI 状态"（文案、颜色）永远应该是业务状态的渲染结果，由一个单一的翻译函数负责，不要让校验规则散落在各个回调里。
10. Reset 不只是清空输入框，还要清空上一次操作留下的所有 UI 反馈。

---

## 11. Knowledge Check

1. 如果把 `validate_port` 里的 `string is integer -strict` 换成不带 `-strict` 的 `string is integer`，具体会在哪个输入上产生错误的校验结果？为什么？

2. 本例中 `submit_config` 内部的 `catch` 只包裹了 `assert_internal_consistency` 和构造 `cfg` 这两步，没有把 `validate_config` 也放进同一个 `catch` 里。如果有人提议"为了保险，把 `validate_config` 也放进 `catch` 里"，这个提议好不好？会带来什么副作用？

3. `render_result` 只读取 `submit_config` 返回的 `dict`，从不读取 `hostVar`/`portVar`/`enableVar`。如果有一天需求变成"校验失败时要把对应的输入框背景改成浅红色"，应该怎么扩展现有的 `dict` 结构和 `render_result`，才能不违反"业务逻辑不知道 widget 存在"的分层原则？
