# Layer 3：控件与变量绑定

> 对应示例代码：[examples/03_form_binding.py](examples/03_form_binding.py)

---

## A. Why

Layer 2 里我们用 `self.result_var.set(...)` 更新了一个 Label，但没有解释清楚：为什么改一个 `StringVar` 的值，Label 的文字就自动变了？如果界面上有 5 个控件都要显示/编辑同一份状态，难道要手写 5 遍 `widget.get()` / `widget.insert()` 来回同步吗？

这一层要解决的核心问题：**Tkinter 提供了一种“变量对象”，让多个控件可以共享同一份状态，并且互相自动保持同步**，这是构建任何表单类界面的基础设施。

---

## B. Mental Model

### B.1 普通 Python 变量 vs Tkinter Variable

```text
普通 Python 变量：
    name = "abc"       # 只是一块内存里的值
    entry.get()         # 要读 Entry 当前内容，必须主动调用方法查询
    entry.insert(...)   # 要改 Entry 显示内容，必须主动调用方法修改
    —— 变量和控件之间没有任何自动联系，同步靠你手写代码。

Tkinter Variable：
    name_var = tk.StringVar(value="abc")
    entry = ttk.Entry(parent, textvariable=name_var)
    —— name_var 和 entry 被“接上线”了：
       用户在 entry 里打字 => name_var 的值自动更新
       代码里 name_var.set("xyz") => entry 显示内容自动变成 "xyz"
```

一句话 Mental Model：

> **`StringVar`/`IntVar`/`BooleanVar` 不是“存了一个值的容器”，而是一个带有观察者机制的共享状态节点。任何控件把自己的 `textvariable`/`variable` 参数指向同一个 Variable 实例，就等于订阅了这个节点——谁改了它的值，所有订阅者（包括用户的输入动作）都会同步。**

这和你熟悉的"响应式/观察者模式"是一回事：Variable 是被观察的主体（Subject），控件是观察者（Observer）。Tk 内部通过 `trace`机制实现这种自动同步（不需要你手写回调去同步，Tk 已经把这层接好了）。

### B.2 本例的数据流

```text
用户操作 Entry/Checkbutton/Radiobutton/Combobox
            |
            | Tk 自动写入
            v
   StringVar / BooleanVar  (UI State)
            |
            | 点击“提交”按钮，collect_form_result() 读取
            v
   校验（ValidationError）
            |
            v
   FormResult (业务数据，与 Tkinter 完全无关)
            |
            v
   summary_var.set(...)  →  Label 自动刷新显示汇总
```

### B.3 UI State 与业务数据的边界

`StringVar`/`BooleanVar` 这些变量对象只负责"界面当前显示/编辑的内容"，它们天然依赖 Tkinter（没有 Tk 根窗口，你创建不了 `tk.StringVar()`）。而 `FormResult` 是一个普通的 `@dataclass`，不 import 任何 Tkinter 相关的东西。

这个边界非常重要：**`collect_form_result()` 这个方法本身就是这条边界线**——它是整段代码里唯一既接触 UI State（读 `*.get()`）又产出业务数据（返回 `FormResult`）的地方。往后无论是把界面从 Tkinter 换成 Web 表单，还是要给这份数据加自动化测试，business 逻辑（校验规则、`FormResult` 结构）都不需要改一行。

---

## C. Architecture

| 对象 | 角色 |
|---|---|
| `self.device_name_var` (`tk.StringVar`) | 和 Entry 的 `textvariable` 绑定，承载"设备名称"输入 |
| `self.enabled_var` (`tk.BooleanVar`) | 和 Checkbutton 的 `variable` 绑定，承载勾选状态 |
| `self.test_mode_var` (`tk.StringVar`) | 两个 Radiobutton 共享同一个 `variable`，谁被选中由它们各自的 `value=` 决定 |
| `self.device_type_var` (`tk.StringVar`) | 和 Combobox 的 `textvariable` 绑定，承载下拉选中的值 |
| `FormResult` (dataclass) | 纯业务数据结构，与 Tkinter 无关 |
| `ValidationError` | 校验失败时的异常类型，携带给用户看的中文提示 |
| `collect_form_result()` | UI State → 业务数据的转换与校验入口（见 B.3） |

