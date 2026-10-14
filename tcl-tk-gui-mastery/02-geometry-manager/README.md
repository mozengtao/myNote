# Layer 2 —— Geometry Manager 与布局系统

> 核心问题：为什么创建 Widget 之后，还需要单独安排布局？

本 Layer 聚焦 Tk 的三种几何管理器（geometry manager）：`pack`、`grid`、`place`。
它们解决的是同一个问题——"子控件应该出现在父容器的哪个位置、占多大尺寸"——
但采用了三种完全不同的设计思路。

---

## 1. Why

在 Layer 1 里我们看到：`button .open -text "Open"` 这一行命令执行完之后，
`.open` 已经是一个合法的 Tcl 命令、一个真实存在的 Tk 控件对象，拥有自己的
内部状态表（文字、颜色、字体……），但它**不会出现在屏幕上任何地方**。

这不是 bug，而是 Tk 有意为之的设计：

- **创建控件**（`button`、`entry`、`label` ……）回答的问题是："这个对象是什么、
  它内部的状态是什么"——这是控件自身的职责。
- **安排布局**（`pack`、`grid`、`place`）回答的问题是："这个对象应该出现在
  父容器的哪个位置、占多大尺寸、父容器变化时它该怎么跟着变"——这是
  **几何管理器**的职责。

如果不把这两件事分开，会出现什么问题？想象一下：如果 `button` 命令本身
就要负责计算"我在父窗口里具体应该放在第几个像素、占多宽"，那么同一个
`button` 命令在"工具栏场景"（顺序排列）、"表单场景"（行列对齐）、
"自由拖拽场景"（绝对坐标）里就需要三种完全不同的内部实现——
控件的"内容"和"摆放算法"会被硬耦合在一起。

Tk 的做法是把"摆放算法"抽出来，做成三种可独立选用、可替换的几何管理器：
同一个 `button`，不改一行创建代码，换一种几何管理器调用方式，就能适配
完全不同的布局场景。这是一次典型的**关注点分离**（separation of concerns）。

---

## 2. Mental Model

> 一句话 Mental Model：
> **"创建控件"决定它是什么；"几何管理器"决定它在哪、多大；
> 两者是两个独立的子系统，靠"控件路径"这一个共同的名字耦合在一起。**

三种几何管理器的直觉差异：

```text
+--------------------------------------------------------------------+
| pack  : side-based ordered cavity allocation                       |
|   "把父容器想象成一块橡皮泥（cavity），每次调用从某个边缘切一块"            |
|                                                                    |
| grid  : table-based cell allocation                                |
|   "把父容器想象成一张看不见的表格，每个控件占一个或多个单元格"              |
|                                                                    |
| place : absolute / relative coordinate placement                   |
|   "直接告诉 Tk：这个控件的 x, y, width, height（或相对比例）是多少"      |
+--------------------------------------------------------------------+
```

- `pack`：**不关心坐标，只关心"顺序"和"边"**。你告诉它"从哪个边開始占"，
  它就按调用顺序，依次从父容器剩余空间里切走一条。
- `grid`：**不关心调用顺序，只关心"行列号"**。你告诉它"这个控件在第几行第几列"，
  它负责把整张表格的行高、列宽算出来，再把每个单元格分配给对应控件。
- `place`：**什么都不算，你说了算**。你直接给出像素坐标或相对坐标（0.0~1.0），
  Tk 原样使用，不做任何"留白分配"或"表格对齐"的推理。

---

## 3. Architecture

### 3.1 参与者

| 角色 | Tk 官方术语 | 说明 |
|---|---|---|
| 父容器 | master | 被管理的子控件所依附的容器（可以是 `.`、`frame`、`toplevel`） |
| 子控件 | slave | 被某个几何管理器安排位置/尺寸的控件 |
| 几何管理器 | geometry manager | `pack` / `grid` / `place` 三者之一，每个 master 在某一时刻只能被其中一种管理 |
| 几何管理协议 | geometry negotiation | master 和 slave 之间关于"要多大空间 / 实际给多大空间"的协商过程 |

### 3.2 几何管理协议（geometry negotiation）如何工作

