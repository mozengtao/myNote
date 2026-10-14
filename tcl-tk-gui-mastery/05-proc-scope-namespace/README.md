# Layer 5：Tcl 过程、作用域与组件封装

> 核心问题：**如何把一个 GUI 程序，从一堆零散的顶层命令，组织成可维护、可复用的模块？**

---

## 1. Why

回想 Layer 0 ~ Layer 4 里写过的所有示例：`hello.tcl`、`pack_example.tcl`，所有变量都是顶层 `set` 出来的全局变量，所有逻辑都直接写在文件的最外层。这在几十行的小程序里完全没问题，但当程序变大之后，三个具体的工程问题会先后浮现：

1. **命名污染**：如果一个程序里有十个窗口、二十个 `.tcl` 文件，每个文件都往全局命名空间里塞 `name`、`port`、`status` 这种常见名字，迟早会有两个文件用了同一个变量名，互相覆盖，而且这种 bug 往往要等到运行时才会表现出来，很难在"读代码"阶段发现。
2. **无法复用成"组件"**：业务上经常需要同一种 UI 元素出现多次——比如一个配置管理工具里，可能要同时显示三台设备各自的配置面板。如果状态只能用全局变量或者"单例"命名空间来存，这种"同一份代码、多份独立状态"的需求就没法优雅地满足。
3. **行为和状态散落在一起，没有清晰的生命周期**：控件什么时候该被创建、什么时候该被清理，如果没有一套固定的组织方式，代码会越来越难以追踪"这个变量现在到底代表哪个窗口的状态"。

本层要讲的 `proc`（过程）、作用域规则（局部变量 / `global` / `upvar`）、`namespace eval`（命名空间）、以及"用 widget 路径当实例 ID"的组件封装技巧，就是 Tcl 社区在不引入 class/object 语法的前提下，解决上面三个问题的标准方案。

本层用**同一个"配置窗口"场景**（Name / Port 两个输入框 + 一个 Save 按钮），写了三个层层递进的文件，让你能直接对比它们的差异：

| 文件 | 组织方式 | 解决了什么 | 还留下什么问题 |
|---|---|---|---|
| `global_style.tcl` | 顶层全局变量 + 普通 `proc` | 能跑起来 | 命名污染、无法共存多实例 |
| `namespace_style.tcl` | `namespace eval` + `variable` | 命名污染 | 命名空间本身是单例，仍然无法共存多实例 |
| `frame_component.tcl` | `namespace` + "widget 路径当数组 key" | 命名污染 + 多实例互不干扰 | （本层范围内已完整解决） |

---

## 2. Mental Model

一句话 Mental Model：

> **作用域（scope）决定一个变量名在哪里"可见"；命名空间（namespace）决定一组相关的名字放进哪个"文件夹"；而 widget 路径本身就是一个天然唯一、不需要额外设计的"实例 ID"——组件封装 = 共享的行为（`proc`）+ 按实例分开存放的状态（以 widget 路径为 key 的 namespace 数组）+ 复用 widget 路径当实例标识。**

### 2.1 作用域的嵌套关系

```text
+-------------------------------------------------------------+
| Global scope  ::                                            |
|   configName, configPort        (global_style.tcl)          |
|                                                             |
|   +------------------------------------------------------+  |
|   | Namespace scope  ::configwin                         |  |
|   |   variable name, variable port                       |  |
|   |   (namespace_style.tcl)                              |  |
|   +------------------------------------------------------+  |
|                                                             |
|   +-------------------------------------------------------+ |
|   | proc call frame (每次调用都是全新的一份)                 | |
|   |   local variables   : 不写 global/variable, proc 返回即消失 | |
|   |   global x           -> link to ::x                   | |
|   |   variable y         -> link to current namespace's y | |
|   |   upvar N v localV   -> link to caller frame N's v    | |
|   +------------------------------------------------------+  |
+-------------------------------------------------------------+
```

### 2.2 组件实例隔离的关系

```text
namespace ConfigWidget
  variable name   : array  name(.panelA)="device-A"  name(.panelB)="device-B"
  variable port   : array  port(.panelA)=8001         port(.panelB)=8002

  proc create / getData / setData / onSave / destroy
      -- one shared implementation, used by every instance --

        .panelA  ----  state slice: name(.panelA), port(.panelA)
        .panelB  ----  state slice: name(.panelB), port(.panelB)

  widget path (".panelA" / ".panelB") = instance identity
```

