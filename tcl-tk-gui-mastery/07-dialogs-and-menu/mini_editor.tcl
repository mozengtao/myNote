#!/usr/bin/env wish
#
# mini_editor.tcl —— Layer 7 Complete Example
#
# 一个功能完整、可运行的小型文本编辑器：
#   New / Open / Save / Save As / Exit，带未保存改动提示、
#   Edit 菜单的 Cut/Copy/Paste（对接 text widget 内置虚拟事件），
#   以及文件读写失败时的错误提示。
#
# 运行方式：
#   有图形环境：   wish mini_editor.tcl
#   headless 环境： tclsh mini_editor.tcl   （只能校验到 package require Tk 这一步）
#
# --------------------------------------------------------------------------
# 关于本文件的结构安排（这一点很重要，Engineering Practice 小节会再展开）：
#
#   本文件故意把"不依赖任何 Tk 命令的纯文件 I/O 逻辑"
#   （ReadFileContents / WriteFileContents）放在 `package require Tk`
#   **之前**定义。Tcl 解释器是从上到下逐条执行的：执行到某一行时，
#   这一行之前的所有 proc 定义、namespace 变量赋值都已经真实生效，
#   与"后面的代码是否会报错"完全无关。
#
#   这样做的直接好处：即使在没有图形界面的 headless 环境下，
#   我们也可以用 `catch {source mini_editor.tcl}` 把整个文件 source 进来——
#   source 执行到 `package require Tk` 时会因为连不上显示而报错，
#   这个错误会被外层 catch 挡住，但在它之前已经定义好的
#   ::editor::ReadFileContents / ::editor::WriteFileContents 两个过程
#   会继续留在解释器里，可以被单独调用、单独测试。
#   具体验证过程写在 README.md 的 Execution Walkthrough 小节里。
# --------------------------------------------------------------------------

# namespace ::editor 保存编辑器的全部状态：
#   filePath —— 当前关联的文件路径；空字符串表示"尚未保存过"（Untitled）。
#   modified —— 当前内容相对上次保存/打开是否有改动。
#   Text     —— 主 text widget 的路径（在 BuildWidgets 里被赋值）。
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

# --------------------------------------------------------------------------
# 从这里开始才需要 Tk：加载 Tk 扩展、连接窗口系统、创建主窗口 "."
# headless 环境下，脚本会在这一行报错并中止——这正是我们期望的校验结果。
# --------------------------------------------------------------------------
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

# ::editor::LoadFile --
#   把 path 指向的文件内容加载进 text widget，并更新编辑器状态。
#   这是"GUI 层"过程：它负责调用纯 I/O 过程 ReadFileContents 拿结果，
#   再决定该怎么对待 widget 和用户提示——真正的读文件逻辑它完全不碰。
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

# ::editor::OpenFile --
#   File > Open... 的入口：先确认是否要丢弃当前未保存改动，
#   再弹出"打开文件"对话框，最后交给 LoadFile 做真正的加载。
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

# ::editor::BuildMenu --
#   创建菜单栏：File 菜单（New/Open/Save/Save As/Exit）、
#   Edit 菜单（Cut/Copy/Paste）。
#   每个 -command 都只写"调用哪个过程"，不在这里堆积业务逻辑——
#   具体怎么判断未保存改动、怎么读写文件，都在上面各自的过程内部完成。
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
