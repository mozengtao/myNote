"""Detail：结构化满足 Logger，打印到终端。"""

from __future__ import annotations

from datetime import datetime, timezone


class ConsoleLogger:
    def _timestamp(self) -> str:
        return datetime.now(timezone.utc).isoformat(timespec="seconds")

    def info(self, message: str) -> None:
        print(f"[{self._timestamp()}] [INFO] {message}")

    def error(self, message: str) -> None:
        print(f"[{self._timestamp()}] [ERROR] {message}")
