"""Abstraction (Port)：设备出厂配置加载契约，具体是读本地文件/远程配置中心由 infrastructure/ 决定。"""

from __future__ import annotations

from typing import Protocol


class ConfigLoader(Protocol):
    def load(self, device_id: str) -> dict: ...
