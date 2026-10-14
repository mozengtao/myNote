# Layer 5：Treeview 与日志查看器

> 对应示例代码：[examples/05_log_viewer.py](examples/05_log_viewer.py)

---

## A. Why

真实工具几乎都需要展示“一批结构化数据”：测试记录、设备列表、日志条目……`ttk.Treeview` 是 Tkinter 里唯一专门为这种场景设计的控件。但新手最容易犯的错误是把 Treeview 当成“数据本身”——直接在增删改查时去操作 Treeview 的行，久而久之界面显示的内容和真实业务数据逐渐“对不上”。这一层要建立的核心认知：**View 和 Data Model 永远是两个东西**。

---

## B. Mental Model

### B.1 View 与 Data Model 的关系

```text
业务数据模型 (self.records: list[TestRecord])      <-- 唯一真相来源
            |
            | refresh_table()：把每条 TestRecord
            | 转换成一行字符串元组（TestRecord.as_row()）
            v
Treeview 显示内容                                    <-- 只是数据的“投影”
```

一句话 Mental Model：

> **Treeview 不是数据库，也不是业务数据的唯一存储位置。它只是把 `self.records` 当前内容“画出来”的一个视图。任何对数据的增删改，都先修改 `self.records`，再调用 `refresh_table()` 让视图跟上，而不是反过来直接操作 Treeview 的行。**

### B.2 为什么需要 iid → record_id 的映射

Treeview 内部给每一行自动分配一个字符串标识（item id，简称 iid）。这个 iid 是 Treeview **自己的概念**，和你的业务数据的主键（本例的 `record_id`）完全是两套编号体系。当用户选中某一行时，Tkinter 给你的是这一行的 iid，你需要一种方式“反查”这个 iid 对应哪条业务数据——这就是 `self._iid_to_record_id` 这张映射表存在的原因。

```text
用户点击表格第 3 行
        |
        v
tree.selection() 返回这一行的 iid（例如 "I003"）
        |
        | self._iid_to_record_id["I003"] -> record_id
        v
self.find_record(record_id) -> 完整的 TestRecord 对象
```

### B.3 整体重绘 vs 增量更新

本例的 `refresh_table()` 每次都 `self.tree.delete(*self.tree.get_children())` 清空所有行，再重新 `insert` 全部数据。对于教学规模（几十到几百条记录）这是最简单可靠的做法——不用担心“Treeview 当前显示的行”和“数据模型”之间出现任何不一致。真实工程中如果数据量很大（上万行）且更新频繁，才需要考虑增量 `insert`/`delete`/`item(..., values=...)` 来优化性能，但那是性能优化手段，不是必须的默认选择，优化前要先确认真的存在性能问题。

---

## C. Architecture

| 对象 | 角色 |
|---|---|
| `TestRecord` (dataclass) | 业务数据模型，数据的唯一真相来源 |
| `self.records: list[TestRecord]` | 当前所有记录的内存存储 |
| `self.tree` (`ttk.Treeview`) | 数据的表格视图 |
| `self._iid_to_record_id` | Treeview 行 iid → 业务数据主键的映射 |
| `self.detail_var` (`tk.StringVar`) | 选中行详情的展示状态（见 Layer 3 的变量绑定） |
| `self.log_text` (`tk.Text` + `ttk.Scrollbar`) | 操作日志的滚动文本区域 |

---

## D. Complete Code

