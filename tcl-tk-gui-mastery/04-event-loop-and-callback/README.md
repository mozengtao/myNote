# Layer 4：事件驱动与 Callback

> 核心问题：**为什么 GUI 程序通常不是按照从上到下的顺序完成所有工作？**

本层是整个教程的核心之一（另一个核心是 Layer 3 的变量绑定）。读完本层之后，你应该能够：

- 清楚区分 `-command`、`bind`、`after` 这三种"注册回调"的机制，知道各自的适用范围。
- 准确描述 `wish` 启动后，一个 Tcl/Tk 程序的控制权是怎么从"你的脚本"转移到"事件循环"的。
- 识别并避免三个最常见的事故：`-command [cmd]` 提前执行、`update` 导致的重入、阻塞循环冻住界面。

---

## 1. Why

先看一个几乎所有初学者都会下意识假设、但是错误的心智模型：

> "GUI 程序和我写的其他脚本一样，从第一行代码执行到最后一行，执行完就结束。"

这个假设对**纯 Tcl 脚本**（`tclsh` 跑的那种，没有 `package require Tk`）基本是对的：脚本从上到下执行一次，遇到 `exit` 或者执行到文件末尾就退出。

但是对 **Tk GUI 程序**（`wish` 跑的那种）完全不是这样。原因在于：

1. 一个 Tk 程序的"主线"其实很短，只做一件事：**初始化**——创建控件、配置布局、注册回调（`-command`、`bind`、`after`）。初始化代码确实是严格按照源码顺序、从上到下执行**一次**。
2. 初始化做完之后（脚本文件执行到末尾），`wish` 会自动把控制权交给**事件循环（Event Loop）**。这一步在源码里完全看不到对应的一行代码——`wish` 是隐式帮你做的（`tclsh` 不会，这是两者最大的区别之一，详见 [00-architecture](../00-architecture/README.md)）。
3. 从这一刻起，程序真正的"业务逻辑"——也就是你写的那些 `proc`——不再由源码顺序驱动，而是由**事件到达的顺序**驱动：用户先点了哪个按钮、哪个定时器先到期、用户先按了哪个键，这些完全取决于运行时的外部世界，而不是你写代码时的先后顺序。
4. 同一个回调（比如 `greet`）在程序的生命周期里可能被调用 0 次、1 次，也可能被调用 1000 次——调用次数和时机都不是源码能静态决定的。

所以更准确的心智模型是：

> **GUI 程序的源码只定义"初始化一次"和"将来可能发生的各种反应"，真正决定执行顺序的是运行时按时间到达的事件流，而不是源码的书写顺序。**

这正是 Layer 4 要讲透的内容：事件从哪里来、怎么被翻译成对你代码的调用、调用之间的时序关系、以及由此产生的一整类新手容易踩的坑。

---

## 2. Mental Model

一句话 Mental Model：

> **初始化阶段只执行一次、严格按源码顺序；初始化完成后，程序变成一堆"等待被触发的函数"，事件循环按事件到达的先后顺序逐个调用它们，调用之间互不重叠。**

用图来对比"你以为的执行顺序"和"实际发生的执行顺序"：

```text
+-------------------------------------------------+
| Source Code Order (as written in the .tcl file) |
|                                                 |
| 1. package require Tk                           |
| 2. proc greet {}        { ... }                 |
| 3. proc onKeyPress {}   { ... }                 |
| 4. proc tick {}         { ... }                 |
| 5. button .b1 -command greet                    |
| 6. bind . <KeyPress> {...}                      |
| 7. after 1000 tick                              |
|    (script reaches its last line and "ends")    |
+-------------------------------------------------+
```

源码里的第 1~7 行只会被执行一次，而且严格按这个顺序。但是脚本"结束"之后，`wish` 并没有退出，而是进入了事件循环，实际发生的事情是这样的：

```text
+-------------------------------------------------------+
| Actual Execution Order (decided by arrival of events) |
|                                                       |
| T=0.0s  Event Loop takes over (wish auto-enters it)   |
| T=0.3s  User clicks .b1       -> calls greet          |
| T=1.0s  Timer fires           -> calls tick           |
| T=1.4s  User presses a key    -> calls onKeyPress     |
| T=2.0s  Timer fires again     -> calls tick           |
| T=3.1s  User clicks .b1 again -> calls greet          |
| T=...   (continues until the window is destroyed)     |
+-------------------------------------------------------+
```

注意：`greet`、`tick`、`onKeyPress` 在源码里的定义顺序分别是第 2、4、3 行，但实际被调用的顺序是 `greet -> tick -> onKeyPress -> tick -> greet -> ...`——和源码顺序完全无关，只和"用户/定时器/窗口系统在什么时刻产生了什么事件"有关。

这就引出三个必须严格区分的"注册回调"机制，以及承载它们的那个事件循环。下一节逐一拆解。

---

## 3. Architecture

### 3.1 四个必须分清楚的概念

本层经常被初学者混为一谈的四个概念，先给出各自的精确定义：

**(1) Widget Command Callback（例如 `-command`）**

- 只存在于**某些特定 widget 的特定选项**里，比如 `button -command`、`scale -command`、`checkbutton -command`。
- 这个选项的值只是**一段被存起来的 Tcl 脚本字符串**，widget 自己内部知道"什么时候"该去执行它——比如 `button` 知道"鼠标在控件上按下又释放"时该执行 `-command`。
- 你不能用 `-command` 去监听"鼠标移动"这种 widget 没有预先定义好的动作；`-command` 的触发时机是 widget 的实现写死的，你只能提供"触发时该执行什么"，不能改变"什么时候触发"。

**(2) Event Binding（`bind`）**

- 更底层、更通用：可以把任意一类 **X 事件**（键盘按下/释放、鼠标按下/释放/移动、窗口获得/失去焦点、窗口尺寸变化……）绑定到：
  - 一个具体的 widget 路径（比如 `.pad`），
  - 一个 widget 类（比如所有 `Button`），
  - 或者 `all`（所有 widget）。
- `bind` 不关心这个 widget 本身是否"原生支持"这个事件——任何 widget 都能响应键盘/鼠标事件，因为这些事件来自窗口系统，不是 widget 自己定义的概念。
- 一次事件可能同时匹配多条 binding（实例级、类级、all 级），Tk 有明确的优先级和执行顺序（`bindtags` 命令可以查看/调整，本层不展开，留给后续章节）。

**(3) Timer Callback（`after ms script`）**

