# Layer 9：调试与故障定位

> 前置阅读：Layer 3（变量绑定）、Layer 4（事件驱动）、Layer 5（过程与命名空间）。
> 本层不引入新的 Tk 特性，而是把前面 8 层建立的 Mental Model 反过来用——
> 当 GUI "不符合预期"时，根据 Mental Model 推断问题出在哪个阶段，
> 再用对应的诊断工具去验证，而不是靠猜。

---

## 0. 总纲：调试 Tcl/Tk GUI 问题的通用思路

Tcl/Tk 程序从启动到响应用户操作，严格经过下面五个阶段。**几乎所有"GUI
不对劲"的问题，都可以先问一句："卡在了哪个阶段？"**，再决定用什么工具去看，
而不是上来就怀疑某一行代码。

```text
+--------------------------------+
| Stage 1: Script Parse          |
| tclsh/wish reads and           |
| evaluates the script text      |
+--------------------------------+
              |
              v
+--------------------------------+
| Stage 2: Widget Create         |
| button/label/entry/...         |
| commands allocate widgets      |
+--------------------------------+
              |
              v
+--------------------------------+
| Stage 3: Layout Compute        |
| pack/grid/place decide         |
| widget size and position       |
+--------------------------------+
              |
              v
+--------------------------------+
| Stage 4: Event Loop Run        |
| wish mainloop / vwait          |
| waits for OS input events      |
+--------------------------------+
              |
              v
+--------------------------------+
| Stage 5: Callback Exec         |
| -command / bind scripts        |
| run inside the loop            |
+--------------------------------+
```

对应到中文术语，这五个阶段分别是：

| 阶段 | 中文名称 | 这个阶段在做什么 | 典型"卡住"的症状 |
|---|---|---|---|
| 1 | 脚本解析期 | 解释器从头到尾读取并求值脚本文本（`source`、变量/命令替换、`proc` 定义） | 语法错误、`source` 路径错误、命名空间变量访问失败 |
| 2 | widget 创建期 | 执行 `button`/`label`/`entry`/`frame` 等命令，在 Tk 内部分配控件对象，并注册同名 Tcl 命令 | 选项拼写错误、`-command` 被提前执行 |
| 3 | 布局计算期 | `pack`/`grid`/`place` 决定每个控件的尺寸和位置 | 控件存在但看不见、`pack`/`grid` 混用冲突 |
| 4 | 事件循环运行期 | `wish` 的隐式事件循环（或显式 `vwait`）持续等待操作系统转发事件 | 窗口没有显示、GUI 冻结 |
| 5 | 回调执行期 | 事件被分发给某个 widget，对应的 `-command`/`bind` 脚本在事件循环内部被求值 | 回调没执行、回调读到旧值、回调访问了已销毁的 widget |

**排查步骤建议：**

1. 先确定现象发生在哪个阶段（例如"控件创建成功但看不到"明显是阶段 2 之后、
   阶段 3 没有按预期完成）。
2. 用下一节列出的诊断工具，在该阶段的"输入"和"输出"上分别打点观察，
   把问题范围从"整个程序"收缩到"某一个 widget / 某一个变量 / 某一次回调"。
3. 只有确定了根因之后，才去改代码——盲目改代码然后"再试试"是效率最低的调试方式。

---

## 1. 诊断工具一览

下面这些工具不依赖图形显示环境本身（它们都是查询 Tcl/Tk 解释器内部状态的
命令），但其中以 `winfo`/`pack`/`grid` 开头的几个需要 Tk 已经加载、widget
确实存在。把它们按"能看到什么 / 不能看到什么"理解清楚，配合起来用，
就能像用调试器单步一样把问题范围越收越小。

| 工具 | 能看到什么 | 不能看到什么 / 局限 |
|---|---|---|
| `puts` | 在脚本任意位置打印任意值，是最朴素也最可靠的"埋点"手段，能确认某一行代码到底有没有被执行、执行时某个变量的值是什么 | 需要你提前想好在哪打点；不会主动告诉你哪里有问题 |
| `info commands` | 列出当前解释器里所有已定义的命令（包括普通 `proc`、widget 命令、Tk 内置命令），加通配符如 `info commands .*` 可以筛选出所有 widget | 不会区分一个命令是"正常的 proc"还是"碰巧同名的 widget"；只能看命令存不存在，看不到命令体内容 |
| `info vars` | 列出当前作用域（全局、某个 proc 内部、某个 namespace 内部）里存在哪些变量名 | 必须在正确的作用域里调用才有意义；在 proc 外面调用看不到 proc 内部的局部变量 |
| `info body` | 打印某个 `proc` 的完整源码（方框体内容），用于确认"这个过程到底是不是我以为的那个实现"（排查重名覆盖、动态生成的 proc 等） | 只能看静态定义，看不到运行时的调用栈或局部变量值 |
| `winfo exists .foo` | 判断某个 widget 路径**当前是否还存在**（没被 `destroy`） | 存在不代表可见——没被任何 geometry manager 管理、或者被别的 widget 完全遮挡，仍然算"exists" |
| `winfo children .foo` | 列出 `.foo` 下面**直接**的子 widget 路径 | 不包括孙子辈；看不到这些子控件有没有被实际布局 |
| `winfo parent .foo` | 查出 `.foo` 的直接父 widget 路径，用于确认 widget 树结构是否符合预期 | 不会告诉你这个父子关系在 geometry manager 层面是否也对应正确的布局关系 |
| `winfo class .foo` | 查出 widget 的类名（如 `Button`、`Entry`），可用于确认"这是不是我以为的那种控件" | 不会告诉你它当前的具体配置选项值 |
| `winfo geometry .foo` | 返回 `widthxheight+x+y` 形式的实际像素尺寸和位置，可用于确认控件是不是被分配成了 0 大小 | 只有在窗口已经被窗口系统映射（mapped）之后，这个值才是"最终"准确的；过早查询可能拿到临时值 |
| `pack slaves .foo` | 列出 `.foo` 这个容器下**当前被 `pack` 管理**的子控件列表 | 如果某个 widget 既不在 `pack slaves` 也不在 `grid slaves` 里，说明它根本没有被安排布局——这是"控件创建成功但看不到"最常见的原因 |
| `grid slaves .foo` | 列出 `.foo` 这个容器下**当前被 `grid` 管理**的子控件列表 | 同上；同一个容器下 `pack slaves` 和 `grid slaves` 不应该同时非空（否则就是故障 7 的冲突场景） |
| `after info` | 列出当前所有**待执行**的 `after` 事件 id，配合 `after info $id` 能看到对应的脚本内容，用于排查定时器是否重复累积 | 只能看到还没触发的；已经触发过并执行完的 `after` 事件不会再出现在列表里 |
| `catch` | 把一段可能出错的脚本包起来执行，错误不会中断外层脚本，并把错误信息捕获到变量里，供后续 `puts`/日志记录 | 只能捕获被它包住的那段代码抛出的错误；它本身不会告诉你错误的根因，只是不让程序崩溃 |
| `bgerror` | Tk 事件循环中，`-command`/`bind` 脚本等回调抛出的、没有被 `catch` 捕获的错误，不会让程序崩溃，而是会被自动传给 `bgerror`（默认实现是弹错误对话框，`tclsh` 下默认打印到 stderr）；自定义 `proc bgerror {msg} {...}` 可以改变这个行为（比如统一打印到日志文件） | 只能捕获事件循环分发回调时发生的错误，普通脚本顶层代码里未捕获的错误不会经过 `bgerror`，而是直接导致脚本中止 |

