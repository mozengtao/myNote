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
