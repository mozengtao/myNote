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
