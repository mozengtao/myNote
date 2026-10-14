"""Infrastructure 层：对子进程执行的唯一封装。

为什么需要这个模块
------------------
整个项目里"真正会调用操作系统去跑一个外部程序"的代码，只允许出现在这一个
文件里。`services/command_service.py` 只负责"这个命令名是否在白名单
内、参数合不合法"这类业务规则，具体"怎么跑、怎么限时、怎么限制输出大小"
这些与操作系统打交道的细节，全部下沉到这里——这样将来如果要把执行方式从
本地 subprocess 换成别的东西（比如通过某种沙箱、容器），只需要改这一个
文件，`command_service.py` 的调用方式完全不用变。

心智模型
--------
本模块提供两种"跑一个子进程"的方式，对应两种不同的使用场景：

- `run_subprocess()`：阻塞调用，"等它跑完，一次性把 stdout/stderr/退出码
  都拿回来"。适合那些本来就跑得很快、不需要用户在界面上看到"实时滚动日志"
  的命令（本项目示例 6 的四个白名单命令都属于这一类）。因为它是阻塞的，
  调用方（Service/UI）如果在 NiceGUI 的事件循环里直接调用它会卡住整个
  界面，所以必须用 `nicegui.run.io_bound(...)` 丢到线程池里执行。
- `stream_subprocess()`：非阻塞（`async def`），"边跑边把每一行输出实时
  回调出去"。适合那些跑得比较久、用户希望看到类似终端滚动效果的场景
  （本项目暂时没有在示例 6 的 UI 里使用它，但 `command_service.py` 提供了
  对应的 `stream_command()`，留给后续示例或读者练习复用）。

两者都必须做到同一件事：**超时之后，子进程必须被真正杀掉**，而不是让它在
后台继续跑、只是 Python 这边"假装不管它了"——那样会在服务器上留下僵尸进程
或者失控的长命令。

层次说明（一个刻意的、狭窄的例外，请知悉）
--------------------------------------------
项目的总原则是"infrastructure/ 绝不 import services/*"。但本次任务同时
提出了两条更具体、互相之间会和这条总原则冲突的硬性要求：

1. "所有你抛出的异常必须是 `services/errors.py` 里已有类型的实例"；
2. 测试规格明确要求**直接**调用 `process_runner.run_subprocess()`（不经过
   `command_service`）来触发超时场景，并断言它直接抛出
   `services.errors.CommandTimeoutError`。

要同时满足这两条，`process_runner.py` 就必须能够直接抛出
`services.errors` 里定义的异常类型，这意味着它必须 import `services.errors`。
本模块因此只做了这一个狭窄的例外：只 import `services.errors`（一个不依赖
nicegui、不包含任何业务逻辑、本身也不 import 本项目其他任何模块的纯异常
定义模块），绝不 import `services/` 下的其他任何模块（不 import
`command_service`、`file_service`、`path_guard`），也绝不 import nicegui。
这和 `services/path_guard.py` 把 `services.errors` 当作"公共、无依赖的叶子
模块"来使用是同一种处理方式。这个设计取舍在交付报告与文档的 Architecture
小节里也有说明。
"""

from __future__ import annotations

import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from services.errors import CommandExecutionError, CommandTimeoutError

#: `run_subprocess()` / `stream_subprocess()` 默认的单路输出（stdout 或
#: stderr 各自独立计算）截断上限，单位字节。
DEFAULT_MAX_OUTPUT_BYTES = 65536


@dataclass
class ProcessResult:
    """一次阻塞子进程执行的结果快照。"""

    returncode: int
    stdout: str
    stderr: str
    duration_seconds: float
    stdout_truncated: bool = False
    stderr_truncated: bool = False


def _truncate_bytes(data: bytes, max_bytes: int) -> tuple[str, bool]:
    """把字节串截断到 `max_bytes` 以内，再解码成文本。

    截断点可能恰好落在一个多字节 UTF-8 字符中间，所以解码时用
    `errors='replace'`，宁可在截断边界出现一个替换字符，也不能因为解码失败
    而让整个命令执行流程抛出一个意料之外的 `UnicodeDecodeError`。
    """
    truncated = len(data) > max_bytes
    clipped = data[:max_bytes]
    text = clipped.decode("utf-8", errors="replace")
    return text, truncated


