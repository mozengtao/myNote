"""
Abstraction (Port)：设备持久化契约。

无论最终存 SQLite、内存字典还是未来的 PostgreSQL，application/ 层都只认识这三个方法。
"""

from __future__ import annotations

from typing import Protocol

from .models import Device


class DeviceRepository(Protocol):
    def save(self, device: Device) -> None: ...

    def get(self, device_id: str) -> Device | None: ...

    def list_all(self) -> list[Device]: ...