---

## 3. Architecture

本层涉及的参与者，以及它们之间的关系：

```text
+-----------------+        creates frame         +------------------+
| proc            |----------------------------->| widget (frame/   |
| (shared code,   |        reads/writes          | entry/button)    |
|  one per task)  |<-----------------------------| widget path = id |
+-----------------+     namespace array(w)       +------------------+
        ^
        | variable / global / upvar
        v
+-----------------+                              +------------------+
| namespace scope |<---- namespace export/import-| other namespace  |
| (::configwin,   |                              | (::caller)       |
|  ConfigWidget)  |                              +------------------+
+-----------------+
        ^
        | (fallback resolution when
        |  not found in current ns)
        v
+-----------------+
| global scope :: |
+-----------------+
```

关键概念逐一说明：

- **`proc`**：每次调用都会建立一个全新的"调用帧"（call frame），帧内的变量默认是局部的，`proc` 返回（或出错）时整个帧连同里面的局部变量一起被销毁。这是 Tcl 作用域规则的基础。
- **局部变量 vs 全局变量**：在 `proc` 内部，哪怕外面已经 `set` 过同名变量，不加任何声明直接读写的都是一个全新的、和外部变量毫无关系的局部变量。这是初学者最容易踩的坑之一。
- **`global`**：`global x` 的作用不是赋值，而是"把当前局部作用域里 `x` 这个名字，重新链接到全局命名空间 `::` 里的同名变量"。链接建立之后，这个 `proc` 内对 `x` 的读写都会直接作用在全局变量上。严格地说，`global x` 等价于 `upvar #0 x x`（`#0` 表示"绝对帧号 0"，也就是全局帧）。
- **`namespace eval`**：创建（或进入）一个具名命名空间，这个命名空间有自己独立的变量存储和命令表，和 `::` 全局命名空间、和其它命名空间都互不干扰。
- **`variable`（namespace 内的）**：在命名空间内部用来声明"命名空间持久变量"；在属于该命名空间的 `proc` 内部使用时，作用和 `global` 类似，但链接的目标是"当前 proc 所属命名空间"的同名变量，而不是 `::` 全局变量。
- **`upvar`**：是 `global`/命名空间版 `variable` 的通用化版本——它可以把当前局部作用域里的一个名字，链接到**调用栈中任意指定层级**（甚至任意指定命名空间）的变量上，而不仅限于"全局"或"当前命名空间"。常见用途是写出"传引用"风格的辅助函数（例如交换两个变量的值）。
- **`namespace export` / `namespace import`**：`namespace export` 只是给命名空间"登记"一份公共接口清单；真正要让调用方用短名字访问，还需要调用方显式执行 `namespace import`。这是 Tcl 命名空间可见性控制的核心机制。
- **`destroy` 与 Widget 生命周期**：每个 widget 都要经历"创建 → 配置 → 使用 → 销毁"四个阶段。调用 `destroy $w` 之后，`$w` 对应的 Tcl 命令会**立即**从解释器里消失——这之后任何代码再调用 `$w` 的任何子命令（例如 `$w configure ...`），都会得到 `invalid command name "$w"` 这样的运行时错误。这是本层"Common Failure"一节要重点展开的内容。

---

## 4. Complete Example

本层包含三个渐进式示例文件，都在同一目录下，可以独立用 `tclsh`/`wish` 运行，也在下面逐段内联展示 + 讲解。

### 4.1 `global_style.tcl` —— 全局变量 + 普通 proc

这是三个文件里最朴素的写法：状态是顶层 `set` 出来的全局变量，行为是几个普通 `proc`，`proc` 内部用 `global` 显式声明要操作哪些全局变量。

状态定义和"读/写全局变量"的核心逻辑：

```28:65:/home/morrism/myNote/tcl-tk-gui-mastery/05-proc-scope-namespace/global_style.tcl
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
```

值得注意的工程设计：`package require Tk` 没有像 Layer 0/2 那样写在文件最前面，而是放进了下面 `BuildConfigWindow` 这个 proc 内部——这样状态管理逻辑（`SaveConfig`/`LoadDefaults`）和界面构建逻辑（`BuildConfigWindow`）就被物理地分开了，前者完全不需要 Tk 就能跑和测试：

