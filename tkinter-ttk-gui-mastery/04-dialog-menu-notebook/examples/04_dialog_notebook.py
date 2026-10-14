"""
04_dialog_notebook.py

Layer 4：对话框、菜单与多页面（messagebox / filedialog / Notebook / Toplevel）。

运行方式：
    python3 examples/04_dialog_notebook.py

本例演示：
    1. 菜单栏：File -> Open...   Help -> About
    2. messagebox：About 对话框、打开文件失败时的错误提示
    3. filedialog：打开文件选择对话框，读取文本内容显示在页面里
    4. ttk.Notebook：至少两个页面（"文件内容" 页 / "运行日志" 页）
    5. Toplevel：独立的"设置"窗口，演示 transient()/grab_set() 的使用边界，
       以及关闭子窗口时如何正确释放（而不是重复创建新实例）
"""

from __future__ import annotations

import tkinter as tk
from tkinter import filedialog, messagebox
from tkinter import ttk

APP_TITLE = "Dialog + Notebook Demo"


class SettingsDialog(tk.Toplevel):
    """独立的设置窗口（Toplevel 的典型用法：功能相对独立、使用频率不高）。

    什么时候用 Toplevel，什么时候用 Notebook 页面：
        - Toplevel：这个界面在逻辑上是"主流程之外"的东西（设置、关于、
          某条记录的详情），用户大多数时间不需要同时看到它和主窗口。
        - Notebook：这些页面都属于"主流程的一部分"，用户经常需要在它们
          之间来回切换着看（本例的"文件内容"和"运行日志"）。
    """

    def __init__(self, master: tk.Tk, on_apply) -> None:
        super().__init__(master)
        self.title("设置")
        self.geometry("280x160")
        self.resizable(False, False)
        self._on_apply = on_apply

        # transient(master)：告诉窗口管理器"我是 master 的附属窗口"，
        # 效果包括：最小化主窗口时这个窗口跟着隐藏、任务栏通常不单独显示它。
        self.transient(master)

        self.theme_var = tk.StringVar(value="light")
        self._build_widgets()

        # grab_set()：把输入焦点"抓"到这个窗口，用户在关闭/处理完这个窗口
        # 之前，无法点击主窗口的其他控件——这就是"模态窗口"。
        #
        # 使用边界：
        #   - 只在"必须让用户先做出一个决定才能继续"的场景使用（比如这里的
        #     设置项需要用户点"应用"或关闭才算完成一次交互）。
        #   - 不要在"仅用于展示信息、不需要阻塞主流程"的窗口上使用
        #     grab_set()，否则会让用户感觉程序"卡住了"。
        #   - 必须保证用户总有办法关闭这个窗口（提供按钮或允许标题栏关闭按钮），
        #     否则会造成主窗口永久无法操作。
        self.grab_set()

        # 捕获窗口管理器的关闭请求（点击标题栏的 X），确保走统一的清理逻辑，
        # 而不是被当作"程序崩溃式"关闭。
        self.protocol("WM_DELETE_WINDOW", self.close)

    def _build_widgets(self) -> None:
        frame = ttk.Frame(self, padding=16)
        frame.pack(fill="both", expand=True)

        ttk.Label(frame, text="主题：").pack(anchor="w")
        for value, text in (("light", "浅色"), ("dark", "深色")):
            ttk.Radiobutton(
                frame, text=text, value=value, variable=self.theme_var
            ).pack(anchor="w")

        btn_row = ttk.Frame(frame)
        btn_row.pack(fill="x", pady=(16, 0))
        ttk.Button(btn_row, text="应用", command=self.apply_and_close).pack(side="left")
        ttk.Button(btn_row, text="取消", command=self.close).pack(side="left", padx=(8, 0))

    def apply_and_close(self) -> None:
        self._on_apply(self.theme_var.get())
        self.close()

    def close(self) -> None:
        """统一的关闭入口：释放 grab，再销毁窗口。

        释放顺序很重要：先 grab_release() 把输入焦点还给主窗口，再
        destroy() 销毁这个 Toplevel 对象本身。如果颠倒顺序或忘记
        grab_release()，某些平台上主窗口会在子窗口销毁后仍然"接收不到"
        点击（表现为主窗口像是卡住了）。
        """
        self.grab_release()
        self.destroy()


class DialogNotebookApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.log_lines: list[str] = []

        # 持有当前打开的设置窗口引用，用于"同一时间只允许一个设置窗口"。
        self._settings_dialog: SettingsDialog | None = None

        self._build_menu()
        self._build_notebook()
        self._log("应用已启动。")

    # ------------------------------------------------------------------
    # 菜单栏
    # ------------------------------------------------------------------
    def _build_menu(self) -> None:
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)

        file_menu = tk.Menu(menubar, tearoff=False)
        file_menu.add_command(label="打开文件...", command=self.on_open_file)
        file_menu.add_separator()
        file_menu.add_command(label="设置...", command=self.on_open_settings)
        file_menu.add_separator()
        file_menu.add_command(label="退出", command=self.root.destroy)
        menubar.add_cascade(label="File", menu=file_menu)

        help_menu = tk.Menu(menubar, tearoff=False)
        help_menu.add_command(label="About", command=self.on_about)
        menubar.add_cascade(label="Help", menu=help_menu)

    # ------------------------------------------------------------------
    # Notebook：两个页面
    # ------------------------------------------------------------------
    def _build_notebook(self) -> None:
        notebook = ttk.Notebook(self.root)
        notebook.pack(fill="both", expand=True, padx=8, pady=8)

        # --- 页面 1：文件内容 ---
        file_page = ttk.Frame(notebook, padding=8)
        self.file_content_text = tk.Text(file_page, height=10, wrap="word")
        self.file_content_text.pack(fill="both", expand=True)
        self.file_content_text.insert("1.0", "（尚未打开任何文件，使用 File -> 打开文件... ）")
        self.file_content_text.config(state="disabled")
        notebook.add(file_page, text="文件内容")

        # --- 页面 2：运行日志 ---
        log_page = ttk.Frame(notebook, padding=8)
        self.log_text = tk.Text(log_page, height=10, wrap="word", state="disabled")
        self.log_text.pack(fill="both", expand=True)
        notebook.add(log_page, text="运行日志")

        self.notebook = notebook

    # ------------------------------------------------------------------
    # 回调
    # ------------------------------------------------------------------
    def on_open_file(self) -> None:
        path = filedialog.askopenfilename(
            title="选择一个文本文件",
            filetypes=(("Text files", "*.txt *.md *.py *.log"), ("All files", "*.*")),
        )
        if not path:
            # 用户点了取消：filedialog 返回空字符串，这是正常情况，不是错误。
            return

        try:
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                content = f.read()
        except OSError as exc:
            # 对外部文件 I/O 做必要的错误处理，并用 messagebox 提示用户，
            # 而不是让异常直接把程序崩掉。
            messagebox.showerror("打开文件失败", f"无法读取文件：\n{path}\n\n原因：{exc}")
            self._log(f"打开文件失败：{path} ({exc})")
            return

        self.file_content_text.config(state="normal")
        self.file_content_text.delete("1.0", "end")
        self.file_content_text.insert("1.0", content)
        self.file_content_text.config(state="disabled")

        # 切换到"文件内容"页面，让用户立刻看到结果。
        self.notebook.select(0)
        self._log(f"已打开文件：{path}")

    def on_about(self) -> None:
        messagebox.showinfo(
            "About",
            f"{APP_TITLE}\n\nLayer 4 示例：菜单 / 对话框 / Notebook / Toplevel。",
        )

    def on_open_settings(self) -> None:
        """确保同一时间只有一个设置窗口：如果已经打开，就把它提到前台，
        而不是无脑再创建一个新实例。"""
        if self._settings_dialog is not None and self._settings_dialog.winfo_exists():
            self._settings_dialog.lift()
            self._settings_dialog.focus_set()
            return

        self._settings_dialog = SettingsDialog(self.root, on_apply=self._apply_theme)

    def _apply_theme(self, theme: str) -> None:
        self._log(f"主题已切换为：{theme}（示例未真正改变外观，仅记录日志）")

    def _log(self, message: str) -> None:
        self.log_lines.append(message)
        self.log_text.config(state="normal")
        self.log_text.insert("end", message + "\n")
        self.log_text.see("end")
        self.log_text.config(state="disabled")


def build_root_window() -> tk.Tk:
    root = tk.Tk()
    root.title(APP_TITLE)
    root.geometry("480x360")
    return root


def main() -> None:
    root = build_root_window()
    DialogNotebookApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
