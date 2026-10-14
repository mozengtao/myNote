"""Service 层：服务器端"可执行命令"的固定白名单与参数校验规则。

为什么需要这个模块
------------------
示例 6 的卖点是"从网页触发一个在服务器上真正执行的命令"，这同时也是整个
项目里风险最高的一块：一旦界面上出现"用户可以输入任意命令名或任意参数"的
入口，就等于在生产环境里放了一个远程命令执行后门。本模块的职责就是把
"能跑什么、怎么跑、跑多久"这几件事,从"UI 想怎么用"这件事里彻底分离出来：
UI 层永远只能从 `COMMAND_WHITELIST` 里选一个已经登记好的命令名，再提供一个
该命令要求的、会被严格校验的参数，除此之外没有任何其他入口。

心智模型
--------
把 `COMMAND_WHITELIST` 想象成一张"预先审批过的操作列表"，列表上的每一项
（`CommandSpec`）回答的是同一组问题：

- 这条命令真正对应哪个可执行文件？（`executable`，在模块加载时用
  `shutil.which()` 解析一次；解析不到就记为 `None`，代表"这台机器上不可用"，
  而不是让整个模块导入失败。）
- 调用时固定会带上哪些参数？（`fixed_args`，例如 `ls -la` 里的 `-la`。）
- 除了固定参数之外，还允许用户提供哪一个"可变参数"，以及这个参数必须满足
  什么规则？（`arg_spec`：可以是"必须是预先登记的几个选项之一"
  （`kind='choice'`，例如目录名），也可以是"必须匹配某个正则表达式"
  （`kind='regex'`，例如主机名、回显文本）。没有 `arg_spec` 就代表这条命令
  根本不接受任何用户输入（例如 `df -h`）。
- 在哪个目录下跑、最多跑多久、输出最多保留多少字节？（`cwd` / `timeout` /
  `max_output_bytes`。）

`run_command()` / `stream_command()` 是这张列表唯一的两个"执行入口"：
它们先用 `_resolve_args()` 把"命令名 + 用户参数"翻译成一份确定的、安全的
`[executable, *final_args]` 列表（翻译失败就直接抛异常，连
`infrastructure.process_runner` 都不会被调用到），再把这份已经验证过的
列表交给下层去真正执行。

安全边界说明
------------
本模块能做到的安全承诺，仅限于：
1. 只能执行 `COMMAND_WHITELIST` 里登记过、且在当前机器上找到了可执行文件
   的命令，不存在"传一个命令名字符串就能跑"的通用入口。
2. 每条命令的参数要么被限制为预先登记的固定选项，要么必须匹配一个专门为
   它设计的正则表达式；任何不满足规则的输入在到达 `subprocess`/
   `asyncio.create_subprocess_exec` 之前就会被 `ValidationError` 拦下。
3. 每条命令都有固定的工作目录和超时时间，调用方（UI）无法覆盖它们。

这不是、也不能被当作一个"通用远程命令执行入口"：它能跑的永远只是这四个
预先审查过的只读命令，参数空间也被压缩到了"几个选项"或"一个正则表达式"，
不存在任何"拼接出一条完整命令行再交给 shell"的路径。
"""

from __future__ import annotations

import re
import shutil
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Optional

from infrastructure import process_runner
from services.errors import PermissionDeniedError, ValidationError

#: 本项目的项目根目录——`list_directory` 白名单目录、各命令默认工作目录都
#: 以它为基准，避免在模块里出现写死的绝对路径字符串。
PROJECT_ROOT = Path(__file__).resolve().parents[1]

#: `ping_once` 的主机名/IPv4 校验规则：只允许字母、数字、点、短横线，
#: 长度 1~253（DNS 主机名的理论长度上限）。不允许分号、管道符、反引号、
#: 空格等任何 shell 特殊字符——即使本项目从不把参数交给 shell 解析，这里
#: 依然按照"防御性校验"的思路收紧规则，避免把明显异常的输入当成合法主机名。
_HOSTNAME_PATTERN = re.compile(r"^[A-Za-z0-9.-]{1,253}$")

#: `echo_text` 的回显文本校验规则：长度 1~200 字符，且不允许换行符（避免
#: 一次“回显”在 `ui.log` 里被拆成好几行，造成误导）。
_ECHO_TEXT_PATTERN = re.compile(r"^[^\r\n]{1,200}$")

#: `list_directory` 允许查看的目录——键是展示给用户、会出现在下拉框里的
#: 短名字，值是真正会被传给 `ls -la` 的绝对路径。用户永远只能从这几个键里
#: 选择，不可能自己输入一个任意路径。
LIST_DIRECTORY_CHOICES: dict[str, Path] = {
    "project_root": PROJECT_ROOT,
    "examples": PROJECT_ROOT / "examples",
    "docs": PROJECT_ROOT / "docs",
    "tmp": Path("/tmp"),
}


