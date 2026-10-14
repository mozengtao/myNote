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
