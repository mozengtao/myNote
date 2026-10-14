# Layer 0：环境与最小 GUI

> 对应示例代码：[examples/00_hello_tkinter.py](examples/00_hello_tkinter.py)

---

## A. Why

在学习任何 API 之前，必须先回答一个问题：**我现在运行的程序，到底是谁在画窗口、谁在等待我点击？**

C/C++/Go/Rust 工程师写惯了“从 `main()` 顺序执行到结束”的程序。而一个 GUI 程序的 `main()` 函数通常只运行几毫秒就“卡住”了——它卡在 `mainloop()` 里，之后的所有行为都是**事件驱动**的，不再是你写的代码从上到下顺序触发的。

Layer 0 要解决的问题是：在不涉及任何业务逻辑的情况下，先把这套“骨架”跑起来，建立最基础的直觉——窗口是怎么出现的，程序是怎么“活”起来的。

---

## B. Mental Model

### B.1 Tkinter / ttk / Tcl/Tk 的关系

很多人以为 Tkinter 是 Python 原生实现的 GUI 库，其实不是。真实情况是三层封装：

```text
+-----------------------------------------------------+
| Your Python Code                                     |
|   import tkinter as tk                                |
|   from tkinter import ttk                             |
+-------------------------+-----------------------------+
                          |  Python 对象方法调用（如 .pack()）
                          v
+-----------------------------------------------------+
| tkinter / tkinter.ttk  (Python 标准库)                |
|   把 Python 方法调用翻译成 Tcl 命令字符串               |
+-------------------------+-----------------------------+
                          |  Tcl 命令字符串（如 "pack .label"）
                          v
+-----------------------------------------------------+
| Tcl 解释器 + Tk 工具包  (C 语言实现，内嵌在 Python 进程里) |
|   解析命令、管理 Widget、维护事件循环                   |
+-------------------------+-----------------------------+
                          |  调用操作系统窗口 API
                          v
+-----------------------------------------------------+
| 操作系统窗口系统 (X11 / Win32 / Aqua)                  |
+-----------------------------------------------------+
```

- **Tcl/Tk**：Tk 是一套用 C 写的 GUI 工具包，最初是 Tcl 语言的扩展包。它提供"创建窗口""创建按钮""处理事件"等能力。
- **Tkinter**：Python 标准库中的一层**绑定（binding）**。当你写 `ttk.Button(root, text="OK")` 时，Tkinter 在背后生成了类似 `ttk::button .button1 -text OK` 的 Tcl 命令，再交给内嵌的 Tcl 解释器执行。
- **ttk（"themed tk"）**：Tk 自带的旧控件（`tk.Button`、`tk.Entry` 等）外观陈旧、跨平台不一致。`ttk` 是 Tk 8.5 之后引入的**主题化控件集**，提供 `ttk.Button`、`ttk.Entry`、`ttk.Combobox`、`ttk.Treeview` 等更现代、跨平台外观更一致的控件。**工程实践中：优先用 `ttk`，只有 `ttk` 没有对应控件时才退回 `tk`**（例如 `tk.Menu`、`tk.Text`、变量对象 `tk.StringVar` 本身也在 `tk` 命名空间下）。

这就是为什么你会同时看到 `import tkinter as tk` 和 `from tkinter import ttk`：`tk` 提供根窗口、变量对象、Menu、Text 等基础设施；`ttk` 提供外观更好的标准控件。

### B.2 Tk() → Widget 创建 → mainloop() 三者的关系

```text
tk.Tk()                 创建一个“Tcl 解释器实例 + 根窗口”
   |
   | 之后创建的每个 Widget 都要显式或隐式关联到这个解释器实例
   v
ttk.Label(root, ...)     向 Widget Tree 里插入一个节点（但还没有被“布局”显示）
   |
   | .pack() / .grid() / .place()
   v
Widget 出现在屏幕上（但还不会响应任何事件，因为事件循环没启动）
   |
   | root.mainloop()
   v
事件循环开始运转：等待事件 -> 分发 -> 执行回调 -> 继续等待 ……
```

一句话 Mental Model：

> **`tk.Tk()` 启动了一个“世界”（Tcl 解释器 + 根窗口）；之后创建的 Widget 是这个世界里的“物件”；`mainloop()` 则是让这个世界开始“流动时间”——没有它，物件只是静态摆设，不会对任何输入做出反应。**

### B.3 为什么必须调用 `mainloop()`

GUI 程序本质上是一个无限循环：

```text
while True:
    event = wait_for_next_event()      # 等待鼠标/键盘/系统事件
    widget, callback = dispatch(event) # 根据事件找到对应的控件和回调
    callback()                         # 执行你写的业务代码
```

`mainloop()` 就是 Tkinter 帮你实现好的这个循环（真实实现在 C 层，但效果等价于上面的伪代码）。如果不调用 `mainloop()`：
- 窗口可能一闪而过就被销毁（因为 Python 脚本执行到文件末尾就退出了，根窗口对象被垃圾回收）。
- 即使窗口显示出来了，它也**不会响应任何点击、输入或拖动**，因为没有任何代码在"等待并分发事件"。

