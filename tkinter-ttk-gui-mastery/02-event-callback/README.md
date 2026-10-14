# Layer 2：事件系统与回调

> 对应示例代码：[examples/02_event_demo.py](examples/02_event_demo.py)

---

## A. Why

Layer 0/1 搭好了“骨架”，但窗口还是“死的”——点什么都没反应。GUI 程序真正的核心是：**用户做了某个动作，程序该执行哪段代码**。这一层要建立的 Mental Model 是：Tkinter 提供了两条不同的“挂钩子”的路径——`command=`（给“点击/选择”类动作用）和 `bind()`（给“任意事件序列”用，更底层、更通用）——它们的回调函数签名约定不一样，混淆是新手最常见的错误来源之一。

---

## B. Mental Model

### B.1 从用户动作到业务逻辑执行的链路

```text
User Input
   |
   v
Event / command dispatch
   |
   v
Callback
   |
   v
Application Logic
   |
   v
Widget Update
```

- **Event / command dispatch**：Tkinter 事件循环（`mainloop()`）收到一个底层事件（鼠标点击、按键），根据“这个事件发生在哪个 Widget 上”“这个 Widget 有没有注册对应的处理方式”，决定要不要触发回调。
- **Callback**：你写的 Python 函数/方法，被 Tkinter 在合适的时机调用。
- **Application Logic**：回调函数内部真正的业务代码（本例是 `submit()`）。
- **Widget Update**：业务逻辑执行完后，通常需要把结果写回某个控件（本例是更新 `result_var`，从而让绑定的 Label 文字自动刷新——这个“变量驱动控件更新”的机制会在 Layer 3 详细展开）。

### B.2 `command=` vs `bind()`：两条不同的挂钩路径

```text
                +-------------------+
                |  Tk Widget         |
                +---------+---------+
                          |
          +---------------+----------------+
          |                                |
          v                                v
   command= 协议                     bind(sequence, cb) 协议
   （仅部分控件支持：                （几乎所有控件 + 几乎所有事件序列都支持：
    Button / Checkbutton /            <Return> <KeyPress> <Button-1>
    Radiobutton / Menu 项 ...）        <FocusIn> <Double-Button-1> ...）
          |                                |
          v                                v
   回调签名：() -> None             回调签名：(event) -> None
   （不接收参数）                    （必须接收 1 个 event 参数）
```

`command=` 是 Tkinter 为“点击一下就该发生某事”这类高频场景提供的**简化协议**：你只需要给一个无参函数，Tk 内部已经帮你处理好“这是鼠标点击”这件事的细节。

`bind()` 是更底层、更通用的协议：你显式指定一个**事件序列**字符串（如 `"<Return>"`、`"<KeyPress-a>"`、`"<Double-Button-1>"`），任何控件、任何这类事件都可以绑定，但回调函数**必须**接收一个 `event` 参数，里面装着这次事件的详细信息。

### B.3 为什么 `command=callback()` 几乎总是错误写法

`command=` 期望的是一个**函数对象**（即“还没被调用、但随时可以被调用”的东西）。而 `callback()` 这个表达式本身，Python 会在**创建控件这一行代码执行的瞬间**立刻把它求值——也就是立刻调用一次 `callback`，然后把 `callback()` 的返回值（通常是 `None`）赋给 `command`。结果是：
- 按钮创建时，`callback` 被意外执行了一次。
- 之后无论点多少次按钮，`command` 实际存的是 `None`，什么也不会发生。

正确写法永远是传“裸的函数名/方法名”（不加括号），或者当你需要**预先绑定参数**时，用 `lambda`：

```python
# 错误：创建按钮的瞬间就调用了 do_something()，之后点击没有任何效果
ttk.Button(parent, command=do_something())

# 正确：传入函数对象本身，Tk 会在点击时才调用它
ttk.Button(parent, command=do_something)

# 需要带参数时，用 lambda 包一层，推迟调用时机
ttk.Button(parent, command=lambda: do_something(some_arg))
```

### B.4 `lambda` 的适用场景

`lambda` 本质上只是“定义一个匿名函数”，它在 Tkinter 回调里最常见的用途是**延迟调用并携带额外参数**——例如同一个处理函数要被绑定到多个按钮，但每个按钮需要传不同的参数：

```python
for name in ("A", "B", "C"):
    ttk.Button(parent, text=name, command=lambda n=name: select_item(n)).pack()
```

