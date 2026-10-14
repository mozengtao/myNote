"""services/ 包下所有模块共用的异常体系。

为什么需要这个模块
------------------
项目规格第 3.4 节要求所有示例和所有 Service 共用同一套异常词汇，而不是让
每个 Service 各自发明临时的错误类型。UI 代码（ui/*.py、examples/*.py）应当
捕获这些具体的异常类型并转换为面向用户的提示信息——而不是用一个笼统的
`except Exception` 把编程错误也一起悄悄吞掉。

心智模型
--------
`ServiceError` 是所有"预期内"失败的根类型——也就是作为操作契约正常一部分
的失败（文件不存在、输入不合法、命令不在白名单内、超时），UI 层必须处理并
展示这类失败，而不是因为它们而崩溃。任何从 Service 中逃逸出来的、不是
ServiceError 子类的异常，都被当作程序 bug 对待，应当继续向上传播，以便在
开发阶段显眼地暴露出来，而不是被静默吞掉。
"""

from __future__ import annotations


class ServiceError(Exception):
    """services/ 抛出的所有预期内（已处理）错误的基类。"""


class NotFoundError(ServiceError):
    """请求的文件、配置、命令名或任务 id 不存在。"""


class ValidationError(ServiceError):
    """用户提供的输入未通过校验（JSON 格式错误、缺少必需字段、类型错误等）。"""


class PermissionDeniedError(ServiceError):
    """因安全/策略原因拒绝执行某个操作。

    例如：解析后的路径逃逸出了允许的基准目录、操作系统因权限不足拒绝读取、
    或者某个命令名不在白名单中。
    """


class CommandTimeoutError(ServiceError):
    """白名单内的命令执行超过了配置的超时时间，已被强制终止。"""


class CommandExecutionError(ServiceError):
    """白名单内的命令无法启动，或者以调用方必须视为错误的方式失败
    （区别于"正常的非零退出码"这种情况）。
    """
