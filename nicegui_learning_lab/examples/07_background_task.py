"""示例 7：后台任务与实时日志 —— 只讲清楚"后台任务机制本身"。

为什么需要这个示例
------------------
前面的示例要么是纯粹的同步界面逻辑（填表单、翻页），要么是"调用一次就
结束"的操作（执行一条命令、读一个文件）。但很多工程 GUI 的核心需求是：
点一个按钮，启动一个**会持续运行一段时间**的后台工作（巡检一圈设备、跑一
轮自动化测试、同步一批配置），同时界面上要能实时看到进度、实时看到滚动
日志，并且中途要能安全地把它停下来。这正是本项目里"后台任务"这一整块的
入口：本示例的唯一目标是把这套机制本身讲清楚——如何启动、如何汇报进度、
如何取消、如何处理异常——**不**掺杂任何具体业务逻辑。下一个示例（08）会
在此基础上，把这里的"裸协程"升级成一个正式的、可复用的 Service（见
`services/test_service.py`），读者可以直接对比这两个示例，看清楚这次
"从机制到业务"的演进关系。

心智模型：协程 / 线程池 / 外部进程，三种"耗时操作"分别怎么处理
----------------------------------------------------------------
在 NiceGUI 里，"耗时操作"大致分三类，处理方式完全不同：

1. **本身就是协程、用 `await asyncio.sleep(...)` 模拟耗时**（本示例）：
   用 `from nicegui import background_tasks; background_tasks.create(coro,
   name=...)` 发起一个"即发即忘"的后台协程。这是本项目**唯一**正确的
   做法——**绝不要**裸用 `asyncio.create_task()`：`background_tasks.create()`
   内部会把创建出来的 task 持有在一个模块级集合里，防止它在还没执行完时
   就被垃圾回收器意外回收（这是 `asyncio.create_task()` 一个广为人知的
   陷阱：如果没有任何代码持有对 task 对象的强引用，Python 的 GC 有可能在
   任意时刻把它回收掉，导致协程莫名其妙地"跑到一半就消失了"）；
   `background_tasks.create()` 还会在协程抛出未捕获异常时，把异常记录到
   NiceGUI 的日志里，而不是让它静默消失。
2. **本身是阻塞的 Python 函数（例如调用标准库 `subprocess.run()`）**：
   见 `infrastructure/process_runner.run_subprocess()` 和
   `services/command_service.run_command()`（示例 6 使用）。这类函数必须用
   `await run.io_bound(阻塞函数, *args)` 丢进线程池执行，绝不能在 NiceGUI
   的事件循环线程里直接调用，否则会卡住整个页面（乃至同一进程里其他用户
   的页面）。
3. **需要启动一个真正独立的外部进程**：见
   `infrastructure/process_runner.stream_subprocess()`（基于
   `asyncio.create_subprocess_exec()`），它本身已经是协程，同样可以配合
   `background_tasks.create()` 使用。

本示例属于第 1 类：`_run_simulated_task()` 从头到尾都是纯 Python 协程，
没有任何阻塞调用，也没有启动任何外部进程。

协作式取消 vs. 杀掉一个外部子进程：本质不同的两件事
----------------------------------------------------
本示例的"取消"是**协作式**的：`cancel_event: asyncio.Event` 只是一个标志
位，`_run_simulated_task()` 在每一步循环开始前检查一次这个标志位，发现
已经被设置就主动抛出一个内部信号异常、结束自己。这要求被取消的协程
"愿意"在某个检查点主动配合——如果某一步忘了检查（见本文档"Failure Modes"
小节），取消请求会被无限期地晾在一边，协程会假装什么都没发生地继续跑
下去，直到自然结束。

这和"杀掉一个正在运行的外部子进程"是完全不同的机制：示例 6 背后的
`infrastructure/process_runner.py`，在超时或需要终止时调用的是操作系统
级别的 `proc.kill()`（再 `await proc.wait()` 回收，避免产生僵尸进程）——
这是**抢占式**的，操作系统强制结束那个进程，不需要、也不可能"等它自己
配合"。协作式取消的好处是可以在退出前做清理工作（记日志、更新状态），
代价是**完全依赖代码里每一步是否记得检查**；抢占式终止不需要目标配合，
但代价是目标进程来不及做任何清理，直接被结束。
"""

from __future__ import annotations

import asyncio
from typing import Callable

from nicegui import background_tasks, ui