```text
+------------------------+        +---------------------------+
| Slave widget           |        | Geometry Manager          |
| (button / entry / ...) |        | (pack state / grid state  |
|                        |        |  attached to this master) |
| - reports "requested   | -----> | - reads slave's requested |
|   size" via its own    |        |   size (winfo reqwidth/   |
|   content + padding/   |        |   reqheight)              |
|   border options       |        | - combines it with        |
|                        |        |   -sticky/-fill/-expand/  |
|                        |        |   -weight/-padx/-pady     |
|                        | <----- | - computes actual x, y,   |
| - gets actually resized|        |   width, height           |
|   / repositioned       |        | - calls low-level resize/ |
|                        |        |   move on the slave       |
+------------------------+        +---------------------------+
            ^                                              ^
            |          master 尺寸变化触发重新计算            |
            +---------------- Configure event -------------+
```

关键认知：

1. 每个 slave 控件自己只负责报告"我天然需要多大空间"（基于文字长度、字体、
   内部 padding/border 等），这叫 **requested size**，不等于最终显示尺寸。
2. 几何管理器拿到这个 requested size，结合你传给 `pack`/`grid`/`place` 的
   选项，以及 master 当前的实际可用空间，计算出每个 slave 最终的
   `x, y, width, height`，这叫 **allocated size**。
3. 当 master 的尺寸发生变化（例如用户拖动窗口边框触发 `Configure` 事件），
   几何管理器会重新执行一遍第 2 步的计算——这就是"窗口缩放时控件会跟着变化"
   的底层机制，也是本 Layer 后面要重点排查的"为什么没有跟着变化"的切入点。
4. **一个 master 在同一时刻只能被一种几何管理器管理它的直接子控件集合**。
   `pack` 和 `grid` 内部各自维护一份"谁是我的 slave、按什么规则排布"的状态表，
   两者的算法和数据结构完全不同，不能对同一个 master 的子控件同时生效
   （第 7 节会展开说明报错表现）。

### 3.3 `place` 的设计思路（点到即可）

`place` 跳过了"留白分配"和"表格对齐"这两套推理，直接使用坐标：

```tcl
;# 示意片段，非完整示例——把 .watermark 永远钉在父容器右下角
place .watermark -relx 1.0 -rely 1.0 -anchor se
```

- `-x`/`-y`：绝对像素坐标（相对 master 左上角）。
- `-relx`/`-rely`：相对坐标，取值 0.0~1.0，表示 master 宽/高的比例位置，
  这是实现"贴边"、"居中悬浮层"效果的常用手段。
- `-anchor`：坐标点对应控件自身的哪个角/边。

`place` 适合**少量、需要精确控制甚至允许重叠**的场景（悬浮工具条、
游戏/绘图类自定义叠加层、可视化编辑器里的拖拽定位），不适合整版表单或
工具栏——那些场景里，手工维护坐标的成本远高于让 `pack`/`grid` 自动计算。
本 Layer 不展开完整示例。

---

## 4. Complete Example

本 Layer 包含两个完整可运行的 `.tcl` 文件。

### 4.1 `pack_example.tcl` —— 垂直工具栏

运行方式：

```bash
# 有图形环境
wish pack_example.tcl

# headless 环境（本教程编写环境），只能校验到 package require Tk 这一步
tclsh pack_example.tcl
```

完整代码：