```89:112:/home/morrism/myNote/tcl-tk-gui-mastery/05-proc-scope-namespace/global_style.tcl
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
```

这种风格最大的问题：如果要同时打开两个独立的配置窗口，`global` 绑定的是写死在代码里的固定名字，没法参数化成"绑定到调用者指定的那组变量"，唯一出路是手写 `configName1`/`configName2`、`SaveConfig1`/`SaveConfig2` 两套平行代码——这正是下一个文件要解决的问题。

### 4.2 `namespace_style.tcl` —— namespace eval 组织模块

把状态和行为都搬进一个具名命名空间 `::configwin`，不再直接污染 `::` 全局命名空间：

```28:65:/home/morrism/myNote/tcl-tk-gui-mastery/05-proc-scope-namespace/namespace_style.tcl
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
```

`namespace export` 声明的接口，配合调用方的 `namespace import`，可以让外部用短名字访问这个模块的公共命令：

```101:111:/home/morrism/myNote/tcl-tk-gui-mastery/05-proc-scope-namespace/namespace_style.tcl
namespace eval ::caller {
    namespace import ::configwin::Save
    namespace import ::configwin::Load
}
puts "=== namespace import demo ==="
puts "calling imported Save() from ::caller: [::caller::Save]"
```

命名污染问题解决了，但 `::configwin::name` / `::configwin::port` 在整个进程里**只有一份存储**——这是标量变量的本质限制，命名空间本身是"单例"的。如果要同时打开两个独立的配置窗口，唯一办法是整段复制代码变成 `::configwin2`，这和 `global_style.tcl` 复制 `configName1`/`configName2` 是同一个问题换了层包装。`frame_component.tcl` 会彻底解决这一点。

### 4.3 `frame_component.tcl` —— frame 封装可复用组件，证明两个实例互不干扰

核心技巧：把标量变量换成**数组**，并且用 widget 路径本身（例如 `.panelA` / `.panelB`）当数组下标。widget 路径在 Tk 里天然唯一，是一个现成的、不需要额外设计的"实例 ID"。这样同一组 `proc`（行为）就能通过"传入哪个 widget 路径"来区分到底在操作哪个实例的状态。

组件的状态定义，以及"初始化状态"（`InitState`）、"获取数据"（`getData`）、"更新数据"（`setData`）、"注册保存回调"（`onSave`）四个纯逻辑 proc——它们都只操作 namespace 数组，完全不调用任何 Tk 命令：

```45:116:/home/morrism/myNote/tcl-tk-gui-mastery/05-proc-scope-namespace/frame_component.tcl
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
```

真正创建控件的 `create`，以及销毁实例的 `destroy`（注意里面 `::destroy` 的用法，这是本层一个重要的"命令名遮蔽"陷阱，会在第 7 节详细展开）：

```129:181:/home/morrism/myNote/tcl-tk-gui-mastery/05-proc-scope-namespace/frame_component.tcl
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
```

脚本末尾分两段：第一段用两个"假装是 widget 路径"的字符串（`.panel1`/`.panel2`）当 key，**完全不依赖 Tk**地验证两个实例互不干扰；第二段才真正调用 `create` 创建两个真实的 `.panelA`/`.panelB` 控件实例：

```183:245:/home/morrism/myNote/tcl-tk-gui-mastery/05-proc-scope-namespace/frame_component.tcl
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
```

> **关于"无 Tk 环境验证"的说明**：本文件把"纯逻辑验证"（用假的路径字符串当 key，不创建任何真实 widget）和"真实创建 widget"（调用 `create`，内部才第一次 `package require Tk`）明确拆成了两段。在没有 `$DISPLAY` 的 headless 环境下用 `tclsh frame_component.tcl` 运行，第一段会完整跑完并打印全部验证结果（见第 5 节的实际输出），第二段会在 `package require Tk` 处因为连不上显示而报错退出——这正好说明：组件的"状态管理"这一半逻辑，从设计上就与 Tk 完全解耦，可以独立测试；而"把状态渲染成真实界面"这一半，必须有图形环境才能完整验证。本教程没有条件做到"完全不依赖 Tk 就能验证出一个真实 frame 控件"，这是本示例在 headless 环境下的明确限制。

---

## 5. Execution Walkthrough

以 `frame_component.tcl` 为例，按 `tclsh frame_component.tcl` 的实际执行顺序逐条解释（其它两个文件的执行顺序结构完全类似，只是没有"多实例"这一段）：