- 基于**时间**触发，不基于用户操作或窗口事件。
- **一次性**：`after 1000 script` 只会在大约 1000ms 后触发一次，之后这个定时器就结束了，不会自动重复。
- 想要周期性效果（比如时钟每秒跳一次），必须在 `script` 执行的最后一步，**显式地重新调用一次 `after`**，这叫"递归调度"，示例 C 会完整演示。

**(4) Event Loop（事件循环）**

- `wish` 在脚本执行完毕后自动进入的一个循环，不断地：
  1. 向操作系统/窗口系统询问"有没有新事件"（键盘、鼠标、窗口重绘请求……），同时检查 `after` 定时器队列里有没有到期的任务；
  2. 如果有，取出**一个**事件或到期的定时器；
  3. 把这个事件翻译成一次 Tcl 命令调用（查 `-command` 的值，或者查 `bind` 表，或者直接是 `after` 存的脚本），用 `uplevel #0`（即在全局作用域）执行这个命令；
  4. 等这次命令执行**完全返回**之后，才回到第 1 步,继续处理下一个事件。
- 关键性质：**单线程、串行**。永远不会有两个回调同时在执行，一个回调必须完整执行完（哪怕耗时很久），事件循环才能去处理下一个事件——这既是 Tk 编程模型简单的原因（不用操心锁和竞态），也是后面"Common Failure"一节要讲的"阻塞回调会冻住界面"的根本原因。

下图把这四者和它们的调用关系画在一起：

```text
+---------------------------------------------------------------+
| Event Sources                                                 |
|                                                               |
| Keyboard / Mouse        (X11 events)                          |
| OS Timer                (after queue, checked each loop pass) |
| Window Manager          (resize / close / expose-redraw)      |
+---------------------------------------------------------------+
                                |
                                v
+-----------------------------------------------------+
| Tk Event Dispatcher (Notifier)                      |
|                                                     |
| multiplexes every source via select()/poll()        |
| turns ONE raw event into ONE Tcl command invocation |
| single-threaded: never runs two callbacks at once   |
+-----------------------------------------------------+
                           |
                           v
+------------------------------------------------------------------+
| Callback Registries  (dispatcher looks up what to call)          |
|                                                                  |
| Widget option   -command         (button, scale, checkbutton...) |
| Bind table       bind <target> <Event> script  (target = widget  |
|                  path | widget class | "all")                    |
| After queue      after ms script  (one-shot, must re-arm itself) |
+------------------------------------------------------------------+
                                  |
                                  v
+----------------------------------------------------------------+
| Tcl Interpreter (single call stack)                            |
|                                                                |
| uplevel #0 <callback script>                                   |
| callback runs to completion, synchronously, top to bottom      |
| dispatcher only looks at the next event AFTER this one returns |
+----------------------------------------------------------------+
```

### 3.2 Tcl 命令执行 与 事件分发 的关系

这是容易被混淆的一点，必须讲清楚：

- **命令执行**是同步的调用栈概念：一条 Tcl 命令调用另一条命令，层层嵌套，最终总会返回，这和任何编程语言的函数调用没有区别。
- **事件分发**是异步到达、但同步处理的：事件"什么时候产生"（用户什么时候点鼠标）是不可预测、异步的；但事件循环"如何处理"这些事件是严格串行的——每次只取一个事件，翻译成一次命令调用，等这次调用的整个调用栈（哪怕里面套了十层 proc）完全返回之后，才去看下一个事件。
- 换句话说：**事件的到达是异步的，但事件的处理永远是同步的、一个接一个的**。不存在"两个回调同时在跑"这种情况（除非你手动引入了嵌套事件循环，见 7.1 节）。

---

## 4. Complete Example

本层提供三个独立的、可运行的完整示例文件，分别覆盖"Widget Command Callback"、"Event Binding"、"Timer Callback"。下面逐一内联展示并讲解。

### 示例 A：`button_callback.tcl` —— Widget Command Callback

```tcl
#!/usr/bin/env wish
#
# button_callback.tcl —— Layer 4 示例 A：Widget Command Callback（-command）
#
# 本例要回答一个新手最容易搞错的问题：
#   -command greet / -command {greet} / -command [greet]
#   这三种写法里，哪些是"正确地延迟执行回调"，哪些会"立刻执行"？
#
# 运行方式：
#   有图形环境：   wish button_callback.tcl
#   headless 环境： tclsh button_callback.tcl  （只能校验到 package require Tk 这一步）
#
# 运行后观察终端输出的顺序，配合 README.md 的"执行顺序图"一起看效果最好。

package require Tk

# --------------------------------------------------------------------------
# 回调函数本体：greet
# --------------------------------------------------------------------------
# greet 只是一个普普通通的 proc，它本身并不知道自己会被当成"回调"使用。
# "回调"这个身份，是由"谁在什么时候调用它"决定的，不是由定义方式决定的。
proc greet {} {
    puts "Hello from callback"
    # 注意：puts 的返回值永远是空字符串 ""
    # 这一点在下面解释 -command [greet] 的坑时非常关键。
}

# --------------------------------------------------------------------------
# 写法 1：-command greet（裸字——正确、最常见的写法）
# --------------------------------------------------------------------------
# button 命令在被求值时，"greet" 只是一个普通字符串词（bare word），
# 没有任何 $ 变量替换、没有任何 [ ] 命令替换需要处理，
# 所以它会被原样当作 -command 选项的值存起来。
# 这一行执行完，greet 这个 proc 完全没有被调用。
puts {===== creating .b1  (-command greet) ====}
button .b1 -text "Click 1: -command greet" -command greet
puts {===== .b1 created; greet was NOT called =====}

# --------------------------------------------------------------------------
# 写法 2：-command {greet}（花括号分组——结果和写法 1 完全一样）
# --------------------------------------------------------------------------
# {greet} 只是用花括号把 "greet" 这个词组合起来，
# 花括号在这里唯一的作用是"分词/分组"，并不会触发任何额外的替换，
# 花括号内部的内容本来就不会被 Tcl 做变量替换或命令替换。
# 所以 {greet} 求值出来的词，和裸字 greet 求值出来的词，是同一个字符串 "greet"。
puts {===== creating .b2  (-command {greet}) ====}
button .b2 -text "Click 2: -command {greet}" -command {greet}
puts {===== .b2 created; greet was NOT called =====}

# --------------------------------------------------------------------------
# 写法 3：-command [greet]（命令替换——经典新手陷阱，会立刻执行）
# --------------------------------------------------------------------------
# [ ] 是命令替换：Tcl 在对 button 这条命令本身的参数做求值的阶段，
# 就会先执行方括号里的 greet，并把它的*返回值*替换到这个位置，
# 然后才用替换后的结果去调用 button。
# greet 的返回值是空字符串 ""（因为 puts 返回 ""），
# 所以这一行等价于：button .b3 -text ... -command ""
# 也就是说：
#   1. greet 在"创建按钮"这一步就已经执行完了（副作用：打印了一行字）。
#   2. .b3 真正存下来的 -command 值是空字符串，点击 .b3 什么都不会发生。
puts {===== creating .b3  (-command [greet]) -- WATCH CAREFULLY ====}
button .b3 -text "Click 3: -command \[greet\] (buggy)" -command [greet]
puts {===== .b3 created; notice greet ALREADY ran above, before any click! =====}

# --------------------------------------------------------------------------
# 用 cget 把三个按钮真正存下来的 -command 值打印出来，验证上面的分析
# --------------------------------------------------------------------------
puts "---- actual stored -command values ----"
puts [format {.b1 -command = "%s"} [.b1 cget -command]]
puts [format {.b2 -command = "%s"} [.b2 cget -command]]
puts [format {.b3 -command = "%s"  (empty! greet's return value, not a callback)} \
    [.b3 cget -command]]

# --------------------------------------------------------------------------
# 布局
# --------------------------------------------------------------------------
label .hint -justify left -text \
    "点击上面三个按钮，对比终端输出：\n- 按钮 1、2 点击后才会打印 Hello\n- 按钮 3 点击后什么都不会发生（因为 -command 是空字符串）"
pack .hint -padx 10 -pady {10 6} -anchor w
pack .b1 .b2 .b3 -padx 20 -pady 4 -anchor w

button .quit -text "Quit" -command {destroy .}
pack .quit -pady 10
```