```tcl
#!/usr/bin/env wish
#
# pack_example.tcl —— Layer 2 Complete Example (1/2): pack 几何管理器
#
# 场景：一个垂直工具栏（Open / Save / Exit）+ 底部状态栏。
# 用来演示 pack 的核心思路——"顺序留白分配"（ordered cavity allocation）：
#   每次调用 pack，都是从父容器当前剩余的那块矩形空间（cavity）里，
#   按 -side 指定的方向切下一条，分配给当前控件；
#   切完之后，剩下的空间会继续作为新的 cavity，留给后面的 pack 调用。
#   整个过程只关心"从哪个边切、切多少、剩多少"，不涉及行列坐标。
#
# 运行方式：
#   有图形环境：   wish pack_example.tcl
#   headless 环境： tclsh pack_example.tcl   （只能校验到 package require Tk 这一步）

package require Tk

wm title . "Pack Example"
wm geometry . 220x160
wm minsize . 160 120

# --------------------------------------------------------------------------
# 阶段 1：创建控件（只是在内存里建立对象，和"摆在哪里"完全无关）
# --------------------------------------------------------------------------
button .open -text "Open"
button .save -text "Save"
button .exit -text "Exit" -command {destroy .}

# 底部状态栏：用 -relief sunken 让它看起来像一条凹陷的状态条
label .status -text "Ready" -anchor w -relief sunken

# --------------------------------------------------------------------------
# 阶段 2：布局（pack：side-based 顺序留白分配）
# --------------------------------------------------------------------------
# 先 claim 底部：无论这一行写在文件里的哪个位置，只要 -side 是 bottom，
# 它就会从"当前剩余 cavity"的底边切一块下来。
# 这里特意把它放在三个按钮之前调用，用来证明：
#   最终布局结果取决于"每次调用用的是哪个 -side"，
#   而不是单纯取决于"这一行代码写在文件里的先后顺序"。
pack .status -side bottom -fill x

# 再从剩余 cavity 的顶部，依次 claim 三条横向空间给三个按钮。
#   -side top   : 每次都从"当前剩余 cavity"的顶边切一条
#   -fill x     : 控件在水平方向被拉伸，填满它分配到的那条空间的宽度；
#                 注意这里没有写 -fill y，所以按钮高度仍是自身的"自然高度"，
#                 不会被拉伸去填满那条空间的高度。
#   -padx/-pady : 控件外侧留白（像素），这部分空间会被预留，
#                 不会被其它控件占用，但也不计入"剩余 cavity"分配逻辑之外。
pack .open .save .exit -side top -fill x -padx 8 -pady 4
```

### 4.2 `grid_example.tcl` —— 可缩放的配置窗口

运行方式：

```bash
# 有图形环境
wish grid_example.tcl

# headless 环境（本教程编写环境），只能校验到 package require Tk 这一步
tclsh grid_example.tcl
```

完整代码：