1. `namespace eval ConfigWidget { ... }` 执行：创建 `ConfigWidget` 命名空间，登记 `name`/`port`/`saveCb` 三个命名空间数组（此时都是空数组），登记导出列表。此时还没有创建任何 proc 的调用，没有任何 widget。
2. 依次定义 `InitState`/`getData`/`setData`/`onSave`/`InvokeSave`/`create`/`destroy` 七个 proc——`proc` 这条命令只是把过程体注册进命令表，不会执行过程体内部的代码。
3. 执行 `ConfigWidget::InitState .panel1 "device-A" 8001`：这是第一次真正调用过程，Tcl 建立一个新的局部调用帧，`variable name`/`variable port`/`variable saveCb` 把局部名字链接到 `ConfigWidget` 命名空间的同名数组，然后往 `name(.panel1)`、`port(.panel1)`、`saveCb(.panel1)` 三个数组元素里写入值。调用帧随后销毁，但数组元素的值留在命名空间里。
4. 对 `.panel2` 重复第 3 步，这次写入的是 `name(.panel2)` 等——因为下标不同，和 `.panel1` 的数据完全不冲突。
5. 调用 `ConfigWidget::getData .panel1` / `.panel2`，分别用 `dict create` 把对应下标的数组元素打包成 dict 返回，`puts` 打印出来——此时能直接看到两份数据完全独立。
6. 调用 `ConfigWidget::setData .panel1 {...}` 只修改 `name(.panel1)`/`port(.panel1)`，紧接着再次 `getData .panel2` 验证它确实没有被改动——这是对"实例隔离"最直接的证明。
7. `onSave` 把两个 lambda 分别存进 `saveCb(.panel1)`/`saveCb(.panel2)`；`InvokeSave` 读出对应的 lambda 并用 `apply` 执行，传入当前的 `getData` 结果——这一步验证"回调注入"机制本身是可用的，且是实例独立的。
8. `ConfigWidget::destroy .panel1` 执行：先 `unset` 掉三个数组里对应 `.panel1` 的元素，然后检查 `[info commands .panel1]` ——因为 `.panel1` 只是一个字符串，根本不是真实的 Tcl 命令，这个检查返回空，所以**不会**尝试调用 `::destroy`。随后 `getData .panel1` 因为数组元素已经被删除而报错，`catch` 捕获到这个错误并打印出来，证明清理生效；而 `getData .panel2` 依然正常，证明它完全没受到 `.panel1` 被销毁的影响。
9. 执行到 `ConfigWidget::create .panelA`：进入 `create` proc，第一条语句 `package require Tk` 尝试加载 Tk 扩展并连接显示系统。在 headless 环境下，这一步连接失败，Tcl 抛出 "no display name and no \$DISPLAY environment variable" 错误，整个脚本在这里中止，`panelB` 的创建、`pack`、最后两条 `puts` 都不会被执行到。

实测输出（`tclsh frame_component.tcl`，节选）：

```text
=== pure logic self-test (no Tk, fake widget-path keys) ===
panel1 initial: name device-A port 8001
panel2 initial: name device-B port 8002
panel1 after setData: name device-A-renamed port 9001
panel2 after panel1's setData (should be unchanged): name device-B port 8002
self-test PASS: two instances are fully independent
panel1 saved callback got: name device-A-renamed port 9001
panel2 saved callback got: name device-B port 8002
panel1 state cleaned up correctly, getData now errors: can't read "name(.panel1)": no such element in array
panel2 still intact after panel1 destroyed: name device-B port 8002
=== creating two real widget instances (requires display) ===
no display name and no $DISPLAY environment variable
    while executing
"load /usr/lib64/libtk8.6.so Tk"
    ...
```

---

## 6. Data Flow / Control Flow

```text
User types in entry widget
        |
        v
Tk auto-writes new value into
  ::ConfigWidget::name(w) / port(w)      (via -textvariable, Layer 3 机制)
        |
        v
User clicks "Save" button
        |
        v
Tk event loop dispatches click
        |
        v
-command [list ConfigWidget::InvokeSave w]   runs
        |
        v
InvokeSave reads saveCb(w)  ->  calls stored lambda via apply
        |                              |
        v                              v
   getData w  builds dict     external callback runs with that dict
   (reads name(w)/port(w))    (e.g. writes to a file, prints, ...)
```