注意这里 `n=name` 的写法是必须的（经典的闭包延迟绑定问题）：如果写成 `command=lambda: select_item(name)`，循环结束后所有按钮里的 `name` 都指向同一个最终值（这是 Python 闭包的通用坑，不是 Tkinter 特有的，但在批量创建控件时非常容易踩中）。

### B.5 按钮回调 vs 键盘事件回调的区别

- 按钮回调（`command=`）：触发条件单一、明确（鼠标点击/空格键激活焦点上的按钮），不需要关心“是哪个按钮被点了”，因为每个按钮各自绑定自己的回调。
- 键盘事件回调（`bind("<KeyPress>", ...)` 等）：同一个绑定可能对应很多种输入，`event.keysym` 告诉你具体是哪个键；而且键盘事件默认绑定在某个具体 Widget 上（如 `self.entry.bind(...)`），只有该 Widget 拥有焦点时才会触发——这是初学者常忽略的细节（“为什么我按 Enter 没反应” 往往是因为焦点不在那个 Entry 上）。

---

## C. Architecture

| 对象 | 角色 |
|---|---|
| `EventDemoApp` | 把控件引用（`self.entry`）、状态（`submit_count`）、回调方法组织在一起的容器类 |
| `self.entry` (`ttk.Entry`) | 既是 `bind("<Return>", ...)` 的绑定目标，也是 `submit()` 读取用户输入的来源 |
| `submit_btn` (`ttk.Button`) | `command=self.on_submit_clicked` |
| `self.submit()` | 唯一的业务逻辑入口，被两条不同的回调路径共同调用，避免重复代码 |
| `self.result_var` / `self.event_info_var` (`tk.StringVar`) | 承接业务逻辑的输出，自动驱动对应 Label 文字刷新（机制见 Layer 3） |

---

## D. Complete Code

```python
"""
02_event_demo.py

Layer 2：事件系统与回调（command / bind / event 对象）。

运行方式：
    python3 examples/02_event_demo.py

本例演示：
    1. 一个输入框 + 一个“提交”按钮：点击按钮触发提交。
    2. 在输入框按 Enter 键也能触发同一个提交逻辑（bind("<Return>", ...)）。
    3. 提交逻辑只写一次（submit()），按钮回调和键盘回调都调用它，
       避免重复编写业务逻辑。
    4. 一个单独的区域展示最近一次事件的信息（event.keysym / event.widget），
       帮助建立“事件对象里到底有什么”的直觉。
"""

from __future__ import annotations

import tkinter as tk
from tkinter import ttk


class EventDemoApp:
    """把“状态”和“回调”都放进一个类里，避免散落的全局变量。

    这不是必须的写法（Layer 0/1 用函数式写法也完全可以），但当一个
    界面同时有多个控件需要互相读取对方状态时，用一个类把它们组织在
    一起会更清晰——这也是为后面 Layer 3/8 的类组织方式做铺垫。
    """

    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.submit_count = 0

        self._build_widgets()

    def _build_widgets(self) -> None:
        container = ttk.Frame(self.root, padding=16)
        container.pack(fill="both", expand=True)

        # --- 输入区 ---
        input_row = ttk.Frame(container)
        input_row.pack(fill="x")

        ttk.Label(input_row, text="输入内容：").pack(side="left")

        self.entry = ttk.Entry(input_row)
        self.entry.pack(side="left", fill="x", expand=True, padx=(4, 8))
        # 关键点：bind 的第二个参数必须是一个“函数对象”（接收 event 参数），
        # 而不是函数调用结果。这里用 lambda 把 event 转发给 on_enter_pressed，
        # 原因见下方 on_enter_pressed 的文档字符串。
        self.entry.bind("<Return>", self.on_enter_pressed)

        # command=self.on_submit_clicked 传入的是“函数对象本身”。
        # 如果写成 command=self.on_submit_clicked()，Python 会在创建按钮的
        # 那一刻立刻调用一次 on_submit_clicked()，并把它的返回值（通常是
        # None）赋给 command——按钮点击时根本不会再触发任何逻辑。这是
        # 新手最常踩的坑之一，详见本层 README 的 Common Failure。
        submit_btn = ttk.Button(input_row, text="提交", command=self.on_submit_clicked)
        submit_btn.pack(side="left")

        # --- 结果展示区 ---
        self.result_var = tk.StringVar(value="（尚未提交）")
        ttk.Label(container, textvariable=self.result_var, foreground="blue").pack(
            fill="x", pady=(12, 4)
        )

        # --- 事件信息展示区：帮助理解 event 对象里有什么 ---
        self.event_info_var = tk.StringVar(value="（尚未触发任何按键事件）")
        ttk.Label(
            container,
            textvariable=self.event_info_var,
            foreground="gray25",
        ).pack(fill="x")

    def submit(self, source: str) -> None:
        """唯一的提交逻辑。无论是“点击按钮”还是“按 Enter”，最终都调用这一个
        方法，避免在两个回调里各写一遍一样的校验/处理代码。
        """
        text = self.entry.get().strip()
        self.submit_count += 1

        if not text:
            self.result_var.set(f"[第 {self.submit_count} 次] 输入为空，已忽略。")
            return

        self.result_var.set(
            f"[第 {self.submit_count} 次，来自 {source}] 你输入了：{text!r}"
        )

    # ------------------------------------------------------------------
    # 回调函数的参数约定：
    #   - command= 绑定的回调：不接收任何参数（Tkinter 调用时不传参）。
    #   - bind() 绑定的回调：Tkinter 会传入恰好一个参数，即 event 对象。
    # 这是两种不同的回调协议，混淆会导致 TypeError。
    # ------------------------------------------------------------------

    def on_submit_clicked(self) -> None:
        """command= 回调：不接收参数。"""
        self.submit(source="按钮点击")

    def on_enter_pressed(self, event: tk.Event) -> None:
        """bind() 回调：必须接收一个 event 参数，即使我们不一定用到它。

        event 对象上常用的几个属性：
            event.widget : 触发事件的控件对象（这里就是 self.entry）
            event.keysym : 按下的键的符号名（这里几乎总是 "Return"，
                           因为我们绑定的序列就是 <Return>）
            event.char   : 按键对应的字符（某些特殊键没有对应字符）
        """
        self.event_info_var.set(
            f"event.widget={event.widget!r}, event.keysym={event.keysym!r}"
        )
        self.submit(source="回车键")


def build_root_window() -> tk.Tk:
    root = tk.Tk()
    root.title("Event Demo - command + bind")
    root.geometry("420x200")
    return root


def main() -> None:
    root = build_root_window()
    EventDemoApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
```