```tcl
#!/usr/bin/env wish
#
# grid_example.tcl —— Layer 2 Complete Example (2/2): grid 几何管理器
#
# 场景：一个"可缩放的配置窗口"。
#   表单区（Name / Port / Host）内部使用 grid 布局；
#   按钮区（Save / Exit）内部使用 pack 布局；
#   —— 演示"不同父 widget 内部可以分别使用不同的几何管理器"。
#
# 布局树（详见 README.md 中的 ASCII 图）：
#   .
#   ├── .form      (frame, 内部用 grid)
#   │   ├── .form.nameLabel / .form.nameEntry
#   │   ├── .form.portLabel / .form.portEntry
#   │   └── .form.hostLabel / .form.hostEntry
#   └── .buttons   (frame, 内部用 pack)
#       ├── .buttons.save
#       └── .buttons.exit
#
# 运行方式：
#   有图形环境：   wish grid_example.tcl
#   headless 环境： tclsh grid_example.tcl   （只能校验到 package require Tk 这一步）

package require Tk

wm title . "Grid Example"
wm geometry . 360x180
wm minsize . 280 140

# --------------------------------------------------------------------------
# 阶段 1：划分区域 —— 用两个 frame 把窗体切成"表单区"和"按钮区"
# --------------------------------------------------------------------------
# frame 本身没有任何可见内容，它唯一的作用是充当"子容器"：
#   - 对 "."（它的父控件）来说，.form / .buttons 只是两个普通子控件，
#     "." 可以用任意一种几何管理器（这里用 pack）去摆放它们；
#   - 对 .form / .buttons 自己来说，它们又是独立的父容器，
#     各自的子控件可以用另一种、互不相关的几何管理器去摆放。
# 这正是"同一棵 widget 树里混用多种几何管理器"的正确做法——
# 混用发生在不同的父子层级之间，而不是在同一个父控件内部。
frame .form
frame .buttons

# --------------------------------------------------------------------------
# 阶段 2：表单区内部 —— 用 grid 搭建一张"不可见表格"
# --------------------------------------------------------------------------
# grid 的核心思路：把父容器想象成一张由若干行、若干列组成的表格；
# 每个子控件占据表格里的一个单元格（cell），单元格的宽/高由
# 该行/列里"最大的那个控件"决定。
label .form.nameLabel -text "Name:"
entry .form.nameEntry

label .form.portLabel -text "Port:"
entry .form.portEntry

label .form.hostLabel -text "Host:"
entry .form.hostEntry

# -sticky 决定控件在"自己所在单元格内部"如何对齐/拉伸：
#   e  = 贴紧单元格右边缘（标签右对齐，和输入框左边缘视觉对齐更整齐）
#   ew = 水平方向拉伸，同时贴紧单元格左右两边
#        （输入框会随着"所在单元格变宽"而一起变宽）
# 注意：-sticky 只回答"控件如何填满自己的单元格"；
#       单元格本身能不能变大，是下面 -weight 的职责——两者缺一不可。
grid .form.nameLabel -row 0 -column 0 -padx 6 -pady 6 -sticky e
grid .form.nameEntry -row 0 -column 1 -padx 6 -pady 6 -sticky ew

grid .form.portLabel -row 1 -column 0 -padx 6 -pady 6 -sticky e
grid .form.portEntry -row 1 -column 1 -padx 6 -pady 6 -sticky ew

grid .form.hostLabel -row 2 -column 0 -padx 6 -pady 6 -sticky e
grid .form.hostEntry -row 2 -column 1 -padx 6 -pady 6 -sticky ew

# -weight 决定"行/列"如何分配父容器里多余的空间：
#   column 0（标签列）：不需要随窗口变宽而变宽，保持默认 weight 0。
#   column 1（输入框列）：weight 1，表示 .form 出现的多余宽度
#   （因为窗口被拉宽了）全部分配给这一列。
# 行方向这里不需要随窗口变高而变高（三行输入框变高没有意义），
# 所以不设置 rowconfigure -weight，保持默认值 0。
grid columnconfigure .form 0 -weight 0
grid columnconfigure .form 1 -weight 1

# --------------------------------------------------------------------------
# 阶段 3：按钮区内部 —— 用 pack 把按钮靠右排列
# --------------------------------------------------------------------------
button .buttons.save -text "Save"
button .buttons.exit -text "Exit" -command {destroy .}

# -side right：从右边缘开始依次 claim 空间，所以先 pack 的 .exit 最靠右，
# 后 pack 的 .save 排在 .exit 左边——这是对话框按钮区常见的排布方式。
pack .buttons.exit -side right -padx 4 -pady 6
pack .buttons.save -side right -padx 4 -pady 6

# --------------------------------------------------------------------------
# 阶段 4：根窗口 "." 内部 —— 用 pack 把两个区域纵向排列
# --------------------------------------------------------------------------
# .form 和 .buttons 都是 "." 的子控件，它们之间用 pack 摆放：
#   .form    : -fill both -expand 1
#              表单区占据所有"多余空间"；窗口变宽/变高时，
#              多余空间先分给 .form，.form 内部再按 grid 的
#              -weight 规则，把多余宽度进一步分给 column 1。
#   .buttons : -fill x（没有 -expand）
#              按钮区只在水平方向填满宽度，高度保持自身内容的
#              自然高度，不参与垂直方向的多余空间分配。
pack .form -side top -fill both -expand 1 -padx 10 -pady 10
pack .buttons -side bottom -fill x -padx 10 -pady {0 10}
```

### 4.3 `grid_example.tcl` 的布局树

```text
.
├── .form (frame, grid inside)
│   ├── .form.nameLabel / .form.nameEntry
│   ├── .form.portLabel / .form.portEntry
│   └── .form.hostLabel / .form.hostEntry
└── .buttons (frame, pack inside)
    ├── .buttons.save
    └── .buttons.exit
```

- `.` 本身用 **pack** 纵向排列 `.form`（上）和 `.buttons`（下）。
- `.form` 内部用 **grid**：3 行 × 2 列的表格。
- `.buttons` 内部用 **pack**：从右边缘依次排列两个按钮。
- 三处几何管理器各自独立：`.` 的 pack 调用、`.form` 的 grid 调用、
  `.buttons` 的 pack 调用，分别只影响各自的直接子控件，互不干扰。

---

## 5. Execution Walkthrough

### 5.1 `pack_example.tcl` 执行顺序

1. `package require Tk` —— 加载 Tk、创建主窗口 `.`（headless 环境在此处报错，
   证明之前的 Tcl 语法全部合法）。
2. `wm title` / `wm geometry` / `wm minsize` —— 设置窗口管理属性，
   此时窗口里还没有任何控件。