**三种写法的本质区别，用执行顺序说明：**

```text
Case 1 & 2: -command greet   /   -command {greet}
(裸字 和 花括号分组后的词，求值结果是完全相同的字符串 "greet")

  step 1: Tcl 对 "button" 这条命令本身的参数做求值
          -> 词 "greet" 或 "{greet}" 都不需要任何替换
          -> 求值结果 = 字符串 "greet"      （只是一个字符串，不是一次调用！）
  step 2: button 把 -command 存成 "greet"
  step 3: button .b1 这条命令返回；greet 这个 proc 还没被调用过
  ...（这里可能过了 0.3 秒，也可能过了 10 分钟，取决于用户什么时候点按钮）
  step N:   用户点击 .b1
  step N+1: Tk 执行:  uplevel #0 greet
  step N+2: greet 现在才真正执行 -> 打印 "Hello from callback"


Case 3: -command [greet]
(方括号触发"命令替换"，替换发生在 button 这条命令自己被求值的时候)

  step 1: Tcl 对 "button" 这条命令本身的参数做求值
          -> 词 "[greet]" 需要先做命令替换
          -> Tcl 立刻、同步地调用 greet()
          -> greet 打印 "Hello from callback"   （副作用现在就发生了！）
          -> greet 返回 ""                        （puts 的返回值永远是空串）
          -> 求值结果 = 空字符串 ""
  step 2: button 把 -command 存成 ""   (空字符串，不是 "greet" 这个名字！)
  step 3: button .b3 这条命令返回；greet 已经被调用过一次了
  ...
  step N:   用户点击 .b3
  step N+1: Tk 执行:  uplevel #0 ""
  step N+2: 什么都不会发生（空脚本执行结果就是什么都不做）
```

一句话总结这个坑：**`[...]` 永远是"现在、立刻执行一次，并把返回值替换进来"，而不是"把这个动作包装起来留到以后执行"**。只要你想要的是"以后再执行"，就不能用 `[...]`，裸字或者 `{...}` 才是对的——当然前提是这个词本身不包含需要求值的内容（比如不含 `$变量`）。如果确实需要带参数的延迟调用，正确做法是 `-command [list greet $arg]`：`list` 本身立刻执行，但它只是把 `greet` 和 `$arg` 的*当前值*安全地拼成一个新的字符串，并不会调用 `greet`，这和 `[greet]`（调用 `greet` 本身）是两件完全不同的事——这个区别留作 Knowledge Check 的思考题。

### 示例 B：`key_binding.tcl` —— Event Binding 与事件替代符号