@dataclass(frozen=True)
class ArgSpec:
    """描述一个白名单命令"唯一允许的可变参数"该如何被校验与翻译。

    `kind` 目前支持两种取值：

    - `'choice'`：用户的输入必须恰好等于 `choices` 中的一个；如果
      `choice_map` 非空，真正传给可执行文件的参数是 `choice_map[用户输入]`
      （例如把目录的短名字 `'docs'` 翻译成它的绝对路径），否则直接使用用户
      输入本身。
    - `'regex'`：用户的输入必须能被 `pattern`完整匹配（`re.match`）。
    """

    label: str
    kind: str
    pattern: Optional[re.Pattern[str]] = None
    choices: tuple[str, ...] = ()
    choice_map: dict[str, str] = field(default_factory=dict)
    error_hint: str = ""


@dataclass(frozen=True)
class CommandSpec:
    """一条白名单命令的完整定义：是什么、怎么跑、跑多久。"""

    name: str
    description: str
    executable: Optional[str]
    fixed_args: tuple[str, ...]
    arg_spec: Optional[ArgSpec]
    cwd: Path
    timeout: float
    max_output_bytes: int = process_runner.DEFAULT_MAX_OUTPUT_BYTES


@dataclass
class CommandResult:
    """`run_command()` 的返回值：命令名 + 底层进程执行结果。"""

    command_name: str
    process_result: process_runner.ProcessResult


def _build_whitelist() -> dict[str, CommandSpec]:
    """在模块加载时构建一次固定白名单。

    对每一条命令都用 `shutil.which()` 解析一次真正的可执行文件绝对路径；
    解析不到就把 `executable` 记为 `None`——代表"这条命令在当前机器上不
    可用"，调用方（`run_command`/`stream_command`）和 UI 都必须据此把它当成
    不可执行的选项处理，而不是让导入这个模块本身失败。
    """
    whitelist: dict[str, CommandSpec] = {}

    whitelist["list_directory"] = CommandSpec(
        name="list_directory",
        description="列出一个预先登记的目录内容（ls -la <目录>）",
        executable=shutil.which("ls"),
        fixed_args=("-la",),
        arg_spec=ArgSpec(
            label="目录",
            kind="choice",
            choices=tuple(LIST_DIRECTORY_CHOICES.keys()),
            choice_map={name: str(path) for name, path in LIST_DIRECTORY_CHOICES.items()},
            error_hint="必须是预先登记的目录选项之一",
        ),
        cwd=PROJECT_ROOT,
        timeout=5.0,
    )

    whitelist["show_disk_usage"] = CommandSpec(
        name="show_disk_usage",
        description="显示磁盘使用情况（df -h），不需要任何参数",
        executable=shutil.which("df"),
        fixed_args=("-h",),
        arg_spec=None,
        cwd=PROJECT_ROOT,
        timeout=5.0,
    )

    whitelist["ping_once"] = CommandSpec(
        name="ping_once",
        description="向指定主机发送一次 ICMP 探测（ping -c 1 -W 2 <host>）",
        executable=shutil.which("ping"),
        fixed_args=("-c", "1", "-W", "2"),
        arg_spec=ArgSpec(
            label="主机名 / IP",
            kind="regex",
            pattern=_HOSTNAME_PATTERN,
            error_hint="只能包含字母、数字、点（.）和短横线（-），长度 1~253",
        ),
        # ping 本身自带 -W 2（等待回复最多 2 秒），这里的 timeout 比它稍长，
        # 用作"ping 进程本身卡死不退出"时的兜底防线。
        cwd=PROJECT_ROOT,
        timeout=5.0,
    )

    whitelist["echo_text"] = CommandSpec(
        name="echo_text",
        description="回显一段文本（echo <text>），用于演示最简单的命令执行",
        executable=shutil.which("echo"),
        fixed_args=(),
        arg_spec=ArgSpec(
            label="要回显的文本",
            kind="regex",
            pattern=_ECHO_TEXT_PATTERN,
            error_hint="长度必须在 1~200 字符之间，且不能包含换行符",
        ),
        cwd=PROJECT_ROOT,
        timeout=5.0,
    )

    return whitelist


#: 固定白名单：模块加载时构建一次，之后在整个进程生命周期内只读，不提供
#: 任何"运行时注册新命令"的接口——新增命令只能通过修改这份源代码来完成。
COMMAND_WHITELIST: dict[str, CommandSpec] = _build_whitelist()