def run_subprocess(
    executable: str,
    args: list[str],
    *,
    cwd: Path,
    timeout: float,
    max_output_bytes: int = DEFAULT_MAX_OUTPUT_BYTES,
) -> ProcessResult:
    """阻塞地执行一个子进程，并对结果做超时与输出大小的双重防护。

    这是一个纯粹的"可执行文件路径 + 参数列表 + 工作目录"接口：调用方必须
    已经把命令名、参数校验这些业务规则处理完毕，这里只管"安全地跑一次
    外部程序"，不关心也不校验 `executable`/`args` 的业务含义。本函数绝不
    会把 `executable`/`args` 拼接成一个 shell 字符串——`subprocess.run()`
    在传入 `list[str]` 且不设置 `shell=True` 时，参数之间不会经过任何
    shell 解析（没有 `;`、`|`、`` ` `` 之类的特殊字符含义），这正是本项目
    "绝不给 shell 注入留任何口子"这条安全要求的底层保障。

    Args:
        executable: 真正会被执行的可执行文件绝对路径（调用方应提前用
            `shutil.which()` 解析好，这里不再做任何猜测或 PATH 查找）。
        args: 传给可执行文件的参数列表，每个参数都是独立的 argv 元素。
        cwd: 子进程的工作目录。
        timeout: 超时时间（秒），超过后子进程会被强制杀掉。
        max_output_bytes: stdout/stderr 各自独立的截断上限（字节）。

    Returns:
        一个 `ProcessResult`，其中 `stdout`/`stderr` 已经按字节截断过。

    Raises:
        CommandTimeoutError: 子进程运行超过 `timeout` 秒，已被强制终止。
        CommandExecutionError: 可执行文件不存在、没有执行权限，或者操作
            系统因为其他原因（`OSError`）无法启动该进程。
    """
    argv = [executable, *args]
    start = time.monotonic()
    try:
        completed = subprocess.run(
            argv,
            cwd=str(cwd),
            capture_output=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as exc:
        # 标准库的 subprocess.run() 在捕获到 TimeoutExpired 时，内部已经对
        # 子进程调用过一次 kill()（POSIX 下随后还会 wait() 回收，避免产生
        # 僵尸进程）。这里再做一次防御性的"保险"：如果这个异常对象恰好带有
        # 对应的 Popen 对象引用（不同 Python 版本/平台的内部实现细节可能
        # 变化），就再显式 kill + wait 一次，绝不依赖"应该已经被杀掉了"这种
        # 假设。
        process = getattr(exc, "process", None)
        if process is not None:
            process.kill()
            process.wait()
        duration = time.monotonic() - start
        raise CommandTimeoutError(
            f"命令执行超时（超过 {timeout} 秒），已强制终止："
            f"{executable} {' '.join(args)}；已运行 {duration:.3f} 秒"
        ) from exc
    except OSError as exc:
        raise CommandExecutionError(f"无法启动命令 {executable!r}：{exc}") from exc

    duration = time.monotonic() - start
    stdout_text, stdout_truncated = _truncate_bytes(completed.stdout or b"", max_output_bytes)
    stderr_text, stderr_truncated = _truncate_bytes(completed.stderr or b"", max_output_bytes)

    return ProcessResult(
        returncode=completed.returncode,
        stdout=stdout_text,
        stderr=stderr_text,
        duration_seconds=duration,
        stdout_truncated=stdout_truncated,
        stderr_truncated=stderr_truncated,
    )


async def stream_subprocess(
    executable: str,
    args: list[str],
    *,
    cwd: Path,
    timeout: float,
    on_stdout_line: Callable[[str], None],
    on_stderr_line: Callable[[str], None],
) -> int:
    """非阻塞地执行一个子进程，逐行实时回调 stdout/stderr。

    与 `run_subprocess()` 的阻塞式"一次性拿到完整结果"不同，这个函数是一个
    协程：它会在子进程还在运行的过程中，每读到一行输出就立刻调用一次对应
    的回调函数，调用方可以在回调里把这一行推到 `ui.log` 之类的组件上，做出
    "实时滚动日志"的效果。

    Args:
        executable: 真正会被执行的可执行文件绝对路径。
        args: 传给可执行文件的参数列表。
        cwd: 子进程的工作目录。
        timeout: 从进程启动到必须结束的总超时时间（秒）。
        on_stdout_line: 每读到一行 stdout（已去掉行尾换行符）就调用一次。
        on_stderr_line: 每读到一行 stderr（已去掉行尾换行符）就调用一次。

    Returns:
        子进程的退出码。

    Raises:
        CommandTimeoutError: 运行超过 `timeout` 秒，子进程已被强制终止
            （`proc.kill()` + `await proc.wait()`，确保不会留下僵尸进程）。
        CommandExecutionError: 可执行文件不存在、没有执行权限，或者操作
            系统因为其他原因（`OSError`）无法启动该进程。
    """
    import asyncio  # 延迟导入：只有真正用到异步路径时才需要 asyncio.subprocess。

    start = time.monotonic()
    try:
        proc = await asyncio.create_subprocess_exec(
            executable,
            *args,
            cwd=str(cwd),
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
    except OSError as exc:
        raise CommandExecutionError(f"无法启动命令 {executable!r}：{exc}") from exc

    async def _pump(stream: asyncio.StreamReader, callback: Callable[[str], None]) -> None:
        """不断读取一个流的每一行，解码后回调出去，直到流结束（EOF）。"""
        while True:
            raw_line = await stream.readline()
            if not raw_line:
                break
            callback(raw_line.decode("utf-8", errors="replace").rstrip("\n"))

    async def _run_to_completion() -> int:
        if proc.stdout is None or proc.stderr is None:
            # 理论上不会发生（我们显式用 PIPE 创建了两个流），这里只是让
            # 类型检查器和阅读者都能确认这一点，并避免后面对 None 调用方法。
            raise CommandExecutionError("子进程未能正确创建 stdout/stderr 管道")
        await asyncio.gather(
            _pump(proc.stdout, on_stdout_line),
            _pump(proc.stderr, on_stderr_line),
        )
        return await proc.wait()

    try:
        return await asyncio.wait_for(_run_to_completion(), timeout=timeout)
    except asyncio.TimeoutError as exc:
        proc.kill()
        await proc.wait()
        duration = time.monotonic() - start
        raise CommandTimeoutError(
            f"命令执行超时（超过 {timeout} 秒），已强制终止："
            f"{executable} {' '.join(args)}；已运行 {duration:.3f} 秒"
        ) from exc
