# Layer 7：文件对话框、菜单与常用 GUI 功能

> 核心问题：怎么做一个真正能用的小工具？
> 本层把前面六层建立的 Mental Model（widget 即命令、geometry manager、变量绑定、事件驱动、namespace 封装、输入校验）
> 整合进一个你每天都会用到的真实场景：**一个能打开/编辑/保存文件的小型文本编辑器**。

---

## 1. Why

到目前为止，前六层的例子都在"程序内部"打转：widget 之间互相联动，数据来自硬编码的初始值或者用户在 Entry 里敲的字符串。
但几乎所有"有用"的桌面小工具都绕不开一个更基本的需求：**和文件系统打交道**——打开一个已有文件、编辑它、存回去，或者另存一份。

这就带来三个前面没有遇到过的新问题：

1. **怎么让用户"选"一个文件路径？**
   不能指望用户自己在 Entry 里手打完整路径（没人会这么用软件），需要一个原生的文件选择窗口。
2. **选好路径之后，怎么把文件内容读进来、改完之后又写回去？**
   这是纯粹的 I/O 问题，和界面美不美观没有任何关系。
3. **用户编辑到一半，还没保存，就点了关闭按钮或者要打开另一个文件，怎么办？**
   这是一个状态管理问题：程序必须知道"当前内容是否已经和磁盘上的文件一致"，并在恰当的时机提醒用户。

Tk 恰好为第 1 类问题提供了现成的解决方案：`tk_getOpenFile`、`tk_getSaveFile`、`tk_messageBox` 这些命令，
本质上是**调用操作系统原生的文件选择对话框 / 消息框**，而不是 Tcl/Tk 自己画出来的界面。
第 2 类问题完全由 Tcl 核心的文件 I/O 命令（`open`/`read`/`puts`/`close`/`fconfigure`）解决，
这些命令在 `tclsh` 里就能用，和是否运行在 `wish` 下没有任何关系。
第 3 类问题则是本层要重点建立的工程习惯：**用 namespace 状态变量 + 若干单一职责的小过程，把"业务状态判断"和"界面搭建"分开**，
这正是为 Layer 8（综合项目）打的地基。

---

## 2. Mental Model

一句话 Mental Model：

> **菜单和对话框只是"触发点"；它们负责在恰当的时刻把控制权交给某个 namespace 过程，
> 过程内部再决定调用哪个更小的过程——界面层永远只问"调用谁"，不关心"怎么做"。**

把这句话拆开看三个层次：

```text
Layer A: Trigger (menu item / dialog / window-close button)
    -> only decides WHICH procedure to call

Layer B: Orchestration procedure (SaveFile / OpenFile / NewFile / Exit)
    -> decides WHAT ORDER sub-steps happen in
    -> talks to the text widget AND to tk_messageBox

Layer C: Pure logic procedure (ReadFileContents / WriteFileContents)
    -> no widget, no Tk command, just open/read/write/close
    -> can be tested with plain tclsh, no GUI required
```

对应到本例的函数名：

- Layer A：`.menuBar.file` 里每一条菜单项的 `-command`，以及 `wm protocol . WM_DELETE_WINDOW`。
- Layer B：`::editor::OpenFile`、`::editor::SaveFile`、`::editor::SaveFileAs`、`::editor::NewFile`、`::editor::Exit`、`::editor::ConfirmDiscardChanges`。
- Layer C：`::editor::ReadFileContents`、`::editor::WriteFileContents`。

这个三层划分不是刻意制造的"设计模式"，而是直接对应了 Tcl/Tk 本身的分层：
**Tk 提供"宿主平台 GUI 服务"（对话框、消息框、菜单）；Tcl 核心提供"语言级能力"（文件 I/O、字符串、控制流）；
你的 namespace 过程负责把两者按业务逻辑串起来。**

---

## 3. Architecture

### 3.1 参与对象全景图

```text
+-----------------------------------------------------------------+
| Operating System GUI Services                                   |
|                                                                   |
|   Native "Open File" dialog     Native "Save File" dialog        |
|   Native message / alert box                                     |
+---------------------------+---------------------------------------+
                            ^  (Tk calls into the OS, gets back
                            |   a path string or a button name)
                            |
+---------------------------+---------------------------------------+
| Tk GUI Shell (this is what "Tk" wraps for you)                   |
|                                                                   |
|   tk_getOpenFile   tk_getSaveFile   tk_messageBox                |
|   menu / menuentry   text widget   scrollbar                     |
|   bind / wm protocol / event generate <<Cut>>                    |
+---------------------------+---------------------------------------+
                            ^  (your code calls these commands,
                            |   gets back a path string, a text,
                            |   or just triggers a side effect)
                            |
+---------------------------+---------------------------------------+
| ::editor namespace (Layer B: orchestration procedures)           |
|                                                                   |
|   OpenFile / SaveFile / SaveFileAs / NewFile / Exit               |
|   ConfirmDiscardChanges / LoadFile                                 |
|   MarkModified / MarkSaved / UpdateTitle / OnTextModified          |
+---------------------------+---------------------------------------+
                            |  (calls down into pure Tcl logic,
                            |   no widget reference at all)
                            v
+-----------------------------------------------------------------+
| Pure Tcl File I/O (Layer C: no Tk dependency)                    |
|                                                                   |
|   ReadFileContents  ->  open / fconfigure / read / close          |
|   WriteFileContents ->  open / fconfigure / puts / close          |
+-----------------------------------------------------------------+
```

