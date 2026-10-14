# Layer 0：整体架构与运行环境

> 核心问题：**Tcl、Tk、wish、Widget、Event Loop 五者是什么关系？**

本层不教你写任何"真正的"GUI 逻辑，只建立一个贯穿全教程的 Mental Model。
后面每一层（Widget 系统、Geometry Manager、变量绑定、事件驱动……）都是在
展开这张图里的某一个细节。如果你对这一层理解不牢，后面会一直用错误的直觉
去套 Tcl/Tk 的行为。

---

## 1. Why（这个机制解决什么问题）

几乎所有初学者第一次接触 Tcl/Tk 时都会问出类似的问题：

- 为什么 `button .b -command {...}` 写完之后点进去没反应（在交互式 `tclsh`
  里粘贴这行代码，程序立刻退出了）？
- `tclsh` 和 `wish` 看起来都能跑 `.tcl` 文件，到底用哪个？
- 控件（widget）到底是"数据结构"还是"命令"？为什么 `.message` 这种路径
  可以直接当命令调用？
- 我的窗口代码明明一行行都执行完了，为什么界面还"卡"在那里不退出？

这些问题的根源都是同一件事：**没有建立"Tcl 命令执行"与"GUI 事件处理"
是两套不同机制、通过 Event Loop 协作在一起**的 Mental Model。

Tcl/Tk 的设计哲学是：**GUI 工具包本身不引入新的语言，而是把所有 GUI 操作
都暴露成普通的 Tcl 命令**。这意味着：

- 你不需要学一套新的"GUI 语法"，`button`、`pack`、`bind` 和 `set`、`if`、
  `proc` 是同一个解释器执行的同一类东西——都是 Tcl 命令。
- 但正因为"创建控件"和"控件将来会做什么"都只是命令调用，
  你必须理解**命令什么时候被解释器求值**，才能理解**代码什么时候真正生效**。

这一层要解决的问题就是：在你写出第一行 `button` 命令之前，先把"谁在执行、
谁在等待、谁在转发"这三件事分清楚。

---

## 2. Mental Model（一句话直觉 + 图）

一句话直觉：

> **Tcl 解释器负责"立刻执行你写的命令"；Tk 只是一堆被注册进这个解释器的
> 命令，专门用来操纵 GUI 对象；Event Loop 是一个不断循环的调度员，
> 负责把操作系统转发来的输入事件，转换成对这些 Tk 命令（或你存进
> `-command` 里的脚本）的调用。**

把这句话拆成三层，对应下面这张图：

```text
+--------------------------------------------------------+
| Tcl Application                                        |
|                                                        |
|  Application Logic                                     |
|       |                                                |
|       +---- Widget Commands                            |
|       +---- Tcl Variables                              |
|       +---- Event Bindings / Callbacks                 |
+--------------------------------------------------------+
                            |
                            v
+--------------------------------------------------------+
| Tcl Interpreter + Tk                                   |
|                                                        |
|  Command Evaluation                                    |
|  Widget Management                                     |
|  Variable Traces / Variable Integration                |
|  Event Processing                                      |
+--------------------------------------------------------+
                            |
                            v
+--------------------------------------------------------+
| Window System / Display                                |
|                                                        |
|  Windows, Controls, Rendering, Input                   |
+--------------------------------------------------------+
```

**非常重要的一点：图中的每一层逻辑边界，不代表每个组件都是独立进程。**
实际运行时，`wish` 是**单个操作系统进程**：Tcl 解释器、Tk 工具包、
事件循环全部跑在这一个进程内部；只有"Window System / Display"这一层
（X11 的 X Server、Windows 的桌面窗口管理器、macOS 的 WindowServer）
通常才是独立于 `wish` 之外的系统进程/服务。图中的分层是**职责边界**，
不是**进程边界**。

---

## 3. Architecture（参与对象及其关系）

### 3.1 Tcl 解释器（Tcl Interpreter）

Tcl 是一门"一切皆命令"的语言。解释器执行任何一行代码都遵循同一个三步
机制：

1. **parse（解析）**：把一行文本按 Tcl 的分词规则切成"命令名 + 参数列表"。
2. **substitute（替换）**：处理 `$var`、`[cmd]`、`\n` 等替换语法，
   把参数列表里的占位符换成真实值。
3. **evaluate（求值）**：查找命令名对应的实现（可能是内置命令、`proc`
   定义的过程，也可能是 Tk 注册的控件命令），并调用它。

这三步对**任何**一条 Tcl 命令都成立——`set x 1`、`if {...}`、
`button .b -text Hi` 走的是完全相同的执行路径。Tk 并没有给 Tcl 引入
新的语法规则，它只是往这个解释器里注册了更多"命令名"。

### 3.2 Tk GUI 工具包