---

## D. Complete Code

```python
"""
03_form_binding.py

Layer 3：控件与变量绑定（StringVar / IntVar / BooleanVar）。

运行方式：
    python3 examples/03_form_binding.py

本例构建一个完整表单：
    - Entry（设备名称，文本输入）
    - Checkbutton（是否启用，布尔值）
    - Radiobutton（测试模式：quick / full，单选）
    - Combobox（设备类型，下拉选择）

所有 UI 状态都保存在 Tkinter Variable 对象里（StringVar/IntVar/BooleanVar），
不使用裸的 Python 变量去“手动同步”界面状态。提交时做校验，校验失败时在界面
提示具体原因；校验通过后把 Variable 中的值转换成一份独立的“业务数据”
（FormResult），展示汇总结果；并提供“重置”按钮恢复默认值。
"""

from __future__ import annotations

import tkinter as tk
from dataclasses import dataclass
from tkinter import ttk


# ---------------------------------------------------------------------------
# 业务数据：和 UI 状态是两件不同的事情。
#
# UI 状态（StringVar 等）只服务于“界面当前显示什么”；FormResult 才是真正
# 要往下传递给业务逻辑/服务层的数据。这个边界在小例子里看起来多余，但在
# 真实工程里非常重要：UI 状态可以随便加字段改展示方式，不应该牵连到
# 业务逻辑的数据结构。
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class FormResult:
    device_name: str
    enabled: bool
    test_mode: str
    device_type: str


class ValidationError(Exception):
    """表单校验失败时抛出，message 是给用户看的中文提示。"""


DEVICE_TYPES = ("CM", "CMTS", "RPD", "OLT")
DEFAULT_DEVICE_NAME = ""
DEFAULT_ENABLED = True
DEFAULT_TEST_MODE = "quick"
DEFAULT_DEVICE_TYPE = DEVICE_TYPES[0]


class FormApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root

        # --- UI 状态：全部用 Tkinter Variable，而不是普通 Python 变量 ---
        # 普通变量（例如 self.device_name = ""）只是一个值，Entry 并不知道
        # 它存在，你需要手动 entry.get()/entry.insert() 来回同步。
        # Variable 对象不同：它在 Tk 内部维护一份“带观察者机制”的值，
        # 任何控件把 textvariable/variable 指向同一个 Variable，就会自动
        # 和它保持同步——改 Variable，控件界面跟着变；改控件（比如用户在
        # Entry 里打字），Variable 的值也会立刻同步，不需要你写一行同步代码。
        self.device_name_var = tk.StringVar(value=DEFAULT_DEVICE_NAME)
        self.enabled_var = tk.BooleanVar(value=DEFAULT_ENABLED)
        self.test_mode_var = tk.StringVar(value=DEFAULT_TEST_MODE)
        self.device_type_var = tk.StringVar(value=DEFAULT_DEVICE_TYPE)

        self.summary_var = tk.StringVar(value="（尚未提交）")
        self.error_var = tk.StringVar(value="")

        self._build_widgets()

    # ------------------------------------------------------------------
    # UI 构建
    # ------------------------------------------------------------------
    def _build_widgets(self) -> None:
        container = ttk.Frame(self.root, padding=16)
        container.pack(fill="both", expand=True)
        container.columnconfigure(1, weight=1)

        row = 0
        ttk.Label(container, text="设备名称：").grid(row=row, column=0, sticky="w", pady=4)
        ttk.Entry(container, textvariable=self.device_name_var).grid(
            row=row, column=1, sticky="ew", pady=4
        )

        row += 1
        ttk.Checkbutton(
            container,
            text="启用该设备",
            variable=self.enabled_var,
        ).grid(row=row, column=0, columnspan=2, sticky="w", pady=4)

        row += 1
        mode_frame = ttk.LabelFrame(container, text="测试模式", padding=8)
        mode_frame.grid(row=row, column=0, columnspan=2, sticky="ew", pady=4)
        ttk.Radiobutton(
            mode_frame, text="快速测试 (quick)", value="quick", variable=self.test_mode_var
        ).pack(side="left", padx=(0, 12))
        ttk.Radiobutton(
            mode_frame, text="完整测试 (full)", value="full", variable=self.test_mode_var
        ).pack(side="left")

        row += 1
        ttk.Label(container, text="设备类型：").grid(row=row, column=0, sticky="w", pady=4)
        combo = ttk.Combobox(
            container,
            textvariable=self.device_type_var,
            values=DEVICE_TYPES,
            state="readonly",  # 只允许从下拉列表选择，不允许手动输入任意文本
        )
        combo.grid(row=row, column=1, sticky="ew", pady=4)

        row += 1
        btn_row = ttk.Frame(container)
        btn_row.grid(row=row, column=0, columnspan=2, sticky="ew", pady=(12, 4))
        ttk.Button(btn_row, text="提交", command=self.on_submit).pack(side="left")
        ttk.Button(btn_row, text="重置", command=self.on_reset).pack(side="left", padx=(8, 0))

        row += 1
        ttk.Label(container, textvariable=self.error_var, foreground="red").grid(
            row=row, column=0, columnspan=2, sticky="w"
        )

        row += 1
        ttk.Label(
            container, textvariable=self.summary_var, foreground="blue", wraplength=320
        ).grid(row=row, column=0, columnspan=2, sticky="w", pady=(8, 0))

    # ------------------------------------------------------------------
    # 业务逻辑：读取 Variable → 校验 → 转成 FormResult → 展示
    # ------------------------------------------------------------------
    def collect_form_result(self) -> FormResult:
        """从 UI 状态（Variable）读取当前值，做校验，组装成业务数据。

        这是“UI State 与业务数据边界”的具体体现：这个函数是边界本身——
        它的输入是 Variable.get()，输出是一个与 Tkinter 完全无关的
        FormResult 对象，之后的业务逻辑（哪怕换成 Web 表单）都可以直接
        复用同一个 FormResult 结构和同一套校验规则。
        """
        device_name = self.device_name_var.get().strip()
        if not device_name:
            raise ValidationError("设备名称不能为空。")
        if len(device_name) > 32:
            raise ValidationError("设备名称长度不能超过 32 个字符。")

        device_type = self.device_type_var.get()
        if device_type not in DEVICE_TYPES:
            raise ValidationError(f"未知设备类型：{device_type!r}。")

        return FormResult(
            device_name=device_name,
            enabled=self.enabled_var.get(),
            test_mode=self.test_mode_var.get(),
            device_type=device_type,
        )

    def on_submit(self) -> None:
        self.error_var.set("")
        try:
            result = self.collect_form_result()
        except ValidationError as exc:
            self.error_var.set(f"校验失败：{exc}")
            self.summary_var.set("（尚未提交）")
            return

        self.summary_var.set(
            "提交成功：\n"
            f"  设备名称 = {result.device_name}\n"
            f"  启用状态 = {'是' if result.enabled else '否'}\n"
            f"  测试模式 = {result.test_mode}\n"
            f"  设备类型 = {result.device_type}"
        )

    def on_reset(self) -> None:
        """重置：把每个 Variable 设回默认值。

        因为控件是通过 textvariable/variable 绑定到这些 Variable 的，
        这里只需要修改 Variable 本身，所有控件会自动刷新显示——不需要
        手动调用 entry.delete()/combo.set() 之类的控件方法。
        """
        self.device_name_var.set(DEFAULT_DEVICE_NAME)
        self.enabled_var.set(DEFAULT_ENABLED)
        self.test_mode_var.set(DEFAULT_TEST_MODE)
        self.device_type_var.set(DEFAULT_DEVICE_TYPE)
        self.summary_var.set("（尚未提交）")
        self.error_var.set("")


def build_root_window() -> tk.Tk:
    root = tk.Tk()
    root.title("Form Binding Demo")
    root.geometry("420x380")
    return root


def main() -> None:
    root = build_root_window()
    FormApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
```

