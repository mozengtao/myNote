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