Tk 本质上是一个 **C 扩展库**，被加载进 Tcl 解释器后，会注册一批新命令：
`button`、`label`、`entry`、`frame`、`toplevel`、`pack`、`grid`、`place`、
`bind`、`wm`、`menu`、`canvas`……等等。

**Tk 不是一门独立语言，它是通过 Tcl 命令暴露出来的一组 GUI 操作接口。**
这也是为什么本教程反复强调："学 Tk 本质上是在学一批新的 Tcl 命令，
而不是学一套新的编程范式。"

### 3.3 窗口系统与原生窗口资源

再往下一层是操作系统提供的窗口系统：Linux/Unix 上通常是 X11（X Server），
Windows 上是 Win32 GUI 子系统，macOS 上是 Aqua（Quartz）。Tk 内部针对
不同平台实现了不同的底层适配代码，但对外暴露的 Tcl 命令接口是一致的
（这也是 Tk"一次编写，多平台运行"的关键）。

真正的像素渲染、窗口标题栏绘制、鼠标/键盘硬件事件捕获，都发生在这一层。
Tk 控件只是**逻辑对象**；当控件被 pack/grid 等 geometry manager 安排好
位置后，Tk 会要求窗口系统创建/更新对应的**原生窗口资源**来实际显示它。

> 再次强调：这三层是**职责边界**而不是**进程边界**。在本教程涉及的
> X11/桌面环境下，`wish` 进程通过本地 IPC（X11 协议、Win32 消息、
> Aqua 事件）与窗口系统服务进程通信，但 Tcl 解释器、Tk 工具包本身
> 都和你的应用逻辑运行在同一个 `wish` 进程地址空间里。

### 3.4 Widget（控件）

Widget 是 Tk 对"一个 GUI 对象"的统一抽象：按钮、标签、输入框、
容器……都是 widget。每个 widget 在创建时都会：

1. 获得一个唯一的 **widget 路径**（例如 `.message`、`.frame1.btnOk`），
   路径本身就表达了它在控件树中的父子关系（`.` 是根）。
2. 在 Tcl 解释器里**注册一个同名的新命令**。例如创建 `.message` 之后，
   `.message configure -text "..."`、`.message cget -text` 都是合法的
   Tcl 命令调用——这正是 Layer 1 要展开讲的"控件即命令"。

本层你只需要记住：**widget 既是一个 GUI 对象，又是一个 Tcl 命令**，
这两者是同一枚硬币的两面。

### 3.5 Event Loop（事件循环）

Event Loop 是 Tk 应用能"活起来"的核心机制。它的工作循环大致是：

```text
loop forever:
    等待一个事件（可能来自窗口系统，也可能是 after 定时器、文件 I/O 等）
    从事件队列里取出一个事件
    判断这个事件归属哪个 widget / 哪个 binding
    调用对应的 Tcl 脚本去处理它
    （处理过程中可能产生新的命令执行、新的界面状态）
    回到循环开头，继续等待下一个事件
```

关键认知：**Tcl 命令的执行是同步的、立即的**（parse -> substitute ->
evaluate 一步到位，不会"挂起"去等别的事情发生）；但**整个 GUI 程序
"活着"这件事，依赖 Event Loop 不断轮询/阻塞等待**。没有 Event Loop，
窗口创建完之后，程序会立刻继续往下执行、直到脚本结束而进程退出——
根本来不及等用户点一下鼠标。

### 3.6 Callback（回调）

Callback 指的是"暂存一段脚本，等未来某个事件发生时才执行"这种模式。
在 Tk 里最常见的两种写法：

- `-command {script}` 选项：几乎所有可点击控件（button、checkbutton、
  menu 条目……）都支持，脚本在控件被"激活"时执行。
- `bind <widget-or-class> <event-sequence> {script}`：更通用的事件绑定，
  可以绑定鼠标移动、按键、窗口大小变化等任意事件序列。

本层只需要建立直觉："回调脚本在写下来的那一刻不会执行，它只是被存进了
某个控件的内部配置表，真正执行的触发者是 Event Loop。" 第 4/5 节会
逐字说明这件事在 `hello.tcl` 里具体是怎么发生的。

### 3.7 Tcl 变量与 Tk Variable 的关系（先提一下，细节见 Layer 3）

Tk 的很多控件支持 `-textvariable` / `-variable` 这类选项，把一个
Tcl 变量和控件显示内容"绑定"起来：变量变了，控件自动刷新；用户在
控件里输入，变量自动更新。这背后的机制是 Tcl 的 **variable trace**
（`trace add variable`），属于"Tcl Interpreter + Tk"这一层的职责
（见架构图中的 "Variable Traces / Variable Integration"）。
本层先知道"有这么个概念"即可，完整原理留到 Layer 3。

### 3.8 Geometry Manager（布局管理器，先提一下，细节见 Layer 2）