---

## E. Lifecycle

1. `FormApp.__init__` 创建 4 个承载 UI 状态的 Variable（都给了默认值），再创建 `summary_var`/`error_var` 用于展示结果和错误。
2. `_build_widgets` 创建控件时，把每个控件的 `textvariable`/`variable` 指向对应的 Variable——从这一刻起，控件和 Variable 开始自动同步。
3. 用户操作表单（打字、勾选、选单选项、选下拉项），对应的 Variable 自动更新，这个过程**不执行任何我们写的回调**，纯粹是 Tk 内部的 trace 机制。
4. 用户点击“提交”：`on_submit` 清空错误提示 → 调用 `collect_form_result()` 读取所有 Variable 当前值并校验 → 校验失败则把异常信息写进 `error_var`（界面红字提示）；校验成功则组装 `FormResult` 并把汇总写进 `summary_var`（界面蓝字展示）。
5. 用户点击“重置”：把所有 Variable 设回默认值，依赖 B.1 所述的自动同步机制，所有控件界面瞬间恢复初始状态，不需要逐个调用控件方法。

---

## F. Key APIs

| API | 作用 | 输入 | 输出 / 副作用 | 常见错误 |
|---|---|---|---|---|
| `tk.StringVar(value=...)` | 创建字符串类型的变量对象 | 初始值（可选） | 可用 `.get()`/`.set()` 读写，可绑定到 `textvariable` | 在没有 `tk.Tk()` 实例存在之前创建（会报 `RuntimeError: Too early to create variable`） |
| `tk.BooleanVar(value=...)` | 创建布尔类型的变量对象 | 初始值（可选） | `.get()` 返回 `bool` | 以为它和 Python `bool` 完全等价，直接做 `is True` 之类的身份比较 |
| `ttk.Entry(parent, textvariable=var)` | 把 Entry 的文本内容绑定到一个 StringVar | `textvariable` | 用户输入自动写入 `var`，`var.set()` 自动更新显示 | 同时又手动调用 `entry.insert()`/`entry.delete()` 去“同步”，和 Variable 产生双重管理 |
| `ttk.Checkbutton(parent, variable=var)` | 把勾选状态绑定到一个 BooleanVar（或 IntVar） | `variable`，可选 `onvalue`/`offvalue` | 勾选/取消自动写入 `var` | 多个 Checkbutton 共享了同一个 `variable`，导致它们变成“只能同时勾选/取消”（这其实是 Radiobutton 的行为，Checkbutton 场景下几乎总是各自独立的 Variable） |
| `ttk.Radiobutton(parent, variable=var, value=v)` | 多个 Radiobutton 共享同一个 Variable，`value` 决定被选中时写入什么 | `variable` 共享，`value` 各自不同 | 选中某个 Radiobutton 时，`var` 被设为该按钮的 `value` | 几个 Radiobutton 的 `value` 写重复，导致选中状态无法区分 |
| `ttk.Combobox(parent, textvariable=var, values=[...], state="readonly")` | 下拉选择控件 | `values` 候选列表；`state="readonly"` 禁止手动输入 | 选中项自动写入 `var` | 不设置 `state="readonly"` 时，用户可以输入 `values` 之外的任意文本，校验时必须处理这种“野值” |