### 3.2 为什么文件对话框属于"Tk 提供的 GUI 服务"

`tk_getOpenFile`、`tk_getSaveFile` 这两个命令，表面上看是"Tcl 命令"（毕竟你是在 Tcl 脚本里调用它们），
但它们的实现本质是：**向操作系统 / 窗口系统请求弹出一个原生的文件选择对话框**（在 Linux 上通常是 Motif 风格或者
GTK 文件选择器，在 Windows 上是 Win32 的 `GetOpenFileName`，在 macOS 上是 Cocoa 的 `NSOpenPanel`），
然后把用户在那个对话框里的选择结果，转换成一个 Tcl 字符串返回给你的脚本。

这意味着：

- 这两个命令是 **Tk 包**提供的（`package require Tk` 之后才存在），不是 Tcl 核心语言的一部分。
- 它们的"界面"（对话框长什么样、怎么浏览文件夹）完全由宿主操作系统决定，Tcl/Tk 自己不画这个界面，
  只负责"发起请求"和"接收结果"——这正是"GUI 服务"而不是"GUI 组件"的含义。
- 同理，`tk_messageBox` 弹出的也是原生消息框，不是用 `label`/`button` 手工拼出来的窗口。

### 3.3 如何从对话框拿到用户的选择

`tk_getOpenFile` / `tk_getSaveFile` 都是**以返回值的形式**把结果交给调用方：

- 用户选中了一个文件并点确认 -> 返回这个文件的**绝对路径字符串**。
- 用户点了取消，或者直接关掉对话框 -> 返回**空字符串 `""`**。

`tk_messageBox` 同理，返回值是用户点击的按钮名（例如 `yes`/`no`/`cancel`/`ok`），而不是通过某个"全局变量"或者回调参数传递。
这意味着调用方必须**显式检查返回值**，尤其是空字符串这种情况——本例的 `OpenFile` 和 `SaveFileAs` 里都能看到这一行检查：

```tcl
if {$path eq ""} {
    return
}
```

如果漏掉这一步，用户点了"取消"之后，程序会把空字符串当成一个合法路径继续执行 `open ""`，
得到一个莫名其妙的错误，而且这个错误和用户的真实操作（取消）完全对应不上，排查起来很容易误导人。

### 3.4 Tcl 文件 I/O 如何与 Tk GUI 协作

`open`/`read`/`puts`/`close`/`fconfigure` 全部是**纯 Tcl 核心命令**，和 Tk 完全无关——
用 `tclsh`（不需要 `package require Tk`）就能完整使用它们，这也是本层后面"校验方式"一节会反复用到的事实。

在本例中，GUI 层（`SaveFile`/`LoadFile` 等）和 I/O 层（`ReadFileContents`/`WriteFileContents`）的分工是：

| 层次 | 负责的事 | 不负责的事 |
|---|---|---|
| GUI 层 | 拿到路径字符串、把文本内容从/向 text widget 搬运、用 `tk_messageBox` 展示结果或错误 | 不关心 `open` 失败的具体原因是权限不足还是磁盘满 |
| I/O 层 | 用 `catch` 包住 `open`/`read`/`puts`/`close`，把成功/失败和具体原因封装成结构化返回值 | 不知道、也不需要知道这个字符串最终会被塞进哪个 widget |

这就是"**Tcl 核心能力 + Tk GUI 外壳**"分层的具体体现：GUI 外壳只做"摆渡"（拿路径、显示结果），
真正干活儿的文件读写用标准 Tcl 文件命令完成，这两件事完全可以独立开发、独立测试。

### 3.5 为什么文件操作要和界面渲染职责分离

如果把 `open`/`read`/`catch` 直接写在创建 widget 的那段代码里（比如混在 `BuildWidgets` 过程里），会有两个明显的坏处：

1. **没法脱离 GUI 单独测试**：想验证"读一个不存在的文件时错误信息对不对"，就必须先启动一整套 Tk 环境，
   在 headless 的 CI/CD 环境里这几乎不可能做到（本教程的校验环境就是如此）。
2. **复用困难**：如果以后要加一个"命令行批量转换文件编码"的小工具，读写逻辑应该能直接拿去用，
   而不需要先剥离一堆 widget 创建代码。

本例把这两类"关心的问题完全不同"的代码拆成了两个独立的过程（`ReadFileContents`/`WriteFileContents`），
它们唯一的输入输出就是路径字符串和内容字符串，不引用任何 widget 变量——这使得它们可以被单独 `source` 进 `tclsh` 测试，
具体过程见下面的 Execution Walkthrough。

### 3.6 如何避免 `-command` 里堆积业务逻辑

对比两种写法：