---

## 2. 十五个高频故障

### 故障 1：窗口没有显示

**最小复现示例**

```tcl
package require Tk
wm title . "My Window"
label .msg -text "Hello, should see a window"
pack .msg -padx 20 -pady 20

# 错误：脚本末尾显式调用了 exit。
# wish 本来会在脚本执行完毕后，自动进入事件循环，让窗口持续存活、
# 等待被窗口系统映射到屏幕上；这里主动调用 exit，
# 进程在窗口真正被绘制出来之前就已经退出了。
exit
```

**发生问题的阶段**

阶段 4（事件循环运行期）没有被进入——脚本本身跑完了，甚至没有报错，
但进程自己提前退出，根本没给事件循环运行的机会。

**相关 Widget、变量或回调**

`.`（主窗口）、`.msg`；本例中没有回调参与，纯粹是脚本控制流问题。

**观察手段**

- 在 `exit` 调用之前加一行 `puts "about to exit"`，确认程序确实执行到了这里再主动退出，而不是在别处崩溃。
- `echo $?` 检查进程退出码，配合判断是正常退出还是报错退出。
- 用 `info commands wm` 确认 Tk 是否真的加载成功（如果误用 `tclsh` 运行一个 GUI 脚本，`package require Tk` 这一步会先报错，`wm`/`pack`/`button` 等命令根本不存在，这是另一类更早期的"窗口没有显示"原因）。

**根因**

两类最常见根因：
1. 脚本里有显式的 `exit`（或者某个 `catch` 之外的错误导致脚本提前中止），抢在事件循环开始之前就终止了进程。
2. 用 `tclsh` 而不是 `wish` 运行了一个依赖 Tk 的脚本，导致 `package require Tk` 直接报错退出，连主窗口都没创建出来。

**修复方案**

```tcl
package require Tk
wm title . "My Window"
label .msg -text "Hello, should see a window"
pack .msg -padx 20 -pady 20
# 去掉 exit，让 wish 的隐式事件循环接管；
# 如果用的是 tclsh 并且确实需要进入事件循环，可以显式写 vwait forever。
```

---

### 故障 2：控件创建成功但看不到

**最小复现示例**

```tcl
package require Tk
label .title  -text "Dashboard"
label .status -text "Ready"
pack .title
# 忘记 pack .status
```

**发生问题的阶段**

阶段 3（布局计算期）：widget 本身在阶段 2 已经创建成功，但没有进入阶段 3
的任何一个 geometry manager 管理范围。

**相关 Widget、变量或回调**

`.status`（已创建但未布局的控件）。

**观察手段**

- `winfo exists .status` → 返回 `1`，证明控件确实存在，排除"创建失败"的可能。
- `pack slaves .` 和 `grid slaves .` → `.status` 都不在这两个列表里，这就是铁证：它根本没有被安排布局。
- `winfo children .` → `.status` 会出现在这里（它是 `.` 的子 widget），但这只能证明父子关系，不能证明可见性，容易误判，所以必须配合 `pack slaves`/`grid slaves` 一起看。

**根因**

widget 树（父子关系）和布局管理（geometry manager）是两件独立的事：
`label .status ...` 只完成"创建一个控件对象，并把它挂在 `.` 下面作为子控件"，
从来没有隐含"把它摆到屏幕上的哪个位置"——那是 `pack`/`grid`/`place` 单独负责的事。
写完创建命令、忘记紧跟一行布局命令，是最常见的手滑。

**修复方案**

```tcl
package require Tk
label .title  -text "Dashboard"
label .status -text "Ready"
pack .title
pack .status
```

---

### 故障 3：`-command` 没有执行

**最小复现示例**

```tcl
package require Tk
proc onSave {} { puts "saved!" }
button .saveBtn -text "Save" -command onSave -state disabled
pack .saveBtn
```

**发生问题的阶段**

阶段 5（回调执行期）根本没有被进入——不是回调脚本执行出错，而是这个
widget 压根不接受点击事件，事件在阶段 4 的事件循环里被直接丢弃。

