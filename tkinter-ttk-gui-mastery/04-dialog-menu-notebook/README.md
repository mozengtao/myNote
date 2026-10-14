# Layer 4：对话框、菜单与多页面

> 对应示例代码：[examples/04_dialog_notebook.py](examples/04_dialog_notebook.py)

---

## A. Why

一个只有单一界面的程序很快就会不够用：真实工具通常需要“菜单栏”“打开文件”“设置”“关于”这些标准外壳，以及“同一个主窗口里切换不同视图”的能力。这一层要解决两个问题：

1. 哪些场景该“新开一个窗口”（`Toplevel`），哪些场景该“在同一个窗口里切页面”（`Notebook`）？
2. 子窗口和主窗口之间该如何正确地协作、关闭、释放，避免出现“设置窗口关不掉”“点哪都没反应”这类问题。

---

## B. Mental Model

### B.1 主窗口与子窗口的关系

```text
tk.Tk()  (root，唯一的根窗口，拥有自己的 Tcl 解释器)
   |
   +-- tk.Menu (菜单栏，挂在 root 上)
   +-- ttk.Notebook (root 的子控件，内部切换多个 Frame 页面)
   |
   +-- tk.Toplevel (SettingsDialog，独立的操作系统窗口，
                     但生命周期仍然依附于 root：root 销毁时，
                     所有 Toplevel 子窗口也会被自动销毁)
```

`Toplevel` 不是第二个 `Tk()`：一个进程里应该只有一个 `tk.Tk()` 实例，其余额外窗口全部用 `Toplevel`。`Toplevel` 仍然共享同一个 Tcl 解释器和事件循环，只是在操作系统层面呈现为一个独立的窗口。

### B.2 什么时候用 `Toplevel`，什么时候用 `Notebook`

| 场景特征 | 选择 |
|---|---|
| 用户大多数时间不需要同时看到它和主界面（设置、关于、某条记录详情） | `Toplevel` |
| 用户经常需要在几个视图之间来回切换着对比看（本例的“文件内容”和“运行日志”） | `Notebook` 页面 |
| 需要阻塞主窗口直到用户做出选择（确认删除、输入必填参数） | `Toplevel` + `grab_set()`（模态） |
| 只是把一个很长的表单拆成几段展示，减少单页信息量 | `Notebook` 页面 |

### B.3 模态窗口（`transient()` + `grab_set()`）的使用边界

```text
SettingsDialog.__init__
     |
     +-- self.transient(master)   # 声明"我是 master 的附属窗口"
     +-- self.grab_set()          # 抢占输入焦点，主窗口暂时无法操作
     |
     | 用户点击"应用"或"取消" / 点击标题栏关闭按钮
     v
self.close()
     +-- self.grab_release()      # 必须先释放 grab
     +-- self.destroy()           # 再销毁窗口对象
```

**边界原则**：
- 只有当“必须让用户先处理完这个窗口才能继续”时才用 `grab_set()`（例如本例的设置窗口：用户点“应用”或“取消”之前，不应该还能操作主窗口的菜单/Notebook）。
- 任何模态窗口必须保证用户**总有办法关闭它**（按钮 + 允许标题栏关闭），否则会把整个程序“锁死”。
- 对于“仅展示信息、不影响后续操作”的窗口（比如一个只读的日志详情窗口），不要用 `grab_set()`，让用户可以同时操作主窗口。

### B.4 关闭子窗口时的释放模式

子窗口关闭时最常见的两个错误：
1. 忘记调用 `grab_release()`，导致主窗口“看起来卡住了”（其实是输入焦点还没还给它）。
2. 每次点“设置”菜单都 `SettingsDialog(root, ...)` 创建一个新实例，用户点几次就开出几个重复的设置窗口。

本例的 `on_open_settings()` 用一个实例属性 `self._settings_dialog` 持有当前窗口引用，并用 `winfo_exists()` 判断它是否还“活着”（没被销毁）：如果还在，就把它提到前台（`lift()` + `focus_set()`），而不是重新创建。

---

## C. Architecture

