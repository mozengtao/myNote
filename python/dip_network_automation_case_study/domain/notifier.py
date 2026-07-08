"""Abstraction (Port)：通知契约，具体是邮件/Slack/短信由 infrastructure/ 决定。"""

from __future__ import annotations

from typing import Protocol


class Notifier(Protocol):
    def notify(self, message: str) -> None: ...