---

## E. Lifecycle

1. 创建 `root` → 创建 `EventDemoApp`，构造函数内部完成所有控件创建、`command=` 绑定、`bind()` 绑定。
2. 进入 `mainloop()`：事件循环开始运转。
3. 用户在输入框打字：这些是底层的 `<KeyPress>` 事件，但我们没有单独绑定它们，所以只是默默更新了 Entry 内部的文本缓冲区（Entry 控件自带这个行为，不需要你写代码）。
4. 用户点击“提交”按钮：Tk 识别出这是一次按钮点击，触发 `command=self.on_submit_clicked` → 调用 `on_submit_clicked()` → 调用 `self.submit("按钮点击")` → 更新 `self.result_var`，Label 文字自动刷新。
5. 用户在输入框按下 Enter：Tk 识别出 Entry 收到了 `<Return>` 事件，且该 Entry 上注册过这个序列的绑定 → 调用 `on_enter_pressed(event)` → 更新事件信息展示 → 调用 `self.submit("回车键")`。
6. 两条路径最终都调用了同一个 `submit()`，所以“业务规则改一处，两条触发路径都生效”。

---

## F. Key APIs

| API | 作用 | 输入 | 输出 / 副作用 | 常见错误 |
|---|---|---|---|---|
| `ttk.Button(parent, command=fn)` | 创建按钮并绑定“点击时调用的无参函数” | `fn` 必须是无参可调用对象 | 点击按钮时调用 `fn()` | 写成 `command=fn()`（见 B.3） |
| `widget.bind(sequence, callback)` | 给某个具体 Widget 绑定某类事件 | `sequence` 如 `"<Return>"`；`callback` 必须接收 1 个 `event` 参数 | 该事件发生在这个 Widget 上时调用 `callback(event)` | 回调函数没有声明 `event` 形参，运行时抛 `TypeError: takes 0 positional arguments but 1 was given` |
| `event.widget` | 触发事件的控件对象引用 | — | 可用来反查“是哪个控件触发的” | 在多控件共享同一个回调时忘记用它区分来源 |
| `event.keysym` | 按键的符号名（如 `"Return"`、`"Escape"`、`"a"`） | — | 用来判断“具体按的是哪个键” | 和 `event.char` 混淆：某些功能键（方向键、F1 等）没有对应的可打印字符，`event.char` 可能是空字符串 |
| `entry.get()` | 读取 Entry 当前文本内容 | — | 返回 `str` | 在用户还没输入时就假设非空，不做 `.strip()` 和空值校验 |