#: 模拟任务一共要跑多少"步"；每一步之间 `await asyncio.sleep(_STEP_DELAY)`。
_TOTAL_STEPS = 8

#: 每一步模拟耗时（秒）。选一个让用户能在界面上看清进度条逐步前进的值，
#: 又不至于让示例演示起来太慢。
_STEP_DELAY = 0.4

#: 如果用户勾选了"模拟一次意外异常"，会在跑到这一步时人为抛出异常。
_UNEXPECTED_ERROR_AT_STEP = 3


class _TaskCancelledByUser(Exception):
    """内部信号异常：仅用于把"检测到取消请求"这件事从循环内部带回调用方的
    `try`/`except`，不代表程序出现了任何错误，不应该被当作"异常处理"的
    失败案例看待——它是协作式取消这套机制里**预期内**的一条正常退出路径。
    """


async def _run_simulated_task(
    *,
    cancel_event: asyncio.Event,
    raise_unexpected_error: bool,
    on_progress: Callable[[float], None],
    on_log: Callable[[str], None],
) -> None:
    """一个极简的"本地模拟任务"协程：循环 + `asyncio.sleep` + 进度/日志回调。

    这个函数故意**不**使用 `models.task.TaskRecord`，也不 import
    `services/test_service.py`——本示例的教学目标是先让读者看清"后台任务
    机制本身"长什么样（启动、取消检查点、异常处理、如何汇报进度），不引入
    任何额外的数据结构。下一个示例（08）会展示如何把这里的"裸协程"改造成
    一个正式的、状态机驱动的 Service。

    Args:
        cancel_event: 调用方传入的取消标志位；每一步循环开始前都会检查一次
            `cancel_event.is_set()`。
        raise_unexpected_error: 为 `True` 时，会在跑到第
            `_UNEXPECTED_ERROR_AT_STEP` 步时人为抛出一个 `RuntimeError`，
            用于演示"协程内部出现意外异常"该如何被上层捕获和展示。
        on_progress: 每完成一步调用一次，参数是 0.0~1.0 之间的完成比例。
        on_log: 每产生一条日志就调用一次。

    Raises:
        _TaskCancelledByUser: 在某一步开始前检测到 `cancel_event` 已被设置。
        RuntimeError: 仅当 `raise_unexpected_error=True` 时，在第
            `_UNEXPECTED_ERROR_AT_STEP` 步人为抛出，用于演示异常处理分支。
    """
    on_log(f'任务开始（共 {_TOTAL_STEPS} 步，每步约 {_STEP_DELAY} 秒）')

    for step in range(1, _TOTAL_STEPS + 1):
        # 取消检查点：永远放在每一步"真正做事"之前。这是协作式取消的核心
        # 约定——检查点越密集，取消请求被响应得越快；但检查点本身没有任何
        # 魔法，纯粹是"写在这里的一行 if"，忘记写就意味着这一段代码无法
        # 被取消（见本示例文档 Failure Modes 第 1 条）。
        if cancel_event.is_set():
            on_log(f'在第 {step} 步开始前检测到取消请求（cancel_event 已置位），提前结束')
            raise _TaskCancelledByUser()

        await asyncio.sleep(_STEP_DELAY)

        if raise_unexpected_error and step == _UNEXPECTED_ERROR_AT_STEP:
            raise RuntimeError(
                f'模拟的意外异常（在第 {step} 步人为触发，用于演示"协程抛出异常'
                '时不能让界面卡死"这条要求）'
            )

        on_progress(step / _TOTAL_STEPS)
        on_log(f'第 {step}/{_TOTAL_STEPS} 步完成')

    on_log('全部步骤已完成')


class _PageState:
    """保存"这一个浏览器会话当前后台任务状态"的小容器。

    每次用户点击"开始"，都会在这里创建一枚**全新的** `asyncio.Event`
    （而不是复用上一次任务用过的那一枚）：如果复用旧对象，上一次任务
    结束时它可能已经被设置过，新任务一启动就会立刻看到"已取消"，这是一个
    很容易被忽略的陷阱。
    """

    def __init__(self) -> None:
        self.cancel_event = asyncio.Event()