```python
"""
05_log_viewer.py

Layer 5：Treeview 与日志查看器。

运行方式：
    python3 examples/05_log_viewer.py

本例演示：
    1. 用 ttk.Treeview 展示测试记录（时间 / 设备 / 状态 / 耗时(ms) 四列）。
    2. 独立的 Python 数据模型（self.records）才是数据的唯一来源，
       Treeview 只是这份数据的"视图"：增删记录时调用 refresh_table()
       重新渲染，而不是手动在 Treeview 和数据之间来回同步修改。
    3. 选中一行查看详情（展示在下方的详情区域）。
    4. 添加 / 删除选中记录。
    5. 一个带滚动条的日志文本区域，记录操作历史。
"""

from __future__ import annotations

import random
import tkinter as tk
from dataclasses import dataclass
from datetime import datetime
from tkinter import ttk


@dataclass
class TestRecord:
    """业务数据模型：一条测试记录。

    这是数据的"唯一存储位置"。Treeview 里显示的每一行文字都是从这里
    派生出来的，不会反过来从 Treeview 读取数据。
    """

    record_id: int
    timestamp: str
    device: str
    status: str  # "PASS" / "FAIL"
    duration_ms: int

    def as_row(self) -> tuple[str, str, str, str]:
        """把业务数据转换成 Treeview 一行需要的字符串元组（视图投影）。"""
        return (self.timestamp, self.device, self.status, f"{self.duration_ms} ms")


COLUMNS = ("timestamp", "device", "status", "duration")
COLUMN_HEADINGS = {
    "timestamp": "时间",
    "device": "设备",
    "status": "状态",
    "duration": "耗时",
}


class LogViewerApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root

        # --- 数据模型：唯一的数据来源 ---
        self.records: list[TestRecord] = []
        self._next_id = 1
        # Treeview 的内部 item id（iid，字符串）→ record_id 的映射，
        # 用于"选中某一行时，反查它对应的业务数据"。
        self._iid_to_record_id: dict[str, int] = {}

        self._build_widgets()
        self._log("日志查看器已启动。")

    # ------------------------------------------------------------------
    # UI 构建
    # ------------------------------------------------------------------
    def _build_widgets(self) -> None:
        root_frame = ttk.Frame(self.root, padding=12)
        root_frame.pack(fill="both", expand=True)
        root_frame.columnconfigure(0, weight=1)
        root_frame.rowconfigure(1, weight=1)

        # --- 工具条 ---
        toolbar = ttk.Frame(root_frame)
        toolbar.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        ttk.Button(toolbar, text="添加随机记录", command=self.on_add_record).pack(side="left")
        ttk.Button(toolbar, text="删除选中记录", command=self.on_delete_selected).pack(
            side="left", padx=(8, 0)
        )

        # --- Treeview + 纵向滚动条 ---
        table_frame = ttk.Frame(root_frame)
        table_frame.grid(row=1, column=0, sticky="nsew")
        table_frame.columnconfigure(0, weight=1)
        table_frame.rowconfigure(0, weight=1)

        self.tree = ttk.Treeview(
            table_frame,
            columns=COLUMNS,
            show="headings",  # 不显示默认的树形缩进列，当成纯表格使用
            selectmode="browse",  # 一次只能选中一行
            height=8,
        )
        for col in COLUMNS:
            self.tree.heading(col, text=COLUMN_HEADINGS[col])
            self.tree.column(col, width=120, anchor="center")
        self.tree.column("timestamp", width=160, anchor="w")

        scrollbar = ttk.Scrollbar(table_frame, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=scrollbar.set)

        self.tree.grid(row=0, column=0, sticky="nsew")
        scrollbar.grid(row=0, column=1, sticky="ns")

        self.tree.bind("<<TreeviewSelect>>", self.on_select_row)

        # --- 详情区 ---
        detail_frame = ttk.LabelFrame(root_frame, text="选中记录详情", padding=8)
        detail_frame.grid(row=2, column=0, sticky="ew", pady=(8, 0))
        self.detail_var = tk.StringVar(value="（未选中任何记录）")
        ttk.Label(detail_frame, textvariable=self.detail_var, justify="left").pack(
            anchor="w"
        )

        # --- 日志文本区 + 滚动条 ---
        log_frame = ttk.LabelFrame(root_frame, text="操作日志", padding=4)
        log_frame.grid(row=3, column=0, sticky="nsew", pady=(8, 0))
        root_frame.rowconfigure(3, weight=1)
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)

        self.log_text = tk.Text(log_frame, height=6, state="disabled", wrap="word")
        log_scrollbar = ttk.Scrollbar(
            log_frame, orient="vertical", command=self.log_text.yview
        )
        self.log_text.configure(yscrollcommand=log_scrollbar.set)
        self.log_text.grid(row=0, column=0, sticky="nsew")
        log_scrollbar.grid(row=0, column=1, sticky="ns")

    # ------------------------------------------------------------------
    # 数据 <-> 视图：核心机制
    # ------------------------------------------------------------------
    def refresh_table(self) -> None:
        """用当前的 self.records 重新渲染 Treeview。

        关键认知：Treeview 不是数据库，也不是业务数据的唯一存储位置。
        每次数据变化（增删），我们先修改 self.records，再调用这个方法
        "重新渲染"视图，而不是直接对 Treeview 做增量操作后就假设数据
        和视图永远同步——对于本例这种规模，整体重绘最简单可靠；数据量
        很大时才需要考虑增量 insert/delete 以优化性能。
        """
        self.tree.delete(*self.tree.get_children())
        self._iid_to_record_id.clear()

        for record in self.records:
            iid = self.tree.insert("", "end", values=record.as_row())
            self._iid_to_record_id[iid] = record.record_id

    def add_record(self, device: str, status: str, duration_ms: int) -> TestRecord:
        record = TestRecord(
            record_id=self._next_id,
            timestamp=datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            device=device,
            status=status,
            duration_ms=duration_ms,
        )
        self._next_id += 1
        self.records.append(record)
        self.refresh_table()
        return record

    def find_record(self, record_id: int) -> TestRecord | None:
        for record in self.records:
            if record.record_id == record_id:
                return record
        return None

    # ------------------------------------------------------------------
    # 回调
    # ------------------------------------------------------------------
    def on_add_record(self) -> None:
        device = random.choice(("CM-01", "CM-02", "CMTS-A", "RPD-05"))
        status = random.choice(("PASS", "PASS", "PASS", "FAIL"))
        duration_ms = random.randint(80, 950)
        record = self.add_record(device, status, duration_ms)
        self._log(f"添加记录 #{record.record_id}: {device} {status} {duration_ms}ms")

    def on_delete_selected(self) -> None:
        selected = self.tree.selection()
        if not selected:
            self._log("删除失败：没有选中任何记录。")
            return

        iid = selected[0]
        record_id = self._iid_to_record_id.get(iid)
        if record_id is None:
            return

        self.records = [r for r in self.records if r.record_id != record_id]
        self.refresh_table()
        self.detail_var.set("（未选中任何记录）")
        self._log(f"已删除记录 #{record_id}")

    def on_select_row(self, event: tk.Event) -> None:
        selected = self.tree.selection()
        if not selected:
            return

        iid = selected[0]
        record_id = self._iid_to_record_id.get(iid)
        record = self.find_record(record_id) if record_id is not None else None
        if record is None:
            self.detail_var.set("（记录不存在，可能已被删除）")
            return

        self.detail_var.set(
            f"记录编号：{record.record_id}\n"
            f"时间：{record.timestamp}\n"
            f"设备：{record.device}\n"
            f"状态：{record.status}\n"
            f"耗时：{record.duration_ms} ms"
        )

    def _log(self, message: str) -> None:
        self.log_text.config(state="normal")
        self.log_text.insert("end", message + "\n")
        self.log_text.see("end")
        self.log_text.config(state="disabled")


def build_root_window() -> tk.Tk:
    root = tk.Tk()
    root.title("Log Viewer - Treeview Demo")
    root.geometry("520x520")
    root.minsize(460, 420)
    return root


def main() -> None:
    root = build_root_window()
    LogViewerApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
```

