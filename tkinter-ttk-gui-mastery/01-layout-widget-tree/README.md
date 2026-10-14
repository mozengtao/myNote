# Layer 1：Widget Tree 与布局管理

> 对应示例代码：[examples/01_layout_demo.py](examples/01_layout_demo.py)

---

## A. Why

Layer 0 只放了一个 Label，不需要考虑“多个控件怎么摆放”。但真实界面必然有多个控件、多层嵌套。这就带来两个必须分开理解的问题：

1. **控件之间的父子关系是什么？**（决定了“谁包含谁”“销毁父控件时子控件怎么办”）
2. **控件在屏幕上具体摆在哪、占多大空间？**（这是“布局”，和第 1 点是完全不同的关注点）

很多初学者的困惑（“为什么我 `grid()` 和 `pack()` 混用就报错/布局乱了”）本质上都是没有把这两件事分开。

---

## B. Mental Model

### B.1 Widget Tree：一棵对象树，不是“屏幕坐标列表”

```text
root (tk.Tk)
├── toolbar (ttk.Frame)
│    ├── Button "New"
│    ├── Button "Open"
│    ├── Button "Save"
│    └── Label "(hint)"
├── form (ttk.LabelFrame)
│    ├── Label "设备地址"   Entry
│    ├── Label "用户名"     Entry
│    └── Label "备注"       Entry
└── status (ttk.Frame)
     └── Label "状态：就绪"
```

每一个 Widget 在创建时（`ttk.Frame(root, ...)`）第一个参数永远是它的 **parent**。这就形成了一棵树：
- `root` 是根。
- `toolbar`、`form`、`status` 是 `root` 的子节点。
- 按钮、输入框是 `toolbar` / `form` 的子节点。

这棵树**只描述“包含关系”**，不描述“谁在屏幕上面、谁在下面”——屏幕位置是布局管理器的职责，不是 Widget Tree 本身决定的。

### B.2 Widget Tree 与 Layout Manager 的关系

```text
Widget Tree（谁是谁的子控件）
        |
        | 每一层父容器，独立决定“用哪种布局管理器摆放它的直接子控件”
        v
Layout 决策（这一层用 pack，还是 grid，还是 place）
        |
        | 布局管理器计算出具体的屏幕坐标和尺寸
        v
实际渲染位置
```

关键认知：**布局管理器不是全局唯一的一种选择，而是“每个容器各自决定”**。本例中：
- `root` 对它的三个直接子 Frame（`toolbar` / `form` / `status`）使用 `pack()`（垂直堆叠）。
- `form` 对它内部的 Label/Entry 使用 `grid()`（表格对齐）。
- `toolbar` 对它内部的按钮使用 `pack(side="left")`（水平排列）。

这三个布局决策互不干扰，因为它们作用于不同的父容器。

### B.3 `pack()` / `grid()` / `place()` 的设计意图

| 布局管理器 | 设计意图 | 典型场景 |
|---|---|---|
| `pack()` | 按“边”（上/下/左/右）依次堆叠子控件，一维排列思路 | 工具条、状态条、简单的垂直/水平排列 |
| `grid()` | 把容器当成一张表格，用行列坐标摆放子控件 | 表单（标签+输入框对齐）、仪表盘式布局 |
| `place()` | 用绝对坐标或相对比例精确摆放 | 极少数需要像素级精确定位的场景（例如叠加在图片上的浮层控件） |

工程经验：**90% 以上的日常界面只需要 `pack()` 和 `grid()`，`place()` 应当是最后选择**，因为它放弃了布局管理器“自动适应窗口大小变化”的能力，窗口缩放时容易出现控件错位或被遮挡。

### B.4 为什么“一个容器内不要混用 pack 和 grid”

`pack()` 和 `grid()` 都会向同一个父容器的几何管理槽位（geometry manager slot）注册自己，Tkinter 底层对同一个容器只允许激活**一种**几何管理器。如果你在同一个父容器的直接子控件上，一部分用 `pack()`、一部分用 `grid()`，会导致：
- Tkinter 抛出 `_tkinter.TclError: cannot use geometry manager grid inside ... which already has slaves packed`。
- 或者两套布局互相覆盖，出现无法预测的空白/重叠。