```tcl
# 反面写法：把判断逻辑、I/O、错误提示全部塞进 -command
$fileMenu add command -label "Save" -command {
    if {$::editor::filePath eq ""} {
        set path [tk_getSaveFile ...]
        if {$path eq ""} { return }
        if {[catch {set fh [open $path w]; puts $fh ...; close $fh}]} {
            tk_messageBox -icon error -message "保存失败"
        }
        set ::editor::filePath $path
    } else {
        # ... 又是一大段几乎一样的逻辑 ...
    }
}
```

```tcl
# 本例的写法：-command 只回答"调用谁"
$fileMenu add command -label "Save" -command ::editor::SaveFile
```

`::editor::SaveFile` 内部再去判断要不要转调 `SaveFileAs`，`SaveFileAs` 内部再去调用 `WriteFileContents`。
每一层只做一件事，层与层之间通过**返回值**（0/1，或者 `{status payload}` 这样的结构化列表）通信，
而不是互相读写一堆分散的全局变量。好处是：任何一个过程都可以单独阅读、单独测试、单独修改，
不需要在脑子里同时装下"菜单怎么创建"和"文件怎么保存"两件事。

---

## 4. Complete Example

完整文件：[`mini_editor.tcl`](mini_editor.tcl)（384 行，可直接用 `wish mini_editor.tcl` 运行）。
下面按功能拆成几块展示，但请记住：**实际文件是一个连贯的整体**，这里只是为了讲解方便做了切分。

### 4.1 状态与纯 I/O 逻辑（不依赖 Tk）

这一段故意写在 `package require Tk` **之前**，原因见本节末尾的说明框。

```tcl
namespace eval ::editor {
    variable filePath ""
    variable modified 0
    variable Text ""
}

# --------------------------------------------------------------------------
# 纯 Tcl 文件 I/O 逻辑：不引用任何 widget、不引用任何 Tk 命令。
# 这两个过程体现"Tcl 核心能力 + Tk GUI 外壳"的分层：
#   它们完全可以脱离 GUI、脱离 wish，单独在 tclsh 里被调用和测试。
# --------------------------------------------------------------------------

# ::editor::ReadFileContents --
#   读取 path 指向的文件的全部内容。
# 返回：
#   一个两元素列表 {status payload}
#     status = ok    -> payload 是文件内容字符串
#     status = error -> payload 是人类可读的错误信息字符串
# 说明：
#   用 catch 包住 open/read/close 三个可能失败的操作（例如文件不存在、
#   没有读权限、路径是目录等），把底层错误转换成结构化返回值，
#   而不是让错误直接抛给调用方或者让程序崩溃。
proc ::editor::ReadFileContents {path} {
    set status [catch {
        set fh [open $path r]
        fconfigure $fh -encoding utf-8
        set content [read $fh]
        close $fh
        set content
    } result]
    if {$status != 0} {
        # catch 返回非 0 表示 body 内发生了错误，
        # result 此时是 Tcl 自动生成的错误信息字符串。
        return [list error $result]
    }
    return [list ok $result]
}

# ::editor::WriteFileContents --
#   把 content 完整写入 path 指向的文件（覆盖原有内容）。
# 返回：
#   同上，{ok {}} 或 {error 错误信息}。
proc ::editor::WriteFileContents {path content} {
    set status [catch {
        set fh [open $path w]
        fconfigure $fh -encoding utf-8
        puts -nonewline $fh $content
        close $fh
    } result]
    if {$status != 0} {
        return [list error $result]
    }
    return [list ok ""]
}
```

> **为什么把这两个过程放在 `package require Tk` 之前？**
> Tcl 解释器从上到下逐条执行：执行到某一行时，之前所有的 `proc` 定义、`namespace` 变量赋值都已经真实生效，
> 和"后面的代码是否会报错"完全无关。于是即使在 headless 环境下，我们也可以用
> `catch {source mini_editor.tcl}` 把整个文件 source 进来——`source` 执行到 `package require Tk` 时
> 会因为连不上显示而报错，这个错误被外层 `catch` 挡住，但在它之前已经定义好的
> `::editor::ReadFileContents` / `::editor::WriteFileContents` 会继续留在解释器里，可以被单独调用测试。
> 具体做法见第 5 节 Execution Walkthrough。

### 4.2 GUI 状态管理：标题、修改标记、未保存确认

