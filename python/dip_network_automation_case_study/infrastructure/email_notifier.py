"""Detail：结构化满足 Notifier，模拟邮件发送。"""

from __future__ import annotations


class EmailNotifier:
    def __init__(self, smtp_host: str, to_address: str) -> None:
        self._smtp_host = smtp_host
        self._to_address = to_address

    def notify(self, message: str) -> None:
        print(f"[Email via {self._smtp_host} -> {self._to_address}] {message}")