| 对象 | 角色 |
|---|---|
| `root` (`tk.Tk`) | 唯一的根窗口 |
| `tk.Menu(menubar)` + 子菜单 | 挂载在 `root` 上的菜单栏 |
| `ttk.Notebook` | `root` 的子控件，内部管理"文件内容"/"运行日志"两个 `ttk.Frame` 页面 |
| `SettingsDialog` (继承 `tk.Toplevel`) | 独立设置窗口，演示模态窗口的创建与释放 |
| `messagebox` / `filedialog` | 标准库提供的"开箱即用"对话框，不需要自己拼界面 |

---

## D. Complete Code

```python
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
```

---

## E. Lifecycle

1. `DialogNotebookApp.__init__` 建菜单栏、建 Notebook 两页，写入第一条日志。
2. 用户点 `File -> 打开文件...`：触发 `on_open_file` → `filedialog.askopenfilename()` 本身会阻塞当前的 Python 调用（它内部运行了自己的小型事件循环，直到用户选择文件或取消），返回路径字符串或空字符串 → 读文件（捕获 `OSError`）→ 更新 Text 内容 → 切换 Notebook 到第 0 页 → 写日志。
3. 用户点 `Help -> About`：`messagebox.showinfo()` 弹出模态提示框（标准对话框自带模态行为，不需要你自己管理 grab），用户点确定后返回。
4. 用户点 `File -> 设置...`：第一次调用会创建 `SettingsDialog` 实例（`__init__` 里完成 `transient` + `grab_set` + 控件创建）；再次点击时，因为上一个实例还没关闭（`winfo_exists()` 为真），只是把它提到前台。
5. 用户在设置窗口点“应用”：调用 `_apply_theme` 回调（写日志）→ `close()`（先 `grab_release()` 再 `destroy()`）→ 主窗口恢复可操作状态。

---

## F. Key APIs

| API | 作用 | 输入 | 输出 / 副作用 | 常见错误 |
|---|---|---|---|---|
| `tk.Menu(parent, tearoff=False)` | 创建菜单/子菜单 | `tearoff=False` 禁用“撕下菜单”的旧式虚线（现代应用通常禁用） | 需要 `menu.add_command/add_cascade/add_separator` 添加内容 | 忘记 `root.config(menu=menubar)`，菜单对象创建了但没有真正挂到窗口上 |
| `messagebox.showinfo/showerror/showwarning(title, message)` | 弹出标准提示/错误/警告对话框 | 标题、消息文本 | 阻塞直到用户点击确定，返回按钮标识 | 把本该用 `error` 的场景写成 `info`，造成用户误判严重程度 |
| `filedialog.askopenfilename(**opts)` | 弹出系统原生的“打开文件”对话框 | `filetypes`、`title` 等 | 返回选中的文件路径（字符串），取消返回空字符串 | 没有判断空字符串就直接往下处理，导致“打开了一个空路径”的错误 |
| `ttk.Notebook(parent)` + `.add(frame, text=...)` | 多页面容器 | 每个页面是一个独立的 `ttk.Frame` | `.select(index)` 可以用代码切换到指定页 | 把需要“同时对比查看”的内容做成 Notebook 分页（应该用并排布局，而不是来回切换） |
| `tk.Toplevel(master)` | 创建一个新的操作系统级窗口 | `master` | 它的生命周期跟随 `master`（`master` 销毁时它也被销毁） | 把它当成 `tk.Tk()` 的替代品反复创建根窗口 |
| `toplevel.transient(master)` | 声明附属关系 | `master` | 窗口管理器层面的"从属"行为（随主窗口最小化等） | 忘记设置，子窗口在任务栏/窗口切换列表里独立出现，用户体验割裂 |
| `toplevel.grab_set()` / `grab_release()` | 开启/关闭输入焦点独占（模态） | 无 | 开启后主窗口无法响应点击，直到释放 | 忘记在关闭前 `grab_release()`，导致主窗口表现异常 |

---

## G. Common Failure

### G.1 点菜单没有任何反应