3. `button .open` / `.save` / `.exit`，`label .status` —— 依次创建四个控件，
   它们此刻都"存在但不可见"：每个都是一个合法的 Tcl 命令，但都还没有被
   任何几何管理器安排位置。
4. `pack .status -side bottom -fill x` —— **第一次布局调用**。
   此时 `.` 的 cavity 就是整个窗口客户区。`.status` 从底边切走一条
   高度等于其自然高度、宽度等于整条可用宽度（因为 `-fill x`）的空间。
   切完之后，剩余 cavity 变成"去掉底部那一条之后，上面剩下的矩形"。
5. `pack .open .save .exit -side top -fill x -padx 8 -pady 4` ——
   在**剩余 cavity**（已经不包含 `.status` 占用的那条）里，依次从顶边
   切三条空间给 `.open`、`.save`、`.exit`，每条都在水平方向填满宽度。
6. 文件执行完毕。`wish` 隐式进入事件循环，窗口保持显示，等待用户点击；
   点击 `.exit` 会执行 `{destroy .}`，销毁整棵 widget 树，进程退出。

最终视觉效果（文字描述）：窗口从上到下依次是 Open / Save / Exit 三个
横向撑满宽度的按钮，窗口最底部是一条贴底的状态栏，无论窗口怎么被拉宽，
三个按钮和状态栏始终保持这个相对顺序和"撑满宽度"的效果，但拉高窗口时，
三者的**高度**不会跟着变（因为没有谁设置了 `-fill y` 或 `-expand 1`，
多出来的高度会变成按钮和状态栏之间、以及按钮区下方的空白）。

### 5.2 `grid_example.tcl` 执行顺序

1. `package require Tk` + `wm` 系列设置，同上。
2. `frame .form` / `frame .buttons` —— 创建两个空容器（此刻它们自己也还
   没被任何几何管理器安排位置，和普通控件一样"存在但不可见"）。
3. 在 `.form` 内部创建 3 组 `label`/`entry`（共 6 个控件）。
4. 6 次 `grid` 调用，把这 6 个控件按 `(row, column)` 放进 `.form` 的
   表格里；此时 `.form` 这张表格的列宽/行高，由其中最大的控件
   （加上 `-padx`/`-pady`）决定——这一步只是"把控件分配进单元格"，
   还没有涉及"父容器多余空间怎么分"的问题。
5. `grid columnconfigure .form 0 -weight 0` / `... 1 -weight 1` ——
   告诉 `.form` 的 grid 管理器："如果我（`.form`）将来变宽了，
   多出来的宽度全部给 column 1"。这一步此刻不会立刻改变任何控件的
   显示尺寸，只是写入 `.form` 的 grid 配置表，等到 `.form` 真正变宽时
   才会生效。
6. 在 `.buttons` 内部创建 `.buttons.save` / `.buttons.exit`，用
   `pack -side right` 把它们从右边缘依次排列。
7. 最后两行 `pack .form ...` / `pack .buttons ...`，把 `.form` 和
   `.buttons` 这两个 frame 本身，纵向排列进 `.` 的 cavity 里：
   `.form` 拿到 `-fill both -expand 1`，意味着它会随着窗口变大而变大；
   `.buttons` 只拿到 `-fill x`，高度保持不变。

最终视觉效果（文字描述）：窗口上半部分是三行"标签 + 输入框"，标签右对齐、
输入框左对齐并撑满剩余宽度；窗口下半部分是一行靠右排列的 Save / Exit 按钮。
把窗口拉宽时，三个输入框会跟着变宽（标签宽度不变）；把窗口拉高时，
表单区和按钮区之间会出现更多空白（因为没有给任何一行设置 `rowweight`），
按钮区本身高度始终不变。

---

## 6. Data Flow / Control Flow

几何管理器涉及的"数据"主要分两类，容易混淆，这里先分清楚：

```text
+----------------------------+      +-----------------------------+
| Widget 自身的 option table  |      | Geometry Manager 的 slave   |
| (由 button/label/entry/... |      | info table                  |
|  等控件命令的 -option 设置)  |      | (由 pack/grid/place 的       |
|                            |      |  -option 设置，附着在 slave   |
| 例如：-text, -bg, -font,    |      |  对象上，但归属于某个          |
|       -relief, -command    |      |  master 的管理器实例)         |
+----------------------------+      +-----------------------------+
            |                                   |
            v                                   v
      决定控件"长什么样、点击做什么"      决定控件"出现在哪、多大"
```