---

## E. Lifecycle

1. `LogViewerApp.__init__` 初始化空的 `self.records`，构建 Treeview（`show="heading"`，4 列）+ 滚动条 + 详情区 + 日志区。
2. 用户点“添加随机记录”：`on_add_record` 生成随机数据 → `add_record()` 组装 `TestRecord` → append 进 `self.records` → `refresh_table()` 清空并重绘 Treeview → 写操作日志。
3. 用户点击表格某一行：触发 `<<TreeviewSelect>>` 虚拟事件 → `on_select_row` 读取 `tree.selection()` 得到 iid → 查映射表得到 `record_id` → `find_record()` 从 `self.records` 里找到完整数据 → 更新 `detail_var`（Label 自动刷新，机制见 Layer 3）。
4. 用户点“删除选中记录”：`on_delete_selected` 同样先定位 `record_id` → 从 `self.records` 里过滤掉它 → `refresh_table()` 重绘（此时映射表也会被清空重建）→ 重置详情区文字 → 写日志。

---

## F. Key APIs

| API | 作用 | 输入 | 输出 / 副作用 | 常见错误 |
|---|---|---|---|---|
| `ttk.Treeview(parent, columns=(...), show="headings")` | 创建表格/树形控件 | `columns` 定义列的内部标识；`show="headings"` 隐藏第一列的树形缩进 | 返回 Treeview 对象 | 忘记 `show="headings"`，表格左侧多出一列空的树形缩进列 |
| `tree.heading(col, text=...)` | 设置列标题文字 | 列标识、标题文本 | 表头显示对应文字 | 只调用 `column()` 不调用 `heading()`，表头是空的 |
| `tree.column(col, width=..., anchor=...)` | 配置列宽、对齐方式等 | 列标识、宽度、对齐 | 控制该列的显示样式 | 不设置宽度，所有列挤在一起显示不全 |
| `tree.insert(parent, index, values=(...))` | 插入一行数据 | `parent=""` 表示插入到顶层；`index="end"` 插入到末尾；`values` 和 `columns` 顺序一致 | 返回这一行的 iid（字符串） | `values` 的元素数量/顺序和 `columns` 不匹配，导致列错位 |
| `tree.delete(*iids)` | 删除指定的行（可以是全部） | 一个或多个 iid | 这些行从表格中消失 | 以为 `delete()` 会连带删除你的业务数据（它只影响视图，不影响 `self.records`） |
| `tree.selection()` | 获取当前选中的行 iid 列表 | 无 | `selectmode="browse"` 下最多一个元素 | 以为返回的是业务数据，实际上是 Treeview 的内部 iid，需要再查一次映射表 |
| `tree.bind("<<TreeviewSelect>>", cb)` | 绑定"选中行发生变化"事件 | `cb` 接收 `event` 参数 | 用户点击/键盘移动选中行时触发 | 和 `tree.bind("<Button-1>", ...)` 混淆：后者是鼠标点击的原始事件，选中状态可能还没来得及更新 |
| `ttk.Scrollbar(parent, orient="vertical", command=widget.yview)` + `widget.configure(yscrollcommand=scrollbar.set)` | 给 Treeview/Text 等控件挂一个滚动条，两者互相联动 | 见左 | 拖动滚动条会调用 `widget.yview`；滚动控件内容会调用 `scrollbar.set` 更新滑块位置 | 只配置了其中一个方向（比如只设置 `command=`，忘记 `yscrollcommand=`），滚动条和内容"各滚各的" |

