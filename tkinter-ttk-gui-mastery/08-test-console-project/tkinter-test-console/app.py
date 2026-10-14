"""
app.py

Layer 8 综合项目：应用入口。

运行方式（必须在 tkinter-test-console/ 目录下运行，这样 Python 才能
正确找到同级的 models.py / services / ui 模块）：

    cd tkinter-ttk-gui-mastery/08-test-console-project/tkinter-test-console
    python3 app.py

这个文件只做"组装"工作：创建根窗口，创建 MainWindow，启动事件循环。
不包含任何业务逻辑或界面细节——那些分别在 models.py / services/
test_service.py / ui/main_window.py 里。
"""

from __future__ import annotations

import tkinter as tk

from ui.main_window import MainWindow

APP_TITLE = "自动化测试控制台 (Tkinter Test Console)"


def build_root_window() -> tk.Tk:
    root = tk.Tk()
    root.title(APP_TITLE)
    root.geometry("560x560")
    root.minsize(480, 480)
    return root


def main() -> None:
    root = build_root_window()
    MainWindow(root)
    root.mainloop()


if __name__ == "__main__":
    main()