关键点：

- **输入 → 状态**：entry 控件的 `-textvariable` 直接绑定到 `::ConfigWidget::name($w)` 这样的数组元素，用户打字时 Tk 自动同步，不需要手写任何事件处理代码（这是 Layer 3 讲过的机制，在组件场景下原样适用，只是变量换成了"按实例分 key"的数组元素）。
- **状态 → 行为**：`getData`/`setData` 是状态和外部世界之间唯一的"正式接口"——无论状态内部是用标量变量（`namespace_style.tcl`）还是数组（`frame_component.tcl`）存的，外部代码只需要认准这两个函数签名，不需要关心内部存储细节。
- **行为 → 回调**：`onSave` 注入的回调，和"保存逻辑具体怎么实现"（写文件？发网络请求？只是打印？）完全解耦——`ConfigWidget` 自己不需要知道、也不需要关心保存之后具体发生什么。

---

## 7. Common Failure

### 7.1 忘记 `global`（或 `variable`），局部变量"读起来像全局变量，实际完全无关"

```tcl
set counter 0
proc Increment {} {
    # 忘记写 global counter
    incr counter
}
Increment
puts $counter   ;# 仍然是 0，而不是 1
```

`incr counter` 在 `Increment` 内部第一次执行时，会先在局部作用域里创建一个全新的 `counter`（初值当作 0），再加 1 变成 1，但这个"1"随着 proc 返回就被销毁了，外面的全局 `counter` 完全没被动过。这是最常见的"作用域"相关 bug，报错信息往往不明显（不会报错，只是结果不对），需要特别小心。

### 7.2 命名空间内的命令名遮蔽（namespace command shadowing）

这正是 `frame_component.tcl` 里 `ConfigWidget::destroy` 要特别处理的问题。如果写成：

```tcl
proc ConfigWidget::destroy {w} {
    # ... 清理数组 ...
    destroy $w     ;# 错误！这里会无限递归调用自己
}
```

执行到 `destroy $w` 这一行时，Tcl 对未加前缀命令名的查找顺序是"先查当前命名空间，再查全局命名空间"。当前命名空间就是 `ConfigWidget`，而这个命名空间里恰好有一个叫 `destroy` 的命令——就是正在执行的这个 proc 自己，于是变成无限递归，最终以 `too many nested evaluations (infinite loop?)` 这样的错误崩溃。正确写法是显式用 `::destroy $w` 强制从全局命名空间开始查找。

**一般规律**：只要你在某个命名空间里定义了一个和 Tcl/Tk 内置命令同名的 proc（`destroy`、`update`、`after` 都是常见的"中雷"名字），这个命名空间内部所有对该名字的"裸调用"都会被你自己的 proc 遮蔽，必须显式加 `::` 前缀才能调用到原本的内置命令。

### 7.3 widget 被销毁后，继续调用它的命令

```tcl
destroy .panelA
.panelA configure -borderwidth 4
```

第二行会报错：`invalid command name ".panelA"`。这是因为 `destroy` 执行后，Tk 不仅销毁了屏幕上的控件，还**立即**把 `.panelA` 这个 Tcl 命令从解释器的命令表里移除了——widget 的"生命周期"和它对应的 Tcl 命令的"存在时间"是完全同步的：创建时命令诞生，销毁时命令立即消失，中间没有任何"已销毁但命令还能用"的过渡状态。如果你的代码里保存了 widget 路径的字符串（比如存进某个数组当 key），一定要在销毁 widget 的同时也清理掉这些残留的引用，否则后续代码一旦不小心调用到它们就会报错——这正是 `ConfigWidget::destroy` 里"先清理数组、再销毁 widget"这个顺序的意义所在。

### 7.4 `-textvariable` 写成裸变量名，忘记完整命名空间路径

```tcl
entry .ename -textvariable name
```

如果这行代码出现在某个命名空间的 proc 里，而你期望它绑定到 `::ConfigWidget::name($w)`，但只写了裸的 `name`，Tk 会去**全局命名空间**找一个叫 `name` 的变量（而不是你的命名空间变量），导致输入框完全不会同步到你期望的那份状态。正确写法必须给出完整路径，例如 `::ConfigWidget::name($w)`。

---

## 8. Engineering Practice