**解决思路不是“小心混用”，而是“从设计上避免混用”**：如果一部分子控件想用 pack、另一部分想用 grid，就把它们分别放进两个不同的子 Frame 里，每个 Frame 内部各自统一使用一种布局管理器（这正是本例的做法：`toolbar` 用 pack，`form` 用 grid，它们是两个不同的容器）。

---

## C. Architecture

```text
核心对象              关系
root (Tk)             Widget Tree 的根节点
toolbar (Frame)       root 的子节点；内部用 pack() 排列按钮
form (LabelFrame)     root 的子节点；内部用 grid() 排列 Label+Entry
status (Frame)        root 的子节点；内部用 pack() 排列状态文字
```

`columnconfigure(col, weight=N)` / `rowconfigure(row, weight=N)` 是 `grid()` 布局里专属的“空间分配权重”配置，`weight` 越大，窗口变化时该行/列分配到的多余空间越多；`weight=0`（默认）表示该行/列不参与多余空间的分配，只保持内容所需的最小尺寸。

---

## D. Complete Code

```python
"""
01_layout_demo.py

Layer 1：Widget Tree 与布局管理（pack / grid）。

运行方式：
    python3 examples/01_layout_demo.py

本例演示：
    1. 一个窗口内可以有多个 Frame（容器），每个 Frame 内部的布局互不干扰。
    2. 顶部工具条 Frame 用 pack() 排列（简单的一维排列场景）。
    3. 中间表单 Frame 用 grid() 排列（需要“对齐成表格”的场景）。
    4. 底部状态条 Frame 用 pack() 排列。
    5. 窗口可以被用户拖动调整大小，中间表单区域会跟着拉伸。

重要约束（本文件严格遵守）：
    - 同一个容器内的直接子控件，只使用一种布局管理器（不混用 pack/grid）。
    - 不同容器（Frame）之间可以分别选择不同的布局管理器。
"""

from __future__ import annotations

import tkinter as tk
from tkinter import ttk


def build_toolbar(root: tk.Tk) -> ttk.Frame:
    """顶部工具条：从左到右依次排列的几个按钮，适合用 pack(side=LEFT)。"""
    toolbar = ttk.Frame(root, padding=(8, 6))
    # 工具条内部的直接子控件统一使用 pack()。
    toolbar.pack(side="top", fill="x")

    for text in ("New", "Open", "Save"):
        btn = ttk.Button(toolbar, text=text)
        btn.pack(side="left", padx=(0, 6))

    hint = ttk.Label(toolbar, text="(工具条使用 pack 从左到右排列)")
    hint.pack(side="left", padx=(12, 0))
    return toolbar


def build_form(root: tk.Tk) -> ttk.Frame:
    """中间表单区域：标签 + 输入框两列对齐，适合用 grid()。

    grid() 的核心价值：让多行控件按“列”整齐对齐，这是 pack() 很难自然
    做到的（pack 是一维排列思路，grid 是二维表格思路）。
    """
    form = ttk.LabelFrame(root, text="设备信息（grid 布局演示）", padding=12)
    form.pack(side="top", fill="both", expand=True, padx=12, pady=8)

    # columnconfigure(1, weight=1) 的意思是：
    # “当容器变宽时，多出来的空间全部分配给第 1 列（输入框所在列）”，
    # 这样标签列保持紧凑，输入框列会随窗口拉伸——这正是“布局管理器负责
    # 空间分配”的直接体现。
    form.columnconfigure(0, weight=0)
    form.columnconfigure(1, weight=1)

    fields = ("设备地址", "用户名", "备注")
    for row, label_text in enumerate(fields):
        label = ttk.Label(form, text=label_text)
        # sticky="w"：标签在格子里靠左对齐（west）。
        label.grid(row=row, column=0, sticky="w", padx=(0, 8), pady=4)

        entry = ttk.Entry(form)
        # sticky="ew"：输入框左右两边都贴紧格子边缘，这样它才会随列宽变化而拉伸。
        entry.grid(row=row, column=1, sticky="ew", pady=4)

    # 让最后一行在垂直方向也能吃掉多余空间，演示 rowconfigure 的作用。
    form.rowconfigure(len(fields), weight=1)
    return form


def build_statusbar(root: tk.Tk) -> ttk.Frame:
    """底部状态条：单个 Label 占满宽度，适合用 pack(fill="x")。"""
    status = ttk.Frame(root, padding=(8, 4))
    status.pack(side="bottom", fill="x")

    label = ttk.Label(status, text="状态：就绪", anchor="w")
    label.pack(side="left", fill="x", expand=True)
    return status


def build_root_window() -> tk.Tk:
    root = tk.Tk()
    root.title("Layout Demo - pack + grid")
    root.geometry("420x260")
    root.minsize(360, 220)
    return root


def main() -> None:
    root = build_root_window()

    # 三个 Frame 都是 root 的直接子控件，它们之间用 pack() 做“垂直堆叠”：
    # 工具条在最上面，状态条钉在最下面，表单区域填满中间剩余空间。
    # 这一层 pack() 和 Frame 内部各自的布局管理器互不影响。
    build_toolbar(root)
    build_statusbar(root)
    build_form(root)

    root.mainloop()


if __name__ == "__main__":
    main()
```