**相关 Widget、变量或回调**

`.saveBtn`，回调过程 `onSave`。

**观察手段**

- `.saveBtn cget -command` → 确认返回的是 `onSave`，排除"绑定内容本身就不对"。
- `.saveBtn cget -state` → 返回 `disabled`，这就是根因所在。
- `info commands onSave` → 确认这个过程确实存在（排除拼写错误绑定了一个不存在的命令名，这种情况其实点击时会在 `bgerror` 里看到 `invalid command name` 错误）。
- 在 `onSave` 内部加一行 `puts "onSave called"`，点击按钮观察终端有没有这行输出，用来和"回调执行了但内部逻辑有 bug"区分开。

**根因**

`-command` 绑定本身完全正确，但 `-state disabled` 让 Tk 在事件分发阶段就
直接忽略针对这个按钮的鼠标事件——这不是 Tcl 层面的错误，不会有任何报错信息，
因为从 Tk 的角度看，"禁用状态的按钮不响应点击"是预期行为，而不是故障。

**修复方案**

```tcl
package require Tk
proc onSave {} { puts "saved!" }
button .saveBtn -text "Save" -command onSave
pack .saveBtn

# 如果确实需要先禁用、再在某个条件满足后启用，
# 用 configure 显式切换 -state，而不是创建时就写死 disabled：
# .saveBtn configure -state normal
```

---

### 故障 4：按钮创建时就提前执行了命令

**最小复现示例**

完整文件：[snippets/04-command-executed-immediately.tcl](snippets/04-command-executed-immediately.tcl)
（已用 `tclsh` 完整校验：纯 Tcl 演示部分可以完整跑通，Tk 部分精确在
`package require Tk` 处报错）。核心对比：

```tcl
proc showMsg {} { puts "called!" ; return "ignored-return-value" }

# 错误：[] 触发命令替换，button 这一行被求值时 showMsg 就被立即调用了一次，
# -command 实际存入的值是 showMsg 的返回值（一个普通字符串），而不是脚本。
button .wrongButton -text "Wrong" -command [showMsg]

# 正确：{} 是脚本字面量，不做任何替换，-command 存入的就是脚本 "showMsg"。
button .rightButton -text "Right" -command {showMsg}
```

**发生问题的阶段**

阶段 2（widget 创建期）。`[showMsg]` 的求值发生在 `button` 这条命令本身
被调用之前——Tcl 对命令参数做替换，和 `button` 命令要不要用这个参数，
是两个独立的步骤，前者必定先发生。

**相关 Widget、变量或回调**

`.wrongButton`，过程 `showMsg`。

**观察手段**

- 在 `showMsg` 内部打印一行带时间戳或调用计数的日志，对照"窗口刚显示出来"和"用户点击按钮"两个时间点，看日志出现在哪个时间点。
- `.wrongButton cget -command` → 返回的是一段固定字符串（`showMsg` 的返回值），而不是 `showMsg` 这个命令名本身，一看便知绑定错了。

**根因**

`-command` 期望接收的是一段**脚本文本**（将来才会被求值），而 `[...]`
是 Tcl 的命令替换语法，会在**当前这条命令被求值的时候**立即执行并替换成
返回值。`-command [showMsg]` 因此变成了："立刻调用一次 `showMsg`，
把它的返回值这个字符串，原封不动地存成按钮点击时要执行的脚本"——
这几乎永远不是想要的效果。

**修复方案**

```tcl
# 两种正确写法二选一：
button .b -command {showMsg}   ;# 推荐：脚本字面量，最直观
button .b -command showMsg     ;# 等价：命令名本身也是合法脚本
```

---

### 故障 5：输入框和 Tcl 变量不同步

**最小复现示例**

完整文件：[snippets/05-entry-variable-out-of-sync.tcl](snippets/05-entry-variable-out-of-sync.tcl)
（已用 `tclsh` 验证纯 Tcl 部分，`::username` 在错误版本里调用后完全没变）。
核心对比：

```tcl
set ::username "alice"

# 错误：proc 内部直接 set username，没有声明 global，
# 这里的 username 是这个 proc 的局部变量，和 Entry 绑定的 ::username
# 是两个完全独立的存储位置，只是恰好同名。
proc resetUsernameWrong {} {
    set username "guest"
}

# 正确：先 global username，再 set，操作的就是 Entry 绑定着的那个全局变量。
proc resetUsernameRight {} {
    global username
    set username "guest"
}
```

**发生问题的阶段**

阶段 5（回调执行期）：回调确实被调用了，`set` 语句也确实执行了，
但写入的是错误的作用域。

**相关 Widget、变量或回调**

`entry .entryUsername -textvariable username`，全局变量 `::username`，
回调过程 `resetUsernameWrong`。

**观察手段**

- 在调用回调前后分别 `puts "::username = $::username"`，对比值到底有没有变。
- `info vars` 在 `resetUsernameWrong` 内部调用（临时加一行 `puts [info vars]`），能看到一个叫 `username` 的局部变量，而不是通过 `global` 关联过来的那个。
- `info body resetUsernameWrong` → 一眼看出过程体里有没有 `global username` 这一行。

**根因**

Tcl 的 `proc` 默认是**独立作用域**：proc 内部直接写变量名，操作的永远是
这个 proc 自己的局部变量表，除非显式用 `global`（或 `variable`、`upvar`）
把某个外层变量"关联"进来。Entry 的 `-textvariable` 绑定的是全局变量
`::username`，如果业务代码在某个 proc 里忘了 `global`，看起来"改了同一个
名字"，实际上是在改一个全新的、和 Entry 毫无关系的局部变量。

**修复方案**

```tcl
proc resetUsernameRight {} {
    global username
    set username "guest"
}
```

---

### 故障 6：回调读取到旧值