```tcl
package require Tk

# ::editor::UpdateTitle --
#   根据 filePath / modified 两个状态变量刷新窗口标题。
#   未保存改动时在标题前加一个 "*"，方便用户一眼看出"有没有存盘"。
proc ::editor::UpdateTitle {} {
    variable filePath
    variable modified

    set name [expr {$filePath eq "" ? "Untitled" : $filePath}]
    set mark [expr {$modified ? "*" : ""}]
    wm title . "Mini Editor - $mark$name"
}

# ::editor::MarkModified --
#   把当前文档标记为"有未保存改动"，并刷新标题。
proc ::editor::MarkModified {} {
    variable modified
    set modified 1
    ::editor::UpdateTitle
}

# ::editor::MarkSaved --
#   把当前文档标记为"已保存/无改动"。
#   除了刷新我们自己的状态变量，还必须显式复位 text widget 内部的
#   "modified" 标志（edit modified 0）——否则下一次用户随便按一个键，
#   text widget 都不会再触发 <<Modified>> 虚拟事件（因为它认为"已经是
#   modified 状态了，不需要再通知一次"）。
proc ::editor::MarkSaved {} {
    variable modified
    set modified 0
    catch {$::editor::Text edit modified 0}
    ::editor::UpdateTitle
}

# ::editor::OnTextModified --
#   绑定在 text widget 的 <<Modified>> 虚拟事件上。
#   这个事件在"用户编辑文本"和"程序调用 delete/insert"两种情况下
#   都会触发，所以 LoadFile / NewFile 在用代码方式改动内容之后，
#   都需要显式调用 MarkSaved 把标志复位，避免被误判为"用户又改了"。
proc ::editor::OnTextModified {} {
    if {[$::editor::Text edit modified]} {
        ::editor::MarkModified
    }
}

# ::editor::ConfirmDiscardChanges --
#   在"即将丢弃当前内容"之前（New / Open 覆盖式加载）调用。
#   如果没有未保存改动，直接放行（返回 1）。
#   如果有未保存改动，弹出三选项对话框：
#     是（yes）    -> 尝试保存；保存成功才放行，保存失败/取消则不放行。
#     否（no）     -> 放弃改动，直接放行。
#     取消（cancel）-> 不放行，什么都不做。
# 返回：
#   1 表示"可以继续执行后面的操作"，0 表示"调用方应该中止操作"。
proc ::editor::ConfirmDiscardChanges {} {
    variable modified
    if {!$modified} {
        return 1
    }

    set answer [tk_messageBox \
        -icon warning \
        -type yesnocancel \
        -title "未保存的改动" \
        -message "当前文档有未保存的改动，是否保存？"]

    switch -- $answer {
        yes {
            # 复用 SaveFile，而不是在这里重新写一遍保存逻辑
            return [::editor::SaveFile]
        }
        no {
            return 1
        }
        cancel {
            return 0
        }
    }
}
```

### 4.3 加载 / 保存 / 另存为（GUI 层调用 I/O 层）

```tcl
proc ::editor::LoadFile {path} {
    lassign [::editor::ReadFileContents $path] status payload
    if {$status eq "error"} {
        tk_messageBox -icon error -type ok -title "打开文件失败" \
            -message "无法读取文件：\n$path\n\n错误信息：\n$payload"
        return 0
    }

    $::editor::Text delete 1.0 end
    $::editor::Text insert 1.0 $payload

    variable filePath
    set filePath $path
    ::editor::MarkSaved
    return 1
}

# ::editor::SaveFile --
#   保存到当前已关联的文件路径；如果还没有关联任何路径（Untitled），
#   等价于调用 SaveFileAs。
proc ::editor::SaveFile {} {
    variable filePath
    if {$filePath eq ""} {
        return [::editor::SaveFileAs]
    }

    set content [$::editor::Text get 1.0 "end-1c"]
    lassign [::editor::WriteFileContents $filePath $content] status payload
    if {$status eq "error"} {
        tk_messageBox -icon error -type ok -title "保存失败" \
            -message "无法写入文件：\n$filePath\n\n错误信息：\n$payload"
        return 0
    }

    ::editor::MarkSaved
    return 1
}

# ::editor::SaveFileAs --
#   弹出"另存为"对话框，用户选好路径后写入当前文本内容，
#   并把这个新路径记为当前关联的文件路径。
proc ::editor::SaveFileAs {} {
    set path [tk_getSaveFile \
        -title "另存为" \
        -defaultextension ".txt" \
        -filetypes {{"Text files" {.txt}} {"All files" {*}}}]

    # tk_getSaveFile 用户点击取消时返回空字符串，必须显式检查这种情况，
    # 否则会把空字符串当成一个合法路径继续往下执行。
    if {$path eq ""} {
        return 0
    }

    set content [$::editor::Text get 1.0 "end-1c"]
    lassign [::editor::WriteFileContents $path $content] status payload
    if {$status eq "error"} {
        tk_messageBox -icon error -type ok -title "保存失败" \
            -message "无法写入文件：\n$path\n\n错误信息：\n$payload"
        return 0
    }

    variable filePath
    set filePath $path
    ::editor::MarkSaved
    return 1
}
```

### 4.4 New / Open / Exit 入口

```tcl
proc ::editor::OpenFile {} {
    if {![::editor::ConfirmDiscardChanges]} {
        return
    }

    set path [tk_getOpenFile \
        -title "打开文件" \
        -filetypes {{"Text files" {.txt}} {"All files" {*}}}]

    # 同样必须检查用户是否取消了对话框。
    if {$path eq ""} {
        return
    }

    ::editor::LoadFile $path
}

# ::editor::NewFile --
#   File > New 的入口：确认丢弃改动之后，清空内容并回到 Untitled 状态。
proc ::editor::NewFile {} {
    if {![::editor::ConfirmDiscardChanges]} {
        return
    }

    $::editor::Text delete 1.0 end
    variable filePath
    set filePath ""
    ::editor::MarkSaved
}

# ::editor::Exit --
#   File > Exit 和窗口关闭按钮共用的入口（见下方 wm protocol 绑定）。
#   确认过未保存改动之后才真正销毁主窗口。
proc ::editor::Exit {} {
    if {[::editor::ConfirmDiscardChanges]} {
        destroy .
    }
}
```