---

## G. Common Failure

### G.1 按钮点击完全没反应

- **现象**：点击按钮，`result_var` 从不更新。
- **根本原因**：`command=self.on_submit_clicked()` 写成了函数调用（多了括号），控件创建时已经调用了一次，`command` 实际值是 `None`。
- **定位方法**：在 `on_submit_clicked` 第一行加一句 `print("called")`，运行程序——如果这句话在**窗口刚显示出来、还没点任何按钮**时就打印了一次，说明是这个坑。
- **修复方案**：去掉多余的括号，传函数对象本身。

### G.2 按 Enter 键没反应，但点击按钮正常

- **现象**：鼠标点按钮触发提交正常，但在输入框里按 Enter 没有任何效果。
- **根本原因**：最常见的是 `bind()` 绑定错了控件（比如绑在了 `root` 而不是 `self.entry` 上，但 `root` 没有焦点，键盘事件不会传给它），或者绑定的事件序列字符串写错（例如 `"Return"` 少了尖括号，变成了普通文本而不是事件序列）。
- **定位方法**：确认 `self.entry.bind("<Return>", ...)` 的调用对象确实是你点击/聚焦的那个 Entry；用 `print()` 在回调首行确认是否真的被调用。
- **修复方案**：确保 `bind()` 绑在正确的 Widget 上，事件序列字符串拼写和尖括号完整。

### G.3 `bind()` 回调抛出 `TypeError`

- **现象**：`TypeError: on_enter_pressed() missing 1 required positional argument: 'event'` 或 `takes 0 positional arguments but 1 was given`。
- **根本原因**：混淆了 `command=` 和 `bind()` 两种协议的函数签名——给 `bind()` 传了一个无参函数，或者把一个本该给 `bind()` 用的函数误用在 `command=` 上。
- **定位方法**：看报错堆栈里具体是哪种不匹配（“missing argument”说明函数签名要 `event` 但没传；“too many arguments”说明函数不要参数但传了 `event`）。
- **修复方案**：`command=` 的回调写成 `def fn(self) -> None`（无额外参数）；`bind()` 的回调写成 `def fn(self, event: tk.Event) -> None`。

---

## H. Exercises

1. **基础修改**：给输入框再绑定一个 `"<Escape>"` 事件，按下 Esc 时清空输入框内容（`self.entry.delete(0, "end")`），并在 `event_info_var` 里提示“已清空”。预期结果：焦点在输入框时按 Esc，输入内容被清空，并显示提示信息。
2. **工程向练习**：把“提交”按钮改造成支持“防抖”——如果用户在 500 毫秒内重复点击/重复按 Enter，第二次调用应该被忽略并在结果区提示“操作过于频繁，已忽略”。提示：你需要记录“上一次成功提交的时间”，判断本次调用时间与上次的差值（这一步先不需要 `after()`，只需要 `time.monotonic()` 做时间差比较；`after()` 本身会在 Layer 6 详细讲）。预期结果：手速很快连续点击按钮，只有间隔超过 500ms 的点击才会真正触发 `submit()`。

---

## I. Summary

本层新增的 Mental Model：

- `command=` 和 `bind()` 是两条独立的“挂钩子”路径，分别对应不同的回调函数签名约定（无参 vs 带 `event` 参数）。
- `command=callback` 传的是函数对象，`command=callback()` 传的是函数调用结果——这个区分贯穿整个 Tkinter 回调体系，务必在脑子里固化。
- 把真正的业务逻辑抽成一个独立方法（本例的 `submit()`），让多条触发路径共享同一份逻辑，是避免“到处复制粘贴校验代码”的基本工程手段。

与前序 Layer 的关系：Layer 0 让窗口“活起来”，Layer 1 让控件“摆整齐”，Layer 2 让控件开始对用户动作“做出反应”。下一层要解决的问题是：业务逻辑产生的“状态”（比如一个复选框是否勾选）该怎么被 Tkinter 自动感知并同步到界面上，而不需要你手写一堆 `get()`/`set()`。

下一步：[Layer 3：控件与变量绑定](../03-variables-form-binding/README.md)。
