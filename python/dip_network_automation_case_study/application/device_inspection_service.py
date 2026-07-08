"""
Application 层 / High-level Module：设备巡检用例编排。

只依赖 DeviceConnector（不知道 SSH/NETCONF/gNMI）与 DeviceRepository（不知道
SQLite/内存）两个抽象，是本项目里"协议无关"的巡检核心逻辑。
"""

from __future__ import annotations

from domain.device_connector import DeviceConnector
from domain.device_repository import DeviceRepository
from domain.logger import Logger


class DeviceInspectionService:
    def __init__(
        self,
        connector: DeviceConnector,
        repo: DeviceRepository,
        logger: Logger,
    ) -> None:
        self._connector = connector
        self._repo = repo
        self._logger = logger

    def inspect(self, device_id: str) -> dict:
        device = self._repo.get(device_id)
        if device is None:
            raise ValueError(f"unknown device: {device_id}")

        state = self._connector.fetch_state(device.host)
        device.mark_inspected()
        self._repo.save(device)
        self._logger.info(f"inspected {device_id}: {state}")
        return state