```tcl
#!/usr/bin/env wish
#
# key_binding.tcl —— Layer 4 示例 B：Event Binding（bind）与事件替代符号
#
# bind 和 -command 是两种不同的回调登记机制（详见 README "Architecture" 一节）：
#   -command 只能用在"控件自己定义好的、少数几个特定动作"上（比如按钮被点击）；
#   bind 可以把任意一类 X 事件（键盘、鼠标、窗口变化……）绑定到任意 widget
#   路径、任意 widget 类、或者 "all" 这种全局标签上，更通用、更底层。
#
# 本例演示：
#   1. 键盘事件绑定在主窗口 "." 这个具体 widget 实例上。
#   2. 鼠标事件绑定在一个具体的 frame 控件 .pad 上。
#   3. 四个常用事件替代符号：%W  %x  %y  %K  %A
#
# 运行方式：
#   有图形环境：   wish key_binding.tcl
#   headless 环境： tclsh key_binding.tcl  （只能校验到 package require Tk 这一步）

package require Tk

wm title    . "Event Binding Demo"
wm geometry . 380x280

label .hint -justify left -text \
    "把焦点留在这个窗口上，按任意键观察下方蓝色文字；\n在下面灰色方块内点击鼠标观察绿色文字。"
pack .hint -padx 10 -pady {10 6} -anchor w

# textvariable 把 Tcl 变量和 label 的显示内容绑定在一起（Layer 3 讲过的机制，这里复用）
set ::lastKeyInfo   "(no key event yet)"
set ::lastMouseInfo "(no mouse event yet)"

label .keyinfo   -textvariable ::lastKeyInfo   -foreground blue      -anchor w
label .mouseinfo -textvariable ::lastMouseInfo -foreground darkgreen -anchor w
pack .keyinfo   -fill x -padx 10 -pady 2
pack .mouseinfo -fill x -padx 10 -pady 2

# 一个专门用来接收鼠标点击的方块，大小固定，方便直观估算坐标
frame .pad -width 240 -height 100 -background #cccccc -relief sunken -borderwidth 2
pack .pad -padx 10 -pady 10
# frame 默认会把自己"收缩"到刚好包住子控件；因为 .pad 里没有任何子控件，
# 如果不加这一行，.pad 的实际尺寸会被压缩成几乎看不见的小方块。
# pack propagate <container> 0 的意思是：
# "不要根据子控件的需求反过来改变我（容器）自己的 -width/-height"。
pack propagate .pad 0

# --------------------------------------------------------------------------
# 把事件处理逻辑放进具名 proc，而不是直接把业务逻辑写进 bind 的脚本参数里
# --------------------------------------------------------------------------
# 这是工程实践上的推荐做法（见 README "Engineering Practice"）：
# bind 脚本只负责"把事件替代符号转发给 proc 当参数"，
# 真正的逻辑都在 proc 里，方便单独测试、阅读、复用。

proc onKeyPress {W K A} {
    # %K —— 按键的 keysym 名称（符号名，不是字符本身）。
    #        字母/数字键的 keysym 通常恰好等于字符本身，比如按 "a" 键，K 就是 "a"；
    #        但很多键没有对应的可打印字符，比如 Return、Escape、Left、F1，
    #        这些键的 keysym 是固定的符号名字符串："Return"、"Escape"、"Left"、"F1"。
    # %A —— 按键对应的"可插入的可打印字符"（如果这个键能产生字符）。
    #        例子对比：
    #          按下字母键 a        -> K="a"      A="a"
    #          按下 Return         -> K="Return"  A=一个不可见的回车字符（CR）
    #          按下 Escape / Left  -> K="Escape"/"Left"  A=""（空字符串，没有可打印字符）
    if {$A eq ""} {
        set shownA "(none)"
    } else {
        set shownA $A
    }
    set ::lastKeyInfo "KeyPress  W=$W  K=$K  A=$shownA"
}

proc onMouseClick {W x y} {
    # %W —— 事件实际发生的 widget 路径。
    #        这里恒为 ".pad"，因为这个 bind 只注册在 .pad 这一个具体实例上；
    #        如果把同一个 proc 绑定在 widget 类（比如 Frame）或 "all" 标签上，
    #        点击不同控件时，W 会变成"实际被点击的那个" widget 路径。
    # %x %y —— 鼠标指针相对于"事件发生的 widget"（即 W，不是屏幕、也不是主窗口）
    #        左上角的像素坐标。
    #        .pad 是 240x100：
    #          在左上角点击 -> x,y 接近 (0, 0)
    #          在正中间点击 -> x,y 接近 (120, 50)
    #          在右下角点击 -> x,y 接近 (240, 100)
    set ::lastMouseInfo "Button-1  W=$W  x=$x  y=$y"
}

# --------------------------------------------------------------------------
# 绑定 1：键盘事件，绑定在主窗口 "." 这个具体 widget 实例上
# --------------------------------------------------------------------------
# <KeyPress> 匹配任意按键按下（<Key> 是它的同义写法）。
# bind 脚本里的 %W %K %A 会在事件真正触发的那一刻，被 Tk 替换成具体的值，
# 然后整段替换后的文本才会作为一条 Tcl 命令被执行——
# 这正是 README 里要强调的"事件替代发生在命令执行之前"的顺序。
bind . <KeyPress> {onKeyPress %W %K %A}

# --------------------------------------------------------------------------
# 绑定 2：鼠标事件，绑定在 .pad 这个具体 widget 实例上
# --------------------------------------------------------------------------
# <Button-1> 匹配鼠标左键按下（按下的瞬间触发，不需要等松开）。
bind .pad <Button-1> {onMouseClick %W %x %y}

# 必须显式让主窗口获得键盘输入焦点，否则 <KeyPress> 绑定永远不会触发：
# X11 的键盘事件只会送给"当前持有输入焦点"的那个 widget，
# 窗口刚创建出来的时候，焦点归属并不确定（取决于窗口管理器策略）。
focus .

button .quit -text "Quit" -command {destroy .}
pack .quit -pady {4 10}
```

**事件替代符号速查：**

| 符号 | 含义 | 本例触发场景 |
|---|---|---|
| `%W` | 事件实际发生的 widget 路径 | 按键时是 `.`；点击 `.pad` 时是 `.pad` |
| `%x` `%y` | 指针相对于 `%W` 左上角的像素坐标 | 在 240x100 的 `.pad` 正中间点击，约为 `(120, 50)` |
| `%K` | 按键的 keysym 符号名 | 按 `a` 键是 `a`；按回车是 `Return`；按左方向键是 `Left` |
| `%A` | 按键对应的可打印字符（没有则为空串） | 按 `a` 键是 `a`；按 `Escape`/方向键是空字符串 |

**为什么要把 `%W %K %A` 转发给 proc，而不是直接在 bind 脚本里写逻辑？** 这是故意的工程实践选择（见第 8 节），把"事件替代"和"业务逻辑"分开，`onKeyPress`/`onMouseClick` 本身是普通 proc，完全可以脱离 Tk、用 `tclsh` 单独调用来测试字符串拼接逻辑是否正确（本层的"校验方式"一节已经这样做了）。

### 示例 C：`clock_timer.tcl` —— Timer Callback 与周期性调度