---

## G. Common Failure

### G.1 添加/删除数据后，表格显示的内容和实际数据不一致

- **现象**：删除了一条记录，表格里却还显示着（或者显示的详情和真实数据对不上）。
- **根本原因**：某个代码路径直接调用了 `self.tree.delete(iid)` 却忘了同步修改 `self.records`，或者反过来改了 `self.records` 却没调用 `refresh_table()`。
- **定位方法**：检查所有修改数据的代码路径，确认它们是否都遵循"先改 `self.records`，再调用 `refresh_table()`"这个固定顺序；临时在 `refresh_table()` 开头打印 `len(self.records)` 和刷新前后的 Treeview 行数对比。
- **修复方案**：把所有"改数据"的逻辑收敛到少数几个方法（如本例的 `add_record`/`on_delete_selected`），这些方法内部职责明确：改 `self.records` → 调 `refresh_table()`，不要在其他地方绕过这个流程直接操作 Treeview。

### G.2 选中某行后，详情显示的是别的记录（或报错找不到记录）

- **现象**：点击第 2 行，详情区却显示了第 1 行的数据；或者偶尔抛 `KeyError`/显示"记录不存在"。
- **根本原因**：`refresh_table()` 每次都会清空并重建 `self._iid_to_record_id` 映射（因为 Treeview 删除重建后，iid 本身也变了——旧的 iid 不再有效）。如果某处代码缓存了"旧的 iid"并在 `refresh_table()` 之后继续使用，就会查到错误的记录或查不到。
- **定位方法**：检查是否有代码在 `refresh_table()` 调用之后，还在使用 `refresh_table()` 调用之前取到的 iid。
- **修复方案**：任何“iid → record_id”的查询都应该紧跟着一次新鲜的 `tree.selection()` 调用，不要跨越一次 `refresh_table()` 去复用旧的 iid。