### 4.5 菜单、widget、绑定与程序入口

```tcl
proc ::editor::BuildMenu {} {
    set menuBar [menu .menuBar]
    . configure -menu $menuBar

    set fileMenu [menu $menuBar.file -tearoff 0]
    $menuBar add cascade -label "File" -menu $fileMenu -underline 0

    $fileMenu add command -label "New" -underline 0 \
        -accelerator "Ctrl+N" -command ::editor::NewFile
    $fileMenu add command -label "Open..." -underline 0 \
        -accelerator "Ctrl+O" -command ::editor::OpenFile
    $fileMenu add command -label "Save" -underline 0 \
        -accelerator "Ctrl+S" -command ::editor::SaveFile
    $fileMenu add command -label "Save As..." -underline 5 \
        -accelerator "Ctrl+Shift+S" -command ::editor::SaveFileAs
    $fileMenu add separator
    $fileMenu add command -label "Exit" -underline 1 \
        -command ::editor::Exit

    set editMenu [menu $menuBar.edit -tearoff 0]
    $menuBar add cascade -label "Edit" -menu $editMenu -underline 0

    # Cut/Copy/Paste 不自己实现剪贴板读写，而是直接向 text widget
    # 发送它内置支持的虚拟事件（<<Cut>> / <<Copy>> / <<Paste>>）。
    # text widget 自带的默认绑定会处理"读写系统剪贴板、更新选中内容"
    # 这些底层细节，我们完全不需要关心 selection/clipboard 命令。
    # 注意：Cut 的 -underline 2（字母 "t"）而不是 0（字母 "C"），
    # 是为了避免和 Copy 的 "C" 快捷字母在同一个菜单里撞车。
    $editMenu add command -label "Cut" -underline 2 \
        -accelerator "Ctrl+X" \
        -command {event generate $::editor::Text <<Cut>>}
    $editMenu add command -label "Copy" -underline 0 \
        -accelerator "Ctrl+C" \
        -command {event generate $::editor::Text <<Copy>>}
    $editMenu add command -label "Paste" -underline 0 \
        -accelerator "Ctrl+V" \
        -command {event generate $::editor::Text <<Paste>>}
}

# ::editor::BuildWidgets --
#   创建主体区域：一个 text widget 配合垂直/水平 scrollbar。
proc ::editor::BuildWidgets {} {
    set textFrame [frame .textFrame]
    pack $textFrame -fill both -expand 1

    set text [text $textFrame.text -wrap none -undo 1]
    set yscroll [scrollbar $textFrame.yscroll -orient vertical \
        -command [list $text yview]]
    set xscroll [scrollbar $textFrame.xscroll -orient horizontal \
        -command [list $text xview]]
    $text configure \
        -yscrollcommand [list $yscroll set] \
        -xscrollcommand [list $xscroll set]

    grid $text    -row 0 -column 0 -sticky nsew
    grid $yscroll -row 0 -column 1 -sticky ns
    grid $xscroll -row 1 -column 0 -sticky ew
    grid rowconfigure    $textFrame 0 -weight 1
    grid columnconfigure $textFrame 0 -weight 1

    variable Text
    set Text $text
}

# ::editor::BuildBindings --
#   绑定键盘快捷键、text widget 的 <<Modified>> 事件，
#   以及窗口管理器的关闭按钮（wm protocol）。
proc ::editor::BuildBindings {} {
    bind $::editor::Text <<Modified>> {::editor::OnTextModified}

    bind . <Control-n> {::editor::NewFile}
    bind . <Control-o> {::editor::OpenFile}
    bind . <Control-s> {::editor::SaveFile}
    bind . <Control-S> {::editor::SaveFileAs}

    # wm protocol . WM_DELETE_WINDOW <proc> ——
    # 把"用户点击窗口标题栏上的关闭按钮（X）"这个窗口管理器事件，
    # 接管为执行我们自己的 ::editor::Exit，而不是让窗口系统直接
    # 不由分说地销毁窗口（那样就跳过了"未保存改动"的确认）。
    wm protocol . WM_DELETE_WINDOW {::editor::Exit}
}

# ::editor::Init --
#   程序入口：设置主窗口属性，依次构建菜单、widget、绑定，
#   最后把初始状态标记为"已保存"（刚启动，空文档，没有任何改动）。
proc ::editor::Init {} {
    wm geometry . 760x480
    ::editor::BuildMenu
    ::editor::BuildWidgets
    ::editor::BuildBindings
    ::editor::MarkSaved
}

::editor::Init
```

---

## 5. Execution Walkthrough

### 5.1 校验 1：确认语法在 `package require Tk` 之前全部合法

headless 环境下用 `tclsh` 执行整个文件：