对比你熟悉的语言：这和嵌入式/网络编程里的 `while(1) { poll(); dispatch(); }` 事件循环，或者 Go 里 `for { select { ... } }` 的思路是一致的——**没有循环，就没有"持续响应"这件事**。更详细的类比见 [10-concept-mapping/README.md](../10-concept-mapping/README.md)。

---

## C. Architecture

Layer 0 涉及的核心对象很少，但要分清楚它们的角色：

| 对象 | 角色 |
|---|---|
| `tk.Tk()` 实例（本例中的 `root`） | 根窗口 + 内嵌 Tcl 解释器的 Python 句柄；是 Widget Tree 的根节点 |
| `ttk.Label` 实例 | 一个具体的 Widget，必须有一个 parent（这里是 `root`） |
| `root.mainloop()` | 事件循环入口，程序会“卡”在这一行，直到窗口关闭 |

---

## D. Complete Code

```python
"""
00_hello_tkinter.py

Layer 0：最小可运行的 Tkinter + ttk 窗口。

运行方式：
    python3 examples/00_hello_tkinter.py

这个文件只做三件事：
    1. 创建一个 Tk 根窗口（root window）。
    2. 在根窗口里创建一个 ttk.Label 控件。
    3. 调用 mainloop() 启动事件循环，让窗口“活起来”。
"""

from __future__ import annotations

import tkinter as tk
from tkinter import ttk


def build_root_window() -> tk.Tk:
    """创建并配置根窗口。

    根窗口（root window）是整个 Tkinter 应用的“地基”：
    - 它是 Widget Tree 的根节点，之后创建的所有控件都必须直接或间接
      挂在这个 root 之下。
    - 它同时对应操作系统窗口管理器里的一个真实窗口（有标题栏、可以
      移动/缩放/关闭）。
    """
    root = tk.Tk()
    root.title("Hello Tkinter")
    # geometry("宽x高") 只是“初始尺寸建议”，用户仍然可以手动调整窗口大小。
    root.geometry("320x120")
    return root


def build_hello_label(parent: tk.Tk) -> ttk.Label:
    """在 parent 容器中创建一个 ttk.Label，并把它放置到窗口里。

    注意：
    - 创建控件（ttk.Label(parent, ...)）和把控件“放到屏幕上”
      （pack / grid / place）是两个独立的步骤。只创建不布局，
      控件不会显示。
    """
    label = ttk.Label(
        parent,
        text="Hello, Tkinter + ttk!",
        anchor="center",
        padding=20,
    )
    label.pack(fill="both", expand=True)
    return label


def main() -> None:
    root = build_root_window()
    build_hello_label(root)

    # mainloop() 是整个程序的“心脏”：
    # 它会反复做三件事，直到窗口被关闭：
    #   1. 等待操作系统的输入事件（鼠标点击、键盘按键、窗口刷新请求……）。
    #   2. 把事件分发给对应的 Tkinter 控件处理。
    #   3. 执行控件绑定的回调函数（这里还没有任何回调）。
    # 在调用 mainloop() 之前写的所有代码只是“搭建场景”，
    # 真正让窗口显示、响应点击的，是这一行代码。
    root.mainloop()

    # 下面这行代码只有在 mainloop() 返回之后（即窗口被关闭后）才会执行。
    print("窗口已关闭，mainloop() 已退出，程序即将结束。")


if __name__ == "__main__":
    main()
```

---

## E. Lifecycle

1. **启动**：Python 解释器执行 `main()`。
2. **创建世界**：`tk.Tk()` 启动内嵌的 Tcl 解释器，创建操作系统窗口。
3. **搭建场景**：创建 `ttk.Label` 并 `pack()`，此时窗口已经可以显示内容，但事件循环还没开始。
4. **进入循环**：`root.mainloop()` 被调用，程序主线程进入"等待事件 → 分发 → 执行回调"的循环，这一行**不会返回**，直到窗口被关闭（点击标题栏的关闭按钮，或代码里调用了 `root.destroy()` / `root.quit()`）。
5. **退出**：用户关闭窗口 → Tk 内部产生销毁事件 → `mainloop()` 返回 → `main()` 继续执行 `mainloop()` 之后的代码（这里是打印一行日志）→ 函数返回 → 进程退出。

---

## F. Key APIs