`pack`、`grid`、`place` 是 Tk 提供的三种 geometry manager，职责是
"决定每个 widget 在父容器里的具体像素位置和尺寸"。它们同样只是
Tcl 命令，运行时属于"Tcl Interpreter + Tk"层。一个 widget 如果被创建
但从未交给任何 geometry manager 管理，它永远不会出现在屏幕上——这是
后面"Common Failure"一节要排查的第一个嫌疑对象。本层先记住这个结论，
原理和三种管理器的对比留到 Layer 2。

### 3.9 `wm`、`toplevel`、`pack`、`grid`、`bind` 在架构图中的位置

这是一个经常被问到的问题：这些命令到底算 Tcl 的一部分，还是 Tk 的
一部分？答案是——**它们全部是 Tk 注册进 Tcl 解释器的命令**，运行时
统一属于架构图中 "Tcl Interpreter + Tk" 这一层：

| 命令 | 职责 | 对应架构图里的职责块 |
|---|---|---|
| `toplevel` | 创建一个新的顶层窗口（独立于 `.` 的另一个根窗口） | Widget Management |
| `wm` | 操作顶层窗口的窗口管理属性（标题、大小、图标、是否可缩放等） | Widget Management（针对顶层窗口的特化操作） |
| `pack` / `grid` / `place` | 几何管理器，计算子控件在父容器内的位置和尺寸 | Widget Management |
| `bind` | 注册"事件序列 -> 脚本"的绑定关系 | Event Processing |

它们都不是独立的子系统，而是同一个 Tk 工具包内部按"职责"划分出来的
命令族，运行时都发生在"Tcl Interpreter + Tk"这一层，最终通过这一层
与"Window System / Display"交互（例如 `wm geometry` 最终要请求窗口
系统调整原生窗口尺寸）。

### 3.10 `wish` 与 `tclsh` 的区别

| | `tclsh` | `wish` |
|---|---|---|
| 内置解释器 | Tcl 解释器 | Tcl 解释器 |
| 是否自动加载 Tk | 否，需要手动 `package require Tk` | **是**，启动时自动执行 |
| 是否自动创建主窗口 | 不适用（没有 Tk） | **是**，自动创建 `.`（根窗口） |
| 脚本跑完后是否自动进入事件循环 | **否** | **是**，自动进入事件循环，阻塞等待事件，直到所有窗口关闭 |
| 没有脚本参数时的默认行为 | 进入交互式 REPL，通过**标准输入**逐行读取命令并执行 | 同样会进入交互式 REPL（并且窗口已经打开），但通常直接传 `.tcl` 文件使用 |

一句话总结：

> **`wish` = `tclsh` + 自动 `package require Tk` + 自动创建主窗口 `.`
> + 自动进入事件循环。**

一个容易被忽略的细节：**`tclsh` 默认并不会为了"保持运行"而主动进入
一个阻塞的事件循环**。如果你写一个只有 `puts hello` 的脚本交给
`tclsh`，它打印完就直接退出。`tclsh` 的"持续运行"能力，是靠它在
没有脚本参数时把**标准输入**当作命令源，一行一行读取并执行
（典型的 REPL 模式），这和"事件循环等待 GUI 事件"是完全不同的两种
"保持运行"的方式，不要混为一谈。

### 3.11 `package require Tk` 做了什么

当一个运行在 `tclsh` 下的脚本执行到 `package require Tk` 时
（或者 `wish` 在启动阶段自动执行这一步），依次发生：

1. **加载 Tk 扩展**：按包管理机制找到 Tk 的实现（在 Tcl 8.6 下通常是
   一个动态库，例如 `libtk8.6.so`），把它加载进当前解释器进程。
2. **初始化 Tk 子系统并连接显示服务器**：Tk 会尝试连接窗口系统
   （读取 `$DISPLAY` 环境变量并连接 X Server，或连接 Win32/Aqua 的
   等价机制）。**如果连接失败，`package require Tk` 会直接报错并
   中止当前命令的执行**——这正是本教程在 headless 环境下校验 GUI
   脚本语法的关键切入点：只要脚本能执行到这一步才出错，就说明这行
   之前的所有 Tcl 语法都是合法的。
3. **创建主窗口 `.`**：连接成功后，Tk 会自动创建一个顶层窗口，
   固定使用 widget 路径 `"."`（永远只有这一个根窗口是隐式创建的，
   之后如果要开新窗口需要显式调用 `toplevel`）。
4. **注册 Tk 相关命令**：`button`、`label`、`pack`、`grid`、`wm`、
   `bind`……等几十个命令被注册进当前 Tcl 解释器，从这一刻起才能被
   当成普通 Tcl 命令调用。

---

## 4. Complete Example（Hello Tk）

文件：[`hello.tcl`](./hello.tcl)（与本节代码完全一致，可直接运行）