---

## G. Common Failure

### G.1 创建 Variable 时报 `RuntimeError: Too early to create variable: no default root window`

- **现象**：代码刚运行就在 `tk.StringVar(...)` 这一行崩溃。
- **根本原因**：Tkinter Variable 内部需要关联一个 Tcl 解释器实例，而这个实例来自 `tk.Tk()`。如果在创建 `tk.Tk()` 根窗口**之前**就创建 Variable（比如写在类属性/模块级别，而不是 `__init__` 里、且 `__init__` 在根窗口创建之后才被调用），就会报这个错。
- **定位方法**：检查 Variable 的创建时机，确认它发生在 `tk.Tk()` 之后（本例中体现为：`main()` 先 `build_root_window()` 得到 `root`，再传给 `FormApp(root)`，Variable 都在 `FormApp.__init__` 里创建）。
- **修复方案**：把 Variable 的创建放到根窗口创建之后，或显式传入 `master=root` 参数（如 `tk.StringVar(master=root, value=...)`）。

### G.2 改了 Variable 的值，界面却没有刷新

- **现象**：代码里 `self.summary_var.set(...)` 执行了，但对应 Label 文字没变。
- **根本原因**：最常见的是 Label 创建时**没有**使用 `textvariable=self.summary_var`，而是用了 `text="..."` 写了一个固定字符串；或者创建了两个不同的 StringVar 实例，代码改的是 A，Label 绑定的是 B。
- **定位方法**：在 `.set()` 调用后面立刻 `print(var.get())` 确认值确实改变了；再检查对应控件创建时参数是 `textvariable=` 还是 `text=`。
- **修复方案**：确保控件使用 `textvariable=`/`variable=` 绑定，并且绑定的是同一个 Variable 实例（不要在构建函数的局部变量和实例属性之间搞混）。