def _resolve_args(spec: CommandSpec, user_args: list[str]) -> list[str]:
    """校验 `user_args` 是否满足 `spec.arg_spec` 的规则，并翻译成最终 argv。

    这是整条安全链路里最关键的一步：翻译失败（参数数量不对、不在选项内、
    不匹配正则）一律抛出 `ValidationError`，调用方必须保证这个函数抛异常
    之后，流程不会继续往下走到任何 `subprocess`/`asyncio` 调用。

    Returns:
        不包含可执行文件本身、可以直接交给 `process_runner` 的参数列表
        （`[*fixed_args, 已校验的可变参数]`，如果该命令没有 `arg_spec` 则
        就是 `[*fixed_args]`）。
    """
    if spec.arg_spec is None:
        if user_args:
            raise ValidationError(f"命令 {spec.name!r} 不接受任何参数，但收到了 {user_args!r}")
        return list(spec.fixed_args)

    if len(user_args) != 1:
        raise ValidationError(
            f"命令 {spec.name!r} 需要且只需要一个参数（{spec.arg_spec.label}），"
            f"但收到了 {len(user_args)} 个"
        )

    value = user_args[0]

    if spec.arg_spec.kind == "choice":
        if value not in spec.arg_spec.choices:
            raise ValidationError(
                f"{spec.arg_spec.label} 不合法：{value!r}。{spec.arg_spec.error_hint}"
            )
        actual_value = spec.arg_spec.choice_map.get(value, value)
        return [*spec.fixed_args, actual_value]

    if spec.arg_spec.kind == "regex":
        pattern = spec.arg_spec.pattern
        if pattern is None or pattern.match(value) is None:
            raise ValidationError(
                f"{spec.arg_spec.label} 不合法：{value!r}。{spec.arg_spec.error_hint}"
            )
        return [*spec.fixed_args, value]

    raise ValidationError(f"未知的参数校验类型：{spec.arg_spec.kind!r}")


def _lookup_runnable_spec(command_name: str) -> CommandSpec:
    """公共的"命令名 -> 可执行的 CommandSpec"校验逻辑，供两个执行入口共用。

    Raises:
        PermissionDeniedError: `command_name` 不在白名单内，或者虽然在白
            名单内、但在当前机器上没有找到对应的可执行文件。
    """
    spec = COMMAND_WHITELIST.get(command_name)
    if spec is None:
        raise PermissionDeniedError(f"命令不在白名单内，拒绝执行：{command_name!r}")
    if spec.executable is None:
        raise PermissionDeniedError(
            f"命令 {command_name!r} 在当前机器上不可用（未找到对应的可执行文件）"
        )
    return spec


def run_command(command_name: str, user_args: list[str]) -> CommandResult:
    """校验命令名与参数后，阻塞地执行一次白名单命令。

    这是一个阻塞调用（内部直接调用 `process_runner.run_subprocess()`），
    在 NiceGUI 的事件处理函数里必须用 `await run.io_bound(run_command, ...)`
    调用，否则会卡住整个页面的事件循环。

    Args:
        command_name: 必须是 `COMMAND_WHITELIST` 中的一个键。
        user_args: 用户提供的可变参数；不需要参数的命令传空列表。

    Raises:
        PermissionDeniedError: 命令名不在白名单内，或在当前机器上不可用。
        ValidationError: 参数数量或内容不满足该命令的校验规则。
        CommandTimeoutError: 见 `infrastructure.process_runner.run_subprocess`。
        CommandExecutionError: 见 `infrastructure.process_runner.run_subprocess`。
    """
    spec = _lookup_runnable_spec(command_name)
    final_args = _resolve_args(spec, user_args)

    process_result = process_runner.run_subprocess(
        spec.executable,  # type: ignore[arg-type]  # 上面已确认非 None
        final_args,
        cwd=spec.cwd,
        timeout=spec.timeout,
        max_output_bytes=spec.max_output_bytes,
    )
    return CommandResult(command_name=command_name, process_result=process_result)


async def stream_command(
    command_name: str,
    user_args: list[str],
    on_stdout: Callable[[str], None],
    on_stderr: Callable[[str], None],
) -> int:
    """校验命令名与参数后，以非阻塞方式执行一次白名单命令并实时回调输出。

    校验规则与 `run_command()` 完全一致（同一个 `_resolve_args()`），区别
    只在于底层调用的是 `process_runner.stream_subprocess()`，适合需要在
    UI 上实时展示滚动日志的场景。本项目示例 6 的 UI 目前优先使用阻塞式的
    `run_command()`（命令本身跑得很快，不需要流式展示），这个函数留给
    有"实时查看长时间运行命令输出"需求的后续示例或读者练习复用。

    Returns:
        子进程的退出码。

    Raises:
        PermissionDeniedError / ValidationError / CommandTimeoutError /
        CommandExecutionError：含义与 `run_command()` 一致。
    """
    spec = _lookup_runnable_spec(command_name)
    final_args = _resolve_args(spec, user_args)

    return await process_runner.stream_subprocess(
        spec.executable,  # type: ignore[arg-type]
        final_args,
        cwd=spec.cwd,
        timeout=spec.timeout,
        on_stdout_line=on_stdout,
        on_stderr_line=on_stderr,
    )