运行方式：

```bash
# 有图形环境（X11 / Xorg / XQuartz / Windows 桌面）：
wish hello.tcl

# headless 环境（本教程编写/校验环境）：只能校验到 package require Tk
tclsh hello.tcl
```

完整代码：

```tcl
#!/usr/bin/env wish
#
# hello.tcl —— Layer 0 Complete Example
#
# 这是本教程第一个完整可运行的 Tk 程序。
# 它故意保持极简：只有一个 label 和一个 button，
# 目的是让你能把全部注意力放在"架构"而不是"界面细节"上。
#
# 运行方式：
#   有图形环境：   wish hello.tcl
#   headless 环境： tclsh hello.tcl   （只能校验到 package require Tk 这一步）
#
# 关于下面这一行：
#   package require Tk
#
# 这一行做了三件事（按发生顺序）：
#   1. 加载 Tk 扩展库（动态加载 libtk*.so / Tk 的 C 扩展），
#      把 button、label、pack、wm、bind 等命令注册进当前 Tcl 解释器。
#   2. 尝试连接到窗口系统（X11 / Win32 / Aqua），
#      如果连接不上（例如没有 $DISPLAY），这一行会直接报错并中止脚本。
#      这正是我们在 headless 环境下校验语法的关键点：
#      只要脚本执行到这里才报错，说明这行之前的 Tcl 语法全部合法。
#   3. 连接成功后，自动创建主窗口（根窗口），其 widget 路径固定为 "."
#      之后所有顶层窗口、控件都以 "." 为根，组成一棵 widget 树。
package require Tk

# --------------------------------------------------------------------------
# 阶段 4：配置窗口（这一步不创建新控件，只是设置主窗口 "." 的窗口管理属性）
# --------------------------------------------------------------------------
# wm 是 "window manager" 命令族，专门用来和窗口系统/窗口管理器打交道，
# 例如设置标题栏文字、初始大小、最小/最大尺寸、是否可缩放等。
# 它和 button/label 这类"控件命令"不同：wm 操作的对象永远是顶层窗口
# （"." 或 toplevel 创建出来的窗口），而不是普通子控件。
wm title    . "Hello Tk"
# 设置窗口初始大小为 400x200（宽 x 高，单位像素）。
# 注意：这只是"期望大小"，真正的像素级渲染要等窗口系统接管后才会发生，
# 而且后续如果里面的控件需要更大空间，几何管理器可能会覆盖这个初始值。
wm geometry . 400x200

# --------------------------------------------------------------------------
# 阶段 1 + 阶段 2：创建控件 + 配置控件（label）
# --------------------------------------------------------------------------
# label .message -text "Hello, Tcl/Tk!"
#
# 这一条命令同时完成了"创建"和"配置"两件事：
#   - 创建：Tk 在内部分配一个新的控件对象，widget 路径是 ".message"
#     （因为父窗口是 "."，所以子控件路径就是 "." + "message"）。
#     同时，Tcl 解释器里会新增一个同名命令 ".message"——
#     这就是 Layer 1 要讲的"控件即命令"。
#   - 配置：-text "Hello, Tcl/Tk!" 在创建时就把文字内容设置好了。
#     这一步只是在控件的内部状态表里写入一个选项值，
#     并不会立刻触发任何屏幕绘制——因为这个控件还没有被"布局"，
#     甚至主窗口本身可能都还没有真正映射到屏幕上。
label .message -text "Hello, Tcl/Tk!"

# --------------------------------------------------------------------------
# 阶段 3：把控件安排到布局中（pack 是 Layer 2 的主角，这里只做最少量使用）
# --------------------------------------------------------------------------
# pack .message -padx 20 -pady 20
#
# pack 是一种"几何管理器"（geometry manager），它的职责是：
#   决定每个控件在父容器里的具体位置和尺寸。
# 在 pack .message 这条命令执行之前，.message 虽然已经存在（是一个合法的
# Tcl 命令），但它并不会出现在窗口里——未被任何 geometry manager 管理的
# 控件永远不会被显示。-padx/-pady 是在控件外侧留白（单位像素）。
pack .message -padx 20 -pady 20

# --------------------------------------------------------------------------
# 阶段 1 + 阶段 2：创建控件 + 配置控件（button），并引入 Callback 概念
# --------------------------------------------------------------------------
# button .exit -text "Exit" -command {destroy .}
#
# 这里的关键点是 -command 选项：
#   {destroy .} 只是一段被原样存储起来的 Tcl 脚本字符串，
#   在这一行执行完之后，它不会被执行，什么都不会发生。
#   它会被 Tk 保存在 .exit 这个控件的内部配置表里，
#   等到将来用户真正点击这个按钮、事件循环把"鼠标释放"事件分发给
#   .exit 控件时，Tk 内部才会用类似 uplevel #0 的方式去执行这段脚本。
#   这就是 Layer 4 要深入讲的 Callback 机制；这里先建立直觉：
#   "创建按钮" 和 "按钮被点击时执行回调" 是两个完全不同时间点发生的事。
button .exit -text "Exit" -command {destroy .}
pack .exit -pady 10

# --------------------------------------------------------------------------
# 阶段 5：进入事件处理过程
# --------------------------------------------------------------------------
# 用 wish 运行本文件时，脚本执行到文件末尾后，wish 会自动进入事件循环
# （源码里等价于隐式调用了一次 vwait 风格的事件等待），
# 此时窗口才会真正保持显示状态，持续等待操作系统转发过来的事件
# （鼠标移动、点击、键盘输入、窗口重绘请求等），并把它们分发给对应的 widget。
# 本文件没有写任何显式的 vwait 语句，这是 wish 和 tclsh 的关键差异之一，
# 在 README.md 的 "wish 与 tclsh 的区别" 一节会详细说明。
#
# 阶段 6（用户触发回调）和阶段 7（销毁窗口）都发生在事件循环内部：
# 用户点击 Exit -> 事件循环捕获事件 -> 分发给 .exit -> 执行 {destroy .}
# -> 主窗口 "." 连同其下所有子控件（.message、.exit 本身）被销毁
# -> 窗口系统中对应的原生窗口资源被释放 -> 事件循环检测到没有窗口可等待
# -> wish 进程自然退出（不需要显式调用 exit）。
```