- **把"状态管理"和"界面构建"物理分开**：本层三个文件都特意把 `package require Tk` 推迟到真正需要创建控件的那一刻（`BuildConfigWindow`/`Build`/`create` 内部），而不是写在文件最前面。好处不仅是方便在 headless 环境里做单元测试，更重要的是这是一种良好的架构习惯——业务逻辑（状态怎么存、怎么存取、要不要校验）不应该和"用什么工具渲染界面"绑死在一起，哪天要把界面换成别的工具库，状态管理那部分代码完全不需要改。
- **公共接口只认函数签名，不暴露内部存储细节**：外部代码调用 `getData`/`setData`/`onSave` 时，不需要知道、也不应该关心内部到底是用三个并列数组还是一个 dict 数组存的——这样未来想优化内部存储结构时，只要这几个函数的输入输出约定不变，调用方代码完全不用动。
- **选用"widget 路径当 key"而不是额外发明一套 ID 系统**：很多人学 Tcl/Tk 之后，第一反应是想引入一个全局自增计数器来生成"实例 ID"。但 Tk 的 widget 路径本身已经是解释器层面强制保证唯一的字符串，直接复用它可以省掉一整套 ID 分配/回收的代码，也天然避免了"ID 和 widget 对不上"的 bug。
- **销毁要成对、要按正确顺序清理**：每一个 `create` 对应的 `destroy`，都要记得把"创建时登记进 namespace 数组的状态"清理掉，而不仅仅是销毁 widget 本身——否则随着程序运行时间变长，这些数组会无限增长，变成一种隐蔽的内存泄漏（Tcl 术语里更准确的说法是"命名空间变量泄漏"）。
- **多文件项目里，坚持用 `namespace` 组织每一个模块**，哪怕这个模块暂时只会被实例化一次（比如一个"应用程序主控制器"）。这样做的好处不是立刻用上"多实例"，而是从第一行代码开始就保证了"命名不会和别的文件冲突"，为未来的扩展（例如这个模块真的需要支持多实例了）留好余地。

---

## 9. Exercises

1. **基础**：仿照 `global_style.tcl`，把 Save 按钮改成同时还有一个 Reset 按钮（点击后把 Name/Port 恢复成默认值）。要求 Reset 对应的 proc 内部也要正确使用 `global`。
2. **进阶**：仿照 `namespace_style.tcl`，给 `::configwin` 命名空间增加一个 `Validate` proc：检查 `port` 是否是 1~65535 之间的整数，不合法时返回错误信息而不是直接崩溃（可以用 `catch` 包裹 `Save`调用它）。再用 `namespace export`/`namespace import` 把这个新接口暴露出去，并在文件末尾写一段自测代码验证合法/非法两种输入。
3. **挑战**：扩展 `frame_component.tcl` 里的 `ConfigWidget`，增加一个 `ConfigWidget::count {}` proc，返回当前存活的实例个数（提示：可以遍历 `name` 数组的下标，用 `array names name` 实现，不需要额外的计数器变量）。再写一段自测代码：创建三个假的实例 key，调用 `count` 验证返回 3；`destroy` 掉一个之后，再调用 `count` 验证返回 2。

---

## 10. Mental Model Summary

1. `proc` 每次调用都会建立一个全新的局部作用域，返回后局部变量全部消失。
2. 不加 `global`/`variable`/`upvar` 声明时，`proc` 内部对某个名字的读写永远是局部的，和同名的外部变量无关。
3. `global x` 本质是把局部名字链接到全局命名空间的同名变量（等价于 `upvar #0 x x`）。
4. `namespace` 内的 `variable` 是把局部名字链接到"当前所属命名空间"的同名持久变量。
5. `upvar` 是 `global`/`variable` 的通用化：可以链接到调用栈任意层级的变量，常用于"传引用"风格的辅助函数。
6. `namespace eval` 解决命名污染，但命名空间本身的变量仍然是"单例"的，不能自动支持多实例。
7. 用 widget 路径当 namespace 数组的下标，是 Tcl 里"实例化"的标准手法：行为（proc）共享一份代码，状态按 key 分开存。
8. Tcl 不需要 class/object 语法：`proc`（行为）+ `namespace` 数组（状态）+ widget 路径（实例标识）已经完整表达了对象的三要素。
9. 命名空间内部定义和内置命令同名的 proc（如 `destroy`），裸调用会被自己遮蔽，必须显式加 `::` 前缀。
10. widget 被 `destroy` 后，对应的 Tcl 命令立即从解释器消失，残留的路径引用必须同步清理。