### G.3 Combobox 选中的值和校验逻辑“对不上”

- **现象**：`collect_form_result()` 里的 `if device_type not in DEVICE_TYPES` 判断，有时候明明界面上选的是候选项之一，却走进了“未知设备类型”的分支。
- **根本原因**：没有设置 `state="readonly"`，用户在 Combobox 里手动输入或粘贴了多余的空格/全角字符，导致 `.get()` 得到的字符串和候选元组里的值不完全相等。
- **定位方法**：`print(repr(self.device_type_var.get()))` 查看是否有看不见的多余字符。
- **修复方案**：生产级表单里对“必须是枚举值”的下拉框一律加 `state="readonly"`；即使加了，也建议在校验函数里对取到的值做一次 `.strip()` 防御。

---

## H. Exercises

1. **基础修改**：给表单新增一个“重试次数”字段，用 `tk.IntVar(value=1)` 承载，界面用 `ttk.Spinbox(parent, from_=1, to=10, textvariable=...)` 展示（`Spinbox` 是 `tk` 里的控件，`ttk` 也提供了同名的主题化版本 `ttk.Spinbox`）。在校验逻辑里加入“重试次数必须在 1~10 之间”的规则。预期结果：输入超出范围的值点击提交，`error_var` 显示对应的中文校验错误。
2. **工程向练习**：把 `collect_form_result()` 和 `FormResult`/`ValidationError` 这三者整体抽到一个独立模块（例如 `form_logic.py`），`FormApp` 只 import 并调用，不直接定义业务规则。然后写一段不依赖 Tkinter 的纯 Python 测试代码（可以是几个 `assert` 语句），直接构造假的 `FormResult` 校验分支逻辑是否正确。预期结果：你能在**完全不启动任何窗口**的情况下，验证表单校验规则是否正确——这正是"UI 与业务逻辑解耦"带来的直接好处（本教程后续 Layer 8 的 `test_service.py` 也遵循同样的组织方式）。

---

## I. Summary

本层新增的 Mental Model：

- Tkinter Variable 是"带自动同步能力的共享状态节点"，不是普通变量；多个控件绑定同一个 Variable 就能免写同步代码。
- UI State（Variable）与业务数据（`FormResult`）要刻意分开：前者服务于界面，后者服务于业务逻辑，`collect_form_result()` 这类函数就是两者之间唯一的转换边界。
- 校验失败用异常（`ValidationError`）而不是“返回错误码”，让 `on_submit` 的控制流更清晰（`try/except`），这个模式会在后面的后台任务异常处理（Layer 7）里再次出现。

与前序 Layer 的关系：Layer 2 讲了"事件怎么触发回调"，Layer 3 讲了"回调要读写的状态怎么和界面自动同步"。两者结合起来，才是一个完整表单的运作方式。下一层开始引入"多窗口/多页面"的组织方式。

下一步：[Layer 4：对话框、菜单与多页面](../04-dialog-menu-notebook/README.md)。
