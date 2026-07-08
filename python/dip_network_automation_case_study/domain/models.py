"""
Domain 层实体：本项目里唯一的"名词"。

Device 只携带数据和最基本的不变量校验，不知道 SSH、不知道数据库、不知道 HTTP。
这是整个项目里最稳定、最不应该因为技术选型变化而变化的代码。
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(slots=True)
class Device:
    device_id: str
    host: str
    model: str
    firmware: str
    status: str = "unknown"

    def mark_onboarded(self) -> None:
        self.status = "onboarded"

    def mark_inspected(self) -> None:
        self.status = "inspected"