---

## 11. Knowledge Check

1. 如果一个 `proc` 内部既没有写 `global`，也没有写 `variable`，直接 `set x 1`，这个 `x` 到底存在哪里？外部代码（包括其它 `proc`）能不能看到它？
2. 为什么 `ConfigWidget::destroy` 内部必须写 `::destroy $w` 而不能直接写 `destroy $w`？如果去掉前面的 `::`，实际运行会发生什么？
3. `namespace_style.tcl` 已经用 `namespace eval` 解决了命名污染问题，但为什么它依然不能像 `frame_component.tcl` 一样支持"同时存在两个互不干扰的配置窗口"？根本原因出在哪一步？

---

## 扩展：如果确实需要更传统的 OO，可以用 TclOO

本层主要内容到这里就结束了：`proc` + `namespace` + "widget 路径当实例 ID" 这套组合，已经足够应付绝大多数 Tcl/Tk GUI 项目里的组件封装需求，不需要引入任何类/对象语法。

但当一个项目里组件的种类变多（比如要做出一整套互相继承的 Widget 库：`BaseWidget` → `ConfigWidget` → `AdvancedConfigWidget`），并且需要"多态分派"（同一个方法名，不同子类有不同实现）时，手写的 namespace + 数组模式会变得越来越笨重——继承关系要自己拿 `proc` 名字拼接或者手写委托，没有语言层面的支持。这种场景下，Tcl 8.5+ 自带的 **TclOO** 框架（Tcl 8.6 默认内建，不需要额外安装）提供了更接近传统面向对象语言的语法：`oo::class create` 定义类，`method` 定义方法，`constructor`/`destructor` 管理生命周期，并支持继承（`superclass`）。

下面是用 TclOO 重写的一个精简版配置组件（已用 `tclsh` 验证，语法和运行结果均正确）：

```tcl
package require TclOO

oo::class create ConfigWidgetOO {
    variable Name
    variable Port
    variable SaveCallback

    constructor {{name "device-01"} {port 8080}} {
        set Name $name
        set Port $port
        set SaveCallback {}
    }

    method getData {} {
        return [dict create name $Name port $Port]
    }

    method setData {data} {
        if {[dict exists $data name]} { set Name [dict get $data name] }
        if {[dict exists $data port]} { set Port [dict get $data port] }
    }

    method onSave {callback} {
        set SaveCallback $callback
    }

    method save {} {
        if {$SaveCallback ne {}} {
            uplevel #0 [list apply $SaveCallback [my getData]]
        }
    }
}

# 创建两个独立实例：每个实例的 Name/Port 都是对象私有的，
# 不需要任何"widget 路径当 key"的手动技巧——TclOO 自动为每个对象
# 分配独立的变量存储空间。
set obj1 [ConfigWidgetOO new "edge-01" 9000]
set obj2 [ConfigWidgetOO new "edge-02" 9001]

$obj1 onSave {data {puts "obj1 saved: $data"}}
$obj2 onSave {data {puts "obj2 saved: $data"}}

$obj1 save
$obj2 save

puts "obj1 data: [$obj1 getData]"
puts "obj2 data: [$obj2 getData]"

# TclOO 对象自带 destroy 方法（继承自 oo::object），
# 调用之后这个对象对应的命令立刻失效，和 widget 的生命周期规则一致。
$obj1 destroy
$obj2 destroy
```

实测输出（`tclsh` 运行上述代码）：

```text
obj1 saved: name edge-01 port 9000
obj2 saved: name edge-02 port 9001
obj1 data: name edge-01 port 9000
obj2 data: name edge-02 port 9001
```

可以看到：TclOO 版本的 `getData`/`setData`/`onSave`/`save` 和前面手写的 `ConfigWidget` 命名空间版本在接口上几乎一一对应，区别只是"实例状态存在哪里"——TclOO 用对象自己的变量槽，手写版本用"命名空间数组 + widget 路径当 key"。两种方式殊途同归，具体选哪种，取决于项目的组件数量和继承复杂度：组件种类少、关系简单，`namespace` + 数组足够清晰；组件种类多、继承/多态需求强，TclOO 能省掉大量手写的分派代码。