---

## E. Lifecycle

1. 创建 `root`。
2. 依次调用 `build_toolbar` / `build_statusbar` / `build_form`，每次调用都会：创建若干 Widget → 把它们挂到 Widget Tree 上（指定 parent）→ 调用布局方法把它们放到屏幕上。
3. 注意调用顺序：代码里先 `build_toolbar`、再 `build_statusbar`、最后 `build_form`，但 `pack()` 的 `side="top"` / `side="bottom"` 已经把工具条和状态条“钉”在上下两端，`form` 用 `side="top"` 填充剩余空间——**pack 的堆叠顺序由 side 参数和调用顺序共同决定**，这是一个常见的练习点（见 Exercises）。
4. `root.mainloop()` 启动事件循环，此时窗口已完整显示，用户拖动窗口边缘调整大小时，`form` 区域会跟着拉伸（因为 `fill="both", expand=True` + `grid` 的列权重配置）。
5. 关闭窗口，`mainloop()` 返回，程序结束。

---

## F. Key APIs

| API | 作用 | 输入 | 输出 / 副作用 | 常见错误 |
|---|---|---|---|---|
| `ttk.Frame(parent, **opts)` | 创建一个无边框容器，纯粹用于组织布局 | `padding` 等 | 返回 Frame 对象 | 把 Frame 当作“必须有视觉边框”的东西，其实它默认是透明容器；需要边框/标题用 `ttk.LabelFrame` |
| `widget.pack(**opts)` | 按边堆叠的布局管理器 | `side`、`fill`、`expand`、`padx`、`pady` | 控件出现在父容器的指定边 | 在同一容器内和 `grid()` 混用（见 B.4） |
| `widget.grid(**opts)` | 表格布局管理器 | `row`、`column`、`sticky`、`padx`、`pady` | 控件出现在指定的行列格子里 | 忘记设置 `sticky`，控件在格子里居中，不随列宽拉伸；行列号跳跃或重复导致意外空行 |
| `container.columnconfigure(index, weight=N)` | 配置 grid 布局中某一列的空间分配权重 | 列索引、权重 | 窗口变化时按权重比例分配多余宽度 | 只给内容列设权重，忘记给标签列设 `weight=0`（其实默认就是 0，但显式写出更清晰） |
| `container.rowconfigure(index, weight=N)` | 配置 grid 布局中某一行的空间分配权重 | 行索引、权重 | 窗口变化时按权重比例分配多余高度 | 同上，行版本 |
| `root.minsize(w, h)` | 设置窗口可缩小的最小尺寸 | 宽、高（像素） | 用户无法把窗口拖得比这个尺寸更小 | 设得比内容实际需要的尺寸还小，导致控件被压缩变形 |