### 4.1 确认 Tcl/Tk 已正确安装

```bash
# 确认两个解释器都存在于 PATH 中
which tclsh wish

# 查看 Tcl 版本号（本教程基于 8.6.11 校验）
echo 'puts [info patchlevel]' | tclsh
```

### 4.2 如何运行 `.tcl` 文件

```bash
# 在有图形显示环境的机器上（本教程推荐的"真实验证"方式）：
wish hello.tcl

# 在 headless 环境（没有 $DISPLAY，没有 Xvfb，本教程的编写/校验环境）：
# 只能用 tclsh 做"语法校验"，看不到真实窗口，预期会在 package require Tk
# 这一行报错，错误信息类似：
#   no display name and no $DISPLAY environment variable
tclsh hello.tcl
```

**这不是 bug，是预期行为**：`tclsh` 不会跳过 `package require Tk`，
它会老老实实尝试加载 Tk 并连接显示服务器，只是因为当前终端没有
`$DISPLAY`、没有运行 Xvfb 这类虚拟显示服务，所以连接必然失败。
只要报错发生的位置是这一行、而不是更早的语法错误，就说明脚本本身
在语法层面是正确的。

---

## 5. Execution Walkthrough（按执行顺序逐条解释关键语句）

把 `hello.tcl` 的执行过程严格拆成 7 个阶段，和题目要求一一对应：

### 阶段 1：创建控件

- `label .message -text "Hello, Tcl/Tk!"` —— Tk 在控件树里新增一个节点
  `.message`，同时在 Tcl 解释器里注册同名命令。
- `button .exit -text "Exit" -command {destroy .}` —— 同理，新增
  `.exit` 节点和同名命令。

这两条命令执行完，控件**已经存在**，但**还没有出现在任何窗口画面里**。

### 阶段 2：配置控件

创建控件的命令本身就带了一批 `-option value` 对（`-text`、`-command`），
这些选项在**创建时**就一起设置好了，等价于创建后紧跟着调用：

```tcl
.message configure -text "Hello, Tcl/Tk!"
.exit    configure -text "Exit" -command {destroy .}
```

也就是说，"创建"和"配置"在语法上可以合并写在同一条命令里，也可以事后
用 `configure` 单独调用——两者修改的是同一张"控件选项表"，没有本质区别。

### 阶段 3：将控件安排到布局中

- `pack .message -padx 20 -pady 20`
- `pack .exit -pady 10`

这两条命令把 `.message`、`.exit` 交给 `pack` 这个 geometry manager
管理。从这一刻起，`pack` 才会在**内部**记录"这个控件归我管"，并且在
稍后重新计算整个窗口的布局（宽高、各控件的具体像素坐标）。
**没有被 `pack`/`grid`/`place` 管理过的控件，永远不会显示**，这是
Layer 2 的核心主题，这里先留一个印象。

### 阶段 4：显示窗口

- `wm title . "Hello Tk"`、`wm geometry . 400x200` 设置的是窗口的
  "期望属性"，它们可以在控件创建之前或之后调用，顺序并不敏感。