| API | 作用 | 输入 | 输出 / 副作用 | 常见错误 |
|---|---|---|---|---|
| `tk.Tk()` | 创建根窗口 + 内嵌 Tcl 解释器 | 无必填参数 | 返回根窗口对象；若无图形环境则抛出 `TclError` | 在同一进程里创建多个 `Tk()` 实例（应该只有一个，多窗口用 `Toplevel`） |
| `root.title(text)` | 设置窗口标题栏文字 | `str` | 修改窗口标题 | 忘记调用，窗口标题显示为默认的脚本名或 `tk` |
| `root.geometry(spec)` | 设置窗口初始大小/位置 | 形如 `"320x120"` 或 `"320x120+100+50"` 的字符串 | 请求窗口管理器以该尺寸显示 | 把它当作"锁定大小"，实际上用户仍可拖动调整（除非额外调用 `resizable(False, False)`） |
| `ttk.Label(parent, **opts)` | 创建一个文本/图片展示控件 | `parent` 为父容器；`text`、`anchor`、`padding` 等 | 返回 Label 对象（尚未显示，需要布局） | 只创建不调用 `pack/grid/place`，导致“写了代码但窗口里什么都没有” |
| `widget.pack(**opts)` | 一种布局管理器，把控件加入父容器的“打包队列” | `fill`、`expand`、`side` 等 | 控件出现在窗口里 | 和 `grid()` 混用在同一父容器的直接子控件上（见 Layer 1） |
| `root.mainloop()` | 启动事件循环 | 无 | 阻塞当前线程，直到窗口关闭 | 忘记调用；或者在它之后写了“应该在运行期执行”的代码（那段代码只会在窗口关闭后才跑） |

---

## G. Common Failure

### G.1 窗口没有任何反应就一闪而过 / 根本没出现

- **现象**：运行脚本后没有看到任何窗口。
- **根本原因**：代码里漏掉了 `root.mainloop()`，或者 `mainloop()` 调用之后紧跟着 `root.destroy()`（脚本执行完就销毁了窗口）。
- **定位方法**：检查 `main()` 的最后一行是不是 `root.mainloop()`；在它之前打印一行日志确认代码确实执行到了这一步。
- **修复方案**：确保 `mainloop()` 是函数的最后一条“有意义”的语句，且没有在它之前意外调用销毁窗口的方法。

### G.2 本地没有图形环境（headless / SSH 无 X11 转发）

- **现象**：报错 `_tkinter.TclError: no display name and no $DISPLAY environment variable`（Linux），或在 CI/容器里直接崩溃。
- **根本原因**：Tk 需要连接一个真实的显示服务器（X11 / Wayland XWayland / Win32 GDI / macOS Aqua）。SSH 远程终端默认不转发图形界面，Docker 容器默认没有显示服务器。
- **定位方法**：在终端执行 `echo $DISPLAY`（Linux），为空说明没有可用的显示环境。
- **修复方案**：
  - Linux：`ssh -X user@host` 开启 X11 转发，或在服务器上安装并启动 `Xvfb :1` 虚拟显示（仅用于自动化测试，看不到真实画面）。
  - 本地直接在桌面环境终端运行即可，无需额外配置。
  - macOS/Windows：直接在本机终端运行即可。

### G.3 `python -m tkinter` 自检失败

- **现象**：运行 `python3 -m tkinter` 没有弹出官方自带的测试窗口，而是报错。
- **根本原因**：常见于 Linux 发行版把 Python 和 Tk 支持拆成了两个包（例如 Debian/Ubuntu 的 `python3` 不自带 `python3-tk`），或 macOS 系统自带的 Python 没有链接到 Tk 框架。
- **定位方法**：
  ```bash
  python3 -c "import tkinter; print(tkinter.TkVersion)"
  ```
  如果这一步报 `ModuleNotFoundError: No module named 'tkinter'`，说明是缺包问题，不是显示环境问题。
- **修复方案**：
  - Debian/Ubuntu：`sudo apt install python3-tk`
  - Fedora/RHEL：`sudo dnf install python3-tkinter`
  - macOS（Homebrew Python）：`brew install python-tk`，或改用 python.org 官方安装包
  - Windows：官方安装包默认自带 Tk，一般不需要额外安装

---

## H. Exercises

1. **基础修改**：把窗口标题改成你自己的名字，把 `geometry` 改成 `480x200`，并在 `ttk.Label` 下面再放一个 `ttk.Label` 显示当前 Tkinter 版本（提示：`tkinter.TkVersion`）。预期结果：窗口变大，显示两行文字。
2. **工程向练习**：给窗口加上 `root.resizable(False, False)` 禁止用户调整大小，并观察调整窗口大小的手柄是否消失；然后反过来，去掉这一行，尝试拖动窗口边缘，观察 `ttk.Label` 的 `fill="both", expand=True` 是如何让文字跟着窗口一起变化位置的。预期结果：你能用一句话解释“固定大小窗口”和“可缩放窗口 + fill/expand”的区别。

---

## I. Summary

本层建立的 Mental Model：

- Tkinter 不是独立 GUI 引擎，而是 Python 对 Tcl/Tk 的绑定；`ttk` 是其中的主题化控件集，日常开发优先使用。
- `tk.Tk()` 创建“世界”，Widget 是“世界里的物件”，`mainloop()` 让这个世界开始响应事件——三者是递进关系，缺一不可。
- 创建控件和布局控件是两个独立步骤，这个区分会在 Layer 1 进一步展开。
- GUI 程序的本质是事件循环，这是贯穿本教程 Layer 0~8 的核心主线。

下一步：[Layer 1：Widget Tree 与布局管理](../01-layout-widget-tree/README.md)。