```bash
$ tclsh mini_editor.tcl
no display name and no $DISPLAY environment variable
    while executing
"load /usr/lib64/libtk8.6.so Tk"
    ("package ifneeded Tk 8.6.11" script)
    invoked from within
"package require Tk"
    (file "mini_editor.tcl" line 95)
```

报错行号正好是 `package require Tk` 这一行（第 95 行），说明：

- `namespace eval ::editor { ... }` 的变量声明没问题。
- `::editor::ReadFileContents` / `::editor::WriteFileContents` 两个 `proc` 定义没问题。
- 剩下几百行里涉及 `menu`/`text`/`scrollbar`/`bind`/`wm` 的代码，Tcl 解释器根本还没执行到——
  这正是本教程"headless 校验"方法论的局限：**语法层面已验证，但不能证明窗口外观和交互效果正确**，
  后者必须在有图形界面的机器上用 `wish mini_editor.tcl` 实际点一遍。

### 5.2 校验 2：单独测试纯 I/O 逻辑（不启动任何 widget）

因为 `ReadFileContents`/`WriteFileContents` 写在 `package require Tk` **之前**，可以利用
"source 中途报错，但报错之前的定义已经生效"这个 Tcl 执行语义，在 `tclsh` 里单独测试它们，
完全不需要真实的文件对话框或者 text widget：

```bash
$ cat > /tmp/test_io.tcl << 'EOF'
# 用 catch 包住 source：package require Tk 会报错，但之前已经定义好的
# ::editor::ReadFileContents / ::editor::WriteFileContents 会继续留在解释器里。
catch {source mini_editor.tcl} err
puts "source caught error: $err"

set tmpfile [file join /tmp mini_editor_test_[pid].txt]

# 测试写入（包含中文，验证 -encoding utf-8 配置是否生效）
set writeResult [::editor::WriteFileContents $tmpfile "hello\n中文内容\nline3"]
puts "write result: $writeResult"

# 测试读取：内容应该原样读回
set readResult [::editor::ReadFileContents $tmpfile]
puts "read result: $readResult"

# 测试读取一个不存在的文件：应该走 error 分支，而不是让 tclsh 直接崩溃退出
set badResult [::editor::ReadFileContents /tmp/does_not_exist_xyz_123.txt]
puts "read missing file result: $badResult"

# 测试写入一个不存在的目录：同样应该走 error 分支
set badWrite [::editor::WriteFileContents /no_such_dir_xyz/foo.txt "x"]
puts "write bad path result: $badWrite"

file delete -force $tmpfile
puts "ALL IO TESTS DONE"
EOF
$ tclsh /tmp/test_io.tcl
```

实际运行输出：

```text
source caught error: no display name and no $DISPLAY environment variable
write result: ok {}
read result: ok {hello
中文内容
line3}
read missing file result: error {couldn't open "/tmp/does_not_exist_xyz_123.txt": no such file or directory}
write bad path result: error {couldn't open "/no_such_dir_xyz/foo.txt": no such file or directory}
ALL IO TESTS DONE
```

四个断言全部符合预期：

1. 写入 + 读回的内容完全一致，包括中文字符——证明 `-encoding utf-8` 配置生效。
2. 读一个不存在的文件，返回 `error` 状态和具体的"couldn't open ..."原因，而不是让解释器直接报错退出。
3. 写入一个不存在的目录，同样被 `catch` 挡住，转换成结构化错误信息。
4. 整个过程完全没有涉及任何 widget、任何 `tk_getOpenFile`——证明这两个过程确实是"纯 Tcl 逻辑"，
   和 `wish`/`$DISPLAY` 完全无关，可以放进任何自动化测试流水线里跑。

### 5.3 真实运行时（需要 `wish` + 图形界面）的执行顺序

在有图形界面的机器上执行 `wish mini_editor.tcl`，发生的事情按顺序是：

1. `package require Tk` 成功，连接上窗口系统，创建主窗口 `.`。
2. `::editor::Init` 被调用：依次执行 `BuildMenu`（挂上菜单栏）、`BuildWidgets`（创建 text + 两个 scrollbar 并布局）、
   `BuildBindings`（绑定快捷键、`<<Modified>>`、`wm protocol`），最后 `MarkSaved` 把标题设为 `Mini Editor - Untitled`。
3. 脚本执行到文件末尾，`wish` 自动进入事件循环，窗口开始响应用户操作。
4. 用户在 text widget 里敲字 -> text widget 内部触发 `<<Modified>>` -> `OnTextModified` 发现
   `edit modified` 为真 -> 调用 `MarkModified` -> 标题变成 `Mini Editor - *Untitled`。
5. 用户点菜单 File > Save -> `::editor::SaveFile` 发现 `filePath` 还是空字符串 -> 转调 `SaveFileAs` ->
   弹出原生"另存为"对话框 -> 用户选好路径 -> `WriteFileContents` 真正写盘 -> 成功后 `MarkSaved` ->
   标题变回 `Mini Editor - /some/path/file.txt`（没有 `*`）。