- 真正"窗口出现在屏幕上"这件事，并不是由某一条命令触发的，而是：
  当 `wish` 完成脚本的同步执行、进入事件循环之前，Tk 会要求窗口系统
  把主窗口 `.` **映射（map）**到屏幕上，此时 geometry manager 已经
  算好了所有子控件的位置，窗口系统据此一次性渲染出完整画面。
  也就是说：**`wm geometry` 只是提了一个尺寸请求，真正的像素级布局
  计算是 pack/grid 做的，而真正的屏幕绘制是窗口系统做的**，三者是
  分工合作的关系。

### 阶段 5：进入事件处理过程

`hello.tcl` 的最后一行代码执行完之后，脚本本身已经"跑完"了。但因为
是用 `wish` 启动的，`wish` 会在脚本执行完毕后自动进入事件循环，
程序不会退出，而是持续等待事件——这正是你用 `wish` 打开一个 GUI 程序后
窗口能"停留在那里"而不是一闪而过的原因。

### 阶段 6：用户触发回调

用户点击 Exit 按钮后，窗口系统捕获到一次鼠标释放事件，经过
Event Loop 分发给 `.exit` 控件，Tk 内部识别出这是一次"按钮被激活"，
于是取出之前存在 `.exit` 的 `-command` 选项里的脚本 `{destroy .}`，
用相当于 `uplevel #0 {destroy .}` 的方式在全局作用域下求值——此时，
也只有此时，这段脚本才第一次被真正执行。

### 阶段 7：销毁窗口

`destroy .` 是一条普通的 Tk 命令，作用是销毁 `.` 这个 widget
**以及它的所有子孙控件**（`.message`、`.exit` 都会被级联销毁）。
销毁主窗口会导致：

1. 窗口系统释放对应的原生窗口资源（窗口从屏幕上消失）。
2. Tk 内部检测到"已经没有任何顶层窗口存活"。
3. Event Loop 因为没有东西可等待而退出循环。
4. `wish` 进程的隐式事件循环结束，脚本执行流"回到"文件末尾，
   由于后面没有更多代码，进程自然终止——全程不需要显式调用 `exit`。

---

## 6. Data Flow / Control Flow（输入、变量、回调、界面更新之间的关系）

```text
+----------------------------------------------------------------+
| 1. User Action (mouse click / key press)                       |
+----------------------------------------------------------------+
                                |
                                v
+----------------------------------------------------------------+
| 2. Window System delivers a native event                       |
|    (XEvent / Win32 message / Aqua event)                       |
+----------------------------------------------------------------+
                                |
                                v
+----------------------------------------------------------------+
| 3. Tk Event Loop receives the event                            |
|    (running inside vwait, driven by wish)                      |
+----------------------------------------------------------------+
                                |
                                v
+----------------------------------------------------------------+
| 4. Tk resolves the target widget and looks up                  |
|    its binding (-command script, or bind sequence)             |
+----------------------------------------------------------------+
                                |
                                v
+----------------------------------------------------------------+
| 5. Tcl interpreter evaluates the callback script               |
|    (parse -> substitute -> evaluate, like any command)         |
+----------------------------------------------------------------+
                                |
                                v
+----------------------------------------------------------------+
| 6. Callback runs: set/trace variables, call widget             |
|    commands, create/destroy widgets, schedule 'after'          |
+----------------------------------------------------------------+
                                |
                                v
+----------------------------------------------------------------+
| 7. Tk re-renders affected widgets via Window System            |
+----------------------------------------------------------------+
        (back to step 3 -- Event Loop waits for next event)
```

另一个同样重要的视角是**时间线**：区分"脚本加载阶段"（同步、从上到下
一次性跑完）和"事件循环阶段"（异步、持续到窗口全部关闭）：

```text
+--------------------------------------------------------------------+
| Script Loading Phase (synchronous, top-to-bottom)                  |
| ----------------------------------------------------               |
| package require Tk        -> Tk subsystem initialized              |
| wm title/geometry .       -> configure root window                 |
| label .message ...        -> create + configure widget             |
| pack .message ...         -> geometry manager places it            |
| button .exit -command {destroy .}                                  |
|                           -> script string is STORED only,         |
|                              NOT executed here                     |
+--------------------------------------------------------------------+

                                  |
                                  v
+--------------------------------------------------------------------+
| Event Loop Phase (asynchronous, runs until no window left)         |
| ----------------------------------------------------------         |
| wish enters event loop automatically (tclsh does not)              |
|    loop: wait for event -> dispatch -> evaluate script             |
|                                                                    |
| user clicks .exit  ==>  stored script {destroy .} is               |
|                         finally evaluated HERE, not before         |
+--------------------------------------------------------------------+
```

从这两张图可以直接回答题目里的核心问题：

> **为什么创建一个按钮不等于立即执行按钮回调？**