**最小复现示例**

完整文件：[snippets/06-callback-stale-value.tcl](snippets/06-callback-stale-value.tcl)
（已用 `tclsh` 验证：错误版本无论"点击"哪个按钮都输出最后一次循环的值，
正确版本每个按钮输出各自创建时的值）。核心对比：

```tcl
foreach i {1 2 3} {
    # 错误：{} 里的 $i 不会立即替换，等到真正点击（求值）时才查一次 $i，
    # 而循环结束后 i 已经停留在最后一个值（3）上，三个按钮点击时读到的
    # 都是同一个"滞留"值。
    button .b$i -text "Button $i" -command {puts "clicked button $i"}
    pack .b$i
}

foreach i {1 2 3} {
    # 正确：用 list 在"创建这一刻"就把当前 $i 的值冻结进命令里，
    # 生成的是 {clickedRight 1}/{clickedRight 2}/{clickedRight 3} 这样
    # 已经写死参数的独立命令。
    button .r$i -text "Button $i" -command [list clickedRight $i]
    pack .r$i
}
```

**发生问题的阶段**

阶段 5（回调执行期）。问题根源其实埋在阶段 2（widget 创建期）：
创建按钮时就该把当前值"冻结"下来，但用了会延迟替换的写法。

**相关 Widget、变量或回调**

`.b1`/`.b2`/`.b3`，循环变量 `i`（Tcl 没有闭包，循环结束后它是一个普通的、
继续存在的变量，并不会随着每次迭代"各自独立"）。

**观察手段**

- 依次 `.b1 cget -command`、`.b2 cget -command`、`.b3 cget -command` → 会发现三个返回值**完全相同**（都是字面量 `puts "clicked button $i"`），这就是问题所在：三个按钮根本没有存下三个不同的脚本。
- `puts $i`（在 `foreach` 循环结束后）→ 确认 `i` 的残留值是多少，对照点击任意按钮得到的输出是不是正好等于这个残留值。

**根因**

`{...}` 花括号脚本是字面量，`$i` 不会在 `button` 命令执行时被替换，而是被
原封不动地存成文本 `$i`；真正的替换发生在**将来事件循环调用这段脚本的那一刻**，
此时查的是**当时**全局作用域里 `i` 的值——而循环早就结束了，`i` 只剩一个
最终残留值，所有按钮共享这同一个"迟到"的替换结果。

**修复方案**

```tcl
proc clickedRight {n} { puts "clicked button $n" }

foreach i {1 2 3} {
    # [list clickedRight $i] 在这一行执行时就把当前 i 的值替换、
    # 冻结进了生成的命令列表里，之后不会再变。
    button .r$i -text "Button $i" -command [list clickedRight $i]
    pack .r$i
}
```

---

### 故障 7：`pack` 和 `grid` 冲突

**最小复现示例**

完整文件：[snippets/07-pack-grid-conflict.tcl](snippets/07-pack-grid-conflict.tcl)
（headless 下已验证到 `package require Tk` 处才报错，之前的语法完全合法；
真实报错需要有显示环境的 `wish` 才会在运行时触发）。核心片段：

```tcl
frame .form
pack .form.lblName -side left
pack .form.entName -side left

# 下面这一行在真实 Tk 环境下会抛出错误并中止脚本：
# "cannot use geometry manager grid inside .form which already has
#  slaves managed by pack"
grid .form.lblPwd -row 0 -column 2
grid .form.entPwd -row 0 -column 3
```

**发生问题的阶段**

阶段 3（布局计算期）：Tk 在这一步会强制校验"同一个父容器下的直接子控件，
必须自始至终只用同一种 geometry manager"，一旦违反就直接报错中止，
不存在"部分生效"。

**相关 Widget、变量或回调**

父容器 `.form`，以及它的四个子控件 `.form.lblName`/`.form.entName`/
`.form.lblPwd`/`.form.entPwd`。

**观察手段**

- `pack slaves .form` 和 `grid slaves .form` 搭配使用：一旦两者都非空，就说明同一个父容器被混用了两种 geometry manager（真实环境中其实走不到这一步，因为第二种会直接报错；这个组合更适合用来"提前自查"——在写布局代码时，习惯性地跑一遍这两条命令，确认某个容器下只有一种非空）。
- `catch {grid .form.lblPwd -row 0 -column 2} err; puts $err` → 捕获具体报错文本，直接告诉你是哪个容器、哪种 geometry manager 冲突。

**根因**

`pack` 和 `grid` 内部对"子控件在父容器里的空间分配"使用完全不同的算法
（约束满足 vs 网格坐标），Tk 不允许同一个父容器下的子控件同时被这两种算法
管理，否则无法定义"最终布局"该听谁的。这个限制是**按父容器**生效的，
不是按整个程序——不同父容器之间可以一个用 `pack`、一个用 `grid`，完全没问题。

**修复方案**

```tcl
frame .formFixed
label .formFixed.lblName -text "Name:"
entry .formFixed.entName
label .formFixed.lblPwd  -text "Password:"
entry .formFixed.entPwd  -show "*"

# 同一个父容器下统一使用 grid：
grid .formFixed.lblName -row 0 -column 0 -sticky e
grid .formFixed.entName -row 0 -column 1 -sticky we
grid .formFixed.lblPwd  -row 1 -column 0 -sticky e
grid .formFixed.entPwd  -row 1 -column 1 -sticky we
grid columnconfigure .formFixed 1 -weight 1
```

---

### 故障 8：GUI 冻结（阻塞调用占用了事件循环）

**最小复现示例**