**控制流（控件创建 -> 显示的全过程）：**

```text
1. 创建控件命令 (button .x ...)
         |
         v
2. Tk 分配内部对象 + 注册同名 Tcl 命令，写入 option table
   （此时控件"存在"，但没有被任何 geometry manager 接纳为 slave）
         |
         v
3. 调用 pack/grid/place .x ...
         |
         v
4. 对应的几何管理器把 .x 登记为自己的 slave，
   记录 .x 的 -side/-sticky/-row/-column/... 等布局选项
         |
         v
5. 几何管理器触发一次"布局计算"：
   读取 master 当前可用尺寸 + 所有 slave 的 requested size + 布局选项
   -> 算出每个 slave 的 (x, y, width, height)
         |
         v
6. 真正的 resize/move 调用生效，.x 第一次出现在屏幕上
         |
         v
7. 之后任何触发 master 尺寸变化的事件（窗口被拖拽缩放、父容器内容变化
   导致 reqsize 改变等）都会重新触发第 5 步
```

**窗口缩放时的数据流（对应 `grid_example.tcl`）：**

```text
用户拖拽窗口边框
      |
      v
窗口系统发出 Configure 事件给顶层窗口 "."
      |
      v
"." 的 pack 管理器重新计算：.form 和 .buttons 各能拿到多大空间
（.form 有 -expand 1，吃掉几乎全部新增空间；.buttons 维持原高度）
      |
      v
.form 自身尺寸变化，向下传播一次 Configure 给 .form
      |
      v
.form 的 grid 管理器重新计算：
  用 columnconfigure 里的 weight 比例，把 .form 新增的宽度分给 column 1
      |
      v
column 1 里的 3 个 entry 因为 -sticky ew，被拉伸填满新的、更宽的单元格
      |
      v
3 个 entry 的实际显示宽度变大（label 所在的 column 0 没有 weight，不变）
```

这条链路说明一个重要事实：**"能不能随窗口缩放"不是某一个 `-weight`
或某一个 `-sticky` 单独决定的，而是从顶层窗口到具体控件，每一级父子关系
都要把"多余空间"正确传递下去**——任何一级断链（比如中间某个 frame
忘了 `-fill`/`-expand`，或者某一列忘了 `-weight`），下游控件就不会
跟着变化。

---

## 7. Common Failure

1. **创建了控件，但忘了 pack/grid/place —— 控件完全不显示。**
   这是最常见的新手错误：代码没有任何报错，窗口打开后却"少了一块"。
   排查方法：在交互式 `wish` 里对该控件执行 `winfo manager .x`，
   如果返回空字符串，说明它确实没有被任何几何管理器接纳。

2. **在同一个父容器里混用 `pack` 和 `grid` 管理同一组子控件。**
   例如：

   ```tcl
   frame .box
   label .box.a -text "A"
   label .box.b -text "B"
   pack  .box.a -side left
   grid  .box.b -row 0 -column 1   ;# 错误：.box 已经被 pack 接管
   ```

   `pack` 和 `grid` 内部各自维护一份"谁是我的 slave"的状态表，
   使用完全不同的算法（cavity 分配 vs 表格分配）去计算同一个父容器
   `.box` 的空间分配。Tk 不允许一个 master 同时存在两份互相矛盾的
   slave 列表，当检测到这种冲突时会直接报错中止（典型报错信息形如
   "cannot use geometry manager grid inside .box which already has
   slaves managed by pack"，具体文案随 Tk 版本略有差异）。
   **正确做法**：永远只在同一个父容器内用一种几何管理器；如果需要
   两种布局思路并存，像 `grid_example.tcl` 那样，用额外的 `frame`
   把它们拆到不同的父子层级上。

3. **设置了 `-weight`，但控件没设置 `-sticky` —— 单元格变大了，控件却没跟着变大。**
   - *设置前*（只有 `-weight`，没有 `-sticky`）：窗口拉宽后，column 1
     对应的单元格确实变宽了，但 `.form.nameEntry` 默认按选项居中/原位
     摆放在这个变宽的单元格里，输入框本身的宽度**不变**，只是周围多了
     一圈看起来"没对齐"的空白。
   - *设置后*（`-sticky ew` + `-weight 1`）：单元格变宽的同时，输入框
     被要求"贴住单元格左右两边"，于是跟着一起变宽，视觉上和窗口拉伸
     是同步的。
   - 结论：`-weight` 决定"单元格要不要变大"，`-sticky` 决定"控件要不要
     跟着单元格一起变大"，两者必须同时设置才能实现"输入框随窗口变宽"。