6. 用户再改几个字符，然后点窗口关闭按钮（X）-> 触发 `wm protocol . WM_DELETE_WINDOW` 绑定的 `::editor::Exit` ->
   `ConfirmDiscardChanges` 发现 `modified` 为真 -> 弹出"是否保存"的 `tk_messageBox` -> 用户选"是" ->
   内部复用 `SaveFile` 保存成功 -> `Exit` 继续执行 `destroy .` -> 窗口关闭，`wish` 进程退出。

---

## 6. Data Flow / Control Flow

```text
User action                 Trigger                      Orchestration proc        Pure logic / Tk service
------------                -------                      -------------------        ------------------------
Click File > Open...   ->   menu -command            ->  ::editor::OpenFile
                                                              |
                                                              v
                                                        ConfirmDiscardChanges  ->  tk_messageBox (if modified)
                                                              |
                                                              v
                                                        tk_getOpenFile          ->  OS native dialog
                                                              |  (returns path, or "" if cancelled)
                                                              v
                                                        ::editor::LoadFile
                                                              |
                                                              v
                                                        ReadFileContents        ->  open/read/close (pure Tcl)
                                                              |  {ok payload} or {error msg}
                                   +--------------------------+--------------------------+
                                   |                                                     |
                                   v                                                     v
                         Text delete/insert, MarkSaved                      tk_messageBox -icon error


Keystroke in text widget -> <<Modified>> event -> OnTextModified -> MarkModified -> UpdateTitle -> wm title .


Click window close (X) -> wm protocol WM_DELETE_WINDOW -> ::editor::Exit -> ConfirmDiscardChanges -> destroy .
```

几个关键的"流向"值得特别记住：

- **用户操作永远先经过一个 Trigger（menu / bind / wm protocol），再进入 Orchestration proc**，
  Trigger 本身没有任何业务逻辑，只是"把控制权交出去"。
- **路径字符串和文本内容都是通过返回值 / 参数传递的，不是通过零散的全局变量**：
  `tk_getOpenFile` 返回路径，`$Text get 1.0 end-1c` 返回内容，`ReadFileContents` 返回 `{status payload}`。
- **错误从最底层一路以返回值的形式"冒泡"上来，而不是用 Tcl 的 `error`/异常直接抛给用户看**：
  `WriteFileContents` 捕获 `open`/`puts` 的失败并转换成 `{error msg}`，`SaveFile` 检查这个返回值，
  决定是否弹 `tk_messageBox`——每一层都只处理"自己能理解"的信息，不会把裸的 Tcl 错误堆栈甩给最终用户。

---

## 7. Common Failure

| 现象 | 原因 | 修复方法 |
|---|---|---|
| 点"取消"后程序报 `couldn't open "": ...` 之类的错误 | 没有检查 `tk_getOpenFile`/`tk_getSaveFile` 返回的空字符串，直接把它传给 `open` | 拿到路径后立即 `if {$path eq ""} { return }` |
| 保存成功了，但标题一直显示带 `*`（看起来像没保存） | 用程序方式（`delete`/`insert`）改了 text widget 内容后，忘了调用 `MarkSaved` 去复位 `edit modified` 标志 | 任何"代码导致的内容变化"之后都要显式 `MarkSaved`，不要指望它自动复位 |
| 第一次改动没有触发 `<<Modified>>` | `text` widget 的 `modified` 标志默认就是关闭状态，一旦被设置为"真"过一次，之后不会重复触发，直到被显式复位 | 确认 `Init`/`MarkSaved` 路径里确实执行了 `$Text edit modified 0`，而不是只在创建时设置一次就不管了 |
| 点窗口关闭按钮（X）直接退出，完全没有"是否保存"的提示 | 没有设置 `wm protocol . WM_DELETE_WINDOW`，窗口管理器会用默认行为直接销毁窗口 | 显式 `wm protocol . WM_DELETE_WINDOW {::editor::Exit}`，把关闭动作接管为自己的确认流程 |
| Cut/Copy/Paste 菜单点了没反应 | `event generate` 的目标 widget 路径写错，或者当时文本没有被选中（Cut/Copy 在没有选区时本来就不会有效果） | 确认 `event generate $::editor::Text <<Cut>>` 里的变量确实指向真实存在的 text widget；先选中一段文字再测试 |
| `tk_messageBox -type yesnocancel` 的返回值判断总是走不到预期分支 | 把返回值当成了大写（如 `Yes`）或者带其他格式去比较 | `tk_messageBox` 返回的是小写的 `yes`/`no`/`cancel`/`ok`，`switch` 分支要完全匹配这个大小写 |
| `tclsh mini_editor.tcl` 在 `package require Tk` 之前就报错 | 真的存在语法问题（括号不匹配、变量名拼错等），不是预期中的"卡在 Tk 初始化"错误 | 对照报错里的行号定位，这说明代码本身有 bug，必须先修好才能继续开发 |

---

## 8. Engineering Practice

1. **三层拆分优先于"能跑就行"**：先确定一个过程属于 Trigger / Orchestration / Pure logic 哪一层，
   再决定它应该写在哪、能不能被单独测试。本例里只要是"读写文件"相关的代码，一律进 Pure logic 层。