```tcl
package require Tk
proc onGenerateReportWrong {} {
    # 错误：exec 不带 & 时是同步阻塞的。Tk 的事件循环和应用脚本
    # 跑在同一个线程里，这几秒钟内，事件循环完全无法处理任何
    # 鼠标点击、窗口重绘，甚至无法响应窗口管理器的"关闭"按钮——
    # 整个窗口看起来像是卡死了。
    exec sleep 3
    puts "report generated"
}
button .genBtn -text "Generate Report" -command onGenerateReportWrong
pack .genBtn
```

**发生问题的阶段**

阶段 5（回调执行期）反过来堵死了阶段 4（事件循环运行期）：回调脚本自己
没有把控制权交还给事件循环，事件循环在回调返回之前，没有机会再去处理
下一个操作系统事件。

**相关 Widget、变量或回调**

`.genBtn`，回调过程 `onGenerateReportWrong`，阻塞点是同步的 `exec sleep 3`。

**观察手段**

- 在阻塞调用前后分别 `puts [clock milliseconds]`，点击按钮后观察终端：两行时间戳之间的程序完全没有响应任何其他操作，时间差就是"冻结"的时长。
- `after info` 在冻结期间（如果能从另一个终端对同一进程发信号观察，或者在阻塞前后打点对比）——会发现冻结期间已经注册的 `after` 定时器也没有被触发，因为事件循环本身被卡住了，这能帮助区分"是事件循环被堵死"还是"只是某个定时器逻辑有问题"。

**根因**

Tcl/Tk 默认是单线程事件循环模型：所有的 widget 创建、布局、事件分发、
回调执行都在同一个线程里顺序发生。任何一个回调如果自己不返回
（比如同步 `exec`、死循环、同步阻塞 I/O），就会让这个线程没有机会
再去 `select`/`poll` 操作系统事件，表现出来就是整个 GUI 失去响应。

**修复方案**

```tcl
package require Tk
proc onGenerateReportRight {} {
    .genBtn configure -state disabled -text "Generating..."
    # exec ... & 以管道方式启动外部进程，立即返回，不阻塞事件循环；
    # 用 fileevent 异步等待进程结束/产生输出。
    set ::reportPipe [open "|sleep 3" r]
    fileevent $::reportPipe readable [list onReportDone $::reportPipe]
}
proc onReportDone {pipe} {
    catch {read $pipe}
    close $pipe
    .genBtn configure -state normal -text "Generate Report"
    puts "report generated (async)"
}
button .genBtn -text "Generate Report" -command onGenerateReportRight
pack .genBtn
```

---

### 故障 9：`after` 定时器重复启动

**最小复现示例**

完整文件：[snippets/09-after-timer-restart.tcl](snippets/09-after-timer-restart.tcl)
（纯 `tclsh` + `vwait` 完整可跑通，无需 Tk：错误版本 350ms 内触发了 9 次
tick，正确版本同样 350ms 内只触发 3 次）。核心对比：

```tcl
# 错误：每次调用都无条件新开一个定时器，没有记录、也没有取消旧的。
proc startPollingWrong {} {
    after 100 tickWrong
}

# 正确：用一个变量记录当前 afterId，重启前先取消旧的。
set pollAfterId ""
proc startPollingRight {} {
    global pollAfterId
    if {$pollAfterId ne ""} {
        after cancel $pollAfterId
    }
    set pollAfterId [after 100 tickRight]
}
```

**发生问题的阶段**

阶段 5（回调执行期）里反复调用了"启动定时器"这个动作，每次都在
阶段 4（事件循环）里新增了一份独立的定时任务。

**相关 Widget、变量或回调**

全局变量 `pollAfterId`（正确版本里用来记录当前定时器 id），
回调过程 `tickWrong`/`tickRight`。

**观察手段**

- `after info` → 错误版本连续调用 3 次 `startPollingWrong` 之后，会列出 3 个独立的、都指向 `tickWrong` 的待执行 id；正确版本连续调用 3 次 `startPollingRight` 之后，始终只有 1 个待执行 id。
- 用一个全局计数器在 tick 回调里累加并 `puts`，对比相同时间窗口内触发次数是否符合预期（单个 100ms 定时器在 350ms 内应该触发 3~4 次，而不是成倍增长）。

**根因**

`after` 每调用一次就创建一个全新的、独立的定时任务，彼此互不知晓、
也不会自动替换。如果"启动定时器"这个动作可能被多次触发（比如一个
Start 按钮被连续点击、或者窗口 resize 时意外重复调用了初始化逻辑），
而代码没有记录已存在的 afterId 并在重启前 `after cancel` 它，
定时器数量就会不断累积，回调触发频率随之成倍增长。

**修复方案**

```tcl
set pollAfterId ""

proc tickRight {} {
    global pollAfterId
    # ... 定时逻辑 ...
    set pollAfterId [after 100 tickRight]
}

proc startPollingRight {} {
    global pollAfterId
    if {$pollAfterId ne ""} {
        after cancel $pollAfterId
    }
    set pollAfterId [after 100 tickRight]
}
```

---

### 故障 10：widget 已销毁但回调仍然访问其路径

**最小复现示例**

完整文件：[snippets/10-destroyed-widget-callback.tcl](snippets/10-destroyed-widget-callback.tcl)
（纯 `tclsh` + `after`/`vwait` 完整可跑通：用自定义 `bgerror` 捕获到了
真实的 `invalid command name ".fakeLabel"` 错误）。核心对比：

```tcl
after 200 {.fakeLabel configure -text "updated later"}
after 100 { rename .fakeLabel {} }   ;# 模拟 destroy .fakeLabel

# 200ms 触发时，.fakeLabel 这个命令已经在 100ms 时被删除，
# 于是报错 "invalid command name \".fakeLabel\""，
# 被自定义的 bgerror 捕获打印，而不是让整个程序崩溃。

# 正确写法：延迟脚本执行前先检查 widget 是否还存在。
proc updateLabelLater {} {
    if {[winfo exists .safeLabel]} {
        .safeLabel configure -text "updated later"
    }
}
```