4. **设置了 `-sticky`，但忘了给对应的行/列设置 `-weight` —— sticky 看起来"没用"。**
   - *设置前*（只有 `-sticky ew`，所有列 `-weight` 都是默认值 0）：
     窗口拉宽后，`.form` 这个容器本身即使变宽了，column 1 的宽度也
     完全不会跟着变——因为没有任何一列被标记为"可以吃掉多余空间"，
     多余的宽度会被作为额外留白堆在整个表格的右侧，输入框保持原宽度。
   - *设置后*：和上一条相同，`-sticky` + `-weight` 同时生效才有效果。

5. **`pack` 里只写了 `-fill x`，没写 `-expand 1`，就以为控件会随窗口变宽。**
   - `-fill` 只决定"控件在它被分配到的那条 cavity 空间里，要不要把
     自己撑满那条空间的宽度/高度"；它**不会**让那条 cavity 本身变大。
   - 如果那条 cavity 的大小是固定的（没有任何同级或自身设置了
     `-expand 1` 去抢占父容器新增的空间），`-fill x` 在窗口被拉宽后
     不会有任何新效果，因为"那条空间"本身就没有变宽。
   - `grid_example.tcl` 里 `pack .form -fill both -expand 1` 正是靠
     `-expand 1` 让 `.form` 这条 cavity 先把窗口新增的空间抢过来，
     `-fill both` 才有东西可以"填"。

6. **中间某一级 frame 忘了传递 `-expand`/`-weight`，导致"链路断裂"。**
   即使 `.form` 内部的 `grid columnconfigure` 配置完全正确，如果
   `pack .form` 这一步漏掉了 `-expand 1`（只写了 `-fill x`），
   `.form` 本身在窗口变高/变宽时也可能分不到多余空间，于是它内部
   column 1 的 `-weight 1` 根本没有"多余宽度"可分——现象上看就是
   "明明照着教程设置了 weight，输入框还是不跟着变宽"，根因在上一级
   frame 的 pack 选项，而不是 grid 配置本身。排查这类问题要**从顶层
   窗口往下，逐级检查每一层的 fill/expand/weight 是否都传递到位**。

7. **误以为 `place` 的 `-relx`/`-rely` 会自动响应内容变化。**
   `place` 只在你调用它或 master 尺寸变化时重新计算一次坐标，它不会
   感知"控件内容变了、想要更大空间"这类请求（这是 `pack`/`grid` 的
   requested-size 协商机制才有的能力）。用 `place` 摆放的控件如果
   内容变化导致需要更大尺寸，很容易出现文字被裁剪或控件重叠。

---

## 8. Engineering Practice

- **按场景选择几何管理器，而不是固守一种：**
  - 工具栏、按钮条、简单的"从边缘排列"场景 —— 用 `pack`。
  - 表单、属性面板、任何需要"标签列 + 输入列对齐"的场景 —— 用 `grid`。
  - 悬浮水印、可拖拽的自由布局、游戏/画布类叠加层 —— 用 `place`。
  同一个窗口里，不同区域用不同管理器是常态，不是"例外情况"。

- **用 `frame` 主动划分布局区域，而不是把所有控件平铺在根窗口下。**
  像 `grid_example.tcl` 那样，先按"功能区域"拆出 `.form`、`.buttons`
  等 frame，再分别决定每个区域内部用哪种几何管理器。这样做的好处：
  一是从物理上避免了"同一个父容器混用 pack/grid"的错误；二是 widget
  路径天然反映了界面结构（`.form.nameEntry` 比 `.nameEntry` 更有信息量，
  也避免了大型界面里全局平铺带来的命名冲突）。

- **`grid` 的 `columnconfigure`/`rowconfigure` 集中写在该区域所有
  `grid` 调用之后**，形成"先摆控件、再配置弹性"两段式写法，方便日后
  回来调整哪一行/列该随窗口变化，不用在一堆 `grid` 调用里反复查找。