- **现象**：点击 `File -> 打开文件...`，没有任何对话框弹出。
- **根本原因**：`tk.Menu` 对象创建好了，但没有调用 `root.config(menu=menubar)` 把它真正设置为窗口的菜单栏；或者 `add_command` 的 `command=` 又犯了 Layer 2 讲过的“传成了函数调用结果”的错误。
- **定位方法**：确认 `root.config(menu=menubar)` 这一行存在；在回调首行加 `print()`。
- **修复方案**：补上 `config(menu=...)`；修正 `command=` 的写法。

### G.2 设置窗口关掉之后，再也打不开了 / 点“设置”没反应

- **现象**：第一次打开设置窗口正常，关闭后再点菜单，什么都不出现。
- **根本原因**：`self._settings_dialog` 仍然持有一个**已经被销毁**的 Toplevel 对象引用，但代码没有用 `winfo_exists()` 判断，而是直接认为“只要变量不是 None 就还活着”，于是走进了“提到前台”的分支，对一个已销毁的窗口调用 `lift()` 会抛异常或静默失败。
- **定位方法**：在 `on_open_settings` 里打印 `self._settings_dialog.winfo_exists()` 的值确认。
- **修复方案**：始终用 `widget is not None and widget.winfo_exists()` 这个组合判断“引用还有效且窗口还没被销毁”，这是管理可选子窗口生命周期的标准写法。

### G.3 模态窗口打开后，主窗口完全无法操作，而且设置窗口关掉后主窗口还是卡着

- **现象**：打开设置窗口后一切正常，但点“取消”关闭它之后，主窗口仍然点不动。
- **根本原因**：`close()` 里只调用了 `self.destroy()`，漏掉了 `self.grab_release()`。有些平台/窗口管理器下，被 `grab_set()` 独占的输入焦点不会随窗口销毁自动释放。
- **定位方法**：对比“点确定关闭”和“点取消关闭”两条路径，检查它们是否都经过同一个释放了 grab 的 `close()` 方法，还是各自写了一遍关闭逻辑导致遗漏。
- **修复方案**：把“释放 grab + destroy”统一收敛到一个方法（本例的 `close()`），所有关闭路径（按钮、标题栏 X、Esc 快捷键）都调用这一个方法，不要分散实现。

---

## H. Exercises

1. **基础修改**：给 `SettingsDialog` 加一个“取消”对应的快捷键——绑定 `self.bind("<Escape>", lambda e: self.close())`，使按下 Esc 键等价于点击“取消”按钮。预期结果：设置窗口获得焦点时按 Esc 可以关闭它，且主窗口恢复可操作。
2. **工程向练习**：新增第三个 Notebook 页面“关于”，把当前菜单里 `Help -> About` 弹出的 `messagebox.showinfo` 内容，改成直接以页面形式展示（而不是弹窗）。然后思考并写一段注释说明：这个改动是不是一个好主意？在什么情况下，把“关于信息”做成常驻页面反而是不好的工程决策（提示：从“用户查看频率”和“占用界面空间”两个角度分析）。预期结果：你能清楚说明 B.2 节的选择标准在这个具体案例上如何应用。

---

## I. Summary

本层新增的 Mental Model：

- 一个进程只应该有一个 `tk.Tk()`，额外窗口全部用 `Toplevel`；选择 `Toplevel` 还是 `Notebook` 页面，取决于“这是不是主流程的一部分”“用户需不需要同时看到它和主界面”。
- 模态窗口（`transient` + `grab_set`）有明确的使用边界：只用于“必须先处理完才能继续”的场景，且必须保证可关闭、必须成对调用 `grab_release()`。
- 可选子窗口的生命周期管理模式：持有引用 + `winfo_exists()` 判断，避免重复创建或操作已销毁的对象；所有关闭路径收敛到同一个清理方法。

与前序 Layer 的关系：Layer 2/3 讲的是“单一界面内”的事件和状态管理；Layer 4 把这些能力扩展到了“多窗口/多页面”的组织层面。下一层会引入一种专门用于展示批量结构化数据的控件——`Treeview`。

下一步：[Layer 5：Treeview 与日志查看器](../05-treeview-log-viewer/README.md)。