### G.3 Treeview 滚动条不生效 / 滚轮滚动但滑块不动

- **现象**：数据超过可见行数，但拖动滚动条滑块表格内容不滚动，或者滑块本身位置不随滚动变化。
- **根本原因**：只设置了 `Scrollbar(..., command=tree.yview)`（滚动条控制表格），却忘了 `tree.configure(yscrollcommand=scrollbar.set)`（表格状态反过来控制滑块位置），两者缺一个联动就不完整。
- **定位方法**：检查这两行配置是否都存在，且互相指向正确的对象（`tree` 和 `scrollbar` 不要写反）。
- **修复方案**：永远成对配置：`command=` 指向内容控件的 `yview`，`yscrollcommand=` 指向滚动条的 `set`。

---

## H. Exercises

1. **基础修改**：给 Treeview 新增一个“备注”列（可以留空），并在 `TestRecord` 里加对应字段。给“添加随机记录”按钮生成的记录随机加一句备注（如果为空就显示 `"-"`）。预期结果：表格里多出一列，内容随机但不会报错。
2. **工程向练习**：实现一个“按状态筛选”的下拉框（`ttk.Combobox`，候选值 `("全部", "PASS", "FAIL")`），切换时只在表格里显示符合条件的记录，但 `self.records`（完整数据）保持不变。提示：这正是在检验你是否真正理解了 B.1 的 Mental Model——筛选操作应该只影响“传给 `refresh_table()` 的那份数据子集”，不应该修改 `self.records` 本身。预期结果：切换筛选条件表格行数变化，但再切回"全部"时，之前添加的所有记录都还在。

---

## I. Summary

本层新增的 Mental Model：

- Treeview 永远只是数据的视图，`self.records`（或任何你定义的数据模型）才是唯一真相来源；增删改的固定顺序是“先改数据，再刷新视图”。
- Treeview 的 iid 是它自己的内部编号体系，和业务数据主键是两套不同的东西，需要显式维护映射关系，且这份映射会随 `refresh_table()` 的整体重绘而失效重建。
- 滚动条和被滚动的控件是双向配置的（`command=` 和 `yscrollcommand=` 成对出现），这个模式对 Treeview、Text、Canvas 等所有可滚动控件通用。

与前序 Layer 的关系：Layer 3 讲了“单个值”如何通过 Variable 和控件同步，Layer 5 讲的是“一批结构化数据”如何通过显式的 `refresh_table()` 和视图同步——思路一致（数据变化驱动界面更新），但规模和实现方式不同。下一层开始引入“时间”维度：如何在不阻塞界面的前提下，让某些操作在未来某个时刻自动发生。

下一步：[Layer 6：`after()` 与定时任务](../06-after-timer/README.md)。