```tcl
#!/usr/bin/env wish
#
# clock_timer.tcl —— Layer 4 示例 C：Timer Callback（after）与周期性调度
#
# after ms script 是"一次性"的定时器：
#   注册之后，script 只会在大约 ms 毫秒之后被调用一次，然后这个定时器就结束了。
#   要做出"每秒走一次"的时钟效果，必须让 script 在自己执行的最后一步，
#   重新调用一次 after，把"下一次触发"重新安排进事件循环——这叫"递归调度"。
#   这和写一个 while 死循环去反复检查时间完全不同：
#   递归调度每次都会"返回"，把控制权交还给事件循环；
#   阻塞循环永远不返回，会把整个程序冻住（见 README "Common Failure"）。
#
# 运行方式：
#   有图形环境：   wish clock_timer.tcl
#   headless 环境： tclsh clock_timer.tcl  （只能校验到 package require Tk 这一步）
#
# 本文件里的 formatNow 是一个完全不依赖 Tk、不依赖事件循环的纯逻辑 proc，
# 可以在 tclsh 里单独定义并调用来验证逻辑是否正确，
# 验证方式见 README.md 的 "Execution Walkthrough" 一节。

package require Tk

wm title . "Clock Timer Demo"

# --------------------------------------------------------------------------
# 纯逻辑部分：只用核心 Tcl 的 clock 命令，不涉及任何 widget / 事件循环
# --------------------------------------------------------------------------
proc formatNow {} {
    # clock seconds  —— 返回当前时间，Unix epoch 整数秒（自 1970-01-01 起的秒数）
    # clock format   —— 把整数秒转换成人类可读的字符串，-format 控制输出格式
    #                    %Y-%m-%d 年-月-日，%H:%M:%S 时:分:秒（24 小时制）
    return [clock format [clock seconds] -format "%Y-%m-%d %H:%M:%S"]
}

# --------------------------------------------------------------------------
# 界面状态
# --------------------------------------------------------------------------
set ::clockText   [formatNow]
set ::timerStatus "stopped"

# afterId 为空字符串表示"当前没有已安排的定时器"。
# 这是避免"重复启动定时器"故障的关键状态变量：
# 每次准备安排新的 after 之前，都必须能判断"是不是已经有一个在排队了"。
set ::timerAfterId ""

label .clock  -textvariable ::clockText   -font {Helvetica 20} -padx 20 -pady 10
label .status -textvariable ::timerStatus -foreground gray
pack .clock
pack .status -pady {0 10}

# --------------------------------------------------------------------------
# tick：周期性定时器回调本体
# --------------------------------------------------------------------------
proc tick {} {
    set ::clockText [formatNow]
    # 关键点：tick 在更新完界面状态之后，"重新安排自己"作为下一次触发，
    # 而不是用 while/for 阻塞等待。
    # after 1000 tick 把"1 秒后调用 tick"这件事注册进事件循环的定时器队列，
    # 然后 tick 立刻返回——把控制权还给事件循环；
    # 大约 1 秒后，事件循环才会真正地把 tick 当作一条命令调用一次。
    # 这里必须把新的 afterId 存回 ::timerAfterId，否则 stopTimer 就找不到
    # "最新这一次"排队的定时器，取消不掉。
    set ::timerAfterId [after 1000 tick]
}

# --------------------------------------------------------------------------
# startTimer / stopTimer：对外暴露的状态切换操作，绑定在按钮上
# --------------------------------------------------------------------------
proc startTimer {} {
    # 防止重复启动：如果 timerAfterId 不是空字符串，说明已经有一个 tick 排队中了。
    # 这时如果再调用一次 tick（或者再 after 1000 tick 一次），
    # 就会同时存在两条独立的定时器链，各自每秒触发一次，
    # 界面上的时钟会变成"一秒跳两次甚至更多次"——这正是本教程第 12 章
    # （故障排查）要专门分析的"after 定时器重复启动"问题，这里先把正确的
    # 防重复写法留给你。
    if {$::timerAfterId ne ""} {
        return
    }
    set ::timerStatus "running"
    tick
}

proc stopTimer {} {
    # after cancel 取消一个"已经安排、还没有触发"的 after 事件。
    # 必须用 startTimer/tick 里存下来的 afterId 才能精确取消对应的那一次安排；
    # 取消之后要把 timerAfterId 清回空字符串，
    # 这样 startTimer 才知道"现在确实没有定时器在跑了，可以重新开始"。
    if {$::timerAfterId ne ""} {
        after cancel $::timerAfterId
        set ::timerAfterId ""
        set ::timerStatus "stopped"
    }
}

button .start -text "Start" -command startTimer
button .stop  -text "Stop"  -command stopTimer
pack .start -side left -padx 10 -pady 10
pack .stop  -side left -padx 10 -pady 10

button .quit -text "Quit" -command {destroy .}
pack .quit -side left -padx 10 -pady 10
```

**关于"启动即运行"的设计选择：** 本例故意**不**在脚本末尾自动调用 `startTimer`，而是要求用户点击 Start 才开始走表。这是为了让你能亲手触发两个关键场景：

1. 点击 Start 一次：时钟开始每秒更新，状态显示 `running`。
2. **连续快速点击 Start 两次**：第二次点击会被 `timerAfterId ne ""` 这个 guard 拦住，直接 `return`，不会产生第二条定时器链——这正是"防止重复启动"的正确写法，第 12 章会继续深入这个故障模式。

---

## 5. Execution Walkthrough

### 5.1 `button_callback.tcl` 的执行顺序

1. `package require Tk` 加载 Tk、连接显示、创建主窗口 `.`（headless 环境下在这一步报错并终止，这是预期行为）。
2. 定义 `proc greet`——这一步只是把一段代码登记成一个命令，不会执行 `greet` 的函数体。
3. 依次创建 `.b1`、`.b2`、`.b3` 三个按钮。关键是**创建按钮这一步**对三个 `-command` 参数的求值方式不同：
   - 创建 `.b1`、`.b2` 时，`greet` / `{greet}` 都只是字符串，`greet` 没有被调用，终端只打印"created; greet was NOT called"。
   - 创建 `.b3` 时，`[greet]` 触发命令替换，`greet` 在这一行就被调用了一次，终端会先打印 `Hello from callback`，然后才打印"created; notice greet ALREADY ran"。
4. 用 `.b1 cget -command`、`.b2 cget -command`、`.b3 cget -command` 把三个按钮真正存下来的值打印出来，验证 `.b1`/`.b2` 存的是字符串 `"greet"`，`.b3` 存的是空字符串 `""`。
5. `pack` 完成布局后，脚本执行到文件末尾，`wish` 隐式进入事件循环。
6. 之后，点击 `.b1`/`.b2` 才会分别触发一次 `uplevel #0 greet`，打印 `Hello from callback`；点击 `.b3` 因为存的是空脚本，什么都不会发生。

### 5.2 `key_binding.tcl` 的执行顺序

1. `package require Tk` 之后创建主窗口、设置标题和尺寸。
2. 创建 `.hint`、`.keyinfo`、`.mouseinfo` 三个 label，其中后两个通过 `-textvariable` 绑定到 `::lastKeyInfo`/`::lastMouseInfo`（Layer 3 的机制：变量变化会自动触发 label 重绘，不需要手动调用任何"刷新"命令）。
3. 创建 `.pad` 这个 frame，并用 `pack propagate .pad 0` 锁定它的尺寸为 240x100。
4. 定义 `onKeyPress`、`onMouseClick` 两个 proc——这一步同样只是登记，不执行。
5. `bind . <KeyPress> {onKeyPress %W %K %A}`：把一段**模板脚本**注册进主窗口 `.` 的 bind 表。注意这一行执行完，`onKeyPress` **没有**被调用——`%W %K %A` 这些占位符要等真正有键盘事件发生时才会被替换成具体值。
6. `bind .pad <Button-1> {onMouseClick %W %x %y}`：同理，注册在 `.pad` 这个具体 widget 上。
7. `focus .`：显式把输入焦点给主窗口，否则第 5 步注册的键盘绑定永远不会被触发。
8. 脚本结束，进入事件循环。此后：
   - 每次按键，事件循环把 `<KeyPress>` 事件翻译成一次具体调用，比如按下 `a` 键变成 `onKeyPress . a a`，执行后 `::lastKeyInfo` 变化，`.keyinfo` 自动刷新显示。
   - 每次在 `.pad` 内点击左键，翻译成类似 `onMouseClick .pad 120 50` 的调用。