---

## G. Common Failure

### G.1 `pack()` 与 `grid()` 混用报错

- **现象**：`_tkinter.TclError: cannot use geometry manager grid inside .frame which already has slaves packed`。
- **根本原因**：同一个父容器的直接子控件，一部分用了 `pack()`，另一部分又用了 `grid()`。
- **定位方法**：报错信息里会直接给出冲突的容器路径（如 `.frame`），回头检查这个容器下所有子控件用的布局方法。
- **修复方案**：把需要不同布局方式的子控件拆到两个独立的子 Frame 里，每个子 Frame 内部统一一种布局管理器（本例的 `toolbar` vs `form` 就是这种拆分）。

### G.2 窗口缩放时，某个区域不跟着变化

- **现象**：拖大窗口后，表单区域两侧出现大片空白，输入框没有跟着变宽。
- **根本原因**：忘记设置 `columnconfigure(col, weight=1)`，或者 Entry 的 `grid()` 忘了 `sticky="ew"`。weight 决定“谁吃掉多余空间”，`sticky` 决定“控件本身是否贴满格子”，两者缺一都不会拉伸。
- **定位方法**：先确认容器层级的 `columnconfigure/rowconfigure` 权重是否设置；再确认控件自身的 `sticky` 是否包含对应方向。
- **修复方案**：按需添加 `columnconfigure`/`rowconfigure` 权重，并给需要拉伸的控件加上对应方向的 `sticky`。

### G.3 子控件“消失”了（其实是被另一个子控件挤没了）

- **现象**：代码里明明创建了 5 个 Label，窗口里却只看到 1 个。
- **根本原因**：`pack()` 多个子控件都用了 `expand=True, fill="both"` 且没有区分 `side`，导致后创建的控件把先创建的控件挤到了尺寸为 0 的区域；或者 `grid()` 时多个控件填了相同的 `row, column` 导致互相覆盖。
- **定位方法**：逐个检查每个控件的 `grid(row=, column=)` 坐标是否唯一；或者临时给每个控件设置不同的背景色（`style` 或 `tk.Label(bg=...)`）直观看出谁盖住了谁。
- **修复方案**：`grid` 场景确保行列坐标不重复；`pack` 场景按需控制 `expand`，通常只给“希望吃掉剩余空间”的那一个控件设置 `expand=True`。

---

## H. Exercises

1. **基础修改**：把 `build_statusbar` 的调用顺序移到 `build_form` 之后（即先 `toolbar` → `form` → `status`），重新观察窗口外观是否发生变化。预期结果：你能解释清楚——对 `pack()` 来说，决定“钉在哪条边”的是 `side` 参数，而不是调用顺序本身（但同一条边上有多个控件时，调用顺序决定了堆叠先后）。
2. **工程向练习**：在 `form` 里新增一行“端口号”，并给这一行的 Entry 加上 `width=8`（限制固定宽度，不随窗口拉伸），同时保持其他 Entry 继续跟随窗口拉伸。预期结果：窗口变宽时，“设备地址/用户名/备注”输入框变宽，“端口号”输入框保持固定宽度——这是真实工程表单里很常见的需求（短字段不需要被拉得很长）。

---

## I. Summary

本层新增的 Mental Model：

- Widget Tree 描述“包含关系”，布局管理器决定“屏幕位置”，这是两个独立维度，必须分开思考。
- 布局决策是“按容器”做出的，不是全局唯一选择；不同容器完全可以分别使用 pack/grid。
- “不要在同一容器内混用 pack/grid”不是一条孤立规则，而是上面两条认知的直接推论——如果你发现自己想在同一容器里混用，说明这些子控件应该先被拆进不同的子 Frame。

与 Layer 0 的关系：Layer 0 的 `mainloop()` 让窗口“活起来”；Layer 1 解决了“活起来之后，窗口里的东西怎么摆放”。下一层要解决的是“摆放好之后，用户点击/输入，程序怎么知道”。

下一步：[Layer 2：事件系统与回调](../02-event-callback/README.md)。