- **`-sticky nsew` + 对应行列 `-weight 1` 是"占满单元格"的标准组合**，
  常用在"主内容区"（例如一个 `text`/`treeview` 控件要尽量占满窗口），
  记成固定搭配，减少每次重新推理。

- **遇到历史代码里"布局好像很乱、改一个地方全乱"的情况，先用
  `winfo manager <widget>` 和 `winfo children <widget>` 这两个命令
  查清楚"这个父容器现在到底被哪个几何管理器接管、它当前都有哪些 slave"**，
  再决定要不要重构，而不要直接在已有的 `pack` 容器里尝试加 `grid` 调用。

- **窗口尺寸相关的默认值（`wm geometry`/`wm minsize`）应该显式写出来**，
  不要依赖"控件自然大小刚好凑成一个合理窗口"的偶然结果，尤其是在
  后续会扩充字段数量的配置类窗口里（新加一行字段，默认高度可能不够）。

---

## 9. Exercises

1. **验证 pack 的顺序留白分配。**
   修改 `pack_example.tcl`：把 `pack .open .save .exit -side top ...`
   改成 `-side left ...`（去掉 `-fill x`，换成 `-fill y`）。先用文字
   预测新的布局效果（三个按钮会怎么排列？`.status` 还会不会贴底？），
   再找一台有图形界面的机器用 `wish` 验证你的预测。

2. **扩充 grid 表单。**
   在 `grid_example.tcl` 的 `.form` 里新增一个 "Password" 字段
   （`.form.passwordLabel` / `.form.passwordEntry`），要求：
   - 正确放在 `row 3`，不破坏现有三行的布局；
   - 不需要新增任何 `columnconfigure` 调用就能让新的 entry 跟着窗口
     变宽——先想清楚为什么"不需要新增"，再动手验证。

3. **故意触发"混用几何管理器"报错，并修复它。**
   在 `.buttons` 内部，`pack .buttons.save` / `pack .buttons.exit`
   这两行之后，故意加一行
   `grid .buttons.save -row 0 -column 0`。用 `tclsh`（能跑到
   `package require Tk` 之前的语法检查）结合你对第 7 节的理解，
   预测这段代码在真正连接到显示环境时会报什么错、为什么会报错；
   然后把这一行删掉，换成合法的写法：新增一个 `frame .buttons.extra`，
   在它内部用 `grid` 摆放一个新按钮，验证"不同父 widget 分别用不同
   管理器"确实不会报错。

---

## 10. Mental Model Summary

1. 创建控件（`button`/`entry`/...）只决定"它是什么"，不决定"它在哪、多大"。
2. 几何管理器（`pack`/`grid`/`place`）是独立子系统，专门负责"在哪、多大"。
3. 没有被任何几何管理器接纳的控件，永远不会出现在屏幕上。
4. `pack` = 顺序留白分配：按 `-side` 从父容器剩余空间的某个边切一块。
5. `grid` = 表格分配：按 `(row, column)` 把控件放进不可见的表格单元格。
6. `-sticky`/`-fill` 决定"控件要不要撑满自己被分配到的空间"。
7. `-weight`/`-expand` 决定"父容器多余的空间要不要分给这一行/列/这个控件"。
8. 两者缺一不可：只有"分到空间"没有"撑满空间"，或者反过来，都不会有
   预期的缩放效果。
9. 同一个父容器只能被一种几何管理器管理；不同的父子层级可以分别使用
   不同的几何管理器，这是推荐的组织方式。
10. 排查"布局没有按预期工作"时，要从顶层窗口开始逐级检查，而不是只看
    报错发生的那一层。

---

## 11. Knowledge Check

1. 为什么 Tk 把"创建控件"和"安排布局"设计成两个独立的步骤，而不是让
   `button`、`label` 这类命令在创建时就顺便决定自己的位置和尺寸？

2. 在 `grid_example.tcl` 里，如果只执行
   `grid columnconfigure .form 1 -weight 1`，但把所有 entry 的
   `-sticky ew` 都删掉，窗口被拉宽之后会发生什么？请说明原因。

3. 为什么不能在同一个父容器（如一个 `frame`）内部，同时用 `pack` 管理
   一部分子控件、用 `grid` 管理另一部分子控件？如果确实需要两种布局
   思路同时出现在一个窗口里，应该怎么组织代码？
