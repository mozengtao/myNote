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