因为 `button .exit -command {destroy .}` 发生在"Script Loading
Phase"（阶段 1~5），这个阶段里 `{destroy .}` 只是一个**被当作字符串
存储**的参数值，和 `-text "Exit"` 没有本质区别——Tcl 解释器在求值
`button` 这条命令时，并不会"顺便"把 `-command` 的值当脚本执行一次，
它只是把这个值原样塞进 `.exit` 控件的选项表。只有进入"Event Loop
Phase"之后，Tk 的按钮事件处理逻辑才会主动对这个存好的字符串做一次
`uplevel`/`eval`，这才是它"被执行"的唯一时刻。

Tcl 变量（`-textvariable`）和界面更新的关系也遵循同样的分层：
设置/读取变量本身是"Script Loading Phase"或"Callback 执行期间"
发生的同步操作；但"变量变化导致界面重绘"这件事，依赖的是 Tk 在
"Tcl Interpreter + Tk"层注册的 variable trace，触发重绘的最终效果
仍然要通过 Window System 完成——这部分完整细节见 Layer 3。

---

## 7. Common Failure（典型错误 + 调试方法）

### 7.1 `package require Tk` 报错

**现象**：

```text
no display name and no $DISPLAY environment variable
```

或

```text
couldn't connect to display ":0"
```

**可能原因**：

- 系统根本没有安装 Tk（`libtk*.so` / Tk 包缺失）——用
  `echo 'package require Tk' | tclsh` 单独测试，如果连"找不到包"
  都报出来（而不是"连不上显示"），说明是安装问题，需要装
  `tk`/`tcl-tk` 相关系统包。
- 当前终端/会话没有 `$DISPLAY` 环境变量（SSH 远程登录没加 `-X`，
  或者就是本教程这种 headless 环境）——用 `echo $DISPLAY` 检查。
- 有 `$DISPLAY` 但指向的 X Server 不可达（网络问题、X Server 没启动、
  `.Xauthority` 权限问题）。

**调试方法**：先确认 `echo $DISPLAY` 是否有输出；再确认目标主机上
是否真的有图形环境或 Xvfb 之类的虚拟显示服务在跑；如果是 SSH，
确认使用了 `ssh -X` 或 `-Y` 并且本地也装了 X Server。

### 7.2 窗口没有显示（脚本能跑，但看不到任何东西）

**可能原因**：

- `wm withdraw .` 被误用或被某个库间接调用——`withdraw` 会主动
  隐藏窗口，这是它的设计用途（例如先隐藏主窗口、只显示一个自定义
  对话框），但如果是误用，就会表现为"程序在跑，但窗口不可见"。
  排查方法：搜索代码里是否出现 `withdraw`，并确认后面有没有
  对应的 `wm deiconify .` 把它重新显示出来。
- 控件创建了，但忘记调用 `pack`/`grid`/`place`——这是最常见的初学者
  错误。控件命令本身不会报错（`.message` 作为命令依然合法），
  但因为没有被任何 geometry manager 接管，它永远不会出现在屏幕上。
  排查方法：对每一个你新建的控件，检查是否有对应的
  `pack <path>` / `grid <path>` / `place <path>` 调用。
- 窗口被 `wm geometry` 设置到了屏幕可视区域之外（例如
  `wm geometry . 300x200+5000+5000`，把窗口放到了屏幕坐标
  `(5000,5000)`，实际屏幕根本没有那么大）——排查方法：去掉
  geometry 字符串里的偏移量部分（`+x+y`），只保留 `宽x高`，
  让窗口管理器用默认位置摆放，确认是不是偏移量的问题。

### 7.3 回调没有执行 / 执行了却没反应

这是 Layer 4 的主题，但这里先给出本层能解释的两个基础排查点：

- 确认 `-command` 里的大括号 `{}` 完整闭合——如果大括号不匹配，
  `tclsh`/`wish` 会在脚本加载阶段就直接报 `missing close-brace` 之类
  的语法错误，根本不会跑到事件循环；用本教程的校验方式
  （`tclsh file.tcl`）可以在 headless 环境下提前发现这类问题。
- 确认回调脚本里引用的变量名、过程名是否在**全局作用域**可见——
  回调执行时默认是在全局作用域下求值的（类似 `uplevel #0`），
  如果脚本里直接写了一个只在某个 `proc` 局部作用域存在的变量名，
  运行到那一刻会报 `can't read "xxx": no such variable`。

---

## 8. Engineering Practice（实际工程中如何组织代码）

即使是 Layer 0 这种"架构普及"层面的内容，也建议遵守下面这几条实践，
它们会贯穿后面所有 Layer：

1. **脚本顶部统一写 `package require Tk`**，不要依赖 `wish` 的隐式加载。
   这样同一份代码未来即使被改造成"可选 GUI"（部分功能不需要界面），
   也能清楚看到 Tk 依赖是显式声明的。
2. **把"窗口级配置"（`wm title`、`wm geometry`）和"控件创建/布局"
   分段放置**，就像 `hello.tcl` 里那样用注释分隔出几个阶段——
   这样读代码的人可以一眼看出"这是在配置窗口本身"还是"在搭建控件树"。