@ui.page('/background-task')
def background_task_page() -> None:
    """示例 7 页面：开始/取消按钮 + 进度条 + 实时日志，纯机制演示。"""
    state = _PageState()

    ui.label('示例 7：后台任务与实时日志').classes('text-2xl font-bold').mark('page-title')
    ui.label(
        '本页面只演示后台任务机制本身（启动 / 实时进度 / 协作式取消 / 异常处理），'
        '不涉及任何真实业务逻辑；点击"开始"后台的模拟任务纯粹是内存里的循环 + 延时。'
    ).classes('text-sm text-gray-600').mark('page-description')

    raise_error_checkbox = ui.checkbox(
        '勾选后：本次运行会在第 3 步人为触发一个意外异常（用于演示异常处理分支）'
    ).mark('raise-error-checkbox')

    status_label = ui.label('尚未开始').classes('text-sm font-bold mt-2').mark('status-label')

    with ui.row().classes('items-center gap-2 mt-2'):
        start_button = ui.button('开始').mark('start-button')
        cancel_button = ui.button('取消').mark('cancel-button')
        cancel_button.disable()

    progress_bar = ui.linear_progress(value=0.0).classes('mt-2').mark('progress-bar')
    log = ui.log(max_lines=100).classes('w-full h-64 mt-2').mark('task-log')

    async def _drive_task() -> None:
        """驱动一次 `_run_simulated_task()` 的运行，并把结果翻译成界面状态。

        这个函数本身就是会被 `background_tasks.create()` 启动的那个协程：
        它负责调用真正的任务逻辑、捕获所有可能的结束路径（成功 / 取消 /
        意外异常），并在 `finally` 里统一做"重新启用开始按钮"这件收尾工作
        ——不管任务是怎么结束的，用户都不应该被卡在一个永远禁用的按钮前面。
        """
        try:
            await _run_simulated_task(
                cancel_event=state.cancel_event,
                raise_unexpected_error=raise_error_checkbox.value,
                on_progress=lambda value: setattr(progress_bar, 'value', value),
                on_log=log.push,
            )
        except _TaskCancelledByUser:
            status_label.text = '已取消'
            ui.notify('任务已被取消', type='warning')
        except Exception as exc:  # noqa: BLE001 - 这里就是要演示"捕获协程内部的
            # 任何意外异常"：如果不加这一层，异常会从 background_tasks.create()
            # 启动的协程里一路冒出去，NiceGUI 会把它记到日志里，但"开始"按钮
            # 永远不会被重新启用——用户看到的就是一个"卡死"的页面。
            status_label.text = f'出现异常：{exc}'
            log.push(f'[异常] {exc}')
            ui.notify(f'任务出现意外异常：{exc}', type='negative')
        else:
            status_label.text = '已成功完成'
            ui.notify('任务已成功完成', type='positive')
        finally:
            start_button.enable()
            cancel_button.disable()

    def handle_start() -> None:
        """"开始"按钮的点击回调：立即禁用自己，再把真正的任务丢给后台。"""
        # 每次开始都用一枚全新的 Event，避免复用到上一次已经被 set() 过的
        # 旧对象（见 _PageState 的文档说明）。
        state.cancel_event = asyncio.Event()
        progress_bar.value = 0.0
        log.clear()
        status_label.text = '运行中…'

        # 立即禁用"开始"按钮，防止用户在任务还没跑完时重复点击、重复启动
        # 多个并发任务（见本示例文档 Failure Modes 第 3 条）。
        start_button.disable()
        cancel_button.enable()

        # 唯一正确的"即发即忘"启动方式：background_tasks.create()。
        # 绝不要在这里裸用 asyncio.create_task(_drive_task())——没有任何
        # 代码持有对该 task 对象的强引用时，Python 的垃圾回收器有可能在
        # 协程还没跑完时就把它回收掉，导致任务不可预测地"跑到一半消失"。
        background_tasks.create(_drive_task(), name='example-07-background-task')

    def handle_cancel() -> None:
        """"取消"按钮的点击回调：只设置标志位，不做任何其他事情。

        真正的"收尾"（更新状态文案、重新启用按钮、弹出通知）全部发生在
        `_drive_task()` 检测到 `_TaskCancelledByUser` 之后——这正是协作式
        取消的含义：点击取消这个动作本身不会让任务立即停下来，它只是提了
        一个"请求"，后台协程要在下一个检查点才会真正响应。
        """
        state.cancel_event.set()

    start_button.on_click(handle_start)
    cancel_button.on_click(handle_cancel)


if __name__ in {'__main__', '__mp_main__'}:
    ui.run(title='示例 7：后台任务与实时日志')
