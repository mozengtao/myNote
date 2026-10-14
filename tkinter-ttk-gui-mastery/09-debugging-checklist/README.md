# 09：常见错误与排查清单

这是一份实际工程排查清单，覆盖本教程 Layer 0~8 中出现过、以及日常 Tkinter + ttk 开发中最常遇到的 12 类问题。每一项都按照"发生在哪个阶段 / 相关对象或状态 / 如何观察 / 最小复现 / 修复思路"的结构组织，便于你在真正遇到问题时按图索骥，而不需要重新读一遍所有 Layer。

---

## 1. 窗口没有显示

- **发生阶段**：程序启动，`tk.Tk()` 之后。
- **相关对象/状态**：`root.mainloop()` 是否被调用；是否过早调用了 `destroy()`。
- **如何观察**：进程启动后立刻结束（`echo $?` 看退出码是 0 还是异常），没有任何窗口闪现。
- **最小复现**：
  ```python
  import tkinter as tk
  root = tk.Tk()
  root.title("test")
  # 忘记调用 root.mainloop()
  ```
- **修复思路**：检查 `main()` 的最后一条语句是不是 `root.mainloop()`；确认没有在它之前意外调用 `root.destroy()`/`root.quit()`。详见 [Layer 0 Common Failure G.1](../00-architecture/README.md#g-common-failure)。

## 2. `python -m tkinter` 失败

- **发生阶段**：环境自检，和具体业务代码无关。
- **相关对象/状态**：Tkinter 模块是否安装；是否有可用的显示环境。
- **如何观察**：
  ```bash
  python3 -c "import tkinter; print(tkinter.TkVersion)"
  echo $DISPLAY   # Linux，为空说明没有显示环境
  ```
- **最小复现**：在没有安装 `python3-tk`（Debian/Ubuntu）的系统上直接 `python3 -m tkinter`；或在 SSH 终端未开 `-X` 转发时运行任意 Tkinter 脚本。
- **修复思路**：区分"缺包"（`ModuleNotFoundError`，装对应系统包）和"没有显示环境"（`TclError: no display name`，需要 X11 转发/本机桌面环境/Xvfb）。详见 [Layer 0 Common Failure G.2/G.3](../00-architecture/README.md#g-common-failure)。

## 3. 按钮点击没有反应

- **发生阶段**：事件绑定时（代码编写阶段），表现在运行时。
- **相关对象/状态**：`command=` 参数的值。
- **如何观察**：在怀疑的回调函数第一行加 `print("called")`；如果这句话在**窗口刚显示、还没点任何按钮**时就打印了，说明回调在创建控件时被意外执行了一次。
- **最小复现**：
  ```python
  ttk.Button(parent, text="OK", command=do_something())  # 多了括号
  ```
- **修复思路**：去掉多余的括号，传函数对象本身；需要带参数时用 `lambda`。详见 [Layer 2 Mental Model B.3](../02-event-callback/README.md#b-mental-model) 和 Common Failure G.1。

## 4. 界面卡死

- **发生阶段**：某个回调执行期间。
- **相关对象/状态**：是否在回调里写了同步阻塞的代码（`time.sleep()`、同步网络请求、大量计算、未设置超时的 I/O）。
- **如何观察**：操作系统标题栏/任务管理器显示程序"未响应"；在怀疑的回调开头和结尾打印时间戳，确认是不是这段代码本身执行时间过长。
- **最小复现**：
  ```python
  def on_click():
      import time
      time.sleep(5)  # 阻塞主线程 5 秒，期间窗口完全无响应
  ```
- **修复思路**：短暂等待用 `after()`（Layer 6）；真正耗时的工作移到后台线程 + `queue.Queue` + `after()` 轮询（Layer 7）。详见 [Layer 6 Common Failure G.3](../06-after-timer/README.md#g-common-failure) 和 [Layer 7 整体 Mental Model](../07-thread-queue/README.md#b-mental-model)。

## 5. `command` 传入了错误的函数调用

- **发生阶段**：代码编写阶段（控件创建时）。
- **相关对象/状态**：`command=fn()` vs `command=fn`。
- **如何观察**：和第 3 项现象一致（回调在控件创建时被执行一次，之后点击无效）；区别在于这一条专指"写法层面"的根因排查，第 3 项是"现象"排查。
- **最小复现**：同第 3 项。
- **修复思路**：牢记"`command=` 要的是函数对象，不是函数调用结果"。详见 [Layer 2 B.3](../02-event-callback/README.md#b-mental-model)。

## 6. `grid()` 与 `pack()` 冲突

- **发生阶段**：布局代码编写/运行阶段。
- **相关对象/状态**：同一个父容器的直接子控件混用了两种几何管理器。
- **如何观察**：报错 `_tkinter.TclError: cannot use geometry manager grid inside .xxx which already has slaves packed`，报错信息里会给出冲突的容器路径。
- **最小复现**：
  ```python
  frame = ttk.Frame(root)
  ttk.Label(frame, text="A").pack()
  ttk.Label(frame, text="B").grid(row=0, column=0)  # 冲突
  ```
- **修复思路**：把需要不同布局方式的子控件拆到两个独立的子 Frame 里，每个 Frame 内部统一一种布局管理器。详见 [Layer 1 Mental Model B.4 和 Common Failure G.1](../01-layout-widget-tree/README.md#b-mental-model)。

## 7. 控件变量没有正确更新

- **发生阶段**：运行时，表单/状态展示相关代码。
- **相关对象/状态**：`tk.StringVar`/`IntVar`/`BooleanVar` 与控件的 `textvariable`/`variable` 绑定关系。
- **如何观察**：`var.set(...)` 之后立刻 `print(var.get())` 确认值确实变了；再检查对应控件创建时用的是 `textvariable=`/`variable=` 还是普通的 `text=`（后者是固定文本，不会跟随 Variable 变化）。
- **最小复现**：
  ```python
  name_var = tk.StringVar(value="A")
  label = ttk.Label(parent, text="A")  # 写成了 text=，不是 textvariable=
  name_var.set("B")  # Label 文字不会变
  ```
- **修复思路**：确保控件使用 `textvariable=`/`variable=` 绑定，且绑定的是同一个 Variable 实例。详见 [Layer 3 Common Failure G.2](../03-variables-form-binding/README.md#g-common-failure)。

## 8. 后台线程更新 UI 导致异常或不稳定

- **发生阶段**：运行时，通常是程序运行一段时间后随机出现，很难稳定复现。
- **相关对象/状态**：是否有 `threading.Thread(target=fn)` 的 `fn` 内部直接引用了 Tkinter 控件/变量对象。
- **如何观察**：现象通常是随机的崩溃或界面绘制错乱，和具体业务逻辑看起来不直接相关；需要审查所有后台线程函数体，确认是否出现了任何 Tkinter 对象引用。
- **最小复现**：
  ```python
  def worker(label):
      import time
      time.sleep(1)
      label.config(text="done")  # 后台线程直接改 Widget，危险

  threading.Thread(target=worker, args=(my_label,)).start()
  ```
- **修复思路**：后台函数只通过 `queue.Queue` 传递纯数据，所有 Widget 更新收敛到主线程里由 `after()` 触发的消费函数中。详见 [Layer 7 Mental Model B.2 和 Common Failure G.1](../07-thread-queue/README.md#b-mental-model)。

## 9. `after()` 被重复调度

- **发生阶段**：运行时，通常在"开始/启动"类按钮被多次点击之后。
- **相关对象/状态**：是否在调度前检查了"是否已经在运行"；是否保存了 `after()` 返回的任务 id。
- **如何观察**：计时器/轮询速度变成了预期的 N 倍（因为同时存在多条并行的调度链，各自独立更新同一份状态）。
- **最小复现**：
  ```python
  def on_start():
      schedule_tick()  # 没有检查 self._running，重复点击会产生多条链

  def schedule_tick():
      self._after_id = root.after(100, tick)
  ```
- **修复思路**：调度前检查运行状态标志，保存并复用唯一的任务 id；需要停止时调用 `after_cancel(after_id)`。详见 [Layer 6 Mental Model 和 Common Failure G.1](../06-after-timer/README.md#g-common-failure)。

## 10. 窗口关闭后仍然存在后台工作

- **发生阶段**：窗口关闭时及之后。
- **相关对象/状态**：是否注册了 `WM_DELETE_WINDOW` 协议；关闭回调里是否取消了挂起的 `after()` 任务、是否通知/等待了后台线程退出。
- **如何观察**：关闭窗口后终端打印出 `invalid command name ".!xxx"` 之类的 Tcl 报错（说明有挂起的 `after()` 任务试图访问已销毁的 Widget）；或者进程在窗口关闭后仍然挂着不退出（后台线程没有被正确处理，且不是 daemon 线程）。
- **最小复现**：
  ```python
  root.after(5000, lambda: label.config(text="x"))
  root.mainloop()
  # 用户在 5 秒内关闭了窗口，之后这个 after() 到期时尝试访问已销毁的 label
  ```
- **修复思路**：`root.protocol("WM_DELETE_WINDOW", on_close)`，在 `on_close` 里 `after_cancel()` 所有挂起任务，设置取消信号并 `thread.join(timeout=...)` 等待后台线程退出，最后才 `root.destroy()`。详见 [Layer 6 Common Failure G.2](../06-after-timer/README.md#g-common-failure) 和 [Layer 7 的 `on_close`](../07-thread-queue/README.md#d-complete-code)。

## 11. 回调异常导致任务状态无法恢复

- **发生阶段**：后台任务执行期间，出现未预期异常。
- **相关对象/状态**：后台函数是否用 `try/except` 包住了全部逻辑；是否在异常分支里仍然发送了"终态消息"。
- **如何观察**：任务抛出异常后，界面永久停留在"运行中"状态，"开始"按钮一直是禁用的，但后台线程其实已经终止（`is_alive()` 为 `False`）。
- **最小复现**：
  ```python
  def worker(out_queue):
      out_queue.put("step1 done")
      raise RuntimeError("boom")  # 没有 try/except，线程静默终止
      out_queue.put("done")       # 永远不会执行，UI 永远等不到终态消息
  ```
- **修复思路**：后台函数用 `try/except Exception` 包住全部逻辑，捕获后把异常信息打包成一条消息放入队列，由主线程的消费函数展示并恢复控件状态。详见 [Layer 7 Mental Model B.5](../07-thread-queue/README.md#b-mental-model) 和 [Layer 8 `_finalize_batch()`](../08-test-console-project/README.md#d-complete-code)。

## 12. Treeview 中显示的数据与业务数据不一致

- **发生阶段**：运行时，增删 Treeview 数据之后。
- **相关对象/状态**：是否存在"直接操作 Treeview 行"而没有同步修改业务数据模型的代码路径；iid→业务主键的映射是否在 `refresh_table()` 之后失效。
- **如何观察**：删除一条记录后表格仍显示它（或反过来）；选中某行查看详情时显示的是别的记录的数据。
- **最小复现**：
  ```python
  tree.delete(selected_iid)      # 只删了视图这一行
  # 忘记：self.records = [r for r in self.records if r.id != target_id]
  ```
- **修复思路**：固定顺序——先修改业务数据模型，再调用 `refresh_table()` 整体重绘视图；任何 iid 查询都应该紧跟一次新鲜的 `tree.selection()`，不要跨越一次 `refresh_table()` 复用旧 iid。详见 [Layer 5 Mental Model B.1/B.2 和 Common Failure G.1/G.2](../05-treeview-log-viewer/README.md#g-common-failure)。

---

## 排查思路总览

```text
现象：窗口/控件行为不符合预期
        |
        v
先定位"阶段"：
   启动阶段？（1、2）
   布局阶段？（6）
   事件绑定阶段？（3、5）
   运行时交互？（4、7、9）
   后台任务相关？（8、10、11）
   数据展示相关？（12）
        |
        v
对照上面对应条目的"如何观察"，用最小复现脚本单独验证根因
        |
        v
应用对应"修复思路"，回到源 Layer 的 README 复习相关 Mental Model
```

这份清单建议收藏备查，而不是一次性记住——实际排错时，先判断现象发生在哪个阶段，再翻到对应条目核对，往往能比凭记忆猜测更快定位问题。