### 5.3 `clock_timer.tcl` 的执行顺序

1. `package require Tk` 之后创建主窗口。
2. 定义 `formatNow`（纯逻辑，见下方"headless 校验"）。
3. `set ::clockText [formatNow]`：初始化时间文字，这里的 `[formatNow]` 也是命令替换，但这次是**我们故意要的"立刻执行一次"**（拿到启动时刻的时间字符串），和示例 A 里 `[greet]` 的坑恰恰相反——这说明命令替换本身没有对错，关键是"你是否真的想要立刻执行"。
4. `set ::timerAfterId ""`：初始状态为"没有定时器在跑"。
5. 创建 `.clock`、`.status` 两个 label，创建 `.start`/`.stop`/`.quit` 三个按钮，`-command` 分别绑定 `startTimer`/`stopTimer`/`{destroy .}`。
6. 脚本结束，进入事件循环，但**此时时钟并没有开始走**（因为我们没有在脚本末尾调用 `startTimer`）。
7. 用户点击 Start -> 调用 `startTimer` -> 因为 `timerAfterId` 是空串，guard 不拦截 -> 设置状态为 `running` -> 调用 `tick`。
8. `tick` 更新 `::clockText`，然后调用 `after 1000 tick`，把"1 秒后再调用一次 tick"安排进事件循环的定时器队列，`tick` **立刻返回**。
9. 大约 1 秒后，事件循环发现这个定时器到期，调用 `tick`，重复第 8 步——这就是"递归调度"实现周期性效果的完整机制。
10. 用户点击 Stop -> 调用 `stopTimer` -> `after cancel $::timerAfterId` 取消下一次还没触发的定时器 -> 清空 `timerAfterId` -> 时钟停止在最后一次更新的时间上。

### 5.4 headless 校验记录（`tclsh`）

三个文件都用 `tclsh` 执行，确认报错只发生在 `package require Tk` 尝试连接显示的那一步，之前没有任何语法错误：

```text
$ tclsh button_callback.tcl
no display name and no $DISPLAY environment variable
    while executing
"load ... Tk"
    (file "button_callback.tcl" line 15)

$ tclsh key_binding.tcl
no display name and no $DISPLAY environment variable
    ...
    (file "key_binding.tcl" line 19)

$ tclsh clock_timer.tcl
no display name and no $DISPLAY environment variable
    ...
    (file "clock_timer.tcl" line 21)
```

三个文件都在 `package require Tk` 这一行才报错，说明这一行之前的所有 Tcl 语法（包括所有 `proc` 定义、`bind`/`after` 的脚本字符串）都是合法的。

对于 `clock_timer.tcl` 里"不依赖 Tk 事件循环的部分"，额外用 `tclsh` 单独验证了两段纯逻辑：

```text
$ tclsh <<'EOF'
proc formatNow {} {
    return [clock format [clock seconds] -format "%Y-%m-%d %H:%M:%S"]
}
puts "formatNow => [formatNow]"
EOF
formatNow => 2026-10-08 19:43:27
```

```text
$ tclsh <<'EOF'
# ...定义 tick / startTimer / stopTimer 的同构逻辑，
# 用 vwait 代替 Tk 事件循环来驱动 after 队列...
startTimer
startTimer   ;# 第二次调用应该被 guard 拦住
vwait ::done
EOF
tick fired, count=1
startTimer: already running, guard triggered, no-op
tick fired, count=2
tick fired, count=3
final tickCount=3
```

这证明：`after`/`after cancel` 本身是 Tcl 核心命令（不属于 Tk），即使没有图形界面，也可以单独用 `tclsh` + `vwait` 驱动定时器队列来验证"防止重复启动"的状态机逻辑是否正确——这正是本层"纯逻辑可以脱离 Tk 独立验证"的具体做法。

---

## 6. Data Flow / Control Flow

任何一次用户操作、定时器到期或窗口事件，都会走完下面这条完整的链路：

```text
+--------------------------------------+
| Application Startup                 |
+--------------------------------------+
                  |
                  v
+--------------------------------------+
| Create Widgets                      |
+--------------------------------------+
                  |
                  v
+--------------------------------------+
| Configure Layout                    |
+--------------------------------------+
                  |
                  v
+--------------------------------------+
| Event Loop                          |
+--------------------------------------+
                  |
                  v
+--------------------------------------+
| User Action / Timer / Window Event  |
+--------------------------------------+
                  |
                  v
+--------------------------------------+
| Dispatch Callback                   |
+--------------------------------------+
                  |
                  v
+--------------------------------------+
| Update Application State            |
+--------------------------------------+
                  |
                  v
+--------------------------------------+
| Refresh UI                          |
+--------------------------------------+
                  |
                  v
+--------------------------------------+
| Return to Event Loop                |
+--------------------------------------+

(Return to Event Loop 会循环回到上面的 Event Loop 这一格，
 只要窗口还存在，这个循环就会一直重复下去。)
```

把这张图和三个示例对应起来看：

- **Application Startup -> Create Widgets -> Configure Layout**：对应三个文件里 `package require Tk` 之后、`pack` 完成之前的所有代码，只执行一次。
- **Event Loop -> User Action / Timer / Window Event**：对应 `wish` 隐式进入的循环，以及用户点击按钮（示例 A）、按键/点击（示例 B）、定时器到期（示例 C）这三类具体事件来源。
- **Dispatch Callback**：对应事件循环把事件翻译成一次具体调用——`greet`、`onKeyPress`、`tick` 分别是三个示例里"被分派"的那个命令。
- **Update Application State**：对应回调内部对 Tcl 变量的修改——示例 B/C 里的 `set ::lastKeyInfo ...`、`set ::clockText ...`。
- **Refresh UI**：这一步在 Tk 里**不需要你手写任何代码**——因为 `.keyinfo`/`.clock` 这些 label 用了 `-textvariable`，Tk 内部用 `trace` 机制监听了这个变量，变量一变，label 立刻自动重绘。这是 Layer 3 讲的变量绑定机制和 Layer 4 的事件循环配合工作的地方。
- **Return to Event Loop**：回调函数（`greet`/`onKeyPress`/`tick`）执行到最后一行、正常 `return`，事件循环才会去看下一个事件——这也是为什么"回调里不能阻塞"（见第 7 节）：只要回调不返回，这个箭头就走不到，整条链路就卡在"Dispatch Callback"这一格，程序从用户的角度看就是"没反应"。

