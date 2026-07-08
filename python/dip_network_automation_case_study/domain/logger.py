"""Abstraction (Port)：日志契约，具体是打印到终端/写文件/发到日志平台由 infrastructure/ 决定。"""

from __future__ import annotations

from typing import Protocol


class Logger(Protocol):
    def info(self, message: str) -> None: ...

    def error(self, message: str) -> None: ...