**发生问题的阶段**

阶段 5（回调执行期）访问了一个在阶段 2 创建、但已经在回调真正触发之前
就被销毁的 widget——widget 命令（本质上就是一个普通 Tcl 命令）已经不存在了。

**相关 Widget、变量或回调**

`.fakeLabel`（真实代码里就是任意一个已经被 `destroy` 的 widget），
延迟执行的 `after`/`bind` 脚本。

**观察手段**

- `winfo exists .fakeLabel` → 在延迟脚本真正执行之前这一刻查询，如果返回 `0`，说明 widget 确实已经被销毁，这正是问题所在。
- 自定义 `proc bgerror {msg} { puts stderr "\[bgerror\] $msg" }`，让原本"悄悄弹窗/被吞掉"的回调错误显式打印到终端，第一时间看到 `invalid command name` 这类报错，而不是纳闷"为什么程序好像卡住/没反应了"。
- `after info` → 确认这个引用了已销毁 widget 的 `after` 事件确实还排在队列里等待触发。

**根因**

`destroy` 一个 widget，本质上是把这个 widget 对应的同名 Tcl 命令从解释器
里删除（类似 `rename .foo {}`）。如果之前已经用 `after`/`bind` 注册了一段
"将来才执行"的脚本，而这段脚本的生存期比 widget 本身还长（widget 先被销毁，
脚本后触发），脚本执行到引用这个 widget 路径的那一行时，Tcl 会报告
"invalid command name"——这和"找不到变量""找不到文件"是同一类错误，
只是对象换成了命令。

**修复方案**

```tcl
# 方案一：延迟脚本执行前先用 winfo exists 守卫
proc updateLabelLater {} {
    if {[winfo exists .safeLabel]} {
        .safeLabel configure -text "updated later"
    }
}
after 200 updateLabelLater

# 方案二：销毁 widget 的同时，主动取消还没触发的 after
set labelAfterId [after 200 updateLabelLater]
# ... 当需要提前销毁 .safeLabel 时：
after cancel $labelAfterId
destroy .safeLabel
```

---

### 故障 11：`source` 加载模块时发生路径错误

**最小复现示例**

```tcl
# main.tcl，目录结构：
#   project/main.tcl
#   project/lib/helper.tcl

# 错误：相对路径依赖"运行 wish/tclsh 时的当前工作目录"，
# 而不是 main.tcl 这个脚本文件本身所在的目录。
source lib/helper.tcl
helperGreet
```

在 `project` 目录里直接 `tclsh main.tcl` 能正常工作（因为当前工作目录
正好是 `project`）；但只要换一个工作目录运行，比如
`tclsh project/main.tcl`，就会报错（已用 `tclsh` 实际验证两种情况）：

```text
couldn't read file "lib/helper.tcl": no such file or directory
```

**发生问题的阶段**

阶段 1（脚本解析期）：`source` 是在脚本求值过程中被调用的，这个故障
和 Tk 完全无关，纯 Tcl 脚本一样会中招。

**相关 Widget、变量或回调**

无特定 widget；涉及的是 `source` 命令本身和 `project/lib/helper.tcl` 这个依赖文件。

**观察手段**

- `puts [pwd]` → 打印当前工作目录，确认它是不是你以为的那个目录。
- `puts [info script]` → 打印当前正在执行的脚本文件路径（通常等于你传给 `wish`/`tclsh` 的那个路径参数）。
- `catch {source lib/helper.tcl} err; puts $err` → 捕获具体的"couldn't read file"错误信息，而不是让整个程序直接中止、不方便定位。

**根因**

`source` 后面跟的相对路径，是相对于**进程的当前工作目录**（谁在哪个目录
下敲的 `wish`/`tclsh` 命令），而不是相对于脚本文件自身所在的目录。
只要调用者从别的目录启动这个脚本（这在实际使用中非常常见，比如写了一个
快捷方式、或者从别的脚本里间接调用），相对路径就会失效。

**修复方案**

```tcl
# 用 info script 拿到当前脚本自身的路径，file normalize 转成绝对路径，
# file dirname 取出它所在的目录，这样无论从哪个工作目录启动，
# 拼出来的 helper.tcl 路径永远正确。
set scriptDir [file dirname [file normalize [info script]]]
source [file join $scriptDir lib helper.tcl]
helperGreet
```

---

### 故障 12：命名空间中的变量无法访问

**最小复现示例**

```tcl
namespace eval ::counter {
    variable total 0
}

proc ::counter::readTotal {} {
    # 忘记写 variable total：proc 既不会自动看到同名的
    # 命名空间变量，也不会自动看到同名的全局变量。
    puts "total = $total"
}

catch {::counter::readTotal} err
puts "错误信息：$err"
```

实测报错（已用 `tclsh` 验证）：

```text
错误信息：can't read "total": no such variable
```

**发生问题的阶段**

阶段 5（回调执行期，如果这个 proc 是某个按钮的 `-command`）或任意
proc 被调用的时刻：作用域查找在这一刻发生并失败。

**相关 Widget、变量或回调**

命名空间 `::counter`，变量 `::counter::total`，过程 `::counter::readTotal`。

**观察手段**

- `catch {...} err` 包住调用，`puts $err` 直接看到 `can't read "total": no such variable` 这条明确的错误信息。
- 在 proc 内部临时加一行 `puts [info vars]`，能看到当前作用域里**没有** `total` 这个名字，证明它既不是局部变量也没有被 `variable` 关联进来。
- `namespace eval ::counter {info vars}` → 列出 `::counter` 这个命名空间里实际存在哪些变量，确认 `total` 确实是以命名空间变量的形式存在的，只是没有被 proc 正确引用到。

**观察手段的补充（更隐蔽的变体）**