2. **返回值而不是异常，用来传递"预期内的失败"**：文件打不开、用户取消对话框，都是"正常会发生的情况"，
   用 `{status payload}` 这种结构化返回值表达，比满屏 `catch`/`error` 更容易让调用方记得处理每一种分支。
3. **`-command` 只接受一个过程名，不接受一段脚本**：本例所有菜单项的 `-command` 都是 `::editor::XXX` 这种单一命令引用
   （Cut/Copy/Paste 例外，因为 `event generate` 本身就是单条命令），一旦发现 `-command {...}` 里面出现了 `if`/`catch`，
   就是该拆成新过程的信号。
4. **状态变量集中放在一个 namespace 里，而不是散落的全局变量**：`::editor::filePath`/`::editor::modified`/`::editor::Text`
   三个变量名字一看就知道属于"编辑器"这个概念，不会和其他模块的变量意外重名。
5. **任何"程序代码导致的 widget 内容变化"之后，都要主动同步状态标志**：`LoadFile`/`NewFile` 修改 text widget 后立刻
   调用 `MarkSaved`，这是容易被忽略但非常常见的 bug 来源（"看起来保存了，其实状态没对上"）。
6. **文件路径一律用变量存起来，不要在多处重复硬编码**：`::editor::filePath` 是全局唯一的"当前文件"事实来源（single source of truth），
   `SaveFile` 判断要不要转 `SaveFileAs`，看的就是这一个变量。
7. **错误提示要包含具体原因，而不是一句"失败了"**：本例的 `tk_messageBox -icon error` 消息里都拼上了 `$payload`
   （Tcl 自动生成的错误信息，例如 "permission denied"），这对用户和未来排查问题的你自己都更有用。

---

## 9. Exercises

1. **【基础】增加"另存为"后自动刷新最近打开列表**：
   在 `::editor::filePath` 之外，增加一个 `::editor::recentFiles` 列表变量，每次 `LoadFile`/`SaveFileAs` 成功后，
   把路径加入这个列表（去重，保留最近 5 个），并在 File 菜单里增加一个 "Recent Files" 子菜单动态显示它们。
2. **【进阶】给 Save 增加"写入前备份"逻辑**：
   在 `WriteFileContents` 真正覆盖写入之前，如果目标文件已存在，先把原文件复制一份为 `原名.bak`
   （提示：用纯 Tcl 的 `file copy -force`，这同样是不依赖 Tk 的逻辑，可以继续放在 Pure logic 层，并单独用 `tclsh` 测试）。
3. **【挑战】把"未保存改动确认"抽象成可复用的组件**：
   把 `ConfirmDiscardChanges` 的逻辑改造成一个更通用的过程 `::editor::ConfirmLossyAction {message}`，
   让 New/Open/Exit 三处调用都能传入各自的提示文案（比如"打开新文件会丢弃当前改动"和"退出前有改动未保存"用不同措辞），
   同时想清楚：这种"参数化提示文案"的改动，是否也应该相应调整 `switch` 里对 `yes` 分支的处理？

---

## 10. Mental Model Summary

1. `tk_getOpenFile`/`tk_getSaveFile`/`tk_messageBox` 是 Tk 对**操作系统原生 GUI 服务**的封装，不是 Tcl 语言本身的能力。
2. 这些对话框命令**以返回值**给出结果：路径字符串，或者用户取消时的空字符串；必须显式检查空字符串。
3. `open`/`read`/`puts`/`close`/`fconfigure` 是**纯 Tcl 核心命令**，和 `wish`/Tk 完全无关，`tclsh` 下就能用、能测。
4. GUI 层只负责"拿路径、展示结果"，真正的读写交给纯 Tcl 过程——这是"Tcl 核心能力 + Tk GUI 外壳"分层的具体实践。
5. 文件操作和界面渲染分离，是为了能脱离 GUI 单独测试、单独复用这部分逻辑。
6. `-command` 只应该回答"调用谁"，业务逻辑应该下沉到 namespace 过程内部，再逐级拆分成更小的子过程。
7. `text` widget 自带 `<<Cut>>`/`<<Copy>>`/`<<Paste>>` 虚拟事件，不需要手写剪贴板读写逻辑。
8. `text` widget 的 `edit modified` 标志需要在每次"程序代码主动改动内容"之后显式复位，否则后续事件不会再触发。
9. `wm protocol . WM_DELETE_WINDOW` 是接管窗口关闭按钮的正规方式，用来插入"未保存改动"确认逻辑。
10. 一切可能失败的文件操作都应该用 `catch` 包住，并把错误转换成用户能看懂的 `tk_messageBox` 提示，而不是让程序崩溃。

---

## 11. Knowledge Check

1. `tk_getOpenFile` 和 `tk_getSaveFile` 返回的到底是什么？如果用户点击了对话框里的"取消"按钮，返回值会是什么，调用方应该怎么处理？
2. 为什么本例把 `ReadFileContents`/`WriteFileContents` 两个过程特意放在 `package require Tk` 这一行**之前**定义？这个顺序如何帮助我们在 headless 环境下单独测试这两个过程？
3. `MarkSaved` 过程里为什么要调用 `$::editor::Text edit modified 0`？如果去掉这一行，会出现什么具体的异常现象？