---

## 7. Common Failure

### 7.1 不要把 `update` 当作默认的事件循环方案

`update` 命令会让 Tcl **立刻处理一批排队中的事件**（包括重绘、其它控件的点击、定时器到期），处理完才返回到调用 `update` 的地方。听起来像是"手动刷新界面"的便捷方法，但它有一个致命问题：

> **`update` 会在当前回调还没返回的情况下，提前启动了一轮新的事件处理——这叫嵌套事件循环（Nested Event Loop）。**

具体风险场景：假设按钮 `.b1` 的 `-command` 里写了 `update`（比如想在一个耗时操作中间"刷新一下界面"）：

```text
用户点击 .b1 (第一次)
  -> 事件循环 dispatch -> 开始执行 .b1 的 -command 回调
     -> 回调内部执行到 update
        -> update 临时启动了"第二层"事件处理
           -> 用户在这个间隙又点击了 .b1 (第二次)
              -> 这次点击被 update 看到了，立刻又 dispatch 了一次
                 .b1 的 -command 回调 (重入！第一次调用还没返回)
           -> 两次 .b1 回调的调用栈现在是嵌套的：
              第一次调用 -> update -> 第二次调用
averaged
```

如果回调内部有"当前是否在处理中"这类状态（比如示例 C 的 `timerAfterId`），重入会让这些状态在还没来得及被第一次调用正确维护之前，就被第二次调用读到一个"半更新"的值，导致难以复现的 bug。**`vwait`、`tkwait` 和 `update` 一样，都会在当前调用栈内再启动一轮事件处理，原理相同、风险相同。**

正确做法：不要在回调内部手动调用 `update` 去"强制刷新"；如果确实需要把一个长任务拆成多步，用 `after idle` / `after 0` 把下一步重新安排进事件循环（见 7.3 节），而不是用 `update` 去抢当前调用栈里的执行权。

### 7.2 不要用阻塞循环更新 GUI

另一个经典错误：

```tcl
# 错误示范：不要这样写！
button .run -text "Run" -command {
    for {set i 0} {$i < 100000000} {incr i} {
        # 假装在做耗时计算
    }
    puts "done"
}
```

这段代码会让整个程序在用户点击 `.run` 之后**完全冻住**：

- 事件循环永远不会处理下一个事件，直到这个 `for` 循环跑完——因为"回调必须完整返回，事件循环才能继续"这条规则没有例外。
- 窗口无法重绘（挪动窗口会留下残影或花屏）、无法响应最小化/关闭按钮、无法响应任何新的点击——用户体验上就是"程序卡死了"，哪怕底层代码逻辑完全正确、没有死循环 bug。
- 这正是"事件分发是同步处理"的直接后果（见 3.2 节）：回调执行期间，事件循环这一侧完全没有机会被调度。

### 7.3 把长任务拆成多个短小的事件回调

正确的做法是把一个大任务拆成很多"足够短"的小步骤，每做完一步就用 `after idle` 或 `after 0` 把下一步重新安排进事件循环，主动把控制权还给事件循环，让它有机会去处理用户输入、重绘等其它事件：

```tcl
# 正确示范：把长任务拆成分片执行
set ::runTotal 100000000
set ::runIndex 0
set ::runChunk 200000   ;# 每一片处理多少次迭代，按实际耗时调整

proc runStep {} {
    set limit [expr {min($::runIndex + $::runChunk, $::runTotal)}]
    for {} {$::runIndex < $limit} {incr ::runIndex} {
        # 真正的计算逻辑（这里只是示意）
    }
    if {$::runIndex < $::runTotal} {
        # 还没做完：用 after idle 把下一片安排到"事件循环空闲时"执行，
        # 这次 runStep 立刻返回，期间事件循环可以正常处理用户输入/重绘
        after idle runStep
    } else {
        puts "done"
    }
}
```

`after idle script` 和 `after 0 script` 的区别：

- `after idle script`：只在事件循环**没有其它待处理事件**（比如用户输入、重绘请求）时才执行，优先级比普通事件低，适合"不着急、尽量不抢占交互响应"的后台分片任务。
- `after 0 script`：按时间 0ms 排入**普通定时器队列**，和 `after idle` 的效果接近，但在某些实现细节和优先级上略有差异（`after idle` 明确是"idle 回调"队列，`after 0` 走的是普通 timer 队列）；两者共同点是**都会先把控制权还给事件循环，再在下一轮循环里被调用**，而不是像 `update` 那样抢占当前调用栈。

### 7.4 为什么回调里的阻塞操作会让界面失去响应

把 7.1~7.3 串起来看，本质原因只有一条：

> **Tk 只有一个事件循环、一个 Tcl 调用栈。回调没有返回之前，事件循环没有任何机会去处理下一个事件——不管这个事件是用户的新操作，还是窗口系统要求的重绘。**

任何会让回调"迟迟不返回"的操作都是危险的：

- 大量计算的阻塞循环（7.2 节）。
- 阻塞式网络请求 / 磁盘大文件 I/O（本层不展开实现，但原理与阻塞循环完全一样：只要调用没返回，事件循环就被卡住）。
- 在回调里调用 `update`/`vwait`/`tkwait` 看似"没有卡住"，但换来的是重入风险（7.1 节），本质上是用一种问题替换了另一种问题。

正确的方向永远是：**让每一次回调尽可能短**，耗时任务拆成分片（7.3 节）或者转移到真正的后台机制（线程/子进程，本教程后续不涉及），回调本身只做"调度下一步"这件轻量的事。

### 7.5 本层三个示例各自的常见坑（回顾）

| 文件 | 常见错误 | 后果 | 正确做法 |
|---|---|---|---|
| `button_callback.tcl` | 写成 `-command [greet]` | `greet` 在创建按钮时就执行了一次，之后点击按钮毫无反应 | 用裸字 `-command greet` 或 `-command [list greet $arg]` |
| `key_binding.tcl` | 忘记调用 `focus .` | `bind . <KeyPress> ...` 永远不会触发 | 创建完 widget 后显式 `focus` 需要接收键盘事件的窗口 |
| `clock_timer.tcl` | 反复点击 Start，每次都 `after 1000 tick` | 出现多条并行的定时器链，时钟"一秒跳好几次" | 用 `timerAfterId` 记录当前是否已有定时器在跑，在 `startTimer` 里做 guard |