如果 proc 里写的不是 `puts $total` 而是 `incr total`（写操作），
Tcl **不会报错**——`incr` 会在 proc 的局部作用域里悄悄创建一个全新的
局部变量 `total` 并从 0 开始计数，`::counter::total` 完全不受影响。
这比直接报错更危险，因为表面上"没有任何错误"，但命名空间里的那个变量
永远不会被真正更新。排查这种情况，`info body` 配合在外部调用前后对比
`set ::counter::total` 的值是否真的变化，是最直接的手段。

**根因**

`namespace eval ::counter { variable total 0 }` 只是在命名空间内部创建了
这个变量，并不会让所有定义在 `::counter::` 下面的 `proc` 自动"看见"它——
和全局变量需要 `global` 声明是完全一样的道理，命名空间变量需要在 proc
内部用 `variable total` 显式声明之后，才能在这个 proc 里直接以 `$total`
的形式读写。

**修复方案**

```tcl
namespace eval ::counter {
    variable total 0
}

proc ::counter::readTotal {} {
    variable total
    puts "total = $total"
}

proc ::counter::increment {} {
    variable total
    incr total
}
```

---

### 故障 13：`bind` 回调中的 `%` 替换与 Tcl 变量替换发生混淆

**最小复现示例**

```tcl
package require Tk
canvas .c -width 200 -height 200
pack .c
set x 0
set y 0

# 错误：想要获取鼠标点击位置，却写成了 $x $y——
# 这其实是在引用脚本里恰好同名的两个普通 Tcl 变量（值恒为 0），
# 和鼠标点击在哪里完全没有关系。
bind .c <Button-1> {puts "clicked at $x,$y"}
```

（headless 下已验证该片段在 `package require Tk` 之前语法完全合法；
`bind` 本身是 Tk 命令，真实的事件触发效果需要在有显示环境的 `wish` 下，
用鼠标实际点击画布才能观察到。）

**发生问题的阶段**

阶段 5（回调执行期）。更准确地说，是两次"替换"被搞混了各自的发生时机：
Tk 对绑定脚本文本做的 `%` 替换，和 Tcl 解释器对 `$` 变量的替换，
是顺序发生的两件独立的事。

**相关 Widget、变量或回调**

`.c`（canvas），`<Button-1>` 绑定脚本，容易被搞混的全局变量 `x`/`y`。

**观察手段**

- `bind .c <Button-1>` （不带第三个参数地调用，查询当前绑定内容）→ 能看到存储的脚本文本本身是 `puts "clicked at $x,$y"` 还是 `puts "clicked at %x,%y"`，这是最直接的"文本层面"核对手段。
- 在绑定脚本里同时打印两种值做对比：`bind .c <Button-1> {puts "x-var=$x y-var=$y vs percent=%x,%y"}`，点击后观察两组数字是否一致，能非常直观地看出 `$x`/`$y` 和 `%x`/`%y` 完全是两回事。

**根因**

Tk 的 `bind` 脚本支持两层替换机制，顺序是：
1. **`%` 替换**：在事件真正发生、Tk 准备调用绑定脚本之前，Tk 会先对脚本的
   **原始文本**做一次字符串级别的替换，把 `%x`、`%y`、`%W` 等占位符替换成
   这次事件的实际坐标、widget 路径等信息。这一步和 Tcl 变量系统完全无关，
   纯粹是文本替换，替换完才交给 Tcl 解释器。
2. **`$` 替换**：第 1 步替换完成之后，得到的文本才作为一段正常的 Tcl 脚本，
   由 Tcl 解释器按常规方式求值，这时候 `$x`/`$y` 才会按 Tcl 变量查找规则，
   查找当前作用域里名叫 `x`/`y` 的 Tcl 变量——如果这两个变量刚好存在
   （哪怕值和鼠标位置毫无关系），也不会报错，只会悄悄给出错误的数值。

如果 `%` 后面跟的字符不是 Tk 定义的合法替换符（例如笔误成 `%q`），
Tk 会在**事件真正触发、准备替换的那一刻**报错（类似 "bad field specifier"），
而不是在 `bind` 这一行注册绑定的时候就报错——这也是初学者常困惑的点：
`bind` 命令本身对脚本内容是"来者不拒"的，校验被推迟到了真正触发时。

**修复方案**

```tcl
# 正确：用 Tk 的 %x %y 拿到事件发生时的坐标。
bind .c <Button-1> {puts "clicked at %x,%y"}

# 如果既要用事件坐标，又要用脚本里的 Tcl 变量，两者可以共存，
# 只要清楚各自的替换机制：
set label "click"
bind .c <Button-1> {puts "$label at %x,%y"}
```

---

### 故障 14：使用 `update` 导致意外的重入

**最小复现示例**

```tcl
set depth 0
proc onClickWrong {} {
    global depth
    incr depth
    puts "onClickWrong 进入，重入深度 = $depth"

    if {$depth == 1} {
        # 排一个"马上要处理"的事件，模拟用户在第一次点击还没处理完时
        # 又点了一次按钮（典型场景：按钮点击后触发网络请求，
        # 为了让界面及时重绘，在等待结果的过程中调用了 update）。
        after 0 onClickWrong
        puts "调用 update，处理排队中的事件……"
        update
        puts "update 返回，但内部已经重入执行过一次 onClickWrong"
    }

    incr depth -1
    puts "onClickWrong 退出，重入深度回落到 = $depth"
}
onClickWrong
```

（已用 `tclsh` 完整验证，输出显示最大重入深度达到 2，证明确实发生了
"`onClickWrong` 还没执行完，自己又被调用了一次"。`update` 是 Tcl 核心
命令而不是 Tk 专属，所以这个现象可以完全脱离 Tk 复现。）

**发生问题的阶段**

