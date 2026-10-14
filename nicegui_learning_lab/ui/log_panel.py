"""“运行日志”面板：把项目统一的 `logging` 输出实时展示在网页上。

为什么需要这个面板
------------------
`docs/architecture.md`/本项目冻结契约规定"每个模块用
`logging.getLogger(__name__)`，格式统一为
`%(asctime)s %(levelname)s %(name)s: %(message)s`，在 `app.py` 中统一配置
一次"。`examples/01-09` 各自是独立脚本，日志只打印到各自的终端；而 `app.py`
作为"一个真实工具"，应该能在网页本身看到刚才点击配置检查/命令执行/测试
运行分别产生了哪些日志——这是教学示例不需要、但一个真实工具通常都会有的
能力，也是本面板存在的唯一理由（不是对前三个面板的重复展示）。

实现方式
--------
`install_log_buffer_handler()` 定义了一个最小的 `logging.Handler` 子类，
`emit()` 只做一件事：把格式化后的日志行追加进一个模块级的、有长度上限的
`collections.deque`（`_LOG_BUFFER`）。这个 handler 由 `app.py` 在启动时
挂载到**根 logger** 上一次，之后项目里任何地方（`ui/config_panel.py`、
`ui/command_panel.py`、`ui/task_panel.py`，乃至 `services/*.py` 里的
`logger.info(...)`）产生的日志都会自动进入这个缓冲区。

`build_log_panel()` 则是纯粹的"读缓冲区、增量展示"：每个浏览器标签页打开
这个面板时，都会用 `ui.timer` 周期性检查缓冲区相对于"这个标签页自己已经
看过多少行"的增量，只把新增的行 `push` 进这个标签页自己的 `ui.log` 组件。
"这个标签页看到第几行"是函数局部变量（闭包捕获），天然按浏览器连接隔离；
而 `_LOG_BUFFER` 本身是进程级共享的——这是刻意的："运行日志"本来就应该是
"这个进程做过什么"的记录，所有管理员看到同一份日志是合理的共享语义，
与 `examples/09_multi_user_state.py` 讨论的"不应该共享的用户数据"是两个
不同的问题（见该示例文档对"故意共享"与"不小心共享"的区分）。
"""

from __future__ import annotations

import logging
from collections import deque

from nicegui import ui

#: 进程级共享的日志缓冲区，最多保留最近 500 行；由 `install_log_buffer_handler()`
#: 挂载的 handler 写入，由 `build_log_panel()` 的每个标签页各自增量读取。
_LOG_BUFFER: deque[str] = deque(maxlen=500)

_LOG_FORMAT = '%(asctime)s %(levelname)s %(name)s: %(message)s'


class _BufferHandler(logging.Handler):
    """把格式化后的日志行写入 `_LOG_BUFFER` 的最小 handler。"""

    def emit(self, record: logging.LogRecord) -> None:
        try:
            _LOG_BUFFER.append(self.format(record))
        except Exception:  # noqa: BLE001 - handler 自身绝不能抛异常中断日志系统
            self.handleError(record)


def install_log_buffer_handler(level: int = logging.INFO) -> None:
    """把 `_BufferHandler` 挂载到根 logger 上；`app.py` 启动时只应调用一次。

    重复调用是安全的（会重复挂载多个 handler，导致日志行重复出现），
    `app.py` 通过只在模块级调用一次来保证这一点，而不是在这个函数内部做
    幂等检查——幂等检查本身需要遍历 `logging.root.handlers` 判断类型，
    增加的复杂度超过了这个场景实际需要的程度。
    """
    handler = _BufferHandler(level=level)
    handler.setFormatter(logging.Formatter(_LOG_FORMAT))
    logging.getLogger().addHandler(handler)


def build_log_panel() -> None:
    """构建运行日志面板：一个 `ui.log` + 周期性增量刷新。"""
    ui.label(
        '本面板展示整个应用进程的运行日志（所有面板共享同一份，进程重启即清空，'
        '最多保留最近 500 行）。试试在"配置检查"或"命令控制台"里做一次操作，'
        '再回到这里查看。'
    ).classes('text-sm text-gray-600').mark('log-panel-description')

    log = ui.log(max_lines=500).classes('w-full h-96 mt-2').mark('log-panel-log')

    # 闭包捕获的局部状态："这个浏览器标签页已经看到缓冲区的前多少行"，
    # 天然按连接隔离，不需要借助任何 app.storage。
    state = {'seen': 0}

    def _refresh() -> None:
        snapshot = list(_LOG_BUFFER)
        for line in snapshot[state['seen']:]:
            log.push(line)
        state['seen'] = len(snapshot)

    _refresh()
    ui.timer(1.0, _refresh)

    ui.button('清空本标签页的显示（不影响其他标签页/底层缓冲区）', on_click=log.clear).classes(
        'mt-2 text-xs'
    ).mark('log-panel-clear-button')