3. **`-command` 里尽量只写一个过程调用，不要写一整段逻辑**，例如
   用 `button .exit -command onExitClicked` 搭配单独定义的
   `proc onExitClicked {} {...}`，而不是把销毁逻辑、日志、校验全部
   塞进一行 `-command` 脚本里。这条实践会在 Layer 4、Layer 5 详细展开。
4. **headless 环境下用 `tclsh file.tcl` 做"预检"是一个值得固化到
   CI 里的习惯**：至少能提前抓住花括号/引号不匹配、未定义命令名等
   纯语法问题，而不需要每次都在真实图形环境里手动点开窗口才发现
   脚本写错了。

---

## 9. Exercises（2~3 个由浅入深的练习）

1. **基础**：复制 `hello.tcl` 为 `hello_ex1.tcl`，在 `.message` 和
   `.exit` 之间再加一个 `label`，内容随意，用 `pack` 让三个控件
   纵向排列。用 `tclsh hello_ex1.tcl` 校验语法（预期仍然在
   `package require Tk` 处报错，且不出现更早的语法错误）。

2. **进阶**：把 `button .exit -command {destroy .}` 改成先打印一行
   日志再销毁窗口，例如：

   ```tcl
   proc onExitClicked {} {
       puts "Exit button clicked, destroying main window..."
       destroy .
   }
   button .exit -text "Exit" -command onExitClicked
   ```

   思考题：为什么这里直接写 `-command onExitClicked`（不加花括号）
   也是合法的？它和 `-command {onExitClicked}` 有区别吗？
   （提示：回忆 3.1 节的 parse -> substitute -> evaluate 三步机制。）

3. **挑战**：故意写一个"控件创建了但忘记 pack"的版本
   （`hello_ex3.tcl`），即只执行 `label .message -text "..."` 而不调用
   `pack .message`。用 `tclsh hello_ex3.tcl` 校验，确认它依然会在
   `package require Tk` 处报错（因为这是语法层面完全合法的代码）。
   然后写一段话解释：为什么这种错误**无法**在 headless 环境靠
   `tclsh` 校验出来，必须去到有图形界面的机器用 `wish` 实际运行才能
   发现"控件没显示"这个问题。这道练习是为了让你亲身体会本层反复
   强调的"语法校验"和"运行时行为校验"是两件不同的事。

---

## 10. Mental Model Summary（不超过 10 条要点）

1. Tcl 解释器只做一件事：parse -> substitute -> evaluate，对所有命令
   （包括 Tk 命令）一视同仁。
2. Tk 不是新语言，是一批通过 Tcl 命令暴露出来的 GUI 操作接口。
3. 架构图的三层（Tcl Application / Tcl Interpreter + Tk /
   Window System）是职责边界，不是进程边界——`wish` 通常是单进程。
4. Widget 既是 GUI 对象，又是一个同名的 Tcl 命令。
5. 控件创建、配置、布局（pack/grid）都发生在"脚本同步执行阶段"，
   不会自动出现在屏幕上，直到窗口被映射、事件循环开始运行。
6. `-command`/`bind` 存的只是一段脚本字符串，写下它的那一刻不会执行。
7. 回调真正被求值的唯一时机，是 Event Loop 把对应事件分发到该控件时。
8. Event Loop 是 GUI 程序"活着"的根本原因；没有它，脚本跑完就退出。
9. `wish` = `tclsh` + 自动 `package require Tk` + 自动创建 `.`
   + 自动进入事件循环；`tclsh` 默认靠标准输入驱动交互式 REPL。
10. headless 环境下，`tclsh file.tcl` 能校验"语法是否合法"，但不能
    校验"界面是否正确显示"——这是两类完全不同的验证。

---

## 11. Knowledge Check（3 个检查问题，不给答案）

1. 如果把 `hello.tcl` 里的 `pack .exit -pady 10` 这一行删掉，用
   `wish` 实际运行会发生什么？`tclsh hello.tcl` 的校验结果会不会
   因此改变？为什么？

2. 假设你把 `button .exit -text "Exit" -command {destroy .}` 改成
   `button .exit -text "Exit" -command {puts "clicked"; destroy .}`，
   从"命令什么时候被求值"的角度解释：这个改动会不会影响
   `tclsh hello.tcl` 的校验结果（报错位置是否还是在
   `package require Tk`）？

3. 请用你自己的语言，解释一遍"为什么创建按钮不等于执行回调"，
   并指出：如果去掉整个事件循环（假设有一种方式可以阻止 `wish`
   自动进入事件循环），`hello.tcl` 里存进 `.exit` 的那段
   `{destroy .}` 脚本永远不会被执行的根本原因是什么？
