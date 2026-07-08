"""Detail：结构化满足 DeviceRepository，纯内存实现，适合单测/本地演示。"""

from __future__ import annotations

from domain.models import Device


class InMemoryDeviceRepository:
    def __init__(self) -> None:
        self._store: dict[str, Device] = {}

    def save(self, device: Device) -> None:
        self._store[device.device_id] = device

    def get(self, device_id: str) -> Device | None:
        return self._store.get(device_id)

    def list_all(self) -> list[Device]:
        return list(self._store.values())