---

## 8. Engineering Practice

1. **回调脚本只做"转发"，逻辑写进具名 proc。** `bind`/`-command` 里尽量只放一个简短的调用（比如 `{onKeyPress %W %K %A}`），不要把大段 if/for 逻辑直接写进字符串里——具名 proc 可读、可单独测试（可以直接用 `tclsh` 调用验证），字符串里的大段逻辑既难读也难测。
2. **用一个专门的状态变量追踪"异步操作是否在进行中"。** `clock_timer.tcl` 的 `::timerAfterId` 就是这个模式：不要在多处分散地猜测"定时器是不是在跑"，用一个变量作唯一真相来源（single source of truth），`startTimer`/`stopTimer` 都通过检查/更新这同一个变量来协调状态。
3. **把"算结果"和"刷新界面"分开写。** `formatNow` 只管算出时间字符串，不接触任何 widget；`tick` 负责把结果写进 `::clockText` 变量。这样 `formatNow` 可以完全脱离 Tk 被测试，`tick` 的职责也更单一。
4. **先用 `tclsh` 验证纯逻辑，再用 `wish` 验证真实交互。** 本层的开发/校验流程本身就是一个值得复用的实践：把不依赖 Tk 的部分（字符串处理、时间格式化、状态机）抽出来，先用 `tclsh`（甚至命令行 heredoc）单独跑通，再上 `wish` 去验证真实的点击/按键效果，能更早发现逻辑错误，而不是等到图形界面里才去排查。
5. **回调里不做阻塞 I/O。** 任何可能"久久不返回"的操作（网络请求、大文件读写、睡眠等待）都不应该直接写在 `-command`/`bind`/`after` 的回调里；需要的话用分片（`after idle`）或者把真正的阻塞工作转移出当前调用栈。
6. **谨慎使用、甚至避免使用 `update`。** 如果发现自己想用 `update` 来"让界面看起来不卡"，通常说明某个回调做得太重了，应该回头用 `after idle` 拆分任务，而不是用 `update` 掩盖问题——这几乎总会在未来引入重入 bug。

---

## 9. Exercises

1. **验证"大括号可以装多条命令"。** 给 `button_callback.tcl` 加一个 `.b4`，用 `-command {greet; greet}` 创建它。先猜一猜点击一次 `.b4` 会打印几行 `Hello from callback`，再运行验证，并解释为什么——这能巩固"`{...}` 分组出来的是一段脚本（script），可以包含多条用 `;` 或换行分隔的命令，而不仅仅是单个词"这个事实。

2. **扩展事件替代符号的练习。** 给 `key_binding.tcl` 的 `.pad` 再绑定一个 `<Double-Button-1>`（双击左键），触发时在 `::lastMouseInfo` 里打印一段不同的提示文字（比如 `"Double-Click  W=... x=... y=..."`），用来区分"单击"和"双击"两种修饰符不同的事件。额外思考：如果把这个新绑定注册在 `.`（而不是 `.pad`）上，`%W` 会打印出什么？为什么？

3. **在不破坏防重复启动机制的前提下加一个 Reset 按钮。** 给 `clock_timer.tcl` 增加一个 `.reset` 按钮，点击后要做到：①如果定时器正在运行，先调用 `stopTimer`；②把 `::clockText` 重新设成 `formatNow` 的当前结果；③点击 Reset 之后，时钟应该保持"停止"状态（不能自动重新开始走），只有再次点击 Start 才继续走。写完后，验证"先点 Start，再点 Reset，再连续点两次 Start"这个操作序列不会产生两条并行的定时器链。

---

## 10. Mental Model Summary

1. GUI 程序只有"初始化阶段"严格按源码顺序执行一次；之后的执行顺序由事件到达顺序决定，不由源码顺序决定。
2. `-command` 是 widget 自带的、范围有限的回调登记方式；`bind` 是通用的、可以绑定任意 X 事件到任意 widget/类/`all` 的机制；`after` 是基于时间、一次性的回调，需要自己重新安排才能周期触发。
3. `[...]`（命令替换）永远是"现在立刻执行一次并取返回值"，绝不是"把这个动作包装起来留到以后执行"——`-command [cmd]` 是新手最常踩的坑。
4. `wish` 在脚本执行到文件末尾后会自动进入事件循环；`tclsh` 不会，需要显式 `vwait` 才能让 `after`/事件相关的调度持续生效。
5. 事件循环是单线程、串行的：一次只处理一个事件，对应的回调必须完全返回之后，才会处理下一个事件——不存在两个回调同时运行的情况（除非手动引入嵌套事件循环）。
6. `update`/`vwait`/`tkwait` 都会在当前调用栈内再启动一轮事件处理，可能导致回调重入、状态读到"半更新"的中间值,应当谨慎使用。
7. 长时间阻塞的 `for`/`while` 循环会让事件循环完全没有机会被调度,界面冻结,包括无法重绘、无法响应关闭按钮。
8. 把长任务拆成短步骤,用 `after idle`/`after 0` 逐步重新安排,每一步做完都把控制权还给事件循环,是避免阻塞的标准做法。
9. `textvariable` 绑定的 widget 会在变量变化时自动重绘,是"Update Application State"自动联动到"Refresh UI"这一步的关键机制,不需要手写刷新代码。
10. 任何会让回调"迟迟不返回"的操作（阻塞计算、阻塞 I/O、滥用 `update`）都会让界面失去响应,因为 Tk 只有一个事件循环、一个调用栈。

---

## 11. Knowledge Check

1. 如果把 `button_callback.tcl` 里的 `-command [greet]` 改写成 `-command [list greet]`，点击按钮后的行为会和原来一样，还是不一样？请从"命令替换发生的时机"和"`list` 这个命令本身做了什么"两个角度解释原因。

2. 假设在某个按钮的 `-command` 回调内部调用了 `update`，而用户在这次回调还没返回之前，又点击了同一个按钮一次，会发生什么？这和"事件循环单线程串行处理事件"这条规则是否矛盾？为什么？

3. 为什么 `clock_timer.tcl` 里的 `tick` 要用 `after 1000 tick` 让自己"重新安排一次"，而不是写成类似 `while {1} { tick; after 1000 }` 这样的阻塞循环来实现同样的每秒刷新效果？两种写法在"谁拥有事件循环的控制权"这一点上有什么本质区别？