阶段 5（回调执行期）内部意外地重新进入了阶段 4（事件循环运行期），
又从阶段 4 里重新分发出了阶段 5——形成了回调调用自身的递归链条。

**相关 Widget、变量或回调**

回调过程 `onClickWrong`，全局重入深度计数器 `depth`。

**观察手段**

- 用一个全局计数器在回调入口 `incr`、出口 `incr -1`，并在每次变化时 `puts`，观察深度是否超过 1——正常情况下一个回调执行期间不应该看到自己的深度计数超过 1。
- `info level` → 在回调内部调用，能看到当前 Tcl 调用栈的层数，如果在同一个逻辑"点击"过程中调用栈层数出现了不止一层 `onClickWrong`，说明确实重入了。
- `after info` → 检查是否有本应该"稍后处理"的事件，结果在 `update` 调用期间被提前处理掉了。

**根因**

`update`（以及 `update idletasks`）的作用是**立即处理一遍当前事件队列
里所有待处理的事件和空闲任务**，而不是单纯"刷新一下界面"。如果队列里
恰好排着一个会再次触发同一个回调的事件（常见于：回调本身用 `after 0`
排了后续步骤、或者用户在回调执行期间又快速点击了同一个按钮，点击事件
被操作系统/Tk 缓冲了下来），调用 `update` 就会让这个事件被提前处理，
相当于在原来的回调还没返回时，又同步地调用了一次自己——如果回调内部
有状态假设"同一时间只会有一个自己在跑"，这种重入会直接破坏该假设。

**修复方案**

```tcl
set busy 0
proc onClickRight {} {
    global busy
    if {$busy} {
        # 用一个忙碌标志位短路掉重入的调用，而不是依赖"调用栈深度"
        # 这种隐式约束。
        return
    }
    set busy 1
    # 确实需要刷新界面时，优先用 update idletasks（只处理重绘相关的
    # 空闲任务，不处理普通事件如按钮点击），而不是无差别的 update。
    update idletasks
    # ... 业务逻辑 ...
    set busy 0
}
```

---

### 故障 15：文件对话框取消后程序处理错误

**最小复现示例**

```tcl
package require Tk
proc onOpenWrong {} {
    set path [tk_getOpenFile]
    # 错误：没有检查 path 是否为空字符串。
    # 用户点击对话框里的"取消"按钮时，tk_getOpenFile 返回空字符串 ""，
    # 而不是报错，后续代码却把它当成一个合法路径直接使用。
    set f [open $path r]
    set data [read $f]
    close $f
    puts $data
}
button .openBtn -text "Open..." -command onOpenWrong
pack .openBtn
```

（headless 下已验证该片段在 `package require Tk` 之前语法完全合法；
`tk_getOpenFile` 本身需要真实的显示环境才能弹出对话框，下面用一个假的
替身函数在纯 `tclsh` 下复现"空字符串被当成路径使用"这部分逻辑错误，
已验证能准确重现报错）：

```tcl
proc fakeGetOpenFileCancelled {} { return "" }
proc loadConfigWrong {} {
    set path [fakeGetOpenFileCancelled]
    set f [open $path r]
    set data [read $f]
    close $f
    return $data
}
catch {loadConfigWrong} err
puts "错误信息：$err"   ;# 实测输出：couldn't open "": no such file or directory
```

**发生问题的阶段**

阶段 5（回调执行期）：对话框本身的交互（弹出、选择/取消、关闭）已经
正常完成，问题出在拿到返回值之后、回调剩余逻辑的处理上。

**相关 Widget、变量或回调**

`.openBtn`，回调过程 `onOpenWrong`，`tk_getOpenFile` 的返回值 `path`。

**观察手段**

- 在 `set path [tk_getOpenFile]` 之后立即 `puts "path=\"$path\""`，点击"取消"观察到的是一个空字符串，而不是某个异常值——这能帮助快速定位到底是对话框返回异常，还是后续逻辑没处理空值。
- `catch {open $path r} err; puts $err` → 捕获到明确的 `couldn't open "": no such file or directory` 错误信息，直接指向空路径这个根因。

**根因**

`tk_getOpenFile`（以及 `tk_getSaveFile`、`tk_chooseDirectory` 等同类对话框
命令）在用户点击"取消"或直接关闭对话框窗口时，**不会报错**，而是返回一个
空字符串 `""`，把"用户取消了"这个信息编码成了一个特殊的返回值，而不是
一个异常。如果调用方只处理了"正常选中了一个文件"这一种情况，没有专门
检查空字符串，空字符串就会被当作一个合法路径传给 `open`，报出和"空路径"
这个根因看起来毫不相关的底层文件系统错误。

**修复方案**

```tcl
package require Tk
proc onOpenRight {} {
    set path [tk_getOpenFile]
    if {$path eq ""} {
        # 用户取消了选择，直接返回，不做任何文件操作。
        puts "用户取消了文件选择"
        return
    }
    set f [open $path r]
    set data [read $f]
    close $f
    puts $data
}
button .openBtn -text "Open..." -command onOpenRight
pack .openBtn
```

---

## 3. 小结：把"调试"变成一个确定性流程

1. 先问阶段：脚本解析期 / widget 创建期 / 布局计算期 / 事件循环运行期 / 回调执行期，问题八成只可能出在其中一个。
2. 再选工具：`winfo`/`pack slaves`/`grid slaves` 查布局，`info commands`/`info vars`/`info body` 查命令与变量的真实状态，`after info` 查定时器，`catch`+自定义 `bgerror` 把"静默失败"变成"看得见的错误"。
3. 用 `puts` 在"怀疑区间"的两端打点，把问题范围从整个程序，收缩到某一行代码、某一次回调调用。
4. 确认根因之后再改代码——15 个故障里，绝大多数修复方案只有一两行改动，难的从来不是"怎么改"，而是"改哪里"。
